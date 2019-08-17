/*
    http.c -- Http client program

    The http program is a client to issue HTTP requests. It is also a test platform for loading and testing web servers.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/******************************** Includes ***********************************/

#include    "http.h"

/*********************************** Locals ***********************************/

#define MAX_REDIRECTS   10

typedef struct ThreadData {
    int             activeRequests;
    MprCond         *cond;
    MprDispatcher   *dispatcher;
    HttpNet         *net;
    MprList         *requests;
} ThreadData;

/*
    State for each stream/conn.
 */
typedef struct Request {
    HttpStream  *stream;
    int         count;
    int         follow;             /* Current follow redirect count */
    MprFile     *outFile;
    cchar       *redirect;          /* Redirect URL */
    int         retries;            /* Current retry count */
    MprEvent    *timeout;           /* Timeout event */
    ThreadData  *threadData;
    bool        written;
} Request;

typedef struct App {
    int         activeLoadThreads;  /* Active threads */
    char        *authType;          /* Authentication: basic|digest */
    int         benchmark;          /* Output benchmarks */
    cchar       *ca;                /* Certificate bundle to use when validating the server certificate */
    cchar       *cert;              /* Certificate to identify the client */
    int         chunkSize;          /* Ask for response data to be chunked in this quanta */
    char        *ciphers;           /* Set of acceptable ciphers to use for SSL */
    int         continueOnErrors;   /* Continue testing even if an error occurs. Default is to stop */
    int         fetchCount;         /* Total count of fetches */
    cchar       *file;              /* File to put / upload */
    MprList     *files;             /* List of files to put / upload (only ever 1 entry) */
    int         packetSize;         /* HTTP/2 input frame size (min 16K) */
    int         hasData;            /* Request has body data */
    MprList     *formData;          /* Form body data */
    MprBuf      *bodyData;          /* Block body data */
    Mpr         *mpr;               /* Portable runtime */
    MprList     *headers;           /* Request headers */
    Http        *http;              /* Http service object */
    char        *host;              /* Host to connect to */
    MprFile     *inFile;            /* Input file for post/put data */
    cchar       *ip;                /* First hop IP for the request URL */
    int         iterations;         /* URLs to fetch (per thread) */
    cchar       *key;               /* Private key file */
    int         loadThreads;        /* Number of threads to use for URL requests */
    int         maxRetries;         /* Times to retry a failed request */
    int         maxFollow;          /* Times to follow a redirect */
    char        *method;            /* HTTP method when URL on cmd line */
    MprMutex    *mutex;             /* Multithread sync */
    bool        needSsl;            /* Need SSL for request */
    int         nextArg;            /* Next arg to parse */
    int         noout;              /* Don't output files */
    int         nofollow;           /* Don't automatically follow redirects */
    char        *outFilename;       /* Output filename */
    char        *password;          /* Password for authentication */
    int         port;               /* TCP/IP port for request */
    int         printable;          /* Make binary output printable */
    int         protocol;           /**< HTTP protocol: 0 for HTTP/1.0, 1 for HTTP/1.1 or 2+ */
    char        *ranges;            /* Request ranges */
    int         sequence;           /* Sequence requests with a custom header */
    int         status;             /* Status for single requests */
    int         showStatus;         /* Output the Http response status */
    int         showHeaders;        /* Output the response headers */
    int         singleStep;         /* Pause between requests */
    MprSsl      *ssl;               /* SSL configuration */
    int         streams;            /* Number of HTTP/2 streams to spawn */
    int         success;            /* Total success flag */
    cchar       *target;            /* Destination url */
    cchar       *test;              /* Test to invoke */
    int         text;               /* Emit errors in plain text */
    MprTicks    timeout;            /* Timeout in msecs for a non-responsive server */
    MprList     *threadData;        /* Per thread data */
    int         upload;             /* Upload using multipart mime */
    HttpUri     *uri;               /* Parsed URL */
    cchar       *url;               /* Request URL */
    char        *username;          /* User name for authentication of requests */
    int         verifyPeer;         /* Validate server certs */
    int         verifyIssuer;       /* Validate the issuer. Permits self-signed certs if false. */
    int         verbose;            /* Trace progress */
    int         window;             /* HTTP/2 input window size (min 65535) */
    int         workers;            /* Worker threads. >0 if multi-threaded */
    int         zeroOnErrors;       /* Exit zero status for any valid HTTP response code  */
} App;

static App *app;

/***************************** Forward Declarations ***************************/

static void     addFormVars(cchar *buf);
static Request  *allocRequest(HttpStream *stream);
static void     checkRequestState(HttpStream *stream);
static Request  *createRequest(ThreadData *td, HttpStream *stream);
static char     *extendUrl(cchar *url);
static void     finishRequest(Request *request);
static void     finishThread(MprThread *thread);
static cchar    *formatOutput(HttpStream *stream, cchar *buf, ssize *count);
static char     *getPassword(void);
static cchar    *getRedirectUrl(HttpStream *stream, cchar *url);
static int      initSettings(void);
static bool     isPort(cchar *name);
static void     manageApp(App *app, int flags);
static void     manageRequest(Request *request, int flags);
static void     manageThreadData(ThreadData *data, int flags);
static void     notifier(HttpStream *stream, int event, int arg);
static int      parseArgs(int argc, char **argv);
static void     parseStatus(HttpStream *stream);
static void     prepHeaders(HttpStream *stream);
static void     readBody(HttpStream *stream);
static int      processResponse(HttpStream *stream);
static int      setContentLength(HttpStream *stream);
static void     setDefaults(void);
static int      showUsage(void);
static void     startRequest(Request *request);
static void     startThreads(void);
static void     threadMain(void *data, MprThread *tp);
static void     trace(HttpStream *stream, cchar *url, int fetchCount, cchar *method, int status, MprOff contentLen);
static void     waitForUser(void);
static ssize    writeBody(HttpStream *stream);

/*********************************** Code *************************************/

