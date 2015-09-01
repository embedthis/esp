/**
    esp.c -- ESP command program

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "esp.h"
#include    "mpr-version.h"

#if ME_COM_ESP || ME_ESP_PRODUCT
/********************************** Locals ************************************/
/*
    Global application object. Provides the top level roots of all data objects for the GC.
 */
typedef struct App {
    Mpr         *mpr;

    cchar       *description;           /* Application description */
    cchar       *name;                  /* Application name */
    cchar       *cipher;                /* Cipher for passwords: "md5" or "blowfish" */
    cchar       *currentDir;            /* Initial starting current directory */
    cchar       *database;              /* Database provider "mdb" | "sdb" */
    cchar       *version;               /* Application version */

    cchar       *binDir;                /* Bin directory */
    cchar       *home;                  /* Home directory */
    cchar       *paksCacheDir;          /* Paks cache directory */
    cchar       *paksDir;               /* Local paks directory */
    cchar       *migDir;                /* Migrations directory */
    cchar       *listen;                /* Listen endpoint for "esp run" */
    cchar       *platform;              /* Target platform os-arch-profile (lower) */

    int         combine;                /* Combine all inputs into one, combine output */
    cchar       *combinePath;           /* Output filename for combine compilations */
    MprFile     *combineFile;           /* Output file for combine compilations */
    MprList     *combineItems;          /* Items to invoke from Init */

    MprList     *routes;                /* Routes to process */
    EspRoute    *eroute;                /* Selected ESP route to build */
    MprJson     *config;                /* Configuration from esp.json (only) */
    MprJson     *package;               /* Configuration from package.json (only) */
    HttpRoute   *route;                 /* Selected route to build */
    HttpHost    *host;                  /* Default host */
    MprList     *files;                 /* List of files to process */
    MprHash     *build;                 /* Items to build */
    MprHash     *built;                 /* Items that have been built */
    MprHash     *targets;               /* Command line targets */
    EdiGrid     *migrations;            /* Migrations table */

    cchar       *command;               /* Compilation or link command */
    cchar       *cacheName;             /* Cached MD5 name */
    cchar       *csource;               /* Name of "C" source for page or controller */
    cchar       *genlink;               /* DEPRECATED: Static link resolution file */
    cchar       *filterRoutePattern;    /* Poute pattern to use for filtering */
    cchar       *filterRoutePrefix;     /* Prefix of route to use for filtering */
    cchar       *logSpec;               /* Arg for --log */
    cchar       *traceSpec;             /* Arg for --trace */
    cchar       *mode;                  /* New "pak.mode" to use */
    cchar       *module;                /* Compiled module name */
    cchar       *base;                  /* Base filename */
    cchar       *entry;                 /* Module entry point */
    cchar       *controller;            /* Controller name for generated entities (lower case) */
    cchar       *password;              /* Password for auth */
    cchar       *title;                 /* Title name for generated entities */
    cchar       *table;                 /* Override table name for migrations, tables */

    int         compileMode;            /* Debug or release compilation */
    int         error;                  /* Any processing error */
    int         keep;                   /* Keep source */
    int         force;                  /* Force the requested action, ignoring unfullfilled dependencies */
    int         quiet;                  /* Don't trace progress */
    int         noupdate;               /* Do not update the esp.json */
    int         require;                /* Initialization requirement flags */
    int         rebuild;                /* Force a rebuild */
    int         reverse;                /* Reverse migrations */
    int         show;                   /* Show routes and compilation commands */
    int         silent;                 /* Totally silent */
    int         singleton;              /* Generate a singleton resource controller */
    int         staticLink;             /* Use static linking */
    int         verbose;                /* Verbose mode */
    int         why;                    /* Why rebuild */
} App;

static App       *app;                  /* Top level application object */
static Esp       *esp;                  /* ESP control object */
static Http      *http;                 /* HTTP service object */
static int       nextMigration;         /* Sequence number for next migration */

/*
    Initialization requirement flags
 */
#define REQ_TARGETS     0x2             /* Require targets list */
#define REQ_ROUTES      0x4             /* Require esp routes */
#define REQ_CONFIG      0x8             /* Require esp.json, otherwise load only if present */
#define REQ_LISTEN      0x10            /* Explicit listening address supplied */
#define REQ_SERVE       0x20            /* Will be running as a server */
#define REQ_NAME        0x40            /* Set "name" */

/*
    CompileFile flags
 */
#define ESP_CONTROlLER  0x1             /* Compile a controller */
#define ESP_VIEW        0x2             /* Compile a view */
#define ESP_PAGE        0x4             /* Compile a stand-alone ESP page */
#define ESP_MIGRATION   0x8             /* Compile a database migration */
#define ESP_SRC         0x10            /* Files in src/ */

#define ESP_FOUND_TARGET 1

#define MAX_PASS        64
#define MAX_VER         1000000000
#define VER_FACTOR      1000
#define VER_FACTOR_MAX  "999"

#define ESP_MIGRATIONS  "_EspMigrations"

#define ESP_PAKS_DIR    "paks"          /* Default paks dir */
#define ESP_MIG_DIR     "migrations"    /* Default migrations dir */

/***************************** Forward Declarations ***************************/

static void clean(int argc, char **argv);
static void config();
static void compile(int argc, char **argv);
static void compileFile(HttpRoute *route, cchar *source, int kind);
static void compileCombined(HttpRoute *route);
static void compileItems(HttpRoute *route);
static App *createApp(Mpr *mpr);
static void createMigration(cchar *name, cchar *table, cchar *comment, int fieldCount, char **fields);
static void editValue(int argc, char **argv);
static void fail(cchar *fmt, ...);
static void fatal(cchar *fmt, ...);
static void generate(int argc, char **argv);
static void generateController(int argc, char **argv);
static void generateItem(cchar *item);
static void genKey(cchar *key, cchar *path, MprHash *tokens);
static void generateMigration(int argc, char **argv);
static char *getPassword();
static void generateScaffold(int argc, char **argv);
static void generateTable(int argc, char **argv);
static cchar *getJson(MprJson *config, cchar *key, cchar *defaultValue);
static MprList *getRoutes();
static MprHash *getTargets(int argc, char **argv);
static cchar *getTemplate(cchar *key, MprHash *tokens);
static cchar *getPakVersion(cchar *name, cchar *version);
static bool identifier(cchar *name);
static void initialize(int argc, char **argv);
static void init(int argc, char **argv);
static MprJson *loadJson(cchar *path);
static void makeEspDir(cchar *dir);
static void makeEspFile(cchar *path, cchar *data, ssize len);
static MprHash *makeTokens(cchar *path, MprHash *other);
static void manageApp(App *app, int flags);
static void migrate(int argc, char **argv);
static int parseArgs(int argc, char **argv);
static void parseCommand(int argc, char **argv);
static void process(int argc, char **argv);
static MprJson *readConfig();
static cchar *readTemplate(cchar *path, MprHash *tokens, ssize *len);
static bool requiredRoute(HttpRoute *route);
static int reverseSortFiles(MprDirEntry **d1, MprDirEntry **d2);
static void role(int argc, char **argv);
static void serve(int argc, char **argv);
static void saveConfig(MprJson *config, cchar *path, int flags);
static bool selectResource(cchar *path, cchar *kind);
static bool selectView(HttpRoute *route, cchar *path);
static void setConfigValue(MprJson *config, cchar *key, cchar *value);
static void setMode(cchar *mode);
static int sortFiles(MprDirEntry **d1, MprDirEntry **d2);
static void qtrace(cchar *tag, cchar *fmt, ...);
static void trace(cchar *tag, cchar *fmt, ...);
static void usageError();
static void user(int argc, char **argv);
static void vtrace(cchar *tag, cchar *fmt, ...);
static void why(cchar *path, cchar *fmt, ...);

#if ME_WIN_LIKE || VXWORKS
static char *getpass(char *prompt);
#endif
/*********************************** Code *************************************/

PUBLIC int main(int argc, char **argv)
{
    Mpr     *mpr;
    int     options, rc;

    if ((mpr = mprCreate(argc, argv, 0)) == 0) {
        exit(1);
    }
    if ((app = createApp(mpr)) == 0) {
        exit(2);
    }
    options = parseArgs(argc, argv);
    process(argc - options, &argv[options]);
    rc = app->error;
    mprDestroy();
    return rc;
}


/*
    Create a master App object for esp. This provids a common root for all esp data.
 */
static App *createApp(Mpr *mpr)
{
    if ((app = mprAllocObj(App, manageApp)) == 0) {
        return 0;
    }
    mprAddRoot(app);
    mprAddStandardSignals();

    app->mpr = mpr;
    app->listen = sclone(ESP_LISTEN);
    app->paksDir = sclone(ESP_PAKS_DIR);
    app->migDir = sclone(ESP_MIG_DIR);
#if ME_COM_SQLITE
    app->database = sclone("sdb");
#elif ME_COM_MDB
    app->database = sclone("mdb");
#else
    mprLog("", 0, "No database provider defined");
#endif
    app->cipher = sclone("blowfish");
    return app;
}


static void manageApp(App *app, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(app->base);
        mprMark(app->binDir);
        mprMark(app->build);
        mprMark(app->built);
        mprMark(app->cacheName);
        mprMark(app->cipher);
        mprMark(app->combineFile);
        mprMark(app->combineItems);
        mprMark(app->combinePath);
        mprMark(app->command);
        mprMark(app->config);
        mprMark(app->controller);
        mprMark(app->csource);
        mprMark(app->currentDir);
        mprMark(app->database);
        mprMark(app->description);
        mprMark(app->entry);
        mprMark(app->eroute);
        mprMark(app->files);
        mprMark(app->filterRoutePattern);
        mprMark(app->filterRoutePrefix);
        mprMark(app->genlink);
        mprMark(app->home);
        mprMark(app->host);
        mprMark(app->listen);
        mprMark(app->logSpec);
        mprMark(app->migrations);
        mprMark(app->migDir);
        mprMark(app->mode);
        mprMark(app->module);
        mprMark(app->mpr);
        mprMark(app->name);
        mprMark(app->package);
        mprMark(app->paksCacheDir);
        mprMark(app->paksDir);
        mprMark(app->password);
        mprMark(app->platform);
        mprMark(app->route);
        mprMark(app->routes);
#if DEPRECATED
        mprMark(app->slink);
#endif
        mprMark(app->table);
        mprMark(app->targets);
        mprMark(app->title);
        mprMark(app->traceSpec);
        mprMark(app->version);
    }
}


