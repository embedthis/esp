/*
    openssl.c - Support for secure sockets via OpenSSL

    This is the interface between the MPR Socket layer and the OpenSSL stack.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "mpr.h"

#if ME_COM_OPENSSL

#if ME_UNIX_LIKE
    /*
        Mac OS X OpenSSL stack is deprecated. Suppress those warnings.
     */
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/* Clashes with WinCrypt.h */
#undef OCSP_RESPONSE

 /*
    Indent includes to bypass MakeMe dependencies
  */
 #include    <openssl/ssl.h>
 #include    <openssl/evp.h>
 #include    <openssl/rand.h>
 #include    <openssl/err.h>
 #include    <openssl/dh.h>

/************************************* Defines ********************************/
/*
    Default ciphers from Mozilla (https://wiki.mozilla.org/Security/Server_Side_TLS) without SSLv3 ciphers.
    TLSv1 and TLSv2 only. Recommended RSA and DH parameter size: 2048 bits.
 */
#define OPENSSL_DEFAULT_CIPHERS "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA:ECDHE-RSA-AES128-SHA:DHE-RSA-AES256-SHA256:DHE-RSA-AES128-SHA256:DHE-RSA-AES256-SHA:DHE-RSA-AES128-SHA:ECDHE-RSA-DES-CBC3-SHA:EDH-RSA-DES-CBC3-SHA:AES256-GCM-SHA384:AES128-GCM-SHA256:AES256-SHA256:AES128-SHA256:AES256-SHA:AES128-SHA:DES-CBC3-SHA:HIGH:!aNULL:!eNULL:!EXPORT:!DES:!MD5:!PSK:!RC4:!SSLv3"

/*
    Configuration for a route/host
 */
typedef struct OpenConfig {
    SSL_CTX         *context;
    DH              *dhKey;
} OpenConfig;

typedef struct OpenSocket {
    MprSocket       *sock;
    OpenConfig      *cfg;
    char            *requiredPeerName;
    SSL             *handle;
    BIO             *bio;
} OpenSocket;

typedef struct RandBuf {
    MprTime     now;
    int         pid;
} RandBuf;

static MprSocketProvider *openProvider;
static OpenConfig *defaultOpenConfig;

/*
    OpenSSL uses shared data and will crash if multithread locks are not used
 */
static int      numLocks;
static MprMutex **olocks;

struct CRYPTO_dynlock_value {
    MprMutex    *mutex;
};
typedef struct CRYPTO_dynlock_value DynLock;

#ifndef ME_MPR_SSL_CURVE
    #define ME_MPR_SSL_CURVE "prime256v1"
#endif

/***************************** Forward Declarations ***************************/

static void     closeOss(MprSocket *sp, bool gracefully);
static int      checkCertificateName(MprSocket *sp);
static OpenConfig *createOpenSslConfig(MprSocket *sp);
static DH       *dhcallback(SSL *ssl, int isExport, int keyLength);
static void     disconnectOss(MprSocket *sp);
static ssize    flushOss(MprSocket *sp);
static DH       *getDhKey(cchar *path);
static char     *getOssState(MprSocket *sp);
static char     *getOssError(MprSocket *sp);
static void     manageOpenConfig(OpenConfig *cfg, int flags);
static void     manageOpenProvider(MprSocketProvider *provider, int flags);
static void     manageOpenSocket(OpenSocket *ssp, int flags);
static ssize    readOss(MprSocket *sp, void *buf, ssize len);
static DynLock  *sslCreateDynLock(cchar *file, int line);
static void     sslDynLock(int mode, DynLock *dl, cchar *file, int line);
static void     sslDestroyDynLock(DynLock *dl, cchar *file, int line);
static void     sslStaticLock(int mode, int n, cchar *file, int line);
static ulong    sslThreadId(void);
static int      upgradeOss(MprSocket *sp, MprSsl *ssl, cchar *requiredPeerName);
static int      verifyPeerCertificate(int ok, X509_STORE_CTX *ctx);
static ssize    writeOss(MprSocket *sp, cvoid *buf, ssize len);


/************************************* Code ***********************************/
/*
    Initialize the MPR SSL layer
 */