MAIN(httpMain, int argc, char **argv, char **envp)
{
    MprTime     start;
    double      elapsed;
    int         success;

    if (mprCreate(argc, argv, MPR_USER_EVENTS_THREAD) == 0) {
        return MPR_ERR_MEMORY;
    }
    if ((app = mprAllocObj(App, manageApp)) == 0) {
        return MPR_ERR_MEMORY;
    }
    mprAddRoot(app);
    mprAddStandardSignals();
    setDefaults();

    if ((app->http = httpCreate(HTTP_CLIENT_SIDE)) == 0) {
        return MPR_ERR_MEMORY;
    }
    if (parseArgs(argc, argv) < 0) {
        return MPR_ERR_BAD_ARGS;
    }
    if (initSettings() < 0) {
        return MPR_ERR_BAD_ARGS;
    }
    if (mprStart() < 0) {
        mprLog("error http", 0, "Cannot start MPR for %s", mprGetAppTitle());
        exit(2);
    }
    start = mprGetTime();

    startThreads();
    mprServiceEvents(-1, 0);

    if (app->benchmark) {
        elapsed = (double) (mprGetTime() - start);
        if (app->fetchCount == 0) {
            elapsed = 0;
            app->fetchCount = 1;
        }
        mprPrintf("\nRequest Count:       %13d\n", app->fetchCount);
        mprPrintf("Time elapsed:        %13.4f sec\n", elapsed / 1000.0);
        mprPrintf("Time per request:    %13.4f sec\n", elapsed / 1000.0 / app->fetchCount);
        mprPrintf("Requests per second: %13.4f\n", app->fetchCount * 1.0 / (elapsed / 1000.0));
        mprPrintf("Load threads:        %13d\n", app->loadThreads);
        mprPrintf("Worker threads:      %13d\n", app->workers);
    }
    if (!app->success && app->verbose) {
        mprLog("error http", 0, "Request failed");
    }
    success = app->success;
    mprDestroy();
    return success ? 0 : 2;
}


static void manageApp(App *app, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(app->authType);
        mprMark(app->ca);
        mprMark(app->cert);
        mprMark(app->ciphers);
        mprMark(app->inFile);
        mprMark(app->ip);
        mprMark(app->file);
        mprMark(app->files);
        mprMark(app->formData);
        mprMark(app->bodyData);
        mprMark(app->headers);
        mprMark(app->http);
        mprMark(app->key);
        mprMark(app->host);
        mprMark(app->outFilename);
        mprMark(app->mutex);
        mprMark(app->password);
        mprMark(app->ranges);
        mprMark(app->ssl);
        mprMark(app->username);
        mprMark(app->threadData);
        mprMark(app->uri);
        mprMark(app->url);
    }
}


static void setDefaults()
{
    app->method = 0;
    app->verbose = 0;
    app->continueOnErrors = 0;
    app->showHeaders = 0;
    app->verifyIssuer = -1;
    app->verifyPeer = 0;
    app->zeroOnErrors = 0;

    app->authType = sclone("basic");
    app->host = sclone("localhost");
    app->iterations = 1;
    app->loadThreads = 1;
    app->maxFollow = 5;
    app->maxRetries = 2;
    app->protocol = 1;
    app->success = 1;
    app->streams = 1;

    /* zero means no timeout */
    app->timeout = 0;
    app->workers = 1;
    app->headers = mprCreateList(0, MPR_LIST_STABLE);
    app->mutex = mprCreateLock();
#if ME_HTTP_HTTP2
    app->packetSize = HTTP2_MIN_FRAME_SIZE;
    app->window = HTTP2_MIN_WINDOW;
#endif
#if WINDOWS
    _setmode(fileno(stdout), O_BINARY);
#endif
}


static int initSsl()
{
#if ME_COM_SSL
    if (app->uri->secure || app->needSsl) {
        app->ssl = mprCreateSsl(0);
        if (app->cert) {
            if (!app->key) {
                mprLog("error http", 0, "Must specify key file");
                return MPR_ERR_BAD_ARGS;
            }
            mprSetSslCertFile(app->ssl, app->cert);
            mprSetSslKeyFile(app->ssl, app->key);
        }
        if (app->ca) {
            mprSetSslCaFile(app->ssl, app->ca);
        }
        if (app->verifyIssuer == -1) {
            app->verifyIssuer = app->verifyPeer ? 1 : 0;
        }
        mprVerifySslPeer(app->ssl, app->verifyPeer);
        mprVerifySslIssuer(app->ssl, app->verifyIssuer);
        if (app->ciphers) {
            mprSetSslCiphers(app->ssl, app->ciphers);
        }
        if (app->protocol >= 2) {
            mprSetSslAlpn(app->ssl, "h2");
        }
    } else {
        mprVerifySslPeer(NULL, 0);
    }
#else
    /* Suppress comp warning */
    mprNop(&app->ssl);
#endif
    return 0;
}


static int parseArgs(int argc, char **argv)
{
    char    *argp, *key, *logSpec, *value, *traceSpec;
    int     setWorkers, nextArg;

    setWorkers = 0;
    app->needSsl = 0;
    logSpec = traceSpec = 0;

    for (nextArg = 1; nextArg < argc; nextArg++) {
        argp = argv[nextArg];
        if (*argp != '-') {
            break;
        }
        if (smatch(argp, "--auth")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->authType = slower(argv[++nextArg]);
            }

        } else if (smatch(argp, "--benchmark") || smatch(argp, "-b")) {
            app->benchmark++;

        } else if (smatch(argp, "--ca")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->ca = sclone(argv[++nextArg]);
                if (!mprPathExists(app->ca, R_OK)) {
                    mprLog("error http", 0, "Cannot find ca file %s", app->ca);
                    return MPR_ERR_BAD_ARGS;
                }
            }
            app->needSsl = 1;

        } else if (smatch(argp, "--cert")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->cert = sclone(argv[++nextArg]);
                if (!mprPathExists(app->cert, R_OK)) {
                    mprLog("error http", 0, "Cannot find cert file %s", app->cert);
                    return MPR_ERR_BAD_ARGS;
                }
            }
            app->needSsl = 1;

        } else if (smatch(argp, "--chunk")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                value = argv[++nextArg];
                app->chunkSize = atoi(value);
                if (app->chunkSize < 0) {
                    mprLog("error http", 0, "Bad chunksize %d", app->chunkSize);
                    return MPR_ERR_BAD_ARGS;
                }
            }

        } else if (smatch(argp, "--cipher") || smatch(argp, "--ciphers")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->ciphers = sclone(argv[++nextArg]);
            }
            app->needSsl = 1;

        } else if (smatch(argp, "--continue") || smatch(argp, "-c")) {
            app->continueOnErrors++;

        } else if (smatch(argp, "--cookie")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                mprAddItem(app->headers, mprCreateKeyPair("Cookie", argv[++nextArg], 0));
            }

        } else if (smatch(argp, "--data")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                if (app->bodyData == 0) {
                    app->bodyData = mprCreateBuf(-1, -1);
                }
                mprPutStringToBuf(app->bodyData, argv[++nextArg]);
            }

        } else if (smatch(argp, "--debugger") || smatch(argp, "-D")) {
            mprSetDebugMode(1);
            app->maxRetries = 0;
            app->timeout = HTTP_UNLIMITED;

        } else if (smatch(argp, "--delete")) {
            app->method = "DELETE";

        } else if (smatch(argp, "--form") || smatch(argp, "-f")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                if (app->formData == 0) {
                    app->formData = mprCreateList(-1, MPR_LIST_STABLE);
                }
                addFormVars(argv[++nextArg]);
            }

        } else if (smatch(argp, "--frame")) {
#if ME_HTTP_HTTP2
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->packetSize = atoi(argv[++nextArg]);
                if (app->packetSize < HTTP2_MIN_FRAME_SIZE) {
                    app->packetSize = HTTP2_MIN_FRAME_SIZE;
                }
            }
