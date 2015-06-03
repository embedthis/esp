/*
    est.c - Embedded Secure Transport

    EST is based on the precursor to PolarSSL.

    Individual sockets are not thread-safe. Must only be used by a single thread.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "mpr.h"

#if ME_COM_EST
 /*
    Indent to bypass MakeMe dependencies
  */
 #include    "est.h"

/************************************* Defines ********************************/
/*
    Per-route SSL configuration
 */
typedef struct EstConfig {
    rsa_context     rsa;                /* RSA context */
    x509_cert       cert;               /* Certificate (own) */
    x509_cert       ca;                 /* Certificate authority bundle to verify peer */
    int             *ciphers;           /* Set of acceptable ciphers */
} EstConfig;

/*
    Per socket state
 */
typedef struct EstSocket {
    MprSocket       *sock;              /* MPR socket object */
    MprTicks        started;            /* When connection begun */
    EstConfig       *cfg;               /* Configuration */
    havege_state    hs;                 /* Random HAVEGE state */
    ssl_context     ctx;                /* SSL state */
    ssl_session     session;            /* SSL sessions */
} EstSocket;

static MprSocketProvider *estProvider;  /* EST socket provider */
static EstConfig *defaultEstConfig;     /* Default configuration */

/*
    DH Parameters from RFC3526
 */
#define DH_KEY_P \
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1" \
    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD" \
    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245" \
    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED" \
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D" \
    "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F" \
    "83655D23DCA3AD961C62F356208552BB9ED529077096966D" \
    "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B" \
    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9" \
    "DE2BCBF6955817183995497CEA956AE515D2261898FA0510" \
    "15728E5A8AACAA68FFFFFFFFFFFFFFFF"

#define DH_KEY_G "02"

/*
    Thread-safe session list
 */
static MprList *sessions;

/***************************** Forward Declarations ***************************/

static void     closeEst(MprSocket *sp, bool gracefully);
static void     disconnectEst(MprSocket *sp);
static void     estTrace(void *fp, int level, char *str);
static int      handshakeEst(MprSocket *sp);
static char     *getEstState(MprSocket *sp);
static void     manageEstConfig(EstConfig *cfg, int flags);
static void     manageEstProvider(MprSocketProvider *provider, int flags);
static void     manageEstSocket(EstSocket *ssp, int flags);
static ssize    readEst(MprSocket *sp, void *buf, ssize len);
static int      upgradeEst(MprSocket *sp, MprSsl *sslConfig, cchar *peerName);
static ssize    writeEst(MprSocket *sp, cvoid *buf, ssize len);

static int      setSession(ssl_context *ssl);
static int      getSession(ssl_context *ssl);

/************************************* Code ***********************************/
/*
    Create the EST module. This is called only once
 */
PUBLIC int mprSslInit(void *unused, MprModule *module)
{
    if ((estProvider = mprAllocObj(MprSocketProvider, manageEstProvider)) == NULL) {
        return MPR_ERR_MEMORY;
    }
    estProvider->upgradeSocket = upgradeEst;
    estProvider->closeSocket = closeEst;
    estProvider->disconnectSocket = disconnectEst;
    estProvider->readSocket = readEst;
    estProvider->writeSocket = writeEst;
    estProvider->socketState = getEstState;
    mprAddSocketProvider("est", estProvider);
    sessions = mprCreateList(0, 0);

    if ((defaultEstConfig = mprAllocObj(EstConfig, manageEstConfig)) == 0) {
        return MPR_ERR_MEMORY;
    }
    return 0;
}


static void manageEstProvider(MprSocketProvider *provider, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(provider->name);
        mprMark(defaultEstConfig);
        mprMark(sessions);

    } else if (flags & MPR_MANAGE_FREE) {
        defaultEstConfig = 0;
        sessions = 0;
    }
}


static void manageEstConfig(EstConfig *cfg, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        ;

    } else if (flags & MPR_MANAGE_FREE) {
        rsa_free(&cfg->rsa);
        x509_free(&cfg->cert);
        x509_free(&cfg->ca);
        free(cfg->ciphers);
    }
}


/*
    Destructor for an EstSocket object
 */
static void manageEstSocket(EstSocket *est, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(est->cfg);
        mprMark(est->sock);

    } else if (flags & MPR_MANAGE_FREE) {
        ssl_free(&est->ctx);
    }
}


static void closeEst(MprSocket *sp, bool gracefully)
{
    EstSocket       *est;

    est = sp->sslSocket;
    lock(sp);
    sp->service->standardProvider->closeSocket(sp, gracefully);
    if (!(sp->flags & MPR_SOCKET_EOF)) {
        ssl_close_notify(&est->ctx);
    }
    unlock(sp);
}


/*
    Upgrade a standard socket to use TLS
 */
