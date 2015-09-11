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
    mbedtls_ssl_cache_context   cache;          /* Session cache context */
    mbedtls_ssl_config          conf;           /* SSL configuration */
    mbedtls_x509_crt            cert;           /* Certificate (own) */
    mbedtls_ctr_drbg_context    ctr;            /* Counter random generator state */
    mbedtls_ssl_ticket_context  tickets;        /* Session tickets */
    mbedtls_pk_context          pkey;           /* Private key */
    mbedtls_x509_crl            revoke;         /* Certificate revoke list */
    int                         *ciphers;       /* Set of acceptable ciphers - null terminated */
} MbedConfig;

/*
    Per socket state
 */
typedef struct MbedSocket {
    MbedConfig                  *cfg;           /* Configuration */
    MprSocket                   *sock;          /* MPR socket object */
    mbedtls_ssl_context         ctx;            /* SSL state */
} MbedSocket;

static mbedtls_entropy_context  mbedEntropy;    /* Entropy context */

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
static void     manageMbedProvider(MprSocketProvider *provider, int flags);
static void     manageMbedSocket(MbedSocket*ssp, int flags);
static int      mbedLock(mbedtls_threading_mutex_t *tm);
static void     mbedTerminator(int state, int how, int status);
static int      mbedUnlock(mbedtls_threading_mutex_t *tm);
static void     merror(int rc, cchar *fmt, ...);
static ssize    readMbed(MprSocket *sp, void *buf, ssize len);
static char     *replaceHyphen(char *cipher, char from, char to);
static void     traceMbed(void *context, int level, cchar *file, int line, cchar *str);
static int      upgradeMbed(MprSocket *sp, MprSsl *sslConfig, cchar *peerName);
static ssize    writeMbed(MprSocket *sp, cvoid *buf, ssize len);

/************************************* Code ***********************************/
/*
    Create the Mbedtls module. This is called only once
 */
PUBLIC int mprSslInit(void *unused, MprModule *module)
{
    if ((mbedProvider = mprAllocObj(MprSocketProvider, manageMbedProvider)) == NULL) {
        return MPR_ERR_MEMORY;
    }
    mbedProvider->name = sclone("mbedtls");
    mbedProvider->upgradeSocket = upgradeMbed;
    mbedProvider->closeSocket = closeMbed;
    mbedProvider->disconnectSocket = disconnectMbed;
    mbedProvider->readSocket = readMbed;
    mbedProvider->writeSocket = writeMbed;
    mbedProvider->socketState = getMbedState;
    mprSetSslProvider(mbedProvider);
    mprAddTerminator(mbedTerminator);

    mbedtls_threading_set_alt(initMbedLock, freeMbedLock, mbedLock, mbedUnlock);
    mbedtls_entropy_init(&mbedEntropy);
    return 0;
}


void terminateMbed()
{
    mbedtls_entropy_free(&mbedEntropy);
}


static void mbedTerminator(int state, int how, int status)
{
    if (state >= MPR_STOPPED) {
        terminateMbed();
    }
}


static void manageMbedProvider(MprSocketProvider *provider, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(provider->name);

    } else if (flags & MPR_MANAGE_FREE) {
        ;
    }
}


