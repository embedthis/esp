/*
    esp.h -- Embedded Server Pages (ESP) Module handler.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

#ifndef _h_ESP
#define _h_ESP 1

/********************************* Includes ***********************************/

#include    "edi.h"

#ifdef __cplusplus
extern "C" {
#endif

/********************************** Tunables **********************************/

#ifndef ME_ESP_ABBREV
    #define ME_ESP_ABBREV 1                             /**< Enable the ESP Abbreviated API */
#endif
#ifndef ME_ESP_EMAIL_TIMEOUT
    #define ME_ESP_EMAIL_TIMEOUT (60 * 1000)            /**< Timeout for sending email */
#endif
#ifndef ME_ESP_RELOAD_TIMEOUT
    #define ME_ESP_RELOAD_TIMEOUT (5 * 1000)            /**< Timeout for reloading esp modules */
#endif
#define ESP_TOK_INCR        1024                        /**< Growth increment for ESP tokens */
#define ESP_LISTEN          "4000"                      /**< Default listening endpoint for the esp program */
#define ESP_UNLOAD_TIMEOUT  (10)                        /**< Very short timeout for reloading */
#define ESP_LIFESPAN        (3600 * TPS)                /**< Default generated content cache lifespan */
#define ESP_COMPILE_JSON    "esp-compile.json"          /**< Compile rules filename */

#if ME_64
    #define ESP_VSKEY "HKLM\\SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\VS7"
#else
    #define ESP_VSKEY "HKLM\\SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7"
#endif

#ifndef ESP_VERSION
    #define ESP_VERSION ME_VERSION
#endif
#ifndef ESP_MAJOR_VERSION
    #define ESP_MAJOR_VERSION ME_MAJOR_VERSION
#ifndef ESP_MINOR_VERSION
    #define ESP_MINOR_VERSION ME_MINOR_VERSION
#endif
#endif

/********************************** Defines ***********************************/
/*
    Forward declare the EspAction
 */
struct EspAction;

/**
    Procedure callback
    @ingroup Esp
    @stability Evolving
 */
typedef void (*EspLegacyProc)(HttpStream *stream);
typedef void (*EspProc)(HttpStream *stream, struct EspAction *action);

#define ESP_CONTENT_MARKER  "${_ESP_CONTENT_MARKER_}"       /* Layout content marker */

#if ME_WIN_LIKE
    #define ESP_EXPORT __declspec(dllexport)
#else
    #define ESP_EXPORT
#endif
#define ESP_EXPORT_STRING MPR_STRINGIFY(ESP_EXPORT)

#define ESP_FEEDBACK_VAR        "__feedback__"

/*
    Default VxWorks environment
 */
#ifndef WIND_BASE
    #define WIND_BASE "WIND_BASE-Not-Configured"
#endif
#ifndef WIND_HOME
    #define WIND_HOME "WIND_HOME-Not-Configured"
#endif
#ifndef WIND_HOST_TYPE
    #define WIND_HOST_TYPE "WIND_HOST_TYPE-Not-Configured"
#endif
#ifndef WIND_PLATFORM
    #define WIND_PLATFORM "WIND_PLATFORM-Not-Configured"
#endif
#ifndef WIND_GNU_PATH
    #define WIND_GNU_PATH "WIND_GNU_PATH-Not-Configured"
#endif

/********************************** Parsing ***********************************/
/**
    ESP page parser structure
    @defgroup EspParse EspParse
    @see Esp
    @internal
 */
typedef struct EspState {
    char    *data;                          /**< Input data to parse */
    char    *next;                          /**< Next character in input */
    int     lineNumber;                     /**< Line number for error reporting */
    MprBuf  *token;                         /**< Current token */
    MprBuf  *global;                        /**< Accumulated compiled esp global code */
    MprBuf  *start;                         /**< Accumulated compiled esp start of function code */
    MprBuf  *end;                           /**< Accumulated compiled esp end of function code */
} EspState;

#define ESP_COMPILE_SYMBOLS     0           /**< Override to compile in debug mode. Defaults to same as Appweb */
#define ESP_COMPILE_OPTIMIZED   1           /**< Override to compile in release mode */

/**
    Top level ESP structure. This is a singleton.
 */
typedef struct Esp {
    MprHash         *databases;             /**< Cloned databases */
    MprEvent        *databasesTimer;        /**< Database prune timer */
    MprHash         *internalOptions;       /**< Table of internal HTML control options  */
    MprThreadLocal  *local;                 /**< Thread local data */
    MprMutex        *mutex;                 /**< Multithread lock */
    EdiService      *ediService;            /**< Database service */
    cchar           *hostedDocuments;       /**< Documents directory if hosted */
    int             compileMode;            /**< Force a debug compile */
    int             inUse;                  /**< Active ESP request counter */
    int             reloading;              /**< Reloading ESP and modules */
    MprHash         *vstudioEnv;            /**< Visual Studio environment */
} Esp;

/**
    Entry point for a loadable ESP module
    @param route HttpRoute object
    @param module Mpr module object
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup EspRoute
    @stability Stable
  */
typedef int (*EspModuleEntry)(struct HttpRoute *route, MprModule *module);

/**
    ESP initialization entry point
    @param module Module object if loaded as an MPR module.
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup Esp
    @stability Evolving
 */
PUBLIC int espOpen(MprModule *module);

/**
    Initialize a static library ESP module
    @description This invokes the ESP initializers for the required pre-compiled ESP shared library.
    @param entry ESP initialization function.
    @param appName Name of the ESP application
    @param routeName Name of the route in the appweb.conf file for this ESP application or page
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup Esp
    @stability Evolving
  */
PUBLIC int espStaticInitialize(EspModuleEntry entry, cchar *appName, cchar *routeName);

/**
    Add HTLM internal options to the Esp.options hash
    @internal
 */
PUBLIC void espInitHtmlOptions(Esp *esp);

/**
    Initialize the ESP configuration file parser
    @internal
 */
PUBLIC int espInitParser(void);

/********************************** EspRoutes *********************************/
/**
    EspRoute extended route configuration.
    Note that HttpRoutes may share an EspRoute.
    @defgroup EspRoute EspRoute
    @see Esp
 */
typedef struct EspRoute {
    cchar           *appName;               /**< App module name */
    struct EspRoute *top;                   /**< Top-level route for this application */
    HttpRoute       *route;                 /**< Back link to route */
    EspProc         commonController;       /**< Common code for all controllers */
    MprTime         loaded;                 /**< When configuration was last loaded */

    MprHash         *actions;               /**< Table of actions */
    MprHash         *env;                   /**< Environment variables for route */
    MprHash         *views;                 /**< Table of views */
    cchar           *currentSession;        /**< Current login session when enforcing a single login */
    cchar           *configFile;            /**< Path to config file */

    cchar           *compileCmd;            /**< Compile command template */
    cchar           *linkCmd;               /**< Link command template */
    cchar           *searchPath;            /**< Search path to use when locating compiler/linker */
    cchar           *winsdk;                /**< Windows SDK */

    uint            app: 1;                 /**< Is an esp mvc application */
    uint            combine: 1;             /**< Combine C source into a single file */
    uint            compileMode: 1;         /**< Compile the application debug or release mode */
    uint            compile: 1;             /**< Enable recompiling the application or esp page */
    uint            encodeTypes: 1;         /**< Encode data types in JSON API request/response */
    uint            keep: 1;                /**< Keep intermediate source code after compiling */
    uint            update: 1;              /**< Enable dynamically updating the application */

    Edi             *edi;                   /**< Default database for this route */

#if DEPRECATED && REMOVE
    cchar           *combineScript;         /**< Combine mode script filename */
    cchar           *combineSheet;          /**< Combine mode stylesheet filename */
#endif
} EspRoute;

#if DEPRECATED && REMOVE
/**
    Add the specified pak to the pak.json packs list.
    @param route HttpRoute defining the ESP application
    @param name Desired pak name. For example: "vue-mvc"
    @param version Pack version string.
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup EspRoute
    @stability Deprecated
 */
PUBLIC void espAddPak(HttpRoute *route, cchar *name, cchar *version);
#endif

/**
    Add a route for the home page.
    @description This will add a home page to route ESP applications. This will add the following route:
    <table>
        <tr><td>Name</td><td>Method</td><td>Pattern</td><td>Target</td></tr>
        <tr><td>home</td><td>GET,POST,PUT</td><td>^/$</td><td>index.esp</td></tr>
    </table>
    @param route Parent route from which to inherit configuration.
    @ingroup EspRoute
    @stability Evolving
 */
PUBLIC void espAddHomeRoute(HttpRoute *route);

/**
    Add a route set
    @description This will add a set of routes. It will add a home route and optional routes depending on the route set.
    <table>
        <tr><td>Name</td><td>Method</td><td>Pattern</td><td>Target</td></tr>
        <tr><td>home</td><td>GET,POST,PUT</td><td>^/$</td><td>index.esp</td></tr>
    </table>
    @param route Parent route from which to inherit configuration.
    @param set Route set to select. Use "vue-mvc", or "html-mvc".
    @ingroup EspRoute
    @stability Stable
 */
PUBLIC void espAddRouteSet(HttpRoute *route, cchar *set);

/**
    Initialize ESP
    @description This initializes a route for ESP. This may be called multiple times for different routes.
    @param route Parent route from which to inherit configuration.
    @param prefix Optional URI prefix for all application URIs.
    @param path Pathname to the esp.json file.
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup EspRoute
    @stability Prototype
 */
PUBLIC int espInit(HttpRoute *route, cchar *prefix, cchar *path);

/**
    Load configuration for an ESP application
    @description Load the application's esp.json and pak.json configuration files.
    @param route Parent route from which to inherit configuration.
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup EspRoute
    @stability Prototype
 */
PUBLIC int espLoadConfig(HttpRoute *route);

/**
    Return the corresponding EspRoute for the given Route.
    @description Returns the defined EspRoute for the given Route. Creates a new EspRoute if required.
    @param route Parent route from which to inherit configuration.
    @param create Set to true to create an EspRoute if a suitable one cannot be found.
    @returns The EspRoute object.
    @ingroup EspRoute
    @stability Prototype
    @param route
 */
PUBLIC EspRoute *espRoute(HttpRoute *route, bool create);

/**
    Add caching for response content.
    @description This call configures caching for request responses. Caching may be used for any HTTP method,
    though typically it is most useful for state-less GET requests. Output data may be uniquely cached for requests
    with different request parameters (query, post and route parameters).
    \n\n
    When server-side caching is requested and manual-mode is not enabled, the request response will be automatically
    cached. Subsequent client requests will revalidate the cached content with the server. If the server-side cached
    content has not expired, a HTTP Not-Modified (304) response will be sent and the client will use its client-side
    cached content.  This results in a very fast transaction with the client as no response data is sent.
    Server-side caching will cache both the response headers and content.
    \n\n
    If manual server-side caching is requested, the response will be automatically cached, but subsequent requests will
    require the handler to explicitly send cached content by calling #httpWriteCached.
    \n\n
    If client-side caching is requested, a "Cache-Control" Http header will be sent to the client with the caching
    "max-age" set to the lifesecs argument value. This causes the client to serve client-cached
    content and to not contact the server at all until the max-age expires.
    Alternatively, you can use #httpSetHeader to explicitly set a "Cache-Control header. For your reference, here are
    some keywords that can be used in the Cache-Control Http header.
    \n\n
        "max-age" Max time in seconds the resource is considered fresh.
        "s-maxage" Max time in seconds the resource is considered fresh from a shared cache.
        "public" marks authenticated responses as cacheable.
        "private" shared caches may not store the response.
        "no-cache" cache must re-submit request for validation before using cached copy.
        "no-store" response may not be stored in a cache.
        "must-revalidate" forces clients to revalidate the request with the server.
        "proxy-revalidate" similar to must-revalidate except only for proxy caches.
    \n\n
    Use client-side caching for static content that will rarely change or for content for which using "reload" in
    the browser is an adequate solution to force a refresh. Use manual server-side caching for situations where you need to
    explicitly control when and how cached data is returned to the client. For most other situations, use server-side
    caching.
    @param route HttpRoute object
    @param uri URI to cache.
        If the URI is set to "*" all URIs for that action are uniquely cached. If the request has POST data,
        the URI may include such post data in a sorted query format. E.g. {uri: /buy?item=scarf&quantity=1}.
    @param lifesecs Lifespan of cache items in seconds. If not set to positive integer, the lifesecs will
        default to the route lifespan.
    @param flags Cache control flags. Select ESP_CACHE_MANUAL to enable manual mode. In manual mode, cached content
        will not be automatically sent. Use #httpWriteCached in the request handler to write previously cached content.
        \n\n
        Select ESP_CACHE_CLIENT to enable client-side caching. In this mode a "Cache-Control" Http header will be
        sent to the client with the caching "max-age". WARNING: the client will not send any request for this URI
        until the max-age timeout has expired.
        \n\n
        Select HTTP_CACHE_RESET to first reset existing caching configuration for this route.
        \n\n
        Select HTTP_CACHE_COMBINED, HTTP_CACHE_ONLY or HTTP_CACHE_UNIQUE to define the server-side caching mode. Only
        one of these three mode flags should be specified.
        \n\n
        If the HTTP_CACHE_COMBINED flag is set, the request params (query, post data and route parameters) will be
        ignored and all request for a given URI path will cache to the same cache record.
        \n\n
        Select HTTP_CACHE_UNIQUE to uniquely cache requests with different request parameters. The URIs specified in
        uris should not contain any request parameters.
        \n\n
        Select HTTP_CACHE_ONLY to cache only the exact URI with parameters specified in uris. The parameters must be
        in sorted www-urlencoded format. For example: /example.esp?hobby=sailing&name=john.
    @return A count of the bytes actually written
    @ingroup EspRoute
    @stability Evolving
    @internal
 */
PUBLIC int espCache(HttpRoute *route, cchar *uri, int lifesecs, int flags);

/**
    Compile an ESP page, controller or view
    @description This compiles ESP resources into loadable, cached modules
    @param route HttpRoute object
    @param dispatcher Optional dispatcher to use when waiting for the compilation command.
    @param source ESP source file name
    @param module Output module file name
    @param cacheName MD5 cache name. Not a full path
    @param isView Set to "true" if the source is a view
    @param errMsg Reference to receive an error message if the routine fails.
    @return "True" if the compilation is successful. Errors are logged and sent back to the client if ShowErrors is true.
    @ingroup EspRoute
    @stability Evolving
    @internal
 */
PUBLIC bool espCompile(HttpRoute *route, MprDispatcher *dispatcher, cchar *source, cchar *module, cchar *cacheName,
    int isView, char **errMsg);

/**
    Convert an ESP web page into C code
    @description This parses an ESP web page into an equivalent C source view.
    @param route HttpRoute object
    @param page ESP web page script.
    @param path Pathname for the ESP web page. This is used to process include directives which are resolved relative
        to this path.
    @param cacheName MD5 cache name. Not a full path.
    @param layout Default layout page. Deprecated.
    @param state Reserved. Must set to NULL.
    @param err Output parameter to hold any relevant error message.
    @return Compiled script. Return NULL on errors.
    @ingroup EspRoute
    @stability Evolving
    @internal
 */
PUBLIC char *espBuildScript(HttpRoute *route, cchar *page, cchar *path, cchar *cacheName, cchar *layout,
    EspState *state, char **err);

/**
    Define an action for a URI pattern.
    @description This creates a new route and binds the action function to a URI pattern.
    @param route Parent route object from which to inherit settings when creating the new route.
    @param pattern URI pattern to use to find the releavant route.
    @param actionProc EspProc callback procedure to invoke when the action is requested.
    @ingroup EspRoute
    @stability Stable
 */
PUBLIC int espBindProc(HttpRoute *route, cchar *pattern, void *actionProc);

/**
    Define a common controller function to invoke before invoking for all controller actions.
    @description A base controller function can be defined that will be called before calling any controller action. This emulates a super class constructor.
    @param route HttpRoute object
    @param baseProc Function to call just prior to invoking a controller action.
    @ingroup EspRoute
    @stability Evolving
 */
PUBLIC void espController(HttpRoute *route, EspProc baseProc);


/**
    Create an EspRoute object
    @param route HttpRoute to associate with
    @return EspRoute object
    @internal
    @stability Stable
 */
PUBLIC EspRoute *espCreateRoute(HttpRoute *route);

#if DEPRECATE || 1
/**
    Define a base controller function to invoke for all controller actions.
    @description A base controller function can be defined that will be called before calling any controller action. This
        emulates a super class constructor.
    @param route HttpRoute object
    @param baseProc Function to call just prior to invoking a controller action.
    @ingroup EspRoute
    @stability Deprecated
 */
PUBLIC void espDefineBase(HttpRoute *route, EspLegacyProc baseProc) ME_DEPRECATED("Use espDefineCommon instead");
#endif


/**
    Define a view
    @description Views are ESP web pages that are executed to return presentation data back to the client.
    @param route Http route object
    @param path Path to the ESP view source code.
    @param viewProc EspViewPrococ callback procedure to invoke when the view is requested.
    @ingroup EspRoute
    @stability Stable
 */
PUBLIC void espDefineView(HttpRoute *route, cchar *path, void *viewProc);

/**
    Expand a compile or link command template
    @description This expands a command template and replaces "${tokens}" with their equivalent value. The supported
        tokens are:
        <ul>
            <li>ARCH - Build architecture (i386, x86_64)</li>
            <li>CC - Compiler pathname</li>
            <li>DEBUG - Compiler debug options (-g, -Zi, -Od)</li>
            <li>INC - Include directory (out/inc)</li>
            <li>LIB - Library directory (out/lib, out/bin)</li>
            <li>LIBS - Required libraries directory (esp, mpr)</li>
            <li>OBJ - Name of compiled source (out/lib/view-MD5.o)</li>
            <li>OUT - Output module (view_MD5.dylib)</li>
            <li>SHLIB - Shared library extension (.lib, .so)</li>
            <li>SHOBJ - Shared object extension (.dll, .so)</li>
            <li>SRC - Path to source code for view or controller (already templated)</li>
            <li>TMP - System temporary directory</li>
            <li>WINSDK - Path to the Windows SDK</li>
            <li>VS - Path to Visual Studio</li>
        </ul>
    @param route HttpRoute object
    @param command Command to run
    @param source ESP web page source pathname
    @param module Output module pathname
    @return An expanded command line
    @ingroup EspRoute
    @stability Evolving
    @internal
 */
PUBLIC char *espExpandCommand(HttpRoute *route, cchar *command, cchar *source, cchar *module);

/**
    Get a configuration value from the ESP pak.json
    @param route HttpRoute defining the ESP application
    @param key Configuration property path. May contain dots.
    @param defaultValue Default value to use if the configuration is not defined. May be null
    @returns the Configuration string value
    @ingroup EspRoute
    @stability Stable
 */
PUBLIC cchar *espGetConfig(HttpRoute *route, cchar *key, cchar *defaultValue);

#if DEPRECATED && REMOVE
/**
    Test if the ESP application includes the specified pak
    @description This tests the dependencies property specified pak.
    @param route HttpRoute defining the ESP application
    @param name Desired pak name. For example: "vue-mvc"
    @returns True if the specified pak is supported
    @ingroup EspRoute
    @stability Deprecated
 */
PUBLIC bool espHasPak(HttpRoute *route, cchar *name);
#endif

/**
    Load the compiler rules from esp-compile.json
    @param route HttpRoute object
    @ingroup EspRoute
    @stability Prototype
    @internal
 */
PUBLIC int espLoadCompilerRules(HttpRoute *route);

#if DEPRECATED && REMOVE
/**
    Save the in-memory ESP pak.json configuration to the default location for the ESP application
    defined by the specified route.
    @param route HttpRoute defining the ESP application
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup EspRoute
    @stability Deprecated
 */
PUBLIC int espSaveConfig(HttpRoute *route);
#endif

/**
    Set a configuration value to the ESP pak.json.
    @description This updates the in-memory copy of the pak.json only.
    @param route HttpRoute defining the ESP application
    @param key Configuration property path. May contain dots.
    @param value Value to set the property to.
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup EspRoute
    @stability Evolving
 */
PUBLIC int espSetConfig(HttpRoute *route, cchar *key, cchar *value);

/**
    Set a private data reference for the current request
    @param stream HttpStream object
    @param data Data object to associate with the current request. This must be a managed reference.
    @return Reference to private data
    @ingroup Esp
    @stability prototype
 */
PUBLIC void espSetData(HttpStream *stream, void *data);

/**
    Test if a configuration property from the ESP pak.json has a desired value.
    @param route HttpRoute defining the ESP application
    @param key Configuration property path. May contain dots.
    @param desired Desired value to compare with.
    @returns True if the configuration property has the desired value.
    @ingroup EspRoute
    @stability Evolving
 */
PUBLIC bool espTestConfig(HttpRoute *route, cchar *key, cchar *desired);

/*
    Internal
 */
PUBLIC cchar *espGetVisualStudio(void);
PUBLIC void espManageEspRoute(EspRoute *eroute, int flags);
PUBLIC bool espModuleIsStale(HttpRoute *route, cchar *source, cchar *module, int *recompile);
PUBLIC int espOpenDatabase(HttpRoute *route, cchar *spec);
PUBLIC void espCloseDatabase(HttpRoute *route);
PUBLIC int espReloadDatabase(HttpRoute *route);
PUBLIC void espSetDefaultDirs(HttpRoute *route, bool app);

/********************************** Requests **********************************/
/**
    View procedure callback.
    @param stream Http stream object
    @ingroup EspReq
    @stability Stable
 */
typedef void (*EspViewProc)(HttpStream *stream);

/**
    ESP request structure
    @defgroup EspReq EspReq
    @stability Internal
    @see Esp
 */
typedef struct EspReq {
    HttpRoute       *route;                 /**< Route reference */
    Esp             *esp;                   /**< Convenient esp reference */
    MprHash         *feedback;              /**< Feedback messages */
    MprHash         *lastFeedback;          /**< Feedback messages from the last request */
    HttpNotifier    notifier;               /**< Http state change notification callback */
    void            *data;                  /**< Custom data for request (managed) */
    void            *staticData;            /**< Custom data for request (unmanaged) */
    cchar           *commandLine;           /**< Command line for compile/link */
    int             autoFinalize;           /**< Request is or will be auto-finalized */
    int             sessionProbed;          /**< Already probed for session store */
    int             lastDomID;              /**< Last generated DOM ID */
    Edi             *edi;                   /**< Database for this request */
} EspReq;

/**
    Add a header to the transmission using a format string.
    @description Add a header if it does not already exist.
    @param stream HttpStream stream object
    @param key Http response header key
    @param fmt Printf style formatted string to use as the header key value
    @param ... Arguments for fmt
    @return Zero if successful, otherwise a negative MPR error code. Returns MPR_ERR_ALREADY_EXISTS if the header already
        exists.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espAddHeader(HttpStream *stream, cchar *key, cchar *fmt, ...);

/**
    Add a header to the transmission.
    @description Add a header if it does not already exist.
    @param stream HttpStream stream object
    @param key Http response header key
    @param value Value to set for the header
    @return Zero if successful, otherwise a negative MPR error code. Returns MPR_ERR_ALREADY_EXISTS if the header already
        exists.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espAddHeaderString(HttpStream *stream, cchar *key, cchar *value);

/**
    Add a request parameter value if it is not already defined.
    @param stream HttpStream stream object
    @param var Name of the request parameter to set
    @param value Value to set.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espAddParam(HttpStream *stream, cchar *var, cchar *value);

/**
    Append a transmission header.
    @description Set the header if it does not already exist. Append with a ", " separator if the header already exists.
    @param stream HttpStream stream object
    @param key Http response header key
    @param fmt Printf style formatted string to use as the header key value
    @param ... Arguments for fmt
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espAppendHeader(HttpStream *stream, cchar *key, cchar *fmt, ...);

/**
    Append a transmission header string.
    @description Set the header if it does not already exist. Append with a ", " separator if the header already exists.
    @param stream HttpStream stream object
    @param key Http response header key
    @param value Value to set for the header
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espAppendHeaderString(HttpStream *stream, cchar *key, cchar *value);

/**
    Auto-finalize transmission of the http request.
    @description If auto-finalization is enabled via #espSetAutoFinalizing, this call will finalize writing Http response
    data by writing the final chunk trailer if required. If using chunked transfers, a null chunk trailer is required
    to signify the end of write data.  If the request is already finalized, this call does nothing.
    @param stream HttpStream stream object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espAutoFinalize(HttpStream *stream);

/**
    Create a session state object.
    @description The session state object can be used to share state between requests.
    If a session has not already been created, this call will create a new session.
    It will create a response cookie containing a session ID that will be sent to the client
    with the response. Note: Objects are stored in the session state using JSON serialization.
    @param stream HttpStream stream object
    @return Session ID string
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espCreateSession(HttpStream *stream);

/**
    Destroy a session state object.
    @description This will destroy the server-side session state and
        emit an expired cookie to the client to force it to erase the session cookie.
    @param stream HttpStream stream object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espDestroySession(HttpStream *stream);

/**
    Send mail using sendmail
    @param stream HttpStream stream object
    @param to Message recipient
    @param from Message sender
    @param subject Message subject
    @param date Message creation date. Set to null to use the current date/time.
    @param mime Message mime type. Set to null for text/plain.
    @param message Message body
    @param files MprList of files to send with the message.
    @return Zero if the email is successfully sent.
    @stability Evolving
 */
PUBLIC int espEmail(HttpStream *stream, cchar *to, cchar *from, cchar *subject, MprTime date, cchar *mime,
    cchar *message, MprList *files);

/**
    Indicate the request is finalized.
    @description Calling this routine indicates that the handler has fully finished processing the request including
        processing all input, generating a full response and any other required processing. This call will invoke
        #httpFinalizeOutput and then set the request finalized flag. If the request is already finalized, this call
        does nothing. A handler MUST call httpFinalize when it has completed processing a request.
        As background: there are three finalize concepts: HttpTx.finalizedOutput means the handler has generated all
        the response output but it may not yet be fully transmited through the pipeline and to the network by the
        connector. HttpTx.finalizedConnector means the connector has sent all the output to the network.  HttpTx.finalized
        means the application has fully processed the request including reading all the input data it wishes to read
        and has generated all the output that will be generated. A fully finalized request has both HttpTx.finalized
        and HttpTx.finalizedConnector true.
    @param stream HttpStream stream object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espFinalize(HttpStream *stream);

/**
    Flush transmit data.
    @description This writes any buffered data and initiates writing to the peer. This will not block.
    @param stream HttpStream stream object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espFlush(HttpStream *stream);

/**
    Get the current route HttpAuth object.
    @param stream HttpStream stream object
    @return The HttpAuth object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC HttpAuth *espGetAuth(HttpStream *stream);

/**
    Get the current request stream.
    @return The HttpStream stream object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC HttpStream *espGetStream(void);

/**
    Get the receive body content length.
    @description Get the length of the receive body content (if any). This is used in servers to get the length of posted
        data and, in clients, to get the response body length.
    @param stream HttpStream stream object
    @return A count of the response content data in bytes.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC MprOff espGetContentLength(HttpStream *stream);

/**
    Get the receive body content type.
    @description Get the content mime type of the receive body content (if any).
    @param stream HttpStream stream object
    @return Mime type of any receive content. Set to NULL if not posted data.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetContentType(HttpStream *stream);

/**
    Get a request cookie.
    @description Get the cookie for the given name.
    @param stream HttpStream stream object
    @param name Cookie name to retrieve
    @return Return the cookie value
        Return null if the cookie is not defined.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetCookie(HttpStream *stream, cchar *name);

/**
    Get the request cookies.
    @description Get the cookies defined in the current request. This returns the HTTP cookies header with all
        cookies in one string.
    @param stream HttpStream stream object
    @return Return a string containing the cookies sent in the Http header of the last request
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetCookies(HttpStream *stream);

/**
    Get the private data reference for the current request set via #setData
    @param stream HttpStream object
    @return Reference to private data
    @ingroup EspReq
    @stability prototype
 */
PUBLIC void *espGetData(HttpStream *stream);

/**
    Get the current database instance.
    @description A route may have a default database configured via the EspDb Appweb.conf configuration directive.
    The database will be opened when the web server initializes and will be shared between all requests using the route.
    @return Edi EDI database handle
    @ingroup EspReq
    @stability Stable
 */
PUBLIC Edi *espGetDatabase(HttpStream *stream);

/**
    Get the current extended route information.
    @return EspRoute instance
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC EspRoute *espGetEspRoute(HttpStream *stream);

/**
    Get the default documents directory for the request route.
    @param stream HttpStream stream object
    @return A directory path name
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetDocuments(HttpStream *stream);

/**
    Get a feedback message defined via #feedback
    @param stream HttpStream object
    @param type type of feedback message to retrieve. This may be set to any word, but the following feedback types
        are typically supported as per RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @return Reference to the feedback message
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *espGetFeedback(HttpStream *stream, cchar *type);

/**
    Get the current database grid reference.
    @description The current grid is defined via #setGrid
    @return EdiGrid instance
    @ingroup EspReq
    @stability Deprecated
    @internal
 */
PUBLIC EdiGrid *espGetGrid(HttpStream *stream);

/**
    Get an rx http header.
    @description Get a http response header for a given header key.
    @param stream HttpStream stream object
    @param key Name of the header to retrieve. This should be a lower case header name. For example: "Connection"
    @return Value associated with the header key or null if the key did not exist in the response.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetHeader(HttpStream *stream, cchar *key);

/**
    Get the hash table of rx Http headers.
    @description Get the internal hash table of rx headers
    @param stream HttpStream stream object
    @return Hash table. See MprHash for how to access the hash table.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC MprHash *espGetHeaderHash(HttpStream *stream);

/**
    Get all the request http headers.
    @description Get all the rx headers. The returned string formats all the headers in the form:
        key: value\\nkey2: value2\\n...
    @param stream HttpStream stream object
    @return String containing all the headers. The caller must free this returned string.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC char *espGetHeaders(HttpStream *stream);

/**
    Get the HTTP method.
    @description This is a convenience API to return the Http method
    @return The HttpStream.rx.method property
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetMethod(HttpStream *stream);

/**
    Get a request parameter.
    @description Get the value of a named request parameter. Request parameters are defined via www-urlencoded
        query, post data contained in the request and route parameters. Route parameters are stored as JSON tree objects
        and may contain nested properties.
    @param stream HttpStream stream object
    @param var Name of the request parameter to retrieve
    @param defaultValue Default value to return if the variable is not defined. Can be null.
    @return String containing the request parameter's value. Caller should not free.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetParam(HttpStream *stream, cchar *var, cchar *defaultValue);

/**
    Get a request pararmeter as an integer.
    @description Get the value of a named request parameter. Request parameters are defined via www-urlencoded
        query, post data contained in the request and route parameters. Request parameters are stored as JSON tree objects
        and may contain nested properties.
    @param stream HttpStream stream object
    @param var Name of the request parameter to retrieve
    @param defaultValue Default value to return if the variable is not defined. Can be null.
    @return Integer containing the request parameter's value
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC int espGetIntParam(HttpStream *stream, cchar *var, int defaultValue);

/**
    Get a request pararmeter as a JSON object.
    @description Get the value of a named request parameter. Request parameters are defined via www-urlencoded
        query, post data contained in the request and route parameters. Request parameters are stored as JSON tree objects
        and may contain nested properties.
    @param stream HttpStream stream object
    @param var Name of the request parameter to retrieve
    @return JSON parameter object.
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC MprJson *espGetParamObj(HttpStream *stream, cchar *var);

/**
    Get the request parameters.
    @description This call gets the request parameters for the current request.
    @description Request parameters are defined via www-urlencoded query, post data contained in the request and route parameters.
        Request parameters are stored as JSON tree objects and may contain nested properties.
    @param stream HttpStream stream object
    @return MprJson instance containing the request parameters
    @ingroup EspReq
    @stability Stable
 */
PUBLIC MprJson *espGetParams(HttpStream *stream);

/**
    Get the request URI path string.
    @description This is a convenience API to return the request URI path. This is the request URI path after removing
        query parameters. It does not include the application route prefix.
    @return The espGetStream()->rx->pathInfo
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *espGetPath(HttpStream *stream);

/**
    Get the request URI query string.
    @description Get URI query string sent with the current request.
    @param stream HttpStream stream object
    @return String containing the request query string. Caller should not free.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetQueryString(HttpStream *stream);

/**
    Get the referring URI.
    @description This returns the referring URI as described in the HTTP "referer" (yes the HTTP specification does
        spell it incorrectly) header. If this header is not defined, this routine will return the home URI as returned
        by uri("~").
    @param stream HttpStream stream object
    @return String URI back to the referring URI. If no referrer is defined, refers to the home URI.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetReferrer(HttpStream *stream);

/**
    Get the current route HttpRoute object.
    @param stream HttpStream stream object
    @return The HttpRoute object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC HttpRoute *espGetRoute(HttpStream *stream);

/**
    Get the default database defined on a route.
    @param route HttpRoute object
    @return Database instance object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC Edi *espGetRouteDatabase(HttpRoute *route);

/**
    Get a route variable
    @description Get the value of a request route variable.
    @param stream HttpStream stream object
    @param var Name of the request parameter to retrieve
    @return String containing the route variable value. Caller should not free.
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *espGetRouteVar(HttpStream *stream, cchar *var);

/**
    Get the session state ID.
    @description This will get the session and return the session ID. This will create a new session state storage area if
        create is true and one does not already exist. This can be used to test if the session state exists for this
        stream.
    @param stream HttpStream stream object
    @param create Set to true to create a new session if one does not already exist.
    @return The session state identifier string.
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *espGetSessionID(HttpStream *stream, int create);

/**
    Get the response status.
    @param stream HttpStream stream object
    @return An integer Http response code. Typically 200 is success.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC int espGetStatus(HttpStream *stream);

/**
    Get the Http response status message.
    @description The HTTP status message is supplied on the first line of the HTTP response.
    @param stream HttpStream stream object
    @returns A Http status message.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetStatusMessage(HttpStream *stream);

/**
    Get the uploaded files.
    @description Get the list of uploaded files.
        This list entries are HttpUploadFile objects.
    @param stream HttpStream stream object
    @return A list of HttpUploadFile objects.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC MprList *espGetUploads(HttpStream *stream);

/**
    Get the request URI string.
    @description This is a convenience API to return the request URI. This is the request URI after removing
        query parameters. It includes any application route prefix.
    @return The espGetStream()->rx->uri
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetUri(HttpStream *stream);

/**
    Test if a current grid has been defined via #espSetGrid.
    @return "True" if a current grid has been defined
    @ingroup EspReq
    @stability Deprecated
    @internal
 */
PUBLIC bool espHasGrid(HttpStream *stream);

/**
    Test if a current record has been defined and save to the database.
    @description This call returns "true" if a current record is defined and has been saved to the database with a
        valid "id" field.
    @return "True" if a current record with a valid "id" is defined.
    @ingroup EspReq
    @stability Deprecated
    @internal
 */
PUBLIC bool espHasRec(HttpStream *stream);

/**
    Test if the request is being made on behalf of the current, single authenticated user.
    @description Set esp.login.single to true to enable current session tracking.
    @return true if the
    @stability Evolving
    @ingroup EspReq
 */
PUBLIC bool espIsCurrentSession(HttpStream *stream);

/**
    Test if the user is authenticated
    @param stream HttpStream stream object
    @return True if the username and password have been authenticated.
    @ingroup EspReq
    @stability Prototype
 */
PUBLIC bool espIsAuthenticated(HttpStream *stream);

/**
    Test if the receive input stream is at end-of-file.
    @param stream HttpStream stream object
    @return "True" if there is no more receive data to read
    @ingroup EspReq
    @stability Stable
 */
PUBLIC bool espIsEof(HttpStream *stream);

/**
    Test if the stream is using SSL and is secure.
    @param stream HttpStream stream object
    @return "True" if the stream is using SSL.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC bool espIsSecure(HttpStream *stream);

/**
    Test if the request has been finalized.
    @description This tests if #espFinalize or #httpFinalize has been called for a request.
    @param stream HttpStream stream object
    @return "True" if the request has been finalized.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC bool espIsFinalized(HttpStream *stream);

/**
    Match a request parameter with an expected value.
    @description Compare a request parameter and return "true" if it exists and its value matches.
    @param stream HttpStream stream object
    @param var Name of the request parameter
    @param value Expected value to match
    @return "True" if the value matches
    @ingroup EspReq
    @stability Stable
 */
PUBLIC bool espMatchParam(HttpStream *stream, cchar *var, cchar *value);

/**
    Read receive body content.
        Use httpReadBlock for more options to read data.
    @description Read body content from the client. This call does not block.
    @param stream HttpStream stream object
    @param buf Buffer to accept content data
    @param size Size of the buffer
    @return A count of bytes read into the buffer
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espReceive(HttpStream *stream, char *buf, ssize size);

/**
    Redirect the client.
    @description Redirect the client to a new uri.
    @param stream HttpStream stream object
    @param status Http status code to send with the response
    @param target New target uri for the client
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espRedirect(HttpStream *stream, int status, cchar *target);

/**
    Redirect the client back to the referrer
    @description Redirect the client to the referring URI.
    @param stream HttpStream stream object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espRedirectBack(HttpStream *stream);

/**
    Remove a cookie
    @param stream HttpStream stream object
    @param name Cookie name
    @ingroup EspReq
    @stability Stable
*/
PUBLIC void espRemoveCookie(HttpStream *stream, cchar *name);

/**
    Remove a header from the transmission
    @description Remove a header if present.
    @param stream HttpStream stream object
    @param key Http response header key
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC int espRemoveHeader(HttpStream *stream, cchar *key);

/**
    Remove a session state variable
    @param stream HttpStream stream object
    @param name Variable name to set
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espRemoveSessionVar(HttpStream *stream, cchar *name);

/**
    Render a formatted string.
    @description Render a formatted string of data into packets to the client. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client and
        may yield to the garbage collector.
    @param stream HttpStream stream object
    @param fmt Printf style formatted string
    @param ... Arguments for fmt
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRender(HttpStream *stream, cchar *fmt, ...);

/**
    Render the client configuration string in JSON
    @param stream HttpStream stream object
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability PRototype
 */
PUBLIC ssize espRenderConfig(HttpStream *stream);

/**
    Render a block of data to the client.
    @description Render a block of data to the client. Data packets will be created as required to store the write data.
    This call may block waiting for the client to absorb the data.
    @param stream HttpStream stream object
    @param buf Buffer containing the write data
    @param size Size of the data in buf
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderBlock(HttpStream *stream, cchar *buf, ssize size);

/**
    Render cached content.
    @description Render the saved, cached response from a prior request to this URI. This is useful if the caching
        mode has been set to "manual".
    @param stream HttpStream stream object
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderCached(HttpStream *stream);

/**
    Render an ESP document
    @description If the document is an ESP page, it will be rendered as a view via #espRenderDocument.
        Otherwise, it will be rendered using the fileHandler as a static document. This routine may yield.
    @param stream Http stream object
    @param path Relative pathname from route->documents to the document to render.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espRenderDocument(HttpStream *stream, cchar *path);

/**
    Render an error message back to the client and finalize the request. The output is Html escaped for security.
    @param stream HttpStream stream object
    @param status Http status code
    @param fmt Printf style message format
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderError(HttpStream *stream, int status, cchar *fmt, ...);

/**
    Render feedback messages.
    @description Feedback messages for one-time messages that are sent to the client. For HTML clients, feedback
    messages use the session state store and persist for only one request. For smart/thick clients, feedback messages
    are sent as JSON responses via the espSendFeedback API. See #espSetFeedback for how to define feedback messages.
    @param stream Http stream object
    @param types Types of feedback message to retrieve. Set to "*" to retrieve all types of feedback.
        This may be set to any word, but the following feedback types are typically supported as per
        RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @return Number of bytes written
    @ingroup EspControl
    @stability Deprecated
    @internal
 */
PUBLIC ssize espRenderFeedback(HttpStream *stream, cchar *types);

/**
    Render the contents of a file back to the client.
    @param stream HttpStream stream object
    @param path File path name
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderFile(HttpStream *stream, cchar *path);

/**
    Read a table from the current database
    @param stream HttpStream stream object
    @param tableName Database table name
    @return An EDI grid containing data for the table.
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC EdiGrid *espReadTable(HttpStream *stream, cchar *tableName);

/**
    Render a formatted string after HTML escaping
    @description Render a formatted string of data and then HTML escape. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client.
    @param stream HttpStream stream object
    @param fmt Printf style formatted string
    @param ... Arguments for fmt
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
*/
PUBLIC ssize espRenderSafe(HttpStream *stream, cchar *fmt, ...);

/**
    Render a safe string of data to the client.
    @description HTML escape a string and then write the string of data to the client.
        Data packets will be created as required to store the write data. This call may block waiting for the data to
        the client to drain.
    @param stream HttpStream stream object
    @param s String containing the data to write
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderSafeString(HttpStream *stream, cchar *s);

/**
    Render a string of data to the client
    @description Render a string of data to the client. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client.
    @param stream HttpStream stream object
    @param s String containing the data to write
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderString(HttpStream *stream, cchar *s);

/**
    Render the value of a request variable to the client.
    If a request parameter is not found by the given name, consult the session store for a variable the same name.
    @description This writes the value of a request variable after HTML escaping its value.
    @param stream HttpStream stream object
    @param name Request parameter variable name
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderVar(HttpStream *stream, cchar *name);

/**
    Render an ESP view page to the client
    @param stream Http stream object
    @param view View name. The view name is interpreted relative to the matching route documents directory and may omit
        an ESP extension. This routine may yield.
    @param flags Reserved. Set to zero.
    @return true if a vew can be rendered.
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC bool espRenderView(HttpStream *stream, cchar *view, int flags);

/**
    Send a database grid as a JSON string
    @description The JSON string is rendered as part of an enclosing "{ data: JSON }" wrapper.
    @param stream HttpStream stream object
    @param grid EDI grid
    @param flags Reserved. Set to zero.
    @return Number of bytes rendered
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC ssize espSendGrid(HttpStream *stream, EdiGrid *grid, int flags);

/**
    Send a database record as a JSON string
    @description The JSON string is rendered as part of an enclosing "{ data: JSON }" wrapper.
    @param stream HttpStream stream object
    @param rec EDI record
    @param flags Reserved. Set to zero.
    @return Number of bytes rendered
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC ssize espSendRec(HttpStream *stream, EdiRec *rec, int flags);

/**
    Send a JSON response result
    @description This renders a JSON response including the request success status, feedback message and field errors.
    The field errors apply to the current EDI record.
    The format of the response is:
        "{ error: 0/1, feedback: {messages}, fieldErrors: {messages}}" wrapper.
    The feedback messages are created via the espSetFeedback API. Field errors are created by ESP validations.
    @param stream HttpStream stream object
    @param success True if the operation was a success.
    @return Number of bytes sent.
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC ssize espSendResult(HttpStream *stream, bool success);

/**
    Enable auto-finalizing for this request
    @param stream HttpStream stream object
    @param on Set to "true" to enable auto-finalizing.
    @return "True" if auto-finalizing was enabled prior to this call
    @ingroup EspReq
    @stability Stable
 */
PUBLIC bool espSetAutoFinalizing(HttpStream *stream, bool on);

/**
    Set the current request stream.
    @param stream The HttpStream stream object to define
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetStream(HttpStream *stream);

/**
    Define a content length header in the transmission.
    @description This will define a "Content-Length: NNN" request header.
    @param stream HttpStream stream object
    @param length Numeric value for the content length header.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetContentLength(HttpStream *stream, MprOff length);

/**
    Set a cookie in the transmission
    @description Define a cookie to send in the transmission Http header
    @param stream HttpStream stream object
    @param name Cookie name
    @param value Cookie value
    @param path URI path to which the cookie applies
    @param domain String Domain in which the cookie applies. Must have 2-3 "." and begin with a leading ".".
        For example: domain: .example.com. Set to NULL to use the current connection's client domain.
    Some browsers will accept cookies without the initial ".", but the spec: (RFC 2109) requires it.
    @param lifespan Duration for the cookie to persist in msec. Set to a negative number to delete a cookie. Set to
        zero for a "session" cookie that lives only for the user's session.
    @param isSecure Set to "true" if the cookie only applies for SSL based connections.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetCookie(HttpStream *stream, cchar *name, cchar *value, cchar *path, cchar *domain, MprTicks lifespan,
    bool isSecure);

/**
    Set the transmission (response) content mime type
    @description Set the mime type Http header in the transmission
    @param stream HttpStream stream object
    @param mimeType Mime type string
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetContentType(HttpStream *stream, cchar *mimeType);

/**
    Set this authenticated session as the current session.
    @description Set esp.login.single to true to enable current session tracking.
    @return true if the
    @stability Evolving
    @ingroup EspReq
 */
PUBLIC void espSetCurrentSession(HttpStream *stream);

/**
    Clear the current authenticated session
    @stability Evolving
    @ingroup EspReq
 */
PUBLIC void espClearCurrentSession(HttpStream *stream);

/**
    Set a feedback message
    @description Feedback messages are a convenient way to aggregate messages state information in the response.
        Feedback messages are removed at the completion of the request.
    @param stream Http stream object
    @param type type of feedback message. This may be set to any word, but the following feedback types
        are typically supported as per RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @param fmt Printf style formatted string to use as the message
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetFeedback(HttpStream *stream, cchar *type, cchar *fmt, ...);

/**
    Send a feedback message
    @param stream Http stream object
    @param type type of feedback message. This may be set to any word, but the following feedback types
        are typically supported as per RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @param fmt Printf style formatted string to use as the message
    @param args Varargs style list
    @ingroup EspReq
    @stability Internal
    @internal
 */
PUBLIC void espSetFeedbackv(HttpStream *stream, cchar *type, cchar *fmt, va_list args);

/**
    Set the current database grid
    @return The grid instance. This permits chaining.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC EdiGrid *espSetGrid(HttpStream *stream, EdiGrid *grid);

/**
    Set a transmission header
    @description Set a Http header to send with the request. If the header already exists, its value is overwritten.
    @param stream HttpStream stream object
    @param key Http response header key
    @param fmt Printf style formatted string to use as the header key value
    @param ... Arguments for fmt
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetHeader(HttpStream *stream, cchar *key, cchar *fmt, ...);

/**
    Set a simple key/value transmission header
    @description Set a Http header to send with the request. If the header already exists, its value is overwritten.
    @param stream HttpStream stream object
    @param key Http response header key
    @param value String value for the key
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetHeaderString(HttpStream *stream, cchar *key, cchar *value);

/**
    Set an integer request parameter value
    @description Set the value of a named request parameter to an integer value. Request parameters are defined via
        www-urlencoded query or post data contained in the request.
    @param stream HttpStream stream object
    @param var Name of the request parameter to set
    @param value Value to set.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetParamInt(HttpStream *stream, cchar *var, int value);
#define espSetIntParam espSetParamInt

/**
    Define a notifier callback for this stream.
    @description The notifier callback will be invoked for state changes and I/O events as requests are processed.
    The supported events are:
    <ul>
    <li>HTTP_EVENT_STATE &mdash; The request is changing state. Valid states are:
        HTTP_STATE_BEGIN, HTTP_STATE_CONNECTED, HTTP_STATE_FIRST, HTTP_STATE_CONTENT, HTTP_STATE_READY,
        HTTP_STATE_RUNNING, HTTP_STATE_FINALIZED and HTTP_STATE_COMPLETE. A request will always visit all states and the
        notifier will be invoked for each and every state. This is true even if the request has no content, the
        HTTP_STATE_CONTENT will still be visited.</li>
    <li>HTTP_EVENT_READABLE &mdash; There is data available to read</li>
    <li>HTTP_EVENT_WRITABLE &mdash; The outgoing pipeline can absorb more data</li>
    <li>HTTP_EVENT_ERROR &mdash; The request has encountered an error</li>
    <li>HTTP_EVENT_DESTROY &mdash; The stream structure is about to be destoyed</li>
    <li>HTTP_EVENT_OPEN &mdash; The application layer is now open</li>
    <li>HTTP_EVENT_CLOSE &mdash; The application layer is now closed</li>
    </ul>
    Before the notifier is invoked, espSetStream is called to set the stream object in the thread local storage.
    This enables the ESP Abbreviated API.
    @param stream HttpStream stream object created via #httpCreateStream
    @param notifier Notifier function.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetNotifier(HttpStream *stream, HttpNotifier notifier);

/**
    Set the current database record
    @description The current record is used to supply data to various abbreviated controls, such as: text(), input(),
        checkbox and dropdown()
    @param stream HttpStream stream object
    @param rec Record object to define as the current record.
    @return The grid instance. This permits chaining.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC EdiRec *espSetRec(HttpStream *stream, EdiRec *rec);

/**
    Set a request parameter value
    @description Set the value of a named request parameter to a string value. Parameters are defined via
        requeset POST data or request URI queries. This API permits these initial request parameters to be set or
        modified.
    @param stream HttpStream stream object
    @param var Name of the request parameter to set
    @param value Value to set.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetParam(HttpStream *stream, cchar *var, cchar *value);

/**
    Set a Http response status.
    @description Set the Http response status for the request. This defaults to 200 (OK).
    @param stream HttpStream stream object
    @param status Http status code.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetStatus(HttpStream *stream, int status);

/**
    Set a session variable.
    @description
    @param stream Http stream object
    @param name Variable name to set
    @param value Variable value to use
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup HttpSession
    @stability Stable
 */
PUBLIC int espSetSessionVar(HttpStream *stream, cchar *name, cchar *value);

/**
    Show request details
    @description This e request details back to the client. This is useful as a debugging tool.
    @param stream HttpStream stream object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espShowRequest(HttpStream *stream);

/**
    Update the cached content for a request
    @description Save the given content for future requests. This is useful if the caching mode has been set to "manual".
    @param stream HttpStream stream object
    @param uri Request URI to cache for
    @param data Data to cache
    @param lifesecs Time in seconds to cache the data
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espUpdateCache(HttpStream *stream, cchar *uri, cchar *data, int lifesecs);

/**
    Write a record to the database
    @description The record will be saved to the database after running any field validations. If any field validations
        fail to pass, the record will not be written and error details can be retrieved via #ediGetRecErrors.
        If the record is a new record and the "id" column is EDI_AUTO_INC, then the "id" will be assigned
        prior to saving the record.
    @param stream HttpStream stream object
    @param rec Record to write to the database.
    @return "true" if the record can be successfully written.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC bool espUpdateRec(HttpStream *stream, EdiRec *rec);

/**
    Create a URI.
    @description Create a URI link by expansions tokens based on the current request and route state.
    The target parameter may contain partial or complete URI information. The missing parts
    are supplied using the current request and route tables. The resulting URI is a normalized, server-local
    URI (that begins with "/"). The URI will include any defined route prefix, but will not include scheme, host or
    port components.
    @param stream HttpStream stream object
    @param target The URI target. The target parameter can be a URI string or JSON style set of options.
        The target will have any embedded "{tokens}" expanded by using token values from the request parameters.
        If the target has an absolute URI path, that path is used directly after tokenization. If the target begins with
        "~", that character will be replaced with the route prefix. This is a very convenient way to create application
        top-level relative links.
        \n\n
        If the target is a string that begins with "{AT}" it will be interpreted as a controller/action pair of the
        form "{AT}controller/action". If the "controller/" portion is absent, the current controller is used. If
        the action component is missing, the "list" action is used. A bare "{AT}" refers to the "list" action
        of the current controller.
        \n\n
        If the target starts with "{" it is interpreted as being a JSON style set of options that describe the link.
        If the target is a relative URI path, it is appended to the current request URI path.
        \n\n
        If the target is a JSON style of options, it can specify the URI components: scheme, host, port, path, reference and
        query. If these component properties are supplied, these will be combined to create a URI.
        \n\n
        If the target specifies either a controller/action or a JSON set of options, The URI will be created according
        to the route URI template. The template may be explicitly specified
        via a "route" target property. Otherwise, if an "action" property is specified, the route of the same
        name will be used. If these don't result in a usable route, the "default" route will be used.
        \n\n
        These are the properties supported in a JSON style "{ ... }" target:
        <ul>
            <li>scheme String URI scheme portion</li>
            <li>host String URI host portion</li>
            <li>port Number URI port number</li>
            <li>path String URI path portion</li>
            <li>reference String URI path reference. Does not include "#"</li>
            <li>query String URI query parameters. Does not include "?"</li>
            <li>controller String controller name if using a controller-based route. This can also be specified via
                the action option.</li>
            <li>action String Action to invoke. This can be a URI string or a controller action of the form
                {AT}controller/action.</li>
            <li>route String Route name to use for the URI template</li>
        </ul>
    @return A normalized, server-local Uri string.
    @example espUri(stream, "http://example.com/index.html", 0); \n
    espUri(stream, "/path/to/index.html", 0); \n
    espUri(stream, "../images/splash.png", 0); \n
    espUri(stream, "~/client/images/splash.png", 0); \n
    espUri(stream, "${app}/client/images/splash.png", 0); \n
    espUri(stream, "@controller/checkout", 0); \n
    espUri(stream, "@controller/") \n
    espUri(stream, "@init") \n
    espUri(stream, "@") \n
    espUri(stream, "{ action: '@post/create' }", 0); \n
    espUri(stream, "{ action: 'checkout' }", 0); \n
    espUri(stream, "{ action: 'logout', controller: 'admin' }", 0); \n
    espUri(stream, "{ action: 'admin/logout'", 0); \n
    espUri(stream, "{ product: 'candy', quantity: '10', template: '/cart/${product}/${quantity}' }", 0); \n
    espUri(stream, "{ route: '~/STAR/edit', action: 'checkout', id: '99' }", 0); \n
    espUri(stream, "{ template: '~/client/images/${theme}/background.jpg', theme: 'blue' }", 0);
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *espUri(HttpStream *stream, cchar *target);


/************************************** Actions *******************************/
/**
    Action definition
    @stability Prototype
 */
typedef struct EspAction {
    cchar       *target;            /**< Route target string */
    cchar       *roles;             /**< Roles or abilities string for action */
    EspProc     callback;           /**< Callback action */
} EspAction;

#if DEPRECATED || 1
/**
    Define an action
    @description Actions are C procedures that are invoked when specific URIs are routed to the controller/action pair.
    This API is deprecated. Use #espAction instead.
    @param route HttpRoute object
    @param targetKey Target key used to select the action in a HttpRoute target. This is typically a URI prefix.
    @param actionProc EspProc callback procedure to invoke when the action is requested.
    @ingroup EspRoute
    @stability Deprecated
 */
PUBLIC void espDefineAction(HttpRoute *route, cchar *targetKey, EspProc actionProc) ME_DEPRECATED("Use espAction instead");
#endif

/**
    Define an action
    @description Actions are C procedures that are invoked when specific URIs are routed to the controller/action pair.
    The action will require the specified roles or abilities.
    @param route HttpRoute object
    @param targetKey Target key used to select the action in a HttpRoute target. This is typically a URI prefix.
    @param roles String Comma separated list of roles or abilities. If set to the empty string, no specific roles are required
        but an authenticated user is required. Set to NULL if an authenticated user is not required.
    @param actionProc EspProc callback procedure to invoke when the action is requested.
    @ingroup EspRoute
    @stability Prototype
 */
PUBLIC void espAction(HttpRoute *route, cchar *targetKey, cchar *roles, EspProc actionProc);

/***************************** Abbreviated Controls ***************************/
#if ME_ESP_ABBREV
/**
    Abbreviated ESP API.
    @description This is a short-form API that uses the current HttpStream stream object.
        These APIs are designed to be terse and highly readable. Consequently, they are not prefixed with "esp".
    @see espAlert
    @defgroup EspAbbrev EspAbbrev
    @stability Stable
  */
typedef struct EspAbbrev { int dummy; } EspAbbrev;

/******************************* Abbreviated API ******************************/

/**
    Create an absolute URI with a scheme and host
    @param target The URI target. See httpLink for details
    @param ... arguments to the formatted target string
    @return A normalized, absolute Uri string containing scheme and host.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *absuri(cchar *target, ...);

/**
    Add a header to the transmission using a format string.
    @description Add a header if it does not already exist.
    @param key Http response header key
    @param fmt Printf style formatted string to use as the header key value
    @param ... Arguments for fmt
    @return Zero if successful, otherwise a negative MPR error code. Returns MPR_ERR_ALREADY_EXISTS if the header already exists.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void addHeader(cchar *key, cchar *fmt, ...);

/**
    Add a request parameter value if not already defined.
    @param name Name of the request parameter to set
    @param value Value to set.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void addParam(cchar *name, cchar *value);

/**
    Test if a user has the required abilities
    @param abilities Comma separated list of abilities to test for. If null, then use the required abilities defined
        for the current request route.
    @param warn If true, warn the user via #sendResult.
    @return True if the user has all the required abilities
    @ingroup EspAbbrev
    @stability prototype
 */
PUBLIC bool canUser(cchar *abilities, bool warn);

/**
    Create a record and initialize field values
    @description This will call #ediCreateRec to create a record based on the table's schema. It will then
        call #setFields to update the record with the given data.
        The record is remembered for this request as the "current" record and can be retrieved via: getRec().
        The record is not written to the database. Use #updateRec to write to the database.
    @param tableName Database table name
    @param data Json object with field values
    @return EdRec instance
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *createRec(cchar *tableName, MprJson *data);

/**
    Create a record from the request parameters
    @description A new record is created with the request parameters in the specified table.
    The record is remembered for this request as the "current" record and can be retrieved via: getRec().
    @param table Database table to update
    @return True if the update is successful.
    @ingroup EspAbbrev
    @stability Prototype
*/
PUBLIC bool createRecByParams(cchar *table);

#if DEPRECATED || 1
/**
    Create a record from the request parameters
    @description A new record is created with the request parameters in the specified table.
    The record is remembered for this request as the "current" record and can be retrieved via: getRec().
    @param table Database table to update
    @return True if the update is successful.
    @ingroup EspAbbrev
    @stability Deprecated
*/
PUBLIC bool createRecFromParams(cchar *table) ME_DEPRECATED("Use updateRecFields(table, params(\"fields\") instead");
#endif

/**
    Create a session state object.
    @description The session state object can be used to share state between requests.
    If a session has not already been created, this call will create a new session.
    It will create a response cookie containing a session ID that will be sent to the client
    with the response. Note: Objects are stored in the session state using JSON serialization.
    @return Session ID string
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *createSession(void);

/**
    Destroy a session state object.
    @description This will emit an expired cookie to the client to force it to erase the session cookie.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void destroySession(void);

/**
    Don't auto-finalize this request
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void dontAutoFinalize(void);

/**
    Display the grid to the debug log
    @param message Prefix message to output
    @param grid EDI grid
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC void dumpGrid(cchar *message, EdiGrid *grid);

/**
    Display request parameters to the debug log
    @param message Prefix message to output
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC void dumpParams(cchar *message);

/**
    Display a record to the debug log
    @param message Prefix message to output
    @param rec Record to log
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC void dumpRec(cchar *message, EdiRec *rec);

/**
    Finalize the response.
    @description Signals the end of any and all response data and flushes any buffered write data to the client.
    If the request has already been finalized, this call has no additional effect.
    This routine calls #espFinalize.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void finalize(void);

/**
    Set a feedback message
    @description Feedback messages are a convenient way to aggregate messages state information in the response.
        The #getFeedback API can be used to retrieve feedback messages.
        Feedback messages are removed at the completion of the request.
    @param type type of feedback message. This may be set to any word, but the following feedback types
        are typically supported as per RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @param fmt Printf style formatted string to use as the message
    @return True if the request has been successful so far, i.e. there is not an error feedback message defined.
        Return false if there is an error feedback defined.
        This permits feedback to be chained as: sendResult(feedback("error", ...));
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool feedback(cchar *type, cchar *fmt, ...);

/**
    Build an EDI selection query from the request parameters for use by SPA applications.
    @description This call creates an EDI "SQL style" query from the request parameters.
        This call expects optional "fields" and "options" parameters with options.offset, options.limit and options.filter parameters. It examines each of the "fields" parameters to build an SQL "WHERE" expression testing the value of each field. The resulting expression looks like:
    \n\n
        field OP value AND field OP value .... LIMIT offset, limit
    @return An EDI sql style selection query string suitable for use with #findRec and #findGrid
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC cchar *findParams();

/**
    Flush transmit data.
    @description This writes any buffered data.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void flush(void);

/**
    Get the auth object for the current route
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC HttpAuth *getAuth(void);

/**
    Get a list of column names.
    @param rec Database record.
    @return An MprList of column names in the given table. If there is no record defined, an empty list is returned.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprList *getColumns(EdiRec *rec);

/**
    Get the request cookies
    @description Get the cookies defined in the current request.
    @return Return a string containing the cookies sent in the Http header of the last request.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getCookies(void);

/**
    Get the HttpStream object
    @description Before a view or controller is run, the current stream object for the request is saved in thread
    local data. Most EspAbbrev APIs take an HttpStream object as an argument.
    @return HttpStream stream instance object.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC HttpStream *getStream(void);

#if ME_COMPAT
/*
    LEGACY redefinitions
 */
#define getConn() getStream()
#define setConn(stream) setStream(stream)
#endif

/**
    Get the receive body content length
    @description Get the length of the receive body content (if any). This is used in servers to get the length of posted
        data and in clients to get the response body length.
    @return A count of the response content data in bytes.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprOff getContentLength(void);

/**
    Get the receive body content type
    @description Get the content mime type of the receive body content (if any).
    @return Mime type of any receive content. Set to NULL if not posted data.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getContentType(void);

/**
    Get the private data reference for the current request set via #setData
    @return Reference to private data
    @ingroup EspAbbrev
    @stability prototype
 */
PUBLIC void *getData(void);

/**
    Get the stream dispatcher object
    @return MprDispatcher stream dispatcher instance object.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprDispatcher *getDispatcher(void);

/**
    Get a feedback message defined via #feedback
    @param type type of feedback message to retrieve. This may be set to any word, but the following feedback types
        are typically supported as per RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @return Reference to private data
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getFeedback(cchar *type);

/**
    Get the current database instance
    @description A route may have a default database configured via the EspDb Appweb.conf configuration directive.
    The database will be opened when the web server initializes and will be shared between all requests using the route.
    @return Edi EDI database handle
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC Edi *getDatabase(void);

/**
    Get the extended route EspRoute structure
    @return EspRoute instance
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EspRoute *getEspRoute(void);

/**
    Get the default document root directory for the request route.
    @return A directory path name
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getDocuments(void);

/**
    Get a field from the current database record
    @param rec Database record.
    @param field Field name to return
    @return String value for "field" in the current record.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getField(EdiRec *rec, cchar *field);

/**
    Get the current database grid
    @description The current grid is defined via #setGrid
    @return EdiGrid instance
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiGrid *getGrid(void);

/**
    Get an rx http header.
    @description Get a http response header for a given header key.
    @param key Name of the header to retrieve. This should be a lower case header name. For example: "Connection".
    @return Value associated with the header key or null if the key did not exist in the response.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getHeader(cchar *key);

/**
    Get the HTTP method
    @description This is a convenience API to return the Http method
    @return The HttpStream.rx.method property
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *getMethod(void);

/**
    Get the HTTP URI query string
    @description This is a convenience API to return the query string for the current request.
    @return The espGetStream()->rx->parsedUri->query property
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getQuery(void);

/**
    Get the referring URI
    @description This returns the referring URI as described in the HTTP "referer" (yes the HTTP specification does
        spell it incorrectly) header. If this header is not defined, this routine will return the home URI as returned
        by uri("~").
    @return String URI back to the referring URI. If no referrer is defined, refers to the home URI.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getReferrer(void);

/**
    Get the ESP request object
    @return EspReq request instance object.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EspReq *getReq(void);

/**
    Get the HttpRoute object for the current route
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC HttpRoute *getRoute(void);

/**
    Get the security token.
    @description To minimize form replay attacks, a security token may be required for POST requests on a route.
    Client-side Javascript must then send this token as a request header in subsquent POST requests.
    To configure a route to require security tokens, call #httpSetRouteXsrf.
    @return the security token.
    @ingroup EspAbbrev
    @stability Evolving
*/
PUBLIC cchar *getSecurityToken(void);

/**
    Get a session state variable
    @description The #session API is an alias for this routine.
    @param name Variable name to get
    @return The session variable value. Returns NULL if not set.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getSessionVar(cchar *name);

/**
    Get the session state ID.
    @description This will get a session and return the session ID. This will create a new session state storage area if
        one does not already exist.
    @return The session state identifier string.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getSessionID(void);

/**
    Test if a field in the current record has input validation errors
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC cchar *getFieldError(cchar *field);

/**
    Get the request URI path string
    @description This is a convenience API to return the request URI path. This is the portion after the application/route
        prefix.
    @return The espGetStream()->rx->pathInfo
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getPath(void);

/**
    Get the current database record
    @return EdiRec instance
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *getRec(void);

/**
    Get a field from the application pak.json configuration
    @param field Property field name in pak.json. May contain dots.
    @return The field value. Returns "" if the field is not found.
    @ingroup EspAbbrev
    @stability deprecated
 */
PUBLIC cchar *getConfig(cchar *field);

/**
    Get the uploaded files
    @description Get the list of uploaded files.
    @return A list of HttpUploadFile objects.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprList *getUploads(void);

/**
    Get the request URI string
    @description This is a convenience API to return the request URI.
    @return The espGetStream()->rx->uri
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getUri(void);

/**
    Test if a current grid has been defined
    @return "true" if a current grid has been defined
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool hasGrid(void);

/**
    Test if a current record has been defined and save to the database
    @description This call returns "true" if a current record is defined and has been saved to the database with a
        valid "id" field.
    @return "true" if a current record with a valid "id" is defined.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool hasRec(void);

/**
    Render an input field as part of a form. This is a smart input control that will call the appropriate
        input control based on the database record field data type. This control should not be used
        if using the esp-vue-mvc or other similar client-side Javascript framework.
    @param field Name for the input field. This defines the HTML element name and provides the source
        of the initial value to display. The field should be a property of the form current record.
        If this call is used without a form control record, the actual data value should be supplied via the
        options.value property.
    @param options These are in JSON string form and are converted to attributes to pass to the input element
    @arg noescape Boolean Do not HTML escape the text before rendering.
    @arg ... Other options are converted and rendered as HTML attributes.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void input(cchar *field, cchar *options);

/**
    Render an input field with a hidden XSRF security token.
    @description Security tokens are used to help guard against CSRF threats.
    This call will generate a hidden input field that includes the CSRF security token for the form.
    This call should not be included in SPA client applications as the SPA framework should automatically
    handle the security token.
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC void inputSecurityToken(void);

/**
    Get an integer request parameter
    @description Get the value of a named request parameter. Request parameters are defined via www-urlencoded
        query or post data contained in the request. This routine calls #espGetParam
    @param name Name of the request parameter to retrieve
    @return Integer containing the request parameter's value. Returns zero if not found.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC int paramInt(cchar *name);
#define intParam paramInt

/**
    Test if the user is authenticated
    @return True if the username and password have been authenticated.
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC bool isAuthenticated(void);

/**
    Test if the receive input stream is at end-of-file
    @return "true" if there is no more receive data to read
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool isEof(void);

/**
    Test if a http request is finalized.
    @description This tests if #espFinalize or #httpFinalize has been called for a request.
    @return "true" if the request has been finalized.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool isFinalized(void);

/**
    Test if the stream is using SSL and is secure
    @return "true" if the stream is using SSL.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool isSecure(void);

/**
    Make a hash table container of property values
    @description This routine formats the given arguments, parses the result as a JSON string and returns an
        equivalent hash of property values. The result after formatting should be of the form:
        hash("{ key: 'value', key2: 'value', key3: 'value' }");
    @param fmt Printf style format string
    @param ... arguments
    @return MprHash instance
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprHash *makeHash(cchar *fmt, ...);

/**
    Make a JSON object container of property values
    @description This routine formats the given arguments, parses the result into a JSON object.
    @param fmt Printf style format string
    @param ... arguments
    @return MprJson instance
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprJson *makeJson(cchar *fmt, ...);

/**
    Make a free-standing record
    @description This call makes a free-standing data record based on the JSON format content string.
        The record is not saved to the database.
    @param content JSON format content string. The content should be a set of property names and values.
    @return An EdiRec instance
    @example: rec = ediMakeRec("{ id: 1, title: 'Message One', body: 'Line one' }");
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *makeRec(cchar *content);

/**
    Create a URI.
    @description Create a URI link by expansions tokens based on the current request and route state.
    The target parameter may contain partial or complete URI information. The missing parts
    are supplied using the current request and route tables. The resulting URI is a normalized, server-local
    URI (that begins with "/"). The URI will include any defined route prefix, but will not include scheme, host or
    port components.
    @param target The URI target. The target parameter can be a URI string or JSON style set of options.
        The target will have any embedded "{tokens}" expanded by using token values from the request parameters.
        If the target has an absolute URI path, that path is used directly after tokenization. If the target begins with
        "~", that character will be replaced with the route prefix. This is a very convenient way to create application
        top-level relative links.
        \n\n
        If the target is a string that begins with "{AT}" it will be interpreted as a controller/action pair of the
        form "{AT}controller/action". If the "controller/" portion is absent, the current controller is used. If
        the action component is missing, the "list" action is used. A bare "{AT}" refers to the "list" action
        of the current controller.
        \n\n
        If the target starts with "{" it is interpreted as being a JSON style set of options that describe the link.
        If the target is a relative URI path, it is appended to the current request URI path.
        \n\n
        If the target is a JSON style of options, it can specify the URI components: scheme, host, port, path, reference and
        query. If these component properties are supplied, these will be combined to create a URI.
        \n\n
        If the target specifies either a controller/action or a JSON set of options, The URI will be created according
        to the route URI template. The template may be explicitly specified
        via a "route" target property. Otherwise, if an "action" property is specified, the route of the same
        name will be used. If these don't result in a usable route, the "default" route will be used.
        \n\n
        These are the properties supported in a JSON style "{ ... }" target:
        <ul>
            <li>scheme String URI scheme portion</li>
            <li>host String URI host portion</li>
            <li>port Number URI port number</li>
            <li>path String URI path portion</li>
            <li>reference String URI path reference. Does not include "#"</li>
            <li>query String URI query parameters. Does not include "?"</li>
            <li>controller String controller name if using a controller-based route. This can also be specified via
                the action option.</li>
            <li>action String Action to invoke. This can be a URI string or a controller action of the form
                {AT}controller/action.</li>
            <li>route String Route name to use for the URI template</li>
        </ul>
    @return A normalized, server-local Uri string.
    @example makeUri("http://example.com/index.html", 0); \n
    makeUri("/path/to/index.html", 0); \n
    makeUri("../images/splash.png", 0); \n
    makeUri("~/client/images/splash.png", 0); \n
    makeUri("${app}/client/images/splash.png", 0); \n
    makeUri("@controller/checkout", 0); \n
    makeUri("@controller/") \n
    makeUri("@init") \n
    makeUri("@") \n
    makeUri("{ action: '@post/create' }", 0); \n
    makeUri("{ action: 'checkout' }", 0); \n
    makeUri("{ action: 'logout', controller: 'admin' }", 0); \n
    makeUri("{ action: 'admin/logout'", 0); \n
    makeUri("{ product: 'candy', quantity: '10', template: '/cart/${product}/${quantity}' }", 0); \n
    makeUri("{ route: '~/STAR/edit', action: 'checkout', id: '99' }", 0); \n
    makeUri("{ template: '~/client/images/${theme}/background.jpg', theme: 'blue' }", 0);
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *makeUri(cchar *target);

/**
    Get an MD5 checksum
    @param str String to hash
    @returns An allocated MD5 checksum string.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *md5(cchar *str);

/**
    Generate a onetime random string
    @returns An MD5 encoded random string
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *nonce(void);

/**
    Test the the application mode
    @description This is typically set to "debug" or "release". The mode is defined by the "profile" property in the pak.json.
    @param check Mode to compare with the current application mode.
    @return True if the current app mode matches the check mode
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC bool modeIs(cchar *check);

/**
    Get a request parameter
    @description Get the value of a named request parameter. Request parameters are defined via www-urlencoded
        query or post data contained in the request. This routine calls #espGetParam.
    @param name Name of the request parameter to retrieve
    @return String containing the request parameter's value. Caller should not free.
        Returns NULL if the parameter is not defined.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *param(cchar *name);

/**
    Get a collection of request parameters
    @description This call gets request parameters for a given variable root.
        Route tokens, request query data, and www-url encoded form data are all entered into the request parameters
    @param var Root property of the params collection. Set to NULL for the root collection.
    @return MprJson instance containing the request parameters
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprJson *params(cchar *var);

/**
    Read matching records in table from the database
    @description This reads a table and returns a grid containing the table data.
    The grid of records is remembered for this request as the "current" grid and can be retrieved via: getGrid().
    @param tableName Database table name
    @param select Selection format string. This is a printf style format string. This will contain a select criteria typically
        of the form: "Field Op Value AND field OP value ...". All fields may be matched by using the pseudo column name "*".
        OP is "==", "!=", "<", ">", "<=", ">=" or "><".
    @return A grid containing all table rows. Returns NULL if the table cannot be found.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiGrid *findGrid(cchar *tableName, cchar *select);

/**
    Read a record identified by SQL style query expression
    @description Read a record from the given table as described by the selection criteria.
    The record is remembered for this request as the "current" record and can be retrieved via: getRec().
    @param tableName Database table name
    @param query SQL like query expression. This arg is a printf style format string. When expanded, this will contain
        a SQL style query expression of the form: "Field Op Value AND field OP value ... LIMIT offset, limit".
        All fields may be matched by using the pseudo column name "*". OP is "==", "!=", "<", ">", "<=", ">=" or "><".
    @return Record instance of EdiRec.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *findRec(cchar *tableName, cchar *query);

/**
    Read a record identified by key value
    @description Read a record from the given table as identified by the key value.
    The record is remembered for this request as the "current" record and can be retrieved via: getRec().
    @param tableName Database table name
    @param key Key value of the record to read
    @return Record instance of EdiRec.
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC EdiRec *readRec(cchar *tableName, cchar *key);

#if DEPRECATED || 1
/**
    Read matching records
    @description This runs a simple query on the database and returns matching records in a grid. The query selects
        all rows that have a "field" that matches the given "value".
    The grid of records is remembered for this request as the "current" grid and can be retrieved via: getGrid().
    @param tableName Database table name
    @param fieldName Database field name to evaluate
    @param operation Comparison operation. Set to "==", "!=", "<", ">", "<=" or ">=".
    @param value Data value to compare with the field values.
    @return A grid containing all matching records. Returns NULL if no matching records.
    @ingroup EspAbbrev
    @stability Deprecated
 */
PUBLIC EdiGrid *readWhere(cchar *tableName, cchar *fieldName, cchar *operation, cchar *value) ME_DEPRECATED("Use findGrid instead");

/**
    Read one record
    @description This runs a simple query on the database and selects the first matching record. The query selects
        a row that has a "field" that matches the given "value".
    The record is remembered for this request as the "current" record and can be retrieved via: getRec().
    @param tableName Database table name
    @param fieldName Database field name to evaluate
    @param operation Comparison operation. Set to "==", "!=", "<", ">", "<=" or ">=".
    @param value Data value to compare with the field values.
    @return First matching record. Returns NULL if no matching records.
    @ingroup EspAbbrev
    @stability Deprecated
 */
PUBLIC EdiRec *findRecWhere(cchar *tableName, cchar *fieldName, cchar *operation, cchar *value) ME_DEPRECATED("Use findRec instead");

/**
    Read all the records in table from the database
    @description This reads a table and returns a grid containing the table data.
    The grid of records is remembered for this request as the "current" grid and can be retrieved via: getGrid().
    @param tableName Database table name
    @return A grid containing all table rows. Returns NULL if the table cannot be found.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiGrid *readTable(cchar *tableName) ME_DEPRECATED("Use findGrid instead");
#endif

/**
    Read receive body content
    @description Read body content from the client. This will not block by default.
        Use httpReadBlock for more options to read data.
    @param buf Buffer to accept content data
    @param size Size of the buffer
    @return A count of bytes read into the buffer
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize receive(char *buf, ssize size);

/**
    Redirect the client
    @description Redirect the client to a new uri. This will redirect with an HTTP 302 status. If a different HTTP status
    code is required, use #espRedirect.
    @param target New target uri for the client
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void redirect(cchar *target);

/**
    Redirect the client back to the referrer
    @description Redirect the client to the referring URI.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void redirectBack(void);

/**
    Remove a cookie
    @param name Cookie name
    @ingroup EspAbbrev
    @stability Evolving
*/
PUBLIC void removeCookie(cchar *name);

#if KEEP
/**
    Remove a record from a database table
    @description Remove the record identified by the query expression.
        As a sideeffect, if the removal succeeds, the feedback message {inform: "Deleted Record"} will be created.
        If the removal fails, a feedback message {error: "Cannot delete Record"} will be created.
    @param tableName Database table name
    @param query SQL like query expression. This arg is a printf style format string. When expanded, this will contain
        a SQL style query expression of the form: "Field Op Value AND field OP value ... LIMIT offset, limit".
        All fields may be matched by using the pseudo column name "*". OP is "==", "!=", "<", ">", "<=", ">=" or "><".
    @return True if the removal succeeds, otherwise false.
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC bool removeRec(cchar *tableName, cchar *query);
#endif

/**
    Remove a record from a database table
    @description Remove the record identified by the key value from the given table.
        If the removal succeeds, the feedback message {inform: "Deleted Record"} will be created. If the removal fails,
        a feedback message {error: "Cannot delete Record"} will be created.
    @param tableName Database table name
    @param key Record key value.
    @return True if the removal succeeds, otherwise false.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool removeRec(cchar *tableName, cchar *key);

/**
    Remove a session state variable
    @param name Variable name to set
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC void removeSessionVar(cchar *name);

/**
    Render a formatted string
    @description Render a formatted string of data into packets to the client. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client.
    @param fmt Printf style formatted string
    @param ... Arguments for fmt
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize render(cchar *fmt, ...);

/**
    Render cached content
    @description Render the saved, cached response from a prior request to this URI. This is useful if the caching
        mode has been set to "manual".
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize renderCached(void);

/**
    Render the pak.json
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC ssize renderConfig(void);

/**
    Render an error message back to the client and finalize the request. The output is Html escaped for security.
    @param status Http status code
    @param fmt Printf style message format
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void renderError(int status, cchar *fmt, ...);

/**
    Render feedback messages.
    @description Feedback notices are one-time messages that are passed to the next request (only).
        See #espSetFeedback and #feedback for how to define feedback messages.
        This API will render feedback messages as HTML in place of the renderFeedback call in ESP page.
    @param types Types of feedback message to retrieve. Set to "*" to retrieve all types of feedback.
        This may be set to any word, but the following feedback types are typically supported as per
        RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void renderFeedback(cchar *types);

/**
    Render a file back to the client
    @description Render a formatted string of data and then HTML escape. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client.
    @param path Filename of the file to send to the client.
    @param ... Arguments for fmt
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize renderFile(cchar *path);

/**
    Render a formatted string after HTML escaping
    @description Render a formatted string of data and then HTML escape. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client.
    @param fmt Printf style formatted string
    @param ... Arguments for fmt
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize renderSafe(cchar *fmt, ...);

/**
    Render a string of data to the client
    @description Render a string of data to the client. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client.
    @param s String containing the data to write
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize renderString(cchar *s);

/**
    Render the value of a request variable to the client.
    If a request parameter is not found by the given name, consult the session store for a variable the same name.
    @description This writes the value of a request variable after HTML escaping its value.
    @param name Request parameter variable name
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize renderVar(cchar *name);

/**
    Render an ESP page to the client
    @param view View name. The view name is interpreted relative to the matching route documents directory and may omit
        an ESP extension.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void renderView(cchar *view);

/**
    Run a command
    @description Run a command and return output.
    @param command Command line and arguments to run.
    @param input Input data to pass to the command. Set to null if not required.
    @param output Pointer to accept command standard output response. Set to null if not required.
    @param error Pointer to accept command standard error response. Set to null if not required.
    @param flags MprCmd flags. Use MPR_CMD_DETACH to run in the background.
    @param timeout Time in milliseconds to wait for the command to complete and exit.
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC int runCmd(cchar *command, char *input, char **output, char **error, MprTicks timeout, int flags);

#if DEPRECATED && REMOVE
/**
    Render scripts
    @description This renders script elements for all matching filenames on the server.
    @param patterns An enhanced glob-style expression pattern. The format is is a comma separated string of filename
    expressions. Each expression may contain the wildcard tokens: "*" which matches any filename portion, "**" which matches
    any filename portion in any subdirectory. An expression may be prefixed with "!" to exclude files of that expression.
    @ingroup EspAbbrev
    @stability Deprecated
 */
PUBLIC void scripts(cchar *patterns);
#endif

/**
    Send a database grid as a JSON string to the request client
    @description The JSON string is rendered as part of an enclosing "{ data: JSON, schema: schema }" wrapper.
    This API is used to send database data to clients.
    @param grid EDI grid
    @return Number of bytes sent
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC ssize sendGrid(EdiGrid *grid);

/**
    Send a database record as a JSON string
    @description The JSON string is rendered as part of an enclosing "{ data: JSON }" wrapper.
    This API is used to send database data to client user interfaces such as VueJS or Aurelia clients.
    @param rec EDI record
    @return Number of bytes sent
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC ssize sendRec(EdiRec *rec);

/**
    Send a JSON response result
    @description This sends a JSON response including the request success status, feedback message and field errors.
    This API is used to send controller action responses to client user interfaces such as VueJS or Aurelia clients.
    The field errors apply to the current EDI record.
    The format of the response is:
        "{ success: STATUS, feedback: {messages}, fieldErrors: {messages}}" wrapper.
    The feedback messages are created via the espSetFeedback API. Field errors are created by ESP validations.
    @param status Request success status. Note: this is not the HTTP response status code.
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC void sendResult(bool status);

#if DEPRECATED && REMOVE
/**
    Render stylesheets
    @description This renders stylesheet elements for all matching filenames on the server.
    @param patterns An enhanced glob-style expression pattern. The format is is a comma separated string of filename
    expressions. Each expression may contain the wildcard tokens: "*" which matches any filename portion, "**" which matches
    any filename portion in any subdirectory. An expression may be prefixed with "!" to exclude files of that expression.
    @ingroup EspAbbrev
    @stability Deprecated
 */
PUBLIC void stylesheets(cchar *patterns);
#endif

/**
    Add the security token to the response.
    @description To minimize form replay attacks, a security token may be required for POST requests on a route.
    This call will set a security token in the response as a response header and as a response cookie.
    Client-side Javascript must then send this token as a request header in subsquent POST requests.
    To configure a route to require security tokens, call #httpSetRouteXsrf.
    @ingroup EspAbbrev
    @stability Evolving
*/
PUBLIC void securityToken(void);

/**
    Get a session state variable
    @description This is a convenient alias for #getSessionVar.
    @param name Variable name to get
    @return The session variable value. Returns NULL if not set.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *session(cchar *name);

/**
    Define a cookie header to send with the response. The Path, Domain, and Expires properties can be set to null for
    default values.
    @param name Cookie name
    @param value Cookie value
    @param path Uri path to which the cookie applies
    @param domain String Domain in which the cookie applies. Must have 2-3 "." and begin with a leading ".".
        For example: domain: .example.com
        Some browsers will accept cookies without the initial ".", but the spec: (RFC 2109) requires it.
    @param lifespan Lifespan of the cookie in seconds.
    @param isSecure Boolean Set to "true" if the cookie only applies for SSL based connections.
    @ingroup EspAbbrev
    @stability Evolving
*/
PUBLIC void setCookie(cchar *name, cchar *value, cchar *path, cchar *domain, MprTicks lifespan, bool isSecure);

/**
    Set the current request stream.
    @param stream The HttpStream stream object to define
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setStream(HttpStream *stream);

/**
    Set the transmission (response) content mime type
    @description Set the mime type Http header in the transmission
    @param mimeType Mime type string
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setContentType(cchar *mimeType);

/**
    Set a private data reference for the current request
    @return Reference to private data
    @ingroup EspAbbrev
    @stability prototype
 */
PUBLIC void setData(void *data);

/**
    Update a record field without writing to the database
    @description This routine updates the record object with the given value. The record will not be written
        to the database. To write to the database, use #updateRec
    @param rec Record to update
    @param fieldName Record field name to update
    @param value Value to update
    @return The record instance if successful, otherwise NULL.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *setField(EdiRec *rec, cchar *fieldName, cchar *value);

/**
    Update record fields without writing to the database
    @description This routine updates the record object with the given values. The "data' argument supplies
        a hash of fieldNames and values. The "data' argument supplies the fieldNames and values as a JSON object. The data
    may come from the request #params or it can be manually created via makeJson to convert a JSON
    string into an options hash. For example: ediWriteFields(rec, params());
    The record runs field validations before saving to the database.
    @param rec Record to update
    @param data Json object of field data.
    @return The record instance if successful, otherwise NULL.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *setFields(EdiRec *rec, MprJson *data);

/**
    Set the current database grid reference.
    @description This sets the current database which is used by many APIs that operate on the current grid.
    @return The grid instance. This permits chaining.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiGrid *setGrid(EdiGrid *grid);

/**
    Set a transmission header
    @description Set a Http header to send with the request. If the header already exists, its value is overwritten.
    @param key Http response header key
    @param fmt Printf style formatted string to use as the header key value
    @param ... Arguments for fmt
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setHeader(cchar *key, cchar *fmt, ...);

/**
    Set an integer request parameter value
    @description Set the value of a named request parameter to an integer value. Request parameters are defined via
        www-urlencoded query or post data contained in the request.
    @param name Name of the request parameter to set
    @param value Integer value to set.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setParamInt(cchar *name, int value);
#define setIntParam setParamInt

/**
    Set a notifier callback for the stream.
    This wraps the streamNotifier and calls espSetStream before invoking the notifier for stream events.
    @param notifier Callback function
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setNotifier(HttpNotifier notifier);

/**
    Set a request parameter value
    @description Set the value of a named request parameter to a string value. Parameters are defined via
        requeset POST data or request URI queries. This API permits these initial request parameters to be set or
        modified.
    @param name Name of the request parameter to set
    @param value Value to set.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setParam(cchar *name, cchar *value);

/**
    Set the current database record
    @description The current record is used to supply data to various abbreviated controls, such as: text(), input(),
        checkbox and dropdown()
    @return The grid instance. This permits chaining.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *setRec(EdiRec *rec);

/**
    Set a session state variable
    @param name Variable name to set
    @param value Value to set
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setSessionVar(cchar *name, cchar *value);

/**
    Set a Http response status.
    @description Set the Http response status for the request. This defaults to 200 (OK).
    @param status Http status code.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setStatus(int status);

/**
    Create a timeout event
    @description invoke the given procedure after the timeout
    @param proc Function to invoke
    @param timeout Time in milliseconds to elapse before invoking the timeout
    @param data Argument to pass to proc
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setTimeout(void *proc, MprTicks timeout, void *data);

/**
    Show request details
    @description This echoes request details back to the client. This is useful as a debugging tool.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void showRequest(void);

//  FUTURE - document
PUBLIC EdiGrid *sortGrid(EdiGrid *grid, cchar *sortColumn, int sortOrder);

/**
    Update the cached content for a request
    @description Save the given content for future requests. This is useful if the caching mode has been set to "manual".
    @param uri Request URI to cache for
    @param data Data to cache
    @param lifesecs Time in seconds to cache the data
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void updateCache(cchar *uri, cchar *data, int lifesecs);

/**
    Write a value to a database table field
    @description Update the value of a table field in the selected table row. Note: validations are not run.
    @param tableName Database table name
    @param key Key value for the table row to update.
    @param fieldName Column name to update
    @param value Value to write to the database field
    @return "true" if the field  can be successfully written.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool updateField(cchar *tableName, cchar *key, cchar *fieldName, cchar *value);

/**
    Write field values to a database row
    @description This routine updates the current record with the given data. The "data' argument supplies the
        fieldNames and values as a JSON object. The data
        may come from the request #params or it can be manually created via makeJson to convert a JSON
        string into an options hash. For example: ediWriteFields(rec, params());
    @param tableName Database table name
    @param data Json object of fields to update
    @return "true" if the field  can be successfully written. Returns false if field validations fail.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool updateFields(cchar *tableName, MprJson *data);

/**
    Save a record to the database
    @description The record will be saved to the database after running any field validations. If any field validations
        fail to pass, the record will not be written and error details can be retrieved via #ediGetRecErrors.
        If the record is a new record and the "id" column is EDI_AUTO_INC, then the "id" will be assigned
        prior to saving the record.
        If the update succeeds, the feedback message {inform: "Saved Record"} will be created. If the update fails,
        a feedback message {error: "Cannot save Record"} will be created.
    @param rec Record to write to the database.
    @return "true" if the record can be successfully written.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool updateRec(EdiRec *rec);

#if UNUSED
/**
    Update a record from the request parameters
    @description The record identified by the params(id) is read and updated with the request parameters. The record is then
        saved to the database via #updateRec.
    @param table Database table to update
    @param data Data as a JSON object to apply to the record. This may come from the #params() API or from #makeJson.
    @return True if the update is successful.
    @ingroup EspAbbrev
    @stability Prototype
*/
PUBLIC bool updateRecFields(cchar *table, MprJson *data);
#endif

#if DEPRECATED || 1
/**
    Update a record from the request parameters
    @description The record identified by the params(id) is read and updated with the request parameters.
    @param table Database table to update
    @return True if the update is successful.
    @ingroup EspAbbrev
    @stability Deprecated
*/
PUBLIC bool updateRecFromParams(cchar *table) ME_DEPRECATED("Use updateRecFields instead");
#endif

/**
    Create a URI link.
    @description Create a URI link based on a given target an expanding embedded tokens based on the current request and
        route state. The target URI parameter may contain partial or complete URI information. The missing parts
    are supplied using the current request and route tables.
    @param target The URI target. The target parameter can be a URI string or JSON style set of options.
        The target will have any embedded "{tokens}" expanded by using token values from the request parameters.
        If the target has an absolute URI path, that path is used directly after tokenization. If the target begins with
        "~", that character will be replaced with the route prefix. This is a very convenient way to create application
        top-level relative links.
        \n\n
        If the target is a string that begins with "{AT}" it will be interpreted as a service/action pair of the
        form "{AT}Service/action". If the "service/" portion is absent, the current service is used. If
        the action component is missing, the "list" action is used. A bare "{AT}" refers to the "list" action
        of the current service.
        \n\n
        If the target starts with "{" it is interpreted as being a JSON style set of options that describe the link.
        If the target is a relative URI path, it is appended to the current request URI path.
        \n\n
        If the is a JSON style of options, it can specify the URI components: scheme, host, port, path, reference and
        query. If these component properties are supplied, these will be combined to create a URI.
        \n\n
        If the target specifies either a service/action or a JSON set of options, The URI will be created according
        to the route URI template. The template may be explicitly specified
        via a "route" target property. Otherwise, if an "action" property is specified, the route of the same
        name will be used. If these don't result in a usable route, the "default" route will be used.
        \n\n
        These are the properties supported in a JSON style "{ ... }" target:
        <ul>
            <li>scheme String URI scheme portion</li>
            <li>host String URI host portion</li>
            <li>port Number URI port number</li>
            <li>path String URI path portion</li>
            <li>reference String URI path reference. Does not include "#"</li>
            <li>query String URI query parameters. Does not include "?"</li>
            <li>service String Service name if using a Service-based route. This can also be specified via
                the action option.</li>
            <li>action String Action to invoke. This can be a URI string or a Service action of the form
                {AT}Service/action.</li>
            <li>route String Route name to use for the URI template</li>
        </ul>
    @param ... arguments to the formatted target string
    @return A normalized Uri string.
    @ingroup EspAbbrev
    @stability Evolving
    @remarks Examples:<pre>
    uri("http://example.com/index.html");
    uri("/path/to/index.html");
    uri("../images/splash.png");
    uri("~/static/images/splash.png");
    uri("${app}/static/images/splash.png");
    uri("@service/checkout");
    uri("@service/")               //  Service = Service, action = index
    uri("@init")                   //  Current service, action = init
    uri("@")                       //  Current service, action = index
    uri("{ action: '@post/create' }");
    uri("{ action: 'checkout' }");
    uri("{ action: 'logout', service: 'admin' }");
    uri("{ action: 'admin/logout'");
    uri("{ product: 'candy', quantity: '10', template: '/cart/${product}/${quantity}' }");
    uri("{ route: '~/STAR/edit', action: 'checkout', id: '99' }");
    uri("{ template: '~/static/images/${theme}/background.jpg', theme: 'blue' }");
</pre>
 */
PUBLIC cchar *uri(cchar *target, ...);

#endif /* ME_ESP_ABBREV */
/*
    LEGACY redefines
 */
#define espGetConn espGetStream
#define espSetConn espSetStream

#if DEPRECATED && REMOVE
#define espGetFlash(stream, type) espGetFeedback(stream, type)
#define espRenderFlash(stream, types) espRenderFeedback(stream, types)
#define espSetFlashv(stream, type, fmt, args) espSetFeedbackv(stream, type, fmt, args)
#define getFlash(type) getFeedback(type)
#define renderFlash(types) renderFeedback(types)
PUBLIC void espSetFlash(HttpStream *stream, cchar *type, cchar *fmt, ...);
PUBLIC void flash(cchar *type, cchar *fmt, ...);
#endif /* DEPRECATED */

#ifdef __cplusplus
} /* extern C */
#endif
#endif /* _h_ESP */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