PUBLIC int mprSslInit(void *unused, MprModule *module)
{
    RandBuf     randBuf;
    int         i;

    randBuf.now = mprGetTime();
    randBuf.pid = getpid();
    RAND_seed((void*) &randBuf, sizeof(randBuf));
#if ME_UNIX_LIKE
    RAND_load_file("/dev/urandom", 256);
#endif

    if ((openProvider = mprAllocObj(MprSocketProvider, manageOpenProvider)) == NULL) {
        return MPR_ERR_MEMORY;
    }
    openProvider->upgradeSocket = upgradeOss;
    openProvider->closeSocket = closeOss;
    openProvider->disconnectSocket = disconnectOss;
    openProvider->flushSocket = flushOss;
    openProvider->socketState = getOssState;
    openProvider->readSocket = readOss;
    openProvider->writeSocket = writeOss;
    mprAddSocketProvider("openssl", openProvider);

    /*
        Configure the SSL library. Use the crypto ID as a one-time test. This allows
        users to configure the library and have their configuration used instead.
     */
    mprGlobalLock();
    if (CRYPTO_get_id_callback() == 0) {
        numLocks = CRYPTO_num_locks();
        if ((olocks = mprAlloc(numLocks * sizeof(MprMutex*))) == 0) {
            return MPR_ERR_MEMORY;
        }
        for (i = 0; i < numLocks; i++) {
            olocks[i] = mprCreateLock();
        }
        CRYPTO_set_id_callback(sslThreadId);
        CRYPTO_set_locking_callback(sslStaticLock);

        CRYPTO_set_dynlock_create_callback(sslCreateDynLock);
        CRYPTO_set_dynlock_destroy_callback(sslDestroyDynLock);
        CRYPTO_set_dynlock_lock_callback(sslDynLock);
#if !ME_WIN_LIKE
        OpenSSL_add_all_algorithms();
#endif
        /*
            WARNING: SSL_library_init() is not reentrant. Caller must ensure safety.
         */
        SSL_library_init();
        SSL_load_error_strings();
    }
    mprGlobalUnlock();
    return 0;
}


/*
    MPR Garbage collector manager callback for OpenConfig. Called on each GC sweep.
 */
static void manageOpenConfig(OpenConfig *cfg, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        /* Nothing to do  */

    } else if (flags & MPR_MANAGE_FREE) {
        if (cfg->context != 0) {
            SSL_CTX_free(cfg->context);
            cfg->context = 0;
        }
        if (cfg == defaultOpenConfig) {
            if (cfg->dhKey) {
                DH_free(cfg->dhKey);
                cfg->dhKey = 0;
            }
        }
    }
}


/*
    MPR Garbage collector manager callback for MprSocketProvider. Called on each GC sweep.
 */
static void manageOpenProvider(MprSocketProvider *provider, int flags)
{
    int     i;

    if (flags & MPR_MANAGE_MARK) {
        /* Mark global locks */
        if (olocks) {
            mprMark(olocks);
            for (i = 0; i < numLocks; i++) {
                mprMark(olocks[i]);
            }
        }
        mprMark(defaultOpenConfig);
        mprMark(provider->name);

    } else if (flags & MPR_MANAGE_FREE) {
        olocks = 0;
    }
}


/*
    Create and initialize an SSL configuration for a route. This configuration is used by all requests for
    a given route. An application can have different SSL configurations for different routes. There is also 
    a default SSL configuration that is used when a route does not define a configuration and one for clients.
 */
