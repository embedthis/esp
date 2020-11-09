/*
    mpr-mbedtls.c - MbedTLS Interface to the MPR

    Individual sockets are not thread-safe. Must only be used by a single thread.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "mpr.h"

#if ME_COM_MBEDTLS
 /*
    Indent to bypass MakeMe dependencies
  */
 #include    "mbedtls.h"

/************************************* Defines ********************************/
/*
    Per-route SSL configuration
 */
typedef struct MbedConfig {
    mbedtls_x509_crt            ca;             /* Certificate authority bundle to verify peer */
    mbedtls_x509_crt            cert;           /* Certificate (own) */
    mbedtls_pk_context          key;            /* Private key */
    mbedtls_x509_crl            revoke;         /* Certificate revoke list */
    mbedtls_ssl_config          conf;           /* SSL configuration */
    int                         *ciphers;       /* Set of acceptable ciphers - null terminated */
} MbedConfig;


typedef struct MbedGlobal {
    mbedtls_ssl_cache_context   cache;          /* Session cache context */
    mbedtls_ctr_drbg_context    ctr;            /* Counter random generator state */
    mbedtls_ssl_ticket_context  tickets;        /* Session tickets */
    mbedtls_entropy_context     mbedEntropy;    /* Entropy context */
} MbedGlobal;

typedef struct MbedSocket {
    MbedConfig                  *cfg;           /* Configuration */
    MprSocket                   *sock;          /* MPR socket object */
    mbedtls_ssl_context         ctx;            /* SSL state */
    int                         configured;     /* Set if SNI configuration has completed */
} MbedSocket;

static MbedGlobal               *mbedGlobal;    /* Global */
static MprSocketProvider        *mbedProvider;  /* Mbedtls socket provider */
static int                      mbedLogLevel;   /* MPR log level to start SSL tracing */

/***************************** Forward Declarations ***************************/

static void     closeMbed(MprSocket *sp, bool gracefully);
static void     disconnectMbed(MprSocket *sp);
static void     freeMbedLock(mbedtls_threading_mutex_t *tm);
static int      *getCipherSuite(MprSsl *ssl);
static char     *getMbedState(MprSocket *sp);
static int      getPeerCertInfo(MprSocket *sp);
static int      handshakeMbed(MprSocket *sp);
static void     initMbedLock(mbedtls_threading_mutex_t *tm);
static void     manageMbedConfig(MbedConfig *cfg, int flags);
static void     manageMbedGlobal(MbedGlobal *cfg, int flags);
static void     manageMbedProvider(MprSocketProvider *provider, int flags);
static void     manageMbedSocket(MbedSocket*ssp, int flags);
static int      mbedLock(mbedtls_threading_mutex_t *tm);
static int      mbedUnlock(mbedtls_threading_mutex_t *tm);
static void     merror(int rc, cchar *fmt, ...);
static int      parseCert(mbedtls_x509_crt *cert, cchar *file, char **errorMsg);
static int      parseCrl(mbedtls_x509_crl *crl, cchar *path, char **errorMsg);
static int      parseKey(mbedtls_pk_context *key, cchar *path, char **errorMsg);
static int      preloadMbed(MprSsl *ssl, int flags);
static ssize    readMbed(MprSocket *sp, void *buf, ssize len);
static char     *replaceHyphen(char *cipher, char from, char to);
PUBLIC int      sniCallback(void *unused, mbedtls_ssl_context *ctx, cuchar *hostname, size_t len);
static void     traceMbed(void *context, int level, cchar *file, int line, cchar *str);
static int      upgradeMbed(MprSocket *sp, MprSsl *ssl, cchar *peerName);
static ssize    writeMbed(MprSocket *sp, cvoid *buf, ssize len);

/************************************* Code ***********************************/
/*
    Create the Mbedtls module. This is called only once
 */