static int parseArgs(int argc, char **argv)
{
    cchar   *argp;
    int     argind;

   for (argind = 1; argind < argc && !app->error; argind++) {
        argp = argv[argind];
        if (*argp++ != '-') {
            break;
        }
        if (*argp == '-') {
            argp++;
        }
        if (smatch(argp, "chdir") || smatch(argp, "home")) {
            if (argind >= argc) {
                usageError();
            } else {
                argp = argv[++argind];
                if (chdir((char*) argp) < 0) {
                    fail("Cannot change directory to %s", argp);
                }
                app->home = sclone(argv[++argind]);
            }

        } else if (smatch(argp, "cipher")) {
            if (argind >= argc) {
                usageError();
            } else {
                app->cipher = sclone(argv[++argind]);
            }

        } else if (smatch(argp, "database")) {
            if (argind >= argc) {
                usageError();
            } else {
                app->database = sclone(argv[++argind]);
                if (!smatch(app->database, "mdb") && !smatch(app->database, "sdb")) {
                    fail("Unknown database \"%s\"", app->database);
                    usageError();
                }
            }

        } else if (smatch(argp, "debugger") || smatch(argp, "D")) {
            mprSetDebugMode(1);

        } else if (smatch(argp, "force") || smatch(argp, "f")) {
            app->force = 1;

#if DEPRECATED || 1
        } else if (smatch(argp, "genlink") || smatch(argp, "g")) {
            if (argind >= argc) {
                usageError();
            } else {
                app->genlink = sclone(argv[++argind]);
            }
#endif

        } else if (smatch(argp, "keep") || smatch(argp, "k")) {
            app->keep = 1;

        } else if (smatch(argp, "listen") || smatch(argp, "l")) {
            if (argind >= argc) {
                usageError();
            } else {
                app->listen = sclone(argv[++argind]);
            }

        } else if (smatch(argp, "log") || smatch(argp, "l")) {
            if (argind >= argc) {
                usageError();
            } else {
                app->logSpec = sclone(argv[++argind]);
            }

        } else if (smatch(argp, "name")) {
            if (argind >= argc) {
                usageError();
            } else {
                if (!identifier(argv[++argind])) {
                    fail("Application name must be a valid C identifier");
                } else {
                    app->name = sclone(argv[argind]);
                    app->title = stitle(app->name);
                }
            }

        } else if (smatch(argp, "noupdate")) {
            app->noupdate = 1;

        } else if (smatch(argp, "optimize") || smatch(argp, "optimized")) {
            app->compileMode = ESP_COMPILE_OPTIMIZED;

#if DEPRECATED || 1
        } else if (smatch(argp, "overwrite")) {
            app->force = 1;
#endif

        } else if (smatch(argp, "platform")) {
            if (argind >= argc) {
                usageError();
            } else {
                app->platform = sclone(argv[++argind]);
            }

        } else if (smatch(argp, "quiet") || smatch(argp, "q")) {
            app->quiet = 1;

        } else if (smatch(argp, "rebuild") || smatch(argp, "r")) {
            app->rebuild = 1;

        //  DEPRECATED routeName
        } else if (smatch(argp, "route") || smatch(argp, "routeName")) {
            if (argind >= argc) {
                usageError();
            } else {
                app->filterRoutePattern = sclone(argv[++argind]);
            }

        } else if (smatch(argp, "routePrefix")) {
            if (argind >= argc) {
                usageError();
            } else {
                app->filterRoutePrefix = sclone(argv[++argind]);
            }

        } else if (smatch(argp, "show") || smatch(argp, "s")) {
            app->show++;

        } else if (smatch(argp, "silent")) {
            app->silent = 1;
            app->quiet = 1;

        } else if (smatch(argp, "singleton") || smatch(argp, "single")) {
            app->singleton = 1;

        } else if (smatch(argp, "static")) {
            app->staticLink = 1;

        } else if (smatch(argp, "symbols")) {
            app->compileMode = ESP_COMPILE_SYMBOLS;

        } else if (smatch(argp, "table")) {
            if (argind >= argc) {
                usageError();
            } else {
                app->table = sclone(argv[++argind]);
            }

        } else if (smatch(argp, "trace") || smatch(argp, "l")) {
            if (argind >= argc) {
                usageError();
            } else {
                app->traceSpec = sclone(argv[++argind]);
            }
        } else if (smatch(argp, "verbose") || smatch(argp, "v")) {
            app->verbose++;
            if (!app->logSpec) {
                app->logSpec = sfmt("stdout:2");
            }
            if (!app->traceSpec) {
                app->traceSpec = sfmt("stdout:2");
            }

        } else if (smatch(argp, "version") || smatch(argp, "V")) {
            mprPrintf("%s\n", ESP_VERSION);
            exit(0);

        } else if (isdigit((uchar) *argp)) {
            app->verbose++;
            if (!app->logSpec) {
                app->logSpec = sfmt("stdout:%d", (int) stoi(argp));
            }
            if (!app->traceSpec) {
                app->traceSpec = sfmt("stdout:%d", (int) stoi(argp));
            }

        } else if (smatch(argp, "why") || smatch(argp, "w")) {
            app->why = 1;

        } else {
            if (!smatch(argp, "?") && !smatch(argp, "help")) {
                fail("Unknown switch \"%s\"", argp);
            }
            usageError();
        }
    }
    parseCommand(argc - argind, &argv[argind]);
    return argind;
}


static void parseCommand(int argc, char **argv)
{
    cchar       *cmd;

    if (app->error) {
        return;
    }
    cmd = argv[0];

    if (argc == 0) {
        /* Run */
        app->require = REQ_SERVE;

    } else if (smatch(cmd, "config")) {
        app->require = 0;

    } else if (smatch(cmd, "clean")) {
        app->require = REQ_TARGETS | REQ_ROUTES;

    } else if (smatch(cmd, "compile")) {
        app->require = REQ_TARGETS | REQ_ROUTES;

    } else if (smatch(cmd, "generate")) {
        app->require = REQ_CONFIG;

    } else if (smatch(cmd, "edit")) {
        app->require = REQ_CONFIG;

    } else if (smatch(cmd, "init")) {
        if (!app->name) {
            app->name = (argc >= 2) ? sclone(argv[1]) : mprGetPathBase(mprGetCurrentPath());
        }
        if (argc >= 3) {
            app->version = sclone(argv[2]);
        }
        app->require = REQ_NAME;

    } else if (smatch(cmd, "migrate")) {
        app->require = REQ_ROUTES;

    } else if (smatch(cmd, "mode")) {
        /* Need config and routes because it does a clean */
        app->require = REQ_CONFIG | REQ_ROUTES;

    } else if (smatch(cmd, "role")) {
        app->require = REQ_CONFIG;

    } else if (smatch(cmd, "serve") || smatch(cmd, "run")) {
        app->require = REQ_SERVE;
        if (argc > 1) {
            app->require = REQ_LISTEN;
        }

    } else if (smatch(cmd, "user")) {
        app->require = REQ_CONFIG;

    } else if (isdigit((uchar) *cmd)) {
        app->require = REQ_LISTEN | REQ_SERVE;

    } else if (cmd && *cmd) {
        fail("Unknown command \"%s\"", cmd);
    }
}


static void initRuntime()
{
    cchar   *home;

    if (app->error) {
        return;
    }
    if (httpCreate(HTTP_CLIENT_SIDE | HTTP_SERVER_SIDE) < 0) {
        fail("Cannot create HTTP service for %s", mprGetAppName());
        return;
    }
    http = MPR->httpService;
    app->route = httpGetDefaultRoute(0);

    mprStartLogging(app->logSpec, MPR_LOG_CMDLINE);
    if (app->traceSpec) {
        httpStartTracing(app->traceSpec);
    }
    app->currentDir = mprGetCurrentPath();
    app->binDir = mprGetAppDir();

    if ((home = getenv("HOME")) != 0) {
        app->paksCacheDir = mprJoinPath(home, ".paks");
    } else {
        app->paksCacheDir = mprJoinPath(mprGetPathParent(mprGetAppDir()), ".paks");
    }
    if (mprStart() < 0) {
        mprLog("", 0, "Cannot start MPR for %s", mprGetAppName());
        mprDestroy();
        app->error = 1;
        return;
    }
    if (app->platform) {
        httpSetPlatformDir(app->platform);
    } else {
        app->platform = http->platform;
        httpSetPlatformDir(NULL);
    }
    mprLog("", 4, "Platform \"%s\"", http->platformDir);
    if (!http->platformDir) {
        if (app->platform) {
            fail("Cannot find platform: \"%s\"", app->platform);
        }
        return;
    }
    HTTP->staticLink = app->staticLink;

    if (app->error) {
        return;
    }
    if (espOpen(NULL) < 0) {
        app->error = 1;
    }
}


static void initialize(int argc, char **argv)
{
    HttpStage   *stage;
    HttpRoute   *route;
    cchar       *criteria, *path;

    if (app->error) {
        return;
    }
    /*
        Trace to stdout if not serving or invoked a plain "esp" to serve
     */
    if (!app->logSpec && (argc == 0 || !(app->require & REQ_SERVE))) {
        app->logSpec = sfmt("stdout:1");
    }
    initRuntime();
    if (app->error) {
        return;
    }
    if (!app->name) {
        app->name = mprGetPathBase(mprGetCurrentPath());
    }
    if (!app->title) {
        app->title = stitle(app->name);
    }
    if (!app->description) {
        app->description = sfmt("%s Application", app->title);
    }
    if (!app->version) {
        app->version = sclone("0.1.0");
    }
    route = app->route;
    if (!(app->require & REQ_SERVE)) {
        route->flags |= HTTP_ROUTE_NO_LISTEN;
    }
    if (app->require & REQ_LISTEN) {
        route->flags |= HTTP_ROUTE_OWN_LISTEN;
    }

    /*
        Read package.json first so esp.json can override
     */
    path = mprJoinPath(route->home, "package.json");
    if (mprPathExists(path, R_OK)) {
        if ((app->package = readConfig(path)) == 0) {
            return;
        }
        if ((criteria = getJson(app->package, "devDependencies.esp", 0)) != 0) {
            if (!mprIsVersionAcceptable(ME_VERSION, criteria)) {
                fail("ESP %s is not acceptable for this application which requires ESP %s", ME_VERSION, criteria);
            }
        }
        app->paksDir = getJson(app->package, "directories.paks", app->paksDir);
    }

    app->description = getJson(app->package, "description", app->description);
    app->name = getJson(app->package, "name", app->name);
    app->title = getJson(app->package, "title", app->title);
    app->version = getJson(app->package, "version", app->version);

    path = mprJoinPath(route->home, "esp.json");
    if (mprPathExists(path, R_OK)) {
        if ((app->config = readConfig(path)) == 0) {
            return;
        }
    }
    /*
        Read name, title, description and version from esp.json - permits execution without package.json
     */
    app->description = getJson(app->config, "description", app->description);
    app->name = getJson(app->config, "name", app->name);
    app->title = getJson(app->config, "title", app->title);
    app->version = getJson(app->config, "version", app->version);

    if (!app->config) {
        app->config = mprParseJson(sfmt("{ name: '%s', title: '%s', description: '%s', version: '%s', \
            http: {}, esp: {}}",
            app->name, app->title, app->description, app->version));
    }
    if (!app->package) {
        app->package = mprParseJson(sfmt("{ name: '%s', title: '%s', description: '%s', version: '%s', \
            dependencies: {}}", app->name, app->title, app->description, app->version));
    }

    if (app->require & REQ_NAME) {
        if (!identifier(app->name)) {
            if (argc >= 1) {
                fail("Application name must be a valid C identifier");
            } else {
                fail("Directory name is used as application name and must be a valid C identifier");
            }
            return;
        }
    }
    if (app->require & REQ_TARGETS) {
        app->targets = getTargets(argc - 1, &argv[1]);
    }
    if (!route->eroute) {
        espCreateRoute(route);
    }
    app->eroute = route->eroute;

    if (mprPathExists("esp.json", R_OK)) {
        if (espLoadApp(route, 0, "esp.json")) {
            fail("Cannot define and load ESP app");
            return;
        }
#if DEPRECATED || 1
    } else if (mprPathExists("package.json", R_OK)) {
        if (espLoadApp(route, 0, "package.json")) {
            fail("Cannot define and load ESP app");
            return;
        }
#endif
    } else {
        /*
            No esp.json - not an ESP app.
         */
        espSetDefaultDirs(route);
        httpAddRouteHandler(route, "espHandler", "esp");
        httpAddRouteHandler(route, "fileHandler", "");
        httpAddRouteIndex(route, "index.esp");
        httpAddRouteIndex(route, "index.html");
        httpSetRouteShowErrors(route, 1);
        route->update = 1;
        espLoadCompilerRules(route);
    }
    httpFinalizeRoute(route);
    app->routes = getRoutes();
    if ((stage = httpLookupStage("espHandler")) == 0) {
        fail("Cannot find ESP handler");
        return;
    }
    esp = stage->stageData;
    esp->compileMode = app->compileMode;

    /*
        Do this after espSetDefaultDirs
     */
#if DEPRECATE || 1
    path = mprJoinPath(route->home, "db/migrations");
#endif
    if (mprPathExists(path, R_OK)) {
        app->migDir = path;
        httpSetDir(route, "MIGRATIONS", path);
    } else {
        app->migDir = httpGetDir(route, "MIGRATIONS");
        app->migDir = getJson(app->package, "directories.migrations", app->migDir);
    }
    mprGC(MPR_GC_FORCE | MPR_GC_COMPLETE);
}