#endif

        } else if (smatch(argp, "--header")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                key = argv[++nextArg];
                if ((value = strchr(key, ':')) == 0) {
                    mprLog("error http", 0, "Bad header format. Must be \"key: value\"");
                    return MPR_ERR_BAD_ARGS;
                }
                *value++ = '\0';
                while (isspace((uchar) *value)) {
                    value++;
                }
                mprAddItem(app->headers, mprCreateKeyPair(key, value, 0));
            }

        } else if (smatch(argp, "--host")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->host = argv[++nextArg];
                if (*app->host == ':') {
                    app->host = &app->host[1];
                }
                if (isPort(app->host)) {
                    app->host = sfmt("http://127.0.0.1:%s", app->host);
                } else {
                    app->host = sclone(app->host);
                }
            }

        } else if (smatch(argp, "--http0") || smatch(argp, "--h0")) {
            app->protocol = 0;

        } else if (smatch(argp, "--http1") || smatch(argp, "--h1")) {
            app->protocol = 1;

        } else if (smatch(argp, "--http2") || smatch(argp, "--h2") || smatch(argp, "-h")) {
#if ME_HTTP_HTTP2
            app->protocol = 2;
#else
            mprLog("error http", 0, "HTTP/2 not supported in this build");
            return MPR_ERR_BAD_STATE;
#endif

        } else if (smatch(argp, "--iterations") || smatch(argp, "-i")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->iterations = atoi(argv[++nextArg]);
            }

        } else if (smatch(argp, "--key")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->key = sclone(argv[++nextArg]);
                if (!mprPathExists(app->key, R_OK)) {
                    mprLog("error http", 0, "Cannot find key file %s", app->key);
                    return MPR_ERR_BAD_ARGS;
                }
            }
            app->needSsl = 1;

        } else if (smatch(argp, "--log") || smatch(argp, "-l")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                logSpec = argv[++nextArg];
            }

        } else if (smatch(argp, "--method") || smatch(argp, "-m")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->method = argv[++nextArg];
            }

        } else if (smatch(argp, "--out") || smatch(argp, "-o")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->outFilename = sclone(argv[++nextArg]);
            }

        } else if (smatch(argp, "--noout") || smatch(argp, "-n")  ||
                   smatch(argp, "--quiet") || smatch(argp, "-q")) {
            app->noout++;

        } else if (smatch(argp, "--nofollow")) {
            app->nofollow++;

        } else if (smatch(argp, "--password") || smatch(argp, "-p")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->password = sclone(argv[++nextArg]);
            }

        } else if (smatch(argp, "--post")) {
            app->method = "POST";

        } else if (smatch(argp, "--printable")) {
            app->printable++;

        } else if (smatch(argp, "--protocol")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                value = argv[++nextArg];
                if (scaselessmatch(value, "HTTP/1.0") || smatch(value, "0")) {
                    app->protocol = 0;
                } else if (scaselessmatch(value, "HTTP/1.1") || smatch(value, "1")) {
                    app->protocol = 1;
                } else if (scaselessmatch(value, "HTTP/2") || scaselessmatch(value, "HTTP/2.0") || smatch(value, "2")) {
#if ME_HTTP_HTTP2
                    app->protocol = 2;
#else
                    mprLog("error http", 0, "HTTP/2 not supported in this build");
                    return MPR_ERR_BAD_STATE;
#endif
                }
            }

        } else if (smatch(argp, "--put")) {
            app->method = "PUT";

        } else if (smatch(argp, "--range")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                if (app->ranges == 0) {
                    app->ranges = sfmt("bytes=%s", argv[++nextArg]);
                } else {
                    app->ranges = srejoin(app->ranges, ",", argv[++nextArg], NULL);
                }
            }

        } else if (smatch(argp, "--retries") || smatch(argp, "-r")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->maxRetries = atoi(argv[++nextArg]);
            }

        } else if (smatch(argp, "--self")) {
            /* Undocumented. Allow self-signed certs. Users should just not set --verify */
            app->verifyIssuer = 0;
            app->needSsl = 1;

        } else if (smatch(argp, "--sequence")) {
            app->sequence++;

        } else if (smatch(argp, "--showHeaders") || smatch(argp, "--show") || smatch(argp, "-s")) {
            app->showHeaders++;

        } else if (smatch(argp, "--showStatus") || smatch(argp, "--showCode")) {
            app->showStatus++;

        } else if (smatch(argp, "--single") || smatch(argp, "-s")) {
            app->singleStep++;

        } else if (smatch(argp, "--streams") || smatch(argp, "-S")) {
#if ME_HTTP_HTTP2
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->streams = atoi(argv[++nextArg]);
            }