PUBLIC int mprSslInit(void *unused, MprModule *module)
{
    MbedGlobal          *global;
    cchar               *appName;
    int                 rc;

    if ((mbedProvider = mprAllocObj(MprSocketProvider, manageMbedProvider)) == NULL) {
        return MPR_ERR_MEMORY;
    }
    mbedProvider->name = sclone("mbedtls");
    mbedProvider->upgradeSocket = upgradeMbed;
    mbedProvider->closeSocket = closeMbed;
    mbedProvider->disconnectSocket = disconnectMbed;
    mbedProvider->preload = preloadMbed;
    mbedProvider->readSocket = readMbed;
    mbedProvider->writeSocket = writeMbed;
    mbedProvider->socketState = getMbedState;
    mprSetSslProvider(mbedProvider);

    if ((global = mprAllocObj(MbedGlobal, manageMbedGlobal)) == 0) {
        return MPR_ERR_MEMORY;
    }
    mbedProvider->managed = mbedGlobal = global;

    mbedtls_threading_set_alt(initMbedLock, freeMbedLock, mbedLock, mbedUnlock);
    mbedtls_ssl_cache_init(&global->cache);
    mbedtls_ctr_drbg_init(&global->ctr);
    mbedtls_ssl_ticket_init(&global->tickets);
    mbedtls_entropy_init(&global->mbedEntropy);

    appName = mprGetAppName();
    if ((rc = mbedtls_ctr_drbg_seed(&global->ctr, mbedtls_entropy_func, &global->mbedEntropy,
            (cuchar*) appName, slen(appName))) < 0) {
        merror(rc, "Cannot seed rng");
        return MPR_ERR_CANT_INITIALIZE;
    }
    if (ME_MPR_SSL_TICKET) {
        if ((rc = mbedtls_ssl_ticket_setup(&global->tickets, mbedtls_ctr_drbg_random, &global->ctr,
                MBEDTLS_CIPHER_AES_256_GCM, ME_MPR_SSL_TIMEOUT)) < 0) {
            merror(rc, "Cannot setup ticketing sessions");
            return MPR_ERR_CANT_INITIALIZE;
        }
    }
    mbedtls_ssl_cache_set_max_entries(&global->cache, ME_MPR_SSL_CACHE);
    mbedtls_ssl_cache_set_timeout(&global->cache, ME_MPR_SSL_TIMEOUT);
    return 0;
}


static void manageMbedProvider(MprSocketProvider *provider, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(provider->name);
        mprMark(provider->managed);
    }
}


static void manageMbedGlobal(MbedGlobal *global, int flags)
{
    if (flags & MPR_MANAGE_FREE) {
        mbedtls_ctr_drbg_free(&global->ctr);
        mbedtls_ssl_cache_free(&global->cache);
        mbedtls_ssl_ticket_free(&global->tickets);
        mbedtls_entropy_free(&global->mbedEntropy);
    }
}


static void manageMbedConfig(MbedConfig *cfg, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(cfg->ciphers);

    } else if (flags & MPR_MANAGE_FREE) {
        mbedtls_pk_free(&cfg->key);
        mbedtls_x509_crt_free(&cfg->cert);
        mbedtls_x509_crt_free(&cfg->ca);
        mbedtls_x509_crl_free(&cfg->revoke);
        mbedtls_ssl_config_free(&cfg->conf);
    }
}


/*
    Destructor for an MbedSocket object
 */
static void manageMbedSocket(MbedSocket *mb, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(mb->cfg);
        mprMark(mb->sock);

    } else if (flags & MPR_MANAGE_FREE) {
        mbedtls_ssl_free(&mb->ctx);
    }
}


static void closeMbed(MprSocket *sp, bool gracefully)
{
    MbedSocket      *mb;

    mb = sp->sslSocket;
    sp->service->standardProvider->closeSocket(sp, gracefully);
    if (!(sp->flags & MPR_SOCKET_EOF)) {
        mbedtls_ssl_close_notify(&mb->ctx);
    }
}