static OpenConfig *createOpenSslConfig(MprSocket *sp)
{
    MprSsl          *ssl;
    OpenConfig      *cfg;
    SSL_CTX         *context;
    cchar           *key;
    uchar           resume[16];
    int             verifyMode;

    ssl = sp->ssl;
    assert(ssl);

    if ((ssl->config = mprAllocObj(OpenConfig, manageOpenConfig)) == 0) {
        return 0;
    }
    cfg = ssl->config;

    if ((context = SSL_CTX_new(SSLv23_method())) == 0) {
        mprLog("error openssl", 0, "Unable to create SSL context"); 
        return 0;
    }
    SSL_CTX_set_app_data(context, (void*) ssl);

    if (ssl->verifyPeer && !(ssl->caFile || ssl->caPath)) {
        sp->errorMsg = sfmt("Cannot verify peer due to undefined CA certificates");
        SSL_CTX_free(context);
        return 0;
    }

    /*
        Configure the certificates
     */
    if (ssl->certFile) {
        if (SSL_CTX_use_certificate_chain_file(context, ssl->certFile) <= 0) {
            if (SSL_CTX_use_certificate_file(context, ssl->certFile, SSL_FILETYPE_ASN1) <= 0) {
                mprLog("error openssl", 0, "Cannot open certificate file: %s", ssl->certFile);
                SSL_CTX_free(context);
                return 0;
            }
        }
        key = (ssl->keyFile == 0) ? ssl->certFile : ssl->keyFile;
        if (key) {
            if (SSL_CTX_use_PrivateKey_file(context, key, SSL_FILETYPE_PEM) <= 0) {
                /* attempt ASN1 for self-signed format */
                if (SSL_CTX_use_PrivateKey_file(context, key, SSL_FILETYPE_ASN1) <= 0) {
                    mprLog("error openssl", 0, "Cannot open private key file: %s", key);
                    SSL_CTX_free(context);
                    return 0;
                }
            }
            if (!SSL_CTX_check_private_key(context)) {
                mprLog("error openssl", 0, "Check of private key file failed: %s", key);
                SSL_CTX_free(context);
                return 0;
            }
        }
    }
    if (!ssl->ciphers) {
        ssl->ciphers = sclone(OPENSSL_DEFAULT_CIPHERS);
        mprLog("info openssl", 2, "Using OpenSSL ciphers: %s", ssl->ciphers);
    }
    if (SSL_CTX_set_cipher_list(context, ssl->ciphers) != 1) {
        sp->errorMsg = sfmt("Unable to set cipher list \"%s\". %s", ssl->ciphers, getOssError(sp)); 
        SSL_CTX_free(context);
        return 0;
    }
    verifyMode = ssl->verifyPeer ? SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT : SSL_VERIFY_NONE;
    if (verifyMode != SSL_VERIFY_NONE) {
        if (!(ssl->caFile || ssl->caPath)) {
            sp->errorMsg = sclone("No defined certificate authority file");
            SSL_CTX_free(context);
            return 0;
        }
        if ((!SSL_CTX_load_verify_locations(context, (char*) ssl->caFile, (char*) ssl->caPath)) ||
                (!SSL_CTX_set_default_verify_paths(context))) {
            sp->errorMsg = sfmt("Unable to set certificate locations: %s: %s", ssl->caFile, ssl->caPath); 
            SSL_CTX_free(context);
            return 0;
        }
        if (ssl->caFile) {
            STACK_OF(X509_NAME) *certNames;
            certNames = SSL_load_client_CA_file(ssl->caFile);
            if (certNames) {
                /*
                    Define the list of CA certificates to send to the client
                    before they send their client certificate for validation
                 */
                SSL_CTX_set_client_CA_list(context, certNames);
            }
        }
#if FUTURE || 1
{
        X509_STORE *store = SSL_CTX_get_cert_store(context);
        if (ssl->revokeList && !X509_STORE_load_locations(store, ssl->revokeList, 0)) {
                mprLog("error openssl", 0, "Cannot load certificate revoke list: %s", ssl->revokeList);
                SSL_CTX_free(context);
                return 0;
        }
}
#endif
        if (sp->flags & MPR_SOCKET_SERVER) {
            SSL_CTX_set_verify_depth(context, ssl->verifyDepth);
        }
    }

    /*
        Define callbacks
     */
    SSL_CTX_set_verify(context, verifyMode, verifyPeerCertificate);

    /*
        Configure DH parameters
     */
    SSL_CTX_set_tmp_dh_callback(context, dhcallback);
    cfg->dhKey = getDhKey(ssl->dhFile);

    /*
        Define default OpenSSL options
     */
    SSL_CTX_set_options(context, SSL_OP_ALL);

    /*
        Ensure we generate a new private key for each connection
     */
    SSL_CTX_set_options(context, SSL_OP_SINGLE_DH_USE);

    /*
        Define a session reuse context
     */
    RAND_bytes(resume, sizeof(resume));
    SSL_CTX_set_session_id_context(context, resume, sizeof(resume));

    /*
        Elliptic Curve initialization
     */
#if SSL_OP_SINGLE_ECDH_USE
    #ifdef SSL_CTX_set_ecdh_auto
        SSL_CTX_set_ecdh_auto(context, 1);
    #else
        {
            EC_KEY  *ecdh;
            cchar   *name;
            int      nid;

            name = ME_MPR_SSL_CURVE;
            if ((nid = OBJ_sn2nid(name)) == 0) {
                sp->errorMsg = sfmt("Unknown curve name \"%s\"", name);
                SSL_CTX_free(context);
                return 0;
            }
            if ((ecdh = EC_KEY_new_by_curve_name(nid)) == 0) {
                sp->errorMsg = sfmt("Unable to create curve \"%s\"", name);
                SSL_CTX_free(context);
                return 0;
            }
            SSL_CTX_set_options(context, SSL_OP_SINGLE_ECDH_USE);
            SSL_CTX_set_tmp_ecdh(context, ecdh);
            EC_KEY_free(ecdh);
        }
    #endif
#endif

    SSL_CTX_set_mode(context, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_AUTO_RETRY | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
#ifdef SSL_OP_MSIE_SSLV2_RSA_PADDING
    SSL_CTX_set_options(context, SSL_OP_MSIE_SSLV2_RSA_PADDING);
#endif
#ifdef SSL_MODE_RELEASE_BUFFERS
    SSL_CTX_set_mode(context, SSL_MODE_RELEASE_BUFFERS);
#endif
#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE
    SSL_CTX_set_mode(context, SSL_OP_CIPHER_SERVER_PREFERENCE);
#endif
    /*
        Select the required protocols
        Disable SSLv2 and SSLv3 by default -- they are insecure.
     */
    SSL_CTX_set_options(context, SSL_OP_NO_SSLv2);
    SSL_CTX_set_options(context, SSL_OP_NO_SSLv3);
#ifdef SSL_OP_NO_TLSv1
    if (!(ssl->protocols & MPR_PROTO_TLSV1)) {
        SSL_CTX_set_options(context, SSL_OP_NO_TLSv1);
    }
#endif
#ifdef SSL_OP_NO_TLSv1_1
    if (!(ssl->protocols & MPR_PROTO_TLSV1_1)) {
        SSL_CTX_set_options(context, SSL_OP_NO_TLSv1_1);
    }
#endif
#ifdef SSL_OP_NO_TLSv1_2
    if (!(ssl->protocols & MPR_PROTO_TLSV1_2)) {
        SSL_CTX_set_options(context, SSL_OP_NO_TLSv1_2);
    }
#endif

    /*
        Options set via main.me mpr.ssl.*
     */
#if defined(SSL_OP_NO_TICKET)
    /*
        Ticket based session reuse is enabled by default
     */
    #if defined(ME_MPR_SSL_TICKET)
        if (ME_MPR_SSL_TICKET) {
            SSL_CTX_clear_options(context, SSL_OP_NO_TICKET);
        } else {
            SSL_CTX_set_options(context, SSL_OP_NO_TICKET);
        }
    #else
        SSL_CTX_clear_options(context, SSL_OP_NO_TICKET);
    #endif
#endif

#if defined(SSL_OP_NO_COMPRESSION)
    /* 
        Use of compression is not secure. Disabled by default.
     */
    #if defined(ME_MPR_SSL_COMPRESSION)
        if (ME_MPR_SSL_COMPRESSION) {
            SSL_CTX_clear_options(context, SSL_OP_NO_COMPRESSION);
        } else {
            SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION);
        }
    #else
        /*
            CRIME attack targets compression
         */
        SSL_CTX_clear_options(context, SSL_OP_NO_COMPRESSION);
    #endif
#endif

#if defined(SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION)
    /*
        Force a new session on renegotiation. Default to true.
     */
    #if defined(ME_MPR_SSL_RENEGOTIATE)
        if (ME_MPR_SSL_RENEGOTIATE) {
            SSL_CTX_clear_options(context, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
        } else {
            SSL_CTX_set_options(context, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
        }
    #else
        SSL_CTX_set_options(context, SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
    #endif
#endif

#if defined(SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS)
    /*
        Disables a countermeasure against a SSL 3.0/TLS 1.0 protocol vulnerability affecting CBC ciphers.
        Defaults to true.
     */
    #if defined(ME_MPR_SSL_EMPTY_FRAGMENTS)
        if (ME_MPR_SSL_EMPTY_FRAGMENTS) {
            /* SSL_OP_ALL disables empty fragments. Only needed for ancient browsers like IE-6 on SSL-3.0/TLS-1.0 */
            SSL_CTX_clear_options(context, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
        } else {
            SSL_CTX_set_options(context, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
        }
    #else
        SSL_CTX_set_options(context, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
    #endif
#endif

#if defined(ME_MPR_SSL_CACHE)
    /*
        Set the number of sessions supported. Default in OpenSSL is 20K.
     */
    SSL_CTX_sess_set_cache_size(context, ME_MPR_SSL_CACHE);
#else
    SSL_CTX_sess_set_cache_size(context, 1024);
#endif

    cfg->context = context;
    return cfg;
}


/*
    MPR Garbage collector manager callback for OpenSocket. Called on each GC sweep.
 */
static void manageOpenSocket(OpenSocket *osp, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(osp->sock);
        mprMark(osp->cfg);
        mprMark(osp->requiredPeerName);

    } else if (flags & MPR_MANAGE_FREE) {
        if (osp->handle) {
            SSL_set_shutdown(osp->handle, SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
            SSL_free(osp->handle);
            osp->handle = 0;
        }
    }
}


static void closeOss(MprSocket *sp, bool gracefully)
{
    OpenSocket    *osp;

    osp = sp->sslSocket;
    if (osp->handle) {
        SSL_shutdown(osp->handle);
    }
    sp->service->standardProvider->closeSocket(sp, gracefully);
    if (osp->handle) {
        SSL_free(osp->handle);
        osp->handle = 0;
    }
}


/*
    Upgrade a standard socket to use SSL/TLS. Used by both clients and servers to upgrade a socket for SSL.
    If a client, this may block while connecting.
 */
static int upgradeOss(MprSocket *sp, MprSsl *ssl, cchar *requiredPeerName)
{
    OpenSocket      *osp;
    OpenConfig      *cfg;
    int             rc;

    assert(sp);

    if (ssl == 0) {
        ssl = mprCreateSsl(sp->flags & MPR_SOCKET_SERVER);
    }
    if ((osp = (OpenSocket*) mprAllocObj(OpenSocket, manageOpenSocket)) == 0) {
        return MPR_ERR_MEMORY;
    }
    osp->sock = sp;
    sp->sslSocket = osp;
    sp->ssl = ssl;

    if (!ssl->config || ssl->changed) {
        /*
            On-demand creation of the SSL configuration 
         */
        if ((ssl->config = createOpenSslConfig(sp)) == 0) {
            return MPR_ERR_CANT_INITIALIZE;
        }
        ssl->changed = 0;
    }

    /*
        Create and configure the SSL struct
     */
    cfg = osp->cfg = sp->ssl->config;
    if ((osp->handle = (SSL*) SSL_new(cfg->context)) == 0) {
        return MPR_ERR_BAD_STATE;
    }
    SSL_set_app_data(osp->handle, (void*) osp);

    /*
        Create a socket bio. We don't use the BIO except as storage for the fd
     */
    if ((osp->bio = BIO_new_socket((int) sp->fd, BIO_NOCLOSE)) == 0) {
        return MPR_ERR_BAD_STATE;
    }
    SSL_set_bio(osp->handle, osp->bio, osp->bio);

    if (sp->flags & MPR_SOCKET_SERVER) {
        SSL_set_accept_state(osp->handle);
    } else {
        if (requiredPeerName) {
            osp->requiredPeerName = sclone(requiredPeerName);
        }
        /*
            Block while connecting
         */
        mprSetSocketBlockingMode(sp, 1);
        sp->errorMsg = 0;
        if ((rc = SSL_connect(osp->handle)) < 1) {
            if (sp->errorMsg) {
                mprLog("info mpr ssl openssl", 4, "Connect failed: %s", sp->errorMsg);
            } else {
                mprLog("info mpr ssl openssl", 4, "Connect failed: error %s", getOssError(sp));
            }
            return MPR_ERR_CANT_CONNECT;
        }
        if (rc > 0 && checkCertificateName(sp) < 0) {
            return MPR_ERR_CANT_CONNECT;
        }
        mprSetSocketBlockingMode(sp, 0);
    }
#if defined(ME_MPR_SSL_RENEGOTIATE) && !ME_MPR_SSL_RENEGOTIATE
    /*
        Disable renegotiation after the initial handshake if renegotiate is explicitly set to false (CVE-2009-3555).
        Note: this really is a bogus CVE as disabling renegotiation is not required nor does it enhance security if
        used with up-to-date (patched) SSL stacks.
     */
    if (osp->handle->s3) {
        osp->handle->s3->flags |= SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS;
    }
#endif
    return 0;
}


static void disconnectOss(MprSocket *sp)
{
    sp->service->standardProvider->disconnectSocket(sp);
}


/*
    Return the number of bytes read. Return -1 on errors and EOF. Distinguish EOF via mprIsSocketEof.
    If non-blocking, may return zero if no data or still handshaking.
 */
static ssize readOss(MprSocket *sp, void *buf, ssize len)
{
    OpenSocket      *osp;
    int             rc, error, retries, i;

    osp = (OpenSocket*) sp->sslSocket;
    assert(osp);

    if (osp->handle == 0) {
        return MPR_ERR_BAD_STATE;
    }
    /*
        Limit retries on WANT_READ. If non-blocking and no data, then this could spin forever.
     */
    retries = 5;
    for (i = 0; i < retries; i++) {
        rc = SSL_read(osp->handle, buf, (int) len);
        if (rc < 0) {
            error = SSL_get_error(osp->handle, rc);
            if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_CONNECT || error == SSL_ERROR_WANT_ACCEPT) {
                continue;
            }
            mprLog("info mpr ssl openssl", 5, "SSL_read %s", getOssError(sp));
        }
        break;
    }
    if (rc <= 0) {
        error = SSL_get_error(osp->handle, rc);
        if (error == SSL_ERROR_WANT_READ) {
            rc = 0;
        } else if (error == SSL_ERROR_WANT_WRITE) {
            rc = 0;
        } else if (error == SSL_ERROR_ZERO_RETURN) {
            sp->flags |= MPR_SOCKET_EOF;
            rc = -1;
        } else if (error == SSL_ERROR_SYSCALL) {
            sp->flags |= MPR_SOCKET_EOF;
            rc = -1;
        } else if (error != SSL_ERROR_ZERO_RETURN) {
            /* SSL_ERROR_SSL */
            mprLog("info mpr ssl openssl", 4, "%s", getOssError(sp));
            rc = -1;
            sp->flags |= MPR_SOCKET_EOF;
        }
    } else {
        if (!sp->secured && checkCertificateName(sp) < 0) {
            return MPR_ERR_BAD_STATE;
        }
        if (SSL_pending(osp->handle) > 0) {
            sp->flags |= MPR_SOCKET_BUFFERED_READ;
            mprRecallWaitHandlerByFd(sp->fd);
        }
    }
    return rc;
}


/*
    Write data. Return the number of bytes written or -1 on errors.
 */
static ssize writeOss(MprSocket *sp, cvoid *buf, ssize len)
{
    OpenSocket  *osp;
    ssize       totalWritten;
    int         error, rc;

    osp = (OpenSocket*) sp->sslSocket;

    if (osp->bio == 0 || osp->handle == 0 || len <= 0) {
        return MPR_ERR_BAD_STATE;
    }
    totalWritten = 0;
    ERR_clear_error();
    error = 0;

    do {
        rc = SSL_write(osp->handle, buf, (int) len);
        mprLog("info mpr ssl openssl", 7, "Wrote %d, requested len %zd", rc, len);
        if (rc <= 0) {
            error = SSL_get_error(osp->handle, rc);
            if (error == SSL_ERROR_WANT_WRITE) {
                break;
            }
            return MPR_ERR_CANT_WRITE;
        }
        totalWritten += rc;
        buf = (void*) ((char*) buf + rc);
        len -= rc;
        mprLog("info mpr ssl openssl", 7, "write len %zd, written %d, total %zd", len, rc, totalWritten);
    } while (len > 0);

    if (totalWritten == 0 && error == SSL_ERROR_WANT_WRITE) {
        mprSetError(EAGAIN);
        return MPR_ERR_NOT_READY;
    }
    return totalWritten;
}


static ssize flushOss(MprSocket *sp)
{
    return 0;
}

 
/*
    Parse the cert info and write properties to the buffer. Modifies the info argument.
 */
static void parseCertFields(MprBuf *buf, char *prefix, char *prefix2, char *info)
{
    char    c, *cp, *term, *key, *value;

    if (info) {
        term = cp = info;
        do {
            c = *cp;
            if (c == '/' || c == '\0') {
                *cp = '\0';
                key = ssplit(term, "=", &value);
                if (smatch(key, "emailAddress")) {
                    key = "EMAIL";
                }
                mprPutToBuf(buf, "%s%s%s=%s,", prefix, prefix2, key, value);
                term = &cp[1];
                *cp = c;
            }
        } while (*cp++ != '\0');
    }
}


static char *getOssSession(MprSocket *sp) 
{
    SSL_SESSION     *sess;
    OpenSocket      *osp;
    MprBuf          *buf;
    int             i;

    osp = sp->sslSocket;

    if ((sess = SSL_get0_session(osp->handle)) != 0) {
        if (sess->session_id_length == 0 && osp->handle->tlsext_ticket_expected) {
            return sclone("ticket");
        }
        buf = mprCreateBuf((sess->session_id_length * 2) + 1, 0);
        assert(buf->start);
        for (i = 0; i < sess->session_id_length; i++) {
            mprPutToBuf(buf, "%02X", (uchar) sess->session_id[i]);
        }
        return mprBufToString(buf);
    }
    return 0;
}


/*
    Get the SSL state of the socket in a buffer
 */
static char *getOssState(MprSocket *sp)
{
    OpenSocket      *osp;
    MprBuf          *buf;
    X509_NAME       *xSubject;
    X509            *cert;
    char            *prefix, subject[512], issuer[512], peer[512];

    osp = sp->sslSocket;
    buf = mprCreateBuf(0, 0);

    mprPutToBuf(buf, "PROVIDER=openssl,CIPHER=%s,SESSION=%s,RESUMED=%d,",
        SSL_get_cipher(osp->handle), sp->session, (int) SSL_session_reused(osp->handle));

    if ((cert = SSL_get_peer_certificate(osp->handle)) == 0) {
        mprPutToBuf(buf, "%s=\"none\",", sp->acceptIp ? "CLIENT_CERT" : "SERVER_CERT");

    } else {
        xSubject = X509_get_subject_name(cert);
        X509_NAME_get_text_by_NID(xSubject, NID_commonName, peer, sizeof(peer) - 1);
        mprPutToBuf(buf, "PEER=\"%s\",", peer);

        prefix = sp->acceptIp ? "CLIENT_" : "SERVER_";
        X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject) -1);
        parseCertFields(buf, prefix, "S_", &subject[1]);

        X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof(issuer) -1);
        parseCertFields(buf, prefix, "I_", &issuer[1]);
        X509_free(cert);
    }
    if ((cert = SSL_get_certificate(osp->handle)) != 0) {
        prefix =  sp->acceptIp ? "SERVER_" : "CLIENT_";
        X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject) -1);
        parseCertFields(buf, prefix, "S_", &subject[1]);

        X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof(issuer) -1);
        parseCertFields(buf, prefix, "I_", &issuer[1]);
        /* Don't call X509_free on own cert */
    }
    return mprGetBufStart(buf);
}