#endif
        } else if (smatch(argp, "--text")) {
            app->text++;

        } else if (smatch(argp, "--test")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->test = sclone(argv[++nextArg]);
            }

        } else if (smatch(argp, "--threads") || smatch(argp, "-t")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->loadThreads = atoi(argv[++nextArg]);
            }

        } else if (smatch(argp, "--timeout")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->timeout = atoi(argv[++nextArg]) * TPS;
            }

        } else if (smatch(argp, "--trace")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                traceSpec = argv[++nextArg];
            }

        } else if (smatch(argp, "--upload") || smatch(argp, "-u")) {
            app->upload++;

        } else if (smatch(argp, "--user") || smatch(argp, "--username")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->username = sclone(argv[++nextArg]);
            }

        } else if (smatch(argp, "--verify")) {
            app->verifyPeer = 1;
            app->needSsl = 1;

        } else if (smatch(argp, "--verbose") || smatch(argp, "-v")) {
            app->verbose++;

        } else if (smatch(argp, "--version") || smatch(argp, "-V")) {
            mprEprintf("%s %s\n"
                "Copyright (C) Embedthis Software\n"
                "Copyright (C) Michael O'Brien\n",
               ME_TITLE, ME_VERSION);
            exit(0);

        } else if (smatch(argp, "--window")) {
#if ME_HTTP_HTTP2
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->window = atoi(argv[++nextArg]);
                if (app->window < HTTP2_MIN_WINDOW) {
                    app->window = HTTP2_MIN_WINDOW;
                }
            }
#endif
        } else if (smatch(argp, "--workerTheads")) {
            if (nextArg >= argc) {
                return showUsage();
            } else {
                app->workers = atoi(argv[++nextArg]);
            }
            setWorkers++;

        } else if (smatch(argp, "--zero")) {
            app->zeroOnErrors++;

        } else if (smatch(argp, "--")) {
            nextArg++;
            break;

        } else if (smatch(argp, "-")) {
            break;

        } else if (isdigit((uchar) argp[1])) {
            if (!logSpec) {
                logSpec = sfmt("stdout:%d", (int) stoi(&argp[1]));
            }
            if (!traceSpec) {
                traceSpec = sfmt("stdout:%d", (int) stoi(&argp[1]));
            }

        } else {
            return showUsage();
        }
    }
    if (logSpec) {
        mprStartLogging(logSpec, MPR_LOG_CMDLINE);
    }
    if (traceSpec) {
        httpStartTracing(traceSpec);
    }
    if (argc == nextArg) {
        return showUsage();
    }
    app->nextArg = nextArg;
    argc = argc - nextArg;
    argv = &argv[nextArg];
    app->target = argv[argc - 1];
    if (--argc > 0) {
        app->file = sclone(argv[0]);
    }

    /*
        Process arg settings
     */
    if (!setWorkers) {
        app->workers = app->loadThreads + 2;
    }
    if (app->loadThreads > 1 || app->streams > 1) {
        app->nofollow = 1;
    }
    if (app->method == 0) {
        if (app->bodyData || app->formData || app->upload) {
            app->method = "POST";
        } else if (app->file) {
            app->method = "PUT";
        } else {
            app->method = "GET";
        }
    }
    if (app->file) {
        if (app->upload) {
            app->url = extendUrl(app->target);
        } else {
            /*
                If URL ends with "/", assume it is a directory on the target and append each file name
             */
            if (app->target[strlen(app->target) - 1] == '/') {
                app->url = mprJoinPath(app->target, mprGetPathBase(app->file));
            } else {
                app->url = app->target;
            }
            app->url = extendUrl(app->url);
            if (app->verbose) {
                mprPrintf("putting: %s to %s\n", app->file, app->url);
            }
        }
        app->files = mprCreateList(1, 0);
        mprAddItem(app->files, app->file);
    } else {
        app->url = extendUrl(app->target);
    }
    if ((app->uri = httpCreateUri(app->url, HTTP_COMPLETE_URI_PATH)) == 0) {
        return MPR_ERR_BAD_ARGS;
    }
    httpGetUriAddress(app->uri, &app->ip, &app->port);
    return 0;
}


static int initSettings()
{
    HttpLimits      *limits;

    if (app->streams > 1 && app->protocol != 2) {
        mprLog("error http", 0, "Cannot use multiple streams except with HTTP/2 protocol");
        return MPR_ERR_BAD_ARGS;
    }
    limits = HTTP->clientLimits;
    if (app->timeout) {
        limits->inactivityTimeout = app->timeout;
        limits->requestTimeout = app->timeout;
    }
#if ME_HTTP_HTTP2
    limits->packetSize = app->packetSize;
    limits->window = app->window;
#endif
    mprSetMaxWorkers(app->workers);

    if (initSsl() < 0) {
        return showUsage();
    }
    return 0;
}