static void manageMbedConfig(MbedConfig *cfg, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(cfg->ciphers);

    } else if (flags & MPR_MANAGE_FREE) {
        mbedtls_ctr_drbg_free(&cfg->ctr);
        mbedtls_pk_free(&cfg->pkey);
        mbedtls_x509_crt_free(&cfg->cert);
        mbedtls_x509_crt_free(&cfg->ca);
        mbedtls_x509_crl_free(&cfg->revoke);
        mbedtls_ssl_cache_free(&cfg->cache);
        mbedtls_ssl_config_free(&cfg->conf);
        mbedtls_ssl_ticket_free(&cfg->tickets);
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


/*
    Create and initialize an SSL configuration for a route. This configuration is used by all requests for
    a given route. An application can have different SSL configurations for different routes. There is also
    a default SSL configuration that is used when a route does not define a configuration and one for clients.
 */
static MbedConfig *createMbedConfig(MprSocket *sp)
{
    MprSsl              *ssl;
    MbedConfig          *cfg;
    mbedtls_ssl_config  *conf;
    cchar               *name;
    int                 rc;

    ssl = sp->ssl;
    assert(ssl);

    if ((cfg = mprAllocObj(MbedConfig, manageMbedConfig)) == 0) {
        return 0;
    }
    conf = &cfg->conf;
    mbedtls_ssl_config_init(conf);

    mbedtls_ssl_conf_dbg(conf, traceMbed, NULL);
    mbedLogLevel = ssl->logLevel;
    if (MPR->logLevel >= mbedLogLevel) {
        mbedtls_debug_set_threshold(MPR->logLevel - mbedLogLevel);
    }
    mbedtls_pk_init(&cfg->pkey);
    mbedtls_ssl_cache_init(&cfg->cache);
    mbedtls_ctr_drbg_init(&cfg->ctr);
    mbedtls_x509_crt_init(&cfg->cert);
    mbedtls_ssl_ticket_init(&cfg->tickets);

    name = mprGetAppName();
    if ((rc = mbedtls_ctr_drbg_seed(&cfg->ctr, mbedtls_entropy_func, &mbedEntropy, (cuchar*) name, slen(name))) < 0) {
        merror(rc, "Cannot seed rng");
        return 0;
    }
    if (ssl->certFile) {
        /*
            Load a PEM format certificate file
         */
        if (mbedtls_x509_crt_parse_file(&cfg->cert, (char*) ssl->certFile) != 0) {
            sp->errorMsg = sfmt("Unable to parse certificate %s", ssl->certFile); 
            return 0;
        }
    }
    if (ssl->keyFile) {
        /*
            Load a decrypted PEM format private key
            Last arg is password if you need to use an encrypted private key
         */
        if (mbedtls_pk_parse_keyfile(&cfg->pkey, (char*) ssl->keyFile, 0) != 0) {
            sp->errorMsg = sfmt("Unable to parse key file %s", ssl->keyFile); 
            return 0;
        }
    }
    if (ssl->verifyPeer) {
        if (!ssl->caFile) {
            sp->errorMsg = sclone("No defined certificate authority file");
            return 0;
        }
        if (mbedtls_x509_crt_parse_file(&cfg->ca, (char*) ssl->caFile) != 0) {
            sp->errorMsg = sfmt("Unable to open or parse certificate authority file %s", ssl->caFile); 
            return 0;
        }
    }
    if (ssl->revoke) {
        /*
            Load a PEM format certificate file
         */
        if (mbedtls_x509_crl_parse_file(&cfg->revoke, (char*) ssl->revoke) != 0) {
            sp->errorMsg = sfmt("Unable to parse revoke list %s", ssl->revoke); 
            return 0;
        }
    }

    cfg->ciphers = getCipherSuite(ssl);
    if ((rc = mbedtls_ssl_config_defaults(conf, 
            sp->flags & MPR_SOCKET_SERVER ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) < 0) {
        merror(rc, "Cannot set mbedtls defaults");
        return 0;
    }
    mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, &cfg->ctr);

    /*
        Configure larger DH parameters 
     */
    if ((rc = mbedtls_ssl_conf_dh_param(conf, MBEDTLS_DHM_RFC5114_MODP_2048_P, MBEDTLS_DHM_RFC5114_MODP_2048_G)) < 0) {
        merror(rc, "Cannot set DH params");
        return 0;
    }

    /*
        Configure ticket-based sessions
     */
    if (sp->flags & MPR_SOCKET_SERVER) {
        if (ssl->ticket) {
            if ((rc = mbedtls_ssl_ticket_setup(&cfg->tickets, mbedtls_ctr_drbg_random, &cfg->ctr, 
                    MBEDTLS_CIPHER_AES_256_GCM, (int) ssl->sessionTimeout)) < 0) {
                merror(rc, "Cannot setup ticketing sessions");
                return 0;
            }
            mbedtls_ssl_conf_session_tickets_cb(conf, mbedtls_ssl_ticket_write, mbedtls_ssl_ticket_parse, &cfg->tickets);
        }
    } else {
        mbedtls_ssl_conf_session_tickets(conf, 1);
    }

    /*
        Set auth mode if peer cert should be verified
     */
    mbedtls_ssl_conf_authmode(conf, ssl->verifyPeer ? MBEDTLS_SSL_VERIFY_OPTIONAL : MBEDTLS_SSL_VERIFY_NONE);

    /*
        Configure server-side session cache
     */
    if (ssl->cacheSize > 0) {
        mbedtls_ssl_conf_session_cache(conf, &cfg->cache, mbedtls_ssl_cache_get, mbedtls_ssl_cache_set);
        mbedtls_ssl_cache_set_max_entries(&cfg->cache, ssl->cacheSize);
        mbedtls_ssl_cache_set_timeout(&cfg->cache, (int) (ssl->sessionTimeout / TPS));
    }

    /*
        Configure explicit cipher suite selection
     */
    if (cfg->ciphers) {
        mbedtls_ssl_conf_ciphersuites(conf, cfg->ciphers);
    }

    /*
        Configure CA certificate bundle and revocation list and optional expected peer name
     */
    mbedtls_ssl_conf_ca_chain(conf, ssl->caFile ? &cfg->ca : NULL, ssl->revoke ? &cfg->revoke : NULL);

    /*
        Configure server cert and key
     */
    if (ssl->keyFile && ssl->certFile) {
        if ((rc = mbedtls_ssl_conf_own_cert(conf, &cfg->cert, &cfg->pkey)) < 0) {
            merror(rc, "Cannot define certificate and private key");
            return 0;
        }
    }
    return cfg;
}


/*
    Upgrade a standard socket to use TLS
 */
static int upgradeMbed(MprSocket *sp, MprSsl *ssl, cchar *peerName)
{
    MbedSocket          *mb;
    MbedConfig          *cfg;
    mbedtls_ssl_context *ctx;

    assert(sp);

    if (ssl == 0) {
        ssl = mprCreateSsl(sp->flags & MPR_SOCKET_SERVER);
    }
    sp->ssl = ssl;

    if (ssl->config && !ssl->changed) {
        cfg = ssl->config;
    } else {
        /*
            On-demand creation of the SSL configuration
         */
        if ((cfg = createMbedConfig(sp)) == 0) {
            return MPR_ERR_CANT_INITIALIZE;
        }
        ssl->config = cfg;
        ssl->changed = 0;
    }
    if ((mb = (MbedSocket*) mprAllocObj(MbedSocket, manageMbedSocket)) == 0) {
        return MPR_ERR_MEMORY;
    }
    sp->sslSocket = mb;
    mb->cfg = cfg;
    mb->sock = sp;
    ctx = &mb->ctx;

    mbedtls_ssl_init(ctx);
    mbedtls_ssl_setup(ctx, &cfg->conf);
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
#if UNUSED
        } else if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            sp->errorMsg = sclone("Connection closed gracefully");
        } else if (rc == MBEDTLS_ERR_SSL_NO_CIPHER_CHOSEN) {
            sp->errorMsg = sclone("No ciphers in common");
        } else if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
            sp->errorMsg = sclone("Connection reset");
