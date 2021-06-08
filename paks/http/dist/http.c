/*
    http.c -- Http client program

    The http program is a client to issue HTTP requests. It is also a test platform for loading and testing web servers.
    Usages:

    http /
    http 80/
    http 127.0.0.1:80/index.html
    http --iterations 10000 -b /
    http fromFile.txt /toFile.txt
    http --upload fromFile.txt /toFile.txt
    http --method DELETE /toFile.txt
    http --threads 10 --iterations 1000 --http2 /
    http --trace stdout:4 /

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/******************************** Includes ***********************************/

#include    "http.h"

/*********************************** Locals ***********************************/

#define MAX_REDIRECTS   5           /* Maximum number of redirects to follow */

typedef struct ThreadData {
    int             activeRequests;
    MprCond         *cond;
    MprDispatcher   *dispatcher;
    HttpNet         *net;
    MprList         *requests;
} ThreadData;

/*
    State for each stream
 */
typedef struct Request {
    HttpNet     *net;
    HttpStream  *stream;
    int         count;              /* Count of request iterations issued so far */
    int         follow;             /* Current follow redirect count */
    MprFile     *file;              /* Put/Upload file */
    cchar       *ip;                /* First hop IP for the request URL */
    MprFile     *outFile;
    cchar       *path;              /* Put/Upload file path */
    int         port;               /* TCP/IP port for request */
    cchar       *redirect;          /* Redirect URL */
    int         retries;            /* Current retry count */
    MprEvent    *timeout;           /* Timeout event */
    ThreadData  *threadData;
    bool        written;
    int         upload;             /* Upload using multipart mime */
    HttpUri     *uri;               /* Parsed target URL */
    char        *url;               /* Request target URL */
} Request;