static void process(int argc, char **argv)
{
    cchar       *cmd;

    initialize(argc, argv);
    if (app->error) {
        return;
    }
    if (argc == 0) {
        serve(argc, argv);
        return;
    }
    cmd = argv[0];

    if (smatch(cmd, "config")) {
        config();

    } else if (smatch(cmd, "clean")) {
        clean(argc -1, &argv[1]);

    } else if (smatch(cmd, "compile")) {
        compile(argc -1, &argv[1]);

    } else if (smatch(cmd, "edit")) {
        editValue(argc - 1, &argv[1]);

    } else if (smatch(cmd, "generate")) {
        generate(argc - 1, &argv[1]);

    } else if (smatch(cmd, "init")) {
        init(argc - 1, &argv[1]);

    } else if (smatch(cmd, "migrate")) {
        migrate(argc - 1, &argv[1]);

    } else if (smatch(cmd, "mode")) {
        if (argc < 2) {
            printf("%s\n", getJson(app->package, "pak.mode", "undefined"));
        } else {
            setMode(argv[1]);
        }

    } else if (smatch(cmd, "role")) {
        role(argc - 1, &argv[1]);

    } else if (smatch(cmd, "serve")) {
        serve(argc - 1, &argv[1]);

    } else if (smatch(cmd, "user")) {
        user(argc - 1, &argv[1]);

    } else if (isdigit((uchar) *cmd)) {
        serve(1, (char**) &cmd);
    }
}


static void config()
{
    printf("ESP configuration:\n");
    printf("Pak cache dir \"%s\"\n", app->paksCacheDir);
    printf("Paks dir      \"%s\"\n", app->paksDir);
    printf("Binaries dir  \"%s\"\n", app->binDir);
}


static void clean(int argc, char **argv)
{
    MprList         *files;
    MprDirEntry     *dp;
    HttpRoute       *route;
    cchar           *cacheDir, *path;
    int             next, nextFile;

    if (app->error) {
        return;
    }
    for (ITERATE_ITEMS(app->routes, route, next)) {
        cacheDir = httpGetDir(route, "CACHE");
        if (cacheDir) {
            if (mprPathExists(cacheDir, X_OK)) {
                files = mprGetPathFiles(cacheDir, MPR_PATH_RELATIVE);
                if (mprGetListLength(files) > 0) {
                    qtrace("Clean", "%s", mprGetRelPath(cacheDir, 0));
                }
                for (nextFile = 0; (dp = mprGetNextItem(files, &nextFile)) != 0; ) {
                    path = mprJoinPath(cacheDir, dp->name);
                    if (mprPathExists(path, R_OK)) {
                        qtrace("Clean", "%s", mprGetRelPath(path, 0));
                        mprDeletePath(path);
                    }
                }
            }
        }
    }
    qtrace("Clean", "Complete");
}


static void generate(int argc, char **argv)
{
    char    *kind;

    if (app->error) {
        return;
    }
    if (argc < 1) {
        usageError();
        return;
    }
    kind = argv[0];

    if (smatch(kind, "appweb") || smatch(kind, "appweb.conf")) {
        generateItem("appweb");

    } else if (smatch(kind, "controller")) {
        generateController(argc - 1, &argv[1]);

    } else if (smatch(kind, "migration")) {
        generateMigration(argc - 1, &argv[1]);

    } else if (smatch(kind, "module")) {
        generateItem(kind);

    } else if (smatch(kind, "scaffold")) {
        generateScaffold(argc - 1, &argv[1]);

    } else if (smatch(kind, "table")) {
        generateTable(argc - 1, &argv[1]);

    } else {
        fatal("Unknown generation kind \"%s\"", kind);
    }
    if (!app->error) {
        qtrace("Generate", "Complete");
    }
}


static cchar *getJson(MprJson *config, cchar *key, cchar *defaultValue)
{
    cchar       *value;

    if (!config) {
        return defaultValue;
    }
    if ((value = mprGetJson(config, key)) != 0) {
        return value;
    }
    return defaultValue;
}


static void editValue(int argc, char **argv)
{
    cchar   *key, *value;
    int     i;

    if (argc < 1) {
        usageError();
        return;
    }
    for (i = 0; i < argc; i++) {
        key = ssplit(sclone(argv[i]), "=", (char**) &value);
        if (value && *value) {
            setConfigValue(app->config, key, value);
            saveConfig(app->config, "esp.json", MPR_JSON_QUOTES);
        } else {
            value = getJson(app->config, key, 0);
            if (value) {
                printf("%s\n", value);
            } else {
                printf("undefined\n");
            }
        }
    }
}


static void init(int argc, char **argv)
{
    MprJson     *config;
    cchar       *data, *path;

    if (!mprPathExists("esp.json", R_OK)) {
        trace("Create", "esp.json");
        config = mprParseJson(sfmt("{" \
            "http: {" \
                "server: {" \
                    "listen: [" \
                        "'http://127.0.0.1:4000'" \
                    "]," \
                "}," \
                "ssl: {" \
                    "certificate: ''," \
                    "key: ''" \
                "}" \
            "}" \
        "}"));
        if (config == 0) {
            fail("Cannot parse config");
            return;
        }
        path = mprJoinPath(app->route ? app->route->home : ".", "esp.json");
        if ((data = mprJsonToString(config, MPR_JSON_PRETTY)) == 0) {
            fail("Cannot save %s", path);
        }
        data = sjoin("/*\n" \
            "   esp.json - ESP Application configuration file\n\n" \
            "   See https://github.com/embedthis/esp/tree/dev/samples/config for a commented sample.\n" \
            " */\n", data, NULL);
        if (mprWritePathContents(path, data, -1, 0644) < 0) {
            fail("Cannot save %s", path);
        }
    }
    /*
        package.json
     */
    if (!mprPathExists("package.json", R_OK)) {
        trace("Create", "package.json");
        config = mprParseJson(sfmt("{" \
            "name: '%s'," \
            "title: '%s'," \
            "description: '%s'," \
            "version: '%s'," \
            "dependencies: {}," \
        "}", app->name, app->title, app->description, app->version));
        if (config == 0) {
            fail("Cannot parse config");
            return;
        }
        path = mprJoinPath(app->route ? app->route->home : ".", "package.json");
        if ((data = mprJsonToString(config, MPR_JSON_PRETTY | MPR_JSON_QUOTES)) == 0) {
            fail("Cannot save %s", path);
        }
        if (mprWritePathContents(path, data, -1, 0644) < 0) {
            fail("Cannot save %s", path);
        }
    }
    /*
        Initially make the directory and do not consult httpGetDir because httpSetDir tests if "dist" exists.
     */
    mprMakeDir("dist", 0755, -1, -1, 1);
}


/*
    esp migrate [forward|backward|NNN]
 */