#endif
        } else {
            char ebuf[256];
            mbedtls_strerror(-rc, ebuf, sizeof(ebuf));
            sp->errorMsg = sfmt("%s: error -0x%x", ebuf, -rc);
        }
        sp->flags |= MPR_SOCKET_EOF;
        errno = EPROTO;
        return MPR_ERR_CANT_READ;
    } 
    if ((vrc = mbedtls_ssl_get_verify_result(&mb->ctx)) != 0) {
        if (vrc & MBEDTLS_X509_BADCERT_MISSING) {
            sp->errorMsg = sclone("Certificate missing");

        } if (vrc & MBEDTLS_X509_BADCERT_EXPIRED) {
            sp->errorMsg = sclone("Certificate expired");

        } else if (vrc & MBEDTLS_X509_BADCERT_REVOKED) {
            sp->errorMsg = sclone("Certificate revoked");

        } else if (vrc & MBEDTLS_X509_BADCERT_CN_MISMATCH) {
            sp->errorMsg = sclone("Certificate common name mismatch");

        } else if (vrc & MBEDTLS_X509_BADCERT_KEY_USAGE || vrc & MBEDTLS_X509_BADCERT_EXT_KEY_USAGE) {
            sp->errorMsg = sclone("Unauthorized key use in certificate");

        } else if (vrc & MBEDTLS_X509_BADCERT_NOT_TRUSTED) {
            sp->errorMsg = sclone("Certificate not trusted");
            if (!sp->ssl->verifyIssuer) {
                vrc = 0;
            }

        } else if (vrc & MBEDTLS_X509_BADCERT_SKIP_VERIFY) {
            /* 
                MBEDTLS_SSL_VERIFY_NONE requested, so ignore error
             */
            vrc = 0;

        } else {
            if (mb->ctx.client_auth && !sp->ssl->certFile) {
                sp->errorMsg = sclone("Server requires a client certificate");
            } else if (rc == MBEDTLS_ERR_NET_CONN_RESET) {
                sp->errorMsg = sclone("Peer disconnected");
            } else {
                sp->errorMsg = sfmt("Cannot handshake: error -0x%x", -rc);
            }
        }
    }
    if (vrc != 0 && sp->ssl->verifyPeer) {
        if (mbedtls_ssl_get_peer_cert(&mb->ctx) == 0) {
            sp->errorMsg = sclone("Peer did not provide a certificate");
        }
        sp->flags |= MPR_SOCKET_EOF;
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

        if (mprGetLogLevel() >= 5) {
            char buf[4096];
            mbedtls_x509_crt_info(buf, sizeof(buf) - 1, "", peer);
            mprLog("info mbedtls", 5, "Peer certificate\n%s", buf);
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
        mprDebug("debug mpr ssl mbedtls", mbedLogLevel, "readMbed %d", rc);
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
        mprDebug("debug mpr ssl mbedtls", 6, "mbedtls write: wrote %d of %zd", rc, len);
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
    char                    *ownPrefix, *peerPrefix;
    char                    cbuf[5120];

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
    int     nciphers, i, *result, code, count;

    result = 0;
    if (mprGetLogLevel() >= 5) {
        static int once = 0;
        if (!once++) {
            mprLog("info mbedtls", 5, "\nMbedTLS Ciphers:");
            for (cp = mbedtls_ssl_list_ciphersuites(); *cp; cp++) {
                scopy(buf, sizeof(buf), mbedtls_ssl_get_ciphersuite_name(*cp));
                replaceHyphen(buf, '-', '_');
                mprLog("info mbedtls", 5, "%s (0x%04X)", buf, *cp);
            }
        }
    }
    ciphers = ssl->ciphers;
    if (ciphers && *ciphers) {
        for (nciphers = 0, cp = mbedtls_ssl_list_ciphersuites(); cp && *cp; cp++, nciphers++) { }
        result = mprAlloc((nciphers + 1) * sizeof(int));

        next = sclone(ciphers);
        for (i = 0; (cipher = stok(next, ":, \t", &next)) != 0; ) {
            replaceHyphen(cipher, '_', '-');
            if ((code = mbedtls_ssl_get_ciphersuite_id(cipher)) <= 0) {
                mprLog("error mpr", 0, "Unsupported cipher \"%s\"", cipher);
                continue;
            }
            result[i++] = code;
        }
        result[i] = 0;
        count = i;
        mprLog("info mbedtls", 5, "\nSelected Ciphers:");
        for (i = 0; i < count; i++) {
            scopy(buf, sizeof(buf), mbedtls_ssl_get_ciphersuite_name(result[i]));
            replaceHyphen(buf, '-', '_');
            mprLog("info mbedtls", 5, "%s (0x%04X)", buf, result[i]);
        }
    } else {
        result = 0;
    }
    return result;
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
    char        ebuf[ME_MAX_BUFFER];

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