typedef struct App {
    int         activeLoadThreads;  /* Active threads */
    char        *authType;          /* Authentication: basic|digest */
    int         benchmark;          /* Output benchmarks */
    MprBuf      *bodyData;          /* Block body data */
    cchar       *ca;                /* Certificate bundle to use when validating the server certificate */
    cchar       *cert;              /* Certificate to identify the client */
    int         chunkSize;          /* Ask for response data to be chunked in this quanta */
    char        *ciphers;           /* Set of acceptable ciphers to use for SSL */
    int         continueOnErrors;   /* Continue testing even if an error occurs. Default is to stop */
    int         fetchCount;         /* Total count of fetches */
    MprList     *files;             /* List of files to put / upload (only ever 1 entry) */
    MprList     *formData;          /* Form body data */
    int         hasData;            /* Request has body data */
    MprList     *headers;           /* Request headers */
    Http        *http;              /* Http service object */
    char        *host;              /* Host to connect to */
    cchar       *ip;                /* Target IP for the request URL */
    int         iterations;         /* URLs to fetch (per thread) */
    cchar       *key;               /* Private key file */
    int         loadThreads;        /* Number of threads to use for URL requests */
    int         maxRetries;         /* Times to retry a failed request */
    int         maxFollow;          /* Times to follow a redirect */
    char        *method;            /* HTTP method when URL on cmd line */
    Mpr         *mpr;               /* Portable runtime */
    MprMutex    *mutex;             /* Multithread sync */
    bool        needSsl;            /* Need SSL for request */
    int         nextArg;            /* Next arg to parse */
    int         noout;              /* Don't output files */
    int         nofollow;           /* Don't automatically follow redirects */
    char        *outFilename;       /* Output filename */
    int         packetSize;         /* HTTP/2 input frame size (min 16K) */
    char        *password;          /* Password for authentication */
    int         port;               /* Target port */
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
static Request  *allocRequest(void);
static void     checkRequestState(HttpStream *stream);
static Request  *createRequest(ThreadData *td, HttpNet *net);
static char     *extendUrl(cchar *url);
static void     finishRequest(Request *req);
static void     finishThread(MprThread *thread);
static cchar    *formatOutput(HttpStream *stream, cchar *buf, ssize *count);
static char     *getPassword(void);
static cchar    *getRedirectUrl(HttpStream *stream, cchar *url);
static HttpStream *getStream(Request *req);
static int      initSettings(void);
static int      initSsl(void);
static bool     isPort(cchar *name);
static void     manageApp(App *app, int flags);
static void     manageRequest(Request *req, int flags);
static void     manageThreadData(ThreadData *data, int flags);
static void     notifier(HttpStream *stream, int event, int arg);
static int      parseArgs(int argc, char **argv);
static void     parseStatus(HttpStream *stream);
static void     prepHeaders(Request *req);
static void     readBody(HttpStream *stream);
static int      prepUri(Request *req);
static int      processResponse(HttpStream *stream);
static int      setContentLength(HttpStream *stream);
static void     setDefaults(void);
static int      showUsage(void);
static void     startRequest(Request *req);
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
    app->maxRetries = 0;
    app->protocol = 1;
    app->success = 1;
    app->streams = 1;

    // zero means no timeout
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


static int parseArgs(int argc, char **argv)
{
    char    *argp, *key, *logSpec, *value, *traceSpec;
    int     i, setWorkers, nextArg;

    setWorkers = 0;
    app->needSsl = 0;
    logSpec = "stdout:1";
    traceSpec = "stdout:1";

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
                if (app->chunkSize > 0) {
                    mprAddItem(app->headers, mprCreateKeyPair("X-Chunk-Size", sfmt("%d", app->chunkSize), 0));
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
                if (app->iterations == 0) {
                    app->iterations = MAXINT;
                }
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

        } else if (smatch(argp, "--load") || smatch(argp, "-L")) {
            //  Undocumented and unsupported.  -i 0 -q -v -D
            app->iterations = MAXINT;
            mprSetDebugMode(1);
            app->timeout = HTTP_UNLIMITED;
            app->maxRetries = 0;
            app->noout++;
            app->verbose++;

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
            mprEprintf("%s\n", ME_VERSION);
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

        } else if (smatch(argp, "-x")) {
            //  Undocumented and unsupported (-D --nofollow -s)
            mprSetDebugMode(1);
            app->maxRetries = 0;
            app->timeout = HTTP_UNLIMITED;
            app->nofollow++;
            app->showHeaders++;

        } else if (smatch(argp, "--pj")) {
            /* Post JSON */
            if (nextArg >= argc) {
                return showUsage();
            } else {
                mprAddItem(app->headers, mprCreateKeyPair("Content-Type", "application/json", 0));
                app->method = "POST";
                app->bodyData = mprCreateBuf(-1, -1);
                mprPutStringToBuf(app->bodyData, argv[++nextArg]);
            }

        } else if (smatch(argp, "--zero")) {
            app->zeroOnErrors++;

        } else if (smatch(argp, "--")) {
            nextArg++;
            break;

        } else if (smatch(argp, "-")) {
            break;

        } else if (isdigit((uchar) argp[1])) {
            logSpec = sfmt("stdout:%d", (int) stoi(&argp[1]));
            traceSpec = sfmt("stdout:%d", (int) stoi(&argp[1]));

        } else {
            return showUsage();
        }
    }
    mprStartLogging(logSpec, MPR_LOG_CMDLINE);
    httpStartTracing(traceSpec);

    if (argc == nextArg) {
        return showUsage();
    }
    app->nextArg = nextArg;
    argc = argc - nextArg;
    argv = &argv[nextArg];
    app->target = argv[argc - 1];
    if (--argc > 0) {
        app->files = mprCreateList(0, -1);
        for (i = 0; i < argc; i++) {
            mprAddItem(app->files, argv[i]);
        }
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
        } else if (app->files) {
            app->method = "PUT";
        } else {
            app->method = "GET";
        }
    }
    return 0;
}