static int showUsage()
{
    mprEprintf("usage: %s [options] [file] url\n"
        "  Options:\n"
        "  --auth basic|digest   # Set authentication type.\n"
        "  --benchmark           # Compute benchmark results.\n"
        "  --ca file             # Certificate bundle to use when validating the server certificate.\n"
        "  --cert file           # Certificate to send to the server to identify the client.\n"
        "  --chunk size          # Request response data to use this chunk size.\n"
        "  --ciphers cipher,...  # List of suitable ciphers.\n"
        "  --continue            # Continue on errors.\n"
        "  --cookie CookieString # Define a cookie header. Multiple uses okay.\n"
        "  --data bodyData       # Body data to send with PUT or POST.\n"
        "  --debugger            # Disable timeouts to make running in a debugger easier.\n"
        "  --delete              # Use the DELETE method. Shortcut for --method DELETE..\n"
        "  --form string         # Form data. Must already be form-www-urlencoded.\n"
        "  --frame size          # Set maximum HTTP/2 input frame size (min 16K).\n"
        "  --header 'key: value' # Add a custom request header.\n"
        "  --host hostName       # Host name or IP address for unqualified URLs.\n"
        "  --http1               # Alias for --protocol HTTP/1 (default HTTP/1.1).\n"
#if ME_HTTP_HTTP2
        "  --http2               # Alias for --protocol HTTP/2 (default HTTP/1.1).\n"
#endif
        "  --iterations count    # Number of times to fetch the URLs per thread (default 1).\n"
        "  --key file            # Private key file.\n"
        "  --log logFile:level   # Log to the file at the verbosity level.\n"
        "  --method KIND         # HTTP request method GET|OPTIONS|POST|PUT|TRACE (default GET).\n"
        "  --nofollow            # Don't automatically follow redirects.\n"
        "  --noout               # Don't output files to stdout.\n"
        "  --out file            # Send output to file.\n"
        "  --password pass       # Password for authentication.\n"
        "  --post                # Use POST method. Shortcut for --method POST.\n"
        "  --printable           # Make binary output printable.\n"
        "  --protocol PROTO      # Set HTTP protocol to HTTP/1.0, HTTP/1.1 or HTTP/2 (default HTTP/1.1).\n"
        "  --put                 # Use PUT method. Shortcut for --method PUT.\n"
        "  --range byteRanges    # Request a subset range of the document.\n"
        "  --retries count       # Number of times to retry failing requests (default 2).\n"
        "  --sequence            # Sequence requests with a custom header.\n"
        "  --showHeaders         # Output response headers.\n"
        "  --showStatus          # Output the Http response status code.\n"
        "  --single              # Single step. Pause for input between requests.\n"
        "  --streams count       # Number of HTTP/2 streams to spawn (default 1).\n"
        "  --threads count       # Number of thread instances to spawn (default 1).\n"
        "  --timeout secs        # Request timeout period in seconds.\n"
        "  --trace file:level    # Trace to the file at the verbosity level.\n"
        "  --upload              # Use multipart mime upload.\n"
        "  --user name           # User name for authentication.\n"
        "  --verify              # Validate server certificates when using SSL.\n"
        "  --verbose             # Verbose operation. Trace progress.\n"
        "  --window size         # Set HTTP/2 input window size (min 65535).\n"
        "  --workers count       # Set maximum worker threads.\n"
        "  --zero                # Exit with zero status for any valid HTTP response.\n"
        , mprGetAppName());
    return MPR_ERR_BAD_ARGS;
}


static void startThreads()
{
    MprThread   *tp;
    ThreadData  *data;
    int         j;

    if (app->chunkSize > 0) {
        mprAddItem(app->headers, mprCreateKeyPair("X-Chunk-Size", sfmt("%d", app->chunkSize), 0));
    }
    app->activeLoadThreads = app->loadThreads;
    app->threadData = mprCreateList(app->loadThreads, 0);

    for (j = 0; j < app->loadThreads; j++) {
        char name[64];
        if ((data = mprAllocObj(ThreadData, manageThreadData)) == 0) {
            return;
        }
        mprAddItem(app->threadData, data);
        fmt(name, sizeof(name), "http.%d", j);
        tp = mprCreateThread(name, threadMain, NULL, 0);
        tp->data = data;
        mprStartThread(tp);
    }
}


static void manageThreadData(ThreadData *data, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(data->cond);
        mprMark(data->dispatcher);
        mprMark(data->requests);
        mprMark(data->net);
    }
}


/*
    Per-thread execution. Called for main thread and helper threads.
 */
static void threadMain(void *data, MprThread *thread)
{
    HttpNet     *net;
    Request     *request;
    HttpStream  *stream;
    ThreadData  *td;
    int         i;

    /*
        Create and start a dispatcher. This ensures that all activity on the network will be serialized.
     */
    td = thread->data;
    td->cond = mprCreateCond();
    td->requests = mprCreateList(0, 0);

    /*
        Create a dispatcher to serialize operations for this thread.
        While this thread waits below for all the requests to complete, IO events will come on worker threads.
     */
    td->dispatcher = mprCreateDispatcher(thread->name, 0);
    mprStartDispatcher(td->dispatcher);
    net = td->net = httpCreateNet(td->dispatcher, NULL, app->protocol, HTTP_NET_ASYNC);

    if (httpConnectNet(net, app->ip, app->port, app->ssl) < 0) {
        mprLog("error http", 0, "%s", net->errorMsg);

    } else {
        for (i = 0; i < app->streams && app->success; i++) {
            if ((stream = httpCreateStream(net, 0)) == 0) {
                mprLog("error http", 0, "Cannot create connection: %s", net->errorMsg);
                app->success = 0;
                break;
            }
            request = createRequest(td, stream);
            mprAddItem(td->requests, request);
            /* Run serialized on the network dispatcher */
            mprCreateEvent(td->dispatcher, "startRequest", 0, startRequest, request, 0);
            td->activeRequests++;
        }
        if (app->success) {
            mprYield(MPR_YIELD_STICKY);
            mprStopDispatcher(td->dispatcher);
            mprWaitForCond(td->cond, -1);
            mprResetYield();
        }
    }
    httpDestroyNet(td->net);
    td->requests = 0;
    td->net = 0;
    td->dispatcher = 0;
    finishThread(thread);
}


static Request *createRequest(ThreadData *td, HttpStream *stream)
{
    Request     *request;
    cchar       *path;

    request = stream->data = allocRequest(stream);
    request->threadData = td;

    httpFollowRedirects(stream, !app->nofollow);
    httpSetStreamNotifier(stream, notifier);

    if (app->iterations == 1) {
        stream->limits->keepAliveMax = 0;
    }
    /*
        Setup authentication
     */
    if (app->username) {
        if (app->password == 0 && !strchr(app->username, ':')) {
            app->password = getPassword();
        }
        httpSetCredentials(stream, app->username, app->password, app->authType);
    }
    /*
        Apply chunk size override if specified on command line
     */
    if (app->chunkSize > 0 && (app->bodyData || app->formData || app->file)) {
        httpSetChunkSize(stream, app->chunkSize);
    }

    /*
        Create file to save output
        TODO - what if iterations?
     */
    if (app->outFilename) {
        path = app->loadThreads > 1 ? sfmt("%s-%s.tmp", app->outFilename, mprGetCurrentThreadName()): app->outFilename;
        if ((request->outFile = mprOpenFile(path, O_CREAT | O_WRONLY | O_TRUNC | O_TEXT, 0664)) == 0) {
            mprLog("error http", 0, "Cannot open %s", path);
            app->success = 0;
            return 0;
        }
    } else {
        request->outFile = mprGetStdout();
    }
    app->hasData = app->bodyData || app->formData || app->file;
    return request;
}