static int configMbed(MprSsl *ssl, int flags, char **errorMsg)
{
    MbedConfig          *cfg;
    mbedtls_ssl_config  *mconf;
    cuchar              dhm_p[] = MBEDTLS_DHM_RFC3526_MODP_2048_P_BIN;
    cuchar              dhm_g[] = MBEDTLS_DHM_RFC3526_MODP_2048_G_BIN;
    char                *anext, *aprotocol;
    int                 i, rc;

    if (ssl->config && !ssl->changed) {
        return 0;
    }
    if ((cfg = mprAllocObj(MbedConfig, manageMbedConfig)) == 0) {
        return MPR_ERR_MEMORY;
    }
    ssl->config = cfg;
    mconf = &cfg->conf;
    mbedtls_ssl_config_init(mconf);
    mbedtls_ssl_conf_dbg(mconf, traceMbed, NULL);
    mbedLogLevel = ssl->logLevel;
    if (MPR->logLevel >= mbedLogLevel) {
        mbedtls_debug_set_threshold(MPR->logLevel - mbedLogLevel);
    }
    mbedtls_pk_init(&cfg->key);
    mbedtls_x509_crt_init(&cfg->cert);

    if (ssl->certFile) {
        if (parseCert(&cfg->cert, ssl->certFile, errorMsg) != 0) {
            return MPR_ERR_CANT_INITIALIZE;
        }
        if (!ssl->keyFile) {
            /* Can include the private key with the cert file */
            ssl->keyFile = ssl->certFile;
        }
    }
    if (ssl->keyFile) {
        /*
            Load a decrypted PEM format private key
            Last arg is password if you need to use an encrypted private key
         */
        if (parseKey(&cfg->key, ssl->keyFile, errorMsg) != 0) {
            return MPR_ERR_CANT_INITIALIZE;
        }
    }
    if (ssl->verifyPeer) {
        if (!ssl->caFile) {
            *errorMsg = sclone("No defined certificate authority file");
            return MPR_ERR_CANT_INITIALIZE;
        }
        if (parseCert(&cfg->ca, ssl->caFile, errorMsg) != 0) {
            return MPR_ERR_CANT_INITIALIZE;
        }
    }
    if (ssl->revoke) {
        /*
            Load a PEM format certificate file
         */
        if (parseCrl(&cfg->revoke, ssl->revoke, errorMsg) != 0) {
            return MPR_ERR_CANT_INITIALIZE;
        }
    }
    if ((rc = mbedtls_ssl_config_defaults(mconf, flags & MPR_SOCKET_SERVER ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) < 0) {
        merror(rc, "Cannot set mbedtls defaults");
        return MPR_ERR_CANT_INITIALIZE;
    }
    mbedtls_ssl_conf_rng(mconf, mbedtls_ctr_drbg_random, &mbedGlobal->ctr);

    /*
        Configure larger DH parameters
     */
    if ((rc = mbedtls_ssl_conf_dh_param_bin(mconf, dhm_p, sizeof(dhm_g), dhm_g, sizeof(dhm_g))) < 0) {
        merror(rc, "Cannot set DH params");
        return MPR_ERR_CANT_INITIALIZE;
    }

    if (flags & MPR_SOCKET_SERVER) {
#if ME_MPR_SSL_TICKET
        /*
            Configure ticket-based sessions
         */
        if (ssl->ticket) {
            mbedtls_ssl_conf_session_tickets_cb(mconf, mbedtls_ssl_ticket_write, mbedtls_ssl_ticket_parse,
                &mbedGlobal->tickets);
        }
#endif
#if ME_MPR_SSL_CACHE_SIZE > 0
        /*
            Configure server-side session cache
         */
        mbedtls_ssl_conf_session_cache(mconf, &mbedGlobal->cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
#endif
    } else {
        mbedtls_ssl_conf_session_tickets(mconf, 1);
    }
    /*
        Set auth mode if peer cert should be verified
     */
    mbedtls_ssl_conf_authmode(mconf, ssl->verifyPeer ? MBEDTLS_SSL_VERIFY_OPTIONAL : MBEDTLS_SSL_VERIFY_NONE);

    /*
        Configure cert, key and CA.
     */
    if (ssl->keyFile && ssl->certFile) {
        if ((rc = mbedtls_ssl_conf_own_cert(mconf, &cfg->cert, &cfg->key)) < 0) {
            merror(rc, "Cannot define certificate and private key");
            return MPR_ERR_CANT_INITIALIZE;
        }
    }
    mbedtls_ssl_conf_ca_chain(mconf, ssl->caFile ? &cfg->ca : NULL, ssl->revoke ? &cfg->revoke : NULL);

    if ((cfg->ciphers = getCipherSuite(ssl)) != 0) {
        mbedtls_ssl_conf_ciphersuites(mconf, cfg->ciphers);
    }
#if ME_MPR_HAS_ALPN
    if (ssl->alpn) {
        mbedtls_ssl_conf_alpn_protocols(mconf, (cchar**) ssl->alpn->items);
    }
#endif
    if (flags & MPR_SOCKET_SERVER && ssl->matchSsl) {
        mbedtls_ssl_conf_sni(mconf, sniCallback, 0);
    }
    ssl->changed = 0;
    return 0;
}


/*
    Upgrade a socket to SSL.
    On-demand processing of the SSL configuration when the first client connects.
    Need to setup all possible (vhost) SSL configuration, then SNI will select the right certificate.
 */
static int upgradeMbed(MprSocket *sp, MprSsl *ssl, cchar *peerName)
{
    MbedSocket          *mb;
    mbedtls_ssl_context *ctx;

    assert(sp);

    lock(ssl);
    if (configMbed(ssl, sp->flags, &sp->errorMsg) < 0) {
        unlock(ssl);
        return MPR_ERR_CANT_INITIALIZE;
    }
    unlock(ssl);

    sp->ssl = ssl;
    assert(ssl->config);

    if ((mb = (MbedSocket*) mprAllocObj(MbedSocket, manageMbedSocket)) == 0) {
        return MPR_ERR_MEMORY;
    }
    sp->sslSocket = mb;
    mb->sock = sp;
    mb->cfg = ssl->config;

    ctx = &mb->ctx;
    mbedtls_ssl_init(ctx);
    ctx->appData = sp;
    mbedtls_ssl_setup(ctx, &mb->cfg->conf);
    mbedtls_ssl_set_bio(ctx, &sp->fd, mbedtls_net_send, mbedtls_net_recv, 0);

    if (peerName && mbedtls_ssl_set_hostname(ctx, peerName) < 0) {
        return MPR_ERR_BAD_ARGS;
    }
    if (handshakeMbed(sp) < 0) {
        return MPR_ERR_CANT_INITIALIZE;
    }
    return 0;
}


static void disconnectMbed(MprSocket *sp)
{
    sp->service->standardProvider->disconnectSocket(sp);
}


/*
    Initiate or continue SSL handshaking with the peer. This routine does not block.
    Return -1 on errors, 0 incomplete and awaiting I/O, 1 if successful
 */
static int handshakeMbed(MprSocket *sp)
{
    MbedSocket  *mb;
    int         rc, vrc;

    mb = (MbedSocket*) sp->sslSocket;
    assert(!(mb->ctx.state == MBEDTLS_SSL_HANDSHAKE_OVER));

    rc = 0;
    sp->flags |= MPR_SOCKET_HANDSHAKING;
    while (mb->ctx.state != MBEDTLS_SSL_HANDSHAKE_OVER && (rc = mbedtls_ssl_handshake(&mb->ctx)) != 0) {
        if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE)  {
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
        Analyze the handshake result
     */
    if (rc < 0) {
        if (rc == MBEDTLS_ERR_SSL_PRIVATE_KEY_REQUIRED && !(sp->ssl->keyFile || sp->ssl->certFile)) {
            sp->errorMsg = sclone("Peer requires a certificate");
            sp->flags |= MPR_SOCKET_CERT_ERROR;
        } else {
            char ebuf[256];
            mbedtls_strerror(-rc, ebuf, sizeof(ebuf));
            sp->errorMsg = sfmt("%s: error -0x%x", ebuf, -rc);
        }
        sp->flags |= MPR_SOCKET_EOF | MPR_SOCKET_ERROR;
        errno = EPROTO;
        return MPR_ERR_CANT_READ;
    }
    if ((vrc = mbedtls_ssl_get_verify_result(&mb->ctx)) != 0) {
        if (vrc & MBEDTLS_X509_BADCERT_MISSING) {
            sp->errorMsg = sclone("Certificate missing");
            sp->flags |= MPR_SOCKET_CERT_ERROR;

        } if (vrc & MBEDTLS_X509_BADCERT_EXPIRED) {
            sp->errorMsg = sclone("Certificate expired");
            sp->flags |= MPR_SOCKET_CERT_ERROR;

        } else if (vrc & MBEDTLS_X509_BADCERT_REVOKED) {
            sp->errorMsg = sclone("Certificate revoked");
            sp->flags |= MPR_SOCKET_CERT_ERROR;

        } else if (vrc & MBEDTLS_X509_BADCERT_CN_MISMATCH) {
            sp->errorMsg = sclone("Certificate common name mismatch");
            sp->flags |= MPR_SOCKET_CERT_ERROR;

        } else if (vrc & MBEDTLS_X509_BADCERT_KEY_USAGE || vrc & MBEDTLS_X509_BADCERT_EXT_KEY_USAGE) {
            sp->errorMsg = sclone("Unauthorized key use in certificate");
            sp->flags |= MPR_SOCKET_CERT_ERROR;

        } else if (vrc & MBEDTLS_X509_BADCERT_NOT_TRUSTED) {
            sp->errorMsg = sclone("Certificate not trusted");
            if (!sp->ssl->verifyIssuer) {
                vrc = 0;
            } else {
                sp->flags |= MPR_SOCKET_CERT_ERROR;
            }

        } else if (vrc & MBEDTLS_X509_BADCERT_SKIP_VERIFY) {
            /*
                MBEDTLS_SSL_VERIFY_NONE requested, so ignore error
             */
            vrc = 0;

        } else {
            if (mb->ctx.client_auth && !sp->ssl->certFile) {
                sp->errorMsg = sclone("Server requires a client certificate");
                sp->flags |= MPR_SOCKET_CERT_ERROR;

            } else if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
                sp->errorMsg = sclone("Peer disconnected");
                sp->flags |= MPR_SOCKET_ERROR;

            } else {
                char ebuf[256];
                mbedtls_x509_crt_verify_info(ebuf, sizeof(ebuf), "", vrc);
                strim(ebuf, "\n", 0);
                sp->errorMsg = sfmt("Cannot handshake: %s, error -0x%x", ebuf, -rc);
                sp->flags |= MPR_SOCKET_ERROR;
            }
        }
    }
    if (vrc != 0 && sp->ssl->verifyPeer) {
        if (mbedtls_ssl_get_peer_cert(&mb->ctx) == 0) {
            sp->errorMsg = sclone("Peer did not provide a certificate");
        }
        sp->flags |= MPR_SOCKET_EOF | MPR_SOCKET_CERT_ERROR;
        errno = EPROTO;
        return MPR_ERR_CANT_READ;
    }
    if (getPeerCertInfo(sp) < 0) {
        return MPR_ERR_CANT_CONNECT;
    }
    sp->secured = 1;
    return 1;
}


static int getPeerCertInfo(MprSocket *sp)
{
    MbedSocket              *mb;
    MprBuf                  *buf;
    mbedtls_ssl_context     *ctx;
    const mbedtls_x509_crt  *peer;
    mbedtls_ssl_session     *session;
    ssize                   len;
    int                     i;
    char                    cbuf[5120], *cp, *end;

    mb = (MbedSocket*) sp->sslSocket;
    ctx = &mb->ctx;

    /*
        Get peer details
     */
    if ((peer = mbedtls_ssl_get_peer_cert(ctx)) != 0) {
        mbedtls_x509_dn_gets(cbuf, sizeof(cbuf), &peer->subject);
        sp->peerCert = sclone(cbuf);
        /*
            Extract the common name for the peer name
         */
        if ((cp = scontains(cbuf, "CN=")) != 0) {
            cp += 3;
            if ((end = schr(cp, ',')) != 0) {
                sp->peerName = snclone(cp, end - cp);
            } else {
                sp->peerName = sclone(cp);
            }
        }
        mbedtls_x509_dn_gets(cbuf, sizeof(cbuf), &peer->issuer);
        sp->peerCertIssuer = sclone(cbuf);

        if (mprGetLogLevel() >= 6) {
            char buf[4096];
            mbedtls_x509_crt_info(buf, sizeof(buf) - 1, "", peer);
            mprLog("info mbedtls", mbedLogLevel, "Peer certificate\n%s", buf);
        }
    }
    sp->cipher = replaceHyphen(sclone(mbedtls_ssl_get_ciphersuite(ctx)), '-', '_');

    /*
        Convert session into a string
     */
    session = ctx->session;
    if (session->start && session->ciphersuite) {
        len = session->id_len;
        if (len > 0) {
            buf = mprCreateBuf(len, 0);
            for (i = 0; i < len; i++) {
                mprPutToBuf(buf, "%02X", (uchar) session->id[i]);
            }
            sp->session = mprBufToString(buf);
        } else {
            sp->session = sclone("ticket");
        }
    }
    return 0;
}

static int preloadMbed(MprSsl *ssl, int flags)
{
    mprLog("error mpr ssl openssl", 4, "Preload not yet supported with MbedTLS");
    return 0;
}

/*
    Return the number of bytes read. Return -1 on errors and EOF. Distinguish EOF via mprIsSocketEof.
    If non-blocking, may return zero if no data or still handshaking.
 */
static ssize readMbed(MprSocket *sp, void *buf, ssize len)
{
    MbedSocket  *mb;
    int         rc;

    mb = (MbedSocket*) sp->sslSocket;
    assert(mb);
    assert(mb->cfg);

    if (sp->fd == INVALID_SOCKET) {
        return MPR_ERR_CANT_READ;
    }
    if (mb->ctx.state != MBEDTLS_SSL_HANDSHAKE_OVER) {
        if ((rc = handshakeMbed(sp)) <= 0) {
            return rc;
        }
    }
    while (1) {
        rc = mbedtls_ssl_read(&mb->ctx, buf, (int) len);
        mprDebug("debug mpr ssl mbedtls", mbedLogLevel, "readMbed read returned %d (0x%04x)", rc, rc);
        if (rc < 0) {
            if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE)  {
                rc = 0;
                break;

            } else if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                mprDebug("debug mpr ssl mbedtls", mbedLogLevel, "connection was closed gracefully");
                sp->flags |= MPR_SOCKET_EOF;
                return MPR_ERR_CANT_READ;

            } else {
                mprDebug("debug mpr ssl mbedtls", 4, "readMbed: error -0x%x", -rc);
                sp->flags |= MPR_SOCKET_EOF;
                return MPR_ERR_CANT_READ;
            }
        } else if (rc == 0) {
            sp->flags |= MPR_SOCKET_EOF;
            return MPR_ERR_CANT_READ;
        }
        break;
    }
    mprHiddenSocketData(sp, mbedtls_ssl_get_bytes_avail(&mb->ctx), MPR_READABLE);
    return rc;
}