static int initSettings()
{
    HttpLimits      *limits;

    if (app->streams > 1 && app->protocol != 2) {
        mprLog("error http", 0, "Cannot use multiple streams except with HTTP/2 protocol");
        return MPR_ERR_BAD_ARGS;
    }
    app->hasData = app->bodyData || app->formData || app->files;

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

    /*
        Setup authentication
     */
    if (app->username) {
        if (app->password == 0 && !strchr(app->username, ':')) {
            app->password = getPassword();
        }
    }
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
        "  --http0               # Alias for --protocol HTTP/1.0 (default HTTP/1.1).\n"
        "  --http1               # Alias for --protocol HTTP/1.1 (default HTTP/1.1).\n"
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
    HttpUri     *uri;
    Request     *req;
    ThreadData  *td;
    cchar       *target;
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

    target = extendUrl(app->target);
    uri = httpCreateUri(target, HTTP_COMPLETE_URI_PATH);
    httpGetUriAddress(uri, &app->ip, &app->port);

    /*
        Pre-connect to the network. Required when creating multiple streams.
     */
    if (net->protocol >= 2 && httpConnectNet(net, app->ip, app->port, app->ssl) < 0) {
        httpNetError(net, "Cannot connect to %s:%d", app->ip, app->port);
        mprLog("error http", 0, "%s", net->errorMsg);
        return;
    }
    for (i = 0; i < app->streams && app->success; i++) {
        req = createRequest(td, net);
        mprAddItem(td->requests, req);
        mprCreateLocalEvent(td->dispatcher, "startRequest", 0, startRequest, req, 0);
        td->activeRequests++;
    }
    if (app->success) {
        mprYield(MPR_YIELD_STICKY);
        mprStopDispatcher(td->dispatcher);
        mprWaitForCond(td->cond, -1);
        mprResetYield();
    }
    httpDestroyNet(net);

    td->requests = 0;
    td->net = 0;
    td->dispatcher = 0;
    finishThread(thread);
}


static Request *createRequest(ThreadData *td, HttpNet *net)
{
    Request     *req;
    cchar       *path;

    req = allocRequest();
    req->threadData = td;
    req->net = net;
    req->upload = app->upload;

    /*
        Create file to save output
     */
    if (app->outFilename) {
        path = app->loadThreads > 1 ? sfmt("%s-%s.tmp", app->outFilename, mprGetCurrentThreadName()): app->outFilename;
        if ((req->outFile = mprOpenFile(path, O_CREAT | O_WRONLY | O_TRUNC | O_TEXT, 0664)) == 0) {
            mprLog("error http", 0, "Cannot open %s", path);
            app->success = 0;
            return 0;
        }
    } else {
        req->outFile = mprGetStdout();
    }
    return req;
}


/*
    Get the stream to use for the request. HTTP/1 reuses streams, HTTP/2 creates anew.
 */
static HttpStream *getStream(Request *req)
{
    HttpNet     *net;
    HttpStream  *stream;

    net = req->net;
    stream = req->stream;

    if (!stream || app->protocol >= 2) {
        if ((stream = httpCreateStream(net, 0)) == 0) {
            mprLog("error http", 0, "Cannot create stream: %s", net->errorMsg);
            app->success = 0;
            return NULL;
        }
        req->stream = stream;
        stream->data = req;

    } else {
        httpResetClientStream(stream, 0);
    }

    httpFollowRedirects(stream, !app->nofollow);
    httpSetStreamNotifier(stream, notifier);

    if (app->iterations == 1) {
        stream->limits->keepAliveMax = 0;
    }
    if (app->username) {
        httpSetCredentials(stream, app->username, app->password, app->authType);
    }
    /*
        Apply chunk size override if specified on command line
     */
    if (app->chunkSize > 0 && (app->bodyData || app->formData || app->files)) {
        httpSetChunkSize(stream, app->chunkSize);
    }
    return stream;
}