/*
    Check the certificate peer name validates and matches the desired name
 */
static int checkCertificateName(MprSocket *sp)
{
    MprSsl      *ssl;
    OpenSocket  *osp;
    X509        *cert;
    X509_NAME   *xSubject;
    char        subject[512], issuer[512], peerName[512], *target, *certName, *tp;

    ssl = sp->ssl;
    osp = (OpenSocket*) sp->sslSocket;
    sp->cipher = sclone(SSL_get_cipher(osp->handle));

    if ((cert = SSL_get_peer_certificate(osp->handle)) == 0) {
        peerName[0] = '\0';
    } else {
        xSubject = X509_get_subject_name(cert);
        X509_NAME_oneline(xSubject, subject, sizeof(subject) -1);
        X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof(issuer) -1);
        X509_NAME_get_text_by_NID(xSubject, NID_commonName, peerName, sizeof(peerName) - 1);
        sp->peerName = sclone(peerName);
        sp->peerCert = sclone(subject);
        sp->peerCertIssuer = sclone(issuer);
        X509_free(cert);
    }
    if (ssl->verifyPeer && osp->requiredPeerName) {
        target = osp->requiredPeerName;
        certName = peerName;

        if (target == 0 || *target == '\0' || strchr(target, '.') == 0) {
            sp->errorMsg = sfmt("Bad peer name");
            return MPR_ERR_BAD_VALUE;
        }
        if (!smatch(certName, "localhost")) {
            if (strchr(certName, '.') == 0) {
                sp->errorMsg = sfmt("Peer certificate must have a domain: \"%s\"", certName);
                return MPR_ERR_BAD_VALUE;
            }
            if (*certName == '*' && certName[1] == '.') {
                /* Wildcard cert */
                certName = &certName[2];
                if (strchr(certName, '.') == 0) {
                    /* Peer must be of the form *.domain.tld. i.e. *.com is not valid */
                    sp->errorMsg = sfmt("Peer CN is not valid %s", peerName);
                    return MPR_ERR_BAD_VALUE;
                }
                if ((tp = strchr(target, '.')) != 0 && strchr(&tp[1], '.')) {
                    /* Strip host name if target has a host name */
                    target = &tp[1];
                }
            }
        }
        if (!smatch(target, certName)) {
            sp->errorMsg = sfmt("Certificate common name mismatch CN \"%s\" vs required \"%s\"", peerName,
                osp->requiredPeerName);
            return MPR_ERR_BAD_VALUE;
        }
    }
    sp->session = getOssSession(sp);
    sp->secured = 1;
    sp->flags |= (SSL_session_reused(osp->handle) ? MPR_SOCKET_RESUMED : 0);
    return 0;
}