/*
    Write data. Return the number of bytes written or -1 on errors or socket closure.
    If non-blocking, may return zero if no data or still handshaking.
 */
static ssize writeMbed(MprSocket *sp, cvoid *buf, ssize len)
{
    MbedSocket  *mb;
    ssize       totalWritten;
    int         rc;

    mb = (MbedSocket*) sp->sslSocket;
    if (len <= 0) {
        assert(0);
        return MPR_ERR_BAD_ARGS;
    }
    if (mb->ctx.state != MBEDTLS_SSL_HANDSHAKE_OVER) {
        if ((rc = handshakeMbed(sp)) <= 0) {
            return rc;
        }
    }
    totalWritten = 0;
    rc = 0;
    do {
        rc = mbedtls_ssl_write(&mb->ctx, (uchar*) buf, (int) len);
        mprDebug("debug mpr ssl mbedtls", mbedLogLevel, "mbedtls write: write returned %d (0x%04x), len %zd", rc, rc, len);
        if (rc <= 0) {
            if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) {
                break;
            }
            if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
                mprDebug("debug mpr ssl mbedtls", mbedLogLevel, "ssl_write peer closed connection");
                return MPR_ERR_CANT_WRITE;
            } else {
                mprDebug("debug mpr ssl mbedtls", mbedLogLevel, "ssl_write failed rc -0x%x", -rc);
                return MPR_ERR_CANT_WRITE;
            }
        } else {
            totalWritten += rc;
            buf = (void*) ((char*) buf + rc);
            len -= rc;
        }
    } while (len > 0);

    mprHiddenSocketData(sp, mb->ctx.out_left, MPR_WRITABLE);

    if (totalWritten == 0 && (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE))  {
        mprSetError(EAGAIN);
        return MPR_ERR_CANT_WRITE;
    }
    return totalWritten;
}


