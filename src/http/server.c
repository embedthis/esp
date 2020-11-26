/**
    server.c  -- Http server test program

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.

    usage: server [options]
            --config configFile     # Use given config file instead
            --debugger              # Disable timeouts to make debugging easier
            --home path             # Set the home working directory
            --log logFile:level     # Log to file file at verbosity level
            --name uniqueName       # Name for this instance
            --show                  # Show route table
            --trace traceFile:level # Log to file file at verbosity level
            --version               # Output version information
            -v                      # Same as --log stderr:2
            -DIGIT                  # Same as --log stderr:DIGIT
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Locals ************************************/
/*
    Global application object. Provides the top level roots of all data objects for the GC.
 */
typedef struct App {
    Mpr         *mpr;
    MprSignal   *signal;
    cchar       *documents;
    cchar       *home;
    cchar       *configFile;
    cchar       *pathVar;
    int         show;
} App;

static App *app;

/***************************** Forward Declarations ***************************/

static int createEndpoints(int argc, char **argv);
static int findConfig(void);
static void manageApp(App *app, int flags);
static void showState(void *ignored, MprSignal *sp);
static void usageError(void);

/*********************************** Code *************************************/

MAIN(http, int argc, char **argv, char **envp)
{
    Mpr     *mpr;
    cchar   *argp;
    char    *logSpec, *traceSpec;
    int     argind;

    logSpec = 0;
    traceSpec = 0;

    if ((mpr = mprCreate(argc, argv, MPR_USER_EVENTS_THREAD)) == NULL) {
        exit(1);
    }
    if ((app = mprAllocObj(App, manageApp)) == NULL) {
        exit(2);
    }
    mprAddRoot(app);
    mprAddStandardSignals();

    if (httpCreate(HTTP_CLIENT_SIDE | HTTP_SERVER_SIDE) < 0) {
        exit(2);
    }
    app->mpr = mpr;
    app->home = mprGetCurrentPath();
    argc = mpr->argc;
    argv = (char**) mpr->argv;

    for (argind = 1; argind < argc; argind++) {
        argp = argv[argind];
        if (*argp != '-') {
            break;
        }
        if (smatch(argp, "--config") || smatch(argp, "--conf")) {
            if (argind >= argc) {
                usageError();
            }
            app->configFile = sclone(argv[++argind]);

        } else if (smatch(argp, "--debugger") || smatch(argp, "-D")) {
            mprSetDebugMode(1);

        } else if (smatch(argp, "--log") || smatch(argp, "-l")) {
            if (argind >= argc) {
                usageError();
            }
            logSpec = argv[++argind];

        } else if (smatch(argp, "--name") || smatch(argp, "-n")) {
            if (argind >= argc) {
                usageError();
            }
            mprSetAppName(argv[++argind], 0, 0);

        } else if (smatch(argp, "--show") || smatch(argp, "-s")) {
            app->show = 1;

        } else if (smatch(argp, "--trace") || smatch(argp, "-t")) {
            if (argind >= argc) {
                usageError();
            }
            traceSpec = argv[++argind];

        } else if (smatch(argp, "--verbose") || smatch(argp, "-v")) {
            if (!logSpec) {
                logSpec = sfmt("stderr:2");
            }
            if (!traceSpec) {
                traceSpec = sfmt("stderr:2");
            }

        } else if (smatch(argp, "--version") || smatch(argp, "-V")) {
            mprPrintf("%s\n", ME_VERSION);
            exit(0);

        } else if (*argp == '-' && isdigit((uchar) argp[1])) {
            if (!logSpec) {
                logSpec = sfmt("stderr:%d", (int) stoi(&argp[1]));
            }
            if (!traceSpec) {
                traceSpec = sfmt("stderr:%d", (int) stoi(&argp[1]));
            }

        } else if (smatch(argp, "-?") || scontains(argp, "-help")) {
            usageError();
            exit(3);

        } else if (*argp == '-') {
            mprLog("error http", 0, "Unknown switch \"%s\"", argp);
            usageError();
            exit(4);
        }
    }
    if (logSpec) {
        mprStartLogging(logSpec, MPR_LOG_CMDLINE);
    }
    if (traceSpec) {
        httpStartTracing(traceSpec);
    }
    if (mprStart() < 0) {
        mprLog("error http", 0, "Cannot start MPR");
        mprDestroy();
        return MPR_ERR_CANT_INITIALIZE;
    }
    if (argc == argind && !app->configFile) {
        if (findConfig() < 0) {
            exit(5);
        }
    }
    if (createEndpoints(argc - argind, &argv[argind]) < 0) {
        return MPR_ERR_CANT_INITIALIZE;
    }
    if (httpStartEndpoints() < 0) {
        mprLog("error http", 0, "Cannot start HTTP service, exiting.");
        exit(6);
    }
    if (app->show) {
        httpLogRoutes(0, 0);
    }
    mprServiceEvents(-1, 0);

    mprLog("info http", 1, "Stopping http-server ...");
    mprDestroy();
    return mprGetExitStatus();
}


static void manageApp(App *app, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(app->signal);
        mprMark(app->configFile);
        mprMark(app->pathVar);
    }
}


static int createEndpoints(int argc, char **argv)
{
    HttpHost    *host;
    HttpRoute   *route;

    host = httpCreateHost();
    httpSetDefaultHost(host);
    route = httpCreateRoute(host);
    httpSetHostDefaultRoute(host, route);
    httpFinalizeRoute(route);

    httpInitConfig(route);
    if (httpLoadConfig(route, app->configFile) < 0) {
        return MPR_ERR_CANT_CREATE;
    }
    mprGC(MPR_GC_FORCE | MPR_GC_COMPLETE);
    app->signal = mprAddSignalHandler(ME_SIGINFO, showState, 0, 0, MPR_SIGNAL_AFTER);
    return 0;
}


static int findConfig()
{
    char    *base, *filename;

    base = sclone("http.json");
    filename = base;
    if (!mprPathExists(filename, R_OK)) {
        filename = mprJoinPath(mprGetPathParent(mprGetAppDir()), base);
        if (!mprPathExists(filename, R_OK)) {
            filename = mprJoinPath(mprGetPathParent(mprGetAppDir()), base);
            if (!mprPathExists(filename, R_OK)) {
                mprError("Cannot find config file %s", base);
                return MPR_ERR_CANT_OPEN;
            }
        }
    }
    app->configFile = filename;
    return 0;
}


static void usageError()
{
    cchar   *name;

    name = mprGetAppName();
    mprEprintf("\n%s Usage:\n\n"
        "  %s [options]\n"
        "  Options:\n"
        "    --config configFile     # Use named config file instead http.json\n"
        "    --debugger              # Disable timeouts to make debugging easier\n"
        "    --log logFile:level     # Log to file at verbosity level (0-5)\n"
        "    --name uniqueName       # Unique name for this instance\n"
        "    --show                  # Show route table\n"
        "    --trace traceFile:level # Trace to file at verbosity level (0-5)\n"
        "    --verbose               # Same as --log stderr:2\n"
        "    --version               # Output version information\n"
        "    --DIGIT                 # Same as --log stderr:DIGIT\n\n",
        mprGetAppTitle(), name);
    exit(7);
}


static void showState(void *ignored, MprSignal *sp)
{
    mprLog(0, 0, "%s", httpStatsReport(0));
    if (MPR->heap->track) {
        mprPrintMem("MPR Memory Report", MPR_MEM_DETAIL);
    } else {
        mprPrintMem("MPR Memory Report", 0);
    }
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