static int verifyPeerCertificate(int ok, X509_STORE_CTX *xContext)
{
    X509            *cert;
    SSL             *handle;
    OpenSocket      *osp;
    MprSocket       *sp;
    MprSsl          *ssl;
    char            subject[512], issuer[512], peerName[512];
    int             error, depth;

    subject[0] = issuer[0] = '\0';

    handle = (SSL*) X509_STORE_CTX_get_app_data(xContext);
    osp = (OpenSocket*) SSL_get_app_data(handle);
    sp = osp->sock;
    ssl = sp->ssl;

    cert = X509_STORE_CTX_get_current_cert(xContext);
    depth = X509_STORE_CTX_get_error_depth(xContext);
    error = X509_STORE_CTX_get_error(xContext);

    ok = 1;
    if (X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject) - 1) < 0) {
        sp->errorMsg = sclone("Cannot get subject name");
        ok = 0;
    }
    if (X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof(issuer) - 1) < 0) {
        sp->errorMsg = sclone("Cannot get issuer name");
        ok = 0;
    }
    if (X509_NAME_get_text_by_NID(X509_get_subject_name(cert), NID_commonName, peerName, sizeof(peerName) - 1) == 0) {
        sp->errorMsg = sclone("Cannot get peer name");
        ok = 0;
    }
    if (ok && ssl->verifyDepth < depth) {
        if (error == 0) {
            error = X509_V_ERR_CERT_CHAIN_TOO_LONG;
        }
    }
    switch (error) {
    case X509_V_OK:
        break;
    case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
    case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
        /* Normal self signed certificate */
        if (ssl->verifyIssuer) {
            sp->errorMsg = sclone("Self-signed certificate");
            ok = 0;
        }
        break;

    case X509_V_ERR_CERT_UNTRUSTED:
    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
        if (ssl->verifyIssuer) {
            /* Issuer cannot be verified */
            sp->errorMsg = sclone("Certificate not trusted");
            ok = 0;
        }
        break;

    case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
    case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
        if (ssl->verifyIssuer) {
            /* Issuer cannot be verified */
            sp->errorMsg = sclone("Certificate not trusted");
            ok = 0;
        }
        break;

    case X509_V_ERR_CERT_HAS_EXPIRED:
        sp->errorMsg = sfmt("Certificate has expired");
        ok = 0;
        break;

    case X509_V_ERR_CERT_CHAIN_TOO_LONG:
    case X509_V_ERR_CERT_NOT_YET_VALID:
    case X509_V_ERR_CERT_REJECTED:
    case X509_V_ERR_CERT_SIGNATURE_FAILURE:
    case X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD:
    case X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD:
    case X509_V_ERR_INVALID_CA:
    default:
        sp->errorMsg = sfmt("Certificate verification error %d", error);
        ok = 0;
        break;
    }
    return ok;
}