/*
    Invoked by mbedtls in response to SNI extensions in the client hello
 */
PUBLIC int sniCallback(void *unused, mbedtls_ssl_context *ctx, cuchar *host, size_t len)
{
    MprSocket   *sp;
    MprSsl      *ssl;
    MbedSocket  *mb;
    MbedConfig  *cfg;
    cchar       *hostname;
    int         verify;

    sp = ctx->appData;
    hostname = snclone((char*) host, len);

    /*
        Select the appropriate configuration for this hostname
     */
    if ((ssl = (sp->ssl->matchSsl)(sp, hostname)) == 0) {
        return MPR_ERR_CANT_FIND;
    }
    lock(ssl);
    if (configMbed(ssl, sp->flags, &sp->errorMsg) < 0) {
        unlock(ssl);
        return MPR_ERR_CANT_INITIALIZE;
    }
    unlock(ssl);
    sp->ssl = ssl;
    mb = sp->sslSocket;

    //  should not need this flag
    assert(!mb->configured);

    if (!mb->configured) {
        assert(ctx->handshake);
        cfg = ssl->config;
        mbedtls_ssl_set_hs_own_cert(ctx, &cfg->cert, &cfg->key);
        if (ssl->caFile) {
            verify = ssl->verifyPeer ? MBEDTLS_SSL_VERIFY_OPTIONAL : MBEDTLS_SSL_VERIFY_NONE;
            mbedtls_ssl_set_hs_authmode(ctx, verify);
            mbedtls_ssl_set_hs_ca_chain(ctx, &cfg->ca, &cfg->revoke);
        }
        if (cfg->ciphers) {
            mbedtls_ssl_conf_ciphersuites(&cfg->conf, cfg->ciphers);
        }
        mb->configured = 1;
    }
    return 0;
}