static void migrate(int argc, char **argv)
{
    MprModule   *mp;
    MprDirEntry *dp;
    Edi         *edi;
    EdiRec      *mig;
    HttpRoute   *route;
    cchar       *command, *file;
    uint64      seq, targetSeq, lastMigration, v;
    int         next, onlyOne, backward, found, i, rc;

    if (app->error) {
        return;
    }
    route = app->route;
    onlyOne = backward = 0;
    targetSeq = 0;
    lastMigration = 0;
    command = 0;

    if ((edi = app->eroute->edi) == 0) {
        fail("Database not defined");
        return;
    }
    if (app->rebuild) {
        ediClose(edi);
        mprDeletePath(edi->path);
        if ((app->eroute->edi = ediOpen(edi->path, edi->provider->name, edi->flags | EDI_CREATE)) == 0) {
            fail("Cannot open database %s", edi->path);
            return;
        }
    }
    /*
        Each database has a _EspMigrations table which has a record for each migration applied
     */
    if ((app->migrations = ediReadTable(edi, ESP_MIGRATIONS)) == 0) {
        rc = ediAddTable(edi, ESP_MIGRATIONS);
        rc += ediAddColumn(edi, ESP_MIGRATIONS, "id", EDI_TYPE_INT, EDI_AUTO_INC | EDI_INDEX | EDI_KEY);
        rc += ediAddColumn(edi, ESP_MIGRATIONS, "version", EDI_TYPE_STRING, 0);
        if (rc < 0) {
            fail("Cannot add migration");
            return;
        }
        app->migrations = ediReadTable(edi, ESP_MIGRATIONS);
    }
    if (app->migrations->nrecords > 0) {
        mig = app->migrations->records[app->migrations->nrecords - 1];
        lastMigration = stoi(ediGetFieldValue(mig, "version"));
    }
    app->files = mprGetPathFiles(app->migDir, MPR_PATH_NO_DIRS);
    mprSortList(app->files, (MprSortProc) (backward ? reverseSortFiles : sortFiles), 0);

    if (argc > 0) {
        command = argv[0];
        if (sstarts(command, "forw")) {
            onlyOne = 1;
        } else if (sstarts(command, "back")) {
            onlyOne = 1;
            backward = 1;
        } else if (*command) {
            /* Find the specified migration, may be a pure sequence number or a filename */
            for (ITERATE_ITEMS(app->files, dp, next)) {
                file = dp->name;
                app->base = mprGetPathBase(file);
                if (smatch(app->base, command)) {
                    targetSeq = stoi(app->base);
                    break;
                } else {
                    if (stoi(app->base) == stoi(command)) {
                        targetSeq = stoi(app->base);
                        break;
                    }
                }
            }
            if (! targetSeq) {
                fail("Cannot find target migration: %s", command);
                return;
            }
            if (lastMigration && targetSeq < lastMigration) {
                backward = 1;
            }
        }
    }

    found = 0;
    for (ITERATE_ITEMS(app->files, dp, next)) {
        file = dp->name;
        app->base = mprGetPathBase(file);
        if (!smatch(mprGetPathExt(app->base), "c") || !isdigit((uchar) *app->base)) {
            continue;
        }
        seq = stoi(app->base);
        if (seq <= 0) {
            continue;
        }
        found = 0;
        mig = 0;
        for (i = 0; i < app->migrations->nrecords; i++) {
            mig = app->migrations->records[i];
            v = stoi(ediGetFieldValue(mig, "version"));
            if (v == seq) {
                found = 1;
                break;
            }
        }
        if (backward) {
            found = !found;
        }
        if (!found) {
            /*
                WARNING: GC may occur while compiling
             */
            compileFile(route, file, ESP_MIGRATION);
            if (app->error) {
                return;
            }
            if ((app->entry = scontains(app->base, "_")) != 0) {
                app->entry = mprTrimPathExt(&app->entry[1]);
            } else {
                app->entry = mprTrimPathExt(app->base);
            }
            app->entry = sfmt("esp_migration_%s", app->entry);
            if ((mp = mprCreateModule(file, app->module, app->entry, edi)) == 0) {
                return;
            }
            if (mprLoadModule(mp) < 0) {
                return;
            }
            if (backward) {
                qtrace("Migrate", "Reverse %s", app->base);
                if (edi->back(edi) < 0) {
                    fail("Cannot reverse migration");
                    return;
                }
            } else {
                qtrace("Migrate", "Apply %s ", app->base);
                if (edi->forw(edi) < 0) {
                    fail("Cannot apply migration");
                    return;
                }
            }
            if (backward) {
                assert(mig);
                ediRemoveRec(edi, ESP_MIGRATIONS, ediGetFieldValue(mig, "id"));
            } else {
                mig = ediCreateRec(edi, ESP_MIGRATIONS);
                ediSetField(mig, "version", itos(seq));
                if (ediUpdateRec(edi, mig) < 0) {
                    fail("Cannot update migrations table");
                    return;
                }
            }
            mprUnloadModule(mp);
            if (onlyOne) {
                return;
            }
        }
        if (targetSeq == seq) {
            return;
        }
    }
    if (!onlyOne) {
        trace("Migrate", "All migrations %s", backward ? "reversed" : "applied");
    }
    app->migrations = 0;
}


/*
    esp role add ROLE ABILITIES
    esp role remove ROLE
 */
static void role(int argc, char **argv)
{
    HttpAuth    *auth;
    HttpRole    *role;
    MprJson     *abilities;
    MprBuf      *buf;
    MprKey      *kp;
    cchar       *cmd, *def, *key, *rolename;

    if ((auth = app->route->auth) == 0) {
        fail("Authentication not configured in %s", "esp.json");
        return;
    }
    if (argc < 2) {
        usageError();
        return;
    }
    cmd = argv[0];
    rolename = argv[1];

    if (smatch(cmd, "remove")) {
        key = sfmt("http.auth.roles.%s", rolename);
        if (mprRemoveJson(app->config, key) < 0) {
            fail("Cannot remove %s", key);
            return;
        }
        if (!app->noupdate) {
            saveConfig(app->config, "esp.json", 0);
            trace("Remove", "Role %s", rolename);
        }
        return;

    } else if (smatch(cmd, "add")) {
        if (smatch(cmd, "add")) {
            def = sfmt("[%s]", sjoinArgs(argc - 2, (cchar**) &argv[2], ","));
            abilities = mprParseJson(def);
            key = sfmt("http.auth.roles.%s", rolename);
            if (mprSetJsonObj(app->config, key, abilities) < 0) {
                fail("Cannot update %s", key);
                return;
            }
            saveConfig(app->config, "esp.json", 0);
            if (!app->noupdate) {
                trace("Update", "Role %s", rolename);
            }
        }
        if (app->show) {
            trace("Info", "%s %s", rolename, sjoinArgs(argc - 2, (cchar**) &argv[3], " "));
        }
    } else if (smatch(cmd, "show")) {
        if ((role = httpLookupRole(app->route->auth, rolename)) == 0) {
            fail("Cannot find role %s", rolename);
            return;
        }
        buf = mprCreateBuf(0, 0);
        for (ITERATE_KEYS(role->abilities, kp)) {
            mprPutToBuf(buf, "%s ", kp->key);
        }
        trace("Info", "%s %s", role->name, mprBufToString(buf));
    }
}


static void setMode(cchar *mode)
{
    int     quiet;

    setConfigValue(app->package, "pak.mode", mode);
    saveConfig(app->package, "package.json", MPR_JSON_QUOTES);
    quiet = app->quiet;
    app->quiet = 1;
    clean(0, NULL);
    app->quiet = quiet;
}


/*
    Edit a key value in the esp.json
 */
static void setConfigValue(MprJson *config, cchar *key, cchar *value)
{
    qtrace("Set", sfmt("Key \"%s\" to \"%s\"", key, value));
    if (mprSetJson(config, key, value, 0) < 0) {
        fail("Cannot update %s with %s", key, value);
        return;
    }
}


/*
    esp serve [ip]:[port] ...
 */
static void serve(int argc, char **argv)
{
    HttpEndpoint    *endpoint;
    cchar           *address;
    char            *ip;
    int             i, port;

    if (app->error) {
        return;
    }
    if (app->show) {
        httpLogRoutes(app->host, app->show > 1);
    }
    if (argc == 0) {
        if (http->endpoints->length == 0) {
            if ((endpoint = httpCreateEndpoint("127.0.0.1", 4000, NULL)) == 0) {
                fail("Cannot create endpoint for 127.0.0.1:%d", 4000);
                return;
            }
            httpAddHostToEndpoints(app->host);
        }
    } else for (i = 0; i < argc; i++) {
        address = argv[i++];
        mprParseSocketAddress(address, &ip, &port, NULL, 80);
        if ((endpoint = httpCreateEndpoint(ip, port, NULL)) == 0) {
            fail("Cannot create endpoint for %s:%d", ip, port);
            return;
        }
        httpAddHostToEndpoints(app->host);
    }
    if (mprGetListLength(HTTP->endpoints) == 0) {
        fail("No configured listening endpoints");
        return;
    }
    httpSetInfoLevel(1);
    if (httpStartEndpoints() < 0) {
        mprLog("", 0, "Cannot start HTTP service, exiting.");
        return;
    }
    /*
        Events thread will service requests
     */
    mprYield(MPR_YIELD_STICKY);
    while (!mprIsStopping()) {
        mprSuspendThread(-1);
    }
    mprResetYield();
    mprLog("", 1, "Stopping ...");
}


/*
    esp user add NAME PASSWORD ROLES
    esp user compute NAME PASSWORD ROLES
    esp user remove NAME
    esp user show NAME
 */
static void user(int argc, char **argv)
{
    HttpAuth    *auth;
    HttpUser    *user;
    MprJson     *credentials;
    char        *password;
    cchar       *cmd, *def, *key, *username, *encodedPassword, *roles;

    if ((auth = app->route->auth) == 0) {
        fail("Authentication not configured in %s", "esp.json");
        return;
    }
    if (argc < 2) {
        usageError();
        return;
    }
    cmd = argv[0];
    username = argv[1];

    if (smatch(cmd, "remove")) {
        if (httpRemoveUser(app->route->auth, username) < 0) {
            fail("Cannot remove user %s", username);
            return;
        }
        key = sfmt("http.auth.users.%s", username);
        if (mprRemoveJson(app->config, key) < 0) {
            fail("Cannot remove %s", key);
            return;
        }
        if (!app->noupdate) {
            saveConfig(app->config, "esp.json", 0);
            trace("Remove", "User %s", username);
        }
        return;

    } else if (smatch(cmd, "add") || smatch(cmd, "compute")) {
        if (argc < 3) {
            usageError();
            return;
        }
        password = argv[2];
        if (smatch(password, "-")) {
            password = getPassword();
        }
        if (auth->realm == 0 || *auth->realm == '\0') {
            fail("An authentication realm has not been defined. Define a \"http.auth.realm\" value.");
            return;
        }
        if (smatch(app->cipher, "md5")) {
            encodedPassword = mprGetMD5(sfmt("%s:%s:%s", username, auth->realm, password));
        } else {
            /* This uses the more secure blowfish cipher */
            encodedPassword = mprMakePassword(sfmt("%s:%s:%s", username, auth->realm, password), 16, 128);
        }
        serase(password);
        if (smatch(cmd, "add")) {
            def = sfmt("{password:'%s',roles:[%s]}", encodedPassword, sjoinArgs(argc - 3, (cchar**) &argv[3], ","));
            credentials = mprParseJson(def);
            key = sfmt("http.auth.users.%s", username);
            if (mprSetJsonObj(app->config, key, credentials) < 0) {
                fail("Cannot update %s", key);
                return;
            }
            saveConfig(app->config, "esp.json", 0);
            if (!app->noupdate) {
                trace("Update", "User %s", username);
            }
        }
        if (smatch(cmd, "compute") || app->show) {
            trace("Info", "%s %s %s", username, encodedPassword, sjoinArgs(argc - 3, (cchar**) &argv[3], " "));
        }

    } else if (smatch(cmd, "show")) {
        if ((user = httpLookupUser(app->route->auth, username)) == 0) {
            fail("Cannot find user %s", username);
            return;
        }
        roles = sreplace(user->roles, ",", "");
        roles = sreplace(roles, "  ", " ");
        roles = strim(roles, " ", 0);
        trace("Info", "%s %s %s", user->name, user->password, roles);
    }
}


static MprHash *getTargets(int argc, char **argv)
{
    MprHash     *targets;
    int         i;

    targets = mprCreateHash(0, MPR_HASH_STABLE);
    for (i = 0; i < argc; i++) {
        mprAddKey(targets, mprGetAbsPath(argv[i]), NULL);
    }
    return targets;
}


static bool similarRoute(HttpRoute *r1, HttpRoute *r2)
{
    if (!smatch(r1->documents, r2->documents)) {
        return 0;
    }
    if (!smatch(r1->home, r2->home)) {
        return 0;
    }
    if (r1->vars != r2->vars) {
        return 0;
    }
    if (scontains(r1->sourceName, "${") == 0 && scontains(r2->sourceName, "${") == 0) {
        if (r1->sourceName || r2->sourceName) {
            return smatch(r1->sourceName, r2->sourceName);
        }
    }
    return 1;
}