static ulong sslThreadId()
{
    return (long) mprGetCurrentOsThread();
}


static void sslStaticLock(int mode, int n, cchar *file, int line)
{
    assert(0 <= n && n < numLocks);

    if (olocks) {
        if (mode & CRYPTO_LOCK) {
            mprLock(olocks[n]);
        } else {
            mprUnlock(olocks[n]);
        }
    }
}


static DynLock *sslCreateDynLock(cchar *file, int line)
{
    DynLock     *dl;

    dl = mprAllocZeroed(sizeof(DynLock));
    dl->mutex = mprCreateLock(dl);
    mprHold(dl->mutex);
    return dl;
}


static void sslDestroyDynLock(DynLock *dl, cchar *file, int line)
{
    if (dl->mutex) {
        mprRelease(dl->mutex);
        dl->mutex = 0;
    }
}


static void sslDynLock(int mode, DynLock *dl, cchar *file, int line)
{
    if (mode & CRYPTO_LOCK) {
        mprLock(dl->mutex);
    } else {
        mprUnlock(dl->mutex);
    }
}


static char *getOssError(MprSocket *sp)
{
    char    ebuf[ME_MAX_BUFFER];
    ulong   error;

    error = ERR_get_error();
    ERR_error_string_n(error, ebuf, sizeof(ebuf) - 1);
    sp->errorMsg = sclone(ebuf);
    return sp->errorMsg;
}