static void putCertToBuf(MprBuf *buf, cchar *key, const mbedtls_x509_name *dn)
{
    const mbedtls_x509_name *np;
    cchar                   *name;
    char                    value[MBEDTLS_X509_MAX_DN_NAME_SIZE];
    ssize                   i;
    uchar                   c;
    int                     ret;

    mprPutToBuf(buf, "\"%s\":{", key);
    for (np = dn; np; np = np->next) {
        if (!np->oid.p) {
            np = np->next;
            continue;
        }
        name = 0;
        if ((ret = mbedtls_oid_get_attr_short_name(&np->oid, &name)) < 0) {
            continue;
        }
        if (smatch(name, "emailAddress")) {
            name = "email";
        }
        mprPutToBuf(buf, "\"%s\":", name);

        for(i = 0; i < np->val.len; i++) {
            if (i >= sizeof(value) - 1) {
                break;
            }
            c = np->val.p[i];
            value[i] = (c < 32 || c == 127 || (c > 128 && c < 160)) ? '?' : c;
        }
        value[i] = '\0';
        mprPutToBuf(buf, "\"%s\",", value);
    }
    mprAdjustBufEnd(buf, -1);
    mprPutToBuf(buf, "},");
}


static void formatCert(MprBuf *buf, mbedtls_x509_crt *crt)
{
    char        text[1024];

    mprPutToBuf(buf, "\"version\":%d,", crt->version);

    mbedtls_x509_serial_gets(text, sizeof(text), &crt->serial);
    mprPutToBuf(buf, "\"serial\":\"%s\",", text);

    putCertToBuf(buf, "issuer", &crt->issuer);
    putCertToBuf(buf, "subject", &crt->subject);

    mprPutToBuf(buf, "\"issued\":\"%04d-%02d-%02d %02d:%02d:%02d\",", crt->valid_from.year, crt->valid_from.mon,
        crt->valid_from.day, crt->valid_from.hour, crt->valid_from.min, crt->valid_from.sec);

    mprPutToBuf(buf, "\"expires\":\"%04d-%02d-%02d %02d:%02d:%02d\",", crt->valid_to.year, crt->valid_to.mon,
        crt->valid_to.day, crt->valid_to.hour, crt->valid_to.min, crt->valid_to.sec);

    mbedtls_x509_sig_alg_gets(text, sizeof(text), &crt->sig_oid, crt->sig_pk, crt->sig_md, crt->sig_opts);
    mprPutToBuf(buf, "\"signed\":\"%s\",", text);

    mprPutToBuf(buf, "\"keysize\": %d,", (int) mbedtls_pk_get_bitlen(&crt->pk));

    if (crt->ext_types & MBEDTLS_X509_EXT_BASIC_CONSTRAINTS) {
        mprPutToBuf(buf, "\"constraints\": \"CA=%s\",", crt->ca_istrue ? "true" : "false");
    }
    mprAdjustBufEnd(buf, -1);
}