static MprList *getRoutes()
{
    HttpRoute   *route, *parent, *rp;
    EspRoute    *eroute;
    MprList     *routes;
    MprKey      *kp;
    cchar       *filterRoutePattern, *filterRoutePrefix;
    int         prev, nextRoute;

    if (app->error) {
        return 0;
    }
    if ((app->host = mprGetFirstItem(http->hosts)) == 0) {
        fail("Cannot find default host");
        return 0;
    }
    filterRoutePattern = app->filterRoutePattern;
    filterRoutePrefix = app->filterRoutePrefix ? app->filterRoutePrefix : 0;
    routes = mprCreateList(0, MPR_LIST_STABLE);

    /*
        Filter ESP routes. Go in reverse order to locate outermost routes first.
     */
    for (prev = -1; (route = mprGetPrevItem(app->host->routes, &prev)) != 0; ) {
        if ((eroute = route->eroute) == 0 || !eroute->compile) {
            /* No ESP configuration for compiling */
            mprLog("", 6, "Skip route name %s - no esp configuration", route->pattern);
            continue;
        }
        if (filterRoutePattern) {
            mprLog("", 6, "Check route name %s, prefix %s with %s", route->pattern, route->startWith, filterRoutePattern);
            if (!smatch(filterRoutePattern, route->pattern)) {
                continue;
            }
        } else if (filterRoutePrefix) {
            mprLog("", 6, "Check route name %s, prefix %s with %s", route->pattern, route->startWith, filterRoutePrefix);
            if (!smatch(filterRoutePrefix, route->prefix) && !smatch(filterRoutePrefix, route->startWith)) {
                continue;
            }
        } else {
            mprLog("", 6, "Check route name %s, prefix %s", route->pattern, route->startWith);
        }
        if (!requiredRoute(route)) {
            mprLog("", 6, "Skip route %s not required for selected targets", route->pattern);
            continue;
        }
        /*
            Check for routes with duplicate documents and home directories
         */
        rp = 0;
        for (ITERATE_ITEMS(routes, rp, nextRoute)) {
            if (similarRoute(route, rp)) {
                mprLog("", 6, "Skip route %s because of prior similar route", route->pattern);
                route = 0;
                break;
            }
        }
        if (!route) {
            continue;
        }
        parent = route->parent;
        if (parent && parent->eroute &&
            ((EspRoute*) parent->eroute)->compile && smatch(route->documents, parent->documents) && parent->startWith) {
            /*
                Use the parent instead if it has the same directory and is not the default route
                This is for MVC apps with a prefix of "/" and a directory the same as the default route.
             */
            continue;
        }
        if (route && mprLookupItem(routes, route) < 0) {
            mprLog("", 6, "Using route name: %s documents:%s prefix: %s", route->pattern, route->documents,
                route->startWith);
            mprAddItem(routes, route);
        }
    }
    if (mprGetListLength(routes) == 0) {
        if (filterRoutePattern) {
            fail("Cannot find usable ESP configuration for route %s", filterRoutePattern);
        } else if (filterRoutePrefix) {
            fail("Cannot find usable ESP configuration for route prefix %s", filterRoutePrefix);
        } else {
            kp = mprGetFirstKey(app->targets);
            if (kp) {
                fail("Cannot find usable ESP configuration for %s", kp->key);
            } else {
                fail("Cannot find usable ESP configuration");
            }
        }
        return 0;
    }
    /*
        Check we have a route for all targets
     */
    for (ITERATE_KEYS(app->targets, kp)) {
        if (!kp->type) {
            fail("Cannot find a usable route for %s", kp->key);
            return 0;
        }
    }
    if ((app->route = mprGetFirstItem(routes)) == 0) {
        if (app->require & REQ_ROUTES) {
            fail("Cannot find a suitable route");
        }
        return 0;
    }
    return routes;
}


static int runEspCommand(HttpRoute *route, cchar *command, cchar *csource, cchar *module)
{
    MprCmd      *cmd;
    MprList     *elist;
    MprKey      *var;
    EspRoute    *eroute;
    cchar       **env;
    char        *err, *out;

    eroute = route->eroute;
    if ((app->command = espExpandCommand(route, command, csource, module)) == 0) {
        fail("Missing EspCompile directive for %s", csource);
        return MPR_ERR_CANT_READ;
    }
    mprLog("", 4, "command: %s", app->command);
    if (eroute->env) {
        elist = mprCreateList(0, MPR_LIST_STABLE);
        for (ITERATE_KEYS(eroute->env, var)) {
            mprAddItem(elist, sfmt("%s=%s", var->key, (char*) var->data));
        }
        mprAddNullItem(elist);
        env = (cchar**) &elist->items[0];
    } else {
        env = 0;
    }
    cmd = mprCreateCmd(0);
    if (eroute->searchPath) {
        mprSetCmdSearchPath(cmd, eroute->searchPath);
    }
    if (app->show) {
        trace("Run", app->command);
    }
    //  WARNING: GC will run here
    if (mprRunCmd(cmd, app->command, env, NULL, &out, &err, -1, 0) != 0) {
        if (err == 0 || *err == '\0') {
            /* Windows puts errors to stdout Ugh! */
            err = out;
        }
        fail("Cannot run command: \n%s\nError: %s", app->command, err);
        mprDestroyCmd(cmd);
        return MPR_ERR_CANT_COMPLETE;
    }
    if (out && *out) {
#if ME_WIN_LIKE
        if (!scontains(out, "Creating library ")) {
            if (!smatch(mprGetPathBase(csource), strim(out, " \t\r\n", MPR_TRIM_BOTH))) {
                mprLog("", 0, "%s", out);
            }
        }
#else
        mprLog("", 0, "%s", out);
#endif
    }
    if (err && *err) {
        mprLog("", 0, "%s", err);
    }
    mprDestroyCmd(cmd);
    return 0;
}


static void compileFile(HttpRoute *route, cchar *source, int kind)
{
    EspRoute    *eroute;
    cchar       *canonical, *defaultLayout, *page, *layout, *data, *prefix, *lpath, *appName, *cacheDir, *layoutsDir;
    char        *err, *quote, *script;
    ssize       len;
    int         recompile;

    if (app->error) {
        return;
    }
    if (app->built) {
        if (mprLookupKey(app->built, source)) {
            return;
        }
        mprAddKey(app->built, source, source);
    }
    cacheDir = httpGetDir(route, "CACHE");
    eroute = route->eroute;
    defaultLayout = 0;
    if (kind == ESP_SRC) {
        prefix = "app_";
    } else if (kind == ESP_CONTROlLER) {
        prefix = "controller_";
    } else if (kind == ESP_MIGRATION) {
        prefix = "migration_";
    } else {
        prefix = "view_";
    }
    canonical = mprGetPortablePath(mprGetRelPath(source, route->home));
    appName = eroute->appName ? eroute->appName : route->host->name;
    app->cacheName = mprGetMD5WithPrefix(sfmt("%s:%s", appName, canonical), -1, prefix);
    app->module = mprNormalizePath(sfmt("%s/%s%s", cacheDir, app->cacheName, ME_SHOBJ));
    if ((layoutsDir = httpGetDir(route, "LAYOUTS")) != 0) {
        defaultLayout = mprJoinPath(layoutsDir, "default.esp");
    } else {
        defaultLayout = 0;
    }
    mprMakeDir(cacheDir, 0755, -1, -1, 1);

    if (app->combine) {
        why(source, "\"combine\" mode requires complete rebuild");

    } else if (app->rebuild) {
        why(source, "due to requested rebuild");

    } else if (!espModuleIsStale(source, app->module, &recompile)) {
        if (kind & (ESP_PAGE | ESP_VIEW)) {
            if ((data = mprReadPathContents(source, &len)) == 0) {
                fail("Cannot read %s", source);
                return;
            }
            if ((lpath = scontains(data, "@ layout \"")) != 0) {
                lpath = strim(&lpath[10], " ", MPR_TRIM_BOTH);
                if ((quote = schr(lpath, '"')) != 0) {
                    *quote = '\0';
                }
                layout = (layoutsDir && *lpath) ? mprJoinPath(layoutsDir, lpath) : 0;
            } else {
                layout = defaultLayout;
            }
            if (!layout || !espModuleIsStale(layout, app->module, &recompile)) {
                why(source, "is up to date");
                return;
            }
        } else {
            why(source, "is up to date");
            return;
        }
    } else if (mprPathExists(app->module, R_OK)) {
        why(source, "has been modified");
    } else {
        why(source, "%s is missing", app->module);
    }
    if (app->combineFile) {
        trace("Catenate", "%s", mprGetRelPath(source, 0));
        mprWriteFileFmt(app->combineFile, "/*\n    Source from %s\n */\n", source);
    }
    if (kind & (ESP_CONTROlLER | ESP_MIGRATION | ESP_SRC)) {
        app->csource = source;
        if (app->combineFile) {
            if ((data = mprReadPathContents(source, &len)) == 0) {
                fail("Cannot read %s", source);
                return;
            }
            if (mprWriteFile(app->combineFile, data, slen(data)) < 0) {
                fail("Cannot write compiled script file %s", app->combineFile->path);
                return;
            }
            mprWriteFileFmt(app->combineFile, "\n\n");
            if (kind & ESP_SRC) {
                mprAddItem(app->combineItems, sfmt("esp_app_%s", eroute->appName));
            } else if (eroute->appName && *eroute->appName) {
                mprAddItem(app->combineItems,
                    sfmt("esp_controller_%s_%s", eroute->appName, mprTrimPathExt(mprGetPathBase(source))));
            } else {
                mprAddItem(app->combineItems, sfmt("esp_controller_%s", mprTrimPathExt(mprGetPathBase(source))));
            }
        }
    }
    if (kind & (ESP_PAGE | ESP_VIEW)) {
        if ((page = mprReadPathContents(source, &len)) == 0) {
            fail("Cannot read %s", source);
            return;
        }
        /* No yield here */
        if ((script = espBuildScript(route, page, source, app->cacheName, defaultLayout, NULL, &err)) == 0) {
            fail("Cannot build %s, error %s", source, err);
            return;
        }
        len = slen(script);
        if (app->combineFile) {
            if (mprWriteFile(app->combineFile, script, len) < 0) {
                fail("Cannot write compiled script file %s", app->combineFile->path);
                return;
            }
            mprWriteFileFmt(app->combineFile, "\n\n");
            mprAddItem(app->combineItems, sfmt("esp_%s", app->cacheName));

        } else {
            app->csource = mprJoinPathExt(mprTrimPathExt(app->module), ".c");
            trace("Parse", "%s", mprGetRelPath(source, 0));
            mprMakeDir(cacheDir, 0755, -1, -1, 1);
            if (mprWritePathContents(app->csource, script, len, 0664) < 0) {
                fail("Cannot write compiled script file %s", app->csource);
                return;
            }
        }
    }
    if (!app->combineFile) {
        /*
            WARNING: GC yield here
         */
        trace("Compile", "%s", mprGetRelPath(app->csource, 0));
        if (!eroute->compile) {
            fail("Missing EspCompile directive for %s", app->csource);
            return;
        }
        if (runEspCommand(route, eroute->compile, app->csource, app->module) < 0) {
            return;
        }
        if (eroute->link) {
            vtrace("Link", "%s", mprGetRelPath(mprTrimPathExt(app->module), NULL));
            if (runEspCommand(route, eroute->link, app->csource, app->module) < 0) {
                return;
            }
#if !(ME_DEBUG && MACOSX)
            /*
                MAC needs the object for debug information
             */
            mprDeletePath(mprJoinPathExt(mprTrimPathExt(app->module), ME_OBJ));
#endif
        }
        if (!route->keepSource && !app->keep && (kind & (ESP_VIEW | ESP_PAGE))) {
            mprDeletePath(app->csource);
        }
    }
}