static int upgradeEst(MprSocket *sp, MprSsl *ssl, cchar *peerName)
{
    EstSocket   *est;
    EstConfig   *cfg;
    int         verifyMode;

    assert(sp);

    if (ssl == 0) {
        ssl = mprCreateSsl(sp->flags & MPR_SOCKET_SERVER);
    }
    if ((est = (EstSocket*) mprAllocObj(EstSocket, manageEstSocket)) == 0) {
        return MPR_ERR_MEMORY;
    }
    est->sock = sp;
    sp->sslSocket = est;
    sp->ssl = ssl;
    verifyMode = ssl->verifyPeer ? SSL_VERIFY_OPTIONAL : SSL_VERIFY_NO_CHECK;

    lock(ssl);
    if (ssl->config && !ssl->changed) {
        est->cfg = cfg = ssl->config;
    } else {
        ssl->changed = 0;

        /*
            One time setup for the SSL configuration for this MprSsl
         */
        if ((cfg = mprAllocObj(EstConfig, manageEstConfig)) == 0) {
            unlock(ssl);
            return MPR_ERR_MEMORY;
        }
        if (ssl->certFile) {
            /*
                Load a PEM format certificate file
             */
            if (x509parse_crtfile(&cfg->cert, (char*) ssl->certFile) != 0) {
                sp->errorMsg = sfmt("Unable to parse certificate %s", ssl->certFile); 
                unlock(ssl);
                return MPR_ERR_CANT_READ;
            }
        }
        if (ssl->keyFile) {
            /*
                Load a decrypted PEM format private key
                Last arg is password if you need to use an encrypted private key
             */
            if (x509parse_keyfile(&cfg->rsa, (char*) ssl->keyFile, 0) != 0) {
                sp->errorMsg = sfmt("Unable to parse key file %s", ssl->keyFile); 
                unlock(ssl);
                return MPR_ERR_CANT_READ;
            }
        }
        if (verifyMode != SSL_VERIFY_NO_CHECK) {
            if (!ssl->caFile) {
                sp->errorMsg = sclone("No defined certificate authority file");
                unlock(ssl);
                return MPR_ERR_CANT_READ;
            }
            if (x509parse_crtfile(&cfg->ca, (char*) ssl->caFile) != 0) {
                sp->errorMsg = sfmt("Unable to open or parse certificate authority file %s", ssl->caFile); 
                unlock(ssl);
                return MPR_ERR_CANT_READ;
            }
        }
        est->cfg = ssl->config = cfg;
        cfg->ciphers = ssl_create_ciphers(ssl->ciphers);
    }
    unlock(ssl);

    ssl_free(&est->ctx);
    havege_init(&est->hs);
    ssl_init(&est->ctx);
    ssl_set_endpoint(&est->ctx, sp->flags & MPR_SOCKET_SERVER ? SSL_IS_SERVER : SSL_IS_CLIENT);
    ssl_set_authmode(&est->ctx, verifyMode);
    ssl_set_rng(&est->ctx, havege_rand, &est->hs);
    ssl_set_dbg(&est->ctx, estTrace, NULL);
    ssl_set_bio(&est->ctx, net_recv, &sp->fd, net_send, &sp->fd);

    ssl_set_scb(&est->ctx, getSession, setSession);
    ssl_set_ciphers(&est->ctx, cfg->ciphers);

    ssl_set_session(&est->ctx, 1, 0, &est->session);
    memset(&est->session, 0, sizeof(ssl_session));

    ssl_set_ca_chain(&est->ctx, ssl->caFile ? &cfg->ca : NULL, (char*) peerName);
    if (ssl->keyFile && ssl->certFile) {
        ssl_set_own_cert(&est->ctx, &cfg->cert, &cfg->rsa);
    }
    ssl_set_dh_param(&est->ctx, DH_KEY_P, DH_KEY_G);
    est->started = mprGetTicks();

    if (handshakeEst(sp) < 0) {
        return -1;
    }
    return 0;
}


static void disconnectEst(MprSocket *sp)
{
    sp->service->standardProvider->disconnectSocket(sp);
}


/*
    Initiate or continue SSL handshaking with the peer. This routine does not block.
    Return -1 on errors, 0 incomplete and awaiting I/O, 1 if successful
 */