static void startRequest(Request *req)
{
    HttpNet     *net;
    HttpStream  *stream;

    stream = getStream(req);

    net = stream->net;

    if (app->singleStep) {
        waitForUser();
    }
    if (prepUri(req) < 0) {
        return;
    }
    req->redirect = 0;
    req->written = 0;

    prepHeaders(req);
    if (setContentLength(stream) < 0) {
        return;
    }
    if (httpConnect(stream, app->method, req->url, app->ssl) < 0) {
        mprLog("error http", 0, "Failed request for \"%s\". %s.", req->url, net->errorMsg);
        app->success = 0;
        if (!app->continueOnErrors) {
            mprCreateLocalEvent(stream->dispatcher, "done", 0, mprSignalCond, req->threadData->cond, 0);
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
    Request     *req;
    int         delay, limit;

    req = stream->data;

    switch (event) {
    case HTTP_EVENT_STATE:
        checkRequestState(stream);
        break;
    case HTTP_EVENT_READABLE:
        readBody(stream);
        break;
    case HTTP_EVENT_ERROR:
        break;
    case HTTP_EVENT_DONE:
        limit = (app->files && mprGetListLength(app->files) > 1) ? mprGetListLength(app->files) : app->iterations;
        if (++req->count >= limit || (!app->success && !app->continueOnErrors)) {
            finishRequest(req);
        } else {
            /*
                Slight delay to better load balance threads
             */
            delay = app->loadThreads > 2 ? 2: 0;
            mprCreateLocalEvent(stream->dispatcher, "startRequest", delay, startRequest, req, 0);
        }
    }
}


static void checkRequestState(HttpStream *stream)
{
    Request *req;
    cchar   *url;

    req = stream->data;
    switch (stream->state) {
    case HTTP_STATE_BEGIN:
        break;

    case HTTP_STATE_CONNECTED:
        if (!app->hasData) {
            httpFinalizeOutput(stream);
        } else {
            if (!req->written) {
                if (writeBody(stream) < 0) {
                    httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot write body data. %s", httpGetError(stream));
                }
                req->written = 1;
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
                if ((req->redirect = getRedirectUrl(stream, url)) == 0) {
                    httpError(stream, HTTP_CODE_BAD_REQUEST, "Invalid redirect");
                    break;
                }
                if (++req->follow >= app->maxFollow) {
                    httpError(stream, HTTP_CODE_NO_RESPONSE, "Too many redirects");
                    break;
                }
                mprDebug("http", 4, "redirect %d of %d for: %s %s", req->follow, app->maxFollow, app->method, req->url);
            } else {
                if (++req->retries >= app->maxRetries) {
                    httpError(stream, HTTP_CODE_NO_RESPONSE, "Too many retries");
                    break;
                }
                req->follow = 0;
                mprDebug("http", 4, "retry %d of %d for: %s %s", req->retries, app->maxRetries, app->method, req->url);
            }
            req->count--;

        } else {
            req->retries = 0;
            req->follow = 0;
            parseStatus(stream);
        }
        break;

    case HTTP_STATE_READY:
    case HTTP_STATE_RUNNING:
    case HTTP_STATE_FINALIZED:
        break;

    case HTTP_STATE_COMPLETE:
        processResponse(stream);
    }
}


static void parseStatus(HttpStream *stream)
{
    HttpRx      *rx;

    if (stream->net->error) {
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


static void prepHeaders(Request *req)
{
    HttpStream      *stream;
    MprKeyValue     *header;
    char            *seq, *url;
    int             next;
    static int      sequence = 0;

    stream = req->stream;
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
        mprLock(app->mutex);
        url = stok(req->url, "?", NULL);
        req->url = sfmt("%s?seq=%d", url, sequence);
        seq = itos(sequence++);
        httpSetHeaderString(stream, "X-Http-Seq", seq);
        mprUnlock(app->mutex);
    }
    if (app->ranges) {
        httpSetHeaderString(stream, "Range", app->ranges);
    }
    if (app->formData) {
        httpSetContentType(stream, "application/x-www-form-urlencoded");
    }
}


static int prepUri(Request *req)
{
    MprFile     *file;
    cchar       *base, *path;

    if (req->redirect) {
        req->url = sclone(req->redirect);

    } else if (req->upload) {
        req->url = extendUrl(app->target);
        /*
        if (app->verbose) {
            mprPrintf("Uploading: %s\n", req->url);
        } */

    } else if (app->files) {
        if (mprGetListLength(app->files) == 1) {
            path = mprGetItem(app->files, 0);
        } else {
            path = mprGetItem(app->files, req->count);
        }
        if (strcmp(path, "-") == 0) {
            file = mprAttachFileFd(0, "stdin", O_RDONLY | O_BINARY);
        } else {
            file = mprOpenFile(path, O_RDONLY | O_BINARY, 0);
        }
        if (file == 0) {
            mprLog("error http", 0, "Cannot open \"%s\"", path);
            return MPR_ERR_CANT_OPEN;
        }
        req->path = path;
        req->file = file;

        /*
            If URL ends with "/", assume it is a directory on the target and append each file name
         */
        base = mprGetPathBase(path);
        if (app->target[strlen(app->target) - 1] == '/') {
            req->url = mprJoinPath(app->target, base);
        } else {
            req->url = sclone(app->target);
        }
        req->url = extendUrl(req->url);
        /*
        if (app->verbose) {
            mprPrintf("Putting: %s to %s\n", path, req->url);
        } */
    } else {
        req->url = extendUrl(app->target);
    }
    req->uri = httpCreateUri(req->url, HTTP_COMPLETE_URI_PATH);
    httpGetUriAddress(req->uri, &req->ip, &req->port);
    return 0;
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
    Request     *req;
    HttpNet     *net;
    HttpRx      *rx;
    MprOff      bytesRead, contentLength;
    cchar       *msg, *responseHeaders, *sep;
    int         status;

    req = stream->data;
    net = stream->net;
    bytesRead = 0;

    if (!stream->rx) {
        return 0;
    }
    app->status = status = httpGetStatus(stream);
    contentLength = httpGetContentLength(stream);

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
        bytesRead = stream->rx->bytesRead;
    }
    if (stream->error) {
        app->success = 0;
        msg = (stream->errorMsg) ? stream->errorMsg : "";
        sep = (msg && *msg) ? "\n" : "";
        mprLog("error http", 0, "Failed \"%s\" request for %s%s%s", app->method, req->url, sep, msg);

    } else if (status < 0) {
        mprLog("error http", 0, "\nCannot process request for \"%s\" %s", req->url, httpGetError(stream));
        return MPR_ERR_CANT_READ;

    } else if (status == 0 && net->protocol == 0) {
        /* Ignore */;

    } else if (!(200 <= status && status <= 206) && !(301 <= status && status <= 304)) {
        if (!app->zeroOnErrors) {
            app->success = 0;
        }
        if (!app->showStatus) {
            mprLog("error http", 0, "\nCannot process request for %s \"%s\" (%d) %s", app->method, req->url, status, httpGetError(stream));
            return MPR_ERR_CANT_READ;
        }
    } else if (contentLength >= 0 && bytesRead != contentLength) {
        app->success = 0;
        mprLog("error http", 0, "Failed \"%s\" request for %s, content not fully received", app->method, req->url);
    }

    mprLock(app->mutex);
    app->fetchCount++;
    if (app->verbose && app->noout) {
        trace(stream, req->url, app->fetchCount, app->method, status, bytesRead);
    }
    mprUnlock(app->mutex);
    return 0;
}


static void readBody(HttpStream *stream)
{
    Request     *req;
    char        buf[ME_BUFSIZE];
    cchar       *result;
    ssize       bytes;

    req = stream->data;
    while (!stream->error && (bytes = httpRead(stream, buf, sizeof(buf))) > 0) {
        if (!app->noout) {
            result = formatOutput(stream, buf, &bytes);
            if (result) {
                mprWriteFile(req->outFile, result, bytes);
            }
        }
    }
    if (bytes <= 0 && req->outFile != mprGetStdout()) {
        mprCloseFile(req->outFile);
        req->outFile = 0;
    }
}


static int setContentLength(HttpStream *stream)
{
    Request     *req;
    MprPath     info;
    MprOff      len;
    char        *pair;
    int         next;

    req = stream->data;
    len = 0;
    if (req->upload) {
        httpEnableUpload(stream);
        return 0;
    }
    if (smatch(req->path, "-")) {
        if (mprGetPathInfo(req->path, &info) < 0) {
            httpError(stream, HTTP_CODE_GONE, "Cannot access file %s", req->path);
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


static ssize writeBody(HttpStream *stream)
{
    Request     *req;
    char        buf[ME_BUFSIZE], *pair;
    ssize       bytes, len, count, nbytes, sofar;
    int         next;

    req = stream->data;
    if (req->upload) {
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
        if (req->file) {
            /*
            if (app->verbose) {
                mprPrintf("Uploading: %s\n", req->path);
            } */
            while ((bytes = mprReadFile(req->file, buf, sizeof(buf))) > 0) {
                sofar = 0;
                while (bytes > 0) {
                    if ((nbytes = httpWriteBlock(stream->writeq, &buf[sofar], bytes, 0)) < 0) {
                        mprCloseFile(req->file);
                        req->file = 0;
                        return MPR_ERR_CANT_WRITE;
                    }
                    bytes -= nbytes;
                    sofar += nbytes;
                    assert(bytes >= 0);
                }
            }
            mprCloseFile(req->file);
            req->file = 0;
            httpEnableNetEvents(stream->net);
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


static void finishRequest(Request *req)
{
    ThreadData  *td;

    if (req) {
        td = req->threadData;
        mprLock(app->mutex);
        if (--td->activeRequests <= 0) {
            /*
                Run as an event so the stack httpIO stack unwinds before threadMain destroys the network.
             */
            mprCreateLocalEvent(req->stream->dispatcher, "done", 0, mprSignalCond, req->threadData->cond, 0);
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


static Request *allocRequest()
{
    return mprAllocObj(Request, manageRequest);
}


static void manageRequest(Request *req, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(req->file);
        mprMark(req->ip);
        mprMark(req->net);
        mprMark(req->outFile);
        mprMark(req->path);
        mprMark(req->stream);
        mprMark(req->threadData);
        mprMark(req->uri);
        mprMark(req->url);
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
    cchar   *proto;

    proto = app->protocol >= 2 ? "https" : "http";

    if (*url == '/') {
        if (app->host) {
            if (sncaselesscmp(app->host, "http://", 7) != 0 && sncaselesscmp(app->host, "https://", 8) != 0) {
                return sfmt("%s://%s%s", proto, app->host, url);
            } else {
                return sfmt("%s%s", app->host, url);
            }
        } else {
            return sfmt("http://127.0.0.1%s", url);
        }
    }
    if (sncaselesscmp(url, "http://", 7) != 0 && sncaselesscmp(url, "https://", 8) != 0) {
        if (*url == ':' && isPort(&url[1])) {
            return sfmt("%s://127.0.0.1%s", proto, url);
        } else if (isPort(url)) {
            return sfmt("%s://127.0.0.1:%s", proto, url);
        } else {
            return sfmt("%s://%s", proto, url);
        }
    }
    return sclone(url);
}


static int initSsl(void)
{
#if ME_COM_SSL
    HttpUri     *uri;
    cchar       *target;

    target = extendUrl(app->target);
    uri = httpCreateUri(target, HTTP_COMPLETE_URI_PATH);
    if (uri->secure || app->needSsl) {
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
    if (sncaselesscmp(url, "http://", 7) == 0) {
        url += 7;
    } else if (sncaselesscmp(url, "https://", 8) == 0) {
        url += 8;
    }
    if ((fetchCount % 200) == 1) {
        if (fetchCount == 1 || (fetchCount % 5000) == 1) {
            if (fetchCount > 1) {
                mprPrintf("\n");
            }
            mprPrintf("  Count   Thread   Op  Code   Bytes  Url\n");
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
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