/*
    Called to log the MbedTLS socket / certificate state
 */
static char *getMbedState(MprSocket *sp)
{
    MbedSocket              *mb;
    MbedConfig              *cfg;
    mbedtls_ssl_context     *ctx;
    mbedtls_ssl_session     *session;
    MprBuf                  *buf;

    if ((mb = sp->sslSocket) == 0) {
        return 0;
    }
    ctx = &mb->ctx;
    if ((session = ctx->session) == 0) {
        return 0;
    }
    cfg = sp->ssl->config;

    buf = mprCreateBuf(0, 0);
    mprPutToBuf(buf, "{");
    mprPutToBuf(buf, "\"provider\":\"mbedtls\",\"cipher\":\"%s\",\"session\":\"%s\",",
        mbedtls_ssl_get_ciphersuite(ctx), sp->session);

    mprPutToBuf(buf, "\"peer\":\"%s\",", sp->peerName ? sp->peerName : "");
    if (session->peer_cert) {
        mprPutToBuf(buf, "\"%s\":{", sp->acceptIp ? "client" : "server");
        formatCert(buf, session->peer_cert);
        mprPutToBuf(buf, "},");
    }
    if (cfg->conf.key_cert && cfg->conf.key_cert->cert) {
        mprPutToBuf(buf, "\"%s\":{", sp->acceptIp ? "server" : "client");
        formatCert(buf, cfg->conf.key_cert->cert);
        mprPutToBuf(buf, "},");
    }
    mprAdjustBufEnd(buf, -1);
    mprPutToBuf(buf, "}");
    return mprBufToString(buf);
}


/*
    Convert string of IANA ciphers into a list of cipher codes
 */
static int *getCipherSuite(MprSsl *ssl)
{
    cchar   *ciphers;
    char    *cipher, *next, buf[128];
    cint    *cp;
    int     nciphers, i, *result, code;

    result = 0;
    ciphers = ssl->ciphers;
    if (ciphers && *ciphers) {
        for (nciphers = 0, cp = mbedtls_ssl_list_ciphersuites(); cp && *cp; cp++, nciphers++) { }
        result = mprAlloc((nciphers + 1) * sizeof(int));

        next = sclone(ciphers);
        for (i = 0; (cipher = stok(next, ":, \t", &next)) != 0; ) {
            replaceHyphen(cipher, '_', '-');
            if ((code = mbedtls_ssl_get_ciphersuite_id(cipher)) <= 0) {
                mprLog("error mpr", 0, "Unsupported cipher %s", cipher);
                continue;
            }
            result[i++] = code;
        }
        result[i] = 0;
    }
    if (mprGetLogLevel() >= 5) {
        static int once = 0;
        if (!once++) {
            cp = (ciphers && *ciphers) ? result : mbedtls_ssl_list_ciphersuites();
            mprLog("info mbedtls", mbedLogLevel, "\nCiphers:");
            for (; *cp; cp++) {
                scopy(buf, sizeof(buf), mbedtls_ssl_get_ciphersuite_name(*cp));
                replaceHyphen(buf, '-', '_');
                mprLog("info mbedtls", mbedLogLevel, "0x%04X %s", *cp, buf);
            }
        }
    }
    return result;
}