static int handshakeEst(MprSocket *sp)
{
    EstSocket   *est;
    char        cbuf[5120];
    int         rc, vrc;

    est = (EstSocket*) sp->sslSocket;
    assert(!(est->ctx.state == SSL_HANDSHAKE_OVER));
    rc = 0;

    sp->flags |= MPR_SOCKET_HANDSHAKING;
    while (est->ctx.state != SSL_HANDSHAKE_OVER && (rc = ssl_handshake(&est->ctx)) != 0) {
        if (rc == EST_ERR_NET_TRY_AGAIN) {
            if (!mprGetSocketBlockingMode(sp)) {
                return 0;
            }
            continue;
        }
        /* Error */
        break;
    }
    sp->flags &= ~MPR_SOCKET_HANDSHAKING;

    /*
        Get peer details
     */
    if (est->ctx.peer_cn) {
        sp->peerName = sclone(est->ctx.peer_cn);
    }
    sp->cipher = sclone(ssl_get_cipher(&est->ctx));
    if (est->ctx.peer_cert && rc == 0) {
        x509parse_dn_gets("", cbuf, sizeof(cbuf), &est->ctx.peer_cert->subject);
        sp->peerCert = sclone(cbuf);
        x509parse_dn_gets("", cbuf, sizeof(cbuf), &est->ctx.peer_cert->issuer);
        sp->peerCertIssuer = sclone(cbuf);
    }

    /*
        Analyze the handshake result
     */
    if (rc < 0) {
        if (rc == EST_ERR_SSL_PRIVATE_KEY_REQUIRED && !(sp->ssl->keyFile || sp->ssl->certFile)) {
            sp->errorMsg = sclone("Peer requires a certificate");
        } else {
            sp->errorMsg = sfmt("Cannot handshake: error -0x%x", -rc);
        }
        sp->flags |= MPR_SOCKET_EOF;
        errno = EPROTO;
        return -1;
    } 

    if ((vrc = ssl_get_verify_result(&est->ctx)) != 0) {
        if (vrc & BADCERT_EXPIRED) {
            sp->errorMsg = sclone("Certificate expired");

        } else if (vrc & BADCERT_REVOKED) {
            sp->errorMsg = sclone("Certificate revoked");

        } else if (vrc & BADCERT_CN_MISMATCH) {
            sp->errorMsg = sclone("Certificate common name mismatch");

        } else if (vrc & BADCERT_NOT_TRUSTED) {
            if (vrc & BADCERT_SELF_SIGNED) {
                sp->errorMsg = sclone("Self-signed certificate");
            } else {
                sp->errorMsg = sclone("Certificate not trusted");
            }
            if (!sp->ssl->verifyIssuer) {
                vrc = 0;
            }

        } else {
            if (est->ctx.client_auth && !sp->ssl->certFile) {
                sp->errorMsg = sclone("Server requires a client certificate");
            } else if (rc == EST_ERR_NET_CONN_RESET) {
                sp->errorMsg = sclone("Peer disconnected");
            } else {
                sp->errorMsg = sfmt("Cannot handshake: error -0x%x", -rc);
            }
        }
    }
    if (sp->ssl->verifyPeer && vrc != 0) {
        if (!est->ctx.peer_cert) {
            sp->errorMsg = sclone("Peer did not provide a certificate");
        }
        sp->flags |= MPR_SOCKET_EOF;
        errno = EPROTO;
        return -1;
    }
    sp->secured = 1;
    return 1;
}


/*
    Return the number of bytes read. Return -1 on errors and EOF. Distinguish EOF via mprIsSocketEof.
    If non-blocking, may return zero if no data or still handshaking.
 */
static ssize readEst(MprSocket *sp, void *buf, ssize len)
{
    EstSocket   *est;
    int         rc;

    est = (EstSocket*) sp->sslSocket;
    assert(est);
    assert(est->cfg);

    if (sp->fd == INVALID_SOCKET) {
        return -1;
    }
    if (est->ctx.state != SSL_HANDSHAKE_OVER) {
        if ((rc = handshakeEst(sp)) <= 0) {
            return rc;
        }
    }
    while (1) {
        rc = ssl_read(&est->ctx, buf, (int) len);
        mprDebug("debug mpr ssl est", 5, "ssl_read %d", rc);
        if (rc < 0) {
            if (rc == EST_ERR_NET_TRY_AGAIN)  {
                rc = 0;
                break;
            } else if (rc == EST_ERR_SSL_PEER_CLOSE_NOTIFY) {
                mprDebug("debug mpr ssl est", 5, "connection was closed gracefully\n");
                sp->flags |= MPR_SOCKET_EOF;
                return -1;
            } else if (rc == EST_ERR_NET_CONN_RESET) {
                mprDebug("debug mpr ssl est", 5, "connection reset");
                sp->flags |= MPR_SOCKET_EOF;
                return -1;
            } else {
                mprDebug("debug mpr ssl est", 4, "read error -0x%x", -rc);
                sp->flags |= MPR_SOCKET_EOF;
                return -1;
            }
        }
        break;
    }
    mprHiddenSocketData(sp, ssl_get_bytes_avail(&est->ctx), MPR_READABLE);
    return rc;
}


/*
    Write data. Return the number of bytes written or -1 on errors or socket closure.
    If non-blocking, may return zero if no data or still handshaking.
 */