static void startRequest(Request *request)
{
    HttpNet     *net;
    HttpStream  *stream;

    stream = request->stream;
    net = stream->net;
    if (request->count++ >= app->iterations || (!app->success && !app->continueOnErrors)) {
        finishRequest(request);
        return;
    }
    request->written = 0;

#if FUTURE
    //  TODO - review
    cchar *authType = stream->authType;
#endif

    app->url = request->redirect ? request->redirect : app->url;
    request->redirect = 0;

    if (app->singleStep) {
        waitForUser();
    }
    prepHeaders(stream);
    if (setContentLength(stream) < 0) {
        return;
    }
    if (httpConnect(stream, app->method, app->url, app->ssl) < 0) {
        mprLog("error http", 0, "Failed request for \"%s\". %s.", app->url, net->errorMsg);
        app->success = 0;
        if (!app->continueOnErrors) {
            mprCreateEvent(stream->dispatcher, "done", 0, mprSignalCond, request->threadData->cond, 0);
        }
        return;
    }
    httpEnableNetEvents(net);
    httpServiceNetQueues(net, 0);
}


/*
    Connection event notifier callback
 */
static void notifier(HttpStream *stream, int event, int arg)
{
    switch (event) {
    case HTTP_EVENT_STATE:
        checkRequestState(stream);
        break;
    case HTTP_EVENT_READABLE:
        readBody(stream);
        break;
    case HTTP_EVENT_ERROR:
        break;
    }
}

static void checkRequestState(HttpStream *stream)
{
    Request *request;
    cchar   *url;

    request = stream->data;
    switch (stream->state) {
    case HTTP_STATE_BEGIN:
        break;

    case HTTP_STATE_CONNECTED:
        if (!app->hasData) {
            httpFinalizeOutput(stream);
        } else {
            if (!request->written) {
                if (writeBody(stream) < 0) {
                    httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot write body data. %s", httpGetError(stream));
                }
                request->written = 1;
            }
        }
        break;

    case HTTP_STATE_FIRST:
        break;

    case HTTP_STATE_PARSED:
        break;

    case HTTP_STATE_CONTENT:
        if (httpNeedRetry(stream, &url)) {
            if (url) {
                if (app->loadThreads > 1 || app->streams > 1) {
                    httpError(stream, HTTP_CODE_BAD_REQUEST, "Cannot redirect when using multiple threads or ");
                    break;
                }
                if ((request->redirect = getRedirectUrl(stream, url)) == 0) {
                    httpError(stream, HTTP_CODE_BAD_REQUEST, "Invalid redirect");
                    break;
                }
                if (++request->follow >= app->maxFollow) {
                    httpError(stream, HTTP_CODE_NO_RESPONSE, "Too many redirects");
                    break;
                }
                mprDebug("http", 4, "redirect %d of %d for: %s %s", request->follow, app->maxFollow, app->method, app->url);
            } else {
#if FUTURE
                //  TODO - check this
                if (stream->rx && stream->rx->status == HTTP_CODE_UNAUTHORIZED && authType && smatch(authType, stream->authType)) {
                    httpError(stream, HTTP_CODE_UNAUTHORIZED, "Authentication failed");
                    //TODO - should this stop all requests?
                    break;
                }
#endif
                if (++request->retries >= app->maxRetries) {
                    httpError(stream, HTTP_CODE_NO_RESPONSE, "Too many retries");
                    break;
                }
                request->follow = 0;
                mprDebug("http", 4, "retry %d of %d for: %s %s", request->retries, app->maxRetries, app->method, app->url);
            }
            request->count--;
            httpSetState(stream, HTTP_STATE_COMPLETE);

        } else {
            request->retries = 0;
            request->follow = 0;
            parseStatus(stream);
        }
        break;

    case HTTP_STATE_READY:
    case HTTP_STATE_RUNNING:
    case HTTP_STATE_FINALIZED:
        break;

    case HTTP_STATE_COMPLETE:
        processResponse(stream);
        mprCreateEvent(stream->dispatcher, "startRequest", 0, startRequest, request, 0);
    }
}


static void parseStatus(HttpStream *stream)
{
    HttpRx      *rx;

    if (stream->net->error) {
        //  TODO - need to stop all streams on this network
        httpNetError(stream->net, "Connection I/O error");

    } else if (stream->error) {
        httpError(stream, HTTP_CODE_COMMS_ERROR, "Connection I/O error");

    } else if ((rx = stream->rx) != 0) {
        if (rx->status == HTTP_CODE_REQUEST_TOO_LARGE || rx->status == HTTP_CODE_REQUEST_URL_TOO_LARGE ||
            rx->status == HTTP_CODE_NOT_ACCEPTABLE || (rx->status == HTTP_CODE_UNAUTHORIZED && stream->username == 0)) {
            httpError(stream, rx->status, "Connection I/O error");

        } else if (stream->sock->flags & MPR_SOCKET_CERT_ERROR) {
            httpError(stream, HTTP_CODE_CERT_ERROR, "Certificate error");
        }
    }
}


static void prepHeaders(HttpStream *stream)
{
    MprKeyValue     *header;
    char            *seq;
    int             next;

    if (stream->net->protocol == 1) {
        httpResetClientStream(stream, 0);
    }
    for (next = 0; (header = mprGetNextItem(app->headers, &next)) != 0; ) {
        if (scaselessmatch(header->key, "User-Agent")) {
            httpSetHeaderString(stream, header->key, header->value);
        } else {
            httpAppendHeaderString(stream, header->key, header->value);
        }
    }
    if (app->text) {
        httpSetHeader(stream, "Accept", "text/plain");
    }
    if (app->sequence) {
        static int next = 0;
        seq = itos(next++);
        httpSetHeaderString(stream, "X-Http-Seq", seq);
    }
    if (app->ranges) {
        httpSetHeaderString(stream, "Range", app->ranges);
    }
    if (app->formData) {
        httpSetContentType(stream, "application/x-www-form-urlencoded");
    }
}