static int parseCert(mbedtls_x509_crt *cert, cchar *path, char **errorMsg)
{
    uchar   *buf;
    ssize   len;

    if ((buf = (uchar*) mprReadPathContents(path, &len)) == 0) {
        if (errorMsg) {
            *errorMsg = sfmt("Unable to read certificate %s", path);
        }
        return MPR_ERR_CANT_INITIALIZE;
    }
    if (scontains((char*) buf, "-----BEGIN ")) {
        /* Looks PEM encoded so count the null in the length */
        len++;
    }
    if (mbedtls_x509_crt_parse(cert, buf, len) != 0) {
        memset(buf, 0, len);
        if (errorMsg) {
            *errorMsg = sfmt("Unable to parse certificate %s", path);
        }
        return MPR_ERR_CANT_INITIALIZE;
    }
    memset(buf, 0, len);
    return 0;
}


static int parseKey(mbedtls_pk_context *key, cchar *path, char **errorMsg)
{
    uchar   *buf;
    ssize   len;

    if ((buf = (uchar*) mprReadPathContents(path, &len)) == 0) {
        if (errorMsg) {
            *errorMsg = sfmt("Unable to read key %s", path);
        }
        return MPR_ERR_CANT_INITIALIZE;
    }
    if (scontains((char*) buf, "-----BEGIN ")) {
        len++;
    }
    if (mbedtls_pk_parse_key(key, buf, len, NULL, 0) != 0) {
        memset(buf, 0, len);
        if (errorMsg) {
            *errorMsg = sfmt("Unable to parse key %s", path);
        }
        return MPR_ERR_CANT_INITIALIZE;
    }
    memset(buf, 0, len);
    return 0;
}


static int parseCrl(mbedtls_x509_crl *crl, cchar *path, char **errorMsg)
{
    uchar   *buf;
    ssize   len;

    if ((buf = (uchar*) mprReadPathContents(path, &len)) == 0) {
        if (errorMsg) {
            *errorMsg = sfmt("Unable to read crl %s", path);
        }
        return MPR_ERR_CANT_INITIALIZE;
    }
    if (sstarts((char*) buf, "-----BEGIN ")) {
        len++;
    }
    if (mbedtls_x509_crl_parse(crl, buf, len) != 0) {
        memset(buf, 0, len);
        if (errorMsg) {
            *errorMsg = sfmt("Unable to parse crl %s", path);
        }
        return MPR_ERR_CANT_INITIALIZE;
    }
    memset(buf, 0, len);
    return 0;
}


static void initMbedLock(mbedtls_threading_mutex_t *tm)
{
    MprMutex    *lock;

    lock = mprCreateLock();
    mprHold(lock);
    *tm = lock;
}


static void freeMbedLock(mbedtls_threading_mutex_t *tm)
{
    mprRelease(*tm);
}


static int mbedLock(mbedtls_threading_mutex_t *tm)
{
    mprLock(*tm);
    return 0;
}


static int mbedUnlock(mbedtls_threading_mutex_t *tm)
{
    mprUnlock(*tm);
    return 0;
}


/*
    Trace from within MbedTLS
 */
static void traceMbed(void *context, int level, cchar *file, int line, cchar *str)
{
    level += mbedLogLevel;
    if (level <= MPR->logLevel) {
        mprLog("info mbedtls", level, "%s", str);
    }
}


static void merror(int rc, cchar *fmt, ...)
{
    va_list     ap;
    char        ebuf[ME_BUFSIZE];

    va_start(ap, fmt);
    mbedtls_strerror(-rc, ebuf, sizeof(ebuf));
    mprLog("error mbedtls ssl", 0, "mbedtls error: 0x%x %s %s", rc, sfmtv(fmt, ap), ebuf);
    va_end(ap);
}


static char *replaceHyphen(char *cipher, char from, char to)
{
    char    *cp;

    for (cp = cipher; *cp; cp++) {
        if (*cp == from) {
            *cp = to;
        }
    }
    return cipher;
}

#endif /* ME_COM_MBEDTLS */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