static ssize writeEst(MprSocket *sp, cvoid *buf, ssize len)
{
    EstSocket   *est;
    ssize       totalWritten;
    int         rc;

    est = (EstSocket*) sp->sslSocket;
    if (len <= 0) {
        assert(0);
        return -1;
    }
    if (est->ctx.state != SSL_HANDSHAKE_OVER) {
        if ((rc = handshakeEst(sp)) <= 0) {
            return rc;
        }
    }
    totalWritten = 0;
    rc = 0;
    do {
        rc = ssl_write(&est->ctx, (uchar*) buf, (int) len);
        mprDebug("debug mpr ssl est", 5, "written %d, requested len %zd", rc, len);
        if (rc <= 0) {
            if (rc == EST_ERR_NET_TRY_AGAIN) {                                                          
                break;
            }
            if (rc == EST_ERR_NET_CONN_RESET) {                                                         
                mprDebug("debug mpr ssl est", 5, "ssl_write peer closed");
                return -1;
            } else {
                mprDebug("debug mpr ssl est", 5, "ssl_write failed rc -0x%x", -rc);
                return -1;
            }
        } else {
            totalWritten += rc;
            buf = (void*) ((char*) buf + rc);
            len -= rc;
            mprDebug("debug mpr ssl est", 5, "write: len %zd, written %d, total %zd", len, rc, totalWritten);
        }
    } while (len > 0);

    mprHiddenSocketData(sp, est->ctx.out_left, MPR_WRITABLE);

    if (totalWritten == 0 && rc == EST_ERR_NET_TRY_AGAIN) {                                                          
        mprSetError(EAGAIN);
        return -1;
    }
    return totalWritten;
}


static char *getEstState(MprSocket *sp)
{
    EstSocket       *est;
    ssl_context     *ctx;
    MprBuf          *buf;
    char            *ownPrefix, *peerPrefix;
    char            cbuf[5120];

    if ((est = sp->sslSocket) == 0) {
        return 0;
    }
    ctx = &est->ctx;
    buf = mprCreateBuf(0, 0);
    mprPutToBuf(buf, "PROVIDER=est,CIPHER=%s,SESSION=%s", ssl_get_cipher(ctx), ctx->session->id);

    mprPutToBuf(buf, "PEER=\"%s\",", est->ctx.peer_cn);
    if (ctx->peer_cert) {
        peerPrefix = sp->acceptIp ? "CLIENT_" : "SERVER_";
        x509parse_cert_info(peerPrefix, cbuf, sizeof(cbuf), ctx->peer_cert);
        mprPutStringToBuf(buf, cbuf);
    } else {
        mprPutToBuf(buf, "%s=\"none\",", sp->acceptIp ? "CLIENT_CERT" : "SERVER_CERT");
    }
    if (ctx->own_cert) {
        ownPrefix =  sp->acceptIp ? "SERVER_" : "CLIENT_";
        x509parse_cert_info(ownPrefix, cbuf, sizeof(cbuf), ctx->own_cert);
        mprPutStringToBuf(buf, cbuf);
    }
    return mprGetBufStart(buf);
}


/*
    Thread-safe session management
 */
static int getSession(ssl_context *ssl)
{
    ssl_session     *session;
    time_t          t;
    int             next;

    t = time(NULL);
    if (!ssl->resume) {
        return 1;
    }
    for (ITERATE_ITEMS(sessions, session, next)) {
        if (ssl->timeout && (t - session->start) > ssl->timeout) {
            continue;
        }
        if (ssl->session->cipher != session->cipher || ssl->session->length != session->length) {
            continue;
        }
        if (memcmp(ssl->session->id, session->id, session->length) != 0) {
            continue;
        }
        memcpy(ssl->session->master, session->master, sizeof(ssl->session->master));
        return 0;
    }
    return 1;
}


static int setSession(ssl_context *ssl)
{
    time_t          t;
    ssl_session     *session;
    int             next;

    t = time(NULL);
    for (ITERATE_ITEMS(sessions, session, next)) {
        if (ssl->timeout != 0 && (t - session->start) > ssl->timeout) {
            /* expired, reuse this slot */
            break;  
        }
        if (memcmp(ssl->session->id, session->id, session->length) == 0) {
            /* client reconnected */
            break;  
        }
    }
    if (session == NULL) {
        if ((session = mprAlloc(sizeof(ssl_session))) == 0) {
            return 1;
        }
        mprAddItem(sessions, session);
    }
    memcpy(session, ssl->session, sizeof(ssl_session));
    return 0;
}


static void estTrace(void *fp, int level, char *str)
{
    level += 3;
    if (level <= MPR->logLevel) {
        mprLog("info est", level, "%s: %d: %s", MPR->name, level, str);
    }
}

#endif /* ME_COM_EST */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