static cchar *getRedirectUrl(HttpStream *stream, cchar *url)
{
    HttpUri     *target, *location;

    httpRemoveHeader(stream, "Host");
    location = httpCreateUri(url, 0);
    if (!location || !location->valid) {
        httpError(stream, HTTP_ABORT, "Invalid location URI");
        return 0;
    }
    target = httpJoinUri(stream->tx->parsedUri, 1, &location);
    return httpUriToString(target, HTTP_COMPLETE_URI);
}


static int processResponse(HttpStream *stream)
{
    HttpNet     *net;
    HttpRx      *rx;
    MprOff      bytesRead;
    cchar       *msg, *responseHeaders, *sep;
    int         status;

    net = stream->net;

    if (!stream->rx) {
        return 0;
    }
    app->status = status = httpGetStatus(stream);
    bytesRead = httpGetContentLength(stream);
    if (bytesRead < 0 && stream->rx) {
        bytesRead = stream->rx->bytesRead;
    }
    mprDebug("http", 6, "Response status %d, elapsed %lld", status, mprGetTicks() - stream->started);
    if (stream->rx) {
        if (app->showHeaders) {
            responseHeaders = httpGetHeaders(stream);
            rx = stream->rx;
            mprPrintf("%s %d %s\n", httpGetProtocol(net), status, rx->statusMessage ? rx->statusMessage : "");
            if (responseHeaders) {
                mprPrintf("%s\n", responseHeaders);
            }
        } else if (app->showStatus) {
            mprPrintf("%d\n", status);
        }
    }
    if (stream->error) {
        app->success = 0;
        msg = (stream->errorMsg) ? stream->errorMsg : "";
        sep = (msg && *msg) ? "\n" : "";
        mprLog("error http", 0, "Failed \"%s\" request for %s%s%s", app->method, app->url, sep, msg);

    } else if (status < 0) {
        mprLog("error http", 0, "\nCannot process request for \"%s\" %s", app->url, httpGetError(stream));
        return MPR_ERR_CANT_READ;

    } else if (status == 0 && net->protocol == 0) {
        /* Ignore */;

    } else if (!(200 <= status && status <= 206) && !(301 <= status && status <= 304)) {
        if (!app->zeroOnErrors) {
            app->success = 0;
        }
        if (!app->showStatus) {
            mprLog("error http", 0, "\nCannot process request for %s \"%s\" (%d) %s", app->method, app->url, status, httpGetError(stream));
            return MPR_ERR_CANT_READ;
        }
    }
    mprLock(app->mutex);
    app->fetchCount++;
    if (app->verbose && app->noout) {
        trace(stream, app->url, app->fetchCount, app->method, status, bytesRead);
    }
    mprUnlock(app->mutex);
    return 0;
}


//  TODO - but this is blocking?
static void readBody(HttpStream *stream)
{
    Request     *request;
    char        buf[ME_BUFSIZE];
    cchar       *result;
    ssize       bytes;

    request = stream->data;
    while (!stream->error && (bytes = httpRead(stream, buf, sizeof(buf))) > 0) {
        if (!app->noout) {
            result = formatOutput(stream, buf, &bytes);
            if (result) {
                mprWriteFile(request->outFile, result, bytes);
            }
        }
    }
    if (bytes <= 0 && request->outFile != mprGetStdout()) {
        mprCloseFile(request->outFile);
        request->outFile = 0;
    }
}


static int setContentLength(HttpStream *stream)
{
    MprPath     info;
    MprOff      len;
    char        *pair;
    int         next;

    len = 0;
    if (app->upload) {
        //  TODO?
        httpEnableUpload(stream);
        return 0;
    }
    if (smatch(app->file, "-")) {
        if (mprGetPathInfo(app->file, &info) < 0) {
            httpError(stream, HTTP_CODE_GONE, "Cannot access file %s", app->file);
            return MPR_ERR_CANT_ACCESS;
        }
        len += info.size;
    }
    if (app->formData) {
        for (next = 0; (pair = mprGetNextItem(app->formData, &next)) != 0; ) {
            len += slen(pair);
        }
        len += mprGetListLength(app->formData) - 1;
    }
    if (app->bodyData) {
        len += mprGetBufLength(app->bodyData);
    }
    if (len > 0) {
        httpSetContentLength(stream, len);
    }
    return 0;
}


//  TODO - how to make this non-blocking?
static ssize writeBody(HttpStream *stream)
{
    MprFile     *file;
    char        buf[ME_BUFSIZE], *path, *pair;
    ssize       bytes, len, count, nbytes, sofar;
    int         next;

    if (app->upload) {
        if (httpWriteUploadData(stream, app->files, app->formData) < 0) {
            return MPR_ERR_CANT_WRITE;
        }
    } else {
        if (app->formData) {
            count = mprGetListLength(app->formData);
            for (next = 0; (pair = mprGetNextItem(app->formData, &next)) != 0; ) {
                len = strlen(pair);
                if (next < count) {
                    len = slen(pair);
                    if (httpWriteString(stream->writeq, pair) != len || httpWriteString(stream->writeq, "&") != 1) {
                        return MPR_ERR_CANT_WRITE;
                    }
                } else {
                    if (httpWrite(stream->writeq, pair, len) != len) {
                        return MPR_ERR_CANT_WRITE;
                    }
                }
            }
        }
        if (app->files) {
            assert(mprGetListLength(app->files) == 1);
            for (next = 0; (path = mprGetNextItem(app->files, &next)) != 0; ) {
                if (strcmp(path, "-") == 0) {
                    file = mprAttachFileFd(0, "stdin", O_RDONLY | O_BINARY);
                } else {
                    file = mprOpenFile(path, O_RDONLY | O_BINARY, 0);
                }
                if (file == 0) {
                    mprLog("error http", 0, "Cannot open \"%s\"", path);
                    return MPR_ERR_CANT_OPEN;
                }
                app->inFile = file;
                if (app->verbose) {
                    mprPrintf("uploading: %s\n", path);
                }
                while ((bytes = mprReadFile(file, buf, sizeof(buf))) > 0) {
                    sofar = 0;
                    while (bytes > 0) {
                        if ((nbytes = httpWriteBlock(stream->writeq, &buf[sofar], bytes, 0)) < 0) {
                            mprCloseFile(file);
                            return MPR_ERR_CANT_WRITE;
                        }
                        bytes -= nbytes;
                        sofar += nbytes;
                        assert(bytes >= 0);
                    }
                }
                mprCloseFile(file);
                app->inFile = 0;
            }
        }
        if (app->bodyData) {
            len = mprGetBufLength(app->bodyData);
            if (httpWriteBlock(stream->writeq, mprGetBufStart(app->bodyData), len, 0) != len) {
                return MPR_ERR_CANT_WRITE;
            }
        }
    }
    httpFinalizeOutput(stream);
    return 0;
}