/*
    esp compile [controller_names | page_names | paths]
 */
static void compile(int argc, char **argv)
{
    HttpRoute   *route;
    MprKey      *kp;
    int         next;

    if (app->error) {
        return;
    }
    app->combine = app->eroute->combine;
    vtrace("Info", "Compiling in %s mode", app->combine ? "combine" : "discrete");

    app->built = mprCreateHash(0, MPR_HASH_STABLE | MPR_HASH_STATIC_VALUES);
    for (ITERATE_ITEMS(app->routes, route, next)) {
        if (app->combine) {
            compileCombined(route);
            break;
        } else {
            compileItems(route);
        }
    }
    app->built = 0;

    /*
        Check we have compiled all targets
     */
    for (ITERATE_KEYS(app->targets, kp)) {
        if (!kp->type) {
            fail("Cannot find target %s to compile", kp->key);
        }
    }
}


/*
    Select a route that is responsible for a target
 */
static bool requiredRoute(HttpRoute *route)
{
    MprKey      *kp;
    cchar       *source;

    if (app->targets == 0 || mprGetHashLength(app->targets) == 0) {
        return 1;
    }
    for (ITERATE_KEYS(app->targets, kp)) {
        if (mprIsPathContained(kp->key, route->documents)) {
            kp->type = ESP_FOUND_TARGET;
            return 1;
        }
        if (route->sourceName) {
            source = mprJoinPath(httpGetDir(route, "CONTROLLERS"), route->sourceName);
            if (mprIsPathContained(kp->key, source)) {
                kp->type = ESP_FOUND_TARGET;
                return 1;
            }
        }
    }
    return 0;
}


/*
    Select a resource that matches specified targets
 */
static bool selectResource(cchar *path, cchar *kind)
{
    MprKey  *kp;
    cchar   *ext;

    ext = mprGetPathExt(path);
    if (kind && !smatch(ext, kind)) {
        return 0;
    }
    if (app->targets == 0 || mprGetHashLength(app->targets) == 0) {
        return 1;
    }
    for (ITERATE_KEYS(app->targets, kp)) {
        if (mprIsPathContained(kp->key, path)) {
            kp->type = ESP_FOUND_TARGET;
            return 1;
        }
    }
    return 0;
}


static bool selectView(HttpRoute *route, cchar *path)
{
    MprKey      *kp;
    MprJson     *extensions, *ext;
    int         index;

    if ((extensions = mprGetJsonObj(route->config, "http.pipeline.handlers.espHandler")) != 0) {
        for (ITERATE_JSON(extensions, ext, index)) {
            if (smatch(mprGetPathExt(path), ext->value)) {
                if (app->targets == 0 || mprGetHashLength(app->targets) == 0) {
                    return 1;
                }
                for (ITERATE_KEYS(app->targets, kp)) {
                    if (mprIsPathContained(kp->key, path)) {
                        kp->type = ESP_FOUND_TARGET;
                        return 1;
                    }
                }
            }
        }
    } else {
        if (smatch(mprGetPathExt(path), "esp")) {
            if (app->targets == 0 || mprGetHashLength(app->targets) == 0) {
                return 1;
            }
            for (ITERATE_KEYS(app->targets, kp)) {
                if (mprIsPathContained(kp->key, path)) {
                    kp->type = ESP_FOUND_TARGET;
                    return 1;
                }
            }
        }
    }
    return 0;
}

/*
    Compile all the items relevant to a route
 */
static void compileItems(HttpRoute *route)
{
    MprDirEntry *dp;
    cchar       *dir, *path;
    int         found, next;

    found = 0;
    vtrace("Info", "Compile items for route \"%s\"", route->pattern);

    if ((dir = httpGetDir(route, "CONTROLLERS")) != 0 && !smatch(dir, ".")) {
        app->files = mprGetPathFiles(dir, MPR_PATH_DESCEND);
        for (next = 0; (dp = mprGetNextItem(app->files, &next)) != 0 && !app->error; ) {
            path = dp->name;
            if (selectResource(path, "c")) {
                compileFile(route, path, ESP_CONTROlLER);
            }
            found++;
        }
    }
#if DEPRECATED || 1
    if ((dir = httpGetDir(route, "VIEWS")) != 0 && !smatch(dir, ".")) {
        app->files = mprGetPathFiles(dir, MPR_PATH_DESCEND);
        for (next = 0; (dp = mprGetNextItem(app->files, &next)) != 0 && !app->error; ) {
            path = dp->name;
            if (sstarts(path, httpGetDir(route, "LAYOUTS"))) {
                continue;
            }
            if (selectResource(path, "esp")) {
                compileFile(route, path, ESP_VIEW);
            }
            found++;
        }
    }
#endif
    if ((dir = mprJoinPath(httpGetDir(route, "SRC"), "app.c")) != 0 && !smatch(dir, ".")) {
        if (mprPathExists(dir, R_OK) && selectResource(dir, "c")) {
            compileFile(route, dir, ESP_SRC);
            found++;
        }
    }
    app->files = mprGetPathFiles(route->documents, MPR_PATH_DESCEND);
    for (next = 0; (dp = mprGetNextItem(app->files, &next)) != 0 && !app->error; ) {
        path = dp->name;
        if (selectView(route, path)) {
            compileFile(route, path, ESP_PAGE);
        }
        found++;
    }
    /*
        Stand-alone controllers
     */
    if (route->sourceName) {
        path = mprJoinPath(route->home, route->sourceName);
        if (mprPathExists(path, R_OK)) {
            compileFile(route, path, ESP_CONTROlLER);
            found++;
        }
    }
    if (!found) {
        trace("Info", "No files to compile for route \"%s\"", route->pattern);
    }
}


/*
    Compile all the items for a route into a combine (single) output file
 */
static void compileCombined(HttpRoute *route)
{
    MprDirEntry     *dp;
    MprKey          *kp;
    EspRoute        *eroute;
    MprJson         *extensions, *ext;
    cchar           *item, *name;
    char            *path, *line;
    int             next, kind, index;

    eroute = route->eroute;
    name = app->name ? app->name : mprGetPathBase(route->documents);
    app->build = mprCreateHash(0, MPR_HASH_STABLE | MPR_HASH_STATIC_VALUES);

    /*
        Combined ... Catenate all source
     */
    app->combineItems = mprCreateList(-1, MPR_LIST_STABLE);
    app->combinePath = mprJoinPath(httpGetDir(route, "CACHE"), sjoin(name, ".c", NULL));

    path = mprJoinPath(httpGetDir(app->route, "SRC"), "app.c");
    if (mprPathExists(path, R_OK)) {
        mprAddKey(app->build, path, "src");
    }
    app->files = mprGetPathFiles(httpGetDir(route, "CONTROLLERS"), MPR_PATH_DESCEND);
    for (next = 0; (dp = mprGetNextItem(app->files, &next)) != 0 && !app->error; ) {
        path = dp->name;
        if (smatch(mprGetPathExt(path), "c")) {
            mprAddKey(app->build, path, "controller");
        }
    }
    app->files = mprGetPathFiles(route->documents, MPR_PATH_DESCEND);
    for (next = 0; (dp = mprGetNextItem(app->files, &next)) != 0 && !app->error; ) {
        path = dp->name;
        if ((extensions = mprGetJsonObj(route->config, "http.pipeline.handlers.espHandler")) != 0) {
            for (ITERATE_JSON(extensions, ext, index)) {
                if (smatch(mprGetPathExt(path), ext->value)) {
                    mprAddKey(app->build, path, "page");
                    break;
                }
            }
        } else if (smatch(mprGetPathExt(path), "esp")) {
            mprAddKey(app->build, path, "page");
        }
    }
    if (mprGetHashLength(app->build) > 0) {
        mprMakeDir(httpGetDir(route, "CACHE"), 0755, -1, -1, 1);
        if ((app->combineFile = mprOpenFile(app->combinePath, O_WRONLY | O_TRUNC | O_CREAT | O_BINARY, 0664)) == 0) {
            fail("Cannot open %s", app->combinePath);
            return;
        }
        mprWriteFileFmt(app->combineFile, "/*\n    Combined compilation of %s\n */\n\n", name);
        mprWriteFileFmt(app->combineFile, "#include \"esp.h\"\n\n");

        for (ITERATE_KEY_DATA(app->build, kp, item)) {
            if (smatch(item, "src")) {
                kind = ESP_SRC;
            } else if (smatch(item, "controller")) {
                kind = ESP_CONTROlLER;
            } else if (smatch(item, "page")) {
                kind = ESP_VIEW;
            } else {
                kind = ESP_PAGE;
            }
            compileFile(route, kp->key, kind);
        }
#if DEPRECATE
        if (app->slink) {
            mprAddItem(app->slink, route);
        }
#endif
        mprWriteFileFmt(app->combineFile,
            "\nESP_EXPORT int esp_app_%s_combine(HttpRoute *route, MprModule *module) {\n", name);
        for (next = 0; (line = mprGetNextItem(app->combineItems, &next)) != 0; ) {
            mprWriteFileFmt(app->combineFile, "    %s(route, module);\n", line);
        }
        mprWriteFileFmt(app->combineFile, "    return 0;\n}\n");
        mprCloseFile(app->combineFile);

        app->module = mprNormalizePath(sfmt("%s/%s%s", httpGetDir(route, "CACHE"), name, ME_SHOBJ));
        trace("Compile", "%s", name);
        if (runEspCommand(route, eroute->compile, app->combinePath, app->module) < 0) {
            return;
        }
        if (eroute->link) {
            trace("Link", "%s", mprGetRelPath(mprTrimPathExt(app->module), NULL));
            if (runEspCommand(route, eroute->link, app->combinePath, app->module) < 0) {
                return;
            }
        }
    }
    app->combineItems = 0;
    app->combineFile = 0;
    app->combinePath = 0;
    app->build = 0;
}


static void generateItem(cchar *item)
{
    if (getJson(app->config, sfmt("esp.generate.%s", item), 0) == 0) {
        fail("No suitable package installed to generate %s", item);
        return;
    }
    genKey(item, 0, 0);
}


/*
    esp generate controller name [action [, action] ...]

    Generate a server-side controller.
 */