/*
    Get the DH parameters. This tries to read the 
    Default DH parameters used if the dh.pem file is not supplied.

    Generated via
        openssl dhparam -C 2048 -out /dev/null >dhparams.h

    Though not essential, you should generate your own local DH parameters 
    and supply a dh.pem file to make it a bit harder for attackers.
 */

static DH *getDhKey(cchar *path)
{
	static unsigned char dh2048_p[] = {
		0xDA,0xDD,0x26,0xAA,0xBF,0x0B,0x2D,0xC5,0x83,0x20,0x26,0xD3,
		0x99,0x62,0x76,0xF6,0xF5,0x55,0x7F,0x84,0xB4,0x6A,0x7F,0x3E,
		0xBF,0x1C,0xF8,0xB6,0xE9,0xD3,0x40,0x0A,0xBB,0xED,0xD9,0xFF,
		0x3F,0x51,0x38,0xCC,0xFD,0x89,0x63,0x4C,0x0F,0x6C,0x9E,0x52,
		0x90,0x14,0xC4,0x55,0x34,0xE8,0xF1,0xD9,0xEB,0x43,0xD4,0xAD,
		0x27,0x67,0x4C,0x3A,0x0F,0x18,0x86,0x96,0x47,0x1D,0xA1,0x17,
		0xE3,0x30,0xEF,0x3B,0x7D,0x34,0x45,0x4C,0x11,0x86,0xFB,0x29,
		0xFC,0xB5,0x05,0x1B,0xC3,0xA8,0x22,0x38,0xC9,0xEA,0xD7,0x2D,
		0x02,0x96,0x2D,0xD9,0x77,0xF8,0x87,0x31,0x96,0xAA,0x6A,0xFF,
		0xEA,0xC1,0x78,0xBE,0x12,0xA2,0x78,0xBD,0x9A,0x78,0x7C,0xA5,
		0x4D,0x2F,0x3B,0xE8,0x6F,0xAD,0xE6,0xBE,0x21,0x3E,0x6C,0x7D,
		0xB5,0x53,0xE1,0x1E,0x83,0x81,0xBD,0x98,0x54,0x8E,0xE5,0x54,
		0xEC,0x43,0x09,0x54,0x9A,0xDC,0x7C,0xC8,0xE9,0xBC,0x20,0x50,
		0x31,0x28,0xE9,0xF5,0x99,0x60,0xE2,0x40,0x48,0x57,0x8D,0xD9,
		0x29,0xF5,0x8B,0x22,0xDE,0x93,0xE2,0x56,0x0B,0x76,0xE3,0x8B,
		0xC4,0x37,0x2F,0xD2,0xC1,0x34,0xF5,0x9B,0x12,0xD8,0x2B,0xE2,
		0x98,0xBA,0x0C,0xBB,0xDC,0x7A,0x65,0x7C,0x2D,0xC2,0x56,0x01,
		0x94,0x9F,0xB5,0xAE,0xC2,0xAA,0x6B,0x42,0x1B,0x54,0x36,0xE3,
		0x86,0xAC,0x21,0x93,0xDD,0x8B,0xD1,0x0D,0xEF,0x39,0x20,0x14,
		0x29,0xA1,0xB1,0xEE,0xE7,0xB1,0xA3,0x29,0x6C,0xD5,0xE6,0xD6,
		0x23,0x20,0xDC,0xDC,0xFA,0x0A,0x06,0x81,0xB3,0xF4,0xEB,0xCE,
		0xA6,0xD7,0x23,0x93,
    };
	static unsigned char dh2048_g[] = {
		0x02,
    };
	DH      *dh;
    BIO     *bio;

    dh = 0;
    if (path && *path) {
        if ((bio = BIO_new_file(path, "r")) != 0) {
            dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
        }
    }
    if (dh == 0) {
        if ((dh = DH_new()) == 0) {
            return 0;
        }
        dh->p = BN_bin2bn(dh2048_p, sizeof(dh2048_p), NULL);
        dh->g = BN_bin2bn(dh2048_g, sizeof(dh2048_g), NULL);
        if ((dh->p == 0) || (dh->g == 0)) { 
            DH_free(dh); 
            return 0;
        }
    }
	return dh;
}


/*
    Set the ephemeral DH key
 */
static DH *dhcallback(SSL *handle, int isExport, int keyLength)
{
    OpenSocket      *osp;
    OpenConfig      *cfg;

    osp = (OpenSocket*) SSL_get_app_data(handle);
    cfg = osp->sock->ssl->config;
    return cfg->dhKey;
}


#endif /* ME_COM_OPENSSL */

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