static void finishRequest(Request *request)
{
    ThreadData  *td;

    if (request) {
        td = request->threadData;
        mprLock(app->mutex);
        if (--td->activeRequests <= 0) {
            /*
                Run as an event so the stack httpIO stack unwinds before threadMain destroys the network.
             */
            mprCreateEvent(request->stream->dispatcher, "done", 0, mprSignalCond, request->threadData->cond, 0);
        }
        mprUnlock(app->mutex);
    }
}


static void finishThread(MprThread *tp)
{
    if (tp) {
        mprLock(app->mutex);
        if (--app->activeLoadThreads <= 0) {
            mprShutdown(MPR_EXIT_NORMAL, 0, 0);
        }
        mprUnlock(app->mutex);
    }
}


static Request *allocRequest(HttpStream *stream)
{
    Request  *request;

    request = mprAllocObj(Request, manageRequest);
    request->stream = stream;
    return request;
}


static void manageRequest(Request *request, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(request->stream);
        mprMark(request->outFile);
        mprMark(request->threadData);
    }
}


static void waitForUser()
{
    int     c;

    mprLock(app->mutex);
    mprPrintf("Pause: ");
    if (read(0, (char*) &c, 1) < 0) {}
    mprUnlock(app->mutex);
}


static void addFormVars(cchar *buf)
{
    char    *pair, *tok;

    pair = stok(sclone(buf), "&", &tok);
    while (pair) {
        mprAddItem(app->formData, sclone(pair));
        pair = stok(0, "&", &tok);
    }
}


static bool isPort(cchar *name)
{
    cchar   *cp;

    for (cp = name; *cp && *cp != '/'; cp++) {
        if (!isdigit((uchar) *cp) || *cp == '.') {
            return 0;
        }
    }
    return 1;
}


static char *extendUrl(cchar *url)
{
    if (*url == '/') {
        if (app->host) {
            if (sncaselesscmp(app->host, "http://", 7) != 0 && sncaselesscmp(app->host, "https://", 8) != 0) {
                return sfmt("http://%s%s", app->host, url);
            } else {
                return sfmt("%s%s", app->host, url);
            }
        } else {
            return sfmt("http://127.0.0.1%s", url);
        }
    }
    if (sncaselesscmp(url, "http://", 7) != 0 && sncaselesscmp(url, "https://", 8) != 0) {
        if (*url == ':' && isPort(&url[1])) {
            return sfmt("http://127.0.0.1%s", url);
        } else if (isPort(url)) {
            return sfmt("http://127.0.0.1:%s", url);
        } else {
            return sfmt("http://%s", url);
        }
    }
    return sclone(url);
}


static cchar *formatOutput(HttpStream *stream, cchar *buf, ssize *count)
{
    cchar       *result;
    int         i, c, isBinary;

    if (app->noout) {
        return 0;
    }
    if (!app->printable) {
        return buf;
    }
    isBinary = 0;
    for (i = 0; i < *count; i++) {
        if (!isprint((uchar) buf[i]) && buf[i] != '\n' && buf[i] != '\r' && buf[i] != '\t') {
            isBinary = 1;
            break;
        }
    }
    if (!isBinary) {
        return buf;
    }
    result = mprAlloc(*count * 3 + 1);
    for (i = 0; i < *count; i++) {
        c = (uchar) buf[i];
        if (app->printable && isBinary) {
            fmt("%02x ", -1, &result[i * 3], c & 0xff);
        } else {
            fmt("%c", -1, &result[i], c & 0xff);
        }
    }
    if (app->printable && isBinary) {
        *count *= 3;
    }
    return result;
}


static void trace(HttpStream *stream, cchar *url, int fetchCount, cchar *method, int status, MprOff contentLen)
{
    if (sncaselesscmp(url, "http://", 7) != 0) {
        url += 7;
    }
    if ((fetchCount % 200) == 1) {
        if (fetchCount == 1 || (fetchCount % 5000) == 1) {
            if (fetchCount > 1) {
                mprPrintf("\n");
            }
            mprPrintf("  Count  Thread   Op  Code   Bytes  Url\n");
        }
        mprPrintf("%7d %7s %4s %5d %7d  %s\n", fetchCount - 1,
            mprGetCurrentThreadName(), method, status, (uchar) contentLen, url);
    }
}


#if (ME_WIN_LIKE && !WINCE) || VXWORKS
static char *getpass(char *prompt)
{
    static char password[80];
    int     c, i;

    fputs(prompt, stdout);
    for (i = 0; i < (int) sizeof(password) - 1; i++) {
#if VXWORKS
        c = getchar();
#else
        c = _getch();
#endif
        if (c == '\r' || c == EOF) {
            break;
        }
        if ((c == '\b' || c == 127) && i > 0) {
            password[--i] = '\0';
            fputs("\b \b", stdout);
            i--;
        } else if (c == 26) {           /* Control Z */
            c = EOF;
            break;
        } else if (c == 3) {            /* Control C */
            fputs("^C\n", stdout);
            exit(255);
        } else if (!iscntrl((uchar) c) && (i < (int) sizeof(password) - 1)) {
            password[i] = c;
            fputc('*', stdout);
        } else {
            fputc('', stdout);
            i--;
        }
    }
    if (c == EOF) {
        return "";
    }
    fputc('\n', stdout);
    password[i] = '\0';
    return password;
}

#endif /* WIN */


static char *getPassword()
{
#if !WINCE
    char    *password;

    password = getpass("Password: ");
#else
    password = "no-user-interaction-support";
#endif
    return sclone(password);
}


#if VXWORKS
/*
    VxWorks link resolution
 */
PUBLIC int _cleanup() {
    return 0;
}

PUBLIC int _exit() {
    return 0;
}
#endif

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