static void generateController(int argc, char **argv)
{
    MprHash     *tokens;
    cchar       *action, *actions, *defines;
    int         i;

    if (argc < 1) {
        usageError();
        return;
    }
    if (getJson(app->config, "esp.generate.controller", 0) == 0) {
        fail("No suitable package installed to generate controllers");
        return;
    }
    app->controller = sclone(argv[0]);
    defines = sclone("");
    actions = sclone("");
    for (i = 1; i < argc; i++) {
        action = argv[i];
        defines = sjoin(defines, sfmt("    espDefineAction(route, \"%s/%s\", %s);\n", app->controller,
            action, action), NULL);
        actions = sjoin(actions, sfmt("static void %s() {\n}\n\n", action), NULL);
    }
    tokens = makeTokens(0, mprDeserialize(sfmt("{ ACTIONS: '%s', DEFINE_ACTIONS: '%s' }", actions, defines)));
    genKey("controller", sfmt("%s/%s.c", httpGetDir(app->route, "CONTROLLERS"), app->controller), tokens);
}


/*
    esp migration description model [field:type [, field:type] ...]

    The description is used to name the migration
 */
static void generateMigration(int argc, char **argv)
{
    cchar       *name, *stem, *table;

    if (argc < 2) {
        fail("Bad migration command line");
    }
    table = app->table ? app->table : sclone(argv[1]);
    stem = sfmt("Migration %s", argv[0]);
    /*
        Migration name used in the filename and in the exported load symbol
     */
    name = sreplace(slower(stem), " ", "_");
    createMigration(name, table, stem, argc - 2, &argv[2]);
}


static void createMigration(cchar *name, cchar *table, cchar *comment, int fieldCount, char **fields)
{
    MprHash     *tokens;
    MprList     *files;
    MprDirEntry *dp;
    cchar       *seq, *forward, *backward, *data, *path, *def, *field, *tail, *typeDefine;
    char        *typeString;
    int         i, type, next;

    seq = sfmt("%s%d", mprGetDate("%Y%m%d%H%M%S"), nextMigration);
    forward = sfmt("    ediAddTable(db, \"%s\");\n", table);
    backward = sfmt("    ediRemoveTable(db, \"%s\");\n", table);

    def = sfmt("    ediAddColumn(db, \"%s\", \"id\", EDI_TYPE_INT, EDI_AUTO_INC | EDI_INDEX | EDI_KEY);\n", table);
    forward = sjoin(forward, def, NULL);

    for (i = 0; i < fieldCount; i++) {
        field = ssplit(sclone(fields[i]), ":", &typeString);
        if ((type = ediParseTypeString(typeString)) < 0) {
            fail("Unknown type '%s' for field '%s'", typeString, field);
            return;
        }
        if (smatch(field, "id")) {
            continue;
        }
        typeDefine = sfmt("EDI_TYPE_%s", supper(ediGetTypeString(type)));
        def = sfmt("    ediAddColumn(db, \"%s\", \"%s\", %s, 0);\n", table, field, typeDefine);
        forward = sjoin(forward, def, NULL);
    }
    tokens = mprDeserialize(sfmt("{ MIGRATION: '%s', TABLE: '%s', COMMENT: '%s', FORWARD: '%s', BACKWARD: '%s' }",
        name, table, comment, forward, backward));
    if ((data = getTemplate("migration", tokens)) == 0) {
        return;
    }
    makeEspDir(app->migDir);
    files = mprGetPathFiles(app->migDir, MPR_PATH_RELATIVE);
    tail = sfmt("%s.c", name);
    for (ITERATE_ITEMS(files, dp, next)) {
        if (sends(dp->name, tail)) {
            if (!app->force) {
                qtrace("Exists", "A migration with the same description already exists: %s", dp->name);
                return;
            }
            mprDeletePath(mprJoinPath(app->migDir, dp->name));
        }
    }
    path = sfmt("%s/%s_%s.c", app->migDir, seq, name);
    makeEspFile(path, data, 0);
}


static void generateScaffoldController(int argc, char **argv)
{
    cchar   *key;

    key = app->singleton ? "controllerSingleton" : "controller";
    genKey(key, sfmt("%s/%s.c", httpGetDir(app->route, "CONTROLLERS"), app->controller), 0);
}


static void generateClientController(int argc, char **argv)
{
    genKey("clientController", sfmt("%s/%s/%sControl.js", httpGetDir(app->route, "SOURCE"),
        app->controller, stitle(app->controller)), 0);
}


static void generateClientModel(int argc, char **argv)
{
    genKey("clientModel", sfmt("%s/%s/%s.js", httpGetDir(app->route, "SOURCE"), app->controller,
        stitle(app->controller)), 0);
}


/*
    Called with args: model [field:type [, field:type] ...]
 */
static void generateScaffoldMigration(int argc, char **argv)
{
    cchar       *comment;

    if (argc < 1) {
        fail("Bad migration command line");
    }
    comment = sfmt("Create Scaffold %s", stitle(app->controller));
    createMigration(sfmt("create_scaffold_%s", app->table), app->table, comment, argc - 1, &argv[1]);
}


/*
    esp generate table name [field:type [, field:type] ...]
 */
static void generateTable(int argc, char **argv)
{
    Edi         *edi;
    cchar       *field;
    char        *typeString;
    int         rc, i, type;

    app->table = app->table ? app->table : sclone(argv[0]);
    if ((edi = app->eroute->edi) == 0) {
        fail("Database not defined");
        return;
    }
    edi->flags |= EDI_SUPPRESS_SAVE;
    if ((rc = ediAddTable(edi, app->table)) < 0) {
        if (rc != MPR_ERR_ALREADY_EXISTS) {
            fail("Cannot add table '%s'", app->table);
        }
    } else {
        if ((rc = ediAddColumn(edi, app->table, "id", EDI_TYPE_INT, EDI_AUTO_INC | EDI_INDEX | EDI_KEY)) != 0) {
            fail("Cannot add column 'id'");
        }
    }
    for (i = 1; i < argc && !app->error; i++) {
        field = ssplit(sclone(argv[i]), ":", &typeString);
        if ((type = ediParseTypeString(typeString)) < 0) {
            fail("Unknown type '%s' for field '%s'", typeString, field);
            break;
        }
        if ((rc = ediAddColumn(edi, app->table, field, type, 0)) != 0) {
            if (rc != MPR_ERR_ALREADY_EXISTS) {
                fail("Cannot add column '%s'", field);
                break;
            } else {
                ediChangeColumn(edi, app->table, field, type, 0);
            }
        }
    }
    edi->flags &= ~EDI_SUPPRESS_SAVE;
    ediSave(edi);
    qtrace("Update", "Database schema");
}


/*
    Called with args: name [field:type [, field:type] ...]
 */
static void generateScaffoldViews(int argc, char **argv)
{
    genKey("clientList", "${SOURCE}/${CONTROLLER}/${FILENAME}", 0);
    genKey("clientEdit", "${SOURCE}/${CONTROLLER}/${FILENAME}", 0);
}


/*
    esp generate scaffold NAME [field:type [, field:type] ...]
 */
static void generateScaffold(int argc, char **argv)
{
    char    *plural;

    if (argc < 1) {
        usageError();
        return;
    }
    if (getJson(app->config, "esp.generate.controller", 0) == 0) {
        fail("No suitable package installed to generate scaffolds");
        return;
    }
    app->controller = sclone(argv[0]);
    if (!identifier(app->controller)) {
        fail("Cannot generate scaffold. Controller name must be a valid C identifier");
        return;
    }
    /*
        This feature is undocumented.
        Having plural database table names greatly complicates things and ejsJoin is not able to follow
        foreign fields: NameId.
     */
    ssplit(sclone(app->controller), "-", &plural);
    if (plural && *plural) {
        app->table = sjoin(app->controller, plural, NULL);
    } else {
        app->table = app->table ? app->table : app->controller;
    }
    generateScaffoldController(argc, argv);
    generateClientController(argc, argv);
    generateScaffoldViews(argc, argv);
    generateClientModel(argc, argv);
    generateScaffoldMigration(argc, argv);
    migrate(0, 0);
}


/*
    Sort versions in decreasing version order.
    Ensure that pre-releases are sorted before production releases
 */
static int reverseSortFiles(MprDirEntry **d1, MprDirEntry **d2)
{
    char    *base1, *base2, *b1, *b2, *p1, *p2;
    int     rc;

    base1 = mprGetPathBase((*d1)->name);
    base2 = mprGetPathBase((*d2)->name);

    if (smatch(base1, base2)) {
        return 0;
    }
    b1 = ssplit(base1, "-", &p1);
    b2 = ssplit(base2, "-", &p2);
    rc = scmp(b1, b2);
    if (rc == 0) {
        if (!p1) {
            rc = 1;
        } else if (!p2) {
            rc = -1;
        } else {
            rc = scmp(p1, p2);
        }
    }
    return -rc;
}


static int sortFiles(MprDirEntry **d1, MprDirEntry **d2)
{
    return scmp((*d1)->name, (*d2)->name);
}


static void makeEspDir(cchar *path)
{
    if (mprPathExists(path, X_OK)) {
        ;
    } else {
        if (mprMakeDir(path, 0755, -1, -1, 1) < 0) {
            app->error++;
        } else {
            trace("Create",  "Directory: %s", mprGetRelPath(path, 0));
        }
    }
}


static void makeEspFile(cchar *path, cchar *data, ssize len)
{
    bool    exists;

    exists = mprPathExists(path, R_OK);
    if (exists && !app->force) {
        trace("Exists", path);
        return;
    }
    makeEspDir(mprGetPathDir(path));
    if (len <= 0) {
        len = slen(data);
    }
    if (mprWritePathContents(path, data, len, 0644) < 0) {
        fail("Cannot write %s", path);
        return;
    }
    if (!exists) {
        trace("Create", mprGetRelPath(path, 0));
    } else {
        trace("Overwrite", path);
    }
}


static cchar *getCachedPaks()
{
    MprJson         *config, *keyword;
    MprList         *files, *result;
    cchar           *base, *dir, *path, *version;
    int             index, next, show;

    if (!app->paksCacheDir) {
        return 0;
    }
    result = mprCreateList(0, 0);
    files = mprGlobPathFiles(app->paksCacheDir, "*/*", 0);
    for (ITERATE_ITEMS(files, dir, next)) {
        version = getPakVersion(dir, NULL);
        path = mprJoinPaths(dir, version, "package.json", NULL);
        if (mprPathExists(path, R_OK)) {
            if ((config = loadJson(path)) != 0) {
                base = mprGetPathBase(path);
                show = 0;
                if (sstarts(base, "esp-")) {
                    show++;
                } else {
                    MprJson *keywords = mprGetJsonObj(config, "keywords");
                    for (ITERATE_JSON(keywords, keyword, index)) {
                        if (sstarts(keyword->value, "esp")) {
                            show++;
                            break;
                        }
                    }
                }
                if (show && !smatch(base, "esp")) {
                    mprAddItem(result, sfmt("%24s: %s", mprGetJson(config, "name"), mprGetJson(config, "description")));
                }
            }
        }
    }
    return mprListToString(result, "\n");
}


static cchar *readTemplate(cchar *path, MprHash *tokens, ssize *len)
{
    cchar   *cp, *data;
    ssize   size;

    if (!path || *path == '\0') {
        return 0;
    }
    if ((data = mprReadPathContents(path, &size)) == 0) {
        fail("Cannot open template file \"%s\"", path);
        return 0;
    }
    if (len) {
        *len = size;
    }
    /* Detect non-text content via premature nulls */
    for (cp = data; *cp; cp++) { }
    if ((cp - data) < size) {
        /* Skip template as the data looks lik binary */
        return data;
    }
    vtrace("Info", "Using template %s", path);
    data = stemplate(data, tokens);
    if (len) {
        *len = slen(data);
    }
    return data;
}


static cchar *getTemplate(cchar *key, MprHash *tokens)
{
    cchar   *pattern;

    if ((pattern = getJson(app->config, sfmt("esp.generate.%s", key), 0)) != 0) {
        if (mprPathExists(app->paksDir, X_OK)) {
            return readTemplate(mprJoinPath(app->paksDir, pattern), tokens, NULL);
        }
#if DEPRECATED
        if (mprPathExists(app->eroute->generateDir, X_OK)) {
            return readTemplate(mprJoinPath(app->eroute->generateDir, pattern), tokens, NULL);
        }
        if (mprPathExists("generate", X_OK)) {
            return readTemplate(mprJoinPath("generate", pattern), tokens, NULL);
        }
        if (mprPathExists("templates", X_OK)) {
            return readTemplate(mprJoinPath("templates", pattern), tokens, NULL);
        }
#endif
    }
    return 0;
}


static MprHash *makeTokens(cchar *path, MprHash *other)
{
    HttpRoute   *route;
    MprHash     *tokens;
    cchar       *filename, *list;

    route = app->route;
    filename = mprGetPathBase(path);
    list = smatch(app->controller, app->table) ? sfmt("%ss", app->controller) : app->table;

    tokens = mprDeserialize(sfmt(
        "{ NAME: '%s', TITLE: '%s', HOME: '%s', DOCUMENTS: '%s', SOURCE: '%s', BINDIR: '%s', DATABASE: '%s', FILENAME: '%s',"
        "LIST: '%s', LISTEN: '%s', CONTROLLER: '%s', UCONTROLLER: '%s', MODEL: '%s', UMODEL: '%s',"
        "TABLE: '%s', ACTIONS: '', DEFINE_ACTIONS: '' }",
        app->name, app->title, route->home, route->documents, httpGetDir(route, "CONTENTS"), app->binDir, app->database,
        filename, list, app->listen, app->controller, stitle(app->controller), app->controller, stitle(app->controller),
        app->table));
    if (other) {
        mprBlendHash(tokens, other);
    }
    return tokens;
}


static void genKey(cchar *key, cchar *path, MprHash *tokens)
{
    cchar       *data, *pattern;

    if (app->error) {
        return;
    }
    if ((pattern = getJson(app->config, sfmt("esp.generate.%s", key), 0)) == 0) {
        return;
    }
    if (!tokens) {
        tokens = makeTokens(pattern, 0);
    }
    if ((data = getTemplate(key, tokens)) == 0) {
        return;
    }
    if (!path) {
        path = mprTrimPathComponents(pattern, 2);
    }
    makeEspFile(stemplate(path, tokens), data, 0);
}


static void usageError()
{
    cchar   *name, *paks;

    name = mprGetAppName();
    mprEprintf("\nESP Usage:\n\n"
    "  %s [options] [commands]\n\n"
    "  Options:\n"
    "    --cipher cipher            # Password cipher 'md5' or 'blowfish'\n"
    "    --database name            # Database provider 'mdb|sdb'\n"
    "    --force                    # Force requested action\n"
#if KEEP
    /*
        Static linking is not recommended due to the complexity of resolving initializers
     */
    "    --genlink filename         # Generate a static link module for combine compilations\n"
#endif
    "    --home directory           # Change to directory first\n"
    "    --keep                     # Keep intermediate source\n"
    "    --listen [ip:]port         # Generate app to listen at address\n"
    "    --log logFile:level        # Log to file at verbosity level (0-5)\n"
    "    --name appName             # Name for the app when combining\n"
    "    --noupdate                 # Do not update the esp.json\n"
    "    --optimized                # Compile optimized without symbols\n"
    "    --quiet                    # Don't emit trace\n"
    "    --platform os-arch-profile # Target platform\n"
    "    --rebuild                  # Force a rebuild\n"
    "    --route pattern            # Route pattern to select\n"
    "    --routePrefix prefix       # Prefix of route to select\n"
    "    --single                   # Generate a singleton controller\n"
    "    --show                     # Show routes and compile commands\n"
#if KEEP
    /*
        Static linking is not recommended due to the complexity of resolving initializers
     */
    "    --static                   # Use static linking\n"
#endif
    "    --symbols                  # Compile for debug with symbols\n"
    "    --table name               # Override table name if plural required\n"
    "    --trace traceFile:level    # Trace to file at verbosity level (0-5)\n"
    "    --verbose                  # Emit more verbose trace\n"
    "    --why                      # Why compile or skip building\n"
    "\n"
    "  Commands:\n"
    "    esp clean\n"
    "    esp compile [pathFilters ...]\n"
    "    esp config\n"
    "    esp edit key[=value]\n"
    "    esp generate controller name [action [, action] ...\n"
    "    esp generate migration description model [field:type [, field:type] ...]\n"
    "    esp generate scaffold model [field:type [, field:type] ...]\n"
    "    esp generate table name [field:type [, field:type] ...]\n"
    "    esp init [name [version]]\n"
    "    esp migrate [forward|backward|NNN]\n"
    "    esp mode [debug|release|...]\n"
    "    esp role [add|remove] rolename abilities...\n"
    "    esp [run] [ip]:[port] ...\n"
    "    esp user [add|compute] username password roles...\n"
    "    esp user [remove|show] username\n"
    "\n", name);

    initRuntime();
    paks = getCachedPaks();
    if (paks) {
        mprEprintf("  Local Paks: (See also https://embedthis.com/catalog/)\n%s\n", paks);
    }
    app->error = 1;
}


static void fail(cchar *fmt, ...)
{
    va_list     args;
    char        *msg;

    va_start(args, fmt);
    msg = sfmtv(fmt, args);
    mprEprintf("%s\n", msg);
    va_end(args);
    app->error = 1;
}


static void fatal(cchar *fmt, ...)
{
    va_list     args;
    char        *msg;

    va_start(args, fmt);
    msg = sfmtv(fmt, args);
    mprEprintf("%s\n", msg);
    va_end(args);
    exit(2);
}


/*
    Trace unless silent
 */
static void qtrace(cchar *tag, cchar *fmt, ...)
{
    va_list     args;
    char        *msg;

    if (!app->silent) {
        va_start(args, fmt);
        msg = sfmtv(fmt, args);
        tag = sfmt("[%s]", tag);
        mprPrintf("%12s %s\n", tag, msg);
        va_end(args);
    }
}


/*
    Trace unless quiet
 */
static void trace(cchar *tag, cchar *fmt, ...)
{
    va_list     args;
    char        *msg;

    if (!app->quiet) {
        va_start(args, fmt);
        msg = sfmtv(fmt, args);
        tag = sfmt("[%s]", tag);
        mprPrintf("%12s %s\n", tag, msg);
        va_end(args);
    }
}


/*
    Trace only when run with --verbose
 */
static void vtrace(cchar *tag, cchar *fmt, ...)
{
    va_list     args;
    char        *msg;

    if (app->verbose && !app->quiet) {
        va_start(args, fmt);
        msg = sfmtv(fmt, args);
        tag = sfmt("[%s]", tag);
        mprPrintf("%12s %s\n", tag, msg);
        va_end(args);
    }
}

static void why(cchar *path, cchar *fmt, ...)
{
    va_list     args;
    char        *msg;

    if (app->why) {
        va_start(args, fmt);
        msg = sfmtv(fmt, args);
        if (mprGetLogLevel() > 0) {
            mprPrintf("%s: %s\n", path, msg);
        } else {
            mprPrintf("%14s %s %s\n", "[Why]", path, msg);
        }
        va_end(args);
    }
}


static MprJson *loadJson(cchar *path)
{
    MprJson *obj;
    cchar   *errMsg, *str;

    if (!mprPathExists(path, R_OK)) {
        fail("Cannot locate %s", path);
        return 0;
    }
    if ((str = mprReadPathContents(path, NULL)) == 0) {
        fail("Cannot read %s", path);
        return 0;
    } else if ((obj = mprParseJsonEx(str, NULL, 0, 0, &errMsg)) == 0) {
        fail("Cannot load %s. Error: %s", path, errMsg);
        return 0;
    }
    return obj;
}


static MprJson *readConfig(cchar *path)
{
    MprJson     *config;
    cchar       *data, *errorMsg;

    if ((data = mprReadPathContents(path, NULL)) == 0) {
        fail("Cannot read configuration from \"%s\"", path);
        return 0;
    }
    if ((config = mprParseJsonEx(data, 0, 0, 0, &errorMsg)) == 0) {
        fail("Cannot parse %s: error %s", path, errorMsg);
        return 0;
    }
    return config;
}


static void saveConfig(MprJson *config, cchar *path, int flags)
{
    if (!app->noupdate) {
        path = mprJoinPath(app->route ? app->route->home : ".", path);
        if (mprSaveJson(config, path, MPR_JSON_PRETTY | flags) < 0) {
            fail("Cannot save %s", path);
        }
    }
}


/*
    Get a version string from a name#version or from the latest cached version
 */
static cchar *getPakVersion(cchar *name, cchar *version)
{
    MprDirEntry     *dp;
    MprList         *files;

    if (!version || smatch(version, "*")) {
        name = ssplit(sclone(name), "#", (char**) &version);
        if (version && *version == '\0') {
            files = mprGetPathFiles(mprJoinPath(app->paksCacheDir, name), MPR_PATH_RELATIVE);
            mprSortList(files, (MprSortProc) reverseSortFiles, 0);
            if ((dp = mprGetFirstItem(files)) != 0) {
                version = mprGetPathBase(dp->name);
            }
            if (version == 0) {
                fail("Cannot find pak: %s", name);
                return 0;
            }
        }
    }
    return version;
}


static bool identifier(cchar *name)
{
    cchar   *cp;

    if (!name) {
        return 0;
    }
    for (cp = name; *cp; cp++) {
        if (!isalnum(*cp)) {
            break;
        }
    }
    return *cp == '\0' && isalpha(*name);
}


static char *getPassword()
{
    char    *password, *confirm;

    password = getpass("New user password: ");
    confirm = getpass("Confirm user password: ");
    if (smatch(password, confirm)) {
        return password;
    }
    mprLog("", 0, "Password not confirmed");
    return 0;
}


#if WINCE
static char *getpass(char *prompt)
{
    return sclone("NOT-SUPPORTED");
}

#elif ME_WIN_LIKE || VXWORKS
static char *getpass(char *prompt)
{
    static char password[MAX_PASS];
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
    return sclone(password);
}

#endif /* ME_WIN_LIKE */

#endif /* ME_COM_ESP */

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
