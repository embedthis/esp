/*
 * Embedthis Http Library Source 9.0.1
 */

#include "http.h"


/********* Start of file src/service.c ************/

/*
    service.c -- Http service. Includes timer for expired requests.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/********************************** Locals ************************************/
/*
    Public singleton
 */
#undef HTTP
PUBLIC Http *HTTP;

/**
    Standard HTTP error code table
 */
typedef struct HttpStatusCode {
    int     code;                           /**< Http error code */
    char    *codeString;                    /**< Code as a string (for hashing) */
    char    *msg;                           /**< Error message */
} HttpStatusCode;


PUBLIC HttpStatusCode HttpStatusCodes[] = {
    { 100, "100", "Continue" },
    { 101, "101", "Switching Protocols" },
    { 200, "200", "OK" },
    { 201, "201", "Created" },
    { 202, "202", "Accepted" },
    { 204, "204", "No Content" },
    { 205, "205", "Reset Content" },
    { 206, "206", "Partial Content" },
    { 301, "301", "Moved Permanently" },
    { 302, "302", "Moved Temporarily" },
    { 304, "304", "Not Modified" },
    { 305, "305", "Use Proxy" },
    { 307, "307", "Temporary Redirect" },
    { 400, "400", "Bad Request" },
    { 401, "401", "Unauthorized" },
    { 402, "402", "Payment Required" },
    { 403, "403", "Forbidden" },
    { 404, "404", "Not Found" },
    { 405, "405", "Method Not Allowed" },
    { 406, "406", "Not Acceptable" },
    { 408, "408", "Request Timeout" },
    { 409, "409", "Conflict" },
    { 410, "410", "Gone" },
    { 411, "411", "Length Required" },
    { 412, "412", "Precondition Failed" },
    { 413, "413", "Request Entity Too Large" },
    { 414, "414", "Request-URI Too Large" },
    { 415, "415", "Unsupported Media Type" },
    { 416, "416", "Requested Range Not Satisfiable" },
    { 417, "417", "Expectation Failed" },
    { 500, "500", "Internal Server Error" },
    { 501, "501", "Not Implemented" },
    { 502, "502", "Bad Gateway" },
    { 503, "503", "Service Unavailable" },
    { 504, "504", "Gateway Timeout" },
    { 505, "505", "Http Version Not Supported" },
    { 507, "507", "Insufficient Storage" },

    /*
        Proprietary codes (used internally) when connection to client is severed
     */
    { 550, "550", "Comms Error" },
    { 551, "551", "General Client Error" },
    { 0,   0 }
};

/****************************** Forward Declarations **************************/

static void httpTimer(Http *http, MprEvent *event);
static bool isHttpServiceIdle(bool traceRequests);
static void manageHttp(Http *http, int flags);
static void terminateHttp(int state, int how, int status);
static void updateCurrentDate(void);

/*********************************** Code *************************************/

PUBLIC Http *httpCreate(int flags)
{
    Http            *http;
    HttpStatusCode  *code;

    mprGlobalLock();
    if (MPR->httpService) {
        mprGlobalUnlock();
        return MPR->httpService;
    }
    if ((http = mprAllocObj(Http, manageHttp)) == 0) {
        mprGlobalUnlock();
        return 0;
    }
    MPR->httpService = HTTP = http;
    http->software = sclone(ME_HTTP_SOFTWARE);
    http->mutex = mprCreateLock();
    http->stages = mprCreateHash(-1, MPR_HASH_STABLE);
    http->hosts = mprCreateList(-1, MPR_LIST_STABLE);
    http->networks = mprCreateList(-1, MPR_LIST_STATIC_VALUES);
    http->authTypes = mprCreateHash(-1, MPR_HASH_CASELESS | MPR_HASH_UNIQUE | MPR_HASH_STABLE);
    http->authStores = mprCreateHash(-1, MPR_HASH_CASELESS | MPR_HASH_UNIQUE | MPR_HASH_STABLE);
    http->routeSets = mprCreateHash(-1, MPR_HASH_STATIC_VALUES | MPR_HASH_STABLE);
    http->booted = mprGetTime();
    http->flags = flags;
    http->monitorPeriod = ME_HTTP_MONITOR_PERIOD;
    http->secret = mprGetRandomString(HTTP_MAX_SECRET);
    http->trace = httpCreateTrace(0);
    http->startLevel = 2;
    http->localPlatform = slower(sfmt("%s-%s-%s", ME_OS, ME_CPU, ME_PROFILE));

    httpSetPlatform(http->localPlatform);
    httpSetPlatformDir(NULL);

    updateCurrentDate();
    http->statusCodes = mprCreateHash(41, MPR_HASH_STATIC_VALUES | MPR_HASH_STATIC_KEYS | MPR_HASH_STABLE);
    for (code = HttpStatusCodes; code->code; code++) {
        mprAddKey(http->statusCodes, code->codeString, code);
    }
    httpGetUserGroup();
    httpInitParser();
    httpInitAuth();
#if ME_HTTP_HTTP2
    http->http2 = 1;
    httpOpenHttp2Filter();
#endif
    httpOpenHttp1Filter();
    httpOpenNetConnector();
    httpOpenRangeFilter();
    httpOpenChunkFilter();
    httpOpenTailFilter();
    httpOpenQueueHead();
#if ME_HTTP_WEB_SOCKETS
    httpOpenWebSockFilter();
#endif
    mprSetIdleCallback(isHttpServiceIdle);
    mprAddTerminator(terminateHttp);

    if (flags & HTTP_SERVER_SIDE) {
        http->endpoints = mprCreateList(-1, MPR_LIST_STABLE);
        http->counters = mprCreateList(-1, MPR_LIST_STABLE);
        http->monitors = mprCreateList(-1, MPR_LIST_STABLE);
        http->routeTargets = mprCreateHash(-1, MPR_HASH_STATIC_VALUES | MPR_HASH_STABLE);
        http->routeConditions = mprCreateHash(-1, MPR_HASH_STATIC_VALUES | MPR_HASH_STABLE);
        http->routeUpdates = mprCreateHash(-1, MPR_HASH_STATIC_VALUES | MPR_HASH_STABLE);
        http->sessionCache = mprCreateCache(MPR_CACHE_SHARED | MPR_HASH_STABLE);
        http->addresses = mprCreateHash(-1, MPR_HASH_STABLE);
        http->defenses = mprCreateHash(-1, MPR_HASH_STABLE);
        http->remedies = mprCreateHash(-1, MPR_HASH_CASELESS | MPR_HASH_STATIC_VALUES | MPR_HASH_STABLE);
#if ME_HTTP_UPLOAD
        httpOpenUploadFilter();
#endif
#if ME_HTTP_CACHE
        httpOpenCacheHandler();
#endif
        httpOpenPassHandler();
        httpOpenActionHandler();
#if ME_HTTP_DIR
        httpOpenDirHandler();
#endif
        httpOpenFileHandler();
        http->serverLimits = httpCreateLimits(1);
        httpDefineRouteBuiltins();
#if ME_HTTP_DEFENSE
        httpAddCounters();
        httpAddRemedies();
#endif
        httpCreateDefaultHost();
    }
    if (flags & HTTP_CLIENT_SIDE) {
        http->defaultClientHost = sclone("127.0.0.1");
        http->defaultClientPort = 80;
        http->clientLimits = httpCreateLimits(0);
        http->clientRoute = httpCreateConfiguredRoute(0, 0);
    }
    mprGlobalUnlock();
    return http;
}


static void manageHttp(Http *http, int flags)
{
    HttpNet     *net;
    int         next;

    if (flags & MPR_MANAGE_MARK) {
        mprMark(http->addresses);
        mprMark(http->authStores);
        mprMark(http->authTypes);
        mprMark(http->clientLimits);
        mprMark(http->clientRoute);
        mprMark(http->networks);
        mprMark(http->context);
        mprMark(http->counters);
        mprMark(http->currentDate);
        mprMark(http->dateCache);
        mprMark(http->defaultClientHost);
        mprMark(http->defenses);
        mprMark(http->endpoints);
        mprMark(http->forkData);
        mprMark(http->group);
        mprMark(http->hosts);
        mprMark(http->jail);
        mprMark(http->localPlatform);
        mprMark(http->monitors);
        mprMark(http->mutex);
        mprMark(http->parsers);
        mprMark(http->platform);
        mprMark(http->platformDir);
        mprMark(http->proxyHost);
        mprMark(http->remedies);
        mprMark(http->routeConditions);
        mprMark(http->routeSets);
        mprMark(http->routeTargets);
        mprMark(http->routeUpdates);
        mprMark(http->secret);
        mprMark(http->serverLimits);
        mprMark(http->sessionCache);
        mprMark(http->software);
        mprMark(http->stages);
        mprMark(http->staticHeaders);
        mprMark(http->statusCodes);
        mprMark(http->timer);
        mprMark(http->timestamp);
        mprMark(http->trace);
        mprMark(http->user);

        /*
            Server endpoints keep network connections alive until a timeout.
            Keep marking even if no other references.
         */
        for (ITERATE_ITEMS(http->networks, net, next)) {
            if (httpIsServer(net)) {
                mprMark(net);
            }
        }
    }
}


PUBLIC Http *httpGetHttp()
{
    return HTTP;
}


PUBLIC int httpStartEndpoints()
{
    HttpEndpoint    *endpoint;
    int             next;

    if (!HTTP) {
        return MPR_ERR_BAD_STATE;
    }
    for (ITERATE_ITEMS(HTTP->endpoints, endpoint, next)) {
        if (httpStartEndpoint(endpoint) < 0) {
            return MPR_ERR_CANT_OPEN;
        }
    }
    if (httpApplyUserGroup() < 0) {
        httpStopEndpoints();
        return MPR_ERR_CANT_OPEN;
    }
    return 0;
}


PUBLIC void httpStopEndpoints()
{
    HttpEndpoint    *endpoint;
    Http            *http;
    int             next;

    if ((http = HTTP) == 0) {
        return;
    }
    lock(http->networks);
    for (next = 0; (endpoint = mprGetNextItem(http->endpoints, &next)) != 0; ) {
        httpStopEndpoint(endpoint);
    }
    unlock(http->networks);
}


/*
    Called to close all networks owned by a service (e.g. ejs)
 */
PUBLIC void httpStopNetworks(void *data)
{
    Http        *http;
    HttpNet     *net;
    int         next;

    if ((http = HTTP) == 0) {
        return;
    }
    lock(http->networks);
    for (ITERATE_ITEMS(http->networks, net, next)) {
        if (data == 0 || net->data == data) {
            httpDestroyNet(net);
        }
    }
    unlock(http->networks);
}


/*
    Destroy the http service. This should be called only after ensuring all running requests have completed.
    Normally invoked by the http terminator from mprDestroy
 */
PUBLIC void httpDestroy()
{
    Http        *http;

    if ((http = HTTP) == 0) {
        return;
    }
    httpStopNetworks(0);
    httpStopEndpoints();
    httpSetDefaultHost(0);

    if (http->timer) {
        mprRemoveEvent(http->timer);
        http->timer = 0;
    }
    if (http->timestamp) {
        mprRemoveEvent(http->timestamp);
        http->timestamp = 0;
    }
    http->hosts = NULL;
    http->clientRoute = NULL;
    http->endpoints = NULL;
    MPR->httpService = NULL;
}


/*
    Http terminator called from mprDestroy
 */
static void terminateHttp(int state, int how, int status)
{
    if (state >= MPR_STOPPED) {
        httpDestroy();
    }
}


/*
    Test if the http service (including MPR) is idle with no running requests
 */
static bool isHttpServiceIdle(bool traceRequests)
{
    Http            *http;
    HttpNet         *net;
    HttpStream      *stream;
    MprTicks        now;
    static MprTicks lastTrace = 0;
    int             next, nextConn;

    if ((http = MPR->httpService) != 0) {
        now = http->now;
        lock(http->networks);
        for (ITERATE_ITEMS(http->networks, net, next)) {
            for (ITERATE_ITEMS(net->streams, stream, nextConn)) {
                if (stream->state != HTTP_STATE_BEGIN && stream->state != HTTP_STATE_COMPLETE) {
                    if (traceRequests && lastTrace < now) {
                        if (stream->rx) {
                            mprLog("info http", 2, "Request for \"%s\" is still active",
                                stream->rx->uri ? stream->rx->uri : stream->rx->pathInfo);
                        }
                        lastTrace = now;
                    }
                    unlock(http->networks);
                    return 0;
                }
            }
        }
        unlock(http->networks);
    } else {
        now = mprGetTicks();
    }
    return mprServicesAreIdle(traceRequests);
}


PUBLIC void httpAddEndpoint(HttpEndpoint *endpoint)
{
    mprAddItem(HTTP->endpoints, endpoint);
}


PUBLIC void httpRemoveEndpoint(HttpEndpoint *endpoint)
{
    mprRemoveItem(HTTP->endpoints, endpoint);
}


/*
    Lookup a host address. If ipAddr is null or port is -1, then those elements are wild.
 */
PUBLIC HttpEndpoint *httpLookupEndpoint(cchar *ip, int port)
{
    HttpEndpoint    *endpoint;
    int             next;

    if (ip == 0) {
        ip = "";
    }
    for (next = 0; (endpoint = mprGetNextItem(HTTP->endpoints, &next)) != 0; ) {
        if (endpoint->port <= 0 || port <= 0 || endpoint->port == port) {
            assert(endpoint->ip);
            if (*endpoint->ip == '\0' || *ip == '\0' || scmp(endpoint->ip, ip) == 0) {
                return endpoint;
            }
        }
    }
    return 0;
}


PUBLIC HttpEndpoint *httpGetFirstEndpoint()
{
    return mprGetFirstItem(HTTP->endpoints);
}


/*
    WARNING: this should not be called by users as httpCreateHost will automatically call this.
 */
PUBLIC void httpAddHost(HttpHost *host)
{
    if (mprLookupItem(HTTP->hosts, host) < 0) {
        mprAddItem(HTTP->hosts, host);
    }
}


PUBLIC void httpRemoveHost(HttpHost *host)
{
    mprRemoveItem(HTTP->hosts, host);
}


PUBLIC HttpHost *httpLookupHost(cchar *name)
{
    HttpHost    *host;
    int         next;

    for (next = 0; (host = mprGetNextItem(HTTP->hosts, &next)) != 0; ) {
        if (smatch(name, host->name)) {
            return host;
        }
    }
    return 0;
}


PUBLIC void httpInitLimits(HttpLimits *limits, bool serverSide)
{
    memset(limits, 0, sizeof(HttpLimits));
    limits->cacheItemSize = ME_MAX_CACHE_ITEM;
    limits->chunkSize = ME_MAX_CHUNK;
    limits->clientMax = ME_MAX_CLIENTS;
    limits->connectionsMax = ME_MAX_CONNECTIONS;
    limits->connectionsPerClientMax = ME_MAX_CONNECTIONS_PER_CLIENT;
    limits->headerMax = ME_MAX_NUM_HEADERS;
    limits->headerSize = ME_MAX_HEADERS;
    limits->keepAliveMax = ME_MAX_KEEP_ALIVE;
    limits->packetSize = ME_PACKET_SIZE;
    limits->processMax = ME_MAX_PROCESSES;
    limits->requestsPerClientMax = ME_MAX_REQUESTS_PER_CLIENT;
    limits->sessionMax = ME_MAX_SESSIONS;
    limits->uriSize = ME_MAX_URI;

    limits->inactivityTimeout = ME_MAX_INACTIVITY_DURATION;
    limits->requestTimeout = ME_MAX_REQUEST_DURATION;
    limits->requestParseTimeout = ME_MAX_PARSE_DURATION;
    limits->sessionTimeout = ME_MAX_SESSION_DURATION;

#if ME_HTTP_WEB_SOCKETS
    limits->webSocketsMax = ME_MAX_WSS_SOCKETS;
    limits->webSocketsMessageSize = ME_MAX_WSS_MESSAGE;
    limits->webSocketsFrameSize = ME_MAX_WSS_FRAME;
    limits->webSocketsPacketSize = ME_MAX_WSS_PACKET;
    limits->webSocketsPing = ME_MAX_PING_DURATION;
#endif

#if ME_HTTP_HTTP2
    /*
        HTTP/2 parameters. Default and minimum frameSize must be 16K and window size 65535 by spec. Do not change.
     */
    limits->hpackMax = ME_MAX_HPACK_SIZE;
    limits->packetSize = HTTP2_MIN_FRAME_SIZE;
    limits->streamsMax = ME_MAX_STREAMS;
    limits->txStreamsMax = ME_MAX_STREAMS;
    limits->window = HTTP2_MIN_WINDOW;
#endif

    if (serverSide) {
        limits->rxFormSize = ME_MAX_RX_FORM;
        limits->rxBodySize = ME_MAX_RX_BODY;
        limits->txBodySize = ME_MAX_TX_BODY;
        limits->uploadSize = ME_MAX_UPLOAD;
    } else {
        limits->rxFormSize = HTTP_UNLIMITED;
        limits->rxBodySize = HTTP_UNLIMITED;
        limits->txBodySize = HTTP_UNLIMITED;
        limits->uploadSize = HTTP_UNLIMITED;
    }
}


PUBLIC HttpLimits *httpCreateLimits(int serverSide)
{
    HttpLimits  *limits;

    if ((limits = mprAllocStruct(HttpLimits)) != 0) {
        httpInitLimits(limits, serverSide);
    }
    return limits;
}


PUBLIC HttpLimits *httpCloneLimits(HttpLimits *limits)
{
    return (HttpLimits*) mprMemdup(limits, sizeof(HttpLimits));
}


PUBLIC void httpEaseLimits(HttpLimits *limits)
{
    limits->rxFormSize = HTTP_UNLIMITED;
    limits->rxBodySize = HTTP_UNLIMITED;
    limits->txBodySize = HTTP_UNLIMITED;
    limits->uploadSize = HTTP_UNLIMITED;
}


PUBLIC void httpAddStage(HttpStage *stage)
{
    mprAddKey(HTTP->stages, stage->name, stage);
}


PUBLIC HttpStage *httpLookupStage(cchar *name)
{
    HttpStage   *stage;

    if (!HTTP) {
        return 0;
    }
    if ((stage = mprLookupKey(HTTP->stages, name)) == 0 || stage->flags & HTTP_STAGE_INTERNAL) {
        return 0;
    }
    return stage;
}


PUBLIC void *httpLookupStageData(cchar *name)
{
    HttpStage   *stage;

    if (!HTTP) {
        return 0;
    }
    if ((stage = mprLookupKey(HTTP->stages, name)) != 0) {
        return stage->stageData;
    }
    return 0;
}


PUBLIC cchar *httpLookupStatus(int status)
{
    HttpStatusCode  *ep;
    char            *key;

    if (!HTTP) {
        return 0;
    }
    key = itos(status);
    ep = (HttpStatusCode*) mprLookupKey(HTTP->statusCodes, key);
    if (ep == 0) {
        return "Custom error";
    }
    return ep->msg;
}


PUBLIC void httpSetForkCallback(MprForkCallback callback, void *data)
{
    HTTP->forkCallback = callback;
    HTTP->forkData = data;
}


PUBLIC void httpSetListenCallback(HttpListenCallback fn)
{
    HTTP->listenCallback = fn;
}


/*
    The http timer does maintenance activities and will fire per second while there are active requests.
    This routine will also be called by httpTerminate with event == 0 to signify a shutdown.
    NOTE: Because we lock the http here, streams cannot be deleted while we are modifying the list.
 */
static void httpTimer(Http *http, MprEvent *event)
{
    HttpNet     *net;
    HttpStream  *stream;
    HttpStage   *stage;
    HttpLimits  *limits;
    MprModule   *module;
    int         next, active, abort, nextConn;

    active = 0;

    if (mprGetDebugMode()) {
        mprRescheduleEvent(event, 15 * 60 * TPS);
        return;
    }

    updateCurrentDate();
    lock(http->networks);

    for (next = 0; (net = mprGetNextItem(http->networks, &next)) != 0; active++) {
        /*
           Check for any inactive streams or expired requests (inactivityTimeout and requestTimeout)
         */
        for (active = 0, nextConn = 0; (stream = mprGetNextItem(net->streams, &nextConn)) != 0; active++) {
            if (!mprGetDebugMode()) {
                limits = stream->limits;
                abort = mprIsStopping();
                if (httpServerStream(stream) && (HTTP_STATE_CONNECTED < stream->state && stream->state < HTTP_STATE_PARSED) &&
                        (http->now - stream->started) > limits->requestParseTimeout) {
                    stream->timeout = HTTP_PARSE_TIMEOUT;
                    abort = 1;
                } else if ((http->now - stream->lastActivity) > limits->inactivityTimeout) {
                    stream->timeout = HTTP_INACTIVITY_TIMEOUT;
                    abort = 1;
                } else if ((http->now - stream->started) > limits->requestTimeout) {
                    stream->timeout = HTTP_REQUEST_TIMEOUT;
                    abort = 1;
                } else if (!event) {
                    /* Called directly from httpStop to stop streams */
                    if (MPR->exitTimeout > 0) {
                        if (stream->state == HTTP_STATE_COMPLETE ||
                            (HTTP_STATE_CONNECTED < stream->state && stream->state < HTTP_STATE_PARSED)) {
                            abort = 1;
                        }
                    } else {
                        abort = 1;
                    }
                }
                if (abort) {
                    httpStreamTimeout(stream);
                }
            }
        }
        if ((http->now - net->lastActivity) > net->limits->inactivityTimeout) {
            if (!mprGetDebugMode()) {
                net->timeout = HTTP_INACTIVITY_TIMEOUT;
                httpNetTimeout(net);
            }
        }
    }

    /*
        Check for unloadable modules
        OPT - could check for modules every minute
     */
    if (mprGetListLength(http->networks) == 0) {
        for (next = 0; (module = mprGetNextItem(MPR->moduleService->modules, &next)) != 0; ) {
            if (module->timeout) {
                if (module->lastActivity + module->timeout < http->now) {
                    mprLog("info http", 2, "Unloading inactive module %s", module->name);
                    if ((stage = httpLookupStage(module->name)) != 0) {
                        if (mprUnloadModule(module) < 0)  {
                            active++;
                        } else {
                            stage->flags |= HTTP_STAGE_UNLOADED;
                        }
                    } else {
                        mprUnloadModule(module);
                    }
                } else {
                    active++;
                }
            }
        }
    }
#if ME_HTTP_DEFENSE
    httpPruneMonitors();
#endif

    if (active == 0 || mprIsStopping()) {
        if (event) {
            mprRemoveEvent(event);
        }
        http->timer = 0;
        /*
            Going to sleep now, so schedule a GC to free as much as possible.
         */
        mprGC(MPR_GC_FORCE | MPR_GC_NO_BLOCK);
    } else {
        mprGC(MPR_GC_NO_BLOCK);
    }
    unlock(http->networks);
}


static void timestamp()
{
    mprLog("info http", 0, "Time: %s", mprGetDate(NULL));
}


PUBLIC void httpSetTimestamp(MprTicks period)
{
    Http    *http;

    http = HTTP;
    if (period < (10 * TPS)) {
        period = (10 * TPS);
    }
    if (http->timestamp) {
        mprRemoveEvent(http->timestamp);
    }
    if (period > 0) {
        http->timestamp = mprCreateTimerEvent(NULL, "httpTimestamp", period, timestamp, NULL,
            MPR_EVENT_CONTINUOUS | MPR_EVENT_QUICK);
    }
}


PUBLIC void httpAddNet(HttpNet *net)
{
    Http    *http;

    http = net->http;

    mprAddItem(http->networks, net);
    http->now = mprGetTicks();
    updateCurrentDate();

    lock(http);
    if (!http->timer) {
        http->timer = mprCreateTimerEvent(NULL, "httpTimer", HTTP_TIMER_PERIOD, httpTimer, http, MPR_EVENT_CONTINUOUS | MPR_EVENT_QUICK);
    }
    unlock(http);
}


PUBLIC void httpRemoveNet(HttpNet *net)
{
    mprRemoveItem(net->http->networks, net);
}


PUBLIC char *httpGetDateString(MprPath *sbuf)
{
    MprTicks    when;

    if (sbuf == 0) {
        when = mprGetTime();
    } else {
        when = (MprTicks) sbuf->mtime * TPS;
    }
    return mprFormatUniversalTime(HTTP_DATE_FORMAT, when);
}


PUBLIC void *httpGetContext()
{
    return HTTP->context;
}


PUBLIC void httpSetContext(void *context)
{
    HTTP->context = context;
}


PUBLIC int httpGetDefaultClientPort()
{
    return HTTP->defaultClientPort;
}


PUBLIC cchar *httpGetDefaultClientHost()
{
    return HTTP->defaultClientHost;
}


PUBLIC void httpSetDefaultClientPort(int port)
{
    HTTP->defaultClientPort = port;
}


PUBLIC void httpSetDefaultClientHost(cchar *host)
{
    HTTP->defaultClientHost = sclone(host);
}


PUBLIC void httpSetSoftware(cchar *software)
{
    HTTP->software = sclone(software);
}


PUBLIC void httpSetProxy(cchar *host, int port)
{
    HTTP->proxyHost = sclone(host);
    HTTP->proxyPort = port;
}


static void updateCurrentDate()
{
    Http        *http;
    MprTicks    diff;

    http = HTTP;
    http->now = mprGetTicks();
    diff = http->now - http->currentTime;
    if (diff <= TPS || diff >= TPS) {
        /*
            Optimize and only update the string date representation once per second
         */
        http->currentTime = http->now;
        http->currentDate = httpGetDateString(NULL);
    }
}


PUBLIC void httpGetStats(HttpStats *sp)
{
    Http                *http;
    HttpAddress         *address;
    MprKey              *kp;
    MprMemStats         *ap;
    MprWorkerStats      wstats;
    ssize               memSessions;

    memset(sp, 0, sizeof(*sp));
    http = HTTP;
    ap = mprGetMemStats();

    sp->cpuUsage = ap->cpuUsage;
    sp->cpuCores = ap->cpuCores;
    sp->ram = ap->ram;
    sp->mem = ap->rss;
    sp->memRedline = ap->warnHeap;
    sp->memMax = ap->maxHeap;

    sp->heap = ap->bytesAllocated;
    sp->heapUsed = ap->bytesAllocated - ap->bytesFree;
    sp->heapPeak = ap->bytesAllocatedPeak;
    sp->heapFree = ap->bytesFree;
    sp->heapRegions = ap->heapRegions;

    mprGetWorkerStats(&wstats);
    sp->workersBusy = wstats.busy;
    sp->workersIdle = wstats.idle;
    sp->workersYielded = wstats.yielded;
    sp->workersMax = wstats.max;

    sp->activeConnections = mprGetListLength(http->networks);
    sp->activeProcesses = http->activeProcesses;

    mprGetCacheStats(http->sessionCache, &sp->activeSessions, &memSessions);
    sp->memSessions = memSessions;

    lock(http->addresses);
    for (ITERATE_KEY_DATA(http->addresses, kp, address)) {
        sp->activeRequests += (int) address->counters[HTTP_COUNTER_ACTIVE_REQUESTS].value;
        sp->activeClients++;
    }
    unlock(http->addresses);

    sp->totalRequests = http->totalRequests;
    sp->totalConnections = http->totalConnections;
    sp->totalSweeps = MPR->heap->stats.sweeps;
}


PUBLIC bool httpConfigure(HttpConfigureProc proc, void *data, MprTicks timeout)
{
    Http        *http;
    MprTicks    mark;

    http = HTTP;
    mark = mprGetTicks();
    if (timeout < 0) {
        timeout = http->serverLimits->requestTimeout;
    } else if (timeout == 0) {
        timeout = MAXINT;
    }
    do {
        lock(http->networks);
        /* Own request will count as 1 */
        if (mprGetListLength(http->networks) == 0) {
            (proc)(data);
            unlock(http->networks);
            return 1;
        }
        unlock(http->networks);
        mprSleep(10);
        /* Defaults to 10 secs */
    } while (mprGetRemainingTicks(mark, timeout) > 0);
    return 0;
}


PUBLIC int httpApplyUserGroup()
{
#if ME_UNIX_LIKE
    Http        *http;
    HttpHost    *host;
    HttpRoute   *route;
    cchar       *path;
    int         nextHost, nextRoute;

    http = HTTP;
    if (http->userChanged || http->groupChanged) {
        if (MPR->logPath && !smatch(MPR->logPath, "stdout") && !smatch(MPR->logPath, "stderr")) {
            if (chown(MPR->logPath, http->uid, http->gid) < 0) {
                mprLog("critical http", 0, "Cannot change ownership on %s", MPR->logPath);
            }
        }
        for (ITERATE_ITEMS(HTTP->hosts, host, nextHost)) {
            for (ITERATE_ITEMS(host->routes, route, nextRoute)) {
                if (route->trace) {
                    path = route->trace->path;
                    if (path && !smatch(path, "stdout") && !smatch(path, "stderr")) {
                        if (chown(path, http->uid, http->gid) < 0) {
                            mprLog("critical http", 0, "Cannot change ownership on %s", path);
                        }
                    }
                }
            }
        }
    }
    if (httpApplyChangedGroup() < 0 || httpApplyChangedUser() < 0) {
        return MPR_ERR_CANT_COMPLETE;
    }
    if (http->userChanged || http->groupChanged) {
        struct group    *gp;
        gid_t           glist[64], gid;
        MprBuf          *gbuf = mprCreateBuf(0, 0);
        cchar           *groups;
        int             i, ngroup;

        gid = getgid();
        ngroup = getgroups(sizeof(glist) / sizeof(gid_t), glist);
        if (ngroup > 1) {
            mprPutStringToBuf(gbuf, ", groups: ");
            for (i = 0; i < ngroup; i++) {
                if (glist[i] == gid) continue;
                if ((gp = getgrgid(glist[i])) != 0) {
                    mprPutToBuf(gbuf, "%s (%d) ", gp->gr_name, glist[i]);
                } else {
                    mprPutToBuf(gbuf, "(%d) ", glist[i]);
                }
            }
        }
        groups = mprGetBufStart(gbuf);
        mprLog("info http", 2, "Running as user \"%s\" (%d), group \"%s\" (%d)%s", http->user, http->uid,
            http->group, http->gid, groups);
    }
#endif
    return 0;
}


PUBLIC void httpGetUserGroup()
{
#if ME_UNIX_LIKE
    Http            *http;
    struct passwd   *pp;
    struct group    *gp;

    http = HTTP;
    http->uid = getuid();
    if ((pp = getpwuid(http->uid)) == 0) {
        mprLog("critical http", 0, "Cannot read user credentials: %d. Check your /etc/passwd file.", http->uid);
    } else {
        http->user = sclone(pp->pw_name);
    }
    http->gid = getgid();
    if ((gp = getgrgid(http->gid)) == 0) {
        mprLog("critical http", 0, "Cannot read group credentials: %d. Check your /etc/group file", http->gid);
    } else {
        http->group = sclone(gp->gr_name);
    }
#else
    Http *http = HTTP;
    http->uid = http->gid = -1;
#endif
}


PUBLIC int httpSetUserAccount(cchar *newUser)
{
    Http        *http;

    http = HTTP;
    if (smatch(newUser, "HTTP") || smatch(newUser, "APPWEB")) {
#if ME_UNIX_LIKE
        /* Only change user if root */
        if (getuid() != 0) {
            mprLog("info http", 2, "Running as user \"%s\"", http->user);
            return 0;
        }
#endif
#if MACOSX || FREEBSD
        newUser = "_www";
#elif LINUX || ME_UNIX_LIKE
        newUser = "nobody";
#elif WINDOWS
        newUser = "Administrator";
#endif
    }
#if ME_UNIX_LIKE
{
    struct passwd   *pp;
    if (snumber(newUser)) {
        http->uid = atoi(newUser);
        if ((pp = getpwuid(http->uid)) == 0) {
            mprLog("critical http", 0, "Bad user id: %d", http->uid);
            return MPR_ERR_CANT_ACCESS;
        }
        newUser = pp->pw_name;

    } else {
        if ((pp = getpwnam(newUser)) == 0) {
            mprLog("critical http", 0, "Bad user name: %s", newUser);
            return MPR_ERR_CANT_ACCESS;
        }
        http->uid = pp->pw_uid;
    }
    http->userChanged = 1;
}
#endif
    http->user = sclone(newUser);
    return 0;
}


PUBLIC int httpSetGroupAccount(cchar *newGroup)
{
    Http    *http;

    http = HTTP;
    if (smatch(newGroup, "HTTP") || smatch(newGroup, "APPWEB")) {
#if ME_UNIX_LIKE
        /* Only change group if root */
        if (getuid() != 0) {
            return 0;
        }
#endif
#if MACOSX || FREEBSD
        newGroup = "_www";
#elif LINUX || ME_UNIX_LIKE
{
        char    *buf;
        newGroup = "nobody";
        /*
            Debian has nogroup, Fedora has nobody. Ugh!
         */
        if ((buf = mprReadPathContents("/etc/group", NULL)) != 0) {
            if (scontains(buf, "nogroup:")) {
                newGroup = "nogroup";
            }
        }
}
#elif WINDOWS
        newGroup = "Administrator";
#endif
    }
#if ME_UNIX_LIKE
    struct group    *gp;

    if (snumber(newGroup)) {
        http->gid = atoi(newGroup);
        if ((gp = getgrgid(http->gid)) == 0) {
            mprLog("critical http", 0, "Bad group id: %d", http->gid);
            return MPR_ERR_CANT_ACCESS;
        }
        newGroup = gp->gr_name;

    } else {
        if ((gp = getgrnam(newGroup)) == 0) {
            mprLog("critical http", 0, "Bad group name: %s", newGroup);
            return MPR_ERR_CANT_ACCESS;
        }
        http->gid = gp->gr_gid;
    }
    http->groupChanged = 1;
#endif
    http->group = sclone(newGroup);
    return 0;
}


PUBLIC int httpApplyChangedUser()
{
#if ME_UNIX_LIKE
    Http    *http;

    http = HTTP;
    if (http->userChanged && http->uid >= 0) {
        if (http->gid >= 0 && http->groupChanged) {
            if (setgroups(0, NULL) == -1) {
                mprLog("critical http", 0, "Cannot clear supplemental groups");
            }
            if (setgid(http->gid) == -1) {
                mprLog("critical http", 0, "Cannot change group to %s: %d"
                    "WARNING: This is a major security exposure", http->group, http->gid);
            }
        } else {
            struct passwd   *pp;
            if ((pp = getpwuid(http->uid)) == 0) {
                mprLog("critical http", 0, "Cannot get user entry for id: %d", http->uid);
                return MPR_ERR_CANT_ACCESS;
            }
            mprLog("http", 4, "Initgroups for %s GID %d", http->user, pp->pw_gid);
            if (initgroups(http->user, pp->pw_gid) == -1) {
                mprLog("critical http", 0, "Cannot initgroups for %s, errno: %d", http->user, errno);
            }
        }
        if ((setuid(http->uid)) != 0) {
            mprLog("critical http", 0, "Cannot change user to: %s: %d"
                "WARNING: This is a major security exposure", http->user, http->uid);
            return MPR_ERR_BAD_STATE;
#if LINUX && PR_SET_DUMPABLE
        } else {
            prctl(PR_SET_DUMPABLE, 1);
#endif
        }
    }
#endif
    return 0;
}


PUBLIC int httpApplyChangedGroup()
{
#if ME_UNIX_LIKE
    Http    *http;

    http = HTTP;
    if (http->groupChanged && http->gid >= 0) {
        if (setgid(http->gid) != 0) {
            mprLog("critical http", 0, "Cannot change group to %s: %d\n"
                "WARNING: This is a major security exposure", http->group, http->gid);
            if (getuid() != 0) {
                mprLog("critical http", 0, "Log in as administrator/root and retry");
            }
            return MPR_ERR_BAD_STATE;
#if LINUX && PR_SET_DUMPABLE
        } else {
            prctl(PR_SET_DUMPABLE, 1);
#endif
        }
    }
#endif
    return 0;
}


PUBLIC int httpParsePlatform(cchar *platform, cchar **osp, cchar **archp, cchar **profilep)
{
    char   *arch, *os, *profile, *rest;

    if (osp) {
        *osp = 0;
    }
    if (archp) {
       *archp = 0;
    }
    if (profilep) {
       *profilep = 0;
    }
    if (platform == 0 || *platform == '\0') {
        return MPR_ERR_BAD_ARGS;
    }
    os = stok(sclone(platform), "-", &rest);
    arch = sclone(stok(NULL, "-", &rest));
    profile = sclone(rest);
    if (os == 0 || arch == 0 || profile == 0 || *os == '\0' || *arch == '\0' || *profile == '\0') {
        return MPR_ERR_BAD_ARGS;
    }
    if (osp) {
        *osp = os;
    }
    if (archp) {
       *archp = arch;
    }
    if (profilep) {
       *profilep = profile;
    }
    return 0;
}


PUBLIC int httpSetPlatform(cchar *platform)
{
    Http    *http;
    cchar   *junk;

    http = HTTP;
    if (platform && httpParsePlatform(platform, &junk, &junk, &junk) < 0) {
        return MPR_ERR_BAD_ARGS;
    }
    http->platform = platform ? sclone(platform) : http->localPlatform;
    mprLog("info http", 4, "Using platform %s", http->platform);
    return 0;
}


/*
    Set the platform objects location
 */
PUBLIC int httpSetPlatformDir(cchar *path)
{
    Http    *http;

    http = HTTP;
    if (path) {
        if (mprPathExists(path, X_OK)) {
            http->platformDir = mprGetAbsPath(path);
        } else {
            /*
                Possible source tree platform directory
             */
            http->platformDir = mprJoinPath(mprGetPathDir(mprGetPathDir(mprGetPathDir(mprGetAppPath()))), path);
            if (!mprPathExists(http->platformDir, X_OK)) {
                http->platformDir = mprGetAbsPath(path);
            }
        }
    } else {
        http->platformDir = mprGetPathDir(mprGetPathDir(mprGetAppPath()));
    }
    return 0;
}


PUBLIC void httpEnableHttp2(int enable)
{
    HTTP->http2 = enable;
}


PUBLIC void httpSetJail(cchar *path)
{
    HTTP->jail = sclone(path);
}


PUBLIC void httpSetEnvCallback(HttpEnvCallback envCallback)
{
    HTTP->envCallback = envCallback;
}


PUBLIC void httpSetRedirectCallback(HttpRedirectCallback redirectCallback)
{
    HTTP->redirectCallback = redirectCallback;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/actionHandler.c ************/

/*
    actionHandler.c -- Action handler

    This handler maps URIs to actions that are C functions that have been registered via httpDefineAction.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/*********************************** Code *************************************/

static void startAction(HttpQueue *q)
{
    HttpStream  *stream;
    HttpAction  action;
    cchar       *name;

    stream = q->stream;
    assert(!stream->error);
    assert(!stream->tx->finalized);

    name = stream->rx->pathInfo;
    if ((action = mprLookupKey(stream->tx->handler->stageData, name)) == 0) {
        httpError(stream, HTTP_CODE_NOT_FOUND, "Cannot find action: %s", name);
    } else {
        (*action)(stream);
    }
}


PUBLIC void httpDefineAction(cchar *name, HttpAction action)
{
    HttpStage   *stage;

    if ((stage = httpLookupStage("actionHandler")) == 0) {
        mprLog("error http action", 0, "Cannot find actionHandler");
        return;
    }
    mprAddKey(stage->stageData, name, action);
}


PUBLIC int httpOpenActionHandler()
{
    HttpStage     *stage;

    if ((stage = httpCreateHandler("actionHandler", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    HTTP->actionHandler = stage;
    if ((stage->stageData = mprCreateHash(0, MPR_HASH_STATIC_VALUES)) == 0) {
        return MPR_ERR_MEMORY;
    }
    stage->start = startAction;
    return 0;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/auth.c ************/

/*

    auth.c - Authorization and access management

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/********************************* Forwards ***********************************/

#undef  GRADUATE_HASH
#define GRADUATE_HASH(auth, field) \
    if (!auth->field) { \
        if (auth->parent && auth->field && auth->field == auth->parent->field) { \
            auth->field = mprCloneHash(auth->parent->field); \
        } else { \
            auth->field = mprCreateHash(0, MPR_HASH_STABLE); \
        } \
    }

static void manageAuth(HttpAuth *auth, int flags);
static void formLogin(HttpStream *stream);
PUBLIC int formParse(HttpStream *stream, cchar **username, cchar **password);
static bool configVerifyUser(HttpStream *stream, cchar *username, cchar *password);

/*********************************** Code *************************************/

PUBLIC void httpInitAuth()
{
    /*
        Auth protocol types: basic, digest, form, app
     */
#if ME_HTTP_BASIC
    httpCreateAuthType("basic", httpBasicLogin, httpBasicParse, httpBasicSetHeaders);
#endif
#if ME_HTTP_DIGEST
    httpCreateAuthType("digest", httpDigestLogin, httpDigestParse, httpDigestSetHeaders);
#endif
    httpCreateAuthType("form", formLogin, formParse, NULL);
    httpCreateAuthType("app", NULL, NULL, NULL);

    /*
        Stores: app (custom in user app), config (config file directives), system (PAM / native O/S)
     */
    httpCreateAuthStore("app", NULL);
    httpCreateAuthStore("config", configVerifyUser);
#if ME_COMPILER_HAS_PAM && ME_HTTP_PAM
    httpCreateAuthStore("system", httpPamVerifyUser);
#endif
}

PUBLIC HttpAuth *httpCreateAuth()
{
    HttpAuth    *auth;

    if ((auth = mprAllocObj(HttpAuth, manageAuth)) == 0) {
        return 0;
    }
    auth->realm = MPR->emptyString;
    return auth;
}


PUBLIC HttpAuth *httpCreateInheritedAuth(HttpAuth *parent)
{
    HttpAuth      *auth;

    if ((auth = mprAllocObj(HttpAuth, manageAuth)) == 0) {
        return 0;
    }
    if (parent) {
        //  OPT. Structure assignment
        auth->flags = parent->flags;
        auth->allow = parent->allow;
        auth->cipher = parent->cipher;
        auth->deny = parent->deny;
        auth->type = parent->type;
        auth->store = parent->store;
        auth->flags = parent->flags;
        auth->qop = parent->qop;
        auth->realm = parent->realm;
        auth->permittedUsers = parent->permittedUsers;
        auth->abilities = parent->abilities;
        auth->userCache = parent->userCache;
        auth->roles = parent->roles;
        auth->loggedOutPage = parent->loggedOutPage;
        auth->loggedInPage = parent->loggedInPage;
        auth->loginPage = parent->loginPage;
        auth->username = parent->username;
        auth->verifyUser = parent->verifyUser;
        auth->parent = parent;
    }
    return auth;
}


static void manageAuth(HttpAuth *auth, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(auth->cipher);
        mprMark(auth->realm);
        mprMark(auth->allow);
        mprMark(auth->deny);
        mprMark(auth->userCache);
        mprMark(auth->roles);
        mprMark(auth->abilities);
        mprMark(auth->permittedUsers);
        mprMark(auth->loginPage);
        mprMark(auth->loggedInPage);
        mprMark(auth->loggedOutPage);
        mprMark(auth->username);
        mprMark(auth->qop);
        mprMark(auth->type);
        mprMark(auth->store);
    }
}


static void manageAuthType(HttpAuthType *type, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(type->name);
    }
}



static void manageAuthStore(HttpAuthStore *store, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(store->name);
    }
}

/*
    Authenticate a user using the session stored username. This will set HttpRx.authenticated if authentication succeeds.
    Note: this does not call httpLogin except for auto-login cases where a password is not used.
 */
PUBLIC bool httpAuthenticate(HttpStream *stream)
{
    HttpRx      *rx;
    HttpAuth    *auth;
    cchar       *ip, *username;

    rx = stream->rx;
    auth = rx->route->auth;

    if (!rx->authenticateProbed) {
        rx->authenticateProbed = 1;

        ip = httpGetSessionVar(stream, HTTP_SESSION_IP, 0);
        username = httpGetSessionVar(stream, HTTP_SESSION_USERNAME, 0);

        if (!smatch(ip, stream->ip) || !username) {
            if (auth->username && *auth->username) {
                /* Auto-login */
                httpLogin(stream, auth->username, NULL);
                username = httpGetSessionVar(stream, HTTP_SESSION_USERNAME, 0);
            }
            if (!username) {
                return 0;
            }
        }
        if (!stream->user && (stream->user = mprLookupKey(auth->userCache, username)) == 0) {
            return 0;
        }
        stream->username = username;
        rx->authenticated = 1;
        httpLog(stream->trace, "auth.login.authenticated", "context",
            "msg:Using cached authentication data, username:%s", username);
    }
    return rx->authenticated;
}


/*
    Test if the user has the requisite abilities to perform an action. Abilities may be explicitly defined or if NULL,
    the abilities specified by the route are used.
 */
PUBLIC bool httpCanUser(HttpStream *stream, cchar *abilities)
{
    HttpAuth    *auth;
    char        *ability, *tok;
    MprKey      *kp;

    auth = stream->rx->route->auth;
    if (auth->permittedUsers && !mprLookupKey(auth->permittedUsers, stream->username)) {
        return 0;
    }
    if (!auth->abilities && !abilities) {
        /* No abilities are required */
        return 1;
    }
    if (!stream->username) {
        /* User not authenticated */
        return 0;
    }
    if (!stream->user && (stream->user = mprLookupKey(auth->userCache, stream->username)) == 0) {
        return 0;
    }
    if (abilities) {
        for (ability = stok(sclone(abilities), " \t,", &tok); abilities; abilities = stok(NULL, " \t,", &tok)) {
            if (!mprLookupKey(stream->user->roles, ability)) {
                if (!mprLookupKey(stream->user->abilities, ability)) {
                    return 0;
                }
            }
        }
    } else {
        for (ITERATE_KEYS(auth->abilities, kp)) {
            if (!mprLookupKey(stream->user->roles, kp->key)) {
                if (!mprLookupKey(stream->user->abilities, kp->key)) {
                    return 0;
                }
            }
        }
    }
    return 1;
}


PUBLIC HttpAuthStore *httpCreateAuthStore(cchar *name, HttpVerifyUser verifyUser)
{
    HttpAuthStore   *store;

    if ((store = mprAllocObj(HttpAuthStore, manageAuthStore)) == 0) {
        return 0;
    }
    store->name = sclone(name);
    store->verifyUser = verifyUser;
    if (mprAddKey(HTTP->authStores, name, store) == 0) {
        return 0;
    }
    return store;
}


PUBLIC int httpCreateAuthType(cchar *name, HttpAskLogin askLogin, HttpParseAuth parseAuth, HttpSetAuth setAuth)
{
    HttpAuthType    *type;

    if ((type = mprAllocObj(HttpAuthType, manageAuthType)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    type->name = sclone(name);
    type->askLogin = askLogin;
    type->parseAuth = parseAuth;
    type->setAuth = setAuth;
    if (!smatch(name, "app")) {
        type->flags = HTTP_AUTH_TYPE_CONDITION;
    }
    if (mprAddKey(HTTP->authTypes, name, type) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    return 0;
}


PUBLIC HttpAuthStore *httpGetAuthStore(cchar *name)
{
    return mprLookupKey(HTTP->authStores, name);
}


/*
    Get the username and password credentials. Called by authCondition which thereafter calls httpLogin.
    If using an in-protocol auth scheme like basic|digest, the rx->authDetails will contain the credentials
    and the parseAuth callback will be invoked to parse. Otherwise, it is expected that "username" and
    "password" fields are present in the request parameters.
 */
PUBLIC bool httpGetCredentials(HttpStream *stream, cchar **username, cchar **password)
{
    HttpAuth    *auth;
    HttpRx      *rx;

    assert(username);
    assert(password);

    rx = stream->rx;

    *username = *password = NULL;

    auth = rx->route->auth;
    if (!auth || !auth->type || !(auth->type->flags & HTTP_AUTH_TYPE_CONDITION)) {
        return 0;
    }
    if (auth->type) {
        if (rx->authType && !smatch(rx->authType, auth->type->name)) {
            return 0;
        }
        if (auth->type->parseAuth && (auth->type->parseAuth)(stream, username, password) < 0) {
            return 0;
        }
    } else {
        *username = httpGetParam(stream, "username", 0);
        *password = httpGetParam(stream, "password", 0);
    }
    return 1;
}


PUBLIC bool httpIsAuthenticated(HttpStream *stream)
{
    return httpAuthenticate(stream);
}


/*
    Login the user and create an authenticated session state store
 */
PUBLIC bool httpLogin(HttpStream *stream, cchar *username, cchar *password)
{
    HttpRx          *rx;
    HttpAuth        *auth;
    HttpVerifyUser  verifyUser;

    rx = stream->rx;
    auth = rx->route->auth;
    if (!username || !*username) {
        httpLog(stream->trace, "auth.login.error", "error", "msg:missing username");
        return 0;
    }
    if (!auth->store) {
        mprLog("error http auth", 0, "No AuthStore defined");
        return 0;
    }
    if ((verifyUser = auth->verifyUser) == 0) {
        if (!auth->parent || (verifyUser = auth->parent->verifyUser) == 0) {
            verifyUser = auth->store->verifyUser;
        }
    }
    if (!verifyUser) {
        mprLog("error http auth", 0, "No user verification routine defined on route %s", rx->route->pattern);
        return 0;
    }
    if (auth->username && *auth->username) {
        /* If using auto-login, replace the username */
        username = auth->username;
        password = 0;

    } else if (!username || !password) {
        return 0;
    }
    if (!(verifyUser)(stream, username, password)) {
        return 0;
    }
    if (!(auth->flags & HTTP_AUTH_NO_SESSION) && !auth->store->noSession) {
        if (httpCreateSession(stream) == 0) {
            /* Too many sessions */
            return 0;
        }
        httpSetSessionVar(stream, HTTP_SESSION_USERNAME, username);
        httpSetSessionVar(stream, HTTP_SESSION_IP, stream->ip);
    }
    rx->authenticated = 1;
    rx->authenticateProbed = 1;
    stream->username = sclone(username);
    stream->encoded = 0;
    return 1;
}


PUBLIC bool httpIsLoggedIn(HttpStream *stream)
{
    return httpAuthenticate(stream);
}


/*
    Log the user out and remove the authentication username from the session state
 */
PUBLIC void httpLogout(HttpStream *stream)
{
    stream->rx->authenticated = 0;
    httpDestroySession(stream);
}


PUBLIC void httpSetAuthVerify(HttpAuth *auth, HttpVerifyUser verifyUser)
{
    auth->verifyUser = verifyUser;
}


PUBLIC void httpSetAuthAllow(HttpAuth *auth, cchar *allow)
{
    GRADUATE_HASH(auth, allow);
    mprAddKey(auth->allow, sclone(allow), auth);
}


PUBLIC void httpSetAuthAnyValidUser(HttpAuth *auth)
{
    auth->permittedUsers = 0;
}


PUBLIC void httpSetAuthLogin(HttpAuth *auth, cchar *value)
{
    auth->loginPage = sclone(value);
}


/*
    Web form login service routine. Called in response to a form-based login request when defined via httpSetAuthLogin.
    It is expected that "authCondition" has already authenticated the request.
 */
static void loginServiceProc(HttpStream *stream)
{
    HttpAuth    *auth;

    auth = stream->rx->route->auth;
    if (httpIsAuthenticated(stream)) {
        httpRedirect(stream, HTTP_CODE_MOVED_TEMPORARILY, auth->loggedInPage ? auth->loggedInPage : "~");
    } else {
        httpRedirect(stream, HTTP_CODE_MOVED_TEMPORARILY, auth->loginPage);
    }
}


/*
    Logout service for use with httpSetAuthFormDetails.
 */
static void logoutServiceProc(HttpStream *stream)
{
    HttpRoute       *route;
    HttpAuth        *auth;
    cchar           *loggedOut;

    route = stream->rx->route;
    auth = route->auth;

    httpLogout(stream);

    loggedOut = (auth->loggedOutPage) ? auth->loggedOutPage : auth->loginPage;
    if (!loggedOut) {
        loggedOut = "/";
    }
    httpRedirect(stream, HTTP_CODE_MOVED_TEMPORARILY, loggedOut);
}


static HttpRoute *createLoginRoute(HttpRoute *route, cchar *pattern, HttpAction action)
{
    bool    secure;

    secure = 0;
    if (sstarts(pattern, "https:///")) {
        pattern = &pattern[8];
        secure = 1;
    } else if (sstarts(pattern, "http:///")) {
        pattern = &pattern[7];
    }
    if ((route = httpCreateInheritedRoute(route)) != 0) {
        httpSetRoutePattern(route, sjoin("^", pattern, "$", NULL), 0);
        if (secure) {
            httpAddRouteCondition(route, "secure", "https://", HTTP_ROUTE_REDIRECT);
        }
        if (action) {
            route->handler = route->http->actionHandler;
            httpDefineAction(pattern, action);
        }
        httpFinalizeRoute(route);
    }
    return route;
}


/*
    Define login URLs by creating routes. Used by Appweb AuthType directive.
    Web frameworks like ESP should NOT use this.
 */
PUBLIC void httpSetAuthFormDetails(HttpRoute *route, cchar *loginPage, cchar *loginService, cchar *logoutService,
    cchar *loggedInPage, cchar *loggedOutPage)
{
    HttpRoute   *loginRoute;
    HttpAuth    *auth;

    auth = route->auth;

    if (!route->cookie) {
        httpSetRouteCookie(route, HTTP_SESSION_COOKIE);
    }
    if (loggedInPage) {
        auth->loggedInPage = sclone(loggedInPage);
    }
    if (loginPage) {
        auth->loginPage = sclone(loginPage);
        createLoginRoute(route, auth->loginPage, 0);
    }
    if (loggedOutPage) {
        if (smatch(loginPage, loggedOutPage)) {
            auth->loggedOutPage = auth->loginPage;
        } else {
            auth->loggedOutPage = sclone(loggedOutPage);
            createLoginRoute(route, auth->loggedOutPage, 0);
        }
    }
    /*
        Put services last so they inherit the auth settings above
     */
    if (loginService) {
        loginRoute = createLoginRoute(route, loginService, loginServiceProc);
        httpAddRouteCondition(loginRoute, "auth", 0, 0);
    }
    if (logoutService) {
        createLoginRoute(route, logoutService, logoutServiceProc);
    }
}


/*
    Can supply a roles or abilities in the "abilities" parameter
 */
PUBLIC void httpSetAuthRequiredAbilities(HttpAuth *auth, cchar *abilities)
{
    char    *ability, *tok;

    GRADUATE_HASH(auth, abilities);
    for (ability = stok(sclone(abilities), " \t,", &tok); abilities; abilities = stok(NULL, " \t,", &tok)) {
        httpComputeRoleAbilities(auth, auth->abilities, ability);
    }
}


PUBLIC void httpSetAuthDeny(HttpAuth *auth, cchar *client)
{
    GRADUATE_HASH(auth, deny);
    mprAddKey(auth->deny, sclone(client), auth);
}


PUBLIC void httpSetAuthOrder(HttpAuth *auth, int order)
{
    auth->flags &= (HTTP_ALLOW_DENY | HTTP_DENY_ALLOW);
    auth->flags |= (order & (HTTP_ALLOW_DENY | HTTP_DENY_ALLOW));
}



/*
    Can also achieve this via abilities
 */
PUBLIC void httpSetAuthPermittedUsers(HttpAuth *auth, cchar *users)
{
    char    *user, *tok;

    GRADUATE_HASH(auth, permittedUsers);
    for (user = stok(sclone(users), " \t,", &tok); users; users = stok(NULL, " \t,", &tok)) {
        if (smatch(user, "*")) {
            auth->permittedUsers = 0;
            break;
        } else {
            mprAddKey(auth->permittedUsers, user, user);
        }
    }
}


PUBLIC void httpSetAuthQop(HttpAuth *auth, cchar *qop)
{
    auth->qop = sclone(qop);
}


PUBLIC void httpSetAuthRealm(HttpAuth *auth, cchar *realm)
{
    auth->realm = sclone(realm);
}


PUBLIC void httpSetAuthStoreSessions(HttpAuthStore *store, bool noSession)
{
    assert(store);
    store->noSession = noSession;
}


PUBLIC void httpSetAuthStoreVerify(HttpAuthStore *store, HttpVerifyUser verifyUser)
{
    if (store) {
        store->verifyUser = verifyUser;
    }
}


PUBLIC void httpSetAuthStoreVerifyByName(cchar *name, HttpVerifyUser verifyUser)
{
    httpSetAuthStoreVerify(httpGetAuthStore(name), verifyUser);
}


PUBLIC void httpSetAuthSession(HttpAuth *auth, bool enable)
{
    auth->flags &= ~HTTP_AUTH_NO_SESSION;
    if (!enable) {
        auth->flags |= HTTP_AUTH_NO_SESSION;
    }
}


PUBLIC int httpSetAuthStore(HttpAuth *auth, cchar *store)
{
    if (store == 0 || *store == '\0' || smatch(store, "none")) {
        auth->store = 0;
        return 0;
    }
    if ((auth->store = mprLookupKey(HTTP->authStores, store)) == 0) {
        return MPR_ERR_CANT_FIND;
    }
    if (smatch(store, "system")) {
#if ME_COMPILER_HAS_PAM && ME_HTTP_PAM
        if (auth->type && smatch(auth->type->name, "digest")) {
            mprLog("critical http auth", 0, "Cannot use the PAM password store with digest authentication");
            return MPR_ERR_BAD_ARGS;
        }
#else
        mprLog("critical http auth", 0, "PAM is not supported in the current configuration");
        return MPR_ERR_BAD_ARGS;
#endif
    }
    GRADUATE_HASH(auth, userCache);
    return 0;
}


PUBLIC int httpSetAuthType(HttpAuth *auth, cchar *type, cchar *details)
{
    if (type == 0 || *type == '\0' || smatch(type, "none")) {
        auth->type = 0;
        return 0;
    }
    if ((auth->type = mprLookupKey(HTTP->authTypes, type)) == 0) {
        mprLog("critical http auth", 0, "Cannot find auth type %s", type);
        return MPR_ERR_CANT_FIND;
    }
    if (!auth->store) {
        httpSetAuthStore(auth, "config");
    }
    return 0;
}


/*
    This implements auto-loging without requiring a password
 */
PUBLIC void httpSetAuthUsername(HttpAuth *auth, cchar *username)
{
    auth->username = sclone(username);
}


PUBLIC HttpAuthType *httpLookupAuthType(cchar *type)
{
    return mprLookupKey(HTTP->authTypes, type);
}


/*
    Verify the user password for the "config" store based on the users defined via configuration directives.
    Password may be NULL only if using auto-login.
 */
static bool configVerifyUser(HttpStream *stream, cchar *username, cchar *password)
{
    HttpRx      *rx;
    HttpAuth    *auth;
    bool        success;
    cchar       *requiredPassword;

    rx = stream->rx;
    auth = rx->route->auth;
    if (!stream->user && (stream->user = mprLookupKey(auth->userCache, username)) == 0) {
        httpLog(stream->trace, "auth.login.error", "error", "msg: Unknown user, username:%s", username);
        return 0;
    }
    if (password) {
        if (auth->realm == 0 || *auth->realm == '\0') {
            mprLog("error http auth", 0, "No AuthRealm defined");
        }
        requiredPassword = (rx->passwordDigest) ? rx->passwordDigest : stream->user->password;
        if (sncmp(requiredPassword, "BF", 2) == 0 && slen(requiredPassword) > 4 && isdigit(requiredPassword[2]) &&
                requiredPassword[3] == ':') {
            /* Blowifsh */
            success = mprCheckPassword(sfmt("%s:%s:%s", username, auth->realm, password), stream->user->password);

        } else {
            if (!stream->encoded) {
                password = mprGetMD5(sfmt("%s:%s:%s", username, auth->realm, password));
                stream->encoded = 1;
            }
            success = smatch(password, requiredPassword);
        }
        if (success) {
            httpLog(stream->trace, "auth.login.authenticated", "context", "msg:User authenticated, username:%s", username);
        } else {
            httpLog(stream->trace, "auth.login.error", "error", "msg:Password failed to authenticate, username:%s", username);
        }
        return success;
    }
    return 1;
}


/*
    Web form-based authentication callback for the "form" auth protocol.
    Asks the user to login via a web page.
 */
static void formLogin(HttpStream *stream)
{
    if (stream->rx->route->auth && stream->rx->route->auth->loginPage) {
        httpRedirect(stream, HTTP_CODE_MOVED_TEMPORARILY, stream->rx->route->auth->loginPage);
    } else {
        httpError(stream, HTTP_CODE_UNAUTHORIZED, "Access denied. Login required");
    }
}


PUBLIC int formParse(HttpStream *stream, cchar **username, cchar **password)
{
    *username = httpGetParam(stream, "username", 0);
    *password = httpGetParam(stream, "password", 0);
    if (username && *username == 0) {
        return MPR_ERR_BAD_FORMAT;
    }
    if (password && *password == 0) {
        return MPR_ERR_BAD_FORMAT;
    }
    return 0;
}


#undef  GRADUATE_HASH

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/basic.c ************/

/*
    basic.c - Basic Authorization

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



#if ME_HTTP_BASIC
/*********************************** Code *************************************/
/*
    Parse the 'Authorization' header and the server 'Www-Authenticate' header
 */
PUBLIC int httpBasicParse(HttpStream *stream, cchar **username, cchar **password)
{
    HttpRx  *rx;
    char    *decoded, *cp;

    rx = stream->rx;
    if (password) {
        *password = NULL;
    }
    if (username) {
        *username = NULL;
    }
    if (!rx->authDetails) {
        return 0;
    }
    if ((decoded = mprDecode64(rx->authDetails)) == 0) {
        return MPR_ERR_BAD_FORMAT;
    }
    if ((cp = strchr(decoded, ':')) != 0) {
        *cp++ = '\0';
    }
    stream->encoded = 0;
    if (decoded && *decoded == '\0') {
        return MPR_ERR_BAD_FORMAT;
    }
    if (username) {
        *username = sclone(decoded);
    }
    if (cp && *cp == '\0') {
        return MPR_ERR_BAD_FORMAT;
    }
    if (password) {
        *password = sclone(cp);
    }
    return 0;
}


/*
    Respond to the request by asking for a login
    Only called if not logged in
 */
PUBLIC void httpBasicLogin(HttpStream *stream)
{
    HttpAuth    *auth;

    auth = stream->rx->route->auth;
    if (auth->loginPage && !sends(stream->rx->referrer, auth->loginPage)) {
        httpRedirect(stream, HTTP_CODE_MOVED_TEMPORARILY, auth->loginPage);
    } else {
        httpSetHeader(stream, "WWW-Authenticate", "Basic realm=\"%s\"", auth->realm);
        httpError(stream, HTTP_CODE_UNAUTHORIZED, "Access denied. Login required");
        httpLog(stream->trace, "auth.basic.error", "error", "msg:Access denied. Login required");
    }
}


/*
    Add the 'Authorization' header for authenticated requests
    NOTE: Can do this without first getting a 401 response
 */
PUBLIC bool httpBasicSetHeaders(HttpStream *stream, cchar *username, cchar *password)
{
    httpAddHeader(stream, "Authorization", "basic %s", mprEncode64(sfmt("%s:%s", username, password)));
    return 1;
}

#endif /* ME_HTTP_BASIC */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/cache.c ************/

/*
    cache.c -- Http request route caching

    Caching operates as both a handler and an output filter. If acceptable cached content is found, the
    cacheHandler will serve it instead of the normal handler. If no content is acceptable and caching is enabled
    for the request, the cacheFilter will capture and save the response.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



#if ME_HTTP_CACHE
/********************************** Forwards **********************************/

static void cacheAtClient(HttpStream *stream);
static bool fetchCachedResponse(HttpStream *stream);
static char *makeCacheKey(HttpStream *stream);
static void manageHttpCache(HttpCache *cache, int flags);
static int matchCacheFilter(HttpStream *stream, HttpRoute *route, int dir);
static int matchCacheHandler(HttpStream *stream, HttpRoute *route, int dir);
static void outgoingCacheFilterService(HttpQueue *q);
static void readyCacheHandler(HttpQueue *q);
static void saveCachedResponse(HttpStream *stream);
static cchar *setHeadersFromCache(HttpStream *stream, cchar *content);

/************************************ Code ************************************/

PUBLIC int httpOpenCacheHandler()
{
    HttpStage     *handler, *filter;

    /*
        Create the cache handler to serve cached content
     */
    if ((handler = httpCreateHandler("cacheHandler", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    HTTP->cacheHandler = handler;
    handler->match = matchCacheHandler;
    handler->ready = readyCacheHandler;

    /*
        Create the cache filter to capture and cache response content
     */
    if ((filter = httpCreateFilter("cacheFilter", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    HTTP->cacheFilter = filter;
    filter->match = matchCacheFilter;
    filter->outgoingService = outgoingCacheFilterService;
    return 0;
}


/*
    See if there is acceptable cached content to serve
 */
static int matchCacheHandler(HttpStream *stream, HttpRoute *route, int dir)
{
    HttpCache   *cache;
    HttpRx      *rx;
    HttpTx      *tx;
    cchar       *mimeType, *ukey;
    int         next;

    rx = stream->rx;
    tx = stream->tx;

    /*
        Find first qualifying cache control entry. Any configured uri,method,extension,type must match.
     */
    for (next = 0; (cache = mprGetNextItem(rx->route->caching, &next)) != 0; ) {
        if (cache->uris) {
            if (cache->flags & HTTP_CACHE_HAS_PARAMS) {
                ukey = sfmt("%s?%s", rx->pathInfo, httpGetParamsString(stream));
            } else {
                ukey = rx->pathInfo;
            }
            if (!mprLookupKey(cache->uris, ukey)) {
                continue;
            }
        }
        if (cache->methods && !mprLookupKey(cache->methods, rx->method)) {
            continue;
        }
        if (cache->extensions && !mprLookupKey(cache->extensions, tx->ext)) {
            continue;
        }
        if (cache->types) {
            if ((mimeType = (char*) mprLookupMime(rx->route->mimeTypes, tx->ext)) == 0) {
                continue;
            }
            if (!mprLookupKey(cache->types, mimeType)) {
                continue;
            }
        }
        tx->cache = cache;

        if (cache->flags & HTTP_CACHE_CLIENT) {
            cacheAtClient(stream);
        }
        if (cache->flags & HTTP_CACHE_SERVER) {
            if (!(cache->flags & HTTP_CACHE_MANUAL) && fetchCachedResponse(stream)) {
                /* Found cached content, so we can use the cache handler */
                return HTTP_ROUTE_OK;
            }
            /*
                Caching is configured but no acceptable cached content yet.
                Create a capture buffer for the cacheFilter.
             */
            if (!tx->cacheBuffer) {
                tx->cacheBuffer = mprCreateBuf(-1, -1);
            }
        }
    }
    /*
        Cannot use the cache handler. Note: may still be using the cache filter.
     */
    return HTTP_ROUTE_REJECT;
}


static void readyCacheHandler(HttpQueue *q)
{
    HttpStream  *stream;
    HttpTx      *tx;
    cchar       *data;

    stream = q->stream;
    tx = stream->tx;

    if (tx->cachedContent) {
        if ((data = setHeadersFromCache(stream, tx->cachedContent)) != 0) {
            tx->length = slen(data);
            httpWriteString(q, data);
        }
    }
    httpFinalize(stream);
}


static int matchCacheFilter(HttpStream *stream, HttpRoute *route, int dir)
{
    if ((dir & HTTP_STAGE_TX) && stream->tx->cacheBuffer) {
        return HTTP_ROUTE_OK;
    }
    return HTTP_ROUTE_OMIT_FILTER;
}


/*
    This will be enabled when caching is enabled for the route and there is no acceptable cache data to use.
    OR - manual caching has been enabled.
 */
static void outgoingCacheFilterService(HttpQueue *q)
{
    HttpPacket  *packet, *data;
    HttpStream  *stream;
    HttpTx      *tx;
    MprKey      *kp;
    cchar       *cachedData;
    ssize       size;

    stream = q->stream;
    tx = stream->tx;
    cachedData = 0;

    if (tx->status < 200 || tx->status > 299) {
        tx->cacheBuffer = 0;
    }

    /*
        This routine will save cached responses to tx->cacheBuffer.
        It will also send cached data if the X-SendCache header is present. Normal caching is done by cacheHandler.
     */
    if (mprLookupKey(stream->tx->headers, "X-SendCache") != 0) {
        if (fetchCachedResponse(stream)) {
            httpLog(stream->trace, "cache.sendcache", "context", "msg:Using cached content");
            cachedData = setHeadersFromCache(stream, tx->cachedContent);
            tx->length = slen(cachedData);
        }
    }
    for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
        if (!httpWillNextQueueAcceptPacket(q, packet)) {
            httpPutBackPacket(q, packet);
            return;
        }
        if (packet->flags & HTTP_PACKET_DATA) {
            if (cachedData) {
                /*
                    Using X-SendCache. Discard the packet.
                 */
                continue;

            } else if (tx->cacheBuffer) {
                /*
                    Save the response packet to the cache buffer. Will write below in saveCachedResponse.
                 */
                if (mprGetBufLength(tx->cacheBuffer) == 0) {
                    /*
                        Add defined headers to the start of the cache buffer. Separate with a double newline.
                     */
                    mprPutToBuf(tx->cacheBuffer, "X-Status: %d\n", tx->status);
                    for (kp = 0; (kp = mprGetNextKey(tx->headers, kp)) != 0; ) {
                        mprPutToBuf(tx->cacheBuffer, "%s: %s\n", kp->key, (char*) kp->data);
                    }
                    mprPutCharToBuf(tx->cacheBuffer, '\n');
                }
                size = mprGetBufLength(packet->content);
                if ((tx->cacheBufferLength + size) < stream->limits->cacheItemSize) {
                    mprPutBlockToBuf(tx->cacheBuffer, mprGetBufStart(packet->content), mprGetBufLength(packet->content));
                    tx->cacheBufferLength += size;
                } else {
                    tx->cacheBuffer = 0;
                    httpLog(stream->trace, "cache.big", "context", "msg:Item too big to cache, size:%zu, limit:%u",
                        tx->cacheBufferLength + size, stream->limits->cacheItemSize);
                }
            }

        } else if (packet->flags & HTTP_PACKET_END) {
            if (cachedData) {
                /*
                    Using X-SendCache but there was no data packet to replace. So do the write here.
                 */
                data = httpCreateDataPacket((ssize) tx->length);
                mprPutBlockToBuf(data->content, cachedData, (ssize) tx->length);
                httpPutPacketToNext(q, data);

            } else if (tx->cacheBuffer) {
                /*
                    Save the cache buffer to the cache store
                 */
                saveCachedResponse(stream);
            }
        }
        httpPutPacketToNext(q, packet);
    }
}


/*
    Find a qualifying cache control entry. Any configured uri,method,extension,type must match.
 */
static void cacheAtClient(HttpStream *stream)
{
    HttpTx      *tx;
    HttpCache   *cache;
    cchar       *value;

    tx = stream->tx;
    cache = stream->tx->cache;

    if (tx->status == HTTP_CODE_OK && !mprLookupKey(tx->headers, "Cache-Control")) {
        if ((value = mprLookupKey(stream->tx->headers, "Cache-Control")) != 0) {
            if (strstr(value, "max-age") == 0) {
                httpAppendHeader(stream, "Cache-Control", "public, max-age=%lld", cache->clientLifespan / TPS);
            }
        } else {
            httpAddHeader(stream, "Cache-Control", "public, max-age=%lld", cache->clientLifespan / TPS);
            /*
                Old HTTP/1.0 clients don't understand Cache-Control
             */
            httpAddHeaderString(stream, "Expires", mprFormatUniversalTime(MPR_HTTP_DATE,
                mprGetTime() + cache->clientLifespan));
        }
    }
}


/*
    See if there is acceptable cached content for this request. If so, return true.
    Will setup tx->cacheBuffer as a side-effect if the output should be captured and cached.
 */
static bool fetchCachedResponse(HttpStream *stream)
{
    HttpTx      *tx;
    MprTime     modified, when;
    cchar       *value, *key, *tag;
    int         status, cacheOk, canUseClientCache;

    tx = stream->tx;

    /*
        Transparent caching. Manual caching must manually call httpWriteCached()
     */
    key = makeCacheKey(stream);
    if ((value = httpGetHeader(stream, "Cache-Control")) != 0 &&
            (scontains(value, "max-age=0") == 0 || scontains(value, "no-cache") == 0)) {
        httpLog(stream->trace, "cache.reload", "context", "msg:Client reload");

    } else if ((tx->cachedContent = mprReadCache(stream->host->responseCache, key, &modified, 0)) != 0) {
        /*
            See if a NotModified response can be served. This is much faster than sending the response.
            Observe headers:
                If-None-Match: "ec18d-54-4d706a63"
                If-Modified-Since: Fri, 04 Mar 2014 04:28:19 GMT
            Set status to OK when content must be transmitted.
         */
        cacheOk = 1;
        canUseClientCache = 0;
        tag = mprGetMD5(key);
        if ((value = httpGetHeader(stream, "If-None-Match")) != 0) {
            canUseClientCache = 1;
            if (scmp(value, tag) != 0) {
                cacheOk = 0;
            }
        }
        if (cacheOk && (value = httpGetHeader(stream, "If-Modified-Since")) != 0) {
            canUseClientCache = 1;
            mprParseTime(&when, value, 0, 0);
            if (modified > when) {
                cacheOk = 0;
            }
        }
        status = (canUseClientCache && cacheOk) ? HTTP_CODE_NOT_MODIFIED : HTTP_CODE_OK;
        httpLog(stream->trace, "cache.cached", "context", "msg:Use cached content, key:%s, status:%d", key, status);
        httpSetStatus(stream, status);
        httpSetHeaderString(stream, "Etag", mprGetMD5(key));
        httpSetHeaderString(stream, "Last-Modified", mprFormatUniversalTime(MPR_HTTP_DATE, modified));
        httpRemoveHeader(stream, "Content-Encoding");
        return 1;
    }
    httpLog(stream->trace, "cache.none", "context", "msg:No cached content, key:%s", key);
    return 0;
}


static void saveCachedResponse(HttpStream *stream)
{
    HttpTx      *tx;
    MprBuf      *buf;
    MprTime     modified;

    tx = stream->tx;
    assert(tx->finalizedOutput && tx->cacheBuffer);

    buf = tx->cacheBuffer;
    tx->cacheBuffer = 0;
    /*
        Truncate modified time to get a 1 sec resolution. This is the resolution for If-Modified headers.
     */
    modified = mprGetTime() / TPS * TPS;
    mprWriteCache(stream->host->responseCache, makeCacheKey(stream), mprGetBufStart(buf), modified,
        tx->cache->serverLifespan, 0, 0);
}


PUBLIC ssize httpWriteCached(HttpStream *stream)
{
    MprTime     modified;
    cchar       *cacheKey, *data, *content;

    if (!stream->tx->cache) {
        return MPR_ERR_CANT_FIND;
    }
    cacheKey = makeCacheKey(stream);
    if ((content = mprReadCache(stream->host->responseCache, cacheKey, &modified, 0)) == 0) {
        httpLog(stream->trace, "cache.none", "context", "msg:No response data in cache, key:%s", cacheKey);
        return 0;
    }
    httpLog(stream->trace, "cache.cached", "context", "msg:Used cached response, key:%s", cacheKey);
    data = setHeadersFromCache(stream, content);
    httpSetHeaderString(stream, "Etag", mprGetMD5(cacheKey));
    httpSetHeaderString(stream, "Last-Modified", mprFormatUniversalTime(MPR_HTTP_DATE, modified));
    stream->tx->cacheBuffer = 0;
    httpWriteString(stream->writeq, data);
    httpFinalizeOutput(stream);
    return slen(data);
}


PUBLIC ssize httpUpdateCache(HttpStream *stream, cchar *uri, cchar *data, MprTicks lifespan)
{
    cchar   *key;
    ssize   len;

    len = slen(data);
    if (len > stream->limits->cacheItemSize) {
        return MPR_ERR_WONT_FIT;
    }
    if (lifespan <= 0) {
        lifespan = stream->rx->route->lifespan;
    }
    key = sfmt("http::response::%s", uri);
    if (data == 0 || lifespan <= 0) {
        mprRemoveCache(stream->host->responseCache, key);
        return 0;
    }
    return mprWriteCache(stream->host->responseCache, key, data, 0, lifespan, 0, 0);
}


/*
    Add cache configuration to the route. This can be called multiple times.
    Uris, extensions and methods may optionally provide a space or comma separated list of items.
    If URI is NULL or "*", cache all URIs for this route. Otherwise, cache only the given URIs.
    The URIs may contain an ordered set of request parameters. For example: "/user/show?name=john&posts=true"
    Note: the URI should not include the route prefix (scriptName)
    The extensions should not contain ".". The methods may contain "*" for all methods.
 */
PUBLIC void httpAddCache(HttpRoute *route, cchar *methods, cchar *uris, cchar *extensions, cchar *types,
        MprTicks clientLifespan, MprTicks serverLifespan, int flags)
{
    HttpCache   *cache;
    char        *item, *tok;

    cache = 0;
    if (!route->caching) {
        if (route->handler) {
            mprLog("error http cache", 0,
                "Caching handler disabled because SetHandler used in route %s. Use AddHandler instead", route->pattern);
        }
        httpAddRouteHandler(route, "cacheHandler", NULL);
        httpAddRouteFilter(route, "cacheFilter", "", HTTP_STAGE_TX);
        route->caching = mprCreateList(0, MPR_LIST_STABLE);

    } else if (flags & HTTP_CACHE_RESET) {
        route->caching = mprCreateList(0, MPR_LIST_STABLE);

    } else if (route->parent && route->caching == route->parent->caching) {
        route->caching = mprCloneList(route->parent->caching);
    }
    if ((cache = mprAllocObj(HttpCache, manageHttpCache)) == 0) {
        return;
    }
    if (extensions) {
        cache->extensions = mprCreateHash(0, MPR_HASH_STABLE);
        for (item = stok(sclone(extensions), " \t,", &tok); item; item = stok(0, " \t,", &tok)) {
            if (*item && !smatch(item, "*")) {
                mprAddKey(cache->extensions, item, cache);
            }
        }
    } else if (types) {
        cache->types = mprCreateHash(0, MPR_HASH_STABLE);
        for (item = stok(sclone(types), " \t,", &tok); item; item = stok(0, " \t,", &tok)) {
            if (*item && !smatch(item, "*")) {
                mprAddKey(cache->types, item, cache);
            }
        }
    } else if (flags & HTTP_CACHE_STATIC) {
        cache->extensions = mprCreateHash(0, MPR_HASH_STABLE);
        mprAddKey(cache->extensions, "css", cache);
        mprAddKey(cache->extensions, "gif", cache);
        mprAddKey(cache->extensions, "ico", cache);
        mprAddKey(cache->extensions, "jpg", cache);
        mprAddKey(cache->extensions, "js", cache);
        mprAddKey(cache->extensions, "html", cache);
        mprAddKey(cache->extensions, "png", cache);
        mprAddKey(cache->extensions, "pdf", cache);
        mprAddKey(cache->extensions, "ttf", cache);
        mprAddKey(cache->extensions, "txt", cache);
        mprAddKey(cache->extensions, "xml", cache);
        mprAddKey(cache->extensions, "woff", cache);
    }
    if (methods) {
        cache->methods = mprCreateHash(0, MPR_HASH_CASELESS | MPR_HASH_STABLE);
        for (item = stok(sclone(methods), " \t,", &tok); item; item = stok(0, " \t,", &tok)) {
            if (smatch(item, "*")) {
                methods = 0;
            } else {
                mprAddKey(cache->methods, item, cache);
            }
        }
    }
    if (uris) {
        cache->uris = mprCreateHash(0, MPR_HASH_STABLE);
        for (item = stok(sclone(uris), " \t,", &tok); item; item = stok(0, " \t,", &tok)) {
            mprAddKey(cache->uris, item, cache);
            if (schr(item, '?')) {
                flags |= HTTP_CACHE_UNIQUE;
            }
        }
    }
    if (clientLifespan <= 0) {
        clientLifespan = route->lifespan;
    }
    cache->clientLifespan = clientLifespan;
    if (serverLifespan <= 0) {
        serverLifespan = route->lifespan;
    }
    cache->serverLifespan = serverLifespan;
    cache->flags = flags;
    mprAddItem(route->caching, cache);
}


static void manageHttpCache(HttpCache *cache, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(cache->extensions);
        mprMark(cache->methods);
        mprMark(cache->types);
        mprMark(cache->uris);
    }
}


static char *makeCacheKey(HttpStream *stream)
{
    HttpRx      *rx;

    rx = stream->rx;
    if (stream->tx->cache->flags & HTTP_CACHE_UNIQUE) {
        return sfmt("http::response::%s%s?%s", rx->route->prefix, rx->pathInfo, httpGetParamsString(stream));
    } else {
        return sfmt("http::response::%s%s", rx->route->prefix, rx->pathInfo);
    }
}


/*
    Parse cached content of the form:  headers \n\n data
    Set headers in the current request and return a reference to the data portion
 */
static cchar *setHeadersFromCache(HttpStream *stream, cchar *content)
{
    cchar   *data;
    char    *header, *headers, *key, *value, *tok;

    if ((data = strstr(content, "\n\n")) == 0) {
        data = content;
    } else {
        headers = snclone(content, data - content);
        data += 2;
        for (header = stok(headers, "\n", &tok); header; header = stok(NULL, "\n", &tok)) {
            key = ssplit(header, ": ", &value);
            if (smatch(key, "X-Status")) {
                stream->tx->status = (int) stoi(value);
            } else {
                httpAddHeaderString(stream, key, value);
            }
        }
    }
    return data;
}

#endif /* ME_HTTP_CACHE */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/chunkFilter.c ************/

/*
    chunkFilter.c - Transfer chunk encoding filter.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/********************************** Forwards **********************************/

static void incomingChunk(HttpQueue *q, HttpPacket *packet);
static bool needChunking(HttpQueue *q);
static void outgoingChunkService(HttpQueue *q);
static void setChunkPrefix(HttpQueue *q, HttpPacket *packet);

/*********************************** Code *************************************/
/*
   Loadable module initialization
 */
PUBLIC int httpOpenChunkFilter()
{
    HttpStage     *filter;

    if ((filter = httpCreateFilter("chunkFilter", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    HTTP->chunkFilter = filter;
    filter->flags |= HTTP_STAGE_INTERNAL;
    filter->incoming = incomingChunk;
    filter->outgoingService = outgoingChunkService;
    return 0;
}


PUBLIC void httpInitChunking(HttpStream *stream)
{
    HttpRx      *rx;

    rx = stream->rx;

    /*
        remainingContent will be revised by the chunk filter as chunks are processed and will
        be set to zero when the tail chunk has been received.
     */
    rx->flags |= HTTP_CHUNKED;
    rx->chunkState = HTTP_CHUNK_START;
    rx->remainingContent = HTTP_UNLIMITED;
    rx->needInputPipeline = 1;
}


/*
    Filter chunk headers and leave behind pure data. This is called for chunked and unchunked data.
    Unchunked data is simply passed upstream. Chunked data format is:
        Chunk spec <CRLF>
        Data <CRLF>
        Chunk spec (size == 0) <CRLF>
        <CRLF>
    Chunk spec is: "HEX_COUNT; chunk length DECIMAL_COUNT\r\n". The "; chunk length DECIMAL_COUNT is optional.
    As an optimization, use "\r\nSIZE ...\r\n" as the delimiter so that the CRLF after data does not special consideration.
    Achive this by parseHeaders reversing the input start by 2.

    Return number of bytes available to read.
    NOTE: may set rx->eof and return 0 bytes on EOF.
 */
static void incomingChunk(HttpQueue *q, HttpPacket *packet)
{
    HttpStream  *stream;
    HttpPacket  *tail;
    HttpRx      *rx;
    MprBuf      *buf;
    ssize       chunkSize, len, nbytes;
    char        *start, *cp;
    int         bad;

    stream = q->stream;
    rx = stream->rx;
    len = httpGetPacketLength(packet);

    if (!httpWillNextQueueAcceptPacket(q, packet)) {
        httpPutBackPacket(q, packet);
        return;
    }

    if (rx->chunkState == HTTP_CHUNK_UNCHUNKED) {
        if (rx->remainingContent > 0) {
            nbytes = min(rx->remainingContent, httpGetPacketLength(packet));
            rx->bytesRead += nbytes;
            rx->remainingContent -= nbytes;
        }
        httpPutPacketToNext(q, packet);
        if (rx->remainingContent <= 0 && !(packet->flags & HTTP_PACKET_END)) {
            httpAddInputEndPacket(stream, q->nextQ);
        }
        return;
    }

    httpJoinPacketForService(q, packet, HTTP_DELAY_SERVICE);

    for (packet = httpGetPacket(q); packet && !stream->error && !rx->eof; packet = httpGetPacket(q)) {
        while (packet && !stream->error && !rx->eof) {
            switch (rx->chunkState) {
            case HTTP_CHUNK_UNCHUNKED:
                httpError(stream, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad chunk state");
                return;

            case HTTP_CHUNK_DATA:
                len = httpGetPacketLength(packet);
                nbytes = min(rx->remainingContent, len);
                rx->remainingContent -= nbytes;
                rx->bytesRead += nbytes;
                if (nbytes < len && (tail = httpSplitPacket(packet, nbytes)) != 0) {
                    httpPutPacketToNext(q, packet);
                    packet = tail;
                } else if (len > 0) {
                    httpPutPacketToNext(q, packet);
                    packet = 0;
                }
                if (rx->remainingContent <= 0) {
                    /* End of chunk - prep for the next chunk */
                    rx->remainingContent = ME_BUFSIZE;
                    rx->chunkState = HTTP_CHUNK_START;
                }
                if (!packet) {
                    break;
                }
                /* Fall through */

            case HTTP_CHUNK_START:
                /*
                    Validate:  "\r\nSIZE.*\r\n"
                 */
                buf = packet->content;
                if (mprGetBufLength(buf) < 5) {
                    httpJoinPacketForService(q, packet, HTTP_DELAY_SERVICE);
                    return;
                }
                start = mprGetBufStart(buf);
                bad = (start[0] != '\r' || start[1] != '\n');
                for (cp = &start[2]; cp < buf->end && *cp != '\n'; cp++) {}
                if (cp >= buf->end || (*cp != '\n' && (cp - start) < 80)) {
                    httpJoinPacketForService(q, packet, HTTP_DELAY_SERVICE);
                    return;
                }
                bad += (cp[-1] != '\r' || cp[0] != '\n');
                if (bad) {
                    httpError(stream, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
                    return;
                }
                chunkSize = (int) stoiradix(&start[2], 16, NULL);
                if (!isxdigit((uchar) start[2]) || chunkSize < 0) {
                    httpError(stream, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
                    return;
                }
                if (chunkSize == 0) {
                    /*
                        Last chunk. Consume the final "\r\n".
                     */
                    if ((cp + 2) >= buf->end) {
                        return;
                    }
                    cp += 2;
                    bad += (cp[-1] != '\r' || cp[0] != '\n');
                    if (bad) {
                        httpError(stream, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad final chunk specification");
                        return;
                    }
                }
                mprAdjustBufStart(buf, (cp - start + 1));
                /* Remaining content is set to the next chunk size */
                rx->remainingContent = chunkSize;
                if (chunkSize == 0) {
                    rx->chunkState = HTTP_CHUNK_EOF;
                    httpAddInputEndPacket(stream, q->nextQ);
                } else {
                    assert(!rx->eof);
                    rx->chunkState = HTTP_CHUNK_DATA;
                }
                break;

            case HTTP_CHUNK_EOF:
                break;

            default:
                httpError(stream, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad chunk state %d", rx->chunkState);
                return;
            }
        }
#if HTTP_PIPELINING
        /* HTTP/1.1 pipelining is not implemented reliably by modern browsers (WARNING: dont use) */
        if (packet && httpGetPacketLength(packet)) {
            httpPutPacket(stream->inputq, tail);
        }
#endif
    }
    if (packet) {
        /* Transfer END packet */
        httpPutPacketToNext(q, packet);
    }
}


static void outgoingChunkService(HttpQueue *q)
{
    HttpStream  *stream;
    HttpPacket  *packet, *finalChunk;
    HttpTx      *tx;

    stream = q->stream;
    tx = stream->tx;

    /*
        Try to determine a content length. If all the data is buffered, we know the length and can do an optimized
        transfer without encoding. We do this here rather than in a put() routine to allow data to buffer.
     */
    if (!(q->flags & HTTP_QUEUE_SERVICED)) {
        tx->needChunking = needChunking(q);
    }
    if (!tx->needChunking) {
        /*
            Join packets if possible to get as few writes as possible. httpWriteBlock can create many small packets.
         */
        if (!stream->upgraded) {
            httpJoinPackets(q, -1);
        }
        //  Must transfer all packets before removing the queue from the pipeline.
        httpTransferPackets(q, q->nextQ);
        httpRemoveQueue(q);
        httpLog(stream->trace, "chunkFilter", "detail", "msg:remove outgoing chunk filter");
        httpTraceQueues(stream);
        return;
    }
    for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
        if (packet->flags & HTTP_PACKET_DATA) {
            httpPutBackPacket(q, packet);
            httpJoinPackets(q, tx->chunkSize);
            packet = httpGetPacket(q);
            if (httpGetPacketLength(packet) > tx->chunkSize) {
                httpResizePacket(q, packet, tx->chunkSize);
            }
        }
        if (!httpWillNextQueueAcceptPacket(q, packet)) {
            httpPutBackPacket(q, packet);
            return;
        }
        if (packet->flags & HTTP_PACKET_DATA) {
            setChunkPrefix(q, packet);

        } else if (packet->flags & HTTP_PACKET_END) {
            /* Insert a packet for the final chunk */
            finalChunk = httpCreateDataPacket(0);
            setChunkPrefix(q, finalChunk);
            httpPutPacketToNext(q, finalChunk);
        }
        httpPutPacketToNext(q, packet);
    }
}


static bool needChunking(HttpQueue *q)
{
    HttpStream  *stream;
    HttpTx      *tx;
    cchar       *value;

    stream = q->stream;
    tx = stream->tx;

    if (stream->net->protocol >= 2 || stream->upgraded) {
        return 0;
    }
    /*
        If we don't know the content length yet (tx->length < 0) and if the tail packet is the end packet. Then
        we have all the data. Thus we can determine the actual content length and can bypass the chunk handler.
     */
    if (tx->length < 0 && (value = mprLookupKey(tx->headers, "Content-Length")) != 0) {
        tx->length = stoi(value);
    }
    if (tx->length < 0 && tx->chunkSize < 0) {
        if (q->last && q->last->flags & HTTP_PACKET_END) {
            if (q->count > 0) {
                tx->length = q->count;
            }
        } else {
            tx->chunkSize = min(stream->limits->chunkSize, q->max);
        }
    }
    if (tx->flags & HTTP_TX_USE_OWN_HEADERS || stream->net->protocol != 1) {
        tx->chunkSize = -1;
    }
    return tx->chunkSize > 0;
}


static void setChunkPrefix(HttpQueue *q, HttpPacket *packet)
{
    if (packet->prefix) {
        return;
    }
    packet->prefix = mprCreateBuf(32, 32);
    /*
        NOTE: prefixes don't count in the queue length. No need to adjust q->count
     */
    if (httpGetPacketLength(packet)) {
        mprPutToBuf(packet->prefix, "\r\n%zx\r\n", httpGetPacketLength(packet));
    } else {
        mprPutStringToBuf(packet->prefix, "\r\n0\r\n\r\n");
    }
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/client.c ************/

/*
    client.c -- Client side specific support.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/********************************* Forwards ***********************************/

static void setDefaultHeaders(HttpStream *stream);
static int clientRequest(HttpStream *stream, cchar *method, cchar *uri, cchar *data, char **err);

/*********************************** Code *************************************/
/*
    Get the IP:PORT for a request URI
 */
PUBLIC void httpGetUriAddress(HttpUri *uri, cchar **ip, int *port)
{
    Http    *http;

    http = HTTP;

    if (!uri->host) {
        *ip = (http->proxyHost) ? http->proxyHost : http->defaultClientHost;
        *port = (http->proxyHost) ? http->proxyPort : uri->port;
    } else {
        *ip = (http->proxyHost) ? http->proxyHost : uri->host;
        *port = (http->proxyHost) ? http->proxyPort : uri->port;
    }
    if (*port == 0) {
        *port = (uri->secure) ? 443 : http->defaultClientPort;
    }
}


/*
    Determine if the current network connection can handle the current URI without redirection
 */
static bool canUse(HttpNet *net, HttpStream *stream, HttpUri *uri, MprSsl *ssl, cchar *ip, int port)
{
    MprSocket   *sock;

    assert(net);

    if ((sock = net->sock) == 0) {
        return 0;
    }
    if (port != net->port || !smatch(ip, net->ip) || uri->secure != (sock->ssl != 0) || sock->ssl != ssl) {
        return 0;
    }
    if (net->eof || net->protocol == 0 || (net->protocol < 2 && stream->keepAliveCount <= 1)) {
        return 0;
    }
    return 1;
}


/*
    Connect to a new URL using the given method.
    If the stream is already connected to a suitable network, it is reused.
 */
PUBLIC int httpConnect(HttpStream *stream, cchar *method, cchar *url, MprSsl *ssl)
{
    HttpNet     *net;
    HttpTx      *tx;
    HttpUri     *uri;
    cchar       *ip;
    int         port;

    assert(stream);
    assert(method && *method);
    assert(url && *url);

    net = stream->net;
    if (httpServerStream(stream)) {
        mprLog("client error", 0, "Cannot call httpConnect() on a server stream");
        return MPR_ERR_BAD_STATE;
    }
    if (net->protocol < 0) {
        //  Default to HTTP/1.1
        net->protocol = 1;
    }
    if (stream->tx == 0 || stream->state != HTTP_STATE_BEGIN) {
        httpResetClientStream(stream, 0);
    }
    tx = stream->tx;
    tx->method = supper(method);
    stream->authRequested = 0;
    stream->startMark = mprGetHiResTicks();

    if ((uri = tx->parsedUri = httpCreateUri(url, HTTP_COMPLETE_URI_PATH)) == 0) {
        return MPR_ERR_BAD_ARGS;
    }
    ssl = uri->secure ? (ssl ? ssl : mprCreateSsl(0)) : 0;
    httpGetUriAddress(uri, &ip, &port);

    if (net->sock) {
        if (net->error) {
            mprCloseSocket(net->sock, 0);
            net->sock = 0;
            net->eof = 0;

        } else if (canUse(net, stream, uri, ssl, ip, port)) {
            httpLog(stream->trace, "client.connection.reuse", "context", "msg:Reuse connection, IP:%s, PORT:%d, KeepAlive:%d", ip, port, stream->keepAliveCount);

        } else if (net->protocol >= 2) {
            if (mprGetListLength(net->streams) > 1) {
                httpError(stream, HTTP_CODE_COMMS_ERROR, "Cannot use network for %s due to other existing requests", ip);
                return MPR_ERR_CANT_FIND;
            }
        } else {
            mprCloseSocket(net->sock, 0);
            net->sock = 0;
            net->eof = 0;
        }
    }
    if (!net->sock) {
        if (httpConnectNet(net, ip, port, ssl) < 0) {
            httpNetError(net, "Cannot connect to %s:%d", ip, port);
            return MPR_ERR_CANT_CONNECT;
        }
        stream->net = net;
        stream->sock = net->sock;
        stream->ip = net->ip;
        stream->port = net->port;
        stream->keepAliveCount = (net->protocol >= 2) ? 0 : stream->limits->keepAliveMax;

#if ME_HTTP_WEB_SOCKETS
        if (net->protocol == 1 && uri->webSockets && httpUpgradeWebSocket(stream) < 0) {
            stream->errorMsg = net->errorMsg = net->sock->errorMsg;
            return 0;
        }
#endif
    }
    httpCreatePipeline(stream);
    tx->started = 1;

    setDefaultHeaders(stream);
    httpSetState(stream, HTTP_STATE_CONNECTED);
    httpPumpOutput(stream->writeq);

    httpServiceNetQueues(net, 0);
    httpEnableNetEvents(net);
    return 0;
}


static void setDefaultHeaders(HttpStream *stream)
{
    HttpAuthType    *ap;

    assert(stream);

    if (stream->username && stream->authType && ((ap = httpLookupAuthType(stream->authType)) != 0)) {
        if ((ap->setAuth)(stream, stream->username, stream->password)) {
            stream->authRequested = 1;
        }
    }
    if (stream->net->protocol < 2) {
        if (stream->port != 80 && stream->port != 443) {
            if (schr(stream->ip, ':')) {
                httpAddHeader(stream, "Host", "[%s]:%d", stream->ip, stream->port);
            } else {
                httpAddHeader(stream, "Host", "%s:%d", stream->ip, stream->port);
            }
        } else {
            httpAddHeaderString(stream, "Host", stream->ip);
        }
        if (stream->keepAliveCount > 0) {
            httpSetHeaderString(stream, "Connection", "Keep-Alive");
        } else {
            httpSetHeaderString(stream, "Connection", "close");
        }
    }
    httpAddHeaderString(stream, "Accept", "*/*");
}


/*
    Check the response for authentication failures and redirections. Return true if a retry is requried.
 */
PUBLIC bool httpNeedRetry(HttpStream *stream, cchar **url)
{
    HttpRx          *rx;
    HttpAuthType    *authType;

    assert(stream->rx);

    *url = 0;
    rx = stream->rx;

    if (stream->error || stream->state < HTTP_STATE_FIRST) {
        return 0;
    }
    if (rx->status == HTTP_CODE_UNAUTHORIZED) {
        if (stream->username == 0 || stream->authType == 0) {
            httpError(stream, rx->status, "Authentication required");

        } else if (stream->authRequested && smatch(stream->authType, rx->authType)) {
            httpError(stream, rx->status, "Authentication failed, wrong authentication type");

        } else {
            assert(httpClientStream(stream));
            if (stream->authType && (authType = httpLookupAuthType(stream->authType)) != 0) {
                (authType->parseAuth)(stream, NULL, NULL);
            }
            return 1;
        }

    } else if (HTTP_CODE_MOVED_PERMANENTLY <= rx->status && rx->status <= HTTP_CODE_MOVED_TEMPORARILY && stream->followRedirects) {
        if (rx->redirect) {
            *url = rx->redirect;
            return 1;
        }
        httpError(stream, rx->status, "Missing location header");
        return 0;
    }
    return 0;
}


/*
    Set the request as being a multipart mime upload. This defines the content type and defines a multipart mime boundary
 */
PUBLIC void httpEnableUpload(HttpStream *stream)
{
    stream->boundary = sfmt("--BOUNDARY--%lld", stream->http->now);
    httpSetHeader(stream, "Content-Type", "multipart/form-data; boundary=%s", &stream->boundary[2]);
}


/*
    Read data. If sync mode, this will block. If async, will never block.
    Will return what data is available up to the requested size.
    Timeout in milliseconds to wait. Set to -1 to use the default inactivity timeout. Set to zero to wait forever.
    Returns a count of bytes read. Returns zero if no data. EOF if returns zero and stream->state is > HTTP_STATE_CONTENT.
 */
PUBLIC ssize httpReadBlock(HttpStream *stream, char *buf, ssize size, MprTicks timeout, int flags)
{
    HttpPacket  *packet;
    HttpQueue   *q;
    HttpLimits  *limits;
    MprBuf      *content;
    MprTicks    start, delay;
    ssize       nbytes, len;
    int64       dispatcherMark;

    q = stream->readq;
    assert(q->count >= 0);
    assert(size >= 0);
    limits = stream->limits;

    if (flags == 0) {
        flags = stream->net->async ? HTTP_NON_BLOCK : HTTP_BLOCK;
    }
    if (timeout < 0) {
        timeout = limits->inactivityTimeout;
    } else if (timeout == 0) {
        timeout = MPR_MAX_TIMEOUT;
    }
    if (flags & HTTP_BLOCK) {
        start = stream->http->now;
        dispatcherMark = mprGetEventMark(stream->dispatcher);
        while (q->count <= 0 && !stream->error && (stream->state <= HTTP_STATE_CONTENT)) {
            if (httpRequestExpired(stream, -1)) {
                break;
            }
            delay = min(limits->inactivityTimeout, mprGetRemainingTicks(start, timeout));
            httpEnableNetEvents(stream->net);
            mprWaitForEvent(stream->dispatcher, delay, dispatcherMark);
            if (mprGetRemainingTicks(start, timeout) <= 0) {
                break;
            }
            dispatcherMark = mprGetEventMark(stream->dispatcher);
        }
    }
    for (nbytes = 0; size > 0 && q->count > 0; ) {
        if ((packet = q->first) == 0) {
            break;
        }
        content = packet->content;
        len = mprGetBufLength(content);
        len = min(len, size);
        assert(len <= q->count);
        if (len > 0) {
            len = mprGetBlockFromBuf(content, buf, len);
        }
        buf += len;
        size -= len;
        q->count -= len;
        assert(q->count >= 0);
        nbytes += len;
        if (mprGetBufLength(content) == 0) {
            httpGetPacket(q);

        }
        if (flags & HTTP_NON_BLOCK) {
            break;
        }
    }
    assert(q->count >= 0);
    if (nbytes < size) {
        buf[nbytes] = '\0';
    }
    if (nbytes == 0 && httpRequestExpired(stream, -1)) {
        return MPR_ERR_TIMEOUT;
    }
    return nbytes;
}


/*
    Read with standard connection timeouts and in blocking mode for clients, non-blocking for server-side
 */
PUBLIC ssize httpRead(HttpStream *stream, char *buf, ssize size)
{
    return httpReadBlock(stream, buf, size, -1, 0);
}


PUBLIC char *httpReadString(HttpStream *stream)
{
    HttpRx      *rx;
    ssize       sofar, nbytes, remaining;
    char        *content;

    rx = stream->rx;
    if (rx->length < 0) {
        return 0;
    }
    remaining = (ssize) min(MAXSSIZE, rx->length);

    if (remaining > 0) {
        if ((content = mprAlloc(remaining + 1)) == 0) {
            return 0;
        }
        sofar = 0;
        while (remaining > 0) {
            nbytes = httpRead(stream, &content[sofar], remaining);
            if (nbytes < 0) {
                return 0;
            }
            sofar += nbytes;
            remaining -= nbytes;
        }
    } else {
        content = NULL;
        sofar = 0;
        while (1) {
            if ((content = mprRealloc(content, sofar + ME_BUFSIZE)) == 0) {
                return 0;
            }
            nbytes = httpRead(stream, &content[sofar], ME_BUFSIZE);
            if (nbytes < 0) {
                return 0;
            } else if (nbytes == 0) {
                break;
            }
            sofar += nbytes;
        }
    }
    content[sofar] = '\0';
    return content;
}


/*
    Convenience method to issue a client http request.
    Assumes the Mpr and Http services are created and initialized.
 */
PUBLIC HttpStream *httpRequest(cchar *method, cchar *uri, cchar *data, int protocol, char **err)
{
    HttpNet         *net;
    HttpStream      *stream;
    MprDispatcher   *dispatcher;

    assert(err);
    dispatcher = mprCreateDispatcher("httpRequest", MPR_DISPATCHER_AUTO);
    mprStartDispatcher(dispatcher);

    net = httpCreateNet(dispatcher, NULL, protocol, 0);
    if ((stream = httpCreateStream(net, 0)) == 0) {
        return 0;
    }
    mprAddRoot(stream);

    if (clientRequest(stream, method, uri, data, err) < 0) {
        mprRemoveRoot(stream);
        httpDestroyNet(net);
        return 0;
    }
    mprRemoveRoot(stream);
    return stream;
}


static int clientRequest(HttpStream *stream, cchar *method, cchar *uri, cchar *data, char **err)
{
    ssize   len;

    /*
       Open a connection to issue the request. Then finalize the request output - this forces the request out.
     */
    *err = 0;
    if (httpConnect(stream, method, uri, NULL) < 0) {
        *err = sfmt("Cannot connect to %s", uri);
        return MPR_ERR_CANT_CONNECT;
    }
    if (data) {
        len = slen(data);
        if (httpWriteBlock(stream->writeq, data, len, HTTP_BLOCK) != len) {
            *err = sclone("Cannot write request body data");
            return MPR_ERR_CANT_WRITE;
        }
    }
    httpFinalizeOutput(stream);
    if (httpWait(stream, HTTP_STATE_CONTENT, MPR_MAX_TIMEOUT) < 0) {
        *err = sclone("No response");
        return MPR_ERR_BAD_STATE;
    }
    return 0;
}


static int blockingFileCopy(HttpStream *stream, cchar *path)
{
    MprFile     *file;
    char        buf[ME_BUFSIZE];
    ssize       bytes, nbytes, offset;

    file = mprOpenFile(path, O_RDONLY | O_BINARY, 0);
    if (file == 0) {
        mprLog("error http client", 0, "Cannot open %s", path);
        return MPR_ERR_CANT_OPEN;
    }
    mprAddRoot(file);
    while ((bytes = mprReadFile(file, buf, sizeof(buf))) > 0) {
        offset = 0;
        while (bytes > 0) {
            if ((nbytes = httpWriteBlock(stream->writeq, &buf[offset], bytes, HTTP_BLOCK)) < 0) {
                mprCloseFile(file);
                mprRemoveRoot(file);
                return MPR_ERR_CANT_WRITE;
            }
            bytes -= nbytes;
            offset += nbytes;
            assert(bytes >= 0);
        }
    }
    httpFlushQueue(stream->writeq, HTTP_BLOCK);
    mprCloseFile(file);
    mprRemoveRoot(file);
    return 0;
}


/*
    Write upload data. This routine blocks. If you need non-blocking ... cut and paste.
 */
PUBLIC ssize httpWriteUploadData(HttpStream *stream, MprList *fileData, MprList *formData)
{
    char    *path, *pair, *key, *value, *name;
    cchar   *type;
    ssize   rc;
    int     next;

    rc = 0;
    if (formData) {
        for (rc = next = 0; rc >= 0 && (pair = mprGetNextItem(formData, &next)) != 0; ) {
            key = ssplit(sclone(pair), "=", &value);
            rc += httpWrite(stream->writeq, "%s\r\nContent-Disposition: form-data; name=\"%s\";\r\n", stream->boundary, key);
            rc += httpWrite(stream->writeq, "Content-Type: application/x-www-form-urlencoded\r\n\r\n%s\r\n", value);
        }
    }
    if (fileData) {
        for (rc = next = 0; rc >= 0 && (path = mprGetNextItem(fileData, &next)) != 0; ) {
            if (!mprPathExists(path, R_OK)) {
                httpError(stream, HTTP_CODE_NOT_FOUND, "Cannot open %s", path);
                return MPR_ERR_CANT_OPEN;
            }
            name = mprGetPathBase(path);
            rc += httpWrite(stream->writeq, "%s\r\nContent-Disposition: form-data; name=\"file%d\"; filename=\"%s\"\r\n",
                stream->boundary, next - 1, name);
            if ((type = mprLookupMime(MPR->mimeTypes, path)) != 0) {
                rc += httpWrite(stream->writeq, "Content-Type: %s\r\n", mprLookupMime(MPR->mimeTypes, path));
            }
            httpWrite(stream->writeq, "\r\n");
            if (blockingFileCopy(stream, path) < 0) {
                return MPR_ERR_CANT_WRITE;
            }
            rc += httpWrite(stream->writeq, "\r\n");
        }
    }
    rc += httpWrite(stream->writeq, "%s--\r\n--", stream->boundary);
    return rc;
}


/*
    Wait for the connection to reach a given state.
    Should only be used on the client side.
    @param state Desired state. Set to zero if you want to wait for one I/O event.
    @param timeout Timeout in msec. If timeout is zero, wait forever. If timeout is < 0, use default inactivity
        and duration timeouts.
 */
PUBLIC int httpWait(HttpStream *stream, int state, MprTicks timeout)
{
    HttpLimits  *limits;
    MprTicks    delay, start;
    int64       dispatcherMark;
    int         justOne;

    limits = stream->limits;
    if (httpServerStream(stream)) {
        return MPR_ERR_BAD_STATE;
    }
    if (stream->state <= HTTP_STATE_BEGIN) {
        return MPR_ERR_BAD_STATE;
    }
    if (state == 0) {
        /* Wait for just one I/O event */
        state = HTTP_STATE_FINALIZED;
        justOne = 1;
    } else {
        justOne = 0;
    }
    if (stream->error) {
        if (stream->state >= state) {
            return 0;
        }
        return MPR_ERR_BAD_STATE;
    }
    if (timeout < 0) {
        timeout = limits->requestTimeout;
    } else if (timeout == 0) {
        timeout = MPR_MAX_TIMEOUT;
    }
    if (state > HTTP_STATE_CONTENT) {
        httpFinalizeOutput(stream);
    }
    start = stream->http->now;
    dispatcherMark = mprGetEventMark(stream->dispatcher);

    while (stream->state < state && !stream->error && !mprIsSocketEof(stream->sock)) {
        if (httpRequestExpired(stream, -1)) {
            return MPR_ERR_TIMEOUT;
        }
        httpEnableNetEvents(stream->net);
        delay = min(limits->inactivityTimeout, mprGetRemainingTicks(start, timeout));
        delay = max(delay, 0);
        mprWaitForEvent(stream->dispatcher, delay, dispatcherMark);
        if (justOne || (mprGetRemainingTicks(start, timeout) <= 0)) {
            break;
        }
        dispatcherMark = mprGetEventMark(stream->dispatcher);
    }
    if (stream->error) {
        return MPR_ERR_NOT_READY;
    }
    if (stream->state < state) {
        if (mprGetRemainingTicks(start, timeout) <= 0) {
            return MPR_ERR_TIMEOUT;
        }
        if (!justOne) {
            return MPR_ERR_CANT_READ;
        }
    }
    stream->lastActivity = stream->http->now;
    return 0;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/config.c ************/

/*
    config.c -- Http JSON Configuration File Parsing

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/*********************************** Includes *********************************/



/************************************ Defines *********************************/

#define ITERATE_CONFIG(route, obj, child, index) \
    index = 0, child = obj ? obj->children: 0; obj && index < obj->length && !route->error; child = child->next, index++

/************************************ Forwards ********************************/

static void parseAuthRoles(HttpRoute *route, cchar *key, MprJson *prop);
static void parseAuthStore(HttpRoute *route, cchar *key, MprJson *prop);

/************************************** Code **********************************/
/*
    Define a configuration callbacks. The key is specified as it is used in json files.
 */

PUBLIC HttpParseCallback httpAddConfig(cchar *key, HttpParseCallback callback)
{
    HttpParseCallback   prior;

    prior = mprLookupKey(HTTP->parsers, key);
    mprAddKey(HTTP->parsers, key, callback);
    return prior;
}


PUBLIC void httpParseError(HttpRoute *route, cchar *fmt, ...)
{
    HttpRoute   *rp;
    va_list     args;
    char        *msg;

    va_start(args, fmt);
    msg = sfmtv(fmt, args);
    mprLog("error http config", 0, "%s", msg);
    va_end(args);
    route->error = 1;
    for (rp = route; rp; rp = rp->parent) {
        rp->error = 1;
    }
}


PUBLIC void httpParseWarn(HttpRoute *route, cchar *fmt, ...)
{
    va_list     args;
    char        *msg;

    va_start(args, fmt);
    msg = sfmtv(fmt, args);
    mprLog("warn http config", 1, "%s", msg);
    va_end(args);
}


/*
    Convert a JSON string to a space-separated C string
 */
static cchar *getList(MprJson *prop)
{
    char    *cp, *p;

    if (prop == 0) {
        return 0;
    }
    if ((cp = mprJsonToString(prop, 0)) == 0) {
        return 0;
    }
    if (*cp == '[') {
        cp = strim(cp, "[]", 0);
    }
    for (p = cp; *p; p++) {
        if (*p == '"') {
            if (p[1] == '"') {
                p++;
            } else {
                *p = ' ';
            }
        } else if (*p == '\'') {
            if (p[1] == '\'') {
                p++;
            } else {
                *p = ' ';
            }
        } else if (*p == ',') {
            *p = ' ';
        }
    }
    if (*cp == ' ') {
        cp = strim(cp, " \t", 0);
    }
    return cp;
}


PUBLIC int parseInclude(HttpRoute *route, MprJson *config, MprJson *inc)
{
    MprJson     *child, *obj;
    MprList     *files;
    cchar       *data, *errorMsg, *path;
    int         ji, next;

    for (ITERATE_CONFIG(route, inc, child, ji)) {
        files = mprGlobPathFiles(".", child->value, MPR_PATH_NO_DIRS | MPR_PATH_RELATIVE);
        for (ITERATE_ITEMS(files, path, next)) {
            if ((data = mprReadPathContents(path, NULL)) == 0) {
                httpParseError(route, "Cannot read configuration from \"%s\"", path);
                return MPR_ERR_CANT_READ;
            }
            if ((obj = mprParseJsonEx(data, 0, 0, 0, &errorMsg)) == 0) {
                httpParseError(route, "Cannot parse %s: error %s", path, errorMsg);
                return MPR_ERR_CANT_READ;
            }
            mprBlendJson(config, obj, MPR_JSON_COMBINE);
        }
    }
    return 0;
}


PUBLIC void httpInitConfig(HttpRoute *route)
{
    route->error = 0;
    route->config = 0;
    route->clientConfig = 0;
}


PUBLIC int httpLoadConfig(HttpRoute *route, cchar *path)
{
    MprJson     *config, *obj, *profiles;
    cchar       *data, *errorMsg, *profile;

    if (!path) {
        return 0;
    }
    /*
        Order of processing matters. First load the file and then blend included files into the same json obj.
        Then blend the mode directives and then assign/blend into the route config.
        Lastly, parse the json config object.
     */
    if ((data = mprReadPathContents(path, NULL)) == 0) {
        mprLog("error http config", 0, "Cannot read configuration from \"%s\"", path);
        return MPR_ERR_CANT_READ;
    }
    if ((config = mprParseJsonEx(data, 0, 0, 0, &errorMsg)) == 0) {
        mprLog("error http config", 0, "Cannot parse %s: error %s", path, errorMsg);
        return MPR_ERR_CANT_READ;
    }
    if ((obj = mprGetJsonObj(config, "include")) != 0) {
        parseInclude(route, config, obj);
    }
    if (!route->mode) {
        if ((profile = mprGetJson(route->config, "profile")) == 0) {
            if ((profile = mprGetJson(route->config, "pak.mode")) == 0) {
                if ((profile = mprGetJson(config, "profile")) == 0) {
                    profile = mprGetJson(config, "pak.mode");
                }
            }
        }
        route->mode = profile;
        route->debug = smatch(route->mode, "debug") || smatch(route->mode, "dev");
    }
    if (route->config) {
        mprBlendJson(route->config, config, MPR_JSON_COMBINE);
    } else {
        route->config = config;
    }
    route->error = 0;

    if (route->mode) {
        /*
            Http uses top level modes, Pak uses top level pak.modes.
         */
        if ((profiles = mprGetJsonObj(config, sfmt("profiles.%s", route->mode))) == 0) {
            if ((profiles = mprGetJsonObj(config, sfmt("modes.%s", route->mode))) == 0) {
                profiles = mprGetJsonObj(config, sfmt("pak.modes.%s", route->mode));
            }
        }
        if (profiles) {
            mprBlendJson(route->config, profiles, MPR_JSON_OVERWRITE);
            httpParseAll(route, 0, profiles);
        }
    }
    httpParseAll(route, 0, config);
    if (route->error) {
        route->config = 0;
        return MPR_ERR_BAD_STATE;
    }
    return 0;
}


/**************************************** Parser Callbacks ****************************************/

static void parseKey(HttpRoute *route, cchar *key, MprJson *prop)
{
    HttpParseCallback   parser;

    key = key ? sjoin(key, ".", prop->name, NULL) : prop->name;
    if ((parser = mprLookupKey(HTTP->parsers, key)) != 0) {
        parser(route, key, prop);
    }
}


PUBLIC void httpParseAll(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        parseKey(route, key, child);
    }
}


static void parseDirectories(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        if (smatch(child->name, "documents")) {
            httpSetRouteDocuments(route, child->value);
        } else if (smatch(child->name, "home")) {
            httpSetRouteHome(route, child->value);
        }
        httpSetDir(route, child->name, child->value);
    }
}


static void parseAliases(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    HttpRoute   *alias;
    MprPath     info;
    cchar       *path, *prefix;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        prefix = child->name;
        path = child->value;
        if (!path || !prefix) {
            httpParseError(route, "Alias is missing path or prefix properties");
            break;
        }
        mprGetPathInfo(path, &info);
        if (info.isDir) {
            alias = httpCreateAliasRoute(route, prefix, path, 0);
            if (sends(prefix, "/")) {
                httpSetRoutePattern(alias, sfmt("^%s(.*)$", prefix), 0);
            } else {
                /* Add a non-capturing optional trailing "/" */
                httpSetRoutePattern(alias, sfmt("^%s(?:/)*(.*)$", prefix), 0);
            }
            httpSetRouteTarget(alias, "run", "$1");
        } else {
            alias = httpCreateAliasRoute(route, sjoin("^", prefix, NULL), 0, 0);
            httpSetRouteTarget(alias, "run", path);
        }
        httpFinalizeRoute(alias);
    }
}


/*
    Attach this host to an endpoint

    attach: 'ip:port'
 */
static void parseAttach(HttpRoute *route, cchar *key, MprJson *prop)
{
    HttpEndpoint    *endpoint;
    MprJson         *child;
    cchar           *ip;
    int             ji, port;

    if (prop->type & MPR_JSON_VALUE) {
        if (mprParseSocketAddress(prop->value, &ip, &port, NULL, -1) < 0) {
            httpParseError(route, "Bad attach address: %s", prop->value);
            return;
        }
        if ((endpoint = httpLookupEndpoint(ip, port)) == 0) {
            httpParseError(route, "Cannot find endpoint %s to attach for host %s", prop->value, route->host->name);
            return;
        }
        httpAddHostToEndpoint(endpoint, route->host);

    } else if (prop->type == MPR_JSON_ARRAY) {
        for (ITERATE_CONFIG(route, prop, child, ji)) {
            if (mprParseSocketAddress(child->value, &ip, &port, NULL, -1) < 0) {
                httpParseError(route, "Bad attach address: %s", child->value);
                return;
            }
            if ((endpoint = httpLookupEndpoint(ip, port)) == 0) {
                httpParseError(route, "Cannot find endpoint %s to attach for host %s", child->value, route->host->name);
                return;
            }
            httpAddHostToEndpoint(endpoint, route->host);
        }
    }
}


static void parseAuth(HttpRoute *route, cchar *key, MprJson *prop)
{
    if (prop->type & MPR_JSON_STRING) {
        /* Permits auth: "app" to set the store */
        if (smatch(prop->value, "none")) {
            httpSetAuthStore(route->auth, NULL);
            httpSetAuthType(route->auth, NULL, 0);
        }
    } else if (prop->type == MPR_JSON_OBJ) {
        httpParseAll(route, key, prop);
    }
}


static void parseAuthAutoName(HttpRoute *route, cchar *key, MprJson *prop)
{
    /* Automatic login as this user. Password not required */
    httpSetAuthUsername(route->auth, prop->value);
}


/*
    Parse roles and compute abilities
 */
static void parseAuthAutoRoles(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprHash     *abilities;
    MprKey      *kp;
    MprJson     *child, *job;
    int         ji;

    if ((job = mprGetJsonObj(route->config, "http.auth.roles")) != 0) {
        parseAuthRoles(route, "http.auth.roles", job);
    }
    abilities = mprCreateHash(0, 0);
    for (ITERATE_CONFIG(route, prop, child, ji)) {
        httpComputeRoleAbilities(route->auth, abilities, child->value);
    }
    if (mprGetHashLength(abilities) > 0) {
        job = mprCreateJson(MPR_JSON_ARRAY);
        for (ITERATE_KEYS(abilities, kp)) {
            mprSetJson(job, "$", kp->key, 0);
        }
        mprSetJsonObj(route->config, "http.auth.auto.abilities", job);
    }
}


static void parseAuthLogin(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetAuthLogin(route->auth, prop->value);
}


static void parseAuthRealm(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetAuthRealm(route->auth, prop->value);
}


static void parseAuthRequireRoles(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        httpSetAuthRequiredAbilities(route->auth, child->value);
    }
}


static void parseAuthRequireUsers(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    if (prop->type & MPR_JSON_STRING) {
        if (smatch(prop->value, "*")) {
            httpSetAuthAnyValidUser(route->auth);
        } else {
            httpSetAuthPermittedUsers(route->auth, prop->value);
        }
    } else if (prop->type & MPR_JSON_OBJ) {
        for (ITERATE_CONFIG(route, prop, child, ji)) {
            if (smatch(prop->value, "*")) {
                httpSetAuthAnyValidUser(route->auth);
                break;
            } else {
                httpSetAuthPermittedUsers(route->auth, getList(child));
            }
        }
    }
}


static void parseAuthRoles(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        if (httpAddRole(route->auth, child->name, getList(child)) == 0) {
            httpParseError(route, "Cannot add role %s", child->name);
            break;
        }
    }
}


static void parseAuthSessionCookie(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteCookie(route, prop->value);
}


static void parseAuthSessionCookiePersist(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteCookiePersist(route, smatch(prop->value, "true"));
}


static void parseAuthSessionCookieSame(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteCookieSame(route, prop->value);
}


static void parseAuthSessionEnable(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetAuthSession(route->auth, 0);
}


static void parseAuthSessionVisible(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteSessionVisibility(route, scaselessmatch(prop->value, "true"));
}


static void parseAuthStore(HttpRoute *route, cchar *key, MprJson *prop)
{
    if (httpSetAuthStore(route->auth, prop->value) < 0) {
        httpParseError(route, "The %s AuthStore is not available on this platform", prop->value);
    }
}


static void parseAuthType(HttpRoute *route, cchar *key, MprJson *prop)
{
    HttpAuth    *auth;
    cchar       *type;

    auth = route->auth;
    type = prop->value;

    if (httpSetAuthType(auth, type, 0) < 0) {
        httpParseError(route, "The %s AuthType is not available on this platform", type);
    }
    if (type && !smatch(type, "none") && !smatch(type, "app")) {
        httpAddRouteCondition(route, "auth", 0, 0);
    }
    if (smatch(type, "basic") || smatch(type, "digest")) {
        /*
            Must not use cookies by default, otherwise, the client cannot logoff.
         */
        httpSetAuthSession(auth, 0);
    }
}


static void parseAuthUsers(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    cchar       *roles, *password;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        password = mprReadJson(child, "password");
        roles = getList(mprReadJsonObj(child, "roles"));
        if (httpAddUser(route->auth, child->name, password, roles) < 0) {
            httpParseError(route, "Cannot add user %s", child->name);
            break;
        }
        if (!route->auth->store) {
            httpSetAuthStore(route->auth, "config");
        }
    }
}


static void parseAutoFinalize(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteAutoFinalize(route, (smatch(prop->value, "true")));
}


static void parseCache(HttpRoute *route, cchar *key, MprJson *prop)
{
#if ME_HTTP_CACHE
    MprJson     *child;
    MprTicks    clientLifespan, serverLifespan;
    cchar       *methods, *extensions, *urls, *mimeTypes, *client, *server;
    int         flags, ji;

    clientLifespan = serverLifespan = 0;
    if (prop->type & MPR_JSON_TRUE || (prop->type == MPR_JSON_STRING && smatch(prop->value, "true"))) {
        httpAddCache(route, 0, 0, 0, 0, 3600 * 1000, 0, HTTP_CACHE_CLIENT | HTTP_CACHE_STATIC);
    } else {
        for (ITERATE_CONFIG(route, prop, child, ji)) {
            flags = 0;
            if ((client = mprReadJson(child, "client")) != 0) {
                flags |= HTTP_CACHE_CLIENT;
                clientLifespan = httpGetTicks(client);
            }
            if ((server = mprReadJson(child, "server")) != 0) {
                flags |= HTTP_CACHE_SERVER;
                serverLifespan = httpGetTicks(server);
            }
            methods = getList(mprReadJsonObj(child, "methods"));
            urls = getList(mprReadJsonObj(child, "urls"));
            mimeTypes = getList(mprReadJsonObj(child, "mime"));
            extensions = getList(mprReadJsonObj(child, "extensions"));
            if (smatch(mprReadJson(child, "unique"), "true")) {
                /* Uniquely cache requests with different params */
                flags |= HTTP_CACHE_UNIQUE;
            }
            if (smatch(mprReadJson(child, "manual"), "true")) {
                /* User must manually call httpWriteCache */
                flags |= HTTP_CACHE_MANUAL;
            }
            httpAddCache(route, methods, urls, extensions, mimeTypes, clientLifespan, serverLifespan, flags);
        }
    }
#else
    mprLog("error http config", 0, "Cache directive not built in binary");
#endif
}


static void parseCanonicalName(HttpRoute *route, cchar *key, MprJson *prop)
{
    if (httpSetRouteCanonicalName(route, prop->value) < 0) {
        httpParseError(route, "Bad canonical name: %s", prop->value);
    }
}


/*
    condition: '[!] auth'
    condition: '[!] condition'
    condition: '[!] exists string'
    condition: '[!] directory string'
    condition: '[!] match string valuePattern'
    condition: '[!] secure'
    condition: '[!] unauthorized'
 */
static void parseConditions(HttpRoute *route, cchar *key, MprJson *prop)
{
    char    *name, *details;
    int     not;

    if (!httpTokenize(route, prop->value, "%! ?S ?*", &not, &name, &details)) {
        httpParseError(route, "Bad condition: %s", prop->value);
        return;
    }
    if (httpAddRouteCondition(route, name, details, not ? HTTP_ROUTE_NOT : 0) < 0) {
        httpParseError(route, "Bad condition: %s", prop->value);
        return;
    }
}

static void parseCgiEscape(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteEnvEscape(route, (prop->type & MPR_JSON_TRUE) ? 1 : 0);
}


static void parseCgiPrefix(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteEnvPrefix(route, prop->value);
}


static void parseCharset(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteCharSet(route, prop->value);
}


static void parseCompress(HttpRoute *route, cchar *key, MprJson *prop)
{
    if (smatch(prop->value, "true")) {
        httpAddRouteMapping(route, "", "${1}.gz, min.${1}.gz, min.${1}");
    } else if (prop->type & MPR_JSON_ARRAY) {
        httpAddRouteMapping(route, mprJsonToString(prop, 0), "${1}.gz, min.${1}.gz, min.${1}");
    }
}


static void parseDatabase(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->database = prop->value;
}


static void parseDeleteUploads(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteAutoDelete(route, (prop->type & MPR_JSON_TRUE) ? 1 : 0);
}


static void parseDocuments(HttpRoute *route, cchar *key, MprJson *prop)
{
    cchar   *path;

    path = httpExpandRouteVars(route, prop->value);
    if (!mprPathExists(path, X_OK)) {
        httpParseError(route, "Cannot locate documents directory %s", path);
    } else {
        httpSetRouteDocuments(route, path);
    }
}


static void parseErrors(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        httpAddRouteErrorDocument(route, (int) stoi(child->name), child->value);
    }
}



static void parseFormatsResponse(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->responseFormat = prop->value;
    if (smatch(route->responseFormat, "json")) {
        route->json = 1;
    }
}


/*
    Alias for pipeline: { handler ... }
 */
static void parseHandler(HttpRoute *route, cchar *key, MprJson *prop)
{
    if (httpSetRouteHandler(route, prop->value) < 0) {
        httpParseError(route, "Cannot set handler %s", prop->value);
    }
}


static void parseHeadersAdd(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        httpAddRouteResponseHeader(route, HTTP_ROUTE_ADD_HEADER, child->name, child->value);
    }
}


static void parseHeadersRemove(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        if (prop->type & MPR_JSON_ARRAY) {
            httpAddRouteResponseHeader(route, HTTP_ROUTE_REMOVE_HEADER, child->value, 0);
        } else {
            httpAddRouteResponseHeader(route, HTTP_ROUTE_REMOVE_HEADER, child->name, 0);
        }
    }
}


static void parseHeadersSet(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        httpAddRouteResponseHeader(route, HTTP_ROUTE_SET_HEADER, child->name, child->value);
    }
}


static void parseHome(HttpRoute *route, cchar *key, MprJson *prop)
{
    cchar   *path;

    path = httpExpandRouteVars(route, prop->value);
    if (!mprPathExists(path, X_OK)) {
        httpParseError(route, "Cannot locate home directory %s", path);
    } else {
        httpSetRouteHome(route, path);
    }
}


static void parseHost(HttpRoute *route, cchar *key, MprJson *prop)
{
    HttpHost    *host;
    HttpRoute   *newRoute;

    host = httpCloneHost(route->host);
    newRoute = httpCreateInheritedRoute(route);
    httpSetRouteHost(newRoute, host);
    httpSetHostDefaultRoute(host, newRoute);
    httpParseAll(newRoute, key, prop);
    httpFinalizeRoute(newRoute);
}


static void parseHosts(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    if (prop->type & MPR_JSON_OBJ) {
        parseHost(route, sreplace(key, ".hosts", ""), prop);

    } else if (prop->type & MPR_JSON_ARRAY) {
        key = sreplace(key, ".hosts", "");
        for (ITERATE_CONFIG(route, prop, child, ji)) {
            parseHost(route, key, child);
        }
    }
}


static void parseHttp2(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpEnableHttp2(smatch(prop->value, "true"));
}


static void parseIndexes(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    httpResetRouteIndexes(route);
    for (ITERATE_CONFIG(route, prop, child, ji)) {
        httpAddRouteIndex(route, child->value);
    }
}


static void parseLanguages(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    cchar       *path, *prefix, *suffix;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        if ((prefix = mprReadJson(child, "prefix")) != 0) {
            httpAddRouteLanguageSuffix(route, child->name, child->value, HTTP_LANG_BEFORE);
        }
        if ((suffix = mprReadJson(child, "suffix")) != 0) {
            httpAddRouteLanguageSuffix(route, child->name, child->value, HTTP_LANG_AFTER);
        }
        if ((path = mprReadJson(child, "path")) != 0) {
            httpAddRouteLanguageDir(route, child->name, mprGetAbsPath(path));
        }
        if (smatch(mprReadJson(child, "default"), "default")) {
            httpSetRouteDefaultLanguage(route, child->name);
        }
    }
}


static void parseLimits(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpGraduateLimits(route, 0);
    httpParseAll(route, key, prop);
}


static void parseLimitsCache(HttpRoute *route, cchar *key, MprJson *prop)
{
    mprSetCacheLimits(route->host->responseCache, 0, 0, httpGetNumber(prop->value), 0);
}


static void parseLimitsCacheItem(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->cacheItemSize = httpGetInt(prop->value);
}


static void parseLimitsChunk(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->chunkSize = httpGetInt(prop->value);
}


static void parseLimitsClients(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->clientMax = httpGetInt(prop->value);
}


static void parseLimitsConnections(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->connectionsMax = httpGetInt(prop->value);
}


static void parseLimitsConnectionsPerClient(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->connectionsPerClientMax = httpGetInt(prop->value);
}

static void parseLimitsDepletion(HttpRoute *route, cchar *key, MprJson *prop)
{
    cchar   *policy;
    int     flags;

    flags = MPR_ALLOC_POLICY_EXIT;
    policy = prop->value;

    if (scmp(policy, "restart") == 0) {
#if VXWORKS
        flags = MPR_ALLOC_POLICY_RESTART;
#else
        /* Appman will restart */
        flags = MPR_ALLOC_POLICY_EXIT;
#endif
    } else if (scmp(policy, "continue") == 0) {
        flags = MPR_ALLOC_POLICY_PRUNE;
    } else {
        httpParseError(route, "Unknown limit depletion policy '%s'", policy);
        return;
    }
    mprSetMemPolicy(flags);
}


static void parseLimitsFiles(HttpRoute *route, cchar *key, MprJson *prop)
{
    mprSetFilesLimit(httpGetInt(prop->value));
}


static void parseLimitsKeepAlive(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->keepAliveMax = httpGetInt(prop->value);
}


static void parseLimitsMemory(HttpRoute *route, cchar *key, MprJson *prop)
{
    ssize   maxMem;

    maxMem = (ssize) httpGetNumber(prop->value);
    mprSetMemLimits(maxMem / 100 * 85, maxMem, -1);
}


static void parseLimitsPacket(HttpRoute *route, cchar *key, MprJson *prop)
{
    int     size;

    size = httpGetInt(prop->value);
    if (size > ME_SANITY_PACKET) {
        size = ME_SANITY_PACKET;
#if ME_HTTP_HTTP2
    } else if (size < HTTP2_MIN_FRAME_SIZE) {
        size = HTTP2_MIN_FRAME_SIZE;
#endif
    }
    route->limits->packetSize = size;
}


static void parseLimitsProcesses(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->processMax = httpGetInt(prop->value);
}


static void parseLimitsRequestsPerClient(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->requestsPerClientMax = httpGetInt(prop->value);
}


static void parseLimitsRxBody(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->rxBodySize = httpGetNumber(prop->value);
}


static void parseLimitsRxForm(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->rxFormSize = httpGetNumber(prop->value);
}


static void parseLimitsRxHeader(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->headerSize = httpGetInt(prop->value);
}


#if ME_HTTP_HTTP2
/*
    Set the total maximum number of streams per network connection
 */
static void parseLimitsStreams(HttpRoute *route, cchar *key, MprJson *prop)
{
    int     size;

    size = httpGetInt(prop->value);
    if (size < 1) {
        size = 1;
    }
    route->limits->streamsMax = size;
}
#endif


static void parseLimitsTxBody(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->txBodySize = httpGetNumber(prop->value);
}


static void parseLimitsSessions(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->sessionMax = httpGetInt(prop->value);
}


static void parseLimitsUri(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->uriSize = httpGetInt(prop->value);
}


static void parseLimitsUpload(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->uploadSize = httpGetNumber(prop->value);
}


#if ME_HTTP_WEB_SOCKETS
static void parseLimitsWebSockets(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->webSocketsMax = httpGetInt(prop->value);
}


static void parseLimitsWebSocketsMessage(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->webSocketsMessageSize = httpGetInt(prop->value);
}


static void parseLimitsWebSocketsFrame(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->webSocketsFrameSize = httpGetInt(prop->value);
}


static void parseLimitsWebSocketsPacket(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->webSocketsPacketSize = httpGetInt(prop->value);
}
#endif


static void parseLimitsWorkers(HttpRoute *route, cchar *key, MprJson *prop)
{
    int     count;

    count = atoi(prop->value);
    if (count <= 0) {
        count = MAXINT;
    }
    mprSetMaxWorkers(count);
}


#if ME_HTTP_HTTP2
static void parseLimitsWindow(HttpRoute *route, cchar *key, MprJson *prop)
{
    int     size;

    size = httpGetInt(prop->value);
    if (size < HTTP2_MIN_WINDOW) {
        size = HTTP2_MIN_WINDOW;
    }
    route->limits->window = size;
}
#endif


static void parseMethods(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteMethods(route, supper(getList(prop)));
}


/*
    Note: this typically comes from pak.json
 */
static void parseProfile(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->mode = prop->value;
}


static void parseName(HttpRoute *route, cchar *key, MprJson *prop)
{
    if (httpSetHostName(route->host, prop->value) < 0) {
        httpParseError(route, "Bad host name: %s", prop->value);
    }
}


/*
    Match route only if param matches
 */
static void parseParams(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    cchar       *name, *value;
    int         not, ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        name = mprReadJson(child, "name");
        value = mprReadJson(child, "value");
        not = smatch(mprReadJson(child, "equals"), "true") ? 0 : HTTP_ROUTE_NOT;
        httpAddRouteParam(route, name, value, not);
    }
}


static void parsePattern(HttpRoute *route, cchar *key, MprJson *prop)
{
    cchar   *pattern;

    pattern = prop->value;
    if (pattern && *pattern != '^') {
        pattern = sfmt("^%s%s", route->parent->prefix, pattern);
    }
    httpSetRoutePattern(route, pattern, 0);
}


static void parsePipelineFilters(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    cchar       *name, *extensions;
    int         flags, ji;

    flags = HTTP_STAGE_RX | HTTP_STAGE_TX;

    if (prop->type & MPR_JSON_STRING) {
        name = prop->value;
        if (httpAddRouteFilter(route, prop->value, NULL, flags) < 0) {
            httpParseWarn(route, "Cannot add filter %s", name);
        }
    } else if (prop->type & MPR_JSON_OBJ) {
        name = mprReadJson(prop, "name");
        extensions = getList(mprReadJsonObj(prop, "extensions"));
        if (httpAddRouteFilter(route, name, extensions, flags) < 0) {
            httpParseWarn(route, "Cannot add filter %s", name);
        }
    } else if (prop->type & MPR_JSON_ARRAY) {
        for (ITERATE_CONFIG(route, prop, child, ji)) {
            parsePipelineFilters(route, key, child);
        }
    }
}


/*
    pipeline: {
        handler: 'espHandler',                     //  For all extensions
    },
 */
static void parsePipelineHandler(HttpRoute *route, cchar *key, MprJson *prop)
{
    if (httpSetRouteHandler(route, prop->value) < 0) {
        httpParseError(route, "Cannot add handler \"%s\"", prop->value);
    }
}


/*
    pipeline: {
        handlers: {
            espHandler: [ '*.esp, '*.xesp' ],
        },
    },
 */
static void parsePipelineHandlers(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    if (prop->type & MPR_JSON_STRING) {
        if (httpAddRouteHandler(route, prop->value, "") < 0) {
            httpParseWarn(route, "Handler \"%s\" is not available", prop->name);
        }

    } else {
        for (ITERATE_CONFIG(route, prop, child, ji)) {
            if (httpAddRouteHandler(route, child->name, getList(child)) < 0) {
                httpParseWarn(route, "Handler \"%s\" is not available", child->name);
            }
        }
    }
}


static void parsePrefix(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRoutePrefix(route, prop->value);
}


static void createRedirectAlias(HttpRoute *route, int status, cchar *from, cchar *to)
{
    HttpRoute   *alias;
    cchar       *pattern;

    if (from == 0 || *from == '\0') {
        from = "/";
    }
    if (sends(from, "/")) {
        pattern = sfmt("^%s%s(.*)$", route->prefix, from);
    } else {
        /* Add a non-capturing optional trailing "/" */
        pattern = sfmt("^%s%s(?:/)*(.*)$", route->prefix, from);
    }
    alias = httpCreateAliasRoute(route, pattern, 0, 0);
    httpSetRouteMethods(alias, "*");
    httpSetRouteTarget(alias, "redirect", sfmt("%d %s/$1", status, to));
    if (sstarts(to, "https")) {
        httpAddRouteCondition(alias, "secure", to, HTTP_ROUTE_REDIRECT);
    }
    httpFinalizeRoute(alias);
}


/*
    redirect: 'secure'
    redirect: [
        '/to/url',
        {
            from: '/somewhere.html',
            to:   '/elsewhere.html',
            status: 302,
        }
    }
 */
static void parseRedirect(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    cchar       *from, *status, *to;
    int         ji;

    if (prop->type & MPR_JSON_FALSE) {
        /* skip */
    } else if (prop->type & MPR_JSON_STRING) {
        if (smatch(prop->value, "secure") ||smatch(prop->value, "https://")) {
            httpAddRouteCondition(route, "secure", "https://", HTTP_ROUTE_REDIRECT);
        } else {
            createRedirectAlias(route, 0, "/", prop->value);
        }

    } else {
        for (ITERATE_CONFIG(route, prop, child, ji)) {
            if (child->type & MPR_JSON_STRING) {
                from = "/";
                to = child->value;
                status = "302";
            } else {
                from = mprReadJson(child, "from");
                to = mprReadJson(child, "to");
                status = mprReadJson(child, "status");
                if (smatch(status, "permanent")) {
                    status = "301";
                } else if (smatch(status, "temporary")) {
                    status = "302";
                }
            }
            if (smatch(child->value, "secure")) {
                httpAddRouteCondition(route, "secure", "https://", HTTP_ROUTE_REDIRECT);
            } else {
                createRedirectAlias(route, (int) stoi(status), from, to);
            }
        }
    }
}


static void parseRenameUploads(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteRenameUploads(route, (prop->type & MPR_JSON_TRUE) ? 1 : 0);
}


/*
    Create RESTful routes
 */
static void parseResources(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child, *groups, *singletons, *sets;
    int         ji;

    if ((sets = mprReadJsonObj(prop, "sets")) != 0) {
        for (ITERATE_CONFIG(route, sets, child, ji)) {
            httpAddRouteSet(route, child->value);
        }
    }
    if ((groups = mprReadJsonObj(prop, "groups")) != 0) {
        for (ITERATE_CONFIG(route, groups, child, ji)) {
            httpAddResourceGroup(route, child->value);
        }
    }
    if ((singletons = mprReadJsonObj(prop, "singletons")) != 0) {
        for (ITERATE_CONFIG(route, singletons, child, ji)) {
            httpAddResource(route, child->value);
        }
    }
}


PUBLIC HttpRouteSetProc httpDefineRouteSet(cchar *name, HttpRouteSetProc fn)
{
    HttpRouteSetProc    prior;

    prior = mprLookupKey(HTTP->routeSets, name);
    mprAddKey(HTTP->routeSets, name, fn);
    return prior;
}


PUBLIC void httpAddRouteSet(HttpRoute *route, cchar *set)
{
    HttpRouteSetProc    proc;

    if (set == 0 || *set == 0) {
        return;
    }
    if ((proc = mprLookupKey(route->http->routeSets, set)) != 0) {
        (proc)(route, set);
    } else {
        mprLog("error http config", 0, "Cannot find route set \"%s\"", set);
    }
}


static void parseHttp(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpParseAll(route, key, prop);
}


static void parseRoute(HttpRoute *route, cchar *key, MprJson *prop)
{
    HttpRoute   *newRoute;
    cchar       *pattern;

    if (prop->type & MPR_JSON_STRING) {
        if (smatch(prop->value, "reset")) {
            httpResetRoutes(route->host);
        } else if (smatch(prop->value, "print")) {
            httpLogRoutes(route->host, 0);
        } else {
            httpAddRouteSet(route, prop->value);
        }

    } else if (prop->type & MPR_JSON_OBJ) {
        newRoute = 0;
        pattern = mprReadJson(prop, "pattern");
        if (pattern) {
            newRoute = httpLookupRoute(route->host, pattern);
            if (!newRoute) {
                newRoute = httpCreateInheritedRoute(route);
                httpSetRouteHost(newRoute, route->host);
            }
        } else {
            newRoute = route;
        }
        httpParseAll(newRoute, key, prop);
        if (pattern) {
            httpFinalizeRoute(newRoute);
        }
    }
}


static void parseRoutes(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    if (prop->type & MPR_JSON_STRING) {
        parseRoute(route, key, prop);

    } else if (prop->type & MPR_JSON_OBJ) {
        key = sreplace(key, ".routes", "");
        parseRoute(route, key, prop);

    } else if (prop->type & MPR_JSON_ARRAY) {
        key = sreplace(key, ".routes", "");
        for (ITERATE_CONFIG(route, prop, child, ji)) {
            parseRoute(route, key, child);
        }
    }
}


static void parseScheme(HttpRoute *route, cchar *key, MprJson *prop)
{
    if (sstarts(prop->value, "https")) {
        httpAddRouteCondition(route, "secure", 0, 0);
    }
}


static void parseServerAccount(HttpRoute *route, cchar *key, MprJson *prop)
{
    cchar       *value;

    if (route->flags & HTTP_ROUTE_HOSTED) {
        return;
    }
    if ((value = mprReadJson(prop, "user")) != 0) {
        if (!smatch(value, "_unchanged_") && !mprGetDebugMode()) {
            httpSetGroupAccount(value);
        }
    }
    if ((value = mprReadJson(prop, "user")) != 0) {
        if (!smatch(value, "_unchanged_") && !mprGetDebugMode()) {
            httpSetUserAccount(value);
        }
    }
}


#if ME_HTTP_DEFENSE
static void parseServerDefenses(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        httpAddDefenseFromJson(child->name, 0, child);
    }
}
#endif


static void parseServerListen(HttpRoute *route, cchar *key, MprJson *prop)
{
    HttpEndpoint    *endpoint, *dual;
    HttpHost        *host;
    MprJson         *child;
    cchar           *ip;
    int             ji, port, secure;

    if (route->flags & (HTTP_ROUTE_HOSTED | HTTP_ROUTE_OWN_LISTEN)) {
        return;
    }
    host = route->host;
    for (ITERATE_CONFIG(route, prop, child, ji)) {
        if (mprParseSocketAddress(child->value, &ip, &port, &secure, 80) < 0) {
            httpParseError(route, "Bad listen address: %s", child->value);
            return;
        }
        if (port == 0) {
            httpParseError(route, "Bad or missing port %d in Listen directive", port);
            return;
        }
        endpoint = httpCreateEndpoint(ip, port, NULL);
        httpAddHostToEndpoint(endpoint, host);

        if (!host->defaultEndpoint) {
            httpSetHostDefaultEndpoint(host, endpoint);
        }
        if (secure) {
            if (route->ssl == 0) {
                if (route->parent && route->parent->ssl) {
                    route->ssl = mprCloneSsl(route->parent->ssl);
                } else {
                    route->ssl = mprCreateSsl(1);
                }
            }
            httpSecureEndpoint(endpoint, route->ssl);
            if (!host->secureEndpoint) {
                httpSetHostSecureEndpoint(host, endpoint);
            }
        }
        /*
            Single stack networks cannot support IPv4 and IPv6 with one socket. So create a specific IPv6 endpoint.
            This is currently used by VxWorks and Windows versions prior to Vista (i.e. XP)
         */
        if (!schr(prop->value, ':') && mprHasIPv6() && !mprHasDualNetworkStack()) {
            dual = httpCreateEndpoint("::", port, NULL);
            httpAddHostToEndpoint(dual, host);
            httpSecureEndpoint(dual, route->ssl);
        }
    }
}


/*
    log: {
        location: 'stdout',
        level: '2',
        backup: 5,
        anew: true,
        size: '10MB',
        timestamp: '1hr',
    }
 */
static void parseLog(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprTicks    timestamp;
    cchar       *location;
    ssize       size;
    int         level, anew, backup;

    if (route->flags & HTTP_ROUTE_HOSTED) {
        return;
    }
    if (mprGetCmdlineLogging()) {
        mprLog("warn http config", 4, "Already logging. Ignoring log configuration");
        return;
    }
    location = mprReadJson(prop, "location");
    level = (int) stoi(mprReadJson(prop, "level"));
    backup = (int) stoi(mprReadJson(prop, "backup"));
    anew = smatch(mprReadJson(prop, "anew"), "true");
    size = (ssize) httpGetNumber(mprReadJson(prop, "size"));
    timestamp = httpGetNumber(mprReadJson(prop, "timestamp"));

    if (size < HTTP_TRACE_MIN_LOG_SIZE) {
        size = HTTP_TRACE_MIN_LOG_SIZE;
    }
    if (location == 0) {
        httpParseError(route, "Missing location");
        return;
    }
    if (!smatch(location, "stdout") && !smatch(location, "stderr")) {
        location = httpMakePath(route, 0, location);
    }
    mprSetLogBackup(size, backup, anew ? MPR_LOG_ANEW : 0);

    if (mprStartLogging(location, 0) < 0) {
        httpParseError(route, "Cannot write to error log: %s", location);
        return;
    }
    mprSetLogLevel(level);
    mprLogConfig();
    if (timestamp) {
        httpSetTimestamp(timestamp);
    }
}


/*
    modules: [
        {
            name: 'espHandler',
            path: '/path/to/module'
        }
    ]
 */
static void parseServerModules(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprModule   *module;
    MprJson     *child;
    cchar       *entry, *name, *path;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        name = mprGetJson(child, "name");
        path = mprGetJson(child, "path");
        if (!name) {
            name = path;
        }
        if (!path) {
            path = sjoin("libmod_", name, ME_SHOBJ, NULL);
        }
        if ((module = mprLookupModule(name)) != 0) {
#if ME_STATIC
            mprLog("info http config", 2, "Activating module (Builtin) %s", name);
#endif
            continue;
        }
        entry = sfmt("http%sInit", stitle(name));
        module = mprCreateModule(name, path, entry, HTTP);

        if (mprLoadModule(module) < 0) {
            break;
        }
    }
}

static void parseServerMonitors(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    MprTicks    period;
    cchar       *counter, *expression, *limit, *relation, *defenses;
    int         ji, enable;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        defenses = mprReadJson(child, "defenses");
        expression = mprReadJson(child, "expression");
        period = httpGetTicks(mprReadJson(child, "period"));
        enable = smatch(mprReadJson(child, "enable"), "true");
        if (!enable) {
            continue;
        }
        if (!httpTokenize(route, expression, "%S %S %S", &counter, &relation, &limit)) {
            httpParseError(route, "Cannot add monitor: %s", prop->name);
            break;
        }
        if (httpAddMonitor(counter, relation, httpGetInt(limit), period, defenses) < 0) {
            httpParseError(route, "Cannot add monitor: %s", prop->name);
            break;
        }
    }
}


static void parseShowErrors(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteShowErrors(route, (prop->type & MPR_JSON_TRUE) ? 1 : 0);
}


static void parseSource(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteSource(route, prop->value);
}


static void parseSsl(HttpRoute *route, cchar *key, MprJson *prop)
{
    HttpRoute   *parent;

    if (route->flags & HTTP_ROUTE_HOSTED) {
        return;
    }
    parent = route->parent;
    if (route->ssl == 0) {
        if (parent && parent->ssl) {
            route->ssl = mprCloneSsl(parent->ssl);
        } else {
            route->ssl = mprCreateSsl(1);
        }
    } else {
        if (parent && route->ssl == parent->ssl) {
            route->ssl = mprCloneSsl(parent->ssl);
        }
    }
    httpParseAll(route, key, prop);
}


static void parseSslAuthorityFile(HttpRoute *route, cchar *key, MprJson *prop)
{
    cchar   *path;

    path = httpExpandRouteVars(route, prop->value);
    if (path && *path) {
        if (!mprPathExists(path, R_OK)) {
            httpParseError(route, "Cannot find ssl.authority.file %s", path);
        } else {
            mprSetSslCaFile(route->ssl, path);
        }
    }
}


static void parseSslCertificate(HttpRoute *route, cchar *key, MprJson *prop)
{
    cchar   *path;

    path = httpExpandRouteVars(route, prop->value);
    if (path && *path) {
        if (!mprPathExists(path, R_OK)) {
            httpParseError(route, "Cannot find ssl certificate %s", path);
        } else {
            mprSetSslCertFile(route->ssl, path);
        }
    }
}


static void parseSslCiphers(HttpRoute *route, cchar *key, MprJson *prop)
{
    mprAddSslCiphers(route->ssl, getList(prop));
}


static void parseSslKey(HttpRoute *route, cchar *key, MprJson *prop)
{
    cchar   *path;

    path = httpExpandRouteVars(route, prop->value);
    if (path && *path) {
        if (!mprPathExists(path, R_OK)) {
            httpParseError(route, "Cannot find ssl.key %s", path);
        } else {
            mprSetSslKeyFile(route->ssl, path);
        }
    }
}


static void parseSslProtocols(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    cchar       *value;
    int         bit, clear, ji, mask;

    mask = 0;
    for (ITERATE_CONFIG(route, prop, child, ji)) {
        value = child->value;
        clear = 0;
        if (sstarts(value, "+")) {
            value++;
        } else if (sstarts(value, "-")) {
            clear = 1;
            value++;
        }
        bit = 0;
        if (scaselessmatch(value, "all")) {
            /* Do not include insecure SSLv2 and SSLv3 */
            bit = MPR_PROTO_TLSV1 | MPR_PROTO_TLSV1_2;
        } else if (scaselessmatch(value, "sslv2")) {
            /* SSLv2 is insecure */
            bit = MPR_PROTO_SSLV2;
        } else if (scaselessmatch(value, "sslv3")) {
            /* SSLv3 is insecure */
            bit = MPR_PROTO_SSLV3;
        } else if (scaselessmatch(value, "tlsv1") || scaselessmatch(value, "tls")) {
            bit = MPR_PROTO_TLSV1;
        } else if (scaselessmatch(value, "tlsv1.0")) {
            bit = MPR_PROTO_TLSV1_0;
        } else if (scaselessmatch(value, "tlsv1.1")) {
            bit = MPR_PROTO_TLSV1_1;
        } else if (scaselessmatch(value, "tlsv1.2")) {
            bit = MPR_PROTO_TLSV1_2;
        } else if (scaselessmatch(value, "tlsv1.3")) {
            bit = MPR_PROTO_TLSV1_3;
        }
        if (clear) {
            mask &= ~bit;
        } else {
            mask |= bit;
        }
    }
    mprSetSslProtocols(route->ssl, mask);
}


static void parseSslLogLevel(HttpRoute *route, cchar *key, MprJson *prop)
{
    mprSetSslLogLevel(route->ssl, (int) stoi(prop->value));
}


static void parseSslRenegotiate(HttpRoute *route, cchar *key, MprJson *prop)
{
    mprSetSslRenegotiate(route->ssl, (prop->type & MPR_JSON_TRUE) ? 1 : 0);
}


static void parseSslTicket(HttpRoute *route, cchar *key, MprJson *prop)
{
    mprSetSslTicket(route->ssl, (prop->type & MPR_JSON_TRUE) ? 1 : 0);
}


static void parseSslVerifyClient(HttpRoute *route, cchar *key, MprJson *prop)
{
    mprVerifySslPeer(route->ssl, (prop->type & MPR_JSON_TRUE) ? 1 : 0);
}


static void parseSslVerifyIssuer(HttpRoute *route, cchar *key, MprJson *prop)
{
    mprVerifySslIssuer(route->ssl, (prop->type & MPR_JSON_TRUE) ? 1 : 0);
}


static void parseStealth(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteStealth(route, (prop->type & MPR_JSON_TRUE) ? 1 : 0);
}


static void parseStream(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    cchar       *mime, *stream, *uri;
    int         ji;

    for (ITERATE_CONFIG(route, prop, child, ji)) {
        if ((mime = mprGetJson(child, "mime")) == 0) {
            mime = "*";
        }
        if ((stream = mprGetJson(child, "stream")) == 0) {
            stream = "true";
        }
        if ((uri = mprGetJson(child, "uri")) == 0) {
            uri = "/";
        }
        httpSetStreaming(route->host, mime, uri, smatch(stream, "true"));
    }
}


/*
    Operations: "close", "redirect", "run", "write"
    Args:
        close:      [immediate]
        redirect:   status URI
        run:        ${DOCUMENT_ROOT}/${request:uri}
        run:        ${controller}-${name}
        write:      [-r] status "Hello World\r\n"
*/
static void parseTarget(HttpRoute *route, cchar *key, MprJson *prop)
{
    cchar   *name, *args;

    if (prop->type & MPR_JSON_OBJ) {
        name = mprReadJson(prop, "operation");
        args = mprReadJson(prop, "args");
    } else {
        name = "run";
        args = prop->value;
    }
    httpSetRouteTarget(route, name, args);
}


static void parseTimeouts(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpGraduateLimits(route, 0);
    httpParseAll(route, key, prop);
}


static void parseTimeoutsExit(HttpRoute *route, cchar *key, MprJson *prop)
{
    mprSetExitTimeout(httpGetTicks(prop->value));
}


static void parseTimeoutsParse(HttpRoute *route, cchar *key, MprJson *prop)
{
    if (! mprGetDebugMode()) {
        route->limits->requestParseTimeout = httpGetTicks(prop->value);
    }
}


static void parseTimeoutsInactivity(HttpRoute *route, cchar *key, MprJson *prop)
{
    if (! mprGetDebugMode()) {
        route->limits->inactivityTimeout = httpGetTicks(prop->value);
    }
}


static void parseTimeoutsRequest(HttpRoute *route, cchar *key, MprJson *prop)
{
    if (! mprGetDebugMode()) {
        route->limits->requestTimeout = httpGetTicks(prop->value);
    }
}


static void parseTimeoutsSession(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->limits->sessionTimeout = httpGetTicks(prop->value);
}


static void parseTrace(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *levels, *child;
    cchar       *location;
    ssize       logSize, maxContent;
    cchar       *format, *formatter;
    char        level;
    int         anew, backup, ji;

    if (route->trace && route->trace->flags & MPR_LOG_CMDLINE) {
        mprLog("info http config", 0, "Already tracing. Ignoring trace configuration in config file.");
        return;
    }
    logSize = (ssize) httpGetNumber(mprReadJson(prop, "size"));
    format = mprReadJson(prop, "format");
    formatter = mprReadJson(prop, "formatter");
    location = mprReadJson(prop, "location");
    level = (char) stoi(mprReadJson(prop, "level"));
    backup = (int) stoi(mprReadJson(prop, "backup"));
    anew = smatch(mprReadJson(prop, "anew"), "true");
    maxContent = (ssize) httpGetNumber(mprReadJson(prop, "content"));

    if (level < 0) {
        level = 0;
    } else if (level > 5) {
        level = 5;
    }
    if (logSize < (10 * 1000)) {
        if (logSize) {
            mprLog("warn http config", 0, "Trace log size is too small, setting to 10MB. Must be larger than 10K.");
        }
        logSize = 10 * 1000 * 1000;
    }
    if (maxContent == 0) {
        maxContent = 40 * 1024;
    }
    if (location == 0) {
        httpParseError(route, "Missing trace filename");
        return;
    }
    if (!smatch(location, "stdout") && !smatch(location, "stderr")) {
        location = httpMakePath(route, 0, location);
    }
    if ((levels = mprReadJsonObj(prop, "levels")) != 0) {
        for (ITERATE_CONFIG(route, prop, child, ji)) {
            httpSetTraceEventLevel(route->trace, child->name, (int) stoi(child->value));
        }
    }
    route->trace = httpCreateTrace(route->trace);
    httpSetTraceFormatterName(route->trace, formatter);
    httpSetTraceLogFile(route->trace, location, logSize, backup, format, anew ? MPR_LOG_ANEW : 0);
    httpSetTraceFormat(route->trace, format);
    httpSetTraceContentSize(route->trace, maxContent);
    httpSetTraceLevel(route->trace, level);
}


static void parseWebSocketsProtocol(HttpRoute *route, cchar *key, MprJson *prop)
{
    route->webSocketsProtocol = sclone(prop->value);
}


static void parseXsrf(HttpRoute *route, cchar *key, MprJson *prop)
{
    httpSetRouteXsrf(route, (prop->type & MPR_JSON_TRUE) ? 1 : 0);
}


PUBLIC uint64 httpGetNumber(cchar *value)
{
    uint64  number;

    if (sstarts(value, "unlimited") || sstarts(value, "infinite") || sstarts(value, "never")) {
        return HTTP_UNLIMITED;
    }
    value = strim(slower(value), " \t", MPR_TRIM_BOTH);
    if (sends(value, "sec") || sends(value, "secs") || sends(value, "seconds") || sends(value, "seconds")) {
        number = stoi(value);
    } else if (sends(value, "min") || sends(value, "mins") || sends(value, "minute") || sends(value, "minutes")) {
        number = stoi(value) * 60;
    } else if (sends(value, "hr") || sends(value, "hrs") || sends(value, "hour") || sends(value, "hours")) {
        number = stoi(value) * 60 * 60;
    } else if (sends(value, "day") || sends(value, "days")) {
        number = stoi(value) * 60 * 60 * 24;
    } else if (sends(value, "week") || sends(value, "weeks")) {
        number = stoi(value) * 60 * 60 * 24 * 7;
    } else if (sends(value, "month") || sends(value, "months")) {
        number = stoi(value) * 60 * 60 * 24 * 30;
    } else if (sends(value, "year") || sends(value, "years")) {
        number = stoi(value) * 60 * 60 * 24 * 365;
    } else if (sends(value, "kb") || sends(value, "k")) {
        number = stoi(value) * 1024;
    } else if (sends(value, "mb") || sends(value, "m")) {
        number = stoi(value) * 1024 * 1024;
    } else if (sends(value, "gb") || sends(value, "g")) {
        number = stoi(value) * 1024 * 1024 * 1024;
    } else if (sends(value, "byte") || sends(value, "bytes")) {
        number = stoi(value);
    } else {
        number = stoi(value);
    }
    return number;
}


PUBLIC MprTicks httpGetTicks(cchar *value)
{
    uint64  num;

    num = httpGetNumber(value);
    if (num >= (MAXINT64 / TPS)) {
        num = MAXINT64 / TPS;
    }
    return num * TPS;
}


PUBLIC int httpGetInt(cchar *value)
{
    uint64  num;

    num = httpGetNumber(value);
    if (num >= MAXINT) {
        num = MAXINT;
    }
    return (int) num;
}


PUBLIC int httpInitParser()
{
    HTTP->parsers = mprCreateHash(0, MPR_HASH_STATIC_VALUES);

    /*
        Parse callbacks keys are specified as they are defined in the json files
     */
    httpAddConfig("directories", parseDirectories);
    httpAddConfig("http", parseHttp);
    httpAddConfig("http.aliases", parseAliases);
    httpAddConfig("http.attach", parseAttach);
    httpAddConfig("http.auth", parseAuth);
    httpAddConfig("http.auth.auto", httpParseAll);
    httpAddConfig("http.auth.auto.name", parseAuthAutoName);
    httpAddConfig("http.auth.auto.roles", parseAuthAutoRoles);
    httpAddConfig("http.auth.login", parseAuthLogin);
    httpAddConfig("http.auth.realm", parseAuthRealm);
    httpAddConfig("http.auth.require", httpParseAll);
    httpAddConfig("http.auth.require.abilities", parseAuthRequireRoles);
    httpAddConfig("http.auth.require.roles", parseAuthRequireRoles);
    httpAddConfig("http.auth.require.users", parseAuthRequireUsers);
    httpAddConfig("http.auth.roles", parseAuthRoles);
    httpAddConfig("http.auth.session", httpParseAll);
    httpAddConfig("http.auth.session.cookie", parseAuthSessionCookie);
    httpAddConfig("http.auth.session.persist", parseAuthSessionCookiePersist);
    httpAddConfig("http.auth.session.same", parseAuthSessionCookieSame);
    httpAddConfig("http.auth.session.enable", parseAuthSessionEnable);
    httpAddConfig("http.auth.session.visible", parseAuthSessionVisible);
    httpAddConfig("http.auth.store", parseAuthStore);
    httpAddConfig("http.auth.type", parseAuthType);
    httpAddConfig("http.auth.users", parseAuthUsers);
    httpAddConfig("http.autoFinalize", parseAutoFinalize);
    httpAddConfig("http.cache", parseCache);
    httpAddConfig("http.canonical", parseCanonicalName);
    httpAddConfig("http.cgi", httpParseAll);
    httpAddConfig("http.cgi.escape", parseCgiEscape);
    httpAddConfig("http.cgi.prefix", parseCgiPrefix);
    httpAddConfig("http.charset", parseCharset);
    httpAddConfig("http.compress", parseCompress);
    httpAddConfig("http.conditions", parseConditions);
    httpAddConfig("http.database", parseDatabase);
    httpAddConfig("http.deleteUploads", parseDeleteUploads);
    httpAddConfig("http.directories", parseDirectories);
    httpAddConfig("http.documents", parseDocuments);
    httpAddConfig("http.errors", parseErrors);
    httpAddConfig("http.formats", httpParseAll);
    httpAddConfig("http.formats.response", parseFormatsResponse);
    httpAddConfig("http.handler", parseHandler);
    httpAddConfig("http.headers", httpParseAll);
    httpAddConfig("http.headers.add", parseHeadersAdd);
    httpAddConfig("http.headers.remove", parseHeadersRemove);
    httpAddConfig("http.headers.set", parseHeadersSet);
    httpAddConfig("http.home", parseHome);
    httpAddConfig("http.hosts", parseHosts);
    httpAddConfig("http.http2", parseHttp2);
    httpAddConfig("http.indexes", parseIndexes);
    httpAddConfig("http.languages", parseLanguages);
    httpAddConfig("http.limits", parseLimits);
    httpAddConfig("http.limits.cache", parseLimitsCache);
    httpAddConfig("http.limits.cacheItem", parseLimitsCacheItem);
    httpAddConfig("http.limits.chunk", parseLimitsChunk);
    httpAddConfig("http.limits.clients", parseLimitsClients);
    httpAddConfig("http.limits.connections", parseLimitsConnections);
    httpAddConfig("http.limits.connectionsPerClient", parseLimitsConnectionsPerClient);
    httpAddConfig("http.limits.depletion", parseLimitsDepletion);
    httpAddConfig("http.limits.keepAlive", parseLimitsKeepAlive);
    httpAddConfig("http.limits.files", parseLimitsFiles);
    httpAddConfig("http.limits.memory", parseLimitsMemory);
    httpAddConfig("http.limits.rxBody", parseLimitsRxBody);
    httpAddConfig("http.limits.rxForm", parseLimitsRxForm);
    httpAddConfig("http.limits.rxHeader", parseLimitsRxHeader);
    httpAddConfig("http.limits.packet", parseLimitsPacket);
    httpAddConfig("http.limits.processes", parseLimitsProcesses);
    httpAddConfig("http.limits.requests", parseLimitsRequestsPerClient);
    httpAddConfig("http.limits.sessions", parseLimitsSessions);
    httpAddConfig("http.limits.txBody", parseLimitsTxBody);
    httpAddConfig("http.limits.upload", parseLimitsUpload);
    httpAddConfig("http.limits.uri", parseLimitsUri);
    httpAddConfig("http.limits.workers", parseLimitsWorkers);
    httpAddConfig("http.log", parseLog);
    httpAddConfig("http.methods", parseMethods);
    httpAddConfig("http.mode", parseProfile);
    httpAddConfig("http.name", parseName);
    httpAddConfig("http.params", parseParams);
    httpAddConfig("http.pattern", parsePattern);
    httpAddConfig("http.pipeline", httpParseAll);
    httpAddConfig("http.pipeline.filters", parsePipelineFilters);
    httpAddConfig("http.pipeline.handler", parsePipelineHandler);
    httpAddConfig("http.pipeline.handlers", parsePipelineHandlers);
    httpAddConfig("http.profile", parseProfile);
    httpAddConfig("http.prefix", parsePrefix);
    httpAddConfig("http.redirect", parseRedirect);
    httpAddConfig("http.renameUploads", parseRenameUploads);
    httpAddConfig("http.routes", parseRoutes);
    httpAddConfig("http.resources", parseResources);
    httpAddConfig("http.scheme", parseScheme);
    httpAddConfig("http.server", httpParseAll);
    httpAddConfig("http.server.account", parseServerAccount);
    httpAddConfig("http.server.listen", parseServerListen);
    httpAddConfig("http.server.modules", parseServerModules);
    httpAddConfig("http.server.monitors", parseServerMonitors);
    httpAddConfig("http.showErrors", parseShowErrors);
    httpAddConfig("http.source", parseSource);
    httpAddConfig("http.ssl", parseSsl);
    httpAddConfig("http.ssl.authority", httpParseAll);
    httpAddConfig("http.ssl.authority.file", parseSslAuthorityFile);
    httpAddConfig("http.ssl.certificate", parseSslCertificate);
    httpAddConfig("http.ssl.ciphers", parseSslCiphers);
    httpAddConfig("http.ssl.key", parseSslKey);
    httpAddConfig("http.ssl.logLevel", parseSslLogLevel);
    httpAddConfig("http.ssl.protocols", parseSslProtocols);
    httpAddConfig("http.ssl.renegotiate", parseSslRenegotiate);
    httpAddConfig("http.ssl.ticket", parseSslTicket);
    httpAddConfig("http.ssl.verify", httpParseAll);
    httpAddConfig("http.ssl.verify.client", parseSslVerifyClient);
    httpAddConfig("http.ssl.verify.issuer", parseSslVerifyIssuer);
    httpAddConfig("http.stealth", parseStealth);
    httpAddConfig("http.stream", parseStream);
    httpAddConfig("http.target", parseTarget);
    httpAddConfig("http.timeouts", parseTimeouts);
    httpAddConfig("http.timeouts.exit", parseTimeoutsExit);
    httpAddConfig("http.timeouts.parse", parseTimeoutsParse);
    httpAddConfig("http.timeouts.inactivity", parseTimeoutsInactivity);
    httpAddConfig("http.timeouts.request", parseTimeoutsRequest);
    httpAddConfig("http.timeouts.session", parseTimeoutsSession);
    httpAddConfig("http.trace", parseTrace);
    httpAddConfig("http.websockets.protocol", parseWebSocketsProtocol);
    httpAddConfig("http.xsrf", parseXsrf);

#if ME_HTTP_DEFENSE
    httpAddConfig("http.server.defenses", parseServerDefenses);
#endif

#if ME_HTTP_HTTP2
    httpAddConfig("http.limits.streams", parseLimitsStreams);
    httpAddConfig("http.limits.window", parseLimitsWindow);
#endif

#if ME_HTTP_WEB_SOCKETS
    httpAddConfig("http.limits.webSockets", parseLimitsWebSockets);
    httpAddConfig("http.limits.webSocketsMessage", parseLimitsWebSocketsMessage);
    httpAddConfig("http.limits.webSocketsPacket", parseLimitsWebSocketsPacket);
    httpAddConfig("http.limits.webSocketsFrame", parseLimitsWebSocketsFrame);
#endif

#if DEPRECATED
    httpAddConfig("http.server.log", parseLog);
    httpAddConfig("http.limits.buffer", parseLimitsPacket);
#endif
    return 0;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/digest.c ************/

/*
    digest.c - Digest Authorization

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



#if ME_HTTP_DIGEST
/********************************** Locals ************************************/
/*
    Per-request digest authorization data
    @see HttpAuth
    @ingroup HttpAuth
    @stability Evolving
 */
typedef struct HttpDigest {
    char    *algorithm;
    char    *cnonce;
    char    *domain;
    char    *nc;
    char    *nonce;
    char    *opaque;
    char    *qop;
    char    *realm;
    char    *stale;
    char    *uri;
} HttpDigest;


/********************************** Forwards **********************************/

static char *calcDigest(HttpStream *stream, HttpDigest *dp, cchar *username);
static char *createDigestNonce(HttpStream *stream, cchar *secret, cchar *realm);
static void manageDigestData(HttpDigest *dp, int flags);
static int parseDigestNonce(char *nonce, cchar **secret, cchar **realm, MprTime *when);

/*********************************** Code *************************************/
/*
    Parse the 'Authorization' header and the server 'Www-Authenticate' header
 */
PUBLIC int httpDigestParse(HttpStream *stream, cchar **username, cchar **password)
{
    HttpRx      *rx;
    HttpDigest  *dp;
    MprTime     when;
    char        *value, *tok, *key, *cp, *sp;
    cchar       *secret, *realm;
    int         seenComma;

    rx = stream->rx;
    if (password) {
        *password = NULL;
    }
    if (username) {
        *username = NULL;
    }
    if (!rx->authDetails) {
        return 0;
    }
    dp = stream->authData = mprAllocObj(HttpDigest, manageDigestData);
    key = sclone(rx->authDetails);

    while (*key) {
        while (*key && isspace((uchar) *key)) {
            key++;
        }
        tok = key;
        while (*tok && !isspace((uchar) *tok) && *tok != ',' && *tok != '=') {
            tok++;
        }
        if (*tok) {
            *tok++ = '\0';
        }
        while (isspace((uchar) *tok)) {
            tok++;
        }
        seenComma = 0;
        if (*tok == '\"') {
            value = ++tok;
            while (*tok && *tok != '\"') {
                tok++;
            }
        } else {
            value = tok;
            while (*tok && *tok != ',') {
                tok++;
            }
            seenComma++;
        }
        if (*tok) {
            *tok++ = '\0';
        }

        /*
            Handle back-quoting
         */
        if (strchr(value, '\\')) {
            for (cp = sp = value; *sp; sp++) {
                if (*sp == '\\') {
                    sp++;
                }
                *cp++ = *sp++;
            }
            *cp = '\0';
        }

        /*
            user, response, oqaque, uri, realm, nonce, nc, cnonce, qop
         */
        switch (tolower((uchar) *key)) {
        case 'a':
            if (scaselesscmp(key, "algorithm") == 0) {
                dp->algorithm = sclone(value);
                break;
            } else if (scaselesscmp(key, "auth-param") == 0) {
                break;
            }
            break;

        case 'c':
            if (scaselesscmp(key, "cnonce") == 0) {
                dp->cnonce = sclone(value);
            }
            break;

        case 'd':
            if (scaselesscmp(key, "domain") == 0) {
                dp->domain = sclone(value);
                break;
            }
            break;

        case 'n':
            if (scaselesscmp(key, "nc") == 0) {
                dp->nc = sclone(value);
            } else if (scaselesscmp(key, "nonce") == 0) {
                dp->nonce = sclone(value);
            }
            break;

        case 'o':
            if (scaselesscmp(key, "opaque") == 0) {
                dp->opaque = sclone(value);
            }
            break;

        case 'q':
            if (scaselesscmp(key, "qop") == 0) {
                dp->qop = sclone(value);
            }
            break;

        case 'r':
            if (scaselesscmp(key, "realm") == 0) {
                dp->realm = sclone(value);
            } else if (scaselesscmp(key, "response") == 0) {
                /*
                    Store the response digest in the password field. This is MD5(HA1:nonce:HA2) where
                    HA1 is MD5(user:realm:password) and HA2 is MD5(method:digestUri)
                 */
                if (password) {
                    *password = sclone(value);
                }
                stream->encoded = 1;
            }
            break;

        case 's':
            if (scaselesscmp(key, "stale") == 0) {
                break;
            }

        case 'u':
            if (scaselesscmp(key, "uri") == 0) {
                dp->uri = sclone(value);
            } else if (scaselesscmp(key, "username") == 0 || scaselesscmp(key, "user") == 0) {
                if (username) {
                    *username = sclone(value);
                }
            }
            break;

        default:
            /*  Just ignore keywords we don't understand */
            ;
        }
        key = tok;
        if (!seenComma) {
            while (*key && *key != ',') {
                key++;
            }
            if (*key) {
                key++;
            }
        }
    }
    if (username && *username == 0) {
        return MPR_ERR_BAD_FORMAT;
    }
    if (password && *password == 0) {
        return MPR_ERR_BAD_FORMAT;
    }
    if (dp->realm == 0 || dp->nonce == 0 || dp->uri == 0) {
        return MPR_ERR_BAD_FORMAT;
    }
    if (dp->qop && (dp->cnonce == 0 || dp->nc == 0)) {
        return MPR_ERR_BAD_FORMAT;
    }
    if (httpServerStream(stream)) {
        realm = secret = 0;
        when = 0;
        parseDigestNonce(dp->nonce, &secret, &realm, &when);
        if (!smatch(stream->http->secret, secret)) {
            httpLog(stream->trace, "auth.digest.error", "error", "msg:Access denied. Nonce mismatch");
            return MPR_ERR_BAD_STATE;

        } else if (!smatch(realm, rx->route->auth->realm)) {
            httpLog(stream->trace, "auth.digest.error", "error", "msg:Access denied. Realm mismatch");
            return MPR_ERR_BAD_STATE;

        } else if (dp->qop && !smatch(dp->qop, "auth")) {
            httpLog(stream->trace, "auth.digest.error", "error", "msg:Access denied. Bad qop");
            return MPR_ERR_BAD_STATE;

        } else if ((when + ME_DIGEST_NONCE_DURATION) < time(0)) {
            httpLog(stream->trace, "auth.digest.error", "error", "msg:Access denied. Nonce is stale");
            return MPR_ERR_BAD_STATE;
        }
        rx->passwordDigest = calcDigest(stream, dp, *username);
    } else {
        if (dp->domain == 0 || dp->opaque == 0 || dp->algorithm == 0 || dp->stale == 0) {
            return MPR_ERR_BAD_FORMAT;
        }
    }
    return 0;
}


static void manageDigestData(HttpDigest *dp, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(dp->algorithm);
        mprMark(dp->cnonce);
        mprMark(dp->domain);
        mprMark(dp->nc);
        mprMark(dp->nonce);
        mprMark(dp->opaque);
        mprMark(dp->qop);
        mprMark(dp->realm);
        mprMark(dp->stale);
        mprMark(dp->uri);
    }
}


/*
    Respond to the request by asking for a login
    Only called if not logged in.
 */
PUBLIC void httpDigestLogin(HttpStream *stream)
{
    HttpAuth    *auth;
    char        *nonce, *opaque;

    auth = stream->rx->route->auth;

    if (auth->loginPage && !sends(stream->rx->referrer, auth->loginPage)) {
        httpRedirect(stream, HTTP_CODE_MOVED_TEMPORARILY, auth->loginPage);
    } else {
        nonce = createDigestNonce(stream, stream->http->secret, auth->realm);
        /* Opaque is unused, set to anything */
        opaque = "799d5";

        if (smatch(auth->qop, "none")) {
            httpSetHeader(stream, "WWW-Authenticate", "Digest realm=\"%s\", nonce=\"%s\"", auth->realm, nonce);
        } else {
            /* qop value of null defaults to "auth" */
            httpSetHeader(stream, "WWW-Authenticate", "Digest realm=\"%s\", domain=\"%s\", "
                "qop=\"auth\", nonce=\"%s\", opaque=\"%s\", algorithm=\"MD5\", stale=\"FALSE\"",
                auth->realm, "/", nonce, opaque);
        }
        httpSetContentType(stream, "text/plain");
        httpError(stream, HTTP_CODE_UNAUTHORIZED, "Access denied. Login required");
    }
}


/*
    Add the 'Authorization' header for authenticated requests
    Must first get a 401 response to get the authData.
 */
PUBLIC bool httpDigestSetHeaders(HttpStream *stream, cchar *username, cchar *password)
{
    Http        *http;
    HttpTx      *tx;
    HttpDigest  *dp;
    char        *ha1, *ha2, *digest, *cnonce;

    http = stream->http;
    tx = stream->tx;
    if ((dp = stream->authData) == 0) {
        /* Need to await a failing auth response */
        return 0;
    }
    cnonce = sfmt("%s:%s:%x", http->secret, dp->realm, (int) http->now);
    ha1 = mprGetMD5(sfmt("%s:%s:%s", username, dp->realm, password));
    ha2 = mprGetMD5(sfmt("%s:%s", tx->method, tx->parsedUri->path));
    if (smatch(dp->qop, "auth")) {
        digest = mprGetMD5(sfmt("%s:%s:%s:%s:%s:%s", ha1, dp->nonce, dp->nc, cnonce, dp->qop, ha2));
        httpAddHeader(stream, "Authorization", "Digest username=\"%s\", realm=\"%s\", domain=\"%s\", "
            "algorithm=\"MD5\", qop=\"%s\", cnonce=\"%s\", nc=\"%s\", nonce=\"%s\", opaque=\"%s\", "
            "stale=\"FALSE\", uri=\"%s\", response=\"%s\"", username, dp->realm, dp->domain, dp->qop,
            cnonce, dp->nc, dp->nonce, dp->opaque, tx->parsedUri->path, digest);
    } else {
        digest = mprGetMD5(sfmt("%s:%s:%s", ha1, dp->nonce, ha2));
        httpAddHeader(stream, "Authorization", "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", "
            "uri=\"%s\", response=\"%s\"", username, dp->realm, dp->nonce, tx->parsedUri->path, digest);
    }
    return 1;
}


/*
    Create a nonce value for digest authentication (RFC 2617)
 */
static char *createDigestNonce(HttpStream *stream, cchar *secret, cchar *realm)
{
    static int64 next = 0;

    assert(realm && *realm);
    return mprEncode64(sfmt("%s:%s:%llx:%llx", secret, realm, mprGetTime(), next++));
}


static int parseDigestNonce(char *nonce, cchar **secret, cchar **realm, MprTime *when)
{
    char    *tok, *decoded, *whenStr;

    if ((decoded = mprDecode64(nonce)) == 0) {
        return MPR_ERR_CANT_READ;
    }
    *secret = stok(decoded, ":", &tok);
    *realm = stok(NULL, ":", &tok);
    whenStr = stok(NULL, ":", &tok);
    *when = (MprTime) stoiradix(whenStr, 16, NULL);
    return 0;
}


/*
    Get a password digest using the MD5 algorithm -- See RFC 2617 to understand this code.
 */
static char *calcDigest(HttpStream *stream, HttpDigest *dp, cchar *username)
{
    HttpAuth    *auth;
    char        *digestBuf, *ha1, *ha2;

    auth = stream->rx->route->auth;
    if (!stream->user) {
        stream->user = mprLookupKey(auth->userCache, username);
    }
    assert(stream->user && stream->user->password);
    if (stream->user == 0 || stream->user->password == 0) {
        return 0;
    }

    /*
        Compute HA1. Password is already expected to be in the HA1 format MD5(username:realm:password).
     */
    ha1 = sclone(stream->user->password);

    /*
        HA2
     */
    if (stream->rx->route->flags & HTTP_ROUTE_DOTNET_DIGEST_FIX) {
        char *uri = stok(sclone(dp->uri), "?", 0);
        ha2 = mprGetMD5(sfmt("%s:%s", stream->rx->method, uri));
    } else {
        ha2 = mprGetMD5(sfmt("%s:%s", stream->rx->method, dp->uri));
    }

    /*
        H(HA1:nonce:HA2)
     */
    if (scmp(dp->qop, "auth") == 0) {
        digestBuf = sfmt("%s:%s:%s:%s:%s:%s", ha1, dp->nonce, dp->nc, dp->cnonce, dp->qop, ha2);
    } else {
        digestBuf = sfmt("%s:%s:%s", ha1, dp->nonce, ha2);
    }
    return mprGetMD5(digestBuf);
}

#endif /* ME_HTTP_DIGEST */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/dirHandler.c ************/

/*
    dirHandler.c - Directory listing handler

    The dirHandler is unusual in that is is called (only) from the fileHandler.
    The fileHandler tests if the request is for a directory and then examines if redirection
    to an index, or rendering a directory listing is required. If a listing, the request is
    relayed here.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



#if ME_HTTP_DIR
/********************************** Defines ***********************************/

#define DIR_NAME "dirHandler"

/****************************** Forward Declarations **************************/

static void filterDirList(HttpStream *stream, MprList *list);
static void manageDir(HttpDir *dir, int flags);
static int  matchDirPattern(cchar *pattern, cchar *file);
static void outputFooter(HttpQueue *q);
static void outputHeader(HttpQueue *q, cchar *dir, int nameSize);
static void outputLine(HttpQueue *q, MprDirEntry *ep, cchar *dir, int nameSize);
static void parseDirQuery(HttpStream *stream);
static void sortList(HttpStream *stream, MprList *list);
static void startDir(HttpQueue *q);

/************************************* Code ***********************************/
/*
    Loadable module initialization
 */
PUBLIC int httpOpenDirHandler()
{
    HttpStage   *handler;
    HttpDir     *dir;

    if ((handler = httpCreateHandler("dirHandler", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    if ((handler->stageData = dir = mprAllocObj(HttpDir, manageDir)) == 0) {
        return MPR_ERR_MEMORY;
    }
    HTTP->dirHandler = handler;
    handler->flags |= HTTP_STAGE_INTERNAL;
    handler->start = startDir;
    dir->sortOrder = 1;
    return 0;
}

/*
    Test if this request is for a directory listing. This routine is called directly by the
    fileHandler. Directory listings are enabled in a route via "Options Indexes".
 */
PUBLIC bool httpShouldRenderDirListing(HttpStream *stream)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpDir     *dir;

    tx = stream->tx;
    rx = stream->rx;
    assert(tx->filename);
    assert(tx->fileInfo.checked);

    if ((dir = httpGetRouteData(rx->route, DIR_NAME)) == 0) {
        return 0;
    }
    if (dir->enabled && tx->fileInfo.isDir && sends(rx->pathInfo, "/")) {
        stream->reqData = dir;
        return 1;
    }
    return 0;
}


/*
    Start the request (and complete it)
 */
static void startDir(HttpQueue *q)
{
    HttpStream      *stream;
    HttpTx          *tx;
    HttpRx          *rx;
    MprList         *list;
    MprDirEntry     *dp;
    HttpDir         *dir;
    cchar           *path;
    uint            nameSize;
    int             next;

    stream = q->stream;
    rx = stream->rx;
    tx = stream->tx;
    if ((dir = stream->reqData) == 0) {
        httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot get directory listing");
        return;
    }
    assert(tx->filename);

    if (!(rx->flags & (HTTP_GET | HTTP_HEAD))) {
        httpError(stream, HTTP_CODE_BAD_METHOD, "Bad method");
        return;
    }
    httpSetContentType(stream, "text/html");
    httpSetHeaderString(stream, "Cache-Control", "no-cache");
    httpSetHeaderString(stream, "Last-Modified", stream->http->currentDate);
    parseDirQuery(stream);

    if ((list = mprGetPathFiles(tx->filename, MPR_PATH_RELATIVE)) == 0) {
        httpWrite(q, "<h2>Cannot get file list</h2>\r\n");
        outputFooter(q);
        return;
    }
    if (dir->pattern) {
        filterDirList(stream, list);
    }
    sortList(stream, list);
    /*
        Get max filename size
     */
    nameSize = 0;
    for (next = 0; (dp = mprGetNextItem(list, &next)) != 0; ) {
        nameSize = max((int) strlen(dp->name), nameSize);
    }
    nameSize = max(nameSize, 22);

    path = rx->route->prefix ? sjoin(rx->route->prefix, rx->pathInfo, NULL) : rx->pathInfo;
    outputHeader(q, path, nameSize);
    for (next = 0; (dp = mprGetNextItem(list, &next)) != 0; ) {
        outputLine(q, dp, tx->filename, nameSize);
    }
    outputFooter(q);
    httpFinalize(stream);
}


static void parseDirQuery(HttpStream *stream)
{
    HttpRx      *rx;
    HttpDir     *dir;
    char        *value, *query, *next, *tok, *field;

    rx = stream->rx;
    dir = stream->reqData;

    query = sclone(rx->parsedUri->query);
    if (query == 0) {
        return;
    }
    tok = stok(query, ";&", &next);
    while (tok) {
        if ((value = strchr(tok, '=')) != 0) {
            *value++ = '\0';
            if (*tok == 'C') {                  /* Sort column */
                field = 0;
                if (*value == 'N') {
                    field = "Name";
                } else if (*value == 'M') {
                    field = "Date";
                } else if (*value == 'S') {
                    field = "Size";
                }
                if (field) {
                    dir->sortField = sclone(field);
                }

            } else if (*tok == 'O') {           /* Sort order */
                if (*value == 'A') {
                    dir->sortOrder = 1;
                } else if (*value == 'D') {
                    dir->sortOrder = -1;
                }

            } else if (*tok == 'F') {           /* Format */
                if (*value == '0') {
                    dir->fancyIndexing = 0;
                } else if (*value == '1') {
                    dir->fancyIndexing = 1;
                } else if (*value == '2') {
                    dir->fancyIndexing = 2;
                }

            } else if (*tok == 'P') {           /* Pattern */
                dir->pattern = sclone(value);
            }
        }
        tok = stok(next, ";&", &next);
    }
}


static void sortList(HttpStream *stream, MprList *list)
{
    MprDirEntry *tmp, **items;
    HttpDir     *dir;
    int         count, i, j, rc;

    dir = stream->reqData;

    if (dir->sortField == 0) {
        return;
    }
    count = mprGetListLength(list);
    items = (MprDirEntry**) list->items;
    if (scaselessmatch(dir->sortField, "Name")) {
        for (i = 1; i < count; i++) {
            for (j = 0; j < i; j++) {
                rc = strcmp(items[i]->name, items[j]->name);
                if (dir->foldersFirst) {
                    if (items[i]->isDir && !items[j]->isDir) {
                        rc = -dir->sortOrder;
                    } else if (items[j]->isDir && !items[i]->isDir) {
                        rc = dir->sortOrder;
                    }
                }
                rc *= dir->sortOrder;
                if (rc < 0) {
                    tmp = items[i];
                    items[i] = items[j];
                    items[j] = tmp;
                }
            }
        }

    } else if (scaselessmatch(dir->sortField, "Size")) {
        for (i = 1; i < count; i++) {
            for (j = 0; j < i; j++) {
                rc = (items[i]->size < items[j]->size) ? -1 : 1;
                if (dir->foldersFirst) {
                    if (items[i]->isDir && !items[j]->isDir) {
                        rc = -dir->sortOrder;
                    } else if (items[j]->isDir && !items[i]->isDir) {
                        rc = dir->sortOrder;
                    }
                }
                rc *= dir->sortOrder;
                if (rc < 0) {
                    tmp = items[i];
                    items[i] = items[j];
                    items[j] = tmp;
                }
            }
        }

    } else if (scaselessmatch(dir->sortField, "Date")) {
        for (i = 1; i < count; i++) {
            for (j = 0; j < i; j++) {
                rc = (items[i]->lastModified < items[j]->lastModified) ? -1: 1;
                if (dir->foldersFirst) {
                    if (items[i]->isDir && !items[j]->isDir) {
                        rc = -dir->sortOrder;
                    } else if (items[j]->isDir && !items[i]->isDir) {
                        rc = dir->sortOrder;
                    }
                }
                rc *= dir->sortOrder;
                if (rc < 0) {
                    tmp = items[i];
                    items[i] = items[j];
                    items[j] = tmp;
                }
            }
        }
    }
}


static void outputHeader(HttpQueue *q, cchar *path, int nameSize)
{
    HttpDir *dir;
    char    *parent, *parentSuffix;
    int     reverseOrder, fancy, isRootDir;

    dir = q->stream->reqData;
    fancy = 1;
    path = mprEscapeHtml(path);

    httpWrite(q, "<!DOCTYPE HTML PUBLIC \"-/*W3C//DTD HTML 3.2 Final//EN\">\r\n");
    httpWrite(q, "<html>\r\n <head>\r\n  <title>Index of %s</title>\r\n", path);
    httpWrite(q, " </head>\r\n");
    httpWrite(q, "<body>\r\n");

    httpWrite(q, "<h1>Index of %s</h1>\r\n", path);

    if (dir->sortOrder > 0) {
        reverseOrder = 'D';
    } else {
        reverseOrder = 'A';
    }
    if (dir->fancyIndexing == 0) {
        fancy = '0';
    } else if (dir->fancyIndexing == 1) {
        fancy = '1';
    } else if (dir->fancyIndexing == 2) {
        fancy = '2';
    }
    parent = mprGetPathDir(path);
    if (parent[strlen(parent) - 1] != '/') {
        parentSuffix = "/";
    } else {
        parentSuffix = "";
    }
    isRootDir = (strcmp(path, "/") == 0);

    if (dir->fancyIndexing == 2) {
        httpWrite(q, "<table><tr><th><img src=\"/icons/blank.gif\" alt=\"[ICO]\" /></th>");

        httpWrite(q, "<th><a href=\"?C=N;O=%c;F=%c\">Name</a></th>", reverseOrder, fancy);
        httpWrite(q, "<th><a href=\"?C=M;O=%c;F=%c\">Last modified</a></th>", reverseOrder, fancy);
        httpWrite(q, "<th><a href=\"?C=S;O=%c;F=%c\">Size</a></th>", reverseOrder, fancy);
        httpWrite(q, "<th><a href=\"?C=D;O=%c;F=%c\">Description</a></th>\r\n", reverseOrder, fancy);

        httpWrite(q, "</tr><tr><th colspan=\"5\"><hr /></th></tr>\r\n");

        if (! isRootDir) {
            httpWrite(q, "<tr><td valign=\"top\"><img src=\"/icons/back.gif\"");
            httpWrite(q, "alt=\"[DIR]\" /></td><td><a href=\"%s%s\">", parent, parentSuffix);
            httpWrite(q, "Parent Directory</a></td>");
            httpWrite(q, "<td align=\"right\">  - </td></tr>\r\n");
        }

    } else if (dir->fancyIndexing == 1) {
        httpWrite(q, "<pre><img src=\"/icons/space.gif\" alt=\"Icon\" /> ");

        httpWrite(q, "<a href=\"?C=N;O=%c;F=%c\">Name</a>%*s", reverseOrder, fancy, nameSize - 3, " ");
        httpWrite(q, "<a href=\"?C=M;O=%c;F=%c\">Last modified</a>       ", reverseOrder, fancy);
        httpWrite(q, "<a href=\"?C=S;O=%c;F=%c\">Size</a>               ", reverseOrder, fancy);
        httpWrite(q, "<a href=\"?C=D;O=%c;F=%c\">Description</a>\r\n", reverseOrder, fancy);

        httpWrite(q, "<hr />");

        if (! isRootDir) {
            httpWrite(q, "<img src=\"/icons/parent.gif\" alt=\"[DIR]\" />");
            httpWrite(q, " <a href=\"%s%s\">Parent Directory</a>\r\n", parent, parentSuffix);
        }

    } else {
        httpWrite(q, "<ul>\n");
        if (! isRootDir) {
            httpWrite(q, "<li><a href=\"%s%s\"> Parent Directory</a></li>\r\n", parent, parentSuffix);
        }
    }
}


static void fmtNum(char *buf, int bufsize, int num, int divisor, char *suffix)
{
    int     whole, point;

    whole = num / divisor;
    point = (num % divisor) / (divisor / 10);

    if (point == 0) {
        fmt(buf, bufsize, "%6d%s", whole, suffix);
    } else {
        fmt(buf, bufsize, "%4d.%d%s", whole, point, suffix);
    }
}


static void outputLine(HttpQueue *q, MprDirEntry *ep, cchar *path, int nameSize)
{
    MprPath     info;
    MprTime     when;
    HttpDir     *dir;
    char        *newPath, sizeBuf[16], timeBuf[48], *icon;
    struct tm   tm;
    bool        isDir;
    int         len;
    cchar       *ext, *mimeType;
    char        *dirSuffix;
    char        *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    path = mprEscapeHtml(path);
    dir = q->stream->reqData;
    if (ep->size >= (1024 * 1024 * 1024)) {
        fmtNum(sizeBuf, sizeof(sizeBuf), (int) ep->size, 1024 * 1024 * 1024, "G");

    } else if (ep->size >= (1024 * 1024)) {
        fmtNum(sizeBuf, sizeof(sizeBuf), (int) ep->size, 1024 * 1024, "M");

    } else if (ep->size >= 1024) {
        fmtNum(sizeBuf, sizeof(sizeBuf), (int) ep->size, 1024, "K");

    } else {
        fmt(sizeBuf, sizeof(sizeBuf), "%6d", (int) ep->size);
    }
    newPath = mprJoinPath(path, ep->name);

    if (mprGetPathInfo(newPath, &info) < 0) {
        when = mprGetTime();
        isDir = 0;
    } else {
        isDir = info.isDir ? 1 : 0;
        when = (MprTime) info.mtime * TPS;
    }
    if (isDir) {
        icon = "folder";
        dirSuffix = "/";
    } else {
        ext = mprGetPathExt(ep->name);
        if (ext && (mimeType = mprLookupMime(q->stream->rx->route->mimeTypes, ext)) != 0) {
            if (strcmp(ext, "es") == 0 || strcmp(ext, "ejs") == 0 || strcmp(ext, "php") == 0) {
                icon = "text";
            } else if (strstr(mimeType, "text") != 0) {
                icon = "text";
            } else {
                icon = "compressed";
            }
        } else {
            icon = "compressed";
        }
        dirSuffix = "";
    }
    mprDecodeLocalTime(&tm, when);

    fmt(timeBuf, sizeof(timeBuf), "%02d-%3s-%4d %02d:%02d", tm.tm_mday, months[tm.tm_mon], tm.tm_year + 1900,
        tm.tm_hour,  tm.tm_min);
    len = (int) strlen(ep->name) + (int) strlen(dirSuffix);

    if (dir->fancyIndexing == 2) {
        httpWrite(q, "<tr><td valign=\"top\">");
        httpWrite(q, "<img src=\"/icons/%s.gif\" alt=\"[   ]\", /></td>", icon);
        httpWrite(q, "<td><a href=\"%s%s\">%s%s</a></td>", ep->name, dirSuffix, ep->name, dirSuffix);
        httpWrite(q, "<td>%s</td><td>%s</td></tr>\r\n", timeBuf, sizeBuf);

    } else if (dir->fancyIndexing == 1) {
        httpWrite(q, "<img src=\"/icons/%s.gif\" alt=\"[   ]\", /> ", icon);
        httpWrite(q, "<a href=\"%s%s\">%s%s</a>%-*s %17s %4s\r\n", ep->name, dirSuffix, ep->name, dirSuffix,
            nameSize - len, "", timeBuf, sizeBuf);

    } else {
        httpWrite(q, "<li><a href=\"%s%s\"> %s%s</a></li>\r\n", ep->name, dirSuffix, ep->name, dirSuffix);
    }
}


static void outputFooter(HttpQueue *q)
{
    HttpStream  *stream;
    MprSocket   *sock;
    HttpDir     *dir;

    stream = q->stream;
    dir = stream->reqData;

    if (dir->fancyIndexing == 2) {
        httpWrite(q, "<tr><th colspan=\"5\"><hr /></th></tr>\r\n</table>\r\n");

    } else if (dir->fancyIndexing == 1) {
        httpWrite(q, "<hr /></pre>\r\n");
    } else {
        httpWrite(q, "</ul>\r\n");
    }
    sock = stream->sock->listenSock;
    httpWrite(q, "<address>%s %s at %s Port %d</address>\r\n", ME_TITLE, ME_VERSION, sock->ip, sock->port);
    httpWrite(q, "</body></html>\r\n");
}


static void filterDirList(HttpStream *stream, MprList *list)
{
    HttpDir         *dir;
    MprDirEntry     *dp;
    int             next;

    dir = stream->reqData;

    /*
        Do pattern matching. Entries that don't match, free the name to mark
     */
    for (ITERATE_ITEMS(list, dp, next)) {
        if (!matchDirPattern(dir->pattern, dp->name)) {
            mprRemoveItem(list, dp);
            next--;
        }
    }
}


/*
    Return true if the file matches the pattern. Supports '?' and '*'
 */
static int matchDirPattern(cchar *pattern, cchar *file)
{
    cchar   *pp, *fp;

    if (pattern == 0 || *pattern == '\0') {
        return 1;
    }
    if (file == 0 || *file == '\0') {
        return 0;
    }
    for (pp = pattern, fp = file; *pp; ) {
        if (*fp == '\0') {
            if (*pp == '*' && pp[1] == '\0') {
                /* Trailing wild card */
                return 1;
            }
            return 0;
        }
        if (*pp == '*') {
            if (matchDirPattern(&pp[1], &fp[0])) {
                return 1;
            }
            fp++;
            continue;

        } else if (*pp == '?' || *pp == *fp) {
            fp++;

        } else {
            return 0;
        }
        pp++;
    }
    if (*fp == '\0') {
        /* Match */
        return 1;
    }
    return 0;
}


#if DIR_DIRECTIVES
static int addIconDirective(MaState *state, cchar *key, cchar *value)
{
    if (!maTokenize(state, value, "%S %W", &path, &dir->extList)) {
        return MPR_ERR_BAD_SYNTAX;
    }
    return 0;
}


static int defaultIconDirective(MaState *state, cchar *key, cchar *value)
{
    state->dir->defaultIcon = sclone(value);
    return 0;
}

/*
    IndexIgnore pat ...
 */
static int indexIgnoreDirective(MaState *state, cchar *key, cchar *value)
{
    if (!maTokenize(state, value, "%W", &dir->ignoreList)) {
        return MPR_ERR_BAD_SYNTAX;
    }
    return 0;
}
#endif


static void manageDir(HttpDir *dir, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
#if DIR_DIRECTIVES
        mprMark(dir->dirList);
        mprMark(dir->defaultIcon);
        mprMark(dir->extList);
        mprMark(dir->ignoreList);
#endif
        mprMark(dir->pattern);
        mprMark(dir->sortField);
    }
}


static HttpDir *allocDir(HttpRoute *route)
{
    HttpDir *dir;

    if ((dir = mprAllocObj(HttpDir, manageDir)) == 0) {
        return 0;
    }
    httpSetRouteData(route, DIR_NAME, dir);
    return dir;
}


static HttpDir *cloneDir(HttpDir *parent, HttpRoute *route)
{
    HttpDir *dir;

    if ((dir = mprAllocObj(HttpDir, manageDir)) == 0) {
        return 0;
    }
    dir->enabled = parent->enabled;
    dir->fancyIndexing = parent->fancyIndexing;
    dir->foldersFirst = parent->foldersFirst;
    dir->pattern = parent->pattern;
    dir->sortField = parent->sortField;
    dir->sortOrder = parent->sortOrder;
    httpSetRouteData(route, DIR_NAME, dir);
    return dir;
}


PUBLIC HttpDir *httpGetDirObj(HttpRoute *route)
{
    HttpDir     *dir, *parent;

    dir = httpGetRouteData(route, DIR_NAME);
    if (route->parent) {
        /*
            If the parent route has the same route data, then force a clone so the parent route does not get modified
         */
        parent = httpGetRouteData(route->parent, DIR_NAME);
        if (dir == parent) {
            dir = 0;
        }
    }
    if (dir == 0) {
        if (route->parent && (parent = httpGetRouteData(route->parent, DIR_NAME)) != 0) {
            dir = cloneDir(parent, route);
        } else {
            dir = allocDir(route);
        }
    }
    assert(dir);
    return dir;
}


#endif /* ME_HTTP_DIR */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/endpoint.c ************/

/*
    endpoint.c -- Create and manage listening endpoints.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/


#include    "pcre.h"

/********************************** Forwards **********************************/

static void acceptNet(HttpEndpoint *endpoint);
static int manageEndpoint(HttpEndpoint *endpoint, int flags);

/************************************ Code ************************************/
/*
    Create a listening endpoint on ip:port. NOTE: ip may be empty which means bind to all addresses.
 */
PUBLIC HttpEndpoint *httpCreateEndpoint(cchar *ip, int port, MprDispatcher *dispatcher)
{
    HttpEndpoint    *endpoint;

    if ((endpoint = mprAllocObj(HttpEndpoint, manageEndpoint)) == 0) {
        return 0;
    }
    endpoint->http = HTTP;
    endpoint->async = 1;
    endpoint->port = port;
    endpoint->ip = sclone(ip);
    endpoint->dispatcher = dispatcher;
    endpoint->hosts = mprCreateList(-1, MPR_LIST_STABLE);
    endpoint->mutex = mprCreateLock();
    httpAddEndpoint(endpoint);
    return endpoint;
}


PUBLIC void httpDestroyEndpoint(HttpEndpoint *endpoint)
{
    if (endpoint->sock) {
        mprCloseSocket(endpoint->sock, 0);
        endpoint->sock = 0;
    }
    httpRemoveEndpoint(endpoint);
}


static int manageEndpoint(HttpEndpoint *endpoint, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(endpoint->http);
        mprMark(endpoint->hosts);
        mprMark(endpoint->ip);
        mprMark(endpoint->context);
        mprMark(endpoint->limits);
        mprMark(endpoint->sock);
        mprMark(endpoint->dispatcher);
        mprMark(endpoint->ssl);
        mprMark(endpoint->mutex);
    }
    return 0;
}


/*
    Convenience function to create and configure a new endpoint without using a config file.
 */
PUBLIC HttpEndpoint *httpCreateConfiguredEndpoint(HttpHost *host, cchar *home, cchar *documents, cchar *ip, int port)
{
    HttpEndpoint    *endpoint;
    HttpRoute       *route;

    if (host == 0) {
        host = httpGetDefaultHost();
    }
    if (host == 0) {
        return 0;
    }
    if (ip == 0 && port <= 0) {
        /*
            If no IP:PORT specified, find the first endpoint
         */
        if ((endpoint = mprGetFirstItem(HTTP->endpoints)) != 0) {
            ip = endpoint->ip;
            port = endpoint->port;
        } else {
            ip = "localhost";
            if (port <= 0) {
                port = ME_HTTP_PORT;
            }
            if ((endpoint = httpCreateEndpoint(ip, port, NULL)) == 0) {
                return 0;
            }
        }
    } else if ((endpoint = httpCreateEndpoint(ip, port, NULL)) == 0) {
        return 0;
    }
    route = host->defaultRoute;
    httpAddHostToEndpoint(endpoint, host);
    if (documents) {
        httpSetRouteDocuments(route, documents);
    }
    if (home) {
        httpSetRouteHome(route, home);
    }
    httpFinalizeRoute(route);
    return endpoint;
}


static bool validateEndpoint(HttpEndpoint *endpoint)
{
    HttpHost    *host;
    HttpRoute   *route;
    int         nextRoute;

    if ((host = mprGetFirstItem(endpoint->hosts)) == 0) {
        host = httpGetDefaultHost();
        httpAddHostToEndpoint(endpoint, host);

    } else {
        /*
            Move default host to the end of the list so virtual hosts will match first
         */
        if (!host->name && mprGetListLength(endpoint->hosts) > 1) {
            mprRemoveItem(endpoint->hosts, host);
            mprAddItem(endpoint->hosts, host);
        }
    }
    for (nextRoute = 0; (route = mprGetNextItem(host->routes, &nextRoute)) != 0; ) {
        if (!route->handler && !mprLookupKey(route->extensions, "")) {
            httpAddRouteHandler(route, "fileHandler", "");
            httpAddRouteIndex(route, "index.html");
        }
    }
    return 1;
}


PUBLIC int httpStartEndpoint(HttpEndpoint *endpoint)
{
    HttpHost    *host;
    cchar       *proto, *ip;
    int         flags, next;

    if (!validateEndpoint(endpoint)) {
        return MPR_ERR_BAD_ARGS;
    }
    for (ITERATE_ITEMS(endpoint->hosts, host, next)) {
        httpStartHost(host);
    }
    if ((endpoint->sock = mprCreateSocket()) == 0) {
        return MPR_ERR_MEMORY;
    }
    flags = MPR_SOCKET_NODELAY | MPR_SOCKET_THREAD;
    if (endpoint->multiple) {
        flags |= MPR_SOCKET_REUSE_PORT;
    }
    if (mprListenOnSocket(endpoint->sock, endpoint->ip, endpoint->port, flags) == SOCKET_ERROR) {
        if (mprGetError() == EADDRINUSE) {
            mprLog("error http", 0, "Cannot open a socket on %s:%d, socket already bound.",
                *endpoint->ip ? endpoint->ip : "*", endpoint->port);
        } else {
            mprLog("error http", 0, "Cannot open a socket on %s:%d", *endpoint->ip ? endpoint->ip : "*", endpoint->port);
        }
        return MPR_ERR_CANT_OPEN;
    }
    if (endpoint->http->listenCallback && (endpoint->http->listenCallback)(endpoint) < 0) {
        return MPR_ERR_CANT_OPEN;
    }
    if (endpoint->async && !endpoint->sock->handler) {
        mprAddSocketHandler(endpoint->sock, MPR_SOCKET_READABLE, endpoint->dispatcher, acceptNet, endpoint,
            (endpoint->dispatcher ? 0 : MPR_WAIT_NEW_DISPATCHER) | MPR_WAIT_IMMEDIATE);
    } else {
        mprSetSocketBlockingMode(endpoint->sock, 1);
    }
    proto = endpoint->ssl ? "HTTPS" : "HTTP";
    ip = *endpoint->ip ? endpoint->ip : "*";
    if (mprIsSocketV6(endpoint->sock)) {
        mprLog("info http", HTTP->startLevel, "Started %s service on [%s]:%d", proto, ip, endpoint->port);
    } else {
        mprLog("info http", HTTP->startLevel, "Started %s service on %s:%d", proto, ip, endpoint->port);
    }
    return 0;
}


PUBLIC void httpStopEndpoint(HttpEndpoint *endpoint)
{
    HttpHost    *host;
    int         next;

    for (ITERATE_ITEMS(endpoint->hosts, host, next)) {
        httpStopHost(host);
    }
    if (endpoint->sock) {
        mprCloseSocket(endpoint->sock, 0);
        endpoint->sock = 0;
    }
}


/*
    This routine runs using the service event thread. It accepts the socket and creates an event on a new dispatcher to
    manage the connection. When it returns, it immediately can listen for new connections without having to modify the
    event listen masks.
 */
static void acceptNet(HttpEndpoint *endpoint)
{
    MprDispatcher   *dispatcher;
    MprSocket       *sock;
    MprWaitHandler  *wp;

    if ((sock = mprAcceptSocket(endpoint->sock)) == 0) {
        return;
    }
    if (mprShouldDenyNewRequests()) {
        mprCloseSocket(sock, 0);
        return;
    }
    wp = endpoint->sock->handler;
    if (wp->flags & MPR_WAIT_NEW_DISPATCHER) {
        dispatcher = mprCreateDispatcher("IO", MPR_DISPATCHER_AUTO);
    } else if (wp->dispatcher) {
        dispatcher = wp->dispatcher;
    } else {
        dispatcher = mprGetDispatcher();
    }
    /*
        Optimization to wake the event service in this amount of time. This ensures that when the HttpTimer is scheduled,
        it won't need to awaken the notifier.
     */
    mprSetEventServiceSleep(HTTP_TIMER_PERIOD);

    mprCreateIOEvent(dispatcher, httpAccept, endpoint, wp, sock);
}


PUBLIC HttpHost *httpMatchHost(HttpNet *net, cchar *hostname)
{
    return httpLookupHostOnEndpoint(net->endpoint, hostname);
}


PUBLIC MprSsl *httpMatchSsl(MprSocket *sp, cchar *hostname)
{
    HttpNet     *net;
    HttpHost    *host;

    assert(sp && sp->data);
    net = sp->data;

    if ((host = httpMatchHost(net, hostname)) == 0) {
        return 0;
    }
    return host->defaultRoute->ssl;
}


PUBLIC void *httpGetEndpointContext(HttpEndpoint *endpoint)
{
    assert(endpoint);
    if (endpoint) {
        return endpoint->context;
    }
    return 0;
}


PUBLIC int httpIsEndpointAsync(HttpEndpoint *endpoint)
{
    assert(endpoint);
    if (endpoint) {
        return endpoint->async;
    }
    return 0;
}


PUBLIC int httpSetEndpointAddress(HttpEndpoint *endpoint, cchar *ip, int port)
{
    assert(endpoint);

    if (ip) {
        endpoint->ip = sclone(ip);
    }
    if (port >= 0) {
        endpoint->port = port;
    }
    if (endpoint->sock) {
        httpStopEndpoint(endpoint);
        if (httpStartEndpoint(endpoint) < 0) {
            return MPR_ERR_CANT_OPEN;
        }
    }
    return 0;
}


PUBLIC void httpSetEndpointAsync(HttpEndpoint *endpoint, int async)
{
    if (endpoint->sock) {
        if (endpoint->async && !async) {
            mprSetSocketBlockingMode(endpoint->sock, 1);
        }
        if (!endpoint->async && async) {
            mprSetSocketBlockingMode(endpoint->sock, 0);
        }
    }
    endpoint->async = async;
}


PUBLIC void httpSetEndpointContext(HttpEndpoint *endpoint, void *context)
{
    assert(endpoint);
    endpoint->context = context;
}


PUBLIC void httpSetEndpointNotifier(HttpEndpoint *endpoint, HttpNotifier notifier)
{
    assert(endpoint);
    endpoint->notifier = notifier;
}


PUBLIC int httpSecureEndpoint(HttpEndpoint *endpoint, struct MprSsl *ssl)
{
#if ME_COM_SSL
    endpoint->ssl = ssl;
    mprSetSslMatch(ssl, httpMatchSsl);
#if ME_HTTP_HTTP2
    if (HTTP->http2) {
        mprSetSslAlpn(ssl, "h2 http/1.1");
    }
#endif
    return 0;
#else
    mprLog("error http", 0, "Configuration lacks SSL support");
    return MPR_ERR_BAD_STATE;
#endif
}


PUBLIC int httpSecureEndpointByName(cchar *name, struct MprSsl *ssl)
{
    HttpEndpoint    *endpoint;
    cchar           *ip;
    int             port, next, count;

    if (mprParseSocketAddress(name, &ip, &port, NULL, -1) < 0) {
        mprLog("error http", 0, "Bad endpoint address: %s", name);
        return MPR_ERR_BAD_ARGS;
    }
    if (ip == 0) {
        ip = "";
    }
    for (count = 0, next = 0; (endpoint = mprGetNextItem(HTTP->endpoints, &next)) != 0; ) {
        if (endpoint->port <= 0 || port <= 0 || endpoint->port == port) {
            assert(endpoint->ip);
            if (*endpoint->ip == '\0' || *ip == '\0' || scmp(endpoint->ip, ip) == 0) {
                httpSecureEndpoint(endpoint, ssl);
                count++;
            }
        }
    }
    return (count == 0) ? MPR_ERR_CANT_FIND : 0;
}


PUBLIC void httpAddHostToEndpoint(HttpEndpoint *endpoint, HttpHost *host)
{
    if (mprLookupItem(endpoint->hosts, host) < 0) {
        mprAddItem(endpoint->hosts, host);
        host->flags |= HTTP_HOST_ATTACHED;
    }
    if (endpoint->limits == 0) {
        endpoint->limits = host->defaultRoute->limits;
    }
}


PUBLIC HttpHost *httpLookupHostOnEndpoint(HttpEndpoint *endpoint, cchar *name)
{
    HttpHost    *host;
    int         matches[ME_MAX_ROUTE_MATCHES * 2], next;

    if (!endpoint) {
        return 0;
    }
    for (next = 0; (host = mprGetNextItem(endpoint->hosts, &next)) != 0; ) {
        if (host->hostname == 0 || *host->hostname == 0 || name == 0 || *name == 0) {
            return host;
        }
        if (smatch(name, host->hostname)) {
            return host;
        }
        if (host->flags & HTTP_HOST_WILD_STARTS) {
            if (sstarts(name, host->hostname)) {
                return host;
            }
        } else if (host->flags & HTTP_HOST_WILD_CONTAINS) {
            if (scontains(name, host->hostname)) {
                return host;
            }
        } else if (host->flags & HTTP_HOST_WILD_REGEXP) {
            if (pcre_exec(host->nameCompiled, NULL, name, (int) slen(name), 0, 0, matches,
                    sizeof(matches) / sizeof(int)) >= 1) {
                return host;
            }
        }
    }
    return 0;
}


PUBLIC void httpSetInfoLevel(int level)
{
    HTTP->startLevel = level;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/error.c ************/

/*
    error.c -- Http error handling
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/********************************** Forwards **********************************/

static void errorv(HttpStream *stream, int flags, cchar *fmt, va_list args);
static cchar *formatErrorv(HttpStream *stream, int status, cchar *fmt, va_list args);

/*********************************** Code *************************************/

PUBLIC void httpNetError(HttpNet *net, cchar *fmt, ...)
{
    va_list     args;
    HttpStream  *stream;
    cchar       *msg;
    int         next;

    if (net == 0 || fmt == 0) {
        return;
    }
    va_start(args, fmt);
    if (!net->error) {
        httpSetNetError(net);
        net->errorMsg = msg = sfmtv(fmt, args);
#if ME_HTTP_HTTP2
        if (net->protocol >= 2 && !net->eof) {
            httpSendGoAway(net, HTTP2_INTERNAL_ERROR, "%s", msg);
        }
#endif
        if (httpIsServer(net)) {
            for (ITERATE_ITEMS(net->streams, stream, next)) {
                httpFinalizeConnector(stream);
                httpError(stream, HTTP_ABORT | HTTP_CODE_COMMS_ERROR, "%s", msg);
            }
            httpMonitorNetEvent(net, HTTP_COUNTER_BAD_REQUEST_ERRORS, 1);
        }
    }
    va_end(args);
}


PUBLIC void httpBadRequestError(HttpStream *stream, int flags, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    if (httpServerStream(stream)) {
        httpMonitorEvent(stream, HTTP_COUNTER_BAD_REQUEST_ERRORS, 1);
    }
    errorv(stream, flags, fmt, args);
    va_end(args);
}


PUBLIC void httpLimitError(HttpStream *stream, int flags, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    if (httpServerStream(stream)) {
        httpMonitorEvent(stream, HTTP_COUNTER_LIMIT_ERRORS, 1);
    }
    errorv(stream, flags, fmt, args);
    va_end(args);
}


PUBLIC void httpError(HttpStream *stream, int flags, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    errorv(stream, flags, fmt, args);
    va_end(args);
}


static void errorRedirect(HttpStream *stream, cchar *uri)
{
    HttpTx      *tx;

    tx = stream->tx;
    if (sstarts(uri, "http") || (tx->flags & HTTP_TX_HEADERS_CREATED)) {
        httpRedirect(stream, HTTP_CODE_MOVED_PERMANENTLY, uri);
    } else {
        tx->errorDocument = httpLinkAbs(stream, uri);
        //  Cap input and output until prepErrorDoc can reconstitute the request
        tx->finalized = tx->finalizedInput = tx->finalizedOutput = 1;
        httpSetState(stream, HTTP_STATE_FINALIZED);
    }
}


static void makeAltBody(HttpStream *stream, int status)
{
    HttpRx      *rx;
    HttpTx      *tx;
    cchar       *statusMsg, *msg;

    rx = stream->rx;
    tx = stream->tx;
    assert(rx && tx);

    statusMsg = httpLookupStatus(status);
    msg = (rx && rx->route && rx->route->flags & HTTP_ROUTE_SHOW_ERRORS) ?  stream->errorMsg : "";
    if (rx && scmp(rx->accept, "text/plain") == 0) {
        tx->altBody = sfmt("Access Error: %d -- %s\r\n%s\r\n", status, statusMsg, msg);
    } else {
        httpSetContentType(stream, "text/html");
        tx->altBody = sfmt("<!DOCTYPE html>\r\n"
            "<head>\r\n"
            "    <title>%s</title>\r\n"
            "    <link rel=\"shortcut icon\" href=\"data:image/x-icon;,\" type=\"image/x-icon\">\r\n"
            "</head>\r\n"
            "<body>\r\n<h2>Access Error: %d -- %s</h2>\r\n<pre>%s</pre>\r\n</body>\r\n</html>\r\n",
            statusMsg, status, statusMsg, mprEscapeHtml(msg));
    }
    tx->length = slen(tx->altBody);
}


/*
    The current request has an error and cannot complete as normal. This call sets the Http response status and
    overrides the normal output with an alternate error message. If the output has alread started (headers sent), then
    the connection MUST be closed so the client can get some indication the request failed.
 */
static void errorv(HttpStream *stream, int flags, cchar *fmt, va_list args)
{
    HttpRx      *rx;
    HttpTx      *tx;
    cchar       *uri;
    int         redirected, status;

    if (stream == 0 || fmt == 0) {
        return;
    }
    rx = stream->rx;
    tx = stream->tx;

    status = flags & HTTP_CODE_MASK;
    if (status == 0) {
        status = HTTP_CODE_INTERNAL_SERVER_ERROR;
    }
    if (flags & (HTTP_ABORT | HTTP_CLOSE)) {
        stream->keepAliveCount = 0;
        httpSetEof(stream);
    }
    if (!stream->error) {
        stream->error = 1;
        httpOmitBody(stream);
        stream->errorMsg = formatErrorv(stream, status, fmt, args);
        httpLog(stream->trace, "error", "error", "msg:%s", stream->errorMsg);
        HTTP_NOTIFY(stream, HTTP_EVENT_ERROR, 0);

        if (httpServerStream(stream)) {
            if (status == HTTP_CODE_NOT_FOUND) {
                httpMonitorEvent(stream, HTTP_COUNTER_NOT_FOUND_ERRORS, 1);
            }
            httpMonitorEvent(stream, HTTP_COUNTER_ERRORS, 1);
        }
        httpSetHeaderString(stream, "Cache-Control", "no-cache");

        if (httpServerStream(stream) && tx && rx) {
            if (tx->flags & HTTP_TX_HEADERS_CREATED) {
                /*
                    If the response headers have been sent, must let the other side of the failure ... aborting
                    the request is the only way as the status has been sent.
                 */
                flags |= HTTP_ABORT;
            } else {
                redirected = 0;
                if (rx->route) {
                    if ((uri = httpLookupRouteErrorDocument(rx->route, tx->status)) != 0) {
                        uri = httpExpandVars(stream, uri);
                    }
                    if (rx->route->callback) {
                        rx->route->callback(stream, HTTP_ROUTE_HOOK_ERROR, &uri);
                    }
                    if (uri && !smatch(uri, rx->uri)) {
                        errorRedirect(stream, uri);
                        redirected = 1;
                    }
                }
                if (!redirected) {
                    makeAltBody(stream, status);
                }
            }
        }
        if (flags & HTTP_ABORT) {
            stream->disconnect = 1;
            httpDiscardData(stream, HTTP_QUEUE_RX);
        }
        if (flags & (HTTP_ABORT | HTTP_CLOSE)) {
            httpFinalize(stream);
        } else {
            //  Still need to consume the input
            httpFinalizeOutput(stream);
        }
    }
    if (stream->disconnect && stream->net->protocol < 2) {
        httpDisconnectStream(stream);
    }
}


/*
    Just format stream->errorMsg and set status - nothing more
    NOTE: this is an internal API. Users should use httpError()
 */
static cchar *formatErrorv(HttpStream *stream, int status, cchar *fmt, va_list args)
{
    if (stream->errorMsg == 0) {
        stream->errorMsg = sfmtv(fmt, args);
        if (status) {
            if (status < 0) {
                status = HTTP_CODE_INTERNAL_SERVER_ERROR;
            }
            if (httpServerStream(stream) && stream->tx) {
                stream->tx->status = status;
            } else if (stream->rx) {
                stream->rx->status = status;
            }
        }
    }
    return stream->errorMsg;
}


PUBLIC cchar *httpGetError(HttpStream *stream)
{
    if (stream->errorMsg) {
        return stream->errorMsg;
    } else if (stream->state >= HTTP_STATE_FIRST) {
        return httpLookupStatus(stream->rx->status);
    } else {
        return "";
    }
}


PUBLIC void httpMemoryError(HttpStream *stream)
{
    httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Memory allocation error");
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/fileHandler.c ************/

/*
    fileHandler.c -- Static file content handler

    This handler manages static file based content such as HTML, GIF /or JPEG pages. It supports all methods including:
    GET, PUT, DELETE, OPTIONS and TRACE. It is event based and does not use worker threads.

    The fileHandler also manages requests for directories that require redirection to an index or responding with
    a directory listing.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/***************************** Forward Declarations ***************************/

static void closeFileHandler(HttpQueue *q);
static void handleDeleteRequest(HttpQueue *q);
static void incomingFile(HttpQueue *q, HttpPacket *packet);
static int openFileHandler(HttpQueue *q);
static void outgoingFileService(HttpQueue *q);
static ssize readFileData(HttpQueue *q, HttpPacket *packet, MprOff pos, ssize size);
static void readyFileHandler(HttpQueue *q);
static int rewriteFileHandler(HttpStream *stream);
static void startFileHandler(HttpQueue *q);
static void startPutRequest(HttpQueue *q);

/*********************************** Code *************************************/
/*
    Loadable module initialization
 */
PUBLIC int httpOpenFileHandler()
{
    HttpStage     *handler;

    /*
        This handler serves requests without using thread workers.
     */
    if ((handler = httpCreateHandler("fileHandler", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    handler->rewrite = rewriteFileHandler;
    handler->open = openFileHandler;
    handler->close = closeFileHandler;
    handler->start = startFileHandler;
    handler->ready = readyFileHandler;
    handler->outgoingService = outgoingFileService;
    handler->incoming = incomingFile;
    HTTP->fileHandler = handler;
    return 0;
}

/*
    Rewrite the request for directories, indexes and compressed content.
 */
static int rewriteFileHandler(HttpStream *stream)
{
    HttpRx      *rx;
    HttpTx      *tx;
    MprPath     *info;

    rx = stream->rx;
    tx = stream->tx;
    info = &tx->fileInfo;

    httpMapFile(stream);
    assert(info->checked);

    if (rx->flags & (HTTP_DELETE | HTTP_PUT)) {
        return HTTP_ROUTE_OK;
    }
    if (info->isDir) {
        return httpHandleDirectory(stream);
    }
    if (rx->flags & (HTTP_GET | HTTP_HEAD | HTTP_POST) && info->valid) {
        tx->entityLength = tx->fileInfo.size;
    }
    return HTTP_ROUTE_OK;
}


/*
    This is called after the headers are parsed
 */
static int openFileHandler(HttpQueue *q)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpStream  *stream;
    MprPath     *info;
    char        *date, dbuf[16];
    MprHash     *dateCache;

    stream = q->stream;
    tx = stream->tx;
    rx = stream->rx;
    info = &tx->fileInfo;

    if (stream->error) {
        return MPR_ERR_CANT_OPEN;
    }
    if (rx->flags & (HTTP_GET | HTTP_HEAD | HTTP_POST)) {
        if (!(info->valid || info->isDir)) {
            httpError(stream, HTTP_CODE_NOT_FOUND, "Cannot find document");
            return 0;
        }
        if (!tx->etag) {
            /* Set the etag for caching in the client */
            tx->etag = itos(info->inode + info->size + info->mtime);
        }
        if (info->mtime) {
            dateCache = stream->http->dateCache;
            if ((date = mprLookupKey(dateCache, itosbuf(dbuf, sizeof(dbuf), (int64) info->mtime, 10))) == 0) {
                if (!dateCache || mprGetHashLength(dateCache) > 128) {
                    stream->http->dateCache = dateCache = mprCreateHash(0, 0);
                }
                date = httpGetDateString(&tx->fileInfo);
                mprAddKey(dateCache, itosbuf(dbuf, sizeof(dbuf), (int64) info->mtime, 10), date);
            }
            httpSetHeaderString(stream, "Last-Modified", date);
        }
        if (httpContentNotModified(stream)) {
            httpSetStatus(stream, HTTP_CODE_NOT_MODIFIED);
            httpRemoveHeader(stream, "Content-Encoding");
            httpOmitBody(stream);
            httpFinalizeOutput(stream);
        }
        if (!tx->fileInfo.isReg && !tx->fileInfo.isLink) {
            httpLog(stream->trace, "fileHandler", "error", "msg:Document is not a regular file, filename:%s", tx->filename);
            httpError(stream, HTTP_CODE_NOT_FOUND, "Cannot serve document");

        } else if (tx->fileInfo.size > stream->limits->txBodySize &&
                stream->limits->txBodySize != HTTP_UNLIMITED) {
            httpError(stream, HTTP_ABORT | HTTP_CODE_REQUEST_TOO_LARGE,
                "Http transmission aborted. File size exceeds max body of %lld bytes", stream->limits->txBodySize);

        } else {
            /*
                If using the net connector, open the file if a body must be sent with the response. The file will be
                automatically closed when the request completes.
             */
            if (!(tx->flags & HTTP_TX_NO_BODY)) {
                tx->file = mprOpenFile(tx->filename, O_RDONLY | O_BINARY, 0);
                if (tx->file == 0) {
                    if (rx->referrer && *rx->referrer) {
                        httpLog(stream->trace, "fileHandler", "error", "msg:Cannot open document, filename:%s, referrer:%s",
                            tx->filename, rx->referrer);
                    } else {
                        httpLog(stream->trace, "fileHandler", "error", "msg:Cannot open document, filename:%s", tx->filename);
                    }
                    httpError(stream, HTTP_CODE_NOT_FOUND, "Cannot open document");
                }
            }
        }

    } else if (rx->flags & HTTP_PUT) {
        startPutRequest(q);

    } else if (rx->flags & (HTTP_OPTIONS | HTTP_DELETE)) {
        //  Handle in startFileHandler

    } else {
        httpError(stream, HTTP_CODE_BAD_METHOD, "Unsupported method");
    }
    return 0;
}


/*
    Called when the request is complete
 */
static void closeFileHandler(HttpQueue *q)
{
    HttpTx  *tx;

    tx = q->stream->tx;
    if (tx->file) {
        mprCloseFile(tx->file);
        tx->file = 0;
    }
}


/*
    Called when all the body content has been received, but may not have yet been processed by our incoming()
 */
static void startFileHandler(HttpQueue *q)
{
    HttpStream      *stream;
    HttpTx          *tx;
    HttpRx          *rx;
    HttpPacket      *packet;
    HttpFillProc    fill;

    stream = q->stream;
    tx = stream->tx;
    rx = stream->rx;

    if (rx->flags & HTTP_DELETE) {
        handleDeleteRequest(q);
        httpFinalizeOutput(stream);

    } else if (stream->rx->flags & HTTP_HEAD) {
        tx->length = tx->entityLength;
        httpFinalizeOutput(stream);

    } else if (rx->flags & HTTP_OPTIONS) {
        httpHandleOptions(q->stream);
        httpFinalizeOutput(stream);

    } else if (stream->rx->flags & HTTP_PUT) {
        //  Handled in incoming

    } else if (stream->rx->flags & (HTTP_GET | HTTP_POST)) {
        if ((!(tx->flags & HTTP_TX_NO_BODY)) && (tx->entityLength >= 0 && !stream->error)) {
            /*
                Create a single dummy data packet based to hold the remaining data length and file seek possition.
                This is used to trigger the outgoing file service. It is not transmitted to the client.
             */
#if ME_HTTP_SENDFILE
            if (tx->simplePipeline) {
                httpLog(stream->trace, "fileHandler", "detail", "msg:Using sendfile for %s", tx->filename);
                httpRemoveChunkFilter(stream->txHead);
                fill = NULL;
            } else {
                if (q->net->protocol < 2 && !q->net->secure) {
                    httpLog(stream->trace, "fileHandler", "detail", "msg:Using slower full pipeline for %s", tx->filename);
                }
                fill = readFileData;
            }
#else
            fill = readFileData;
#endif
            packet = httpCreateEntityPacket(0, tx->entityLength, fill);

            /*
                Set the content length if not chunking and not using ranges
             */
            if (!tx->outputRanges && tx->chunkSize < 0) {
                tx->length = tx->entityLength;
            }
            httpPutPacket(q, packet);
        }
    } else {
        httpFinalizeOutput(stream);
    }
}


/*
    The ready callback is invoked when all the input body data has been received
    The queue already contains a single data packet representing all the output data.
 */
static void readyFileHandler(HttpQueue *q)
{
    if (!q->stream->tx->finalized) {
        httpScheduleQueue(q);
    }
}


/*
    The incoming callback is invoked to receive body data
 */
static void incomingFile(HttpQueue *q, HttpPacket *packet)
{
    HttpStream  *stream;
    HttpTx      *tx;
    HttpRx      *rx;
    HttpRange   *range;
    MprBuf      *buf;
    MprFile     *file;
    ssize       len;

    stream = q->stream;
    tx = stream->tx;
    rx = stream->rx;
    file = (MprFile*) q->queueData;

    if (packet->flags & HTTP_PACKET_END) {
        /* End of input */
        if (file) {
            mprCloseFile(file);
            q->queueData = 0;
        }
        if (!tx->etag) {
            /* Set the etag for caching in the client */
            mprGetPathInfo(tx->filename, &tx->fileInfo);
            tx->etag = itos(tx->fileInfo.inode + tx->fileInfo.size + tx->fileInfo.mtime);
        }
        httpFinalizeInput(stream);

        if (rx->flags & HTTP_PUT) {
            httpFinalizeOutput(stream);
        }

    } else if (file) {
        buf = packet->content;
        len = mprGetBufLength(buf);
        if (len > 0) {
            range = rx->inputRange;
            if (range && mprSeekFile(file, SEEK_SET, range->start) != range->start) {
                httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot seek to range start to %lld", range->start);

            } else if (mprWriteFile(file, mprGetBufStart(buf), len) != len) {
                httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot PUT to %s", tx->filename);
            }
        }
    }
}


/*
    The service callback will be invoked to service outgoing packets on the service queue. It will only be called
    once all incoming data has been received and then when the downstream queues drain sufficiently to absorb
    more data. This routine may flow control if the downstream stage cannot accept all the file data. It will
    then be re-called as required to send more data.
 */
static void outgoingFileService(HttpQueue *q)
{
    HttpStream  *stream;
    HttpRx      *rx;
    HttpTx      *tx;
    HttpPacket  *data, *packet;
    ssize       size, nbytes;

    stream = q->stream;
    rx = stream->rx;
    tx = stream->tx;

    /*
        The queue will contain a dummy entity packet which holds the position from which to read in the file.
        If the downstream queue is full, the data packet will be put onto the queue ahead of the entity packet.
        When EOF, and END packet will be added to the queue via httpFinalizeOutput which will then be sent.
     */
    for (packet = q->first; packet; packet = q->first) {
        if (packet->fill) {
            size = min(packet->esize, q->packetSize);
            size = min(size, q->nextQ->packetSize);
            if (size > 0) {
                data = httpCreateDataPacket(size);
                if ((nbytes = readFileData(q, data, tx->filePos, size)) < 0) {
                    httpError(stream, HTTP_CODE_NOT_FOUND, "Cannot read document");
                    return;
                }
                tx->filePos += nbytes;
                packet->epos += nbytes;
                packet->esize -= nbytes;
                if (packet->esize == 0) {
                    httpGetPacket(q);
                }
                /*
                    This may split the packet and put back the tail portion ahead of the just putback entity packet.
                 */
                if (!httpWillNextQueueAcceptPacket(q, data)) {
                    httpPutBackPacket(q, data);
                    return;
                }
                httpPutPacketToNext(q, data);
            } else {
                httpGetPacket(q);
            }
        } else {
            // Don't flow control as the packet is already consuming memory
            packet = httpGetPacket(q);
            httpPutPacketToNext(q, packet);
        }
        if (!tx->finalizedOutput && !q->first) {
            /*
                Optimize FinalizeOutput by putting the end packet manually. The helps the HTTP/2 protocol
                can see the END_DATA packet now and set an END_DATA flag on the last data frame rather than
                waiting for this outgoing to be rescheduled later.
             */
            tx->putEndPacket = 1;
            httpPutPacketToNext(q, httpCreateEndPacket());
            httpFinalizeOutput(stream);
        }
    }
    if (!tx->finalized && tx->finalizedOutput && tx->finalizedInput) {
        httpFinalize(stream);
    }
}


/*
    Populate a packet with file data. Return the number of bytes read or a negative error code.
 */
static ssize readFileData(HttpQueue *q, HttpPacket *packet, MprOff pos, ssize size)
{
    HttpStream  *stream;
    HttpTx      *tx;
    ssize       nbytes;

    stream = q->stream;
    tx = stream->tx;

    if (size <= 0) {
        return 0;
    }
    if (mprGetBufSpace(packet->content) < size) {
        size = mprGetBufSpace(packet->content);
    }
    if (pos >= 0) {
        mprSeekFile(tx->file, SEEK_SET, pos);
    }
    if ((nbytes = mprReadFile(tx->file, mprGetBufStart(packet->content), size)) != size) {
        /*
            As we may have sent some data already to the client, the only thing we can do is abort and hope the client
            notices the short data.
         */
        httpError(stream, HTTP_CODE_SERVICE_UNAVAILABLE, "Cannot read file %s", tx->filename);
        return MPR_ERR_CANT_READ;
    }
    mprAdjustBufEnd(packet->content, nbytes);
    return nbytes;
}


/*
    This is called to setup for a HTTP PUT request. It is called before receiving the post data via incomingFileData
 */
static void startPutRequest(HttpQueue *q)
{
    HttpStream  *stream;
    HttpTx      *tx;
    MprFile     *file;
    cchar       *path;

    assert(q->queueData == 0);

    stream = q->stream;
    tx = stream->tx;
    assert(tx->filename);
    assert(tx->fileInfo.checked);

    path = tx->filename;
    if (tx->outputRanges) {
        /*
            Open an existing file with fall-back to create
         */
        if ((file = mprOpenFile(path, O_BINARY | O_WRONLY, 0644)) == 0) {
            if ((file = mprOpenFile(path, O_CREAT | O_TRUNC | O_BINARY | O_WRONLY, 0644)) == 0) {
                httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot create the put URI");
                return;
            }
        } else {
            mprSeekFile(file, SEEK_SET, 0);
        }
    } else {
        if ((file = mprOpenFile(path, O_CREAT | O_TRUNC | O_BINARY | O_WRONLY, 0644)) == 0) {
            httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot create the put URI");
            return;
        }
    }
    if (!tx->fileInfo.isReg) {
        httpSetHeaderString(stream, "Location", stream->rx->uri);
    }
    /*
        These are both success returns. 204 means already existed.
     */
    httpSetStatus(stream, tx->fileInfo.isReg ? HTTP_CODE_NO_CONTENT : HTTP_CODE_CREATED);
    q->queueData = (void*) file;
}


static void handleDeleteRequest(HttpQueue *q)
{
    HttpStream  *stream;
    HttpTx      *tx;

    stream = q->stream;
    tx = stream->tx;
    assert(tx->filename);
    assert(tx->fileInfo.checked);

    if (!tx->fileInfo.isReg) {
        httpError(stream, HTTP_CODE_NOT_FOUND, "Document not found");
        return;
    }
    if (mprDeletePath(tx->filename) < 0) {
        httpError(stream, HTTP_CODE_NOT_FOUND, "Cannot remove document");
        return;
    }
    httpSetStatus(stream, HTTP_CODE_NO_CONTENT);
}


PUBLIC int httpHandleDirectory(HttpStream *stream)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpRoute   *route;
    HttpUri     *req;
    cchar       *index, *pathInfo, *path;
    int         next;

    rx = stream->rx;
    tx = stream->tx;
    req = rx->parsedUri;
    route = rx->route;

    /*
        Manage requests for directories
     */
    if (!sends(req->path, "/")) {
        /*
           Append "/" and do an external redirect. Use the original request URI. Use httpFormatUri to preserve query.
         */
        httpRedirect(stream, HTTP_CODE_MOVED_PERMANENTLY,
            httpFormatUri(0, 0, 0, sjoin(req->path, "/", NULL), req->reference, req->query, 0));
        return HTTP_ROUTE_OK;
    }
    if (route->indexes) {
        /*
            Ends with a "/" so do internal redirection to an index file
         */
        path = 0;
        for (ITERATE_ITEMS(route->indexes, index, next)) {
            /*
                Internal directory redirections. Transparently append index. Test indexes in order.
             */
            path = mprJoinPath(tx->filename, index);
            if (mprPathExists(path, R_OK)) {
                break;
            }
            if (route->map && !(tx->flags & HTTP_TX_NO_MAP)) {
                path = httpMapContent(stream, path);
                if (mprPathExists(path, R_OK)) {
                    break;
                }
            }
            path = 0;
        }
        if (path) {
            pathInfo = sjoin(req->path, index, NULL);
            if (httpSetUri(stream, httpFormatUri(req->scheme, req->host, req->port, pathInfo, req->reference,
                    req->query, 0)) < 0) {
                mprLog("error http", 0, "Cannot handle directory \"%s\"", pathInfo);
                return HTTP_ROUTE_REJECT;
            }
            tx->filename = httpMapContent(stream, path);
            mprGetPathInfo(tx->filename, &tx->fileInfo);
            return HTTP_ROUTE_REROUTE;
        }
    }
#if ME_HTTP_DIR
    /*
        Directory Listing. Test if a directory listing should be rendered. If so, delegate to the dirHandler.
        Must use the netConnector.
     */
    if (httpShouldRenderDirListing(stream)) {
        tx->handler = stream->http->dirHandler;
        tx->connector = stream->http->netConnector;
        return HTTP_ROUTE_OK;
    }
#endif
    return HTTP_ROUTE_OK;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/host.c ************/

/*
    host.c -- Host class for all HTTP hosts

    The Host class is used for the default HTTP server and for all virtual hosts (including SSL hosts).
    Many objects are controlled at the host level. Eg. URL handlers.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/


#include    "pcre.h"

/*********************************** Locals ***********************************/

static HttpHost *defaultHost;

/********************************** Forwards **********************************/

static void manageHost(HttpHost *host, int flags);

/*********************************** Code *************************************/

PUBLIC HttpHost *httpCreateHost()
{
    HttpHost    *host;

    if ((host = mprAllocObj(HttpHost, manageHost)) == 0) {
        return 0;
    }
    if ((host->responseCache = mprCreateCache(MPR_CACHE_SHARED)) == 0) {
        return 0;
    }
    mprSetCacheLimits(host->responseCache, 0, ME_MAX_CACHE_DURATION, 0, 0);

    host->routes = mprCreateList(-1, MPR_LIST_STABLE);
    host->flags = HTTP_HOST_NO_TRACE;
    host->streaming = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_STABLE);
    httpSetStreaming(host, "application/x-www-form-urlencoded", NULL, 0);
    httpSetStreaming(host, "application/json", NULL, 0);
    httpSetStreaming(host, "application/csp-report", NULL, 0);
    httpSetStreaming(host, "multipart/form-data", NULL, 0);
    httpAddHost(host);
    return host;
}


PUBLIC HttpHost *httpCloneHost(HttpHost *parent)
{
    HttpHost    *host;

    if ((host = httpCreateHost()) == 0) {
        return 0;
    }
    /*
        The dirs and routes are all copy-on-write.
        Do not clone routes, ip, port and name
     */
    host->parent = parent;
    host->flags = parent->flags & HTTP_HOST_NO_TRACE;
    host->streaming = parent->streaming;
    host->routes = mprCreateList(-1, MPR_LIST_STABLE);
    return host;
}


static void manageHost(HttpHost *host, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(host->hostname);
        mprMark(host->name);
        mprMark(host->parent);
        mprMark(host->responseCache);
        mprMark(host->routes);
        mprMark(host->defaultRoute);
        mprMark(host->defaultEndpoint);
        mprMark(host->secureEndpoint);
        mprMark(host->streaming);

    } else if (flags & MPR_MANAGE_FREE) {
        if (host->nameCompiled) {
            free(host->nameCompiled);
        }
    }
}


PUBLIC HttpHost *httpCreateDefaultHost()
{
    HttpHost    *host;
    HttpRoute   *route;

    if (defaultHost) {
        return defaultHost;
    }
    defaultHost = host = httpCreateHost();
    route = httpCreateRoute(host);
    httpSetHostDefaultRoute(host, route);
    route->limits = route->http->serverLimits;
    return host;
}


PUBLIC int httpStartHost(HttpHost *host)
{
    HttpRoute   *route;
    int         next;

    for (ITERATE_ITEMS(host->routes, route, next)) {
        httpStartRoute(route);
    }
    for (ITERATE_ITEMS(host->routes, route, next)) {
        if (!route->trace && route->parent && route->parent->trace) {
            route->trace = route->parent->trace;
        }
    }
    return 0;
}


PUBLIC void httpStopHost(HttpHost *host)
{
    HttpRoute   *route;
    int         next;

    for (ITERATE_ITEMS(host->routes, route, next)) {
        httpStopRoute(route);
    }
}


PUBLIC HttpRoute *httpGetHostDefaultRoute(HttpHost *host)
{
    return host->defaultRoute;
}


static void printRouteHeader(HttpHost *host, int *methodsLen, int *patternLen, int *targetLen)
{
    HttpRoute   *route;
    int         next;

    *methodsLen = (int) slen("Methods");
    *patternLen = (int) slen("Route");
    *targetLen = (int) slen("$&");

    for (next = 0; (route = mprGetNextItem(host->routes, &next)) != 0; ) {
        *targetLen = (int) max(*targetLen, slen(route->target));
        *patternLen = (int) max(*patternLen, slen(route->pattern));
        *methodsLen = (int) max(*methodsLen, slen(httpGetRouteMethods(route)));
    }
    printf("\nRoutes for host: %s\n\n", host->name ? host->name : "default");
    printf("%-*s %-*s %-*s\n", *patternLen, "Route", *methodsLen, "Methods", *targetLen, "Target");
    printf("%-*s %-*s %-*s\n", *patternLen, "-----", *methodsLen, "-------", *targetLen, "------");
}


static void printRoute(HttpRoute *route, int idx, bool full, int methodsLen, int patternLen, int targetLen)
{
    HttpRouteOp *condition;
    HttpStage   *handler;
    HttpAuth    *auth;
    MprKey      *kp;
    cchar       *methods, *pattern, *target, *index;
    int         nextIndex;

    auth = route->auth;
    methods = httpGetRouteMethods(route);
    methods = methods ? methods : "*";
    pattern = (route->pattern && *route->pattern) ? route->pattern : "^.*$";
    target = (route->target && *route->target) ? route->target : "$&";

    if (full) {
        printf("\nRoutes for host: %s\n", route->host->name ? route->host->name : "default");
        printf("\n  Route [%d]. %s\n", idx, pattern);
        if (route->prefix && *route->prefix) {
            printf("    Prefix:       %s\n", route->prefix);
        }
        printf("    RegExp:       %s\n", route->optimizedPattern ? route->optimizedPattern : "");
        printf("    Methods:      %s\n", methods);
        printf("    Target:       %s\n", target);
        printf("    Auth:         %s\n", auth->type ? auth->type->name : "-");
        printf("    Home:         %s\n", route->home);
        printf("    Documents:    %s\n", route->documents);
        if (route->sourceName) {
            printf("    Source:       %s\n", route->sourceName);
        }
        if (route->tplate) {
            printf("    Template:     %s\n", route->tplate);
        }
        if (route->indexes) {
            for (ITERATE_ITEMS(route->indexes, index, nextIndex)) {
                printf("    Indexes:      %s \n", index);
            }
        }
        if (route->conditions) {
            for (nextIndex = 0; (condition = mprGetNextItem(route->conditions, &nextIndex)) != 0; ) {
                printf("    Condition:    %s %s\n", condition->name, condition->details ? condition->details : "");
            }
        }
        if (route->handler) {
            printf("    Handler:      %s\n", route->handler->name);
        }
        if (route->extensions) {
            for (ITERATE_KEYS(route->extensions, kp)) {
                handler = (HttpStage*) kp->data;
                printf("    Extension:    \"%s\" => %s\n", kp->key, handler->name);
            }
        }
        if (route->handlers) {
            for (ITERATE_ITEMS(route->handlers, handler, nextIndex)) {
                printf("    Handler:      %s\n", handler->name);
            }
        }
    } else {
        printf("%-*s %-*s %-*s\n", patternLen, pattern, methodsLen, methods, targetLen, target);
    }
}


PUBLIC void httpLogRoutes(HttpHost *host, bool full)
{
    HttpRoute   *route;
    int         index, methodsLen, patternLen, targetLen;

    if (!host) {
        host = httpGetDefaultHost();
    }
    if (mprGetListLength(host->routes) == 0 && !host->defaultRoute) {
        printf("\nRoutes for host: %s: none\n", host->name ? host->name : "default");
    } else {
        methodsLen = patternLen = targetLen = 0;
        if (!full) {
            printRouteHeader(host, &methodsLen, &patternLen, &targetLen);
        }
        for (index = 0; (route = mprGetNextItem(host->routes, &index)) != 0; ) {
            printRoute(route, index - 1, full, methodsLen, patternLen, targetLen);
        }
        if (mprLookupItem(host->routes, host->defaultRoute) < 0) {
            printRoute(host->defaultRoute, index, full, methodsLen, patternLen, targetLen);
        }
    }
    printf("\n");
}


PUBLIC int httpSetHostName(HttpHost *host, cchar *name)
{
    cchar   *errMsg;
    char    *cp;
    int     column;

    if (!name || *name == '\0') {
        mprLog("error http", 0, "Empty host name");
        return MPR_ERR_BAD_ARGS;
    }
    host->name = sclone(name);
    host->hostname = strim(name, "/*", MPR_TRIM_BOTH);
    if ((cp = schr(host->hostname, ':')) != 0) {
        host->hostname = ssplit((char*) host->hostname, ":", NULL);
    }
    host->flags &= ~(HTTP_HOST_WILD_STARTS | HTTP_HOST_WILD_CONTAINS | HTTP_HOST_WILD_REGEXP);
    if (sends(name, "*")) {
        host->flags |= HTTP_HOST_WILD_STARTS;

    } else if (*name == '*') {
        host->flags |= HTTP_HOST_WILD_CONTAINS;

    } else if (*name == '/') {
        host->flags |= HTTP_HOST_WILD_REGEXP;
        if (host->nameCompiled) {
            free(host->nameCompiled);
        }
        if ((host->nameCompiled = pcre_compile2(host->hostname, 0, 0, &errMsg, &column, NULL)) == 0) {
            mprLog("error http route", 0, "Cannot compile condition match pattern. Error %s at column %d", errMsg, column);
            return MPR_ERR_BAD_SYNTAX;
        }
    }
    return 0;
}


PUBLIC int httpAddRoute(HttpHost *host, HttpRoute *route)
{
    HttpRoute   *prev, *item, *lastRoute;
    int         i, thisRoute;

    assert(route);

    if (host->parent && host->routes == host->parent->routes) {
        host->routes = mprCloneList(host->parent->routes);
    }
    if (mprLookupItem(host->routes, route) < 0) {
        if (route->pattern[0] && (lastRoute = mprGetLastItem(host->routes)) && lastRoute->pattern[0] == '\0') {
            /*
                Insert non-default route before last default route
             */
            thisRoute = mprInsertItemAtPos(host->routes, mprGetListLength(host->routes) - 1, route);
        } else {
            thisRoute = mprAddItem(host->routes, route);
        }
        if (thisRoute > 0) {
            prev = mprGetItem(host->routes, thisRoute - 1);
            if (!smatch(prev->startSegment, route->startSegment)) {
                prev->nextGroup = thisRoute;
                for (i = thisRoute - 2; i >= 0; i--) {
                    item = mprGetItem(host->routes, i);
                    if (smatch(item->startSegment, prev->startSegment)) {
                        item->nextGroup = thisRoute;
                    } else {
                        break;
                    }
                }
            }
        }
    }
    httpSetRouteHost(route, host);
    return 0;
}


PUBLIC HttpRoute *httpLookupRoute(HttpHost *host, cchar *pattern)
{
    HttpRoute   *route;
    int         next;

    if (smatch(pattern, "default")) {
        pattern = "";
    }
    if (smatch(pattern, "/") || smatch(pattern, "^/") || smatch(pattern, "^/$")) {
        pattern = "";
    }
    if (!host && (host = httpGetDefaultHost()) == 0) {
        return 0;
    }
    for (next = 0; (route = mprGetNextItem(host->routes, &next)) != 0; ) {
        assert(route->pattern);
        if (smatch(route->pattern, pattern)) {
            return route;
        }
    }
    return 0;
}


PUBLIC void httpResetRoutes(HttpHost *host)
{
    host->routes = mprCreateList(-1, MPR_LIST_STABLE);
}


PUBLIC void httpSetHostDefaultRoute(HttpHost *host, HttpRoute *route)
{
    host->defaultRoute = route;
}


PUBLIC void httpSetDefaultHost(HttpHost *host)
{
    defaultHost = host;
}


PUBLIC void httpSetHostSecureEndpoint(HttpHost *host, HttpEndpoint *endpoint)
{
    host->secureEndpoint = endpoint;
}


PUBLIC void httpSetHostDefaultEndpoint(HttpHost *host, HttpEndpoint *endpoint)
{
    host->defaultEndpoint = endpoint;
}


PUBLIC HttpHost *httpGetDefaultHost()
{
    return defaultHost;
}


PUBLIC HttpRoute *httpGetDefaultRoute(HttpHost *host)
{
    if (host) {
        return host->defaultRoute;
    } else if (defaultHost) {
        return defaultHost->defaultRoute;
    }
    return 0;
}


/*
    Determine if input body content should be streamed or buffered for requests with content of a given mime type.
 */
 PUBLIC bool httpGetStreaming(HttpStream *stream)
 {
     MprKey      *kp;
     HttpRx      *rx;
     cchar       *mime;

     rx = stream->rx;

     mime = rx->mimeType;
     if (schr(mime, ';')) {
         mime = ssplit(sclone(mime), ";", 0);
     }
     if ((kp = mprLookupKeyEntry(stream->host->streaming, mime)) != 0) {
         if (kp->data == NULL || sstarts(rx->uri, kp->data)) {
             /* Type is set to the enable value */
             return kp->type;
         }
     }
     if ((kp = mprLookupKeyEntry(stream->host->streaming, "*")) != 0) {
         if (kp->data == NULL || sstarts(rx->uri, kp->data)) {
             /* Type is set to the enable value */
             return kp->type;
         }
     }
     if ((rx->flags & HTTP_POST) && (rx->form || rx->json)) {
         return 0;
     }
     return 1;
}


/*
    Define streaming to be enabled for a given mime type that starts with the given URI for this host
 */
PUBLIC void httpSetStreaming(HttpHost *host, cchar *mime, cchar *uri, bool enable)
{
    MprKey  *kp;

    assert(host);
    if ((kp = mprAddKey(host->streaming, mime, uri)) != 0) {
        /*
            We store the enable value in the key type to save an allocation
         */
        kp->type = enable;
    }
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/hpack.c ************/

/*
    hpack.c - Http/2 header packing.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



#if ME_HTTP_HTTP2
/*********************************** Code *************************************/

static cchar *staticStrings[] = {
    ":authority", NULL,
    ":method", "GET",
    ":method", "POST",
    ":path", "/",
    ":path", "/index.html",
    ":scheme", "http",
    ":scheme", "https",
    ":status", "200",
    ":status", "204",
    ":status", "206",
    ":status", "304",
    ":status", "400",
    ":status", "404",
    ":status", "500",
    "accept-charset", NULL,
    "accept-encoding", "gzip, deflate",
    "accept-language", NULL,
    "accept-ranges", NULL,
    "accept", NULL,
    "access-control-allow-origin", NULL,
    "age", NULL,
    "allow", NULL,
    "authorization", NULL,
    "cache-control", NULL,
    "content-disposition", NULL,
    "content-encoding", NULL,
    "content-language", NULL,
    "content-length", NULL,
    "content-location", NULL,
    "content-range", NULL,
    "content-type", NULL,
    "cookie", NULL,
    "date", NULL,
    "etag", NULL,
    "expect", NULL,
    "expires", NULL,
    "from", NULL,
    "host", NULL,
    "if-match", NULL,
    "if-modified-since", NULL,
    "if-none-match", NULL,
    "if-range", NULL,
    "if-unmodified-since", NULL,
    "last-modified", NULL,
    "link", NULL,
    "location", NULL,
    "max-forwards", NULL,
    "proxy-authenticate", NULL,
    "proxy-authorization", NULL,
    "range", NULL,
    "referer", NULL,
    "refresh", NULL,
    "retry-after", NULL,
    "server", NULL,
    "set-cookie", NULL,
    "strict-transport-security", NULL,
    "transfer-encoding", NULL,
    "user-agent", NULL,
    "vary", NULL,
    "via", NULL,
    "www-authenticate", NULL,
    NULL, NULL
};

#define HTTP2_STATIC_TABLE_ENTRIES ((sizeof(staticStrings) / sizeof(char*) / 2) - 1)

/*********************************** Code *************************************/

PUBLIC void httpCreatePackedHeaders()
{
    cchar   **cp;

    /*
        Create the static table of common headers
     */
    HTTP->staticHeaders = mprCreateList(HTTP2_STATIC_TABLE_ENTRIES, 0);
    for (cp = staticStrings; *cp; cp += 2) {
        mprAddItem(HTTP->staticHeaders, mprCreateKeyPair(cp[0], cp[1], 0));
    }
}


/*
    Lookup a key/value in the HPACK header table.
    Look in the dynamic list first as it will contain most of the headers with values.
    Set *withValue if the value matches as well as the name.
    The dynamic table uses indexes after the static table.
 */
PUBLIC int httpLookupPackedHeader(HttpHeaderTable *headers, cchar *key, cchar *value, bool *withValue)
{
    MprKeyValue     *kp;
    int             next, onext;

    assert(headers);
    assert(key && *key);
    assert(value && *value);

    *withValue = 0;

    /*
        Prefer dynamic table as we can encode more values
     */
    for (ITERATE_ITEMS(headers->list, kp, next)) {
        if (strcmp(key, kp->key) == 0) {
            onext = next;
            do {
                if (smatch(kp->value, value) && smatch(kp->key, key)) {
                    *withValue = 1;
                    return next + HTTP2_STATIC_TABLE_ENTRIES;
                }
            } while ((kp = mprGetNextItem(headers->list, &next)) != 0);
            return onext + HTTP2_STATIC_TABLE_ENTRIES;
        }
    }

    for (ITERATE_ITEMS(HTTP->staticHeaders, kp, next)) {
        if (smatch(key, kp->key)) {
            if (value && kp->value) {
                onext = next;
                do {
                    if (smatch(kp->value, value) && smatch(kp->key, key)) {
                        *withValue = 1;
                        return next;
                    }
                } while ((kp = mprGetNextItem(HTTP->staticHeaders, &next)) != 0);
                return onext;
            } else {
                return next;
            }
        }
    }
    return 0;
}


/*
    Add a header to the dynamic table.
 */
PUBLIC int httpAddPackedHeader(HttpHeaderTable *headers, cchar *key, cchar *value)
{
    MprKeyValue     *kp;
    ssize           len;
    int             index;

    len = slen(key) + slen(value) + HTTP2_HEADER_OVERHEAD;
    if (len > headers->max) {
        return MPR_ERR_WONT_FIT;
    }
    /*
        Make room for the new entry if required. Evict the oldest entries first.
     */
    while ((headers->size + len) >= headers->max) {
        if (headers->list->length == 0) {
            break;
        }
        kp = mprPopItem(headers->list);
        headers->size -= (slen(kp->key) + slen(kp->value) + HTTP2_HEADER_OVERHEAD);
        if (headers->size < 0) {
            /* Should never happen */
            return MPR_ERR_BAD_STATE;
        }
    }
    assert (headers->size >= 0 && (headers->size + len) < headers->max);

    /*
        New entries are inserted at the start of the table and all existing entries shuffle down
     */
    if ((index = mprInsertItemAtPos(headers->list, 0, mprCreateKeyPair(key, value, 0))) < 0) {
        return MPR_ERR_MEMORY;
    }
    index += 1 + HTTP2_STATIC_TABLE_ENTRIES;
    headers->size += len;
    return index;
}



/*
    Set a new maximum header table size. Evict oldest entries if currently over budget.

    hpack/6.3 says must be equal or lower than the settings value
    generic/5 says must accept multiple updates
    https://github.com/summerwind/h2spec/blob/master/hpack/6_3_dynamic_table_size_update.go
    https://github.com/summerwind/h2spec/blob/master/generic/5_hpack.go
 */
PUBLIC int httpSetPackedHeadersMax(HttpHeaderTable *headers, int max)
{
    MprKeyValue     *kp;

    if (max < 0) {
        return MPR_ERR_BAD_ARGS;
    }
    if (max > HTTP2_TABLE_SIZE) {
        return MPR_ERR_BAD_ARGS;
    }
    headers->max = max;
    while (headers->size >= max) {
        if (headers->list->length == 0) {
            break;
        }
        kp = mprPopItem(headers->list);
        headers->size -= (slen(kp->key) + slen(kp->value) + HTTP2_HEADER_OVERHEAD);
        if (headers->size < 0) {
            /* Should never happen */
            return MPR_ERR_BAD_STATE;
        }
    }
    return 0;
}

/*
    Get a header at a specific index.
 */
PUBLIC MprKeyValue *httpGetPackedHeader(HttpHeaderTable *headers, int index)
{
    if (index <= 0 || index > (1 + HTTP2_STATIC_TABLE_ENTRIES + mprGetListLength(headers->list))) {
        return 0;
    }
    if (--index < HTTP2_STATIC_TABLE_ENTRIES) {
        return mprGetItem(HTTP->staticHeaders, index);
    }
    index = index - HTTP2_STATIC_TABLE_ENTRIES;
    if (index >= mprGetListLength(headers->list)) {
        /* Bad index */
        return 0;
    }
    return mprGetItem(headers->list, index);
}
#endif /* ME_HTTP_HTTP2 */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/http1Filter.c ************/

/*
    http1Filter.c - HTTP/1 protocol handling.

    The HttpFilter is after the TailFilter and before the NetConnector.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/*********************************** Locals ***********************************/

#define TOKEN_HEADER_KEY        0x1     /* Validate token as a header key */
#define TOKEN_HEADER_VALUE      0x2     /* Validate token as a header value */
#define TOKEN_URI               0x4     /* Validate token as a URI value */
#define TOKEN_NUMBER            0x8     /* Validate token as a number */
#define TOKEN_WORD              0x10    /* Validate token as single word with no spaces */
#define TOKEN_LINE              0x20    /* Validate token as line with no newlines */

/********************************** Forwards **********************************/

static cchar *eatBlankLines(HttpPacket *packet);
static HttpStream *findStream1(HttpQueue *q);
static char *getToken(HttpPacket *packet, cchar *delim, int validation);
static bool gotHeaders(HttpQueue *q, HttpPacket *packet);
static void incomingHttp1(HttpQueue *q, HttpPacket *packet);
static void outgoingHttp1(HttpQueue *q, HttpPacket *packet);
static void outgoingHttp1Service(HttpQueue *q);
static void parseFields(HttpQueue *q, HttpPacket *packet);
static bool parseHeaders1(HttpQueue *q, HttpPacket *packet);
static void parseRequestLine(HttpQueue *q, HttpPacket *packet);
static void parseResponseLine(HttpQueue *q, HttpPacket *packet);
static char *validateToken(char *token, char *endToken, int validation);

/*********************************** Code *************************************/
/*
   Loadable module initialization
 */
PUBLIC int httpOpenHttp1Filter()
{
    HttpStage     *filter;

    if ((filter = httpCreateConnector("Http1Filter", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    HTTP->http1Filter = filter;
    filter->incoming = incomingHttp1;
    filter->outgoing = outgoingHttp1;
    filter->outgoingService = outgoingHttp1Service;
    return 0;
}


/*
    The queue is the net->inputq == netHttp-rx
 */
static void incomingHttp1(HttpQueue *q, HttpPacket *packet)
{
    HttpStream  *stream;

    stream = findStream1(q);

    // There will typically be no packets on the queue, so this will be fast.
    httpJoinPacketForService(q, packet, HTTP_DELAY_SERVICE);

    for (packet = httpGetPacket(q); packet && !stream->error; packet = httpGetPacket(q)) {
        if (stream->state < HTTP_STATE_PARSED) {
            if (!parseHeaders1(q, packet)) {
                httpPutBackPacket(q, packet);
                break;
            }
            if (httpGetPacketLength(packet) == 0) {
                packet = 0;
            }
        }
        if (packet) {
            if (!httpWillQueueAcceptPacket(q, stream->inputq, packet)) {
                httpPutBackPacket(q, packet);
                break;
            }
            httpPutPacket(stream->inputq, packet);
        }
    }
    if (stream->error && packet && packet->flags & HTTP_PACKET_END) {
        httpFinalizeInput(stream);
    }
}


static void outgoingHttp1(HttpQueue *q, HttpPacket *packet)
{
    httpPutForService(q, packet, 1);
}


static void outgoingHttp1Service(HttpQueue *q)
{
    HttpStream  *stream;
    HttpPacket  *packet;

    stream = q->stream;
    for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
        if (!httpWillQueueAcceptPacket(q, q->net->socketq, packet)) {
            httpPutBackPacket(q, packet);
            return;
        }
        /*
            Mutliplex directly onto the net connector and not use q->nextQ
         */
        httpPutPacket(q->net->socketq, packet);
    }
    if (stream && q->count <= q->low && (stream->outputq->flags & HTTP_QUEUE_SUSPENDED)) {
        httpResumeQueue(stream->outputq, 0);
    }
}


/*
    After parsing headers, return a packet with body data.
    If headers are not yet fully read, return the packet.
 */
static bool parseHeaders1(HttpQueue *q, HttpPacket *packet)
{
    HttpStream  *stream;
    HttpRx      *rx;

    stream = q->stream;
    assert(stream->rx);
    assert(stream->tx);
    rx = stream->rx;

    if (! gotHeaders(q, packet)) {
        return 0;
    }
    rx->headerPacket = packet;
    if (httpServerStream(stream)) {
        parseRequestLine(q, packet);
    } else {
        parseResponseLine(q, packet);
    }
    parseFields(q, packet);
    return 1;
}


static bool gotHeaders(HttpQueue *q, HttpPacket *packet)
{
    HttpStream  *stream;
    HttpLimits  *limits;
    cchar       *end, *start;
    ssize       len;

    stream = q->stream;
    limits = stream->limits;
    start = eatBlankLines(packet);
    len = httpGetPacketLength(packet);

    if ((end = sncontains(start, "\r\n\r\n", len)) != 0 || (end = sncontains(start, "\n\n", len)) != 0) {
        len = end - start;
    }
    if (len >= limits->headerSize || len >= q->max) {
        httpLimitError(stream, HTTP_ABORT | HTTP_CODE_REQUEST_TOO_LARGE,
            "Header too big. Length %zd vs limit %d", len, limits->headerSize);
        return 0;
    }
    return end ? 1 : 0;
}


/*
    Parse the first line of a http request. Return true if the first line parsed. This is only called once all the headers
    have been read and buffered. Requests look like: METHOD URL HTTP/1.X.
 */
static void parseRequestLine(HttpQueue *q, HttpPacket *packet)
{
    HttpStream  *stream;
    HttpRx      *rx;
    HttpLimits  *limits;
    char        *method, *uri, *protocol;
    ssize       len;

    stream = q->stream;
    rx = stream->rx;
    limits = stream->limits;

    method = getToken(packet, NULL, TOKEN_WORD);
    if (method == NULL || *method == '\0') {
        httpBadRequestError(stream, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad HTTP request. Empty method");
        return;
    }
    rx->originalMethod = rx->method = supper(method);
    httpParseMethod(stream);

    uri = getToken(packet, NULL, TOKEN_URI);
    if (uri == NULL || *uri == '\0') {
        httpBadRequestError(stream, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad HTTP request. Invalid URI");
        return;
    }
    len = slen(uri);
    if (len >= limits->uriSize) {
        httpLimitError(stream, HTTP_ABORT | HTTP_CODE_REQUEST_URL_TOO_LARGE,
            "Bad request. URI too long. Length %zd vs limit %d", len, limits->uriSize);
        return;
    }
    rx->uri = sclone(uri);
    if (!rx->originalUri) {
        rx->originalUri = rx->uri;
    }
    protocol = getToken(packet, "\r\n", TOKEN_WORD);
    if (protocol == NULL || *protocol == '\0') {
        httpBadRequestError(stream, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad HTTP request. Empty protocol");
        return;
    }
    rx->protocol = protocol = supper(protocol);
    if (smatch(protocol, "HTTP/1.0") || *protocol == 0) {
        if (rx->flags & (HTTP_POST|HTTP_PUT)) {
            rx->remainingContent = HTTP_UNLIMITED;
            rx->needInputPipeline = 1;
        }
        stream->net->protocol = 0;
    } else if (strcmp(protocol, "HTTP/1.1") != 0) {
        httpBadRequestError(stream, HTTP_ABORT | HTTP_CODE_NOT_ACCEPTABLE, "Unsupported HTTP protocol");
        return;
    } else {
        stream->net->protocol = 1;
    }
    httpSetState(stream, HTTP_STATE_FIRST);
}


/*
    Parse the first line of a http response. Return true if the first line parsed. This is only called once all the headers
    have been read and buffered. Response status lines look like: HTTP/1.X CODE Message
 */
static void parseResponseLine(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;
    HttpStream  *stream;
    HttpRx      *rx;
    HttpTx      *tx;
    char        *message, *protocol, *status;
    ssize       len;

    net = q->net;
    stream = q->stream;
    rx = stream->rx;
    tx = stream->tx;

    protocol = getToken(packet, NULL, TOKEN_WORD);
    if (protocol == NULL || *protocol == '\0') {
        httpBadRequestError(stream, HTTP_ABORT | HTTP_CODE_NOT_ACCEPTABLE, "Bad response protocol");
        return;
    }
    protocol = supper(protocol);
    if (strcmp(protocol, "HTTP/1.0") == 0) {
        net->protocol = 0;
        if (!scaselessmatch(tx->method, "HEAD")) {
            rx->remainingContent = HTTP_UNLIMITED;
        }
    } else if (strcmp(protocol, "HTTP/1.1") != 0) {
        httpBadRequestError(stream, HTTP_ABORT | HTTP_CODE_NOT_ACCEPTABLE, "Unsupported HTTP protocol");
        return;
    }
    rx->protocol = supper(protocol);
    status = getToken(packet, NULL, TOKEN_NUMBER);
    if (status == NULL || *status == '\0') {
        httpBadRequestError(stream, HTTP_ABORT | HTTP_CODE_NOT_ACCEPTABLE, "Bad response status code");
        return;
    }
    rx->status = atoi(status);

    message = getToken(packet, "\r\n", TOKEN_LINE);
    if (message == NULL || *message == '\0') {
        httpBadRequestError(stream, HTTP_ABORT | HTTP_CODE_NOT_ACCEPTABLE, "Bad response status message");
        return;
    }
    rx->statusMessage = sclone(message);

    len = slen(rx->statusMessage);
    if (len >= stream->limits->uriSize) {
        httpLimitError(stream, HTTP_ABORT | HTTP_CODE_REQUEST_URL_TOO_LARGE,
            "Bad response. Status message too long. Length %zd vs limit %d", len, stream->limits->uriSize);
        return;
    }
}


/*
    Parse the header fields and return a following body packet if present.
    Return zero on errors.
 */
static void parseFields(HttpQueue *q, HttpPacket *packet)
{
    HttpStream  *stream;
    HttpRx      *rx;
    HttpLimits  *limits;
    char        *key, *value;
    int         count;

    stream = q->stream;
    rx = stream->rx;

    limits = stream->limits;

    for (count = 0; mprGetBufLength(packet->content) > 0 && packet->content->start[0] != '\r' && !stream->error; count++) {
        if (count >= limits->headerMax) {
            httpLimitError(stream, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Too many headers");
            return;
        }
        key = getToken(packet, ":", TOKEN_HEADER_KEY);
        if (key == NULL || *key == '\0' || mprGetBufLength(packet->content) == 0) {
            httpBadRequestError(stream, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad header format");
            return;
        }
        value = getToken(packet, "\r\n", TOKEN_HEADER_VALUE);
        if (value == NULL || mprGetBufLength(packet->content) == 0 || packet->content->start[0] == '\0') {
            httpBadRequestError(stream, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad header value");
            return;
        }
        if (scaselessmatch(key, "set-cookie")) {
            mprAddDuplicateKey(rx->headers, key, sclone(value));
        } else {
            mprAddKey(rx->headers, key, sclone(value));
        }
    }
    /*
        If there were no headers, there will be no trailing ...
     */
    if (mprGetBufLength(packet->content) < 2) {
        httpBadRequestError(stream, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad header format");
        return;
    }
    /*
        Split the headers and retain the data for later. Step over "\r\n" after headers except if chunked
        so chunking can parse a single chunk delimiter of "\r\nSIZE ...\r\n"
     */
    if (smatch(httpGetHeader(stream, "transfer-encoding"), "chunked")) {
        httpInitChunking(stream);
    } else {
        mprAdjustBufStart(packet->content, 2);
    }
    if (rx->status != HTTP_CODE_CONTINUE) {
        httpSetState(stream, HTTP_STATE_PARSED);
    }
}


/*
    Get the next input token. The content buffer is advanced to the next token.
    The delimiter is a string to match and not a set of characters.
    If the delimeter null, it means use white space (space or tab) as a delimiter.
 */
static char *getToken(HttpPacket *packet, cchar *delim, int validation)
{
    MprBuf  *buf;
    char    *token, *endToken;

    buf = packet->content;
    /* Already null terminated but for safety */
    mprAddNullToBuf(buf);
    token = mprGetBufStart(buf);
    endToken = mprGetBufEnd(buf);

    /*
        Eat white space before token
     */
    for (; token < endToken && (*token == ' ' || *token == '\t'); token++) {}

    if (delim) {
        if ((endToken = sncontains(token, delim, endToken - token)) == NULL) {
            return NULL;
        }
        /* Only eat one occurence of the delimiter */
        buf->start = endToken + strlen(delim);
        *endToken = '\0';

    } else {
        delim = " \t";
        if ((endToken = strpbrk(token, delim)) == NULL) {
            return NULL;
        }
        buf->start = endToken + strspn(endToken, delim);
        *endToken = '\0';
    }
    token = validateToken(token, endToken, validation);
    return token;
}


static char *validateToken(char *token, char *endToken, int validation)
{
    char   *t;

    if (validation == TOKEN_HEADER_KEY) {
        if (*token == '\0') {
            return NULL;
        }
        if (strpbrk(token, "\"\\/ \t\r\n(),:;<=>?@[]{}")) {
            return NULL;
        }
        for (t = token; t < endToken && *t; t++) {
            if (!isprint(*t)) {
                return NULL;
            }
        }
    } else if (validation == TOKEN_HEADER_VALUE) {
        if (token < endToken) {
            /* Trim white space */
            for (t = endToken - 1; t >= token; t--) {
                if (isspace((uchar) *t)) {
                    *t = '\0';
                } else {
                    break;
                }
            }
        }
        while (isspace((uchar) *token)) {
            token++;
        }
        for (t = token; *t; t++) {
            if (!isprint(*t)) {
                return NULL;
            }
        }
    } else if (validation == TOKEN_URI) {
        if (!httpValidUriChars(token)) {
            return NULL;
        }
    } else if (validation == TOKEN_NUMBER) {
        if (!snumber(token)) {
            return NULL;
        }
    } else if (validation == TOKEN_WORD) {
        if (strpbrk(token, " \t\r\n") != NULL) {
            return NULL;
        }
    } else {
        if (strpbrk(token, "\r\n") != NULL) {
            return NULL;
        }
    }
    return token;
}


PUBLIC void httpCreateHeaders1(HttpQueue *q, HttpPacket *packet)
{
    Http        *http;
    HttpStream  *stream;
    HttpTx      *tx;
    HttpUri     *parsedUri;
    MprKey      *kp;
    MprBuf      *buf;

    stream = q->stream;
    http = stream->http;
    tx = stream->tx;
    buf = packet->content;

    tx->responded = 1;

    if (tx->chunkSize <= 0 && q->count > 0 && tx->length < 0) {
        /* No content length and there appears to be output data -- must close connection to signify EOF */
        stream->keepAliveCount = 0;
    }
    if ((tx->flags & HTTP_TX_USE_OWN_HEADERS) && !stream->error) {
        /* Cannot count on content length */
        stream->keepAliveCount = 0;
        return;
    }
    httpPrepareHeaders(stream);

    if (httpServerStream(stream)) {
        mprPutStringToBuf(buf, httpGetProtocol(stream->net));
        mprPutCharToBuf(buf, ' ');
        mprPutIntToBuf(buf, tx->status);
        mprPutCharToBuf(buf, ' ');
        mprPutStringToBuf(buf, httpLookupStatus(tx->status));
        /* Server tracing of status happens in the "complete" event */

    } else {
        mprPutStringToBuf(buf, tx->method);
        mprPutCharToBuf(buf, ' ');
        parsedUri = tx->parsedUri;
        if (http->proxyHost && *http->proxyHost) {
            if (parsedUri->query && *parsedUri->query) {
                mprPutToBuf(buf, "http://%s:%d%s?%s %s", http->proxyHost, http->proxyPort,
                    parsedUri->path, parsedUri->query, httpGetProtocol(stream->net));
            } else {
                mprPutToBuf(buf, "http://%s:%d%s %s", http->proxyHost, http->proxyPort, parsedUri->path,
                    httpGetProtocol(stream->net));
            }
        } else {
            if (parsedUri->query && *parsedUri->query) {
                mprPutToBuf(buf, "%s?%s %s", parsedUri->path, parsedUri->query, httpGetProtocol(stream->net));
            } else {
                mprPutStringToBuf(buf, parsedUri->path);
                mprPutCharToBuf(buf, ' ');
                mprPutStringToBuf(buf, httpGetProtocol(stream->net));
            }
        }
    }
    mprPutStringToBuf(buf, "\r\n");

    if (httpTracing(q->net)) {
        if (httpServerStream(stream)) {
            httpLog(stream->trace, "tx.http.status", "result", "@%s %d %s",
                httpGetProtocol(stream->net), tx->status, httpLookupStatus(tx->status));
        } else {
            if (parsedUri->query && *parsedUri->query) {
                httpLog(stream->trace, "tx.http.status", "request", "@%s %s?%s %s", tx->method, tx->parsedUri->path,
                    tx->parsedUri->query, httpGetProtocol(stream->net));
            } else {
                httpLog(stream->trace, "tx.http.status", "request", "@%s %s %s", tx->method, tx->parsedUri->path, httpGetProtocol(stream->net));
            }
        }
        httpLog(stream->trace, "tx.http.headers", "headers", "@%s", httpTraceHeaders(tx->headers));
    }
    /*
        Output headers
     */
    kp = mprGetFirstKey(stream->tx->headers);
    while (kp) {
        mprPutStringToBuf(packet->content, kp->key);
        mprPutStringToBuf(packet->content, ": ");
        if (kp->data) {
            mprPutStringToBuf(packet->content, kp->data);
        }
        mprPutStringToBuf(packet->content, "\r\n");
        kp = mprGetNextKey(stream->tx->headers, kp);
    }
    /*
        By omitting the "\r\n" delimiter after the headers, chunks can emit "\r\nSize\r\n" as a single chunk delimiter
     */
    if (tx->chunkSize <= 0) {
        mprPutStringToBuf(buf, "\r\n");
    }
    tx->headerSize = mprGetBufLength(buf);
    tx->flags |= HTTP_TX_HEADERS_CREATED;
}


static HttpStream *findStream1(HttpQueue *q)
{
    HttpStream  *stream;

    if (!q->stream) {
        if ((stream = httpCreateStream(q->net, 1)) == 0) {
            /* Memory error - centrally reported */
            return 0;
        }
        q->stream = stream;
        q->pair->stream = stream;
    } else {
        stream = q->stream;
    }
    stream->lastActivity = stream->net->lastActivity;
    return stream;
}


static cchar *eatBlankLines(HttpPacket *packet)
{
    MprBuf  *content;
    cchar   *start;

    content = packet->content;
    while (mprGetBufLength(content) > 0) {
        start = mprGetBufStart(content);
        if (*start != '\r' && *start != '\n') {
            break;
        }
        mprGetCharFromBuf(packet->content);
    }
    return mprGetBufStart(content);
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/http2Filter.c ************/

/*
    http2Filter.c - HTTP/2 protocol handling.

    HTTP/2 protocol filter for HTTP/2 frame processing.

    For historical reasons, the HttpStream object is used to implement HTTP2 streams and HttpNet is
    used to implement HTTP2 network connections.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



#if ME_HTTP_HTTP2
/********************************** Locals ************************************/

#define httpGetPrefixMask(bits) ((1 << (bits)) - 1)
#define httpSetPrefix(bits)     (1 << (bits))

typedef void (*FrameHandler)(HttpQueue *q, HttpPacket *packet);

/************************************ Forwards ********************************/

static bool addHeaderToSet(HttpStream *stream, cchar *key, cchar *value);
static int setState(HttpStream *stream, int event);
static void closeNetworkWhenDone(HttpQueue *q);
static HttpFrame *createFrame(HttpQueue *q, HttpPacket *packet, int type, int flags, int streamID);
static int decodeInt(HttpPacket *packet, uint prefix);
static HttpPacket *defineFrame(HttpQueue *q, HttpPacket *packet, int type, uchar flags, int stream);
static void definePseudoHeaders(HttpStream *stream, HttpPacket *packet);
static void encodeHeader(HttpStream *stream, HttpPacket *packet, cchar *key, cchar *value);
static void encodeInt(HttpPacket *packet, uint prefix, uint bits, uint value);
static void encodeString(HttpPacket *packet, cchar *src, uint lower);
static HttpStream *findStream(HttpNet *net, int stream);
static int getFrameFlags(HttpQueue *q, HttpPacket *packet);
static HttpStream *getStream(HttpQueue *q, HttpPacket *packet);
static void incomingHttp2(HttpQueue *q, HttpPacket *packet);
static void invalidState(HttpNet *net, HttpStream *stream, int event);
static void outgoingHttp2(HttpQueue *q, HttpPacket *packet);
static void outgoingHttp2Service(HttpQueue *q);
static void manageFrame(HttpFrame *frame, int flags);
static void parseDataFrame(HttpQueue *q, HttpPacket *packet);
static HttpFrame *parseFrame(HttpQueue *q, HttpPacket *packet, int *done);
static void parseGoAwayFrame(HttpQueue *q, HttpPacket *packet);
static void parseHeaderFrame(HttpQueue *q, HttpPacket *packet);
static cchar *parseHeaderField(HttpQueue *q, HttpStream *stream, HttpPacket *packet);
static bool parseHeader(HttpQueue *q, HttpStream *stream, HttpPacket *packet);
static void parseHeaders2(HttpQueue *q, HttpStream *stream);
static void parsePriorityFrame(HttpQueue *q, HttpPacket *packet);
static void parsePushFrame(HttpQueue *q, HttpPacket *packet);
static void parsePingFrame(HttpQueue *q, HttpPacket *packet);
static void parseResetFrame(HttpQueue *q, HttpPacket *packet);
static void parseSettingsFrame(HttpQueue *q, HttpPacket *packet);
static void parseWindowFrame(HttpQueue *q, HttpPacket *packet);
static void pickStreamNumber(HttpStream *stream);
static void processDataFrame(HttpQueue *q, HttpPacket *packet);
static void resetStream(HttpStream *stream, cchar *msg, int error);
static ssize resizePacket(HttpQueue *q, ssize max, HttpPacket *packet);
static void restartSuspendedStreams(HttpNet *net);
static void sendFrame(HttpQueue *q, HttpPacket *packet);
static void sendGoAway(HttpQueue *q, int status, cchar *fmt, ...);
static void sendPreface(HttpQueue *q);
static void sendReset(HttpQueue *q, HttpStream *stream, int status, cchar *fmt, ...);
static void sendSettings(HttpQueue *q);
static void sendSettingsFrame(HttpQueue *q);
static void sendWindowFrame(HttpQueue *q, int streamID, ssize size);
static bool validateHeader(cchar *key, cchar *value);

/*
    Frame callback handlers (order matters)
 */
static FrameHandler frameHandlers[] = {
    parseDataFrame,
    parseHeaderFrame,
    parsePriorityFrame,
    parseResetFrame,
    parseSettingsFrame,
    parsePushFrame,
    parsePingFrame,
    parseGoAwayFrame,
    parseWindowFrame,
    /* ContinuationFrame */ parseHeaderFrame,
};

/*
    Just for debug
 */
static char *packetTypes[] = {
    "DATA",
    "HEADERS",
    "PRIORITY",
    "RESET",
    "SETTINGS",
    "PUSH",
    "PING",
    "GOAWAY",
    "WINDOW",
    "CONTINUE",
};

/*
    Target states
 */
 #define H2_ERR      -1          /* Invalid state transition */
 #define H2_SAME     -2          /* Don't change state */
 #define H2_IDLE     0           /* Initial state */
 #define H2_RLOCAL   1           /* Reserved local */
 #define H2_RREMOTE  2           /* Reserved remote */
 #define H2_OPEN     3
 #define H2_CLOCAL   4           /* Half closed local */
 #define H2_CREMOTE  5           /* Half closed remote */
 #define H2_CLOSED   6
 #define H2_GONE     7           /* Connection gone */
 #define H2_MAX      8


static const char* States[] = {
    "Idle",
    "Reserved Local",
    "Reserved Remote",
    "Open",
    "Closed Local",
    "Closed Remote",
    "Closed",
    "Gone"
};

/*
    State events
 */
#define E2_DATA     HTTP2_DATA_FRAME
#define E2_HDR      HTTP2_HEADERS_FRAME
#define E2_PRI      HTTP2_PRIORITY_FRAME
#define E2_RESET    HTTP2_RESET_FRAME
#define E2_SETTINGS HTTP2_SETTINGS_FRAME
#define E2_PUSH     HTTP2_PUSH_FRAME
#define E2_PING     HTTP2_PING_FRAME
#define E2_GOAWAY   HTTP2_GOAWAY_FRAME
#define E2_WINDOW   HTTP2_WINDOW_FRAME
#define E2_CONT     HTTP2_CONT_FRAME
#define E2_END      HTTP2_MAX_FRAME + 0         /* Receive end stream */
#define E2_TX_END   HTTP2_MAX_FRAME +1          /* Transmit end stream */
#define E2_TX_PUSH  HTTP2_MAX_FRAME + 2         /* Transmit push promise */
#define E2_TX_RESET HTTP2_MAX_FRAME + 3         /* Transmit push promise */
#define E2_TX_HDR   HTTP2_MAX_FRAME + 4         /* Transmit header */
#define E2_TX_DATA  HTTP2_MAX_FRAME + 5         /* Transmit header */
#define E2_MAX      HTTP2_MAX_FRAME + 6

static const char *Events[] = {
    "Receive Data Frame",
    "Receive Header Frame",
    "Receive Priority Frame",
    "Receive Reset Frame",
    "Receive Settings Frame",
    "Receive Push Frame",
    "Receive Ping Frame",
    "Receive GoAway Frame",
    "Receive Window Frame",
    "Receive Continue Frame",
    "Receive End Stream Flag",
    "Send End Stream Flag",
    "Send Push Frame",
    "Send Reset Frame",
    "Send Header Frame",
    "Send Data Frame",
};

static int StateMatrix[E2_MAX][H2_MAX] = {
/* States         idle        rsv-local   rsv-remote  open         clocal      cremote     closed      gone */

/* Receive */
/* data */      { H2_ERR,     H2_ERR,     H2_ERR,     H2_SAME,     H2_SAME,    H2_ERR,     H2_ERR,     H2_ERR  },
/* header */    { H2_OPEN,    H2_OPEN,    H2_ERR,     H2_SAME,     H2_SAME,    H2_ERR,     H2_ERR,     H2_ERR  },
/* pri */       { H2_SAME,    H2_SAME,    H2_SAME,    H2_SAME,     H2_SAME,    H2_SAME,    H2_SAME,    H2_SAME },
/* reset */     { H2_CLOSED,  H2_CLOSED,  H2_CLOSED,  H2_CLOSED,   H2_CLOSED,  H2_CLOSED,  H2_SAME,    H2_ERR  },
/* settings */  { H2_SAME,    H2_SAME,    H2_SAME,    H2_SAME,     H2_SAME,    H2_SAME,    H2_SAME,    H2_ERR  },
/* rx push */   { H2_RREMOTE, H2_ERR,     H2_ERR,     H2_ERR,      H2_ERR,     H2_ERR,     H2_ERR,     H2_ERR  },
/* ping */      { H2_SAME,    H2_SAME,    H2_SAME,    H2_SAME,     H2_SAME,    H2_SAME,    H2_SAME,    H2_SAME },
/* goaway */    { H2_GONE,    H2_GONE,    H2_GONE,    H2_GONE,     H2_GONE,    H2_GONE,    H2_GONE,    H2_GONE },
/* window */    { H2_SAME,    H2_SAME,    H2_SAME,    H2_SAME,     H2_SAME,    H2_SAME,    H2_SAME,    H2_ERR  },
/* cont  */     { H2_OPEN,    H2_ERR,     H2_CLOCAL,  H2_SAME,     H2_ERR,     H2_SAME,    H2_ERR,     H2_ERR  },
/* end */       { H2_ERR,     H2_ERR,     H2_ERR,     H2_CREMOTE,  H2_CLOSED,  H2_ERR,     H2_ERR,     H2_ERR  },

/* Transmit */
/* end */       { H2_ERR,     H2_ERR,     H2_ERR,     H2_CLOCAL,   H2_ERR,     H2_CLOSED,  H2_ERR,     H2_ERR  },
/* push */      { H2_RLOCAL,  H2_ERR,     H2_ERR,     H2_ERR,      H2_ERR,     H2_ERR,     H2_ERR,     H2_ERR  },
/* reset */     { H2_CLOSED,  H2_CLOSED,  H2_CLOSED,  H2_CLOSED,   H2_CLOSED,  H2_CLOSED,  H2_ERR,     H2_ERR  },
/* header */    { H2_OPEN,    H2_OPEN,    H2_ERR,     H2_SAME,     H2_ERR,     H2_SAME,    H2_ERR,     H2_ERR  },
/* data */      { H2_ERR,     H2_ERR,     H2_ERR,     H2_SAME,     H2_ERR,     H2_SAME,    H2_ERR,     H2_ERR  },
};

/*********************************** Code *************************************/
/*
    Loadable module initialization
 */
PUBLIC int httpOpenHttp2Filter()
{
    HttpStage     *filter;

    if ((filter = httpCreateConnector("Http2Filter", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    HTTP->http2Filter = filter;
    filter->incoming = incomingHttp2;
    filter->outgoing = outgoingHttp2;
    filter->outgoingService = outgoingHttp2Service;
    httpCreatePackedHeaders();
    return 0;
}


/*
    Receive and process incoming HTTP/2 packets.
 */
static void incomingHttp2(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;
    HttpStream  *stream;
    HttpFrame   *frame;
    int         done;

    net = q->net;

    /*
        Join packets into a single packet for processing. Typically will be only one packet and do nothing.
     */
    httpJoinPacketForService(q, packet, HTTP_DELAY_SERVICE);
    sendSettings(q);
    /*
        Process frames until can process no more. Initially will be only one packet, but the frame handlers
        may split packets as required and put back the tail for processing here.
     */
    for (done = 0, packet = httpGetPacket(q); packet && !net->receivedGoaway; packet = httpGetPacket(q)) {

        if ((frame = parseFrame(q, packet, &done)) != 0) {
            net->frame = frame;
            frameHandlers[frame->type](q, packet);
            net->frame = 0;
            stream = frame->stream;
            if (stream && !stream->destroyed) {
                if (stream->rx->endStream) {
                    httpAddInputEndPacket(stream, stream->inputq);
                }
                if (stream->disconnect) {
                    sendReset(q, stream, HTTP2_INTERNAL_ERROR, "Stream request error %s", stream->errorMsg);
                }
                if (stream->h2State >= H2_CLOSED) {
                    httpDestroyStream(stream);
                }
            }
        }
        if (done) {
            break;
        }
        if (mprNeedYield()) {
            mprYield(0);
        }
    }
    closeNetworkWhenDone(q);
}


/*
    Accept packet for sending
 */
static void outgoingHttp2(HttpQueue *q, HttpPacket *packet)
{
    HttpStream  *stream;

    stream = packet->stream;

    sendSettings(q);
    if (stream->streamID == 0 && !httpIsServer(stream->net)) {
        pickStreamNumber(stream);
    }
    if (packet->flags & HTTP_PACKET_DATA) {
        // Must compute window here as this is run synchronously from the upstream sender
        assert(stream->outputq->window >= 0);
        stream->outputq->window -= httpGetPacketLength(packet);
        assert(stream->outputq->window >= 0);
    }
    httpPutForService(q, packet, HTTP_SCHEDULE_QUEUE);
}


/*
    Service the outgoing queue of packets. This uses HTTP/2 for end to end flow control rather than
    using httpWillQueueAcceptPacket for downstream flow control.
 */
static void outgoingHttp2Service(HttpQueue *q)
{
    HttpNet     *net;
    HttpStream  *stream;
    HttpPacket  *packet, *frame;
    HttpTx      *tx;
    int         flags;

    net = q->net;
    assert(!(q->flags & HTTP_QUEUE_SUSPENDED));

    /*
        Note: httpGetPacket will not automatically resume the previous queue (which logically is each streams' tailFilter).
        Note: q->nextQ == q->prevQ == socketq, so we must explicitly re-enable the stream's tail filter below.
     */
    for (packet = httpGetPacket(q); packet && !net->error; packet = httpGetPacket(q)) {
        net->lastActivity = net->http->now;

        if (net->outputq->window <= 0 || net->socketq->count >= net->socketq->max) {
            /*
                The output queue has depleted the HTTP/2 transmit window. Flow control and wait for
                a window update message from the peer.
             */
            httpSuspendQueue(q);
            httpPutBackPacket(q, packet);
            break;
        }
        stream = packet->stream;

        if (stream && !stream->destroyed) {
            if (packet->flags & HTTP_PACKET_HEADER) {
                if (setState(stream, E2_TX_HDR) == H2_ERR) {
                    invalidState(net, stream, E2_TX_HDR);
                    break;
                }
            } else if (packet->flags & HTTP_PACKET_DATA) {
                if (setState(stream, E2_TX_DATA) == H2_ERR) {
                    invalidState(net, stream, E2_TX_DATA);
                    break;
                }
                /*
                    Resize data packets to not exceed the remaining HTTP/2 window flow control credits.
                 */
                resizePacket(net->outputq, net->outputq->window, packet);
            }

            if (net->receivedGoaway && (net->lastStreamID && stream->streamID >= net->lastStreamID)) {
                /* Network is being closed. Continue to process existing streams but accept no new streams */
                continue;
            }
            if (stream->disconnect) {
                sendReset(q, stream, HTTP2_INTERNAL_ERROR, "Stream request error %s", stream->errorMsg);
                continue;
            }
            stream->lastActivity = stream->http->now;
            tx = stream->tx;

            if (packet->flags & HTTP_PACKET_END && tx->allDataSent) {
                httpPutPacket(q->net->socketq, packet);
                packet = 0;
            }
            assert(net->outputq->window > 0);

            if (packet) {
                flags = getFrameFlags(q, packet);
                if (flags & HTTP2_END_STREAM_FLAG) {
                    if (setState(stream, E2_TX_END) == H2_ERR) {
                        invalidState(net, stream, E2_TX_END);
                        break;
                    }
                }
                if (packet->flags & HTTP_PACKET_HEADER) {
                    if (stream->tx->startedHeader) {
                        packet->type = HTTP2_CONT_FRAME;
                    } else {
                        packet->type = HTTP2_HEADERS_FRAME;
                        stream->tx->startedHeader = 1;
                    }
                } else if (packet->flags & (HTTP_PACKET_DATA | HTTP_PACKET_END)) {
                    packet->type = HTTP2_DATA_FRAME;
                }
                frame = defineFrame(q, packet, packet->type, flags, stream->streamID);
                sendFrame(q, frame);
            }

            /*
                Resume upstream if there is now room
             */
            if (q->count <= q->low && (stream->outputq->flags & HTTP_QUEUE_SUSPENDED)) {
                httpResumeQueue(stream->outputq, 0);
            }
        }
        if (net->outputq->window <= 0) {
            httpSuspendQueue(q);
            break;
        }
    }

    if (net->outputq->window >= 0 && q->count <= q->low && !(q->flags & HTTP_QUEUE_SUSPENDED)) {
        restartSuspendedStreams(net);
    }
    closeNetworkWhenDone(q);
}


/*
    Get the HTTP/2 frame flags for this packet
 */
static int getFrameFlags(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;
    HttpStream  *stream;
    HttpTx      *tx;
    int         flags;

    net = q->net;
    stream = packet->stream;
    tx = stream->tx;
    flags = 0;

    if (packet->flags & HTTP_PACKET_HEADER && packet->last) {
        flags |= HTTP2_END_HEADERS_FLAG;

    } else if (packet->flags & HTTP_PACKET_DATA && packet->last) {
        flags |= HTTP2_END_STREAM_FLAG;
        tx->allDataSent = 1;

    } else if (packet->flags & HTTP_PACKET_END && !tx->allDataSent) {
        flags |= HTTP2_END_STREAM_FLAG;
        tx->allDataSent = 1;
    }
    if (packet->flags & HTTP_PACKET_DATA) {
        assert(net->outputq->window > 0);
        net->outputq->window -= httpGetPacketLength(packet);
    }
    return flags;
}


/*
    Resize a packet to utilize the remaining HTTP/2 window credits. Must not exceed the remaining window size.
    OPT - would be more optimal to not split packets
 */
static ssize resizePacket(HttpQueue *q, ssize max, HttpPacket *packet)
{
    HttpPacket  *tail;
    ssize       len;

    len = httpGetPacketLength(packet);
    if (len > max) {
        if ((tail = httpSplitPacket(packet, max)) == 0) {
            /* Memory error - centrally reported */
            return len;
        }
        httpPutBackPacket(q, tail);
        len = httpGetPacketLength(packet);
    }
    return len;
}


PUBLIC void httpFinalizeHttp2Stream(HttpStream *stream)
{
    if (stream->h2State >= H2_CLOSED) {
        httpDestroyStream(stream);
    }
}



/*
    Close the network connection on errors of if instructed to go away.
 */
static void closeNetworkWhenDone(HttpQueue *q)
{
    HttpNet     *net;

    net = q->net;
    if (net->error) {
        if (!net->sentGoaway) {
            sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Closing network");
        }
    }
    if (net->receivedGoaway) {
        if (net->sentGoaway) {
            if (mprGetListLength(net->streams) == 0) {
                /* This ensures a recall on the netConnector IOEvent handler */
                mprDisconnectSocket(net->sock);
            }
        } else {
            sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Closing network");
        }
    }
}

static void logIncomingPacket(HttpQueue *q, HttpPacket *packet, ssize payloadLen, int type, int streamID, int flags)
{
    MprBuf      *buf;
    cchar       *typeStr;
    ssize       excess, len;

    buf = packet->content;
    if (httpTracing(q->net)) {
        len = httpGetPacketLength(packet);
        excess = len - payloadLen;
        if (excess > 0) {
            mprAdjustBufEnd(buf, -excess);
        }
        mprAdjustBufStart(buf, -HTTP2_FRAME_OVERHEAD);

        typeStr = (type < HTTP2_MAX_FRAME) ? packetTypes[type] : "unknown";
        httpLog(q->net->trace, "rx.http2", "packet",
            "frame=%s flags=%x stream=%d length=%zd", typeStr, flags, streamID, httpGetPacketLength(packet));

        mprAdjustBufStart(buf, HTTP2_FRAME_OVERHEAD);
        if (excess > 0) {
            mprAdjustBufEnd(buf, excess);
        }
    }
}


/*
    Parse an incoming HTTP/2 frame. Return true to keep going with this or subsequent request, zero means
    insufficient data to proceed.
 */
static HttpFrame *parseFrame(HttpQueue *q, HttpPacket *packet, int *done)
{
    HttpNet     *net;
    HttpPacket  *tail;
    MprBuf      *buf;
    ssize       payloadLen, size;
    uint32      lenType;
    uint        flags, type;
    int         streamID;

    net = q->net;
    buf = packet->content;
    *done = 0;

    size = mprGetBufLength(buf);
    if (size < HTTP2_FRAME_OVERHEAD) {
        if (size > 0) {
            /* Insufficient data */
            httpPutBackPacket(q, packet);
        }
        *done = 1;
        return 0;
    }
    /*
        Length is 24 bits. Read with type
     */
    lenType = mprGetUint32FromBuf(buf);
    type = lenType & 0xFF;
    payloadLen = lenType >> 8;
    flags = mprGetCharFromBuf(buf);
    streamID = mprGetUint32FromBuf(buf) & HTTP_STREAM_MASK;

    if (payloadLen > q->packetSize || payloadLen > HTTP2_MAX_FRAME_SIZE) {
        logIncomingPacket(q, packet, payloadLen, type, streamID, flags);
        sendGoAway(q, HTTP2_FRAME_SIZE_ERROR, "Bad frame size %d vs %d", payloadLen, q->packetSize);
        return 0;
    }
    if (net->sentGoaway && net->lastStreamID && streamID >= net->lastStreamID) {
        logIncomingPacket(q, packet, payloadLen, type, streamID, flags);
        /* Network is being closed. Continue to process existing streams but accept no new streams */
        return 0;
    }

    /*
        Split data for a following frame and put back on the queue for later servicing.
     */
    size = mprGetBufLength(buf);

    if (payloadLen > size) {
        mprAdjustBufStart(buf, -HTTP2_FRAME_OVERHEAD);
        httpPutBackPacket(q, packet);
        //  Don't log packet yet
        *done = 1;
        return 0;
    }
    logIncomingPacket(q, packet, payloadLen, type, streamID, flags);

    if (payloadLen < size) {
        if ((tail = httpSplitPacket(packet, payloadLen)) == 0) {
            /* Memory error - centrally reported */
            *done = 1;
            return 0;
        }
        httpPutBackPacket(q, tail);
        buf = packet->content;
    }
    return createFrame(q, packet, type, flags, streamID);
}


static HttpFrame *createFrame(HttpQueue *q, HttpPacket *packet, int type, int flags, int streamID)
{
    HttpNet     *net;
    HttpFrame   *frame;
    HttpStream  *stream;
    int         state;

    /*
        Parse the various HTTP/2 frame fields and store in a local HttpFrame object.
     */
    if ((frame = mprAllocObj(HttpFrame, manageFrame)) == NULL) {
        /* Memory error - centrally reported */
        return 0;
    }
    net = q->net;
    packet->data = frame;
    frame->type = type;
    frame->flags = flags;
    frame->streamID = streamID;

    stream = frame->stream = findStream(net, streamID);

    if (frame->type < 0 || frame->type >= HTTP2_MAX_FRAME) {
        //  Must ignore invalid or unknown frame types except when parsing headers
        if (net->parsingHeaders) {
            sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Invalid frame while parsing headers type %d, stream %d", frame->type, frame->streamID);
        }
        return 0;
    }
    if (stream) {
        if ((state = setState(stream, type)) == H2_ERR) {
            invalidState(net, stream, type);
            return 0;

        } else if (flags & HTTP2_END_STREAM_FLAG && type != HTTP2_HEADERS_FRAME && type != HTTP2_CONT_FRAME) {
            stream->rx->endStream = 1;
            state = setState(stream, E2_END);
            if (state == H2_ERR) {
                invalidState(net, stream, E2_END);
                return 0;
            }
        }
        if (net->parsingHeaders && type != HTTP2_CONT_FRAME) {
            invalidState(net, stream, type);
            return 0;
        }
    }
    return frame;
}


/*
    Always get a settings frame at the start of any network connection
 */
static void parseSettingsFrame(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;
    HttpFrame   *frame;
    HttpLimits  *limits;
    HttpStream  *stream;
    MprBuf      *buf;
    uint        field, value;
    int         diff, next;

    net = q->net;
    limits = net->limits;
    buf = packet->content;
    frame = packet->data;

    if (frame->flags & HTTP2_ACK_FLAG || net->sentGoaway || net->receivedGoaway) {
        if (httpGetPacketLength(packet) > 0) {
            sendGoAway(q, HTTP2_FRAME_SIZE_ERROR, "Invalid settings packet with both Ack and payload");
        } else {
            /* Simple ack */
        }
        return;
    }
    if (frame->streamID) {
        sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Invalid settings packet with streamID %d", frame->streamID);
        return;
    }
    while (httpGetPacketLength(packet) >= HTTP2_SETTINGS_SIZE) {
        field = mprGetUint16FromBuf(buf);
        value = mprGetUint32FromBuf(buf);

        switch (field) {
        case HTTP2_HEADER_TABLE_SIZE_SETTING:
            value = min((int) value, limits->hpackMax);
            httpSetPackedHeadersMax(net->txHeaders, value);
            break;

        case HTTP2_ENABLE_PUSH_SETTING:
            if (value != 0 && value != 1) {
                sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Invalid push value");
                return;
            }
            /* Push is not yet supported, we just store the value but do nothing */
            net->push = value;
            break;

        case HTTP2_MAX_STREAMS_SETTING:
            /* Permit peer supporting more streams, but don't ever create more than streamsMax limit */
            if (value <= 0) {
                sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Too many streams setting %d max %d", value, ME_MAX_STREAMS);
                return;
            }
            limits->txStreamsMax = min((int) value, limits->streamsMax);
            break;

        case HTTP2_INIT_WINDOW_SIZE_SETTING:
            if (value > HTTP2_MAX_WINDOW) {
                sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Invalid window size setting %d max %d", value, HTTP2_MAX_WINDOW);
                return;
            }
            if (mprGetListLength(net->streams) > 0) {
                diff = (int) value - (int) net->window;
                for (ITERATE_ITEMS(net->streams, stream, next)) {
                    if (stream->h2State == H2_OPEN || stream->h2State == H2_CREMOTE) {
                        stream->outputq->window += diff;
                    }
                }
            } else {
                net->window = value;
            }
            break;

        case HTTP2_MAX_FRAME_SIZE_SETTING:
            /* Permit peer supporting bigger frame sizes, but don't ever create packets larger than the packetSize limit */
            if (value < HTTP2_MIN_FRAME_SIZE || value > HTTP2_MAX_FRAME_SIZE) {
                sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Invalid frame size setting %d max %d", value, ME_PACKET_SIZE);
                return;
            }
            if (value < net->outputq->packetSize) {
                net->outputq->packetSize = min(value, ME_PACKET_SIZE);
            }
            break;

        case HTTP2_MAX_HEADER_SIZE_SETTING:
            if (value <= 0 || value > ME_MAX_HEADERS) {
                sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Invalid header size setting %d max %d", value, ME_MAX_HEADERS);
                return;
            }
            if ((int) value < limits->headerSize) {
                if ((limits = httpCreateLimits(1)) != 0) {
                    memcpy(limits, net->limits, sizeof(HttpLimits));
                    limits->headerSize = value;
                    net->limits = limits;
                }
            }
            break;

        default:
            /* Ignore unknown settings values (per spec) */
            break;
        }
    }
    if (httpGetPacketLength(packet) > 0) {
        sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Invalid setting packet length");
        return;
    }
    mprFlushBuf(packet->content);
    sendFrame(q, defineFrame(q, packet, HTTP2_SETTINGS_FRAME, HTTP2_ACK_FLAG, 0));
    restartSuspendedStreams(net);
}


/*
    Parse a HTTP header or HTTP header continuation frame
 */
static void parseHeaderFrame(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;
    HttpStream  *stream;
    HttpFrame   *frame;
    MprBuf      *buf;
    bool        padded, priority;
    ssize       size, payloadLen;
    int         value, padLen;

    net = q->net;
    buf = packet->content;
    frame = packet->data;
    stream = frame->stream;
    padded = frame->flags & HTTP2_PADDED_FLAG;
    priority = frame->flags & HTTP2_PRIORITY_FLAG;

    size = 0;
    if (padded) {
        size++;
    }
    if (priority) {
        /* dependency + weight */
        size += sizeof(uint32) + 1;
    }
    payloadLen = mprGetBufLength(buf);
    if (payloadLen <= size) {
        sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Incorrect header length");
        return;
    }
    if (padded) {
        padLen = (int) mprGetCharFromBuf(buf);
        if (padLen >= payloadLen) {
            sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Incorrect padding length");
            return;
        }
        mprAdjustBufEnd(buf, -padLen);
    }
    frame->weight = HTTP2_DEFAULT_WEIGHT;
    if (priority) {
        /* Priority Frames */
        value = mprGetUint32FromBuf(buf);
        frame->depend = value & 0x7fffffff;
        frame->exclusive = value >> 31;
        frame->weight = mprGetCharFromBuf(buf) + 1;
    }
    if ((frame->streamID % 2) != 1 || (net->lastStreamID && frame->streamID < net->lastStreamID)) {
        sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Bad sesssion");
        return;
    }
    if ((stream = getStream(q, packet)) != 0) {
        if (setState(stream, frame->type) == H2_ERR) {
            invalidState(net, stream, frame->type);
            return;
        }
        if (frame->flags & HTTP2_END_STREAM_FLAG) {
            /*
                Replicate here from parseFrame because parseFrame did not have the stream
             */
            if (setState(stream, E2_END) == H2_ERR) {
                invalidState(net, stream, E2_END);
                return;
            }
            stream->rx->endStream = 1;

        } else if (stream->state > HTTP_STATE_PARSED) {
            //  Trailer must have end stream
            sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Trailer headers must have end stream");
            return;
        }
        if (frame->type == HTTP2_CONT_FRAME && !net->parsingHeaders) {
            sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Unexpected continue frame");
            return;
        }
        net->parsingHeaders = 1;
        if (frame->flags & HTTP2_END_HEADERS_FLAG) {
            net->parsingHeaders = 0;
            parseHeaders2(q, stream);
        }
        /*
            Must only update for a successfully received frame
         */
        if (!net->error) {
            net->lastStreamID = frame->streamID;
        }
    }
}


/*
    Once the header frame and all continuation frames are received, they are joined into a single rx->headerPacket.
 */
static void parseHeaders2(HttpQueue *q, HttpStream *stream)
{
    HttpNet     *net;
    HttpPacket  *packet;
    HttpRx      *rx;

    net = stream->net;
    rx = stream->rx;
    packet = rx->headerPacket;

    while (httpGetPacketLength(packet) > 0 && !net->sentGoaway) {
        if (!parseHeader(q, stream, packet)) {
            sendReset(q, stream, HTTP2_STREAM_CLOSED, "Cannot parse headers");
            break;
        }
    }
    if (httpIsServer(net)) {
        if (!rx->method || !rx->scheme || !rx->uri) {
            sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Missing :method, :path or :scheme in request headers");
            return;
        }
    }
    if (!net->sentGoaway) {
        rx->protocol = sclone("HTTP/2.0");
        httpSetState(stream, HTTP_STATE_PARSED);
    }
}


/*
    Parse the next header item in the packet of headers
 */
static bool parseHeader(HttpQueue *q, HttpStream *stream, HttpPacket *packet)
{
    HttpNet     *net;
    MprBuf      *buf;
    MprKeyValue *kp;
    cchar       *name, *value;
    uchar       ch;
    int         index, max;

    net = stream->net;
    buf = packet->content;

    /*
        Decode the type of header record. It can be:
        1. Fully indexed header field.
        2. Literal header that should be added to the header table.
        3. Literal header without updating the header table.
     */
    ch = mprLookAtNextCharInBuf(buf);
    if ((ch >> 7) == 1) {
        /*
            Fully indexed header field
         */
        index = decodeInt(packet, 7);
        if ((kp = httpGetPackedHeader(net->rxHeaders, index)) == 0) {
            sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Bad header prefix");
            return 0;
        }
        if (!addHeaderToSet(stream, kp->key, kp->value)) {
            return 0;
        }

    } else if ((ch >> 6) == 1) {
        /*
            Literal header and add to index
         */
        if ((index = decodeInt(packet, 6)) < 0) {
            sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Bad header prefix");
            return 0;
        } else if (index > 0) {
            if ((kp = httpGetPackedHeader(net->rxHeaders, index)) == 0) {
                sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Unknown header index");
                return 0;
            }
            name = kp->key;
        } else {
            name = parseHeaderField(q, stream, packet);
        }
        value = parseHeaderField(q, stream, packet);
        if (!addHeaderToSet(stream, name, value)) {
            return 0;
        }
        if (httpAddPackedHeader(net->rxHeaders, name, value) < 0) {
            sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Cannot fit header in hpack table");
            return 0;
        }

    } else if ((ch >> 5) == 1) {
        /* Dynamic table max size update */
        if (stream->rx->headers->length) {
            sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Bad hpack dynamic table update after started parsing headers");
            return 0;
        }
        max = decodeInt(packet, 5);
        if (httpSetPackedHeadersMax(net->rxHeaders, max) < 0) {
            sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Cannot add indexed header");
            return 0;
        }

    } else /* if ((ch >> 4) == 1 || (ch >> 4) == 0)) */ {
        /* Literal header field without indexing */
        if ((index = decodeInt(packet, 4)) < 0) {
            sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Bad header prefix");
            return 0;
        } else if (index > 0) {
            if ((kp = httpGetPackedHeader(net->rxHeaders, index)) == 0) {
                sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Unknown header index");
                return 0;
            }
            name = kp->key;
        } else {
            name = parseHeaderField(q, stream, packet);
        }
        value = parseHeaderField(q, stream, packet);
        if (!addHeaderToSet(stream, name, value)) {
            return 0;
        }
    }
    return 1;
}


/*
    Parse a single header field
 */
static cchar *parseHeaderField(HttpQueue *q, HttpStream *stream, HttpPacket *packet)
{
    MprBuf      *buf;
    cchar       *value;
    int         huff, len;

    buf = packet->content;

    huff = ((uchar) mprLookAtNextCharInBuf(buf)) >> 7;
    len = decodeInt(packet, 7);
    if (len < 0 || len > mprGetBufLength(buf)) {
        sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Invalid header field length");
        return 0;
    }
    if (huff) {
        /*
            Huffman encoded
         */
        if ((value = httpHuffDecode((uchar*) mprGetBufStart(buf), len)) == 0) {
            sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Invalid encoded header field");
            return 0;
        }
    } else {
        /* Literal */
        value = snclone(buf->start, len);
    }
    mprAdjustBufStart(buf, len);
    return value;
}


/*
    Add a header key/value pair to the set of headers for the stream (stream)
 */
static bool addHeaderToSet(HttpStream *stream, cchar *key, cchar *value)
{
    HttpNet     *net;
    HttpRx      *rx;
    HttpLimits  *limits;
    ssize       len;

    net = stream ->net;
    rx = stream->rx;
    limits = stream->limits;

    if (!key || !value) {
        sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Invalid header missing key or value");
        return 0;
    }
    if (!validateHeader(key, value)) {
        sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Invalid header name/value");
        return 0;
    }

    if (key[0] == ':') {
        if (rx->seenRegularHeader) {
            sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Invalid pseudo header after regular header");
            return 0;
        }
        if (httpIsServer(net)) {
            if (key[1] == 'a' && smatch(key, ":authority")) {
                mprAddKey(stream->rx->headers, "host", value);

            } else if (key[1] == 'm' && smatch(key, ":method")) {
                if (rx->method || *value == '\0') {
                    sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Invalid or duplicate :method in headers");
                    return 0;
                } else {
                    rx->originalMethod = rx->method = supper(value);
                    httpParseMethod(stream);
                }

            } else if (key[1] == 'p' && smatch(key, ":path")) {
                if (rx->uri) {
                    sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Duplicate :path in headers");
                    return 0;
                } else {
                    len = slen(value);
                    if (*value == '\0') {
                        sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Bad request path");
                        return 0;
                    } else if (len >= limits->uriSize) {
                        httpLimitError(stream, HTTP_ABORT | HTTP_CODE_REQUEST_URL_TOO_LARGE,
                            "Bad request. URI too long. Length %zd vs limit %d", len, limits->uriSize);
                        return 0;
                    }
                    rx->uri = sclone(value);
                    if (!rx->originalUri) {
                        rx->originalUri = rx->uri;
                    }
                }

            } else if (smatch(key, ":scheme")) {
                if (rx->scheme) {
                    sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Invalid duplicate pseudo header");
                } else {
                    rx->scheme = sclone(value);
                }

            } else {
                sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Invalid pseudo header");
                return 0;
            }
        } else {
            if (key[1] == 's' && smatch(key, ":status")) {
                rx->status = atoi(value);
            } else {
                sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Invalid pseudo header");
                return 0;
            }
        }
    } else {
        rx->seenRegularHeader = 1;
        if (scaselessmatch(key, "connection")) {
            sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Invalid connection header");
        } else if (scaselessmatch(key, "te") && !smatch(value, "trailers")) {
            sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Invalid connection header");
        } else if (scaselessmatch(key, "set-cookie") || scaselessmatch(key, "cookie")) {
            mprAddDuplicateKey(rx->headers, key, value);
        } else if (scaselessmatch(key, "content-length")) {
            rx->http2ContentLength = stoi(value);
        } else {
            mprAddKey(rx->headers, key, value);
        }
    }
    return 1;
}


/*
    Briefly, do some validation
 */
static bool validateHeader(cchar *key, cchar *value)
{
    uchar   *cp, c;

    if (!key || *key == 0 || !value || !value) {
        return 0;
    }
    if (*key == ':') {
        key++;
    }
    for (cp = (uchar*) key; *cp; cp++) {
        c = *cp;
        if (('a' <= c && c <= 'z') || c == '-' || c == '_' || ('0' <= c && c <= '9')) {
            continue;
        }
        if (c == '\0' || c == '\n' || c == '\r' || c == ':' || ('A' <= c && c <= 'Z')) {
            mprLog("info http", 5, "Invalid header name %s", key);
            return 0;
        }
    }
    for (cp = (uchar*) value; *cp; cp++) {
        c = *cp;
        if (c == '\0' || c == '\n' || c == '\r') {
            mprLog("info http", 5, "Invalid header value %s", value);
            return 0;
        }
    }
    return 1;
}


/*
    Priority frames are not yet implemented. They are parsed but not validated or implemented.
 */
static void parsePriorityFrame(HttpQueue *q, HttpPacket *packet)
{
    HttpFrame   *frame;
    HttpStream  *stream;
    MprBuf      *buf;
    int         value;

    frame = packet->data;
    stream = frame->stream;

    //  Firefox sends priority before stream allocated (has streamID)
    if (/* !stream || */ q->net->parsingHeaders || httpGetPacketLength(packet) != HTTP2_PRIORITY_SIZE) {
        sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Bad priority frame");
        return;
    }
    buf = packet->content;
    value = mprGetUint32FromBuf(buf);
    frame->depend = value & 0x7fffffff;
    frame->exclusive = value >> 31;
    frame->weight = mprGetCharFromBuf(buf) + 1;

    if (frame->depend == frame->streamID) {
        sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Bad stream dependency in priority frame");
    }
}


/*
    Push frames are not yet implemented
 */
static void parsePushFrame(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;

    net = q->net;
    if (httpIsServer(net)) {
        sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Invalid PUSH frame from client");
    } else {
        sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "PUSH frames not implemented");
    }
#if FUTURE
    stream->h2State = H2_RESERVED;
#endif
}


/*
    Receive a ping frame
 */
static void parsePingFrame(HttpQueue *q, HttpPacket *packet)
{
    HttpFrame   *frame;

    frame = packet->data;

    if (q->net->sentGoaway) {
        sendGoAway(q, HTTP2_STREAM_CLOSED, "Network already closed");
        return;
    }
    if (frame->streamID) {
        sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Bad stream in ping frame");
        return;
    }
    if (!(frame->flags & HTTP2_ACK_FLAG)) {
        /* Resend the ping payload with the acknowledgement */
        sendFrame(q, defineFrame(q, packet, HTTP2_PING_FRAME, HTTP2_ACK_FLAG, 0));
    }
}


/*
    Peer is instructing the stream to be closed.
    5.1, 6.4 -- Must protocol error reset on an idle stream
    2..1 -- Must accept reset on 1/2 closed stream
 */
static void parseResetFrame(HttpQueue *q, HttpPacket *packet)
{
    HttpStream  *stream;
    HttpFrame   *frame;
    uint32      error;

    frame = packet->data;
    stream = frame->stream;

    /*
    if (stream->h2State == H2_IDLE) {
        sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Bad reset frame");
    } */
    if (httpGetPacketLength(packet) != sizeof(uint32) || frame->streamID == 0) {
        sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Bad reset frame");
        return;
    }
    if (!stream) {
        if (frame->streamID >= q->net->lastStreamID) {
            //  Reset on an idle stream
            sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Bad reset frame");
        } else {
            packet = httpCreatePacket(HTTP2_RESET_SIZE);
            mprPutUint32ToBuf(packet->content, HTTP2_STREAM_CLOSED);
            sendFrame(q, defineFrame(q, packet, HTTP2_RESET_FRAME, 0, frame->streamID));
        }
        return;
    }
    /*
        Received reset packets can race with the stream being closed
     */
    if (stream->h2State != H2_CLOSED) {
        error = mprGetUint32FromBuf(packet->content) & HTTP_STREAM_MASK;
        resetStream(frame->stream, "Stream reset by peer", error);
    }
}


/*
    Receive a GoAway which informs us that this network should not be used anymore.
 */
static void parseGoAwayFrame(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;
    HttpStream  *stream;
    MprBuf      *buf;
    cchar       *msg;
    ssize       len;
    int         error, lastStreamID, next;

    net = q->net;
    buf = packet->content;
    lastStreamID = mprGetUint32FromBuf(buf) & HTTP_STREAM_MASK;
    error = mprGetUint32FromBuf(buf);
    len = mprGetBufLength(buf);
    msg = len ? snclone(buf->start, len) : "";

    net->receivedGoaway = 1;

    httpLog(net->trace, "rx.http2", "context", "msg:Receive GoAway. %s, error:%d, lastStream:%d", msg, error, lastStreamID);

    for (ITERATE_ITEMS(net->streams, stream, next)) {
        if (stream->streamID > lastStreamID) {
            resetStream(stream, "Stream reset by peer", HTTP2_REFUSED_STREAM);
        }
    }
}


/*
    Receive a window update frame that increases the window size of permissible data to send.
    This is a credit based system for flow control of both the network and the stream.
 */
static void parseWindowFrame(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;
    HttpStream  *stream;
    HttpFrame   *frame;
    int         increment;

    net = q->net;
    frame = packet->data;
    increment = mprGetUint32FromBuf(packet->content);
    if (increment == 0) {
        sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Bad window frame size of zero");
        return;
    }
    stream = frame->stream;
    if (stream) {
        if (increment > (HTTP2_MAX_WINDOW - stream->outputq->window)) {
            sendReset(q, stream, HTTP2_FLOW_CONTROL_ERROR, "Invalid window update for stream %d", stream->streamID);
        } else {
            stream->outputq->window += increment;
            httpResumeQueue(stream->outputq, 0);
        }
    } else {
        if (frame->streamID) {
            if (frame->streamID >= net->lastStreamID) {
                sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Bad window frame");
            } else {
                //  Ignore window frames for recent streams that have been closed
            }
        } else if (increment > (HTTP2_MAX_WINDOW + 1 - net->outputq->window)) {
            sendGoAway(q, HTTP2_FLOW_CONTROL_ERROR, "Invalid window update for network");

        } else {
            net->outputq->window += increment;
            httpResumeQueue(net->outputq, 0);
        }
    }
}


/*
    Receive an application data frame
 */
static void parseDataFrame(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;
    HttpFrame   *frame;
    HttpStream  *stream;
    HttpLimits  *limits;
    MprBuf      *buf;
    ssize       len, padLen, payloadLen;
    int         padded;

    net = q->net;
    limits = net->limits;
    buf = packet->content;
    frame = packet->data;
    stream = frame->stream;

    if (!stream) {
        sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Received data frame without stream");
        return;
    }
    padded = frame->flags & HTTP2_PADDED_FLAG;
    if (padded) {
        payloadLen = mprGetBufLength(buf);
        padLen = (int) mprGetCharFromBuf(buf);
        if (padLen >= payloadLen) {
            sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Incorrect padding length");
            return;
        }
        mprAdjustBufEnd(buf, -padLen);
    }
    len = httpGetPacketLength(packet);

    processDataFrame(q, packet);

    /*
        Network flow control, do after processing the data frame incase the stream is now complete.
     */
    if (len > net->inputq->window) {
        sendGoAway(q, HTTP2_FLOW_CONTROL_ERROR, "Peer exceeded flow control window");
        return;
    }
    net->inputq->window -= len;
    assert(net->inputq->window >= 0);

    if (net->inputq->window <= net->inputq->packetSize) {
        /*
            Update the remote window size for network flow control
         */
        sendWindowFrame(q, 0, limits->window - net->inputq->window);
        net->inputq->window = limits->window;
    }

    /*
        Stream flow control
     */
    if (!stream->destroyed) {
        if (len > stream->inputq->window) {
            sendReset(q, stream, HTTP2_FLOW_CONTROL_ERROR, "Receive data exceeds window for stream");
            return;
        }
        stream->inputq->window -= len;

        if (stream->inputq->window <= net->inputq->packetSize) {
            /*
                Update the remote window size for stream flow control
             */
            sendWindowFrame(q, stream->streamID, limits->window - stream->inputq->window);
            stream->inputq->window = limits->window;
        }
    }
}


/*
    Process the frame and add to the stream input queue
 */
static void processDataFrame(HttpQueue *q, HttpPacket *packet)
{
    HttpFrame   *frame;
    HttpStream  *stream;
    HttpRx      *rx;
    ssize       len;

    frame = packet->data;
    stream = frame->stream;
    rx = stream->rx;

    len = httpGetPacketLength(packet);
    rx->dataFrameLength += len;
    if ((rx->http2ContentLength >= 0) &&
            ((rx->dataFrameLength > rx->http2ContentLength) || (rx->eof && rx->dataFrameLength < rx->http2ContentLength))) {
        sendGoAway(q->net->socketq, HTTP2_PROTOCOL_ERROR, "Data content vs content-length mismatch");

    } else if (httpGetPacketLength(packet) > 0) {
        httpPutPacket(stream->inputq, packet);
    }
}


/*
    Public API to terminate a network connection
 */
PUBLIC void httpSendGoAway(HttpNet *net, int status, cchar *fmt, ...)
{
    va_list     ap;
    cchar       *msg;

    va_start(ap, fmt);
    msg = sfmtv(fmt, ap);
    va_end(ap);
    sendGoAway(net->outputq, status, "%s", msg);
}


/*
    Send a ping packet. Some intermediaries or peers may use pings to keep a connection alive.
 */
PUBLIC bool sendPing(HttpQueue *q, uchar *data)
{
    HttpPacket  *packet;

    if ((packet = httpCreatePacket(HTTP2_WINDOW_SIZE)) == 0) {
        return 0;
    }
    mprPutBlockToBuf(packet->content, (char*) data, 64);
    sendFrame(q, defineFrame(q, packet, HTTP2_PING_FRAME, 0, 0));
    return 1;
}


/*
    Immediately close a stream. The peer is informed and the stream is disconnected.
 */
static void sendReset(HttpQueue *q, HttpStream *stream, int status, cchar *fmt, ...)
{
    HttpPacket  *packet;
    va_list     ap;
    char        *msg;

    assert(stream);

    if (!stream || stream->destroyed) {
        return;
    }
    if (setState(stream, E2_RESET) == H2_ERR) {
        invalidState(q->net, stream, E2_RESET);
        return;
    }
    if ((packet = httpCreatePacket(HTTP2_RESET_SIZE)) == 0) {
        return;
    }
    va_start(ap, fmt);
    msg = sfmtv(fmt, ap);
    httpLog(stream->trace, "tx.http2", "context", "msg:Send stream reset, stream:%d, status:%d, msg:%s",
        stream->streamID, status, msg);
    va_end(ap);

    mprPutUint32ToBuf(packet->content, status);
    sendFrame(q, defineFrame(q, packet, HTTP2_RESET_FRAME, 0, stream->streamID));

    httpError(stream, HTTP_CODE_COMMS_ERROR, "%s", msg);
}


/*
    Mark a stream as being reset (terminated). Called in response to receipt of GoAway or Reset frame.
 */
static void resetStream(HttpStream *stream, cchar *msg, int error)
{
    httpLog(stream->trace, "rx.http2", "context", "msg:Receive GoAway. %s, error:%d", msg, error);
    setState(stream, E2_RESET);
}


/*
    A stream must exchange settings before it is used
 */
static void sendSettings(HttpQueue *q)
{
    HttpNet     *net;
    HttpStream  *stream;

    net = q->net;
    stream = q->stream;

    if (!net->init) {
        sendSettingsFrame(q);
        net->init = 1;
    }
}


static void pickStreamNumber(HttpStream *stream)
{
    HttpNet     *net;

    net = stream->net;
    stream->streamID = net->nextStreamID;
    net->nextStreamID += 2;
    //  TEST
    if (stream->streamID >= HTTP2_MAX_STREAM) {
        httpError(stream, HTTP_CODE_BAD_REQUEST, "Stream ID overflow");
    }
}


/*
    Clients must send a preface before settings
 */
static void sendPreface(HttpQueue *q)
{
    HttpPacket  *packet;

    if ((packet = httpCreatePacket(HTTP2_PREFACE_SIZE)) == 0) {
        return;
    }
    packet->flags = 0;
    mprPutBlockToBuf(packet->content, HTTP2_PREFACE, HTTP2_PREFACE_SIZE);
    httpPutPacket(q->net->socketq, packet);
}


/*
    Send a settings packet before using the stream
 */
static void sendSettingsFrame(HttpQueue *q)
{
    HttpNet     *net;
    HttpPacket  *packet;
    ssize       size;

    net = q->net;
    if (!net->init && httpIsClient(net)) {
        sendPreface(q);
    }
    if ((packet = httpCreatePacket(HTTP2_SETTINGS_SIZE * 3)) == 0) {
        return;
    }
    mprPutUint16ToBuf(packet->content, HTTP2_MAX_STREAMS_SETTING);
    mprPutUint32ToBuf(packet->content, net->limits->streamsMax - net->ownStreams);

    mprPutUint16ToBuf(packet->content, HTTP2_INIT_WINDOW_SIZE_SETTING);
    mprPutUint32ToBuf(packet->content, (uint32) net->limits->window);

    mprPutUint16ToBuf(packet->content, HTTP2_MAX_FRAME_SIZE_SETTING);
    size = max(net->inputq->packetSize, HTTP2_MIN_FRAME_SIZE);
    mprPutUint32ToBuf(packet->content, (uint32) size);

    mprPutUint16ToBuf(packet->content, HTTP2_HEADER_TABLE_SIZE_SETTING);
    mprPutUint32ToBuf(packet->content, (uint32) HTTP2_TABLE_SIZE);

#if FUTURE
    mprPutUint32ToBuf(packet->content, HTTP2_TABLE_SIZE);
    mprPutUint16ToBuf(packet->content, HTTP2_MAX_HEADER_SIZE_SETTING);
    mprPutUint32ToBuf(packet->content, (uint32) net->limits->headerSize);
    mprPutUint16ToBuf(packet->content, HTTP2_ENABLE_PUSH_SETTING);
    mprPutUint32ToBuf(packet->content, 0);
#endif

    sendFrame(q, defineFrame(q, packet, HTTP2_SETTINGS_FRAME, 0, 0));
}


static void sendWindowFrame(HttpQueue *q, int streamID, ssize inc)
{
    HttpPacket  *packet;

    assert(inc >= 0);
    if ((packet = httpCreatePacket(HTTP2_WINDOW_SIZE)) == 0) {
        return;
    }
    mprPutUint32ToBuf(packet->content, (uint32) inc);
    sendFrame(q, defineFrame(q, packet, HTTP2_WINDOW_FRAME, 0, streamID));
}


/*
    Populate the HTTP headers as a HTTP/2 header packet in the given packet
    This is called from the tailFilter and the packet is then split into packetSize chunks and passed to outgoingHttp2.
    There, the relevant HTTP/2 packet type is assigned HTTP2_HEADERS_FRAME or HTTP2_CONT_FRAME.
 */
PUBLIC void httpCreateHeaders2(HttpQueue *q, HttpPacket *packet)
{
    HttpStream  *stream;
    HttpTx      *tx;
    MprKey      *kp;

    assert(packet->flags == HTTP_PACKET_HEADER);

    stream = packet->stream;
    tx = stream->tx;
    if (tx->flags & HTTP_TX_HEADERS_CREATED) {
        return;
    }
    tx->responded = 1;

    httpPrepareHeaders(stream);
    definePseudoHeaders(stream, packet);
    if (httpTracing(q->net)) {
        if (httpServerStream(stream)) {
            httpLog(stream->trace, "tx.http2", "result", "@%s %d %s\n",
                httpGetProtocol(stream->net), tx->status, httpLookupStatus(tx->status));
            httpLog(stream->trace, "tx.http2", "headers", "@%s", httpTraceHeaders(stream->tx->headers));
        } else {
            httpLog(stream->trace, "tx.http2", "request", "@%s %s %s\n",
                tx->method, tx->parsedUri->path, httpGetProtocol(stream->net));
            httpLog(stream->trace, "tx.http2", "headers", "@%s", httpTraceHeaders(stream->tx->headers));
        }
    }

    /*
        Not emitting any padding, dependencies or weights.
     */
    for (ITERATE_KEYS(tx->headers, kp)) {
        if (kp->key[0] == ':') {
            if (smatch(kp->key, ":status")) {
                switch (tx->status) {
                case 200:
                    encodeInt(packet, httpSetPrefix(7), 7, HTTP2_STATUS_200); break;
                case 204:
                    encodeInt(packet, httpSetPrefix(7), 7, HTTP2_STATUS_204); break;
                case 206:
                    encodeInt(packet, httpSetPrefix(7), 7, HTTP2_STATUS_206); break;
                case 304:
                    encodeInt(packet, httpSetPrefix(7), 7, HTTP2_STATUS_304); break;
                case 400:
                    encodeInt(packet, httpSetPrefix(7), 7, HTTP2_STATUS_400); break;
                case 404:
                    encodeInt(packet, httpSetPrefix(7), 7, HTTP2_STATUS_404); break;
                case 500:
                    encodeInt(packet, httpSetPrefix(7), 7, HTTP2_STATUS_500); break;
                default:
                    encodeHeader(stream, packet, kp->key, kp->data);
                }
            } else if (smatch(kp->key, ":method")){
                if (smatch(kp->data, "GET")) {
                    encodeInt(packet, httpSetPrefix(7), 7, HTTP2_METHOD_GET);
                } else if (smatch(kp->data, "POST")) {
                    encodeInt(packet, httpSetPrefix(7), 7, HTTP2_METHOD_POST);
                } else {
                    encodeHeader(stream, packet, kp->key, kp->data);
                }
            } else if (smatch(kp->key, ":path")) {
                if (smatch(kp->data, "/")) {
                    encodeInt(packet, httpSetPrefix(7), 7, HTTP2_PATH_ROOT);
                } else if (smatch(kp->data, "/index.html")) {
                    encodeInt(packet, httpSetPrefix(7), 7, HTTP2_PATH_INDEX);
                } else {
                    encodeHeader(stream, packet, kp->key, kp->data);
                }
            } else {
                encodeHeader(stream, packet, kp->key, kp->data);
            }
        }
    }
    for (ITERATE_KEYS(tx->headers, kp)) {
        if (kp->key[0] != ':') {
            encodeHeader(stream, packet, kp->key, kp->data);
        }
    }
    tx->flags |= HTTP_TX_HEADERS_CREATED;
}


/*
    Define the pseudo headers for status, method, scheme and authority.
 */
static void definePseudoHeaders(HttpStream *stream, HttpPacket *packet)
{
    HttpUri     *parsedUri;
    HttpTx      *tx;
    Http        *http;
    cchar       *authority, *path;

    http = stream->http;
    tx = stream->tx;

    if (httpServerStream(stream)) {
        httpAddHeaderString(stream, ":status", itos(tx->status));

    } else {
        authority = stream->rx->hostHeader ? stream->rx->hostHeader : stream->ip;
        httpAddHeaderString(stream, ":method", tx->method);
        httpAddHeaderString(stream, ":scheme", stream->secure ? "https" : "http");
        httpAddHeaderString(stream, ":authority", authority);

        parsedUri = tx->parsedUri;
        if (http->proxyHost && *http->proxyHost) {
            if (parsedUri->query && *parsedUri->query) {
                path = sfmt("http://%s:%d%s?%s", http->proxyHost, http->proxyPort, parsedUri->path, parsedUri->query);
            } else {
                path = sfmt("http://%s:%d%s", http->proxyHost, http->proxyPort, parsedUri->path);
            }
        } else {
            if (parsedUri->query && *parsedUri->query) {
                path = sfmt("%s?%s", parsedUri->path, parsedUri->query);
            } else {
                path = parsedUri->path;
            }
        }
        httpAddHeaderString(stream, ":path", path);
    }
}


/*
    Encode headers using the HPACK and huffman encoding.
 */
static void encodeHeader(HttpStream *stream, HttpPacket *packet, cchar *key, cchar *value)
{
    HttpNet     *net;
    int         index;
    bool        indexedValue;

    net = stream->net;
    stream->tx->headerSize = 0;

    if ((index = httpLookupPackedHeader(net->txHeaders, key, value, &indexedValue)) > 0) {
        if (indexedValue) {
            /* Fully indexed header key and value */
            encodeInt(packet, httpSetPrefix(7), 7, index);
        } else {
            encodeInt(packet, httpSetPrefix(6), 6, index);
            index = httpAddPackedHeader(net->txHeaders, key, value);
            encodeString(packet, value, 0);
        }
    } else {
        index = httpAddPackedHeader(net->txHeaders, key, value);
        encodeInt(packet, httpSetPrefix(6), 6, 0);
        encodeString(packet, key, 1);
        encodeString(packet, value, 0);
#if FUTURE
        //  no indexing
        encodeInt(packet, 0, 4, 0);
        encodeString(packet, key, 1);
        encodeString(packet, value, 0);
#endif
    }
}


/*
    Decode a HPACK encoded integer
 */
static int decodeInt(HttpPacket *packet, uint bits)
{
    MprBuf      *buf;
    uchar       *bp, *end, *start;
    uint        mask, shift, value;
    int         done;

    if (bits < 0 || bits > 8 || !packet || httpGetPacketLength(packet) == 0) {
        return MPR_ERR_BAD_STATE;
    }
    buf = packet->content;
    bp = start = (uchar*) mprGetBufStart(buf);
    end = (uchar*) mprGetBufEnd(buf);

    mask = httpGetPrefixMask(bits);
    value = *bp++ & mask;
    if (value == mask) {
        value = 0;
        shift = 0;
        do {
            if (bp >= end) {
                return MPR_ERR_BAD_STATE;
            }
            done = (*bp & 0x80) != 0x80;
            value += (*bp++ & 0x7f) << shift;
            shift += 7;
        } while (!done);
        value += mask;
    }
    mprAdjustBufStart(buf, bp - start);
    return value;
}


/*
    Encode an integer using HPACK.
 */
static void encodeInt(HttpPacket *packet, uint flags, uint bits, uint value)
{
    MprBuf      *buf;
    uint        mask;

    buf = packet->content;
    mask = (1 << bits) - 1;

    if (value < mask) {
        mprPutCharToBuf(buf, flags | value);
    } else {
        mprPutCharToBuf(buf, flags | mask);
        value -= mask;
        while (value >= 128) {
            mprPutCharToBuf(buf, value % 128 + 128);
            value /= 128;
        }
        mprPutCharToBuf(buf, (uchar) value);
    }
}


/*
    Encode a string using HPACK.
 */
static void encodeString(HttpPacket *packet, cchar *src, uint lower)
{
    MprBuf      *buf;
    cchar       *cp;
    char        *dp, *encoded;
    ssize       len, hlen, extra;

    buf = packet->content;
    len = slen(src);

    /*
        Encode the string in the buffer. Allow some extra space incase the huff encoding is bigger then src and
        some room after the end of the buffer for an encoded integer length.
     */
    extra = 16;
    if (mprGetBufSpace(buf) < (len + extra)) {
        mprGrowBuf(buf, (len + extra) - mprGetBufSpace(buf));
    }
    /*
        Leave room at the end of the buffer for an encoded integer.
     */
    encoded = mprGetBufEnd(buf) + (extra / 2);
    hlen = httpHuffEncode(src, slen(src), encoded, lower);
    assert((encoded + hlen) < buf->endbuf);
    assert(hlen < len);

    if (hlen > 0) {
        encodeInt(packet, HTTP2_ENCODE_HUFF, 7, (uint) hlen);
        memmove(mprGetBufEnd(buf), encoded, hlen);
        mprAdjustBufEnd(buf, hlen);
    } else {
        encodeInt(packet, 0, 7, (uint) len);
        if (lower) {
            for (cp = src, dp = mprGetBufEnd(buf); cp < &src[len]; cp++) {
                *dp++ = tolower(*cp);
            }
            assert(dp < buf->endbuf);
        } else {
            memmove(mprGetBufEnd(buf), src, len);
        }
        mprAdjustBufEnd(buf, len);
    }
    assert(buf->end < buf->endbuf);
}


/*
    Define a frame in the given packet. If null, allocate a packet.
 */
static HttpPacket *defineFrame(HttpQueue *q, HttpPacket *packet, int type, uchar flags, int stream)
{
    HttpNet     *net;
    MprBuf      *buf;
    ssize       length;
    cchar       *typeStr;

    net = q->net;
    if (!packet) {
        packet = httpCreatePacket(0);
    }
    packet->type = type;
    if ((buf = packet->prefix) == 0) {
        buf = packet->prefix = mprCreateBuf(HTTP2_FRAME_OVERHEAD, HTTP2_FRAME_OVERHEAD);
    }
    length = httpGetPacketLength(packet);
    assert(length <= HTTP2_MAX_FRAME_SIZE);

    /*
        Not yet supporting priority or weight
     */
    mprPutUint32ToBuf(buf, (((uint32) length) << 8 | type));
    mprPutCharToBuf(buf, flags);
    mprPutUint32ToBuf(buf, stream);

    typeStr = (type < HTTP2_MAX_FRAME) ? packetTypes[type] : "unknown";
    httpLog(net->trace, "tx.http2", "packet", "frame:%s, flags:%x, stream:%d, length:%zd,", typeStr, flags, stream, length);
    return packet;
}


/*
    Send a HTTP/2 packet downstream to the network
 */
static void sendFrame(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;

    net = q->net;
    if (packet && !net->sentGoaway && !net->eof && !net->error) {
        httpPutPacket(q->net->socketq, packet);
    }
}



/*
    Get or create a stream connection
 */
static HttpStream *getStream(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;
    HttpStream  *stream;
    HttpRx      *rx;
    HttpFrame   *frame;

    net = q->net;
    frame = packet->data;
    stream = frame->stream;
    frame = packet->data;

    if (!stream && httpIsServer(net)) {
        if (net->sentGoaway) {
            /*
                Ignore new streams as the network is going away. Don't send a reset, just ignore.
                sendReset(q, stream, HTTP2_REFUSED_STREAM, "Network is going away");
             */
            return 0;
        }
        if ((stream = httpCreateStream(net, 1)) == 0) {
            /* Memory error - centrally reported */
            return 0;
        }
        /*
            HTTP/2 does not use Content-Length or chunking. End of stream is detected by a frame with HTTP2_END_STREAM_FLAG
         */
        stream->rx->remainingContent = HTTP_UNLIMITED;
        stream->streamID = frame->streamID;
        frame->stream = stream;

        /*
            Servers create a new connection stream
         */
        if (mprGetListLength(net->streams) >= net->limits->streamsMax) {
            sendReset(q, stream, HTTP2_REFUSED_STREAM, "Too many streams for connection: %s %d/%d", net->ip,
                (int) mprGetListLength(net->streams), net->limits->streamsMax);
            return 0;
        }
    }
    if (frame->depend == frame->streamID) {
        sendReset(q, stream, HTTP2_PROTOCOL_ERROR, "Bad stream dependency");
        return 0;
    }
    if (frame->type == HTTP2_CONT_FRAME && (!stream->rx || !stream->rx->headerPacket)) {
        sendGoAway(q, HTTP2_PROTOCOL_ERROR, "Unexpected continue frame before headers frame");
        return 0;
    }
    rx = stream->rx;
    if (rx->headerPacket) {
        httpJoinPacket(rx->headerPacket, packet);
    } else {
        rx->headerPacket = packet;
    }
    packet->stream = stream;

    if (httpGetPacketLength(rx->headerPacket) > stream->limits->headerSize) {
        sendReset(q, stream, HTTP2_REFUSED_STREAM, "Header too big, length %ld, limit %ld",
            httpGetPacketLength(rx->headerPacket), stream->limits->headerSize);
        return 0;
    }
    return stream;
}


/*
    Find a HttpStream stream using the HTTP/2 stream ID
 */
static HttpStream *findStream(HttpNet *net, int streamID)
{
    HttpStream  *stream;
    int         next;

    for (ITERATE_ITEMS(net->streams, stream, next)) {
        if (stream->streamID == streamID) {
            stream->lastActivity = net->lastActivity;
            return stream;
        }
    }
    return 0;
}


/*
    Garbage collector callback.
 */
static void manageFrame(HttpFrame *frame, int flags)
{
    assert(frame);

    if (flags & MPR_MANAGE_MARK) {
        mprMark(frame->stream);
    }
}


static void restartSuspendedStreams(HttpNet *net) {
    HttpStream  *stream;
    HttpQueue   *q;
    int         next;

    for (ITERATE_ITEMS(net->streams, stream, next)) {
        q = stream->outputq;
        if (q->count && q->window > 0 && (q->flags & HTTP_QUEUE_SUSPENDED)) {
            httpResumeQueue(q, 0);
        }
    }
}


static void invalidState(HttpNet *net, HttpStream *stream, int event)
{
    cchar       *stateStr;

    stateStr = (stream->h2State < 0) ? "Error" : States[stream->h2State];
    sendGoAway(net->socketq, HTTP2_PROTOCOL_ERROR, "Invalid event on stream %d, \"%s\" (%d) in stream state \"%s\" (%d)",
        stream->streamID, Events[event], event, stateStr, stream->h2State);
}


/*
    Shutdown a network. This is not necessarily an error. Peer should open a new network.
    Continue processing current streams, but stop processing any new streams.
 */
static void sendGoAway(HttpQueue *q, int status, cchar *fmt, ...)
{
    HttpNet     *net;
    HttpPacket  *packet;
    HttpStream  *stream;
    MprBuf      *buf;
    va_list     ap;
    cchar       *msg;
    int         next;

    net = q->net;
    if (net->sentGoaway) {
        return;
    }
    if ((packet = httpCreatePacket(HTTP2_GOAWAY_SIZE)) == 0) {
        return;
    }
    va_start(ap, fmt);
    net->errorMsg = msg = sfmtv(fmt, ap);
    httpLog(net->trace, "tx.http2", "error", "Send network goAway, lastStream:%d, status:%d, msg:%s", net->lastStreamID, status, msg);
    va_end(ap);

    buf = packet->content;
    mprPutUint32ToBuf(buf, net->lastStreamID);
    mprPutUint32ToBuf(buf, status);
    if (msg && *msg) {
        mprPutStringToBuf(buf, msg);
    }
    sendFrame(q, defineFrame(q, packet, HTTP2_GOAWAY_FRAME, 0, 0));

    for (ITERATE_ITEMS(q->net->streams, stream, next)) {
        if (stream->streamID > net->lastStreamID) {
            resetStream(stream, "Stream terminated", HTTP2_REFUSED_STREAM);
            setState(stream, E2_GOAWAY);
        }
    }
    net->parsingHeaders = 0;
    net->sentGoaway = 1;
}


static int setState(HttpStream *stream, int event)
{
    HttpNet     *net;
    cchar       *stateStr, *type;
    int         state;

    net = stream->net;
    if (event < 0 || event >= E2_MAX) {
        return H2_ERR;
    }
    state = StateMatrix[event][stream->h2State];

    type = (state == H2_ERR) ? "error" : "packet";

    if (state != H2_SAME) {
        stateStr = (state < 0) ? "Error" : States[state];
        httpLog(net->trace, "rx.http2", type, "msg:State change for stream %d from \"%s\" (%d) to \"%s\" (%d) via event \"%s\" (%d)",
            stream->streamID, States[stream->h2State], stream->h2State, stateStr, state, Events[event], event);
        stream->h2State = state;
    }
    return state;
}

#endif /* ME_HTTP_HTTP2 */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/huff.c ************/

/*
    huff.c - Process HTTP request/response.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



#if ME_HTTP_HTTP2
/********************************** Forwards **********************************/

#define align(d, a) (((d) + (a - 1)) & ~(a - 1))

static int decodeBits(uchar *state, uchar *ending, uint bits, uchar **dstp);
static int decodeHuff(uchar *state, uchar *src, int len, uchar *dst, uint last);

/*********************************** Code *************************************/

#if FUTURE
#if ME_64
    #if (ME_ENDIAN == ME_LITTLE_ENDIAN)
        #define encodeBuf(dst, buf) \
            ((dst)[0] = (uchar) ((buf) >> 56), \
            (dst)[1] = (uchar) ((buf) >> 48),  \
            (dst)[2] = (uchar) ((buf) >> 40),  \
            (dst)[3] = (uchar) ((buf) >> 32),  \
            (dst)[4] = (uchar) ((buf) >> 24),  \
            (dst)[5] = (uchar) ((buf) >> 16),  \
            (dst)[6] = (uchar) ((buf) >> 8),   \
            (dst)[7] = (uchar)  (buf))
    #else /* BIG_ENDIAN */
        #define encodeBuf(dst, buf) (*(uint64 *) (dst) = (buf))
    #endif
#else /* 32 bit */
    #define encodeBuf(dst, buf) (*(uint *) (dst) = htonl(buf))
#endif
#endif /* FUTURE */

#define encodeBuf(dst, buf) (*(uint *) (dst) = htonl(buf))

typedef struct Decodes {
    uchar  next;
    uchar  emit;
    uchar  sym;
    uchar  ending;
} Decodes;

static Decodes decodes[256][16] = {
    {
        {0x04, 0x00, 0x00, 0x00}, {0x05, 0x00, 0x00, 0x00}, {0x07, 0x00, 0x00, 0x00}, {0x08, 0x00, 0x00, 0x00},
        {0x0b, 0x00, 0x00, 0x00}, {0x0c, 0x00, 0x00, 0x00}, {0x10, 0x00, 0x00, 0x00}, {0x13, 0x00, 0x00, 0x00},
        {0x19, 0x00, 0x00, 0x00}, {0x1c, 0x00, 0x00, 0x00}, {0x20, 0x00, 0x00, 0x00}, {0x23, 0x00, 0x00, 0x00},
        {0x2a, 0x00, 0x00, 0x00}, {0x31, 0x00, 0x00, 0x00}, {0x39, 0x00, 0x00, 0x00}, {0x40, 0x00, 0x00, 0x01}
    }, {
        {0x00, 0x01, 0x30, 0x01}, {0x00, 0x01, 0x31, 0x01}, {0x00, 0x01, 0x32, 0x01}, {0x00, 0x01, 0x61, 0x01},
        {0x00, 0x01, 0x63, 0x01}, {0x00, 0x01, 0x65, 0x01}, {0x00, 0x01, 0x69, 0x01}, {0x00, 0x01, 0x6f, 0x01},
        {0x00, 0x01, 0x73, 0x01}, {0x00, 0x01, 0x74, 0x01}, {0x0d, 0x00, 0x00, 0x00}, {0x0e, 0x00, 0x00, 0x00},
        {0x11, 0x00, 0x00, 0x00}, {0x12, 0x00, 0x00, 0x00}, {0x14, 0x00, 0x00, 0x00}, {0x15, 0x00, 0x00, 0x00}
    }, {
        {0x01, 0x01, 0x30, 0x00}, {0x16, 0x01, 0x30, 0x01}, {0x01, 0x01, 0x31, 0x00}, {0x16, 0x01, 0x31, 0x01},
        {0x01, 0x01, 0x32, 0x00}, {0x16, 0x01, 0x32, 0x01}, {0x01, 0x01, 0x61, 0x00}, {0x16, 0x01, 0x61, 0x01},
        {0x01, 0x01, 0x63, 0x00}, {0x16, 0x01, 0x63, 0x01}, {0x01, 0x01, 0x65, 0x00}, {0x16, 0x01, 0x65, 0x01},
        {0x01, 0x01, 0x69, 0x00}, {0x16, 0x01, 0x69, 0x01}, {0x01, 0x01, 0x6f, 0x00}, {0x16, 0x01, 0x6f, 0x01}
    }, {
        {0x02, 0x01, 0x30, 0x00}, {0x09, 0x01, 0x30, 0x00}, {0x17, 0x01, 0x30, 0x00}, {0x28, 0x01, 0x30, 0x01},
        {0x02, 0x01, 0x31, 0x00}, {0x09, 0x01, 0x31, 0x00}, {0x17, 0x01, 0x31, 0x00}, {0x28, 0x01, 0x31, 0x01},
        {0x02, 0x01, 0x32, 0x00}, {0x09, 0x01, 0x32, 0x00}, {0x17, 0x01, 0x32, 0x00}, {0x28, 0x01, 0x32, 0x01},
        {0x02, 0x01, 0x61, 0x00}, {0x09, 0x01, 0x61, 0x00}, {0x17, 0x01, 0x61, 0x00}, {0x28, 0x01, 0x61, 0x01}
    }, {
        {0x03, 0x01, 0x30, 0x00}, {0x06, 0x01, 0x30, 0x00}, {0x0a, 0x01, 0x30, 0x00}, {0x0f, 0x01, 0x30, 0x00},
        {0x18, 0x01, 0x30, 0x00}, {0x1f, 0x01, 0x30, 0x00}, {0x29, 0x01, 0x30, 0x00}, {0x38, 0x01, 0x30, 0x01},
        {0x03, 0x01, 0x31, 0x00}, {0x06, 0x01, 0x31, 0x00}, {0x0a, 0x01, 0x31, 0x00}, {0x0f, 0x01, 0x31, 0x00},
        {0x18, 0x01, 0x31, 0x00}, {0x1f, 0x01, 0x31, 0x00}, {0x29, 0x01, 0x31, 0x00}, {0x38, 0x01, 0x31, 0x01}
    }, {
        {0x03, 0x01, 0x32, 0x00}, {0x06, 0x01, 0x32, 0x00}, {0x0a, 0x01, 0x32, 0x00}, {0x0f, 0x01, 0x32, 0x00},
        {0x18, 0x01, 0x32, 0x00}, {0x1f, 0x01, 0x32, 0x00}, {0x29, 0x01, 0x32, 0x00}, {0x38, 0x01, 0x32, 0x01},
        {0x03, 0x01, 0x61, 0x00}, {0x06, 0x01, 0x61, 0x00}, {0x0a, 0x01, 0x61, 0x00}, {0x0f, 0x01, 0x61, 0x00},
        {0x18, 0x01, 0x61, 0x00}, {0x1f, 0x01, 0x61, 0x00}, {0x29, 0x01, 0x61, 0x00}, {0x38, 0x01, 0x61, 0x01}
    }, {
        {0x02, 0x01, 0x63, 0x00}, {0x09, 0x01, 0x63, 0x00}, {0x17, 0x01, 0x63, 0x00}, {0x28, 0x01, 0x63, 0x01},
        {0x02, 0x01, 0x65, 0x00}, {0x09, 0x01, 0x65, 0x00}, {0x17, 0x01, 0x65, 0x00}, {0x28, 0x01, 0x65, 0x01},
        {0x02, 0x01, 0x69, 0x00}, {0x09, 0x01, 0x69, 0x00}, {0x17, 0x01, 0x69, 0x00}, {0x28, 0x01, 0x69, 0x01},
        {0x02, 0x01, 0x6f, 0x00}, {0x09, 0x01, 0x6f, 0x00}, {0x17, 0x01, 0x6f, 0x00}, {0x28, 0x01, 0x6f, 0x01}
    }, {
        {0x03, 0x01, 0x63, 0x00}, {0x06, 0x01, 0x63, 0x00}, {0x0a, 0x01, 0x63, 0x00}, {0x0f, 0x01, 0x63, 0x00},
        {0x18, 0x01, 0x63, 0x00}, {0x1f, 0x01, 0x63, 0x00}, {0x29, 0x01, 0x63, 0x00}, {0x38, 0x01, 0x63, 0x01},
        {0x03, 0x01, 0x65, 0x00}, {0x06, 0x01, 0x65, 0x00}, {0x0a, 0x01, 0x65, 0x00}, {0x0f, 0x01, 0x65, 0x00},
        {0x18, 0x01, 0x65, 0x00}, {0x1f, 0x01, 0x65, 0x00}, {0x29, 0x01, 0x65, 0x00}, {0x38, 0x01, 0x65, 0x01}
    }, {
        {0x03, 0x01, 0x69, 0x00}, {0x06, 0x01, 0x69, 0x00}, {0x0a, 0x01, 0x69, 0x00}, {0x0f, 0x01, 0x69, 0x00},
        {0x18, 0x01, 0x69, 0x00}, {0x1f, 0x01, 0x69, 0x00}, {0x29, 0x01, 0x69, 0x00}, {0x38, 0x01, 0x69, 0x01},
        {0x03, 0x01, 0x6f, 0x00}, {0x06, 0x01, 0x6f, 0x00}, {0x0a, 0x01, 0x6f, 0x00}, {0x0f, 0x01, 0x6f, 0x00},
        {0x18, 0x01, 0x6f, 0x00}, {0x1f, 0x01, 0x6f, 0x00}, {0x29, 0x01, 0x6f, 0x00}, {0x38, 0x01, 0x6f, 0x01}
    }, {
        {0x01, 0x01, 0x73, 0x00}, {0x16, 0x01, 0x73, 0x01}, {0x01, 0x01, 0x74, 0x00}, {0x16, 0x01, 0x74, 0x01},
        {0x00, 0x01, 0x20, 0x01}, {0x00, 0x01, 0x25, 0x01}, {0x00, 0x01, 0x2d, 0x01}, {0x00, 0x01, 0x2e, 0x01},
        {0x00, 0x01, 0x2f, 0x01}, {0x00, 0x01, 0x33, 0x01}, {0x00, 0x01, 0x34, 0x01}, {0x00, 0x01, 0x35, 0x01},
        {0x00, 0x01, 0x36, 0x01}, {0x00, 0x01, 0x37, 0x01}, {0x00, 0x01, 0x38, 0x01}, {0x00, 0x01, 0x39, 0x01}
    }, {
        {0x02, 0x01, 0x73, 0x00}, {0x09, 0x01, 0x73, 0x00}, {0x17, 0x01, 0x73, 0x00}, {0x28, 0x01, 0x73, 0x01},
        {0x02, 0x01, 0x74, 0x00}, {0x09, 0x01, 0x74, 0x00}, {0x17, 0x01, 0x74, 0x00}, {0x28, 0x01, 0x74, 0x01},
        {0x01, 0x01, 0x20, 0x00}, {0x16, 0x01, 0x20, 0x01}, {0x01, 0x01, 0x25, 0x00}, {0x16, 0x01, 0x25, 0x01},
        {0x01, 0x01, 0x2d, 0x00}, {0x16, 0x01, 0x2d, 0x01}, {0x01, 0x01, 0x2e, 0x00}, {0x16, 0x01, 0x2e, 0x01}
    }, {
        {0x03, 0x01, 0x73, 0x00}, {0x06, 0x01, 0x73, 0x00}, {0x0a, 0x01, 0x73, 0x00}, {0x0f, 0x01, 0x73, 0x00},
        {0x18, 0x01, 0x73, 0x00}, {0x1f, 0x01, 0x73, 0x00}, {0x29, 0x01, 0x73, 0x00}, {0x38, 0x01, 0x73, 0x01},
        {0x03, 0x01, 0x74, 0x00}, {0x06, 0x01, 0x74, 0x00}, {0x0a, 0x01, 0x74, 0x00}, {0x0f, 0x01, 0x74, 0x00},
        {0x18, 0x01, 0x74, 0x00}, {0x1f, 0x01, 0x74, 0x00}, {0x29, 0x01, 0x74, 0x00}, {0x38, 0x01, 0x74, 0x01}
    }, {
        {0x02, 0x01, 0x20, 0x00}, {0x09, 0x01, 0x20, 0x00}, {0x17, 0x01, 0x20, 0x00}, {0x28, 0x01, 0x20, 0x01},
        {0x02, 0x01, 0x25, 0x00}, {0x09, 0x01, 0x25, 0x00}, {0x17, 0x01, 0x25, 0x00}, {0x28, 0x01, 0x25, 0x01},
        {0x02, 0x01, 0x2d, 0x00}, {0x09, 0x01, 0x2d, 0x00}, {0x17, 0x01, 0x2d, 0x00}, {0x28, 0x01, 0x2d, 0x01},
        {0x02, 0x01, 0x2e, 0x00}, {0x09, 0x01, 0x2e, 0x00}, {0x17, 0x01, 0x2e, 0x00}, {0x28, 0x01, 0x2e, 0x01}
    }, {
        {0x03, 0x01, 0x20, 0x00}, {0x06, 0x01, 0x20, 0x00}, {0x0a, 0x01, 0x20, 0x00}, {0x0f, 0x01, 0x20, 0x00},
        {0x18, 0x01, 0x20, 0x00}, {0x1f, 0x01, 0x20, 0x00}, {0x29, 0x01, 0x20, 0x00}, {0x38, 0x01, 0x20, 0x01},
        {0x03, 0x01, 0x25, 0x00}, {0x06, 0x01, 0x25, 0x00}, {0x0a, 0x01, 0x25, 0x00}, {0x0f, 0x01, 0x25, 0x00},
        {0x18, 0x01, 0x25, 0x00}, {0x1f, 0x01, 0x25, 0x00}, {0x29, 0x01, 0x25, 0x00}, {0x38, 0x01, 0x25, 0x01}
    }, {
        {0x03, 0x01, 0x2d, 0x00}, {0x06, 0x01, 0x2d, 0x00}, {0x0a, 0x01, 0x2d, 0x00}, {0x0f, 0x01, 0x2d, 0x00},
        {0x18, 0x01, 0x2d, 0x00}, {0x1f, 0x01, 0x2d, 0x00}, {0x29, 0x01, 0x2d, 0x00}, {0x38, 0x01, 0x2d, 0x01},
        {0x03, 0x01, 0x2e, 0x00}, {0x06, 0x01, 0x2e, 0x00}, {0x0a, 0x01, 0x2e, 0x00}, {0x0f, 0x01, 0x2e, 0x00},
        {0x18, 0x01, 0x2e, 0x00}, {0x1f, 0x01, 0x2e, 0x00}, {0x29, 0x01, 0x2e, 0x00}, {0x38, 0x01, 0x2e, 0x01}
    }, {
        {0x01, 0x01, 0x2f, 0x00}, {0x16, 0x01, 0x2f, 0x01}, {0x01, 0x01, 0x33, 0x00}, {0x16, 0x01, 0x33, 0x01},
        {0x01, 0x01, 0x34, 0x00}, {0x16, 0x01, 0x34, 0x01}, {0x01, 0x01, 0x35, 0x00}, {0x16, 0x01, 0x35, 0x01},
        {0x01, 0x01, 0x36, 0x00}, {0x16, 0x01, 0x36, 0x01}, {0x01, 0x01, 0x37, 0x00}, {0x16, 0x01, 0x37, 0x01},
        {0x01, 0x01, 0x38, 0x00}, {0x16, 0x01, 0x38, 0x01}, {0x01, 0x01, 0x39, 0x00}, {0x16, 0x01, 0x39, 0x01}
    }, {
        {0x02, 0x01, 0x2f, 0x00}, {0x09, 0x01, 0x2f, 0x00}, {0x17, 0x01, 0x2f, 0x00}, {0x28, 0x01, 0x2f, 0x01},
        {0x02, 0x01, 0x33, 0x00}, {0x09, 0x01, 0x33, 0x00}, {0x17, 0x01, 0x33, 0x00}, {0x28, 0x01, 0x33, 0x01},
        {0x02, 0x01, 0x34, 0x00}, {0x09, 0x01, 0x34, 0x00}, {0x17, 0x01, 0x34, 0x00}, {0x28, 0x01, 0x34, 0x01},
        {0x02, 0x01, 0x35, 0x00}, {0x09, 0x01, 0x35, 0x00}, {0x17, 0x01, 0x35, 0x00}, {0x28, 0x01, 0x35, 0x01}
    }, {
        {0x03, 0x01, 0x2f, 0x00}, {0x06, 0x01, 0x2f, 0x00}, {0x0a, 0x01, 0x2f, 0x00}, {0x0f, 0x01, 0x2f, 0x00},
        {0x18, 0x01, 0x2f, 0x00}, {0x1f, 0x01, 0x2f, 0x00}, {0x29, 0x01, 0x2f, 0x00}, {0x38, 0x01, 0x2f, 0x01},
        {0x03, 0x01, 0x33, 0x00}, {0x06, 0x01, 0x33, 0x00}, {0x0a, 0x01, 0x33, 0x00}, {0x0f, 0x01, 0x33, 0x00},
        {0x18, 0x01, 0x33, 0x00}, {0x1f, 0x01, 0x33, 0x00}, {0x29, 0x01, 0x33, 0x00}, {0x38, 0x01, 0x33, 0x01}
    }, {
        {0x03, 0x01, 0x34, 0x00}, {0x06, 0x01, 0x34, 0x00}, {0x0a, 0x01, 0x34, 0x00}, {0x0f, 0x01, 0x34, 0x00},
        {0x18, 0x01, 0x34, 0x00}, {0x1f, 0x01, 0x34, 0x00}, {0x29, 0x01, 0x34, 0x00}, {0x38, 0x01, 0x34, 0x01},
        {0x03, 0x01, 0x35, 0x00}, {0x06, 0x01, 0x35, 0x00}, {0x0a, 0x01, 0x35, 0x00}, {0x0f, 0x01, 0x35, 0x00},
        {0x18, 0x01, 0x35, 0x00}, {0x1f, 0x01, 0x35, 0x00}, {0x29, 0x01, 0x35, 0x00}, {0x38, 0x01, 0x35, 0x01}
    }, {
        {0x02, 0x01, 0x36, 0x00}, {0x09, 0x01, 0x36, 0x00}, {0x17, 0x01, 0x36, 0x00}, {0x28, 0x01, 0x36, 0x01},
        {0x02, 0x01, 0x37, 0x00}, {0x09, 0x01, 0x37, 0x00}, {0x17, 0x01, 0x37, 0x00}, {0x28, 0x01, 0x37, 0x01},
        {0x02, 0x01, 0x38, 0x00}, {0x09, 0x01, 0x38, 0x00}, {0x17, 0x01, 0x38, 0x00}, {0x28, 0x01, 0x38, 0x01},
        {0x02, 0x01, 0x39, 0x00}, {0x09, 0x01, 0x39, 0x00}, {0x17, 0x01, 0x39, 0x00}, {0x28, 0x01, 0x39, 0x01}
    }, {
        {0x03, 0x01, 0x36, 0x00}, {0x06, 0x01, 0x36, 0x00}, {0x0a, 0x01, 0x36, 0x00}, {0x0f, 0x01, 0x36, 0x00},
        {0x18, 0x01, 0x36, 0x00}, {0x1f, 0x01, 0x36, 0x00}, {0x29, 0x01, 0x36, 0x00}, {0x38, 0x01, 0x36, 0x01},
        {0x03, 0x01, 0x37, 0x00}, {0x06, 0x01, 0x37, 0x00}, {0x0a, 0x01, 0x37, 0x00}, {0x0f, 0x01, 0x37, 0x00},
        {0x18, 0x01, 0x37, 0x00}, {0x1f, 0x01, 0x37, 0x00}, {0x29, 0x01, 0x37, 0x00}, {0x38, 0x01, 0x37, 0x01}
    }, {
        {0x03, 0x01, 0x38, 0x00}, {0x06, 0x01, 0x38, 0x00}, {0x0a, 0x01, 0x38, 0x00}, {0x0f, 0x01, 0x38, 0x00},
        {0x18, 0x01, 0x38, 0x00}, {0x1f, 0x01, 0x38, 0x00}, {0x29, 0x01, 0x38, 0x00}, {0x38, 0x01, 0x38, 0x01},
        {0x03, 0x01, 0x39, 0x00}, {0x06, 0x01, 0x39, 0x00}, {0x0a, 0x01, 0x39, 0x00}, {0x0f, 0x01, 0x39, 0x00},
        {0x18, 0x01, 0x39, 0x00}, {0x1f, 0x01, 0x39, 0x00}, {0x29, 0x01, 0x39, 0x00}, {0x38, 0x01, 0x39, 0x01}
    }, {
        {0x1a, 0x00, 0x00, 0x00}, {0x1b, 0x00, 0x00, 0x00}, {0x1d, 0x00, 0x00, 0x00}, {0x1e, 0x00, 0x00, 0x00},
        {0x21, 0x00, 0x00, 0x00}, {0x22, 0x00, 0x00, 0x00}, {0x24, 0x00, 0x00, 0x00}, {0x25, 0x00, 0x00, 0x00},
        {0x2b, 0x00, 0x00, 0x00}, {0x2e, 0x00, 0x00, 0x00}, {0x32, 0x00, 0x00, 0x00}, {0x35, 0x00, 0x00, 0x00},
        {0x3a, 0x00, 0x00, 0x00}, {0x3d, 0x00, 0x00, 0x00}, {0x41, 0x00, 0x00, 0x00}, {0x44, 0x00, 0x00, 0x01}
    }, {
        {0x00, 0x01, 0x3d, 0x01}, {0x00, 0x01, 0x41, 0x01}, {0x00, 0x01, 0x5f, 0x01}, {0x00, 0x01, 0x62, 0x01},
        {0x00, 0x01, 0x64, 0x01}, {0x00, 0x01, 0x66, 0x01}, {0x00, 0x01, 0x67, 0x01}, {0x00, 0x01, 0x68, 0x01},
        {0x00, 0x01, 0x6c, 0x01}, {0x00, 0x01, 0x6d, 0x01}, {0x00, 0x01, 0x6e, 0x01}, {0x00, 0x01, 0x70, 0x01},
        {0x00, 0x01, 0x72, 0x01}, {0x00, 0x01, 0x75, 0x01}, {0x26, 0x00, 0x00, 0x00}, {0x27, 0x00, 0x00, 0x00}
    }, {
        {0x01, 0x01, 0x3d, 0x00}, {0x16, 0x01, 0x3d, 0x01}, {0x01, 0x01, 0x41, 0x00}, {0x16, 0x01, 0x41, 0x01},
        {0x01, 0x01, 0x5f, 0x00}, {0x16, 0x01, 0x5f, 0x01}, {0x01, 0x01, 0x62, 0x00}, {0x16, 0x01, 0x62, 0x01},
        {0x01, 0x01, 0x64, 0x00}, {0x16, 0x01, 0x64, 0x01}, {0x01, 0x01, 0x66, 0x00}, {0x16, 0x01, 0x66, 0x01},
        {0x01, 0x01, 0x67, 0x00}, {0x16, 0x01, 0x67, 0x01}, {0x01, 0x01, 0x68, 0x00}, {0x16, 0x01, 0x68, 0x01}
    }, {
        {0x02, 0x01, 0x3d, 0x00}, {0x09, 0x01, 0x3d, 0x00}, {0x17, 0x01, 0x3d, 0x00}, {0x28, 0x01, 0x3d, 0x01},
        {0x02, 0x01, 0x41, 0x00}, {0x09, 0x01, 0x41, 0x00}, {0x17, 0x01, 0x41, 0x00}, {0x28, 0x01, 0x41, 0x01},
        {0x02, 0x01, 0x5f, 0x00}, {0x09, 0x01, 0x5f, 0x00}, {0x17, 0x01, 0x5f, 0x00}, {0x28, 0x01, 0x5f, 0x01},
        {0x02, 0x01, 0x62, 0x00}, {0x09, 0x01, 0x62, 0x00}, {0x17, 0x01, 0x62, 0x00}, {0x28, 0x01, 0x62, 0x01}
    }, {
        {0x03, 0x01, 0x3d, 0x00}, {0x06, 0x01, 0x3d, 0x00}, {0x0a, 0x01, 0x3d, 0x00}, {0x0f, 0x01, 0x3d, 0x00},
        {0x18, 0x01, 0x3d, 0x00}, {0x1f, 0x01, 0x3d, 0x00}, {0x29, 0x01, 0x3d, 0x00}, {0x38, 0x01, 0x3d, 0x01},
        {0x03, 0x01, 0x41, 0x00}, {0x06, 0x01, 0x41, 0x00}, {0x0a, 0x01, 0x41, 0x00}, {0x0f, 0x01, 0x41, 0x00},
        {0x18, 0x01, 0x41, 0x00}, {0x1f, 0x01, 0x41, 0x00}, {0x29, 0x01, 0x41, 0x00}, {0x38, 0x01, 0x41, 0x01}
    }, {
        {0x03, 0x01, 0x5f, 0x00}, {0x06, 0x01, 0x5f, 0x00}, {0x0a, 0x01, 0x5f, 0x00}, {0x0f, 0x01, 0x5f, 0x00},
        {0x18, 0x01, 0x5f, 0x00}, {0x1f, 0x01, 0x5f, 0x00}, {0x29, 0x01, 0x5f, 0x00}, {0x38, 0x01, 0x5f, 0x01},
        {0x03, 0x01, 0x62, 0x00}, {0x06, 0x01, 0x62, 0x00}, {0x0a, 0x01, 0x62, 0x00}, {0x0f, 0x01, 0x62, 0x00},
        {0x18, 0x01, 0x62, 0x00}, {0x1f, 0x01, 0x62, 0x00}, {0x29, 0x01, 0x62, 0x00}, {0x38, 0x01, 0x62, 0x01}
    }, {
        {0x02, 0x01, 0x64, 0x00}, {0x09, 0x01, 0x64, 0x00}, {0x17, 0x01, 0x64, 0x00}, {0x28, 0x01, 0x64, 0x01},
        {0x02, 0x01, 0x66, 0x00}, {0x09, 0x01, 0x66, 0x00}, {0x17, 0x01, 0x66, 0x00}, {0x28, 0x01, 0x66, 0x01},
        {0x02, 0x01, 0x67, 0x00}, {0x09, 0x01, 0x67, 0x00}, {0x17, 0x01, 0x67, 0x00}, {0x28, 0x01, 0x67, 0x01},
        {0x02, 0x01, 0x68, 0x00}, {0x09, 0x01, 0x68, 0x00}, {0x17, 0x01, 0x68, 0x00}, {0x28, 0x01, 0x68, 0x01}
    }, {
        {0x03, 0x01, 0x64, 0x00}, {0x06, 0x01, 0x64, 0x00}, {0x0a, 0x01, 0x64, 0x00}, {0x0f, 0x01, 0x64, 0x00},
        {0x18, 0x01, 0x64, 0x00}, {0x1f, 0x01, 0x64, 0x00}, {0x29, 0x01, 0x64, 0x00}, {0x38, 0x01, 0x64, 0x01},
        {0x03, 0x01, 0x66, 0x00}, {0x06, 0x01, 0x66, 0x00}, {0x0a, 0x01, 0x66, 0x00}, {0x0f, 0x01, 0x66, 0x00},
        {0x18, 0x01, 0x66, 0x00}, {0x1f, 0x01, 0x66, 0x00}, {0x29, 0x01, 0x66, 0x00}, {0x38, 0x01, 0x66, 0x01}
    }, {
        {0x03, 0x01, 0x67, 0x00}, {0x06, 0x01, 0x67, 0x00}, {0x0a, 0x01, 0x67, 0x00}, {0x0f, 0x01, 0x67, 0x00},
        {0x18, 0x01, 0x67, 0x00}, {0x1f, 0x01, 0x67, 0x00}, {0x29, 0x01, 0x67, 0x00}, {0x38, 0x01, 0x67, 0x01},
        {0x03, 0x01, 0x68, 0x00}, {0x06, 0x01, 0x68, 0x00}, {0x0a, 0x01, 0x68, 0x00}, {0x0f, 0x01, 0x68, 0x00},
        {0x18, 0x01, 0x68, 0x00}, {0x1f, 0x01, 0x68, 0x00}, {0x29, 0x01, 0x68, 0x00}, {0x38, 0x01, 0x68, 0x01}
    }, {
        {0x01, 0x01, 0x6c, 0x00}, {0x16, 0x01, 0x6c, 0x01}, {0x01, 0x01, 0x6d, 0x00}, {0x16, 0x01, 0x6d, 0x01},
        {0x01, 0x01, 0x6e, 0x00}, {0x16, 0x01, 0x6e, 0x01}, {0x01, 0x01, 0x70, 0x00}, {0x16, 0x01, 0x70, 0x01},
        {0x01, 0x01, 0x72, 0x00}, {0x16, 0x01, 0x72, 0x01}, {0x01, 0x01, 0x75, 0x00}, {0x16, 0x01, 0x75, 0x01},
        {0x00, 0x01, 0x3a, 0x01}, {0x00, 0x01, 0x42, 0x01}, {0x00, 0x01, 0x43, 0x01}, {0x00, 0x01, 0x44, 0x01}
    }, {
        {0x02, 0x01, 0x6c, 0x00}, {0x09, 0x01, 0x6c, 0x00}, {0x17, 0x01, 0x6c, 0x00}, {0x28, 0x01, 0x6c, 0x01},
        {0x02, 0x01, 0x6d, 0x00}, {0x09, 0x01, 0x6d, 0x00}, {0x17, 0x01, 0x6d, 0x00}, {0x28, 0x01, 0x6d, 0x01},
        {0x02, 0x01, 0x6e, 0x00}, {0x09, 0x01, 0x6e, 0x00}, {0x17, 0x01, 0x6e, 0x00}, {0x28, 0x01, 0x6e, 0x01},
        {0x02, 0x01, 0x70, 0x00}, {0x09, 0x01, 0x70, 0x00}, {0x17, 0x01, 0x70, 0x00}, {0x28, 0x01, 0x70, 0x01}
    }, {
        {0x03, 0x01, 0x6c, 0x00}, {0x06, 0x01, 0x6c, 0x00}, {0x0a, 0x01, 0x6c, 0x00}, {0x0f, 0x01, 0x6c, 0x00},
        {0x18, 0x01, 0x6c, 0x00}, {0x1f, 0x01, 0x6c, 0x00}, {0x29, 0x01, 0x6c, 0x00}, {0x38, 0x01, 0x6c, 0x01},
        {0x03, 0x01, 0x6d, 0x00}, {0x06, 0x01, 0x6d, 0x00}, {0x0a, 0x01, 0x6d, 0x00}, {0x0f, 0x01, 0x6d, 0x00},
        {0x18, 0x01, 0x6d, 0x00}, {0x1f, 0x01, 0x6d, 0x00}, {0x29, 0x01, 0x6d, 0x00}, {0x38, 0x01, 0x6d, 0x01}
    }, {
        {0x03, 0x01, 0x6e, 0x00}, {0x06, 0x01, 0x6e, 0x00}, {0x0a, 0x01, 0x6e, 0x00}, {0x0f, 0x01, 0x6e, 0x00},
        {0x18, 0x01, 0x6e, 0x00}, {0x1f, 0x01, 0x6e, 0x00}, {0x29, 0x01, 0x6e, 0x00}, {0x38, 0x01, 0x6e, 0x01},
        {0x03, 0x01, 0x70, 0x00}, {0x06, 0x01, 0x70, 0x00}, {0x0a, 0x01, 0x70, 0x00}, {0x0f, 0x01, 0x70, 0x00},
        {0x18, 0x01, 0x70, 0x00}, {0x1f, 0x01, 0x70, 0x00}, {0x29, 0x01, 0x70, 0x00}, {0x38, 0x01, 0x70, 0x01}
    }, {
        {0x02, 0x01, 0x72, 0x00}, {0x09, 0x01, 0x72, 0x00}, {0x17, 0x01, 0x72, 0x00}, {0x28, 0x01, 0x72, 0x01},
        {0x02, 0x01, 0x75, 0x00}, {0x09, 0x01, 0x75, 0x00}, {0x17, 0x01, 0x75, 0x00}, {0x28, 0x01, 0x75, 0x01},
        {0x01, 0x01, 0x3a, 0x00}, {0x16, 0x01, 0x3a, 0x01}, {0x01, 0x01, 0x42, 0x00}, {0x16, 0x01, 0x42, 0x01},
        {0x01, 0x01, 0x43, 0x00}, {0x16, 0x01, 0x43, 0x01}, {0x01, 0x01, 0x44, 0x00}, {0x16, 0x01, 0x44, 0x01}
    }, {
        {0x03, 0x01, 0x72, 0x00}, {0x06, 0x01, 0x72, 0x00}, {0x0a, 0x01, 0x72, 0x00}, {0x0f, 0x01, 0x72, 0x00},
        {0x18, 0x01, 0x72, 0x00}, {0x1f, 0x01, 0x72, 0x00}, {0x29, 0x01, 0x72, 0x00}, {0x38, 0x01, 0x72, 0x01},
        {0x03, 0x01, 0x75, 0x00}, {0x06, 0x01, 0x75, 0x00}, {0x0a, 0x01, 0x75, 0x00}, {0x0f, 0x01, 0x75, 0x00},
        {0x18, 0x01, 0x75, 0x00}, {0x1f, 0x01, 0x75, 0x00}, {0x29, 0x01, 0x75, 0x00}, {0x38, 0x01, 0x75, 0x01}
    }, {
        {0x02, 0x01, 0x3a, 0x00}, {0x09, 0x01, 0x3a, 0x00}, {0x17, 0x01, 0x3a, 0x00}, {0x28, 0x01, 0x3a, 0x01},
        {0x02, 0x01, 0x42, 0x00}, {0x09, 0x01, 0x42, 0x00}, {0x17, 0x01, 0x42, 0x00}, {0x28, 0x01, 0x42, 0x01},
        {0x02, 0x01, 0x43, 0x00}, {0x09, 0x01, 0x43, 0x00}, {0x17, 0x01, 0x43, 0x00}, {0x28, 0x01, 0x43, 0x01},
        {0x02, 0x01, 0x44, 0x00}, {0x09, 0x01, 0x44, 0x00}, {0x17, 0x01, 0x44, 0x00}, {0x28, 0x01, 0x44, 0x01}
    }, {
        {0x03, 0x01, 0x3a, 0x00}, {0x06, 0x01, 0x3a, 0x00}, {0x0a, 0x01, 0x3a, 0x00}, {0x0f, 0x01, 0x3a, 0x00},
        {0x18, 0x01, 0x3a, 0x00}, {0x1f, 0x01, 0x3a, 0x00}, {0x29, 0x01, 0x3a, 0x00}, {0x38, 0x01, 0x3a, 0x01},
        {0x03, 0x01, 0x42, 0x00}, {0x06, 0x01, 0x42, 0x00}, {0x0a, 0x01, 0x42, 0x00}, {0x0f, 0x01, 0x42, 0x00},
        {0x18, 0x01, 0x42, 0x00}, {0x1f, 0x01, 0x42, 0x00}, {0x29, 0x01, 0x42, 0x00}, {0x38, 0x01, 0x42, 0x01}
    }, {
        {0x03, 0x01, 0x43, 0x00}, {0x06, 0x01, 0x43, 0x00}, {0x0a, 0x01, 0x43, 0x00}, {0x0f, 0x01, 0x43, 0x00},
        {0x18, 0x01, 0x43, 0x00}, {0x1f, 0x01, 0x43, 0x00}, {0x29, 0x01, 0x43, 0x00}, {0x38, 0x01, 0x43, 0x01},
        {0x03, 0x01, 0x44, 0x00}, {0x06, 0x01, 0x44, 0x00}, {0x0a, 0x01, 0x44, 0x00}, {0x0f, 0x01, 0x44, 0x00},
        {0x18, 0x01, 0x44, 0x00}, {0x1f, 0x01, 0x44, 0x00}, {0x29, 0x01, 0x44, 0x00}, {0x38, 0x01, 0x44, 0x01}
    }, {
        {0x2c, 0x00, 0x00, 0x00}, {0x2d, 0x00, 0x00, 0x00}, {0x2f, 0x00, 0x00, 0x00}, {0x30, 0x00, 0x00, 0x00},
        {0x33, 0x00, 0x00, 0x00}, {0x34, 0x00, 0x00, 0x00}, {0x36, 0x00, 0x00, 0x00}, {0x37, 0x00, 0x00, 0x00},
        {0x3b, 0x00, 0x00, 0x00}, {0x3c, 0x00, 0x00, 0x00}, {0x3e, 0x00, 0x00, 0x00}, {0x3f, 0x00, 0x00, 0x00},
        {0x42, 0x00, 0x00, 0x00}, {0x43, 0x00, 0x00, 0x00}, {0x45, 0x00, 0x00, 0x00}, {0x48, 0x00, 0x00, 0x01}
    }, {
        {0x00, 0x01, 0x45, 0x01}, {0x00, 0x01, 0x46, 0x01}, {0x00, 0x01, 0x47, 0x01}, {0x00, 0x01, 0x48, 0x01},
        {0x00, 0x01, 0x49, 0x01}, {0x00, 0x01, 0x4a, 0x01}, {0x00, 0x01, 0x4b, 0x01}, {0x00, 0x01, 0x4c, 0x01},
        {0x00, 0x01, 0x4d, 0x01}, {0x00, 0x01, 0x4e, 0x01}, {0x00, 0x01, 0x4f, 0x01}, {0x00, 0x01, 0x50, 0x01},
        {0x00, 0x01, 0x51, 0x01}, {0x00, 0x01, 0x52, 0x01}, {0x00, 0x01, 0x53, 0x01}, {0x00, 0x01, 0x54, 0x01}
    }, {
        {0x01, 0x01, 0x45, 0x00}, {0x16, 0x01, 0x45, 0x01}, {0x01, 0x01, 0x46, 0x00}, {0x16, 0x01, 0x46, 0x01},
        {0x01, 0x01, 0x47, 0x00}, {0x16, 0x01, 0x47, 0x01}, {0x01, 0x01, 0x48, 0x00}, {0x16, 0x01, 0x48, 0x01},
        {0x01, 0x01, 0x49, 0x00}, {0x16, 0x01, 0x49, 0x01}, {0x01, 0x01, 0x4a, 0x00}, {0x16, 0x01, 0x4a, 0x01},
        {0x01, 0x01, 0x4b, 0x00}, {0x16, 0x01, 0x4b, 0x01}, {0x01, 0x01, 0x4c, 0x00}, {0x16, 0x01, 0x4c, 0x01}
    }, {
        {0x02, 0x01, 0x45, 0x00}, {0x09, 0x01, 0x45, 0x00}, {0x17, 0x01, 0x45, 0x00}, {0x28, 0x01, 0x45, 0x01},
        {0x02, 0x01, 0x46, 0x00}, {0x09, 0x01, 0x46, 0x00}, {0x17, 0x01, 0x46, 0x00}, {0x28, 0x01, 0x46, 0x01},
        {0x02, 0x01, 0x47, 0x00}, {0x09, 0x01, 0x47, 0x00}, {0x17, 0x01, 0x47, 0x00}, {0x28, 0x01, 0x47, 0x01},
        {0x02, 0x01, 0x48, 0x00}, {0x09, 0x01, 0x48, 0x00}, {0x17, 0x01, 0x48, 0x00}, {0x28, 0x01, 0x48, 0x01}
    }, {
        {0x03, 0x01, 0x45, 0x00}, {0x06, 0x01, 0x45, 0x00}, {0x0a, 0x01, 0x45, 0x00}, {0x0f, 0x01, 0x45, 0x00},
        {0x18, 0x01, 0x45, 0x00}, {0x1f, 0x01, 0x45, 0x00}, {0x29, 0x01, 0x45, 0x00}, {0x38, 0x01, 0x45, 0x01},
        {0x03, 0x01, 0x46, 0x00}, {0x06, 0x01, 0x46, 0x00}, {0x0a, 0x01, 0x46, 0x00}, {0x0f, 0x01, 0x46, 0x00},
        {0x18, 0x01, 0x46, 0x00}, {0x1f, 0x01, 0x46, 0x00}, {0x29, 0x01, 0x46, 0x00}, {0x38, 0x01, 0x46, 0x01}
    }, {
        {0x03, 0x01, 0x47, 0x00}, {0x06, 0x01, 0x47, 0x00}, {0x0a, 0x01, 0x47, 0x00}, {0x0f, 0x01, 0x47, 0x00},
        {0x18, 0x01, 0x47, 0x00}, {0x1f, 0x01, 0x47, 0x00}, {0x29, 0x01, 0x47, 0x00}, {0x38, 0x01, 0x47, 0x01},
        {0x03, 0x01, 0x48, 0x00}, {0x06, 0x01, 0x48, 0x00}, {0x0a, 0x01, 0x48, 0x00}, {0x0f, 0x01, 0x48, 0x00},
        {0x18, 0x01, 0x48, 0x00}, {0x1f, 0x01, 0x48, 0x00}, {0x29, 0x01, 0x48, 0x00}, {0x38, 0x01, 0x48, 0x01}
    }, {
        {0x02, 0x01, 0x49, 0x00}, {0x09, 0x01, 0x49, 0x00}, {0x17, 0x01, 0x49, 0x00}, {0x28, 0x01, 0x49, 0x01},
        {0x02, 0x01, 0x4a, 0x00}, {0x09, 0x01, 0x4a, 0x00}, {0x17, 0x01, 0x4a, 0x00}, {0x28, 0x01, 0x4a, 0x01},
        {0x02, 0x01, 0x4b, 0x00}, {0x09, 0x01, 0x4b, 0x00}, {0x17, 0x01, 0x4b, 0x00}, {0x28, 0x01, 0x4b, 0x01},
        {0x02, 0x01, 0x4c, 0x00}, {0x09, 0x01, 0x4c, 0x00}, {0x17, 0x01, 0x4c, 0x00}, {0x28, 0x01, 0x4c, 0x01}
    }, {
        {0x03, 0x01, 0x49, 0x00}, {0x06, 0x01, 0x49, 0x00}, {0x0a, 0x01, 0x49, 0x00}, {0x0f, 0x01, 0x49, 0x00},
        {0x18, 0x01, 0x49, 0x00}, {0x1f, 0x01, 0x49, 0x00}, {0x29, 0x01, 0x49, 0x00}, {0x38, 0x01, 0x49, 0x01},
        {0x03, 0x01, 0x4a, 0x00}, {0x06, 0x01, 0x4a, 0x00}, {0x0a, 0x01, 0x4a, 0x00}, {0x0f, 0x01, 0x4a, 0x00},
        {0x18, 0x01, 0x4a, 0x00}, {0x1f, 0x01, 0x4a, 0x00}, {0x29, 0x01, 0x4a, 0x00}, {0x38, 0x01, 0x4a, 0x01}
    }, {
        {0x03, 0x01, 0x4b, 0x00}, {0x06, 0x01, 0x4b, 0x00}, {0x0a, 0x01, 0x4b, 0x00}, {0x0f, 0x01, 0x4b, 0x00},
        {0x18, 0x01, 0x4b, 0x00}, {0x1f, 0x01, 0x4b, 0x00}, {0x29, 0x01, 0x4b, 0x00}, {0x38, 0x01, 0x4b, 0x01},
        {0x03, 0x01, 0x4c, 0x00}, {0x06, 0x01, 0x4c, 0x00}, {0x0a, 0x01, 0x4c, 0x00}, {0x0f, 0x01, 0x4c, 0x00},
        {0x18, 0x01, 0x4c, 0x00}, {0x1f, 0x01, 0x4c, 0x00}, {0x29, 0x01, 0x4c, 0x00}, {0x38, 0x01, 0x4c, 0x01}
    }, {
        {0x01, 0x01, 0x4d, 0x00}, {0x16, 0x01, 0x4d, 0x01}, {0x01, 0x01, 0x4e, 0x00}, {0x16, 0x01, 0x4e, 0x01},
        {0x01, 0x01, 0x4f, 0x00}, {0x16, 0x01, 0x4f, 0x01}, {0x01, 0x01, 0x50, 0x00}, {0x16, 0x01, 0x50, 0x01},
        {0x01, 0x01, 0x51, 0x00}, {0x16, 0x01, 0x51, 0x01}, {0x01, 0x01, 0x52, 0x00}, {0x16, 0x01, 0x52, 0x01},
        {0x01, 0x01, 0x53, 0x00}, {0x16, 0x01, 0x53, 0x01}, {0x01, 0x01, 0x54, 0x00}, {0x16, 0x01, 0x54, 0x01}
    }, {
        {0x02, 0x01, 0x4d, 0x00}, {0x09, 0x01, 0x4d, 0x00}, {0x17, 0x01, 0x4d, 0x00}, {0x28, 0x01, 0x4d, 0x01},
        {0x02, 0x01, 0x4e, 0x00}, {0x09, 0x01, 0x4e, 0x00}, {0x17, 0x01, 0x4e, 0x00}, {0x28, 0x01, 0x4e, 0x01},
        {0x02, 0x01, 0x4f, 0x00}, {0x09, 0x01, 0x4f, 0x00}, {0x17, 0x01, 0x4f, 0x00}, {0x28, 0x01, 0x4f, 0x01},
        {0x02, 0x01, 0x50, 0x00}, {0x09, 0x01, 0x50, 0x00}, {0x17, 0x01, 0x50, 0x00}, {0x28, 0x01, 0x50, 0x01}
    }, {
        {0x03, 0x01, 0x4d, 0x00}, {0x06, 0x01, 0x4d, 0x00}, {0x0a, 0x01, 0x4d, 0x00}, {0x0f, 0x01, 0x4d, 0x00},
        {0x18, 0x01, 0x4d, 0x00}, {0x1f, 0x01, 0x4d, 0x00}, {0x29, 0x01, 0x4d, 0x00}, {0x38, 0x01, 0x4d, 0x01},
        {0x03, 0x01, 0x4e, 0x00}, {0x06, 0x01, 0x4e, 0x00}, {0x0a, 0x01, 0x4e, 0x00}, {0x0f, 0x01, 0x4e, 0x00},
        {0x18, 0x01, 0x4e, 0x00}, {0x1f, 0x01, 0x4e, 0x00}, {0x29, 0x01, 0x4e, 0x00}, {0x38, 0x01, 0x4e, 0x01}
    }, {
        {0x03, 0x01, 0x4f, 0x00}, {0x06, 0x01, 0x4f, 0x00}, {0x0a, 0x01, 0x4f, 0x00}, {0x0f, 0x01, 0x4f, 0x00},
        {0x18, 0x01, 0x4f, 0x00}, {0x1f, 0x01, 0x4f, 0x00}, {0x29, 0x01, 0x4f, 0x00}, {0x38, 0x01, 0x4f, 0x01},
        {0x03, 0x01, 0x50, 0x00}, {0x06, 0x01, 0x50, 0x00}, {0x0a, 0x01, 0x50, 0x00}, {0x0f, 0x01, 0x50, 0x00},
        {0x18, 0x01, 0x50, 0x00}, {0x1f, 0x01, 0x50, 0x00}, {0x29, 0x01, 0x50, 0x00}, {0x38, 0x01, 0x50, 0x01}
    }, {
        {0x02, 0x01, 0x51, 0x00}, {0x09, 0x01, 0x51, 0x00}, {0x17, 0x01, 0x51, 0x00}, {0x28, 0x01, 0x51, 0x01},
        {0x02, 0x01, 0x52, 0x00}, {0x09, 0x01, 0x52, 0x00}, {0x17, 0x01, 0x52, 0x00}, {0x28, 0x01, 0x52, 0x01},
        {0x02, 0x01, 0x53, 0x00}, {0x09, 0x01, 0x53, 0x00}, {0x17, 0x01, 0x53, 0x00}, {0x28, 0x01, 0x53, 0x01},
        {0x02, 0x01, 0x54, 0x00}, {0x09, 0x01, 0x54, 0x00}, {0x17, 0x01, 0x54, 0x00}, {0x28, 0x01, 0x54, 0x01}
    }, {
        {0x03, 0x01, 0x51, 0x00}, {0x06, 0x01, 0x51, 0x00}, {0x0a, 0x01, 0x51, 0x00}, {0x0f, 0x01, 0x51, 0x00},
        {0x18, 0x01, 0x51, 0x00}, {0x1f, 0x01, 0x51, 0x00}, {0x29, 0x01, 0x51, 0x00}, {0x38, 0x01, 0x51, 0x01},
        {0x03, 0x01, 0x52, 0x00}, {0x06, 0x01, 0x52, 0x00}, {0x0a, 0x01, 0x52, 0x00}, {0x0f, 0x01, 0x52, 0x00},
        {0x18, 0x01, 0x52, 0x00}, {0x1f, 0x01, 0x52, 0x00}, {0x29, 0x01, 0x52, 0x00}, {0x38, 0x01, 0x52, 0x01}
    }, {
        {0x03, 0x01, 0x53, 0x00}, {0x06, 0x01, 0x53, 0x00}, {0x0a, 0x01, 0x53, 0x00}, {0x0f, 0x01, 0x53, 0x00},
        {0x18, 0x01, 0x53, 0x00}, {0x1f, 0x01, 0x53, 0x00}, {0x29, 0x01, 0x53, 0x00}, {0x38, 0x01, 0x53, 0x01},
        {0x03, 0x01, 0x54, 0x00}, {0x06, 0x01, 0x54, 0x00}, {0x0a, 0x01, 0x54, 0x00}, {0x0f, 0x01, 0x54, 0x00},
        {0x18, 0x01, 0x54, 0x00}, {0x1f, 0x01, 0x54, 0x00}, {0x29, 0x01, 0x54, 0x00}, {0x38, 0x01, 0x54, 0x01}
    }, {
        {0x00, 0x01, 0x55, 0x01}, {0x00, 0x01, 0x56, 0x01}, {0x00, 0x01, 0x57, 0x01}, {0x00, 0x01, 0x59, 0x01},
        {0x00, 0x01, 0x6a, 0x01}, {0x00, 0x01, 0x6b, 0x01}, {0x00, 0x01, 0x71, 0x01}, {0x00, 0x01, 0x76, 0x01},
        {0x00, 0x01, 0x77, 0x01}, {0x00, 0x01, 0x78, 0x01}, {0x00, 0x01, 0x79, 0x01}, {0x00, 0x01, 0x7a, 0x01},
        {0x46, 0x00, 0x00, 0x00}, {0x47, 0x00, 0x00, 0x00}, {0x49, 0x00, 0x00, 0x00}, {0x4a, 0x00, 0x00, 0x01}
    }, {
        {0x01, 0x01, 0x55, 0x00}, {0x16, 0x01, 0x55, 0x01}, {0x01, 0x01, 0x56, 0x00}, {0x16, 0x01, 0x56, 0x01},
        {0x01, 0x01, 0x57, 0x00}, {0x16, 0x01, 0x57, 0x01}, {0x01, 0x01, 0x59, 0x00}, {0x16, 0x01, 0x59, 0x01},
        {0x01, 0x01, 0x6a, 0x00}, {0x16, 0x01, 0x6a, 0x01}, {0x01, 0x01, 0x6b, 0x00}, {0x16, 0x01, 0x6b, 0x01},
        {0x01, 0x01, 0x71, 0x00}, {0x16, 0x01, 0x71, 0x01}, {0x01, 0x01, 0x76, 0x00}, {0x16, 0x01, 0x76, 0x01}
    }, {
        {0x02, 0x01, 0x55, 0x00}, {0x09, 0x01, 0x55, 0x00}, {0x17, 0x01, 0x55, 0x00}, {0x28, 0x01, 0x55, 0x01},
        {0x02, 0x01, 0x56, 0x00}, {0x09, 0x01, 0x56, 0x00}, {0x17, 0x01, 0x56, 0x00}, {0x28, 0x01, 0x56, 0x01},
        {0x02, 0x01, 0x57, 0x00}, {0x09, 0x01, 0x57, 0x00}, {0x17, 0x01, 0x57, 0x00}, {0x28, 0x01, 0x57, 0x01},
        {0x02, 0x01, 0x59, 0x00}, {0x09, 0x01, 0x59, 0x00}, {0x17, 0x01, 0x59, 0x00}, {0x28, 0x01, 0x59, 0x01}
    }, {
        {0x03, 0x01, 0x55, 0x00}, {0x06, 0x01, 0x55, 0x00}, {0x0a, 0x01, 0x55, 0x00}, {0x0f, 0x01, 0x55, 0x00},
        {0x18, 0x01, 0x55, 0x00}, {0x1f, 0x01, 0x55, 0x00}, {0x29, 0x01, 0x55, 0x00}, {0x38, 0x01, 0x55, 0x01},
        {0x03, 0x01, 0x56, 0x00}, {0x06, 0x01, 0x56, 0x00}, {0x0a, 0x01, 0x56, 0x00}, {0x0f, 0x01, 0x56, 0x00},
        {0x18, 0x01, 0x56, 0x00}, {0x1f, 0x01, 0x56, 0x00}, {0x29, 0x01, 0x56, 0x00}, {0x38, 0x01, 0x56, 0x01}
    }, {
        {0x03, 0x01, 0x57, 0x00}, {0x06, 0x01, 0x57, 0x00}, {0x0a, 0x01, 0x57, 0x00}, {0x0f, 0x01, 0x57, 0x00},
        {0x18, 0x01, 0x57, 0x00}, {0x1f, 0x01, 0x57, 0x00}, {0x29, 0x01, 0x57, 0x00}, {0x38, 0x01, 0x57, 0x01},
        {0x03, 0x01, 0x59, 0x00}, {0x06, 0x01, 0x59, 0x00}, {0x0a, 0x01, 0x59, 0x00}, {0x0f, 0x01, 0x59, 0x00},
        {0x18, 0x01, 0x59, 0x00}, {0x1f, 0x01, 0x59, 0x00}, {0x29, 0x01, 0x59, 0x00}, {0x38, 0x01, 0x59, 0x01}
    }, {
        {0x02, 0x01, 0x6a, 0x00}, {0x09, 0x01, 0x6a, 0x00}, {0x17, 0x01, 0x6a, 0x00}, {0x28, 0x01, 0x6a, 0x01},
        {0x02, 0x01, 0x6b, 0x00}, {0x09, 0x01, 0x6b, 0x00}, {0x17, 0x01, 0x6b, 0x00}, {0x28, 0x01, 0x6b, 0x01},
        {0x02, 0x01, 0x71, 0x00}, {0x09, 0x01, 0x71, 0x00}, {0x17, 0x01, 0x71, 0x00}, {0x28, 0x01, 0x71, 0x01},
        {0x02, 0x01, 0x76, 0x00}, {0x09, 0x01, 0x76, 0x00}, {0x17, 0x01, 0x76, 0x00}, {0x28, 0x01, 0x76, 0x01}
    }, {
        {0x03, 0x01, 0x6a, 0x00}, {0x06, 0x01, 0x6a, 0x00}, {0x0a, 0x01, 0x6a, 0x00}, {0x0f, 0x01, 0x6a, 0x00},
        {0x18, 0x01, 0x6a, 0x00}, {0x1f, 0x01, 0x6a, 0x00}, {0x29, 0x01, 0x6a, 0x00}, {0x38, 0x01, 0x6a, 0x01},
        {0x03, 0x01, 0x6b, 0x00}, {0x06, 0x01, 0x6b, 0x00}, {0x0a, 0x01, 0x6b, 0x00}, {0x0f, 0x01, 0x6b, 0x00},
        {0x18, 0x01, 0x6b, 0x00}, {0x1f, 0x01, 0x6b, 0x00}, {0x29, 0x01, 0x6b, 0x00}, {0x38, 0x01, 0x6b, 0x01}
    }, {
        {0x03, 0x01, 0x71, 0x00}, {0x06, 0x01, 0x71, 0x00}, {0x0a, 0x01, 0x71, 0x00}, {0x0f, 0x01, 0x71, 0x00},
        {0x18, 0x01, 0x71, 0x00}, {0x1f, 0x01, 0x71, 0x00}, {0x29, 0x01, 0x71, 0x00}, {0x38, 0x01, 0x71, 0x01},
        {0x03, 0x01, 0x76, 0x00}, {0x06, 0x01, 0x76, 0x00}, {0x0a, 0x01, 0x76, 0x00}, {0x0f, 0x01, 0x76, 0x00},
        {0x18, 0x01, 0x76, 0x00}, {0x1f, 0x01, 0x76, 0x00}, {0x29, 0x01, 0x76, 0x00}, {0x38, 0x01, 0x76, 0x01}
    }, {
        {0x01, 0x01, 0x77, 0x00}, {0x16, 0x01, 0x77, 0x01}, {0x01, 0x01, 0x78, 0x00}, {0x16, 0x01, 0x78, 0x01},
        {0x01, 0x01, 0x79, 0x00}, {0x16, 0x01, 0x79, 0x01}, {0x01, 0x01, 0x7a, 0x00}, {0x16, 0x01, 0x7a, 0x01},
        {0x00, 0x01, 0x26, 0x01}, {0x00, 0x01, 0x2a, 0x01}, {0x00, 0x01, 0x2c, 0x01}, {0x00, 0x01, 0x3b, 0x01},
        {0x00, 0x01, 0x58, 0x01}, {0x00, 0x01, 0x5a, 0x01}, {0x4b, 0x00, 0x00, 0x00}, {0x4e, 0x00, 0x00, 0x01}
    }, {
        {0x02, 0x01, 0x77, 0x00}, {0x09, 0x01, 0x77, 0x00}, {0x17, 0x01, 0x77, 0x00}, {0x28, 0x01, 0x77, 0x01},
        {0x02, 0x01, 0x78, 0x00}, {0x09, 0x01, 0x78, 0x00}, {0x17, 0x01, 0x78, 0x00}, {0x28, 0x01, 0x78, 0x01},
        {0x02, 0x01, 0x79, 0x00}, {0x09, 0x01, 0x79, 0x00}, {0x17, 0x01, 0x79, 0x00}, {0x28, 0x01, 0x79, 0x01},
        {0x02, 0x01, 0x7a, 0x00}, {0x09, 0x01, 0x7a, 0x00}, {0x17, 0x01, 0x7a, 0x00}, {0x28, 0x01, 0x7a, 0x01}
    }, {
        {0x03, 0x01, 0x77, 0x00}, {0x06, 0x01, 0x77, 0x00}, {0x0a, 0x01, 0x77, 0x00}, {0x0f, 0x01, 0x77, 0x00},
        {0x18, 0x01, 0x77, 0x00}, {0x1f, 0x01, 0x77, 0x00}, {0x29, 0x01, 0x77, 0x00}, {0x38, 0x01, 0x77, 0x01},
        {0x03, 0x01, 0x78, 0x00}, {0x06, 0x01, 0x78, 0x00}, {0x0a, 0x01, 0x78, 0x00}, {0x0f, 0x01, 0x78, 0x00},
        {0x18, 0x01, 0x78, 0x00}, {0x1f, 0x01, 0x78, 0x00}, {0x29, 0x01, 0x78, 0x00}, {0x38, 0x01, 0x78, 0x01}
    }, {
        {0x03, 0x01, 0x79, 0x00}, {0x06, 0x01, 0x79, 0x00}, {0x0a, 0x01, 0x79, 0x00}, {0x0f, 0x01, 0x79, 0x00},
        {0x18, 0x01, 0x79, 0x00}, {0x1f, 0x01, 0x79, 0x00}, {0x29, 0x01, 0x79, 0x00}, {0x38, 0x01, 0x79, 0x01},
        {0x03, 0x01, 0x7a, 0x00}, {0x06, 0x01, 0x7a, 0x00}, {0x0a, 0x01, 0x7a, 0x00}, {0x0f, 0x01, 0x7a, 0x00},
        {0x18, 0x01, 0x7a, 0x00}, {0x1f, 0x01, 0x7a, 0x00}, {0x29, 0x01, 0x7a, 0x00}, {0x38, 0x01, 0x7a, 0x01}
    }, {
        {0x01, 0x01, 0x26, 0x00}, {0x16, 0x01, 0x26, 0x01}, {0x01, 0x01, 0x2a, 0x00}, {0x16, 0x01, 0x2a, 0x01},
        {0x01, 0x01, 0x2c, 0x00}, {0x16, 0x01, 0x2c, 0x01}, {0x01, 0x01, 0x3b, 0x00}, {0x16, 0x01, 0x3b, 0x01},
        {0x01, 0x01, 0x58, 0x00}, {0x16, 0x01, 0x58, 0x01}, {0x01, 0x01, 0x5a, 0x00}, {0x16, 0x01, 0x5a, 0x01},
        {0x4c, 0x00, 0x00, 0x00}, {0x4d, 0x00, 0x00, 0x00}, {0x4f, 0x00, 0x00, 0x00}, {0x51, 0x00, 0x00, 0x01}
    }, {
        {0x02, 0x01, 0x26, 0x00}, {0x09, 0x01, 0x26, 0x00}, {0x17, 0x01, 0x26, 0x00}, {0x28, 0x01, 0x26, 0x01},
        {0x02, 0x01, 0x2a, 0x00}, {0x09, 0x01, 0x2a, 0x00}, {0x17, 0x01, 0x2a, 0x00}, {0x28, 0x01, 0x2a, 0x01},
        {0x02, 0x01, 0x2c, 0x00}, {0x09, 0x01, 0x2c, 0x00}, {0x17, 0x01, 0x2c, 0x00}, {0x28, 0x01, 0x2c, 0x01},
        {0x02, 0x01, 0x3b, 0x00}, {0x09, 0x01, 0x3b, 0x00}, {0x17, 0x01, 0x3b, 0x00}, {0x28, 0x01, 0x3b, 0x01}
    }, {
        {0x03, 0x01, 0x26, 0x00}, {0x06, 0x01, 0x26, 0x00}, {0x0a, 0x01, 0x26, 0x00}, {0x0f, 0x01, 0x26, 0x00},
        {0x18, 0x01, 0x26, 0x00}, {0x1f, 0x01, 0x26, 0x00}, {0x29, 0x01, 0x26, 0x00}, {0x38, 0x01, 0x26, 0x01},
        {0x03, 0x01, 0x2a, 0x00}, {0x06, 0x01, 0x2a, 0x00}, {0x0a, 0x01, 0x2a, 0x00}, {0x0f, 0x01, 0x2a, 0x00},
        {0x18, 0x01, 0x2a, 0x00}, {0x1f, 0x01, 0x2a, 0x00}, {0x29, 0x01, 0x2a, 0x00}, {0x38, 0x01, 0x2a, 0x01}
    }, {
        {0x03, 0x01, 0x2c, 0x00}, {0x06, 0x01, 0x2c, 0x00}, {0x0a, 0x01, 0x2c, 0x00}, {0x0f, 0x01, 0x2c, 0x00},
        {0x18, 0x01, 0x2c, 0x00}, {0x1f, 0x01, 0x2c, 0x00}, {0x29, 0x01, 0x2c, 0x00}, {0x38, 0x01, 0x2c, 0x01},
        {0x03, 0x01, 0x3b, 0x00}, {0x06, 0x01, 0x3b, 0x00}, {0x0a, 0x01, 0x3b, 0x00}, {0x0f, 0x01, 0x3b, 0x00},
        {0x18, 0x01, 0x3b, 0x00}, {0x1f, 0x01, 0x3b, 0x00}, {0x29, 0x01, 0x3b, 0x00}, {0x38, 0x01, 0x3b, 0x01}
    }, {
        {0x02, 0x01, 0x58, 0x00}, {0x09, 0x01, 0x58, 0x00}, {0x17, 0x01, 0x58, 0x00}, {0x28, 0x01, 0x58, 0x01},
        {0x02, 0x01, 0x5a, 0x00}, {0x09, 0x01, 0x5a, 0x00}, {0x17, 0x01, 0x5a, 0x00}, {0x28, 0x01, 0x5a, 0x01},
        {0x00, 0x01, 0x21, 0x01}, {0x00, 0x01, 0x22, 0x01}, {0x00, 0x01, 0x28, 0x01}, {0x00, 0x01, 0x29, 0x01},
        {0x00, 0x01, 0x3f, 0x01}, {0x50, 0x00, 0x00, 0x00}, {0x52, 0x00, 0x00, 0x00}, {0x54, 0x00, 0x00, 0x01}
    }, {
        {0x03, 0x01, 0x58, 0x00}, {0x06, 0x01, 0x58, 0x00}, {0x0a, 0x01, 0x58, 0x00}, {0x0f, 0x01, 0x58, 0x00},
        {0x18, 0x01, 0x58, 0x00}, {0x1f, 0x01, 0x58, 0x00}, {0x29, 0x01, 0x58, 0x00}, {0x38, 0x01, 0x58, 0x01},
        {0x03, 0x01, 0x5a, 0x00}, {0x06, 0x01, 0x5a, 0x00}, {0x0a, 0x01, 0x5a, 0x00}, {0x0f, 0x01, 0x5a, 0x00},
        {0x18, 0x01, 0x5a, 0x00}, {0x1f, 0x01, 0x5a, 0x00}, {0x29, 0x01, 0x5a, 0x00}, {0x38, 0x01, 0x5a, 0x01}
    }, {
        {0x01, 0x01, 0x21, 0x00}, {0x16, 0x01, 0x21, 0x01}, {0x01, 0x01, 0x22, 0x00}, {0x16, 0x01, 0x22, 0x01},
        {0x01, 0x01, 0x28, 0x00}, {0x16, 0x01, 0x28, 0x01}, {0x01, 0x01, 0x29, 0x00}, {0x16, 0x01, 0x29, 0x01},
        {0x01, 0x01, 0x3f, 0x00}, {0x16, 0x01, 0x3f, 0x01}, {0x00, 0x01, 0x27, 0x01}, {0x00, 0x01, 0x2b, 0x01},
        {0x00, 0x01, 0x7c, 0x01}, {0x53, 0x00, 0x00, 0x00}, {0x55, 0x00, 0x00, 0x00}, {0x58, 0x00, 0x00, 0x01}
    }, {
        {0x02, 0x01, 0x21, 0x00}, {0x09, 0x01, 0x21, 0x00}, {0x17, 0x01, 0x21, 0x00}, {0x28, 0x01, 0x21, 0x01},
        {0x02, 0x01, 0x22, 0x00}, {0x09, 0x01, 0x22, 0x00}, {0x17, 0x01, 0x22, 0x00}, {0x28, 0x01, 0x22, 0x01},
        {0x02, 0x01, 0x28, 0x00}, {0x09, 0x01, 0x28, 0x00}, {0x17, 0x01, 0x28, 0x00}, {0x28, 0x01, 0x28, 0x01},
        {0x02, 0x01, 0x29, 0x00}, {0x09, 0x01, 0x29, 0x00}, {0x17, 0x01, 0x29, 0x00}, {0x28, 0x01, 0x29, 0x01}
    }, {
        {0x03, 0x01, 0x21, 0x00}, {0x06, 0x01, 0x21, 0x00}, {0x0a, 0x01, 0x21, 0x00}, {0x0f, 0x01, 0x21, 0x00},
        {0x18, 0x01, 0x21, 0x00}, {0x1f, 0x01, 0x21, 0x00}, {0x29, 0x01, 0x21, 0x00}, {0x38, 0x01, 0x21, 0x01},
        {0x03, 0x01, 0x22, 0x00}, {0x06, 0x01, 0x22, 0x00}, {0x0a, 0x01, 0x22, 0x00}, {0x0f, 0x01, 0x22, 0x00},
        {0x18, 0x01, 0x22, 0x00}, {0x1f, 0x01, 0x22, 0x00}, {0x29, 0x01, 0x22, 0x00}, {0x38, 0x01, 0x22, 0x01}
    }, {
        {0x03, 0x01, 0x28, 0x00}, {0x06, 0x01, 0x28, 0x00}, {0x0a, 0x01, 0x28, 0x00}, {0x0f, 0x01, 0x28, 0x00},
        {0x18, 0x01, 0x28, 0x00}, {0x1f, 0x01, 0x28, 0x00}, {0x29, 0x01, 0x28, 0x00}, {0x38, 0x01, 0x28, 0x01},
        {0x03, 0x01, 0x29, 0x00}, {0x06, 0x01, 0x29, 0x00}, {0x0a, 0x01, 0x29, 0x00}, {0x0f, 0x01, 0x29, 0x00},
        {0x18, 0x01, 0x29, 0x00}, {0x1f, 0x01, 0x29, 0x00}, {0x29, 0x01, 0x29, 0x00}, {0x38, 0x01, 0x29, 0x01}
    }, {
        {0x02, 0x01, 0x3f, 0x00}, {0x09, 0x01, 0x3f, 0x00}, {0x17, 0x01, 0x3f, 0x00}, {0x28, 0x01, 0x3f, 0x01},
        {0x01, 0x01, 0x27, 0x00}, {0x16, 0x01, 0x27, 0x01}, {0x01, 0x01, 0x2b, 0x00}, {0x16, 0x01, 0x2b, 0x01},
        {0x01, 0x01, 0x7c, 0x00}, {0x16, 0x01, 0x7c, 0x01}, {0x00, 0x01, 0x23, 0x01}, {0x00, 0x01, 0x3e, 0x01},
        {0x56, 0x00, 0x00, 0x00}, {0x57, 0x00, 0x00, 0x00}, {0x59, 0x00, 0x00, 0x00}, {0x5a, 0x00, 0x00, 0x01}
    }, {
        {0x03, 0x01, 0x3f, 0x00}, {0x06, 0x01, 0x3f, 0x00}, {0x0a, 0x01, 0x3f, 0x00}, {0x0f, 0x01, 0x3f, 0x00},
        {0x18, 0x01, 0x3f, 0x00}, {0x1f, 0x01, 0x3f, 0x00}, {0x29, 0x01, 0x3f, 0x00}, {0x38, 0x01, 0x3f, 0x01},
        {0x02, 0x01, 0x27, 0x00}, {0x09, 0x01, 0x27, 0x00}, {0x17, 0x01, 0x27, 0x00}, {0x28, 0x01, 0x27, 0x01},
        {0x02, 0x01, 0x2b, 0x00}, {0x09, 0x01, 0x2b, 0x00}, {0x17, 0x01, 0x2b, 0x00}, {0x28, 0x01, 0x2b, 0x01}
    }, {
        {0x03, 0x01, 0x27, 0x00}, {0x06, 0x01, 0x27, 0x00}, {0x0a, 0x01, 0x27, 0x00}, {0x0f, 0x01, 0x27, 0x00},
        {0x18, 0x01, 0x27, 0x00}, {0x1f, 0x01, 0x27, 0x00}, {0x29, 0x01, 0x27, 0x00}, {0x38, 0x01, 0x27, 0x01},
        {0x03, 0x01, 0x2b, 0x00}, {0x06, 0x01, 0x2b, 0x00}, {0x0a, 0x01, 0x2b, 0x00}, {0x0f, 0x01, 0x2b, 0x00},
        {0x18, 0x01, 0x2b, 0x00}, {0x1f, 0x01, 0x2b, 0x00}, {0x29, 0x01, 0x2b, 0x00}, {0x38, 0x01, 0x2b, 0x01}
    }, {
        {0x02, 0x01, 0x7c, 0x00}, {0x09, 0x01, 0x7c, 0x00}, {0x17, 0x01, 0x7c, 0x00}, {0x28, 0x01, 0x7c, 0x01},
        {0x01, 0x01, 0x23, 0x00}, {0x16, 0x01, 0x23, 0x01}, {0x01, 0x01, 0x3e, 0x00}, {0x16, 0x01, 0x3e, 0x01},
        {0x00, 0x01, 0x00, 0x01}, {0x00, 0x01, 0x24, 0x01}, {0x00, 0x01, 0x40, 0x01}, {0x00, 0x01, 0x5b, 0x01},
        {0x00, 0x01, 0x5d, 0x01}, {0x00, 0x01, 0x7e, 0x01}, {0x5b, 0x00, 0x00, 0x00}, {0x5c, 0x00, 0x00, 0x01}
    }, {
        {0x03, 0x01, 0x7c, 0x00}, {0x06, 0x01, 0x7c, 0x00}, {0x0a, 0x01, 0x7c, 0x00}, {0x0f, 0x01, 0x7c, 0x00},
        {0x18, 0x01, 0x7c, 0x00}, {0x1f, 0x01, 0x7c, 0x00}, {0x29, 0x01, 0x7c, 0x00}, {0x38, 0x01, 0x7c, 0x01},
        {0x02, 0x01, 0x23, 0x00}, {0x09, 0x01, 0x23, 0x00}, {0x17, 0x01, 0x23, 0x00}, {0x28, 0x01, 0x23, 0x01},
        {0x02, 0x01, 0x3e, 0x00}, {0x09, 0x01, 0x3e, 0x00}, {0x17, 0x01, 0x3e, 0x00}, {0x28, 0x01, 0x3e, 0x01}
    }, {
        {0x03, 0x01, 0x23, 0x00}, {0x06, 0x01, 0x23, 0x00}, {0x0a, 0x01, 0x23, 0x00}, {0x0f, 0x01, 0x23, 0x00},
        {0x18, 0x01, 0x23, 0x00}, {0x1f, 0x01, 0x23, 0x00}, {0x29, 0x01, 0x23, 0x00}, {0x38, 0x01, 0x23, 0x01},
        {0x03, 0x01, 0x3e, 0x00}, {0x06, 0x01, 0x3e, 0x00}, {0x0a, 0x01, 0x3e, 0x00}, {0x0f, 0x01, 0x3e, 0x00},
        {0x18, 0x01, 0x3e, 0x00}, {0x1f, 0x01, 0x3e, 0x00}, {0x29, 0x01, 0x3e, 0x00}, {0x38, 0x01, 0x3e, 0x01}
    }, {
        {0x01, 0x01, 0x00, 0x00}, {0x16, 0x01, 0x00, 0x01}, {0x01, 0x01, 0x24, 0x00}, {0x16, 0x01, 0x24, 0x01},
        {0x01, 0x01, 0x40, 0x00}, {0x16, 0x01, 0x40, 0x01}, {0x01, 0x01, 0x5b, 0x00}, {0x16, 0x01, 0x5b, 0x01},
        {0x01, 0x01, 0x5d, 0x00}, {0x16, 0x01, 0x5d, 0x01}, {0x01, 0x01, 0x7e, 0x00}, {0x16, 0x01, 0x7e, 0x01},
        {0x00, 0x01, 0x5e, 0x01}, {0x00, 0x01, 0x7d, 0x01}, {0x5d, 0x00, 0x00, 0x00}, {0x5e, 0x00, 0x00, 0x01}
    }, {
        {0x02, 0x01, 0x00, 0x00}, {0x09, 0x01, 0x00, 0x00}, {0x17, 0x01, 0x00, 0x00}, {0x28, 0x01, 0x00, 0x01},
        {0x02, 0x01, 0x24, 0x00}, {0x09, 0x01, 0x24, 0x00}, {0x17, 0x01, 0x24, 0x00}, {0x28, 0x01, 0x24, 0x01},
        {0x02, 0x01, 0x40, 0x00}, {0x09, 0x01, 0x40, 0x00}, {0x17, 0x01, 0x40, 0x00}, {0x28, 0x01, 0x40, 0x01},
        {0x02, 0x01, 0x5b, 0x00}, {0x09, 0x01, 0x5b, 0x00}, {0x17, 0x01, 0x5b, 0x00}, {0x28, 0x01, 0x5b, 0x01}
    }, {
        {0x03, 0x01, 0x00, 0x00}, {0x06, 0x01, 0x00, 0x00}, {0x0a, 0x01, 0x00, 0x00}, {0x0f, 0x01, 0x00, 0x00},
        {0x18, 0x01, 0x00, 0x00}, {0x1f, 0x01, 0x00, 0x00}, {0x29, 0x01, 0x00, 0x00}, {0x38, 0x01, 0x00, 0x01},
        {0x03, 0x01, 0x24, 0x00}, {0x06, 0x01, 0x24, 0x00}, {0x0a, 0x01, 0x24, 0x00}, {0x0f, 0x01, 0x24, 0x00},
        {0x18, 0x01, 0x24, 0x00}, {0x1f, 0x01, 0x24, 0x00}, {0x29, 0x01, 0x24, 0x00}, {0x38, 0x01, 0x24, 0x01}
    }, {
        {0x03, 0x01, 0x40, 0x00}, {0x06, 0x01, 0x40, 0x00}, {0x0a, 0x01, 0x40, 0x00}, {0x0f, 0x01, 0x40, 0x00},
        {0x18, 0x01, 0x40, 0x00}, {0x1f, 0x01, 0x40, 0x00}, {0x29, 0x01, 0x40, 0x00}, {0x38, 0x01, 0x40, 0x01},
        {0x03, 0x01, 0x5b, 0x00}, {0x06, 0x01, 0x5b, 0x00}, {0x0a, 0x01, 0x5b, 0x00}, {0x0f, 0x01, 0x5b, 0x00},
        {0x18, 0x01, 0x5b, 0x00}, {0x1f, 0x01, 0x5b, 0x00}, {0x29, 0x01, 0x5b, 0x00}, {0x38, 0x01, 0x5b, 0x01}
    }, {
        {0x02, 0x01, 0x5d, 0x00}, {0x09, 0x01, 0x5d, 0x00}, {0x17, 0x01, 0x5d, 0x00}, {0x28, 0x01, 0x5d, 0x01},
        {0x02, 0x01, 0x7e, 0x00}, {0x09, 0x01, 0x7e, 0x00}, {0x17, 0x01, 0x7e, 0x00}, {0x28, 0x01, 0x7e, 0x01},
        {0x01, 0x01, 0x5e, 0x00}, {0x16, 0x01, 0x5e, 0x01}, {0x01, 0x01, 0x7d, 0x00}, {0x16, 0x01, 0x7d, 0x01},
        {0x00, 0x01, 0x3c, 0x01}, {0x00, 0x01, 0x60, 0x01}, {0x00, 0x01, 0x7b, 0x01}, {0x5f, 0x00, 0x00, 0x01}
    }, {
        {0x03, 0x01, 0x5d, 0x00}, {0x06, 0x01, 0x5d, 0x00}, {0x0a, 0x01, 0x5d, 0x00}, {0x0f, 0x01, 0x5d, 0x00},
        {0x18, 0x01, 0x5d, 0x00}, {0x1f, 0x01, 0x5d, 0x00}, {0x29, 0x01, 0x5d, 0x00}, {0x38, 0x01, 0x5d, 0x01},
        {0x03, 0x01, 0x7e, 0x00}, {0x06, 0x01, 0x7e, 0x00}, {0x0a, 0x01, 0x7e, 0x00}, {0x0f, 0x01, 0x7e, 0x00},
        {0x18, 0x01, 0x7e, 0x00}, {0x1f, 0x01, 0x7e, 0x00}, {0x29, 0x01, 0x7e, 0x00}, {0x38, 0x01, 0x7e, 0x01}
    }, {
        {0x02, 0x01, 0x5e, 0x00}, {0x09, 0x01, 0x5e, 0x00}, {0x17, 0x01, 0x5e, 0x00}, {0x28, 0x01, 0x5e, 0x01},
        {0x02, 0x01, 0x7d, 0x00}, {0x09, 0x01, 0x7d, 0x00}, {0x17, 0x01, 0x7d, 0x00}, {0x28, 0x01, 0x7d, 0x01},
        {0x01, 0x01, 0x3c, 0x00}, {0x16, 0x01, 0x3c, 0x01}, {0x01, 0x01, 0x60, 0x00}, {0x16, 0x01, 0x60, 0x01},
        {0x01, 0x01, 0x7b, 0x00}, {0x16, 0x01, 0x7b, 0x01}, {0x60, 0x00, 0x00, 0x00}, {0x6e, 0x00, 0x00, 0x01}
    }, {
        {0x03, 0x01, 0x5e, 0x00}, {0x06, 0x01, 0x5e, 0x00}, {0x0a, 0x01, 0x5e, 0x00}, {0x0f, 0x01, 0x5e, 0x00},
        {0x18, 0x01, 0x5e, 0x00}, {0x1f, 0x01, 0x5e, 0x00}, {0x29, 0x01, 0x5e, 0x00}, {0x38, 0x01, 0x5e, 0x01},
        {0x03, 0x01, 0x7d, 0x00}, {0x06, 0x01, 0x7d, 0x00}, {0x0a, 0x01, 0x7d, 0x00}, {0x0f, 0x01, 0x7d, 0x00},
        {0x18, 0x01, 0x7d, 0x00}, {0x1f, 0x01, 0x7d, 0x00}, {0x29, 0x01, 0x7d, 0x00}, {0x38, 0x01, 0x7d, 0x01}
    }, {
        {0x02, 0x01, 0x3c, 0x00}, {0x09, 0x01, 0x3c, 0x00}, {0x17, 0x01, 0x3c, 0x00}, {0x28, 0x01, 0x3c, 0x01},
        {0x02, 0x01, 0x60, 0x00}, {0x09, 0x01, 0x60, 0x00}, {0x17, 0x01, 0x60, 0x00}, {0x28, 0x01, 0x60, 0x01},
        {0x02, 0x01, 0x7b, 0x00}, {0x09, 0x01, 0x7b, 0x00}, {0x17, 0x01, 0x7b, 0x00}, {0x28, 0x01, 0x7b, 0x01},
        {0x61, 0x00, 0x00, 0x00}, {0x65, 0x00, 0x00, 0x00}, {0x6f, 0x00, 0x00, 0x00}, {0x85, 0x00, 0x00, 0x01}
    }, {
        {0x03, 0x01, 0x3c, 0x00}, {0x06, 0x01, 0x3c, 0x00}, {0x0a, 0x01, 0x3c, 0x00}, {0x0f, 0x01, 0x3c, 0x00},
        {0x18, 0x01, 0x3c, 0x00}, {0x1f, 0x01, 0x3c, 0x00}, {0x29, 0x01, 0x3c, 0x00}, {0x38, 0x01, 0x3c, 0x01},
        {0x03, 0x01, 0x60, 0x00}, {0x06, 0x01, 0x60, 0x00}, {0x0a, 0x01, 0x60, 0x00}, {0x0f, 0x01, 0x60, 0x00},
        {0x18, 0x01, 0x60, 0x00}, {0x1f, 0x01, 0x60, 0x00}, {0x29, 0x01, 0x60, 0x00}, {0x38, 0x01, 0x60, 0x01}
    }, {
        {0x03, 0x01, 0x7b, 0x00}, {0x06, 0x01, 0x7b, 0x00}, {0x0a, 0x01, 0x7b, 0x00}, {0x0f, 0x01, 0x7b, 0x00},
        {0x18, 0x01, 0x7b, 0x00}, {0x1f, 0x01, 0x7b, 0x00}, {0x29, 0x01, 0x7b, 0x00}, {0x38, 0x01, 0x7b, 0x01},
        {0x62, 0x00, 0x00, 0x00}, {0x63, 0x00, 0x00, 0x00}, {0x66, 0x00, 0x00, 0x00}, {0x69, 0x00, 0x00, 0x00},
        {0x70, 0x00, 0x00, 0x00}, {0x77, 0x00, 0x00, 0x00}, {0x86, 0x00, 0x00, 0x00}, {0x99, 0x00, 0x00, 0x01}
    }, {
        {0x00, 0x01, 0x5c, 0x01}, {0x00, 0x01, 0xc3, 0x01}, {0x00, 0x01, 0xd0, 0x01}, {0x64, 0x00, 0x00, 0x00},
        {0x67, 0x00, 0x00, 0x00}, {0x68, 0x00, 0x00, 0x00}, {0x6a, 0x00, 0x00, 0x00}, {0x6b, 0x00, 0x00, 0x00},
        {0x71, 0x00, 0x00, 0x00}, {0x74, 0x00, 0x00, 0x00}, {0x78, 0x00, 0x00, 0x00}, {0x7e, 0x00, 0x00, 0x00},
        {0x87, 0x00, 0x00, 0x00}, {0x8e, 0x00, 0x00, 0x00}, {0x9a, 0x00, 0x00, 0x00}, {0xa9, 0x00, 0x00, 0x01}
    }, {
        {0x01, 0x01, 0x5c, 0x00}, {0x16, 0x01, 0x5c, 0x01}, {0x01, 0x01, 0xc3, 0x00}, {0x16, 0x01, 0xc3, 0x01},
        {0x01, 0x01, 0xd0, 0x00}, {0x16, 0x01, 0xd0, 0x01}, {0x00, 0x01, 0x80, 0x01}, {0x00, 0x01, 0x82, 0x01},
        {0x00, 0x01, 0x83, 0x01}, {0x00, 0x01, 0xa2, 0x01}, {0x00, 0x01, 0xb8, 0x01}, {0x00, 0x01, 0xc2, 0x01},
        {0x00, 0x01, 0xe0, 0x01}, {0x00, 0x01, 0xe2, 0x01}, {0x6c, 0x00, 0x00, 0x00}, {0x6d, 0x00, 0x00, 0x00}
    }, {
        {0x02, 0x01, 0x5c, 0x00}, {0x09, 0x01, 0x5c, 0x00}, {0x17, 0x01, 0x5c, 0x00}, {0x28, 0x01, 0x5c, 0x01},
        {0x02, 0x01, 0xc3, 0x00}, {0x09, 0x01, 0xc3, 0x00}, {0x17, 0x01, 0xc3, 0x00}, {0x28, 0x01, 0xc3, 0x01},
        {0x02, 0x01, 0xd0, 0x00}, {0x09, 0x01, 0xd0, 0x00}, {0x17, 0x01, 0xd0, 0x00}, {0x28, 0x01, 0xd0, 0x01},
        {0x01, 0x01, 0x80, 0x00}, {0x16, 0x01, 0x80, 0x01}, {0x01, 0x01, 0x82, 0x00}, {0x16, 0x01, 0x82, 0x01}
    }, {
        {0x03, 0x01, 0x5c, 0x00}, {0x06, 0x01, 0x5c, 0x00}, {0x0a, 0x01, 0x5c, 0x00}, {0x0f, 0x01, 0x5c, 0x00},
        {0x18, 0x01, 0x5c, 0x00}, {0x1f, 0x01, 0x5c, 0x00}, {0x29, 0x01, 0x5c, 0x00}, {0x38, 0x01, 0x5c, 0x01},
        {0x03, 0x01, 0xc3, 0x00}, {0x06, 0x01, 0xc3, 0x00}, {0x0a, 0x01, 0xc3, 0x00}, {0x0f, 0x01, 0xc3, 0x00},
        {0x18, 0x01, 0xc3, 0x00}, {0x1f, 0x01, 0xc3, 0x00}, {0x29, 0x01, 0xc3, 0x00}, {0x38, 0x01, 0xc3, 0x01}
    }, {
        {0x03, 0x01, 0xd0, 0x00}, {0x06, 0x01, 0xd0, 0x00}, {0x0a, 0x01, 0xd0, 0x00}, {0x0f, 0x01, 0xd0, 0x00},
        {0x18, 0x01, 0xd0, 0x00}, {0x1f, 0x01, 0xd0, 0x00}, {0x29, 0x01, 0xd0, 0x00}, {0x38, 0x01, 0xd0, 0x01},
        {0x02, 0x01, 0x80, 0x00}, {0x09, 0x01, 0x80, 0x00}, {0x17, 0x01, 0x80, 0x00}, {0x28, 0x01, 0x80, 0x01},
        {0x02, 0x01, 0x82, 0x00}, {0x09, 0x01, 0x82, 0x00}, {0x17, 0x01, 0x82, 0x00}, {0x28, 0x01, 0x82, 0x01}
    }, {
        {0x03, 0x01, 0x80, 0x00}, {0x06, 0x01, 0x80, 0x00}, {0x0a, 0x01, 0x80, 0x00}, {0x0f, 0x01, 0x80, 0x00},
        {0x18, 0x01, 0x80, 0x00}, {0x1f, 0x01, 0x80, 0x00}, {0x29, 0x01, 0x80, 0x00}, {0x38, 0x01, 0x80, 0x01},
        {0x03, 0x01, 0x82, 0x00}, {0x06, 0x01, 0x82, 0x00}, {0x0a, 0x01, 0x82, 0x00}, {0x0f, 0x01, 0x82, 0x00},
        {0x18, 0x01, 0x82, 0x00}, {0x1f, 0x01, 0x82, 0x00}, {0x29, 0x01, 0x82, 0x00}, {0x38, 0x01, 0x82, 0x01}
    }, {
        {0x01, 0x01, 0x83, 0x00}, {0x16, 0x01, 0x83, 0x01}, {0x01, 0x01, 0xa2, 0x00}, {0x16, 0x01, 0xa2, 0x01},
        {0x01, 0x01, 0xb8, 0x00}, {0x16, 0x01, 0xb8, 0x01}, {0x01, 0x01, 0xc2, 0x00}, {0x16, 0x01, 0xc2, 0x01},
        {0x01, 0x01, 0xe0, 0x00}, {0x16, 0x01, 0xe0, 0x01}, {0x01, 0x01, 0xe2, 0x00}, {0x16, 0x01, 0xe2, 0x01},
        {0x00, 0x01, 0x99, 0x01}, {0x00, 0x01, 0xa1, 0x01}, {0x00, 0x01, 0xa7, 0x01}, {0x00, 0x01, 0xac, 0x01}
    }, {
        {0x02, 0x01, 0x83, 0x00}, {0x09, 0x01, 0x83, 0x00}, {0x17, 0x01, 0x83, 0x00}, {0x28, 0x01, 0x83, 0x01},
        {0x02, 0x01, 0xa2, 0x00}, {0x09, 0x01, 0xa2, 0x00}, {0x17, 0x01, 0xa2, 0x00}, {0x28, 0x01, 0xa2, 0x01},
        {0x02, 0x01, 0xb8, 0x00}, {0x09, 0x01, 0xb8, 0x00}, {0x17, 0x01, 0xb8, 0x00}, {0x28, 0x01, 0xb8, 0x01},
        {0x02, 0x01, 0xc2, 0x00}, {0x09, 0x01, 0xc2, 0x00}, {0x17, 0x01, 0xc2, 0x00}, {0x28, 0x01, 0xc2, 0x01}
    }, {
        {0x03, 0x01, 0x83, 0x00}, {0x06, 0x01, 0x83, 0x00}, {0x0a, 0x01, 0x83, 0x00}, {0x0f, 0x01, 0x83, 0x00},
        {0x18, 0x01, 0x83, 0x00}, {0x1f, 0x01, 0x83, 0x00}, {0x29, 0x01, 0x83, 0x00}, {0x38, 0x01, 0x83, 0x01},
        {0x03, 0x01, 0xa2, 0x00}, {0x06, 0x01, 0xa2, 0x00}, {0x0a, 0x01, 0xa2, 0x00}, {0x0f, 0x01, 0xa2, 0x00},
        {0x18, 0x01, 0xa2, 0x00}, {0x1f, 0x01, 0xa2, 0x00}, {0x29, 0x01, 0xa2, 0x00}, {0x38, 0x01, 0xa2, 0x01}
    }, {
        {0x03, 0x01, 0xb8, 0x00}, {0x06, 0x01, 0xb8, 0x00}, {0x0a, 0x01, 0xb8, 0x00}, {0x0f, 0x01, 0xb8, 0x00},
        {0x18, 0x01, 0xb8, 0x00}, {0x1f, 0x01, 0xb8, 0x00}, {0x29, 0x01, 0xb8, 0x00}, {0x38, 0x01, 0xb8, 0x01},
        {0x03, 0x01, 0xc2, 0x00}, {0x06, 0x01, 0xc2, 0x00}, {0x0a, 0x01, 0xc2, 0x00}, {0x0f, 0x01, 0xc2, 0x00},
        {0x18, 0x01, 0xc2, 0x00}, {0x1f, 0x01, 0xc2, 0x00}, {0x29, 0x01, 0xc2, 0x00}, {0x38, 0x01, 0xc2, 0x01}
    }, {
        {0x02, 0x01, 0xe0, 0x00}, {0x09, 0x01, 0xe0, 0x00}, {0x17, 0x01, 0xe0, 0x00}, {0x28, 0x01, 0xe0, 0x01},
        {0x02, 0x01, 0xe2, 0x00}, {0x09, 0x01, 0xe2, 0x00}, {0x17, 0x01, 0xe2, 0x00}, {0x28, 0x01, 0xe2, 0x01},
        {0x01, 0x01, 0x99, 0x00}, {0x16, 0x01, 0x99, 0x01}, {0x01, 0x01, 0xa1, 0x00}, {0x16, 0x01, 0xa1, 0x01},
        {0x01, 0x01, 0xa7, 0x00}, {0x16, 0x01, 0xa7, 0x01}, {0x01, 0x01, 0xac, 0x00}, {0x16, 0x01, 0xac, 0x01}
    }, {
        {0x03, 0x01, 0xe0, 0x00}, {0x06, 0x01, 0xe0, 0x00}, {0x0a, 0x01, 0xe0, 0x00}, {0x0f, 0x01, 0xe0, 0x00},
        {0x18, 0x01, 0xe0, 0x00}, {0x1f, 0x01, 0xe0, 0x00}, {0x29, 0x01, 0xe0, 0x00}, {0x38, 0x01, 0xe0, 0x01},
        {0x03, 0x01, 0xe2, 0x00}, {0x06, 0x01, 0xe2, 0x00}, {0x0a, 0x01, 0xe2, 0x00}, {0x0f, 0x01, 0xe2, 0x00},
        {0x18, 0x01, 0xe2, 0x00}, {0x1f, 0x01, 0xe2, 0x00}, {0x29, 0x01, 0xe2, 0x00}, {0x38, 0x01, 0xe2, 0x01}
    }, {
        {0x02, 0x01, 0x99, 0x00}, {0x09, 0x01, 0x99, 0x00}, {0x17, 0x01, 0x99, 0x00}, {0x28, 0x01, 0x99, 0x01},
        {0x02, 0x01, 0xa1, 0x00}, {0x09, 0x01, 0xa1, 0x00}, {0x17, 0x01, 0xa1, 0x00}, {0x28, 0x01, 0xa1, 0x01},
        {0x02, 0x01, 0xa7, 0x00}, {0x09, 0x01, 0xa7, 0x00}, {0x17, 0x01, 0xa7, 0x00}, {0x28, 0x01, 0xa7, 0x01},
        {0x02, 0x01, 0xac, 0x00}, {0x09, 0x01, 0xac, 0x00}, {0x17, 0x01, 0xac, 0x00}, {0x28, 0x01, 0xac, 0x01}
    }, {
        {0x03, 0x01, 0x99, 0x00}, {0x06, 0x01, 0x99, 0x00}, {0x0a, 0x01, 0x99, 0x00}, {0x0f, 0x01, 0x99, 0x00},
        {0x18, 0x01, 0x99, 0x00}, {0x1f, 0x01, 0x99, 0x00}, {0x29, 0x01, 0x99, 0x00}, {0x38, 0x01, 0x99, 0x01},
        {0x03, 0x01, 0xa1, 0x00}, {0x06, 0x01, 0xa1, 0x00}, {0x0a, 0x01, 0xa1, 0x00}, {0x0f, 0x01, 0xa1, 0x00},
        {0x18, 0x01, 0xa1, 0x00}, {0x1f, 0x01, 0xa1, 0x00}, {0x29, 0x01, 0xa1, 0x00}, {0x38, 0x01, 0xa1, 0x01}
    }, {
        {0x03, 0x01, 0xa7, 0x00}, {0x06, 0x01, 0xa7, 0x00}, {0x0a, 0x01, 0xa7, 0x00}, {0x0f, 0x01, 0xa7, 0x00},
        {0x18, 0x01, 0xa7, 0x00}, {0x1f, 0x01, 0xa7, 0x00}, {0x29, 0x01, 0xa7, 0x00}, {0x38, 0x01, 0xa7, 0x01},
        {0x03, 0x01, 0xac, 0x00}, {0x06, 0x01, 0xac, 0x00}, {0x0a, 0x01, 0xac, 0x00}, {0x0f, 0x01, 0xac, 0x00},
        {0x18, 0x01, 0xac, 0x00}, {0x1f, 0x01, 0xac, 0x00}, {0x29, 0x01, 0xac, 0x00}, {0x38, 0x01, 0xac, 0x01}
    }, {
        {0x72, 0x00, 0x00, 0x00}, {0x73, 0x00, 0x00, 0x00}, {0x75, 0x00, 0x00, 0x00}, {0x76, 0x00, 0x00, 0x00},
        {0x79, 0x00, 0x00, 0x00}, {0x7b, 0x00, 0x00, 0x00}, {0x7f, 0x00, 0x00, 0x00}, {0x82, 0x00, 0x00, 0x00},
        {0x88, 0x00, 0x00, 0x00}, {0x8b, 0x00, 0x00, 0x00}, {0x8f, 0x00, 0x00, 0x00}, {0x92, 0x00, 0x00, 0x00},
        {0x9b, 0x00, 0x00, 0x00}, {0xa2, 0x00, 0x00, 0x00}, {0xaa, 0x00, 0x00, 0x00}, {0xb4, 0x00, 0x00, 0x01}
    }, {
        {0x00, 0x01, 0xb0, 0x01}, {0x00, 0x01, 0xb1, 0x01}, {0x00, 0x01, 0xb3, 0x01}, {0x00, 0x01, 0xd1, 0x01},
        {0x00, 0x01, 0xd8, 0x01}, {0x00, 0x01, 0xd9, 0x01}, {0x00, 0x01, 0xe3, 0x01}, {0x00, 0x01, 0xe5, 0x01},
        {0x00, 0x01, 0xe6, 0x01}, {0x7a, 0x00, 0x00, 0x00}, {0x7c, 0x00, 0x00, 0x00}, {0x7d, 0x00, 0x00, 0x00},
        {0x80, 0x00, 0x00, 0x00}, {0x81, 0x00, 0x00, 0x00}, {0x83, 0x00, 0x00, 0x00}, {0x84, 0x00, 0x00, 0x00}
    }, {
        {0x01, 0x01, 0xb0, 0x00}, {0x16, 0x01, 0xb0, 0x01}, {0x01, 0x01, 0xb1, 0x00}, {0x16, 0x01, 0xb1, 0x01},
        {0x01, 0x01, 0xb3, 0x00}, {0x16, 0x01, 0xb3, 0x01}, {0x01, 0x01, 0xd1, 0x00}, {0x16, 0x01, 0xd1, 0x01},
        {0x01, 0x01, 0xd8, 0x00}, {0x16, 0x01, 0xd8, 0x01}, {0x01, 0x01, 0xd9, 0x00}, {0x16, 0x01, 0xd9, 0x01},
        {0x01, 0x01, 0xe3, 0x00}, {0x16, 0x01, 0xe3, 0x01}, {0x01, 0x01, 0xe5, 0x00}, {0x16, 0x01, 0xe5, 0x01}
    }, {
        {0x02, 0x01, 0xb0, 0x00}, {0x09, 0x01, 0xb0, 0x00}, {0x17, 0x01, 0xb0, 0x00}, {0x28, 0x01, 0xb0, 0x01},
        {0x02, 0x01, 0xb1, 0x00}, {0x09, 0x01, 0xb1, 0x00}, {0x17, 0x01, 0xb1, 0x00}, {0x28, 0x01, 0xb1, 0x01},
        {0x02, 0x01, 0xb3, 0x00}, {0x09, 0x01, 0xb3, 0x00}, {0x17, 0x01, 0xb3, 0x00}, {0x28, 0x01, 0xb3, 0x01},
        {0x02, 0x01, 0xd1, 0x00}, {0x09, 0x01, 0xd1, 0x00}, {0x17, 0x01, 0xd1, 0x00}, {0x28, 0x01, 0xd1, 0x01}
    }, {
        {0x03, 0x01, 0xb0, 0x00}, {0x06, 0x01, 0xb0, 0x00}, {0x0a, 0x01, 0xb0, 0x00}, {0x0f, 0x01, 0xb0, 0x00},
        {0x18, 0x01, 0xb0, 0x00}, {0x1f, 0x01, 0xb0, 0x00}, {0x29, 0x01, 0xb0, 0x00}, {0x38, 0x01, 0xb0, 0x01},
        {0x03, 0x01, 0xb1, 0x00}, {0x06, 0x01, 0xb1, 0x00}, {0x0a, 0x01, 0xb1, 0x00}, {0x0f, 0x01, 0xb1, 0x00},
        {0x18, 0x01, 0xb1, 0x00}, {0x1f, 0x01, 0xb1, 0x00}, {0x29, 0x01, 0xb1, 0x00}, {0x38, 0x01, 0xb1, 0x01}
    }, {
        {0x03, 0x01, 0xb3, 0x00}, {0x06, 0x01, 0xb3, 0x00}, {0x0a, 0x01, 0xb3, 0x00}, {0x0f, 0x01, 0xb3, 0x00},
        {0x18, 0x01, 0xb3, 0x00}, {0x1f, 0x01, 0xb3, 0x00}, {0x29, 0x01, 0xb3, 0x00}, {0x38, 0x01, 0xb3, 0x01},
        {0x03, 0x01, 0xd1, 0x00}, {0x06, 0x01, 0xd1, 0x00}, {0x0a, 0x01, 0xd1, 0x00}, {0x0f, 0x01, 0xd1, 0x00},
        {0x18, 0x01, 0xd1, 0x00}, {0x1f, 0x01, 0xd1, 0x00}, {0x29, 0x01, 0xd1, 0x00}, {0x38, 0x01, 0xd1, 0x01}
    }, {
        {0x02, 0x01, 0xd8, 0x00}, {0x09, 0x01, 0xd8, 0x00}, {0x17, 0x01, 0xd8, 0x00}, {0x28, 0x01, 0xd8, 0x01},
        {0x02, 0x01, 0xd9, 0x00}, {0x09, 0x01, 0xd9, 0x00}, {0x17, 0x01, 0xd9, 0x00}, {0x28, 0x01, 0xd9, 0x01},
        {0x02, 0x01, 0xe3, 0x00}, {0x09, 0x01, 0xe3, 0x00}, {0x17, 0x01, 0xe3, 0x00}, {0x28, 0x01, 0xe3, 0x01},
        {0x02, 0x01, 0xe5, 0x00}, {0x09, 0x01, 0xe5, 0x00}, {0x17, 0x01, 0xe5, 0x00}, {0x28, 0x01, 0xe5, 0x01}
    }, {
        {0x03, 0x01, 0xd8, 0x00}, {0x06, 0x01, 0xd8, 0x00}, {0x0a, 0x01, 0xd8, 0x00}, {0x0f, 0x01, 0xd8, 0x00},
        {0x18, 0x01, 0xd8, 0x00}, {0x1f, 0x01, 0xd8, 0x00}, {0x29, 0x01, 0xd8, 0x00}, {0x38, 0x01, 0xd8, 0x01},
        {0x03, 0x01, 0xd9, 0x00}, {0x06, 0x01, 0xd9, 0x00}, {0x0a, 0x01, 0xd9, 0x00}, {0x0f, 0x01, 0xd9, 0x00},
        {0x18, 0x01, 0xd9, 0x00}, {0x1f, 0x01, 0xd9, 0x00}, {0x29, 0x01, 0xd9, 0x00}, {0x38, 0x01, 0xd9, 0x01}
    }, {
        {0x03, 0x01, 0xe3, 0x00}, {0x06, 0x01, 0xe3, 0x00}, {0x0a, 0x01, 0xe3, 0x00}, {0x0f, 0x01, 0xe3, 0x00},
        {0x18, 0x01, 0xe3, 0x00}, {0x1f, 0x01, 0xe3, 0x00}, {0x29, 0x01, 0xe3, 0x00}, {0x38, 0x01, 0xe3, 0x01},
        {0x03, 0x01, 0xe5, 0x00}, {0x06, 0x01, 0xe5, 0x00}, {0x0a, 0x01, 0xe5, 0x00}, {0x0f, 0x01, 0xe5, 0x00},
        {0x18, 0x01, 0xe5, 0x00}, {0x1f, 0x01, 0xe5, 0x00}, {0x29, 0x01, 0xe5, 0x00}, {0x38, 0x01, 0xe5, 0x01}
    }, {
        {0x01, 0x01, 0xe6, 0x00}, {0x16, 0x01, 0xe6, 0x01}, {0x00, 0x01, 0x81, 0x01}, {0x00, 0x01, 0x84, 0x01},
        {0x00, 0x01, 0x85, 0x01}, {0x00, 0x01, 0x86, 0x01}, {0x00, 0x01, 0x88, 0x01}, {0x00, 0x01, 0x92, 0x01},
        {0x00, 0x01, 0x9a, 0x01}, {0x00, 0x01, 0x9c, 0x01}, {0x00, 0x01, 0xa0, 0x01}, {0x00, 0x01, 0xa3, 0x01},
        {0x00, 0x01, 0xa4, 0x01}, {0x00, 0x01, 0xa9, 0x01}, {0x00, 0x01, 0xaa, 0x01}, {0x00, 0x01, 0xad, 0x01}
    }, {
        {0x02, 0x01, 0xe6, 0x00}, {0x09, 0x01, 0xe6, 0x00}, {0x17, 0x01, 0xe6, 0x00}, {0x28, 0x01, 0xe6, 0x01},
        {0x01, 0x01, 0x81, 0x00}, {0x16, 0x01, 0x81, 0x01}, {0x01, 0x01, 0x84, 0x00}, {0x16, 0x01, 0x84, 0x01},
        {0x01, 0x01, 0x85, 0x00}, {0x16, 0x01, 0x85, 0x01}, {0x01, 0x01, 0x86, 0x00}, {0x16, 0x01, 0x86, 0x01},
        {0x01, 0x01, 0x88, 0x00}, {0x16, 0x01, 0x88, 0x01}, {0x01, 0x01, 0x92, 0x00}, {0x16, 0x01, 0x92, 0x01}
    }, {
        {0x03, 0x01, 0xe6, 0x00}, {0x06, 0x01, 0xe6, 0x00}, {0x0a, 0x01, 0xe6, 0x00}, {0x0f, 0x01, 0xe6, 0x00},
        {0x18, 0x01, 0xe6, 0x00}, {0x1f, 0x01, 0xe6, 0x00}, {0x29, 0x01, 0xe6, 0x00}, {0x38, 0x01, 0xe6, 0x01},
        {0x02, 0x01, 0x81, 0x00}, {0x09, 0x01, 0x81, 0x00}, {0x17, 0x01, 0x81, 0x00}, {0x28, 0x01, 0x81, 0x01},
        {0x02, 0x01, 0x84, 0x00}, {0x09, 0x01, 0x84, 0x00}, {0x17, 0x01, 0x84, 0x00}, {0x28, 0x01, 0x84, 0x01}
    }, {
        {0x03, 0x01, 0x81, 0x00}, {0x06, 0x01, 0x81, 0x00}, {0x0a, 0x01, 0x81, 0x00}, {0x0f, 0x01, 0x81, 0x00},
        {0x18, 0x01, 0x81, 0x00}, {0x1f, 0x01, 0x81, 0x00}, {0x29, 0x01, 0x81, 0x00}, {0x38, 0x01, 0x81, 0x01},
        {0x03, 0x01, 0x84, 0x00}, {0x06, 0x01, 0x84, 0x00}, {0x0a, 0x01, 0x84, 0x00}, {0x0f, 0x01, 0x84, 0x00},
        {0x18, 0x01, 0x84, 0x00}, {0x1f, 0x01, 0x84, 0x00}, {0x29, 0x01, 0x84, 0x00}, {0x38, 0x01, 0x84, 0x01}
    }, {
        {0x02, 0x01, 0x85, 0x00}, {0x09, 0x01, 0x85, 0x00}, {0x17, 0x01, 0x85, 0x00}, {0x28, 0x01, 0x85, 0x01},
        {0x02, 0x01, 0x86, 0x00}, {0x09, 0x01, 0x86, 0x00}, {0x17, 0x01, 0x86, 0x00}, {0x28, 0x01, 0x86, 0x01},
        {0x02, 0x01, 0x88, 0x00}, {0x09, 0x01, 0x88, 0x00}, {0x17, 0x01, 0x88, 0x00}, {0x28, 0x01, 0x88, 0x01},
        {0x02, 0x01, 0x92, 0x00}, {0x09, 0x01, 0x92, 0x00}, {0x17, 0x01, 0x92, 0x00}, {0x28, 0x01, 0x92, 0x01}
    }, {
        {0x03, 0x01, 0x85, 0x00}, {0x06, 0x01, 0x85, 0x00}, {0x0a, 0x01, 0x85, 0x00}, {0x0f, 0x01, 0x85, 0x00},
        {0x18, 0x01, 0x85, 0x00}, {0x1f, 0x01, 0x85, 0x00}, {0x29, 0x01, 0x85, 0x00}, {0x38, 0x01, 0x85, 0x01},
        {0x03, 0x01, 0x86, 0x00}, {0x06, 0x01, 0x86, 0x00}, {0x0a, 0x01, 0x86, 0x00}, {0x0f, 0x01, 0x86, 0x00},
        {0x18, 0x01, 0x86, 0x00}, {0x1f, 0x01, 0x86, 0x00}, {0x29, 0x01, 0x86, 0x00}, {0x38, 0x01, 0x86, 0x01}
    }, {
        {0x03, 0x01, 0x88, 0x00}, {0x06, 0x01, 0x88, 0x00}, {0x0a, 0x01, 0x88, 0x00}, {0x0f, 0x01, 0x88, 0x00},
        {0x18, 0x01, 0x88, 0x00}, {0x1f, 0x01, 0x88, 0x00}, {0x29, 0x01, 0x88, 0x00}, {0x38, 0x01, 0x88, 0x01},
        {0x03, 0x01, 0x92, 0x00}, {0x06, 0x01, 0x92, 0x00}, {0x0a, 0x01, 0x92, 0x00}, {0x0f, 0x01, 0x92, 0x00},
        {0x18, 0x01, 0x92, 0x00}, {0x1f, 0x01, 0x92, 0x00}, {0x29, 0x01, 0x92, 0x00}, {0x38, 0x01, 0x92, 0x01}
    }, {
        {0x01, 0x01, 0x9a, 0x00}, {0x16, 0x01, 0x9a, 0x01}, {0x01, 0x01, 0x9c, 0x00}, {0x16, 0x01, 0x9c, 0x01},
        {0x01, 0x01, 0xa0, 0x00}, {0x16, 0x01, 0xa0, 0x01}, {0x01, 0x01, 0xa3, 0x00}, {0x16, 0x01, 0xa3, 0x01},
        {0x01, 0x01, 0xa4, 0x00}, {0x16, 0x01, 0xa4, 0x01}, {0x01, 0x01, 0xa9, 0x00}, {0x16, 0x01, 0xa9, 0x01},
        {0x01, 0x01, 0xaa, 0x00}, {0x16, 0x01, 0xaa, 0x01}, {0x01, 0x01, 0xad, 0x00}, {0x16, 0x01, 0xad, 0x01}
    }, {
        {0x02, 0x01, 0x9a, 0x00}, {0x09, 0x01, 0x9a, 0x00}, {0x17, 0x01, 0x9a, 0x00}, {0x28, 0x01, 0x9a, 0x01},
        {0x02, 0x01, 0x9c, 0x00}, {0x09, 0x01, 0x9c, 0x00}, {0x17, 0x01, 0x9c, 0x00}, {0x28, 0x01, 0x9c, 0x01},
        {0x02, 0x01, 0xa0, 0x00}, {0x09, 0x01, 0xa0, 0x00}, {0x17, 0x01, 0xa0, 0x00}, {0x28, 0x01, 0xa0, 0x01},
        {0x02, 0x01, 0xa3, 0x00}, {0x09, 0x01, 0xa3, 0x00}, {0x17, 0x01, 0xa3, 0x00}, {0x28, 0x01, 0xa3, 0x01}
    }, {
        {0x03, 0x01, 0x9a, 0x00}, {0x06, 0x01, 0x9a, 0x00}, {0x0a, 0x01, 0x9a, 0x00}, {0x0f, 0x01, 0x9a, 0x00},
        {0x18, 0x01, 0x9a, 0x00}, {0x1f, 0x01, 0x9a, 0x00}, {0x29, 0x01, 0x9a, 0x00}, {0x38, 0x01, 0x9a, 0x01},
        {0x03, 0x01, 0x9c, 0x00}, {0x06, 0x01, 0x9c, 0x00}, {0x0a, 0x01, 0x9c, 0x00}, {0x0f, 0x01, 0x9c, 0x00},
        {0x18, 0x01, 0x9c, 0x00}, {0x1f, 0x01, 0x9c, 0x00}, {0x29, 0x01, 0x9c, 0x00}, {0x38, 0x01, 0x9c, 0x01}
    }, {
        {0x03, 0x01, 0xa0, 0x00}, {0x06, 0x01, 0xa0, 0x00}, {0x0a, 0x01, 0xa0, 0x00}, {0x0f, 0x01, 0xa0, 0x00},
        {0x18, 0x01, 0xa0, 0x00}, {0x1f, 0x01, 0xa0, 0x00}, {0x29, 0x01, 0xa0, 0x00}, {0x38, 0x01, 0xa0, 0x01},
        {0x03, 0x01, 0xa3, 0x00}, {0x06, 0x01, 0xa3, 0x00}, {0x0a, 0x01, 0xa3, 0x00}, {0x0f, 0x01, 0xa3, 0x00},
        {0x18, 0x01, 0xa3, 0x00}, {0x1f, 0x01, 0xa3, 0x00}, {0x29, 0x01, 0xa3, 0x00}, {0x38, 0x01, 0xa3, 0x01}
    }, {
        {0x02, 0x01, 0xa4, 0x00}, {0x09, 0x01, 0xa4, 0x00}, {0x17, 0x01, 0xa4, 0x00}, {0x28, 0x01, 0xa4, 0x01},
        {0x02, 0x01, 0xa9, 0x00}, {0x09, 0x01, 0xa9, 0x00}, {0x17, 0x01, 0xa9, 0x00}, {0x28, 0x01, 0xa9, 0x01},
        {0x02, 0x01, 0xaa, 0x00}, {0x09, 0x01, 0xaa, 0x00}, {0x17, 0x01, 0xaa, 0x00}, {0x28, 0x01, 0xaa, 0x01},
        {0x02, 0x01, 0xad, 0x00}, {0x09, 0x01, 0xad, 0x00}, {0x17, 0x01, 0xad, 0x00}, {0x28, 0x01, 0xad, 0x01}
    }, {
        {0x03, 0x01, 0xa4, 0x00}, {0x06, 0x01, 0xa4, 0x00}, {0x0a, 0x01, 0xa4, 0x00}, {0x0f, 0x01, 0xa4, 0x00},
        {0x18, 0x01, 0xa4, 0x00}, {0x1f, 0x01, 0xa4, 0x00}, {0x29, 0x01, 0xa4, 0x00}, {0x38, 0x01, 0xa4, 0x01},
        {0x03, 0x01, 0xa9, 0x00}, {0x06, 0x01, 0xa9, 0x00}, {0x0a, 0x01, 0xa9, 0x00}, {0x0f, 0x01, 0xa9, 0x00},
        {0x18, 0x01, 0xa9, 0x00}, {0x1f, 0x01, 0xa9, 0x00}, {0x29, 0x01, 0xa9, 0x00}, {0x38, 0x01, 0xa9, 0x01}
    }, {
        {0x03, 0x01, 0xaa, 0x00}, {0x06, 0x01, 0xaa, 0x00}, {0x0a, 0x01, 0xaa, 0x00}, {0x0f, 0x01, 0xaa, 0x00},
        {0x18, 0x01, 0xaa, 0x00}, {0x1f, 0x01, 0xaa, 0x00}, {0x29, 0x01, 0xaa, 0x00}, {0x38, 0x01, 0xaa, 0x01},
        {0x03, 0x01, 0xad, 0x00}, {0x06, 0x01, 0xad, 0x00}, {0x0a, 0x01, 0xad, 0x00}, {0x0f, 0x01, 0xad, 0x00},
        {0x18, 0x01, 0xad, 0x00}, {0x1f, 0x01, 0xad, 0x00}, {0x29, 0x01, 0xad, 0x00}, {0x38, 0x01, 0xad, 0x01}
    }, {
        {0x89, 0x00, 0x00, 0x00}, {0x8a, 0x00, 0x00, 0x00}, {0x8c, 0x00, 0x00, 0x00}, {0x8d, 0x00, 0x00, 0x00},
        {0x90, 0x00, 0x00, 0x00}, {0x91, 0x00, 0x00, 0x00}, {0x93, 0x00, 0x00, 0x00}, {0x96, 0x00, 0x00, 0x00},
        {0x9c, 0x00, 0x00, 0x00}, {0x9f, 0x00, 0x00, 0x00}, {0xa3, 0x00, 0x00, 0x00}, {0xa6, 0x00, 0x00, 0x00},
        {0xab, 0x00, 0x00, 0x00}, {0xae, 0x00, 0x00, 0x00}, {0xb5, 0x00, 0x00, 0x00}, {0xbe, 0x00, 0x00, 0x01}
    }, {
        {0x00, 0x01, 0xb2, 0x01}, {0x00, 0x01, 0xb5, 0x01}, {0x00, 0x01, 0xb9, 0x01}, {0x00, 0x01, 0xba, 0x01},
        {0x00, 0x01, 0xbb, 0x01}, {0x00, 0x01, 0xbd, 0x01}, {0x00, 0x01, 0xbe, 0x01}, {0x00, 0x01, 0xc4, 0x01},
        {0x00, 0x01, 0xc6, 0x01}, {0x00, 0x01, 0xe4, 0x01}, {0x00, 0x01, 0xe8, 0x01}, {0x00, 0x01, 0xe9, 0x01},
        {0x94, 0x00, 0x00, 0x00}, {0x95, 0x00, 0x00, 0x00}, {0x97, 0x00, 0x00, 0x00}, {0x98, 0x00, 0x00, 0x00}
    }, {
        {0x01, 0x01, 0xb2, 0x00}, {0x16, 0x01, 0xb2, 0x01}, {0x01, 0x01, 0xb5, 0x00}, {0x16, 0x01, 0xb5, 0x01},
        {0x01, 0x01, 0xb9, 0x00}, {0x16, 0x01, 0xb9, 0x01}, {0x01, 0x01, 0xba, 0x00}, {0x16, 0x01, 0xba, 0x01},
        {0x01, 0x01, 0xbb, 0x00}, {0x16, 0x01, 0xbb, 0x01}, {0x01, 0x01, 0xbd, 0x00}, {0x16, 0x01, 0xbd, 0x01},
        {0x01, 0x01, 0xbe, 0x00}, {0x16, 0x01, 0xbe, 0x01}, {0x01, 0x01, 0xc4, 0x00}, {0x16, 0x01, 0xc4, 0x01}
    }, {
        {0x02, 0x01, 0xb2, 0x00}, {0x09, 0x01, 0xb2, 0x00}, {0x17, 0x01, 0xb2, 0x00}, {0x28, 0x01, 0xb2, 0x01},
        {0x02, 0x01, 0xb5, 0x00}, {0x09, 0x01, 0xb5, 0x00}, {0x17, 0x01, 0xb5, 0x00}, {0x28, 0x01, 0xb5, 0x01},
        {0x02, 0x01, 0xb9, 0x00}, {0x09, 0x01, 0xb9, 0x00}, {0x17, 0x01, 0xb9, 0x00}, {0x28, 0x01, 0xb9, 0x01},
        {0x02, 0x01, 0xba, 0x00}, {0x09, 0x01, 0xba, 0x00}, {0x17, 0x01, 0xba, 0x00}, {0x28, 0x01, 0xba, 0x01}
    }, {
        {0x03, 0x01, 0xb2, 0x00}, {0x06, 0x01, 0xb2, 0x00}, {0x0a, 0x01, 0xb2, 0x00}, {0x0f, 0x01, 0xb2, 0x00},
        {0x18, 0x01, 0xb2, 0x00}, {0x1f, 0x01, 0xb2, 0x00}, {0x29, 0x01, 0xb2, 0x00}, {0x38, 0x01, 0xb2, 0x01},
        {0x03, 0x01, 0xb5, 0x00}, {0x06, 0x01, 0xb5, 0x00}, {0x0a, 0x01, 0xb5, 0x00}, {0x0f, 0x01, 0xb5, 0x00},
        {0x18, 0x01, 0xb5, 0x00}, {0x1f, 0x01, 0xb5, 0x00}, {0x29, 0x01, 0xb5, 0x00}, {0x38, 0x01, 0xb5, 0x01}
    }, {
        {0x03, 0x01, 0xb9, 0x00}, {0x06, 0x01, 0xb9, 0x00}, {0x0a, 0x01, 0xb9, 0x00}, {0x0f, 0x01, 0xb9, 0x00},
        {0x18, 0x01, 0xb9, 0x00}, {0x1f, 0x01, 0xb9, 0x00}, {0x29, 0x01, 0xb9, 0x00}, {0x38, 0x01, 0xb9, 0x01},
        {0x03, 0x01, 0xba, 0x00}, {0x06, 0x01, 0xba, 0x00}, {0x0a, 0x01, 0xba, 0x00}, {0x0f, 0x01, 0xba, 0x00},
        {0x18, 0x01, 0xba, 0x00}, {0x1f, 0x01, 0xba, 0x00}, {0x29, 0x01, 0xba, 0x00}, {0x38, 0x01, 0xba, 0x01}
    }, {
        {0x02, 0x01, 0xbb, 0x00}, {0x09, 0x01, 0xbb, 0x00}, {0x17, 0x01, 0xbb, 0x00}, {0x28, 0x01, 0xbb, 0x01},
        {0x02, 0x01, 0xbd, 0x00}, {0x09, 0x01, 0xbd, 0x00}, {0x17, 0x01, 0xbd, 0x00}, {0x28, 0x01, 0xbd, 0x01},
        {0x02, 0x01, 0xbe, 0x00}, {0x09, 0x01, 0xbe, 0x00}, {0x17, 0x01, 0xbe, 0x00}, {0x28, 0x01, 0xbe, 0x01},
        {0x02, 0x01, 0xc4, 0x00}, {0x09, 0x01, 0xc4, 0x00}, {0x17, 0x01, 0xc4, 0x00}, {0x28, 0x01, 0xc4, 0x01}
    }, {
        {0x03, 0x01, 0xbb, 0x00}, {0x06, 0x01, 0xbb, 0x00}, {0x0a, 0x01, 0xbb, 0x00}, {0x0f, 0x01, 0xbb, 0x00},
        {0x18, 0x01, 0xbb, 0x00}, {0x1f, 0x01, 0xbb, 0x00}, {0x29, 0x01, 0xbb, 0x00}, {0x38, 0x01, 0xbb, 0x01},
        {0x03, 0x01, 0xbd, 0x00}, {0x06, 0x01, 0xbd, 0x00}, {0x0a, 0x01, 0xbd, 0x00}, {0x0f, 0x01, 0xbd, 0x00},
        {0x18, 0x01, 0xbd, 0x00}, {0x1f, 0x01, 0xbd, 0x00}, {0x29, 0x01, 0xbd, 0x00}, {0x38, 0x01, 0xbd, 0x01}
    }, {
        {0x03, 0x01, 0xbe, 0x00}, {0x06, 0x01, 0xbe, 0x00}, {0x0a, 0x01, 0xbe, 0x00}, {0x0f, 0x01, 0xbe, 0x00},
        {0x18, 0x01, 0xbe, 0x00}, {0x1f, 0x01, 0xbe, 0x00}, {0x29, 0x01, 0xbe, 0x00}, {0x38, 0x01, 0xbe, 0x01},
        {0x03, 0x01, 0xc4, 0x00}, {0x06, 0x01, 0xc4, 0x00}, {0x0a, 0x01, 0xc4, 0x00}, {0x0f, 0x01, 0xc4, 0x00},
        {0x18, 0x01, 0xc4, 0x00}, {0x1f, 0x01, 0xc4, 0x00}, {0x29, 0x01, 0xc4, 0x00}, {0x38, 0x01, 0xc4, 0x01}
    }, {
        {0x01, 0x01, 0xc6, 0x00}, {0x16, 0x01, 0xc6, 0x01}, {0x01, 0x01, 0xe4, 0x00}, {0x16, 0x01, 0xe4, 0x01},
        {0x01, 0x01, 0xe8, 0x00}, {0x16, 0x01, 0xe8, 0x01}, {0x01, 0x01, 0xe9, 0x00}, {0x16, 0x01, 0xe9, 0x01},
        {0x00, 0x01, 0x01, 0x01}, {0x00, 0x01, 0x87, 0x01}, {0x00, 0x01, 0x89, 0x01}, {0x00, 0x01, 0x8a, 0x01},
        {0x00, 0x01, 0x8b, 0x01}, {0x00, 0x01, 0x8c, 0x01}, {0x00, 0x01, 0x8d, 0x01}, {0x00, 0x01, 0x8f, 0x01}
    }, {
        {0x02, 0x01, 0xc6, 0x00}, {0x09, 0x01, 0xc6, 0x00}, {0x17, 0x01, 0xc6, 0x00}, {0x28, 0x01, 0xc6, 0x01},
        {0x02, 0x01, 0xe4, 0x00}, {0x09, 0x01, 0xe4, 0x00}, {0x17, 0x01, 0xe4, 0x00}, {0x28, 0x01, 0xe4, 0x01},
        {0x02, 0x01, 0xe8, 0x00}, {0x09, 0x01, 0xe8, 0x00}, {0x17, 0x01, 0xe8, 0x00}, {0x28, 0x01, 0xe8, 0x01},
        {0x02, 0x01, 0xe9, 0x00}, {0x09, 0x01, 0xe9, 0x00}, {0x17, 0x01, 0xe9, 0x00}, {0x28, 0x01, 0xe9, 0x01}
    }, {
        {0x03, 0x01, 0xc6, 0x00}, {0x06, 0x01, 0xc6, 0x00}, {0x0a, 0x01, 0xc6, 0x00}, {0x0f, 0x01, 0xc6, 0x00},
        {0x18, 0x01, 0xc6, 0x00}, {0x1f, 0x01, 0xc6, 0x00}, {0x29, 0x01, 0xc6, 0x00}, {0x38, 0x01, 0xc6, 0x01},
        {0x03, 0x01, 0xe4, 0x00}, {0x06, 0x01, 0xe4, 0x00}, {0x0a, 0x01, 0xe4, 0x00}, {0x0f, 0x01, 0xe4, 0x00},
        {0x18, 0x01, 0xe4, 0x00}, {0x1f, 0x01, 0xe4, 0x00}, {0x29, 0x01, 0xe4, 0x00}, {0x38, 0x01, 0xe4, 0x01}
    }, {
        {0x03, 0x01, 0xe8, 0x00}, {0x06, 0x01, 0xe8, 0x00}, {0x0a, 0x01, 0xe8, 0x00}, {0x0f, 0x01, 0xe8, 0x00},
        {0x18, 0x01, 0xe8, 0x00}, {0x1f, 0x01, 0xe8, 0x00}, {0x29, 0x01, 0xe8, 0x00}, {0x38, 0x01, 0xe8, 0x01},
        {0x03, 0x01, 0xe9, 0x00}, {0x06, 0x01, 0xe9, 0x00}, {0x0a, 0x01, 0xe9, 0x00}, {0x0f, 0x01, 0xe9, 0x00},
        {0x18, 0x01, 0xe9, 0x00}, {0x1f, 0x01, 0xe9, 0x00}, {0x29, 0x01, 0xe9, 0x00}, {0x38, 0x01, 0xe9, 0x01}
    }, {
        {0x01, 0x01, 0x01, 0x00}, {0x16, 0x01, 0x01, 0x01}, {0x01, 0x01, 0x87, 0x00}, {0x16, 0x01, 0x87, 0x01},
        {0x01, 0x01, 0x89, 0x00}, {0x16, 0x01, 0x89, 0x01}, {0x01, 0x01, 0x8a, 0x00}, {0x16, 0x01, 0x8a, 0x01},
        {0x01, 0x01, 0x8b, 0x00}, {0x16, 0x01, 0x8b, 0x01}, {0x01, 0x01, 0x8c, 0x00}, {0x16, 0x01, 0x8c, 0x01},
        {0x01, 0x01, 0x8d, 0x00}, {0x16, 0x01, 0x8d, 0x01}, {0x01, 0x01, 0x8f, 0x00}, {0x16, 0x01, 0x8f, 0x01}
    }, {
        {0x02, 0x01, 0x01, 0x00}, {0x09, 0x01, 0x01, 0x00}, {0x17, 0x01, 0x01, 0x00}, {0x28, 0x01, 0x01, 0x01},
        {0x02, 0x01, 0x87, 0x00}, {0x09, 0x01, 0x87, 0x00}, {0x17, 0x01, 0x87, 0x00}, {0x28, 0x01, 0x87, 0x01},
        {0x02, 0x01, 0x89, 0x00}, {0x09, 0x01, 0x89, 0x00}, {0x17, 0x01, 0x89, 0x00}, {0x28, 0x01, 0x89, 0x01},
        {0x02, 0x01, 0x8a, 0x00}, {0x09, 0x01, 0x8a, 0x00}, {0x17, 0x01, 0x8a, 0x00}, {0x28, 0x01, 0x8a, 0x01}
    }, {
        {0x03, 0x01, 0x01, 0x00}, {0x06, 0x01, 0x01, 0x00}, {0x0a, 0x01, 0x01, 0x00}, {0x0f, 0x01, 0x01, 0x00},
        {0x18, 0x01, 0x01, 0x00}, {0x1f, 0x01, 0x01, 0x00}, {0x29, 0x01, 0x01, 0x00}, {0x38, 0x01, 0x01, 0x01},
        {0x03, 0x01, 0x87, 0x00}, {0x06, 0x01, 0x87, 0x00}, {0x0a, 0x01, 0x87, 0x00}, {0x0f, 0x01, 0x87, 0x00},
        {0x18, 0x01, 0x87, 0x00}, {0x1f, 0x01, 0x87, 0x00}, {0x29, 0x01, 0x87, 0x00}, {0x38, 0x01, 0x87, 0x01}
    }, {
        {0x03, 0x01, 0x89, 0x00}, {0x06, 0x01, 0x89, 0x00}, {0x0a, 0x01, 0x89, 0x00}, {0x0f, 0x01, 0x89, 0x00},
        {0x18, 0x01, 0x89, 0x00}, {0x1f, 0x01, 0x89, 0x00}, {0x29, 0x01, 0x89, 0x00}, {0x38, 0x01, 0x89, 0x01},
        {0x03, 0x01, 0x8a, 0x00}, {0x06, 0x01, 0x8a, 0x00}, {0x0a, 0x01, 0x8a, 0x00}, {0x0f, 0x01, 0x8a, 0x00},
        {0x18, 0x01, 0x8a, 0x00}, {0x1f, 0x01, 0x8a, 0x00}, {0x29, 0x01, 0x8a, 0x00}, {0x38, 0x01, 0x8a, 0x01}
    }, {
        {0x02, 0x01, 0x8b, 0x00}, {0x09, 0x01, 0x8b, 0x00}, {0x17, 0x01, 0x8b, 0x00}, {0x28, 0x01, 0x8b, 0x01},
        {0x02, 0x01, 0x8c, 0x00}, {0x09, 0x01, 0x8c, 0x00}, {0x17, 0x01, 0x8c, 0x00}, {0x28, 0x01, 0x8c, 0x01},
        {0x02, 0x01, 0x8d, 0x00}, {0x09, 0x01, 0x8d, 0x00}, {0x17, 0x01, 0x8d, 0x00}, {0x28, 0x01, 0x8d, 0x01},
        {0x02, 0x01, 0x8f, 0x00}, {0x09, 0x01, 0x8f, 0x00}, {0x17, 0x01, 0x8f, 0x00}, {0x28, 0x01, 0x8f, 0x01}
    }, {
        {0x03, 0x01, 0x8b, 0x00}, {0x06, 0x01, 0x8b, 0x00}, {0x0a, 0x01, 0x8b, 0x00}, {0x0f, 0x01, 0x8b, 0x00},
        {0x18, 0x01, 0x8b, 0x00}, {0x1f, 0x01, 0x8b, 0x00}, {0x29, 0x01, 0x8b, 0x00}, {0x38, 0x01, 0x8b, 0x01},
        {0x03, 0x01, 0x8c, 0x00}, {0x06, 0x01, 0x8c, 0x00}, {0x0a, 0x01, 0x8c, 0x00}, {0x0f, 0x01, 0x8c, 0x00},
        {0x18, 0x01, 0x8c, 0x00}, {0x1f, 0x01, 0x8c, 0x00}, {0x29, 0x01, 0x8c, 0x00}, {0x38, 0x01, 0x8c, 0x01}
    }, {
        {0x03, 0x01, 0x8d, 0x00}, {0x06, 0x01, 0x8d, 0x00}, {0x0a, 0x01, 0x8d, 0x00}, {0x0f, 0x01, 0x8d, 0x00},
        {0x18, 0x01, 0x8d, 0x00}, {0x1f, 0x01, 0x8d, 0x00}, {0x29, 0x01, 0x8d, 0x00}, {0x38, 0x01, 0x8d, 0x01},
        {0x03, 0x01, 0x8f, 0x00}, {0x06, 0x01, 0x8f, 0x00}, {0x0a, 0x01, 0x8f, 0x00}, {0x0f, 0x01, 0x8f, 0x00},
        {0x18, 0x01, 0x8f, 0x00}, {0x1f, 0x01, 0x8f, 0x00}, {0x29, 0x01, 0x8f, 0x00}, {0x38, 0x01, 0x8f, 0x01}
    }, {
        {0x9d, 0x00, 0x00, 0x00}, {0x9e, 0x00, 0x00, 0x00}, {0xa0, 0x00, 0x00, 0x00}, {0xa1, 0x00, 0x00, 0x00},
        {0xa4, 0x00, 0x00, 0x00}, {0xa5, 0x00, 0x00, 0x00}, {0xa7, 0x00, 0x00, 0x00}, {0xa8, 0x00, 0x00, 0x00},
        {0xac, 0x00, 0x00, 0x00}, {0xad, 0x00, 0x00, 0x00}, {0xaf, 0x00, 0x00, 0x00}, {0xb1, 0x00, 0x00, 0x00},
        {0xb6, 0x00, 0x00, 0x00}, {0xb9, 0x00, 0x00, 0x00}, {0xbf, 0x00, 0x00, 0x00}, {0xcf, 0x00, 0x00, 0x01}
    }, {
        {0x00, 0x01, 0x93, 0x01}, {0x00, 0x01, 0x95, 0x01}, {0x00, 0x01, 0x96, 0x01}, {0x00, 0x01, 0x97, 0x01},
        {0x00, 0x01, 0x98, 0x01}, {0x00, 0x01, 0x9b, 0x01}, {0x00, 0x01, 0x9d, 0x01}, {0x00, 0x01, 0x9e, 0x01},
        {0x00, 0x01, 0xa5, 0x01}, {0x00, 0x01, 0xa6, 0x01}, {0x00, 0x01, 0xa8, 0x01}, {0x00, 0x01, 0xae, 0x01},
        {0x00, 0x01, 0xaf, 0x01}, {0x00, 0x01, 0xb4, 0x01}, {0x00, 0x01, 0xb6, 0x01}, {0x00, 0x01, 0xb7, 0x01}
    }, {
        {0x01, 0x01, 0x93, 0x00}, {0x16, 0x01, 0x93, 0x01}, {0x01, 0x01, 0x95, 0x00}, {0x16, 0x01, 0x95, 0x01},
        {0x01, 0x01, 0x96, 0x00}, {0x16, 0x01, 0x96, 0x01}, {0x01, 0x01, 0x97, 0x00}, {0x16, 0x01, 0x97, 0x01},
        {0x01, 0x01, 0x98, 0x00}, {0x16, 0x01, 0x98, 0x01}, {0x01, 0x01, 0x9b, 0x00}, {0x16, 0x01, 0x9b, 0x01},
        {0x01, 0x01, 0x9d, 0x00}, {0x16, 0x01, 0x9d, 0x01}, {0x01, 0x01, 0x9e, 0x00}, {0x16, 0x01, 0x9e, 0x01}
    }, {
        {0x02, 0x01, 0x93, 0x00}, {0x09, 0x01, 0x93, 0x00}, {0x17, 0x01, 0x93, 0x00}, {0x28, 0x01, 0x93, 0x01},
        {0x02, 0x01, 0x95, 0x00}, {0x09, 0x01, 0x95, 0x00}, {0x17, 0x01, 0x95, 0x00}, {0x28, 0x01, 0x95, 0x01},
        {0x02, 0x01, 0x96, 0x00}, {0x09, 0x01, 0x96, 0x00}, {0x17, 0x01, 0x96, 0x00}, {0x28, 0x01, 0x96, 0x01},
        {0x02, 0x01, 0x97, 0x00}, {0x09, 0x01, 0x97, 0x00}, {0x17, 0x01, 0x97, 0x00}, {0x28, 0x01, 0x97, 0x01}
    }, {
        {0x03, 0x01, 0x93, 0x00}, {0x06, 0x01, 0x93, 0x00}, {0x0a, 0x01, 0x93, 0x00}, {0x0f, 0x01, 0x93, 0x00},
        {0x18, 0x01, 0x93, 0x00}, {0x1f, 0x01, 0x93, 0x00}, {0x29, 0x01, 0x93, 0x00}, {0x38, 0x01, 0x93, 0x01},
        {0x03, 0x01, 0x95, 0x00}, {0x06, 0x01, 0x95, 0x00}, {0x0a, 0x01, 0x95, 0x00}, {0x0f, 0x01, 0x95, 0x00},
        {0x18, 0x01, 0x95, 0x00}, {0x1f, 0x01, 0x95, 0x00}, {0x29, 0x01, 0x95, 0x00}, {0x38, 0x01, 0x95, 0x01}
    }, {
        {0x03, 0x01, 0x96, 0x00}, {0x06, 0x01, 0x96, 0x00}, {0x0a, 0x01, 0x96, 0x00}, {0x0f, 0x01, 0x96, 0x00},
        {0x18, 0x01, 0x96, 0x00}, {0x1f, 0x01, 0x96, 0x00}, {0x29, 0x01, 0x96, 0x00}, {0x38, 0x01, 0x96, 0x01},
        {0x03, 0x01, 0x97, 0x00}, {0x06, 0x01, 0x97, 0x00}, {0x0a, 0x01, 0x97, 0x00}, {0x0f, 0x01, 0x97, 0x00},
        {0x18, 0x01, 0x97, 0x00}, {0x1f, 0x01, 0x97, 0x00}, {0x29, 0x01, 0x97, 0x00}, {0x38, 0x01, 0x97, 0x01}
    }, {
        {0x02, 0x01, 0x98, 0x00}, {0x09, 0x01, 0x98, 0x00}, {0x17, 0x01, 0x98, 0x00}, {0x28, 0x01, 0x98, 0x01},
        {0x02, 0x01, 0x9b, 0x00}, {0x09, 0x01, 0x9b, 0x00}, {0x17, 0x01, 0x9b, 0x00}, {0x28, 0x01, 0x9b, 0x01},
        {0x02, 0x01, 0x9d, 0x00}, {0x09, 0x01, 0x9d, 0x00}, {0x17, 0x01, 0x9d, 0x00}, {0x28, 0x01, 0x9d, 0x01},
        {0x02, 0x01, 0x9e, 0x00}, {0x09, 0x01, 0x9e, 0x00}, {0x17, 0x01, 0x9e, 0x00}, {0x28, 0x01, 0x9e, 0x01}
    }, {
        {0x03, 0x01, 0x98, 0x00}, {0x06, 0x01, 0x98, 0x00}, {0x0a, 0x01, 0x98, 0x00}, {0x0f, 0x01, 0x98, 0x00},
        {0x18, 0x01, 0x98, 0x00}, {0x1f, 0x01, 0x98, 0x00}, {0x29, 0x01, 0x98, 0x00}, {0x38, 0x01, 0x98, 0x01},
        {0x03, 0x01, 0x9b, 0x00}, {0x06, 0x01, 0x9b, 0x00}, {0x0a, 0x01, 0x9b, 0x00}, {0x0f, 0x01, 0x9b, 0x00},
        {0x18, 0x01, 0x9b, 0x00}, {0x1f, 0x01, 0x9b, 0x00}, {0x29, 0x01, 0x9b, 0x00}, {0x38, 0x01, 0x9b, 0x01}
    }, {
        {0x03, 0x01, 0x9d, 0x00}, {0x06, 0x01, 0x9d, 0x00}, {0x0a, 0x01, 0x9d, 0x00}, {0x0f, 0x01, 0x9d, 0x00},
        {0x18, 0x01, 0x9d, 0x00}, {0x1f, 0x01, 0x9d, 0x00}, {0x29, 0x01, 0x9d, 0x00}, {0x38, 0x01, 0x9d, 0x01},
        {0x03, 0x01, 0x9e, 0x00}, {0x06, 0x01, 0x9e, 0x00}, {0x0a, 0x01, 0x9e, 0x00}, {0x0f, 0x01, 0x9e, 0x00},
        {0x18, 0x01, 0x9e, 0x00}, {0x1f, 0x01, 0x9e, 0x00}, {0x29, 0x01, 0x9e, 0x00}, {0x38, 0x01, 0x9e, 0x01}
    }, {
        {0x01, 0x01, 0xa5, 0x00}, {0x16, 0x01, 0xa5, 0x01}, {0x01, 0x01, 0xa6, 0x00}, {0x16, 0x01, 0xa6, 0x01},
        {0x01, 0x01, 0xa8, 0x00}, {0x16, 0x01, 0xa8, 0x01}, {0x01, 0x01, 0xae, 0x00}, {0x16, 0x01, 0xae, 0x01},
        {0x01, 0x01, 0xaf, 0x00}, {0x16, 0x01, 0xaf, 0x01}, {0x01, 0x01, 0xb4, 0x00}, {0x16, 0x01, 0xb4, 0x01},
        {0x01, 0x01, 0xb6, 0x00}, {0x16, 0x01, 0xb6, 0x01}, {0x01, 0x01, 0xb7, 0x00}, {0x16, 0x01, 0xb7, 0x01}
    }, {
        {0x02, 0x01, 0xa5, 0x00}, {0x09, 0x01, 0xa5, 0x00}, {0x17, 0x01, 0xa5, 0x00}, {0x28, 0x01, 0xa5, 0x01},
        {0x02, 0x01, 0xa6, 0x00}, {0x09, 0x01, 0xa6, 0x00}, {0x17, 0x01, 0xa6, 0x00}, {0x28, 0x01, 0xa6, 0x01},
        {0x02, 0x01, 0xa8, 0x00}, {0x09, 0x01, 0xa8, 0x00}, {0x17, 0x01, 0xa8, 0x00}, {0x28, 0x01, 0xa8, 0x01},
        {0x02, 0x01, 0xae, 0x00}, {0x09, 0x01, 0xae, 0x00}, {0x17, 0x01, 0xae, 0x00}, {0x28, 0x01, 0xae, 0x01}
    }, {
        {0x03, 0x01, 0xa5, 0x00}, {0x06, 0x01, 0xa5, 0x00}, {0x0a, 0x01, 0xa5, 0x00}, {0x0f, 0x01, 0xa5, 0x00},
        {0x18, 0x01, 0xa5, 0x00}, {0x1f, 0x01, 0xa5, 0x00}, {0x29, 0x01, 0xa5, 0x00}, {0x38, 0x01, 0xa5, 0x01},
        {0x03, 0x01, 0xa6, 0x00}, {0x06, 0x01, 0xa6, 0x00}, {0x0a, 0x01, 0xa6, 0x00}, {0x0f, 0x01, 0xa6, 0x00},
        {0x18, 0x01, 0xa6, 0x00}, {0x1f, 0x01, 0xa6, 0x00}, {0x29, 0x01, 0xa6, 0x00}, {0x38, 0x01, 0xa6, 0x01}
    }, {
        {0x03, 0x01, 0xa8, 0x00}, {0x06, 0x01, 0xa8, 0x00}, {0x0a, 0x01, 0xa8, 0x00}, {0x0f, 0x01, 0xa8, 0x00},
        {0x18, 0x01, 0xa8, 0x00}, {0x1f, 0x01, 0xa8, 0x00}, {0x29, 0x01, 0xa8, 0x00}, {0x38, 0x01, 0xa8, 0x01},
        {0x03, 0x01, 0xae, 0x00}, {0x06, 0x01, 0xae, 0x00}, {0x0a, 0x01, 0xae, 0x00}, {0x0f, 0x01, 0xae, 0x00},
        {0x18, 0x01, 0xae, 0x00}, {0x1f, 0x01, 0xae, 0x00}, {0x29, 0x01, 0xae, 0x00}, {0x38, 0x01, 0xae, 0x01}
    }, {
        {0x02, 0x01, 0xaf, 0x00}, {0x09, 0x01, 0xaf, 0x00}, {0x17, 0x01, 0xaf, 0x00}, {0x28, 0x01, 0xaf, 0x01},
        {0x02, 0x01, 0xb4, 0x00}, {0x09, 0x01, 0xb4, 0x00}, {0x17, 0x01, 0xb4, 0x00}, {0x28, 0x01, 0xb4, 0x01},
        {0x02, 0x01, 0xb6, 0x00}, {0x09, 0x01, 0xb6, 0x00}, {0x17, 0x01, 0xb6, 0x00}, {0x28, 0x01, 0xb6, 0x01},
        {0x02, 0x01, 0xb7, 0x00}, {0x09, 0x01, 0xb7, 0x00}, {0x17, 0x01, 0xb7, 0x00}, {0x28, 0x01, 0xb7, 0x01}
    }, {
        {0x03, 0x01, 0xaf, 0x00}, {0x06, 0x01, 0xaf, 0x00}, {0x0a, 0x01, 0xaf, 0x00}, {0x0f, 0x01, 0xaf, 0x00},
        {0x18, 0x01, 0xaf, 0x00}, {0x1f, 0x01, 0xaf, 0x00}, {0x29, 0x01, 0xaf, 0x00}, {0x38, 0x01, 0xaf, 0x01},
        {0x03, 0x01, 0xb4, 0x00}, {0x06, 0x01, 0xb4, 0x00}, {0x0a, 0x01, 0xb4, 0x00}, {0x0f, 0x01, 0xb4, 0x00},
        {0x18, 0x01, 0xb4, 0x00}, {0x1f, 0x01, 0xb4, 0x00}, {0x29, 0x01, 0xb4, 0x00}, {0x38, 0x01, 0xb4, 0x01}
    }, {
        {0x03, 0x01, 0xb6, 0x00}, {0x06, 0x01, 0xb6, 0x00}, {0x0a, 0x01, 0xb6, 0x00}, {0x0f, 0x01, 0xb6, 0x00},
        {0x18, 0x01, 0xb6, 0x00}, {0x1f, 0x01, 0xb6, 0x00}, {0x29, 0x01, 0xb6, 0x00}, {0x38, 0x01, 0xb6, 0x01},
        {0x03, 0x01, 0xb7, 0x00}, {0x06, 0x01, 0xb7, 0x00}, {0x0a, 0x01, 0xb7, 0x00}, {0x0f, 0x01, 0xb7, 0x00},
        {0x18, 0x01, 0xb7, 0x00}, {0x1f, 0x01, 0xb7, 0x00}, {0x29, 0x01, 0xb7, 0x00}, {0x38, 0x01, 0xb7, 0x01}
    }, {
        {0x00, 0x01, 0xbc, 0x01}, {0x00, 0x01, 0xbf, 0x01}, {0x00, 0x01, 0xc5, 0x01}, {0x00, 0x01, 0xe7, 0x01},
        {0x00, 0x01, 0xef, 0x01}, {0xb0, 0x00, 0x00, 0x00}, {0xb2, 0x00, 0x00, 0x00}, {0xb3, 0x00, 0x00, 0x00},
        {0xb7, 0x00, 0x00, 0x00}, {0xb8, 0x00, 0x00, 0x00}, {0xba, 0x00, 0x00, 0x00}, {0xbb, 0x00, 0x00, 0x00},
        {0xc0, 0x00, 0x00, 0x00}, {0xc7, 0x00, 0x00, 0x00}, {0xd0, 0x00, 0x00, 0x00}, {0xdf, 0x00, 0x00, 0x01}
    }, {
        {0x01, 0x01, 0xbc, 0x00}, {0x16, 0x01, 0xbc, 0x01}, {0x01, 0x01, 0xbf, 0x00}, {0x16, 0x01, 0xbf, 0x01},
        {0x01, 0x01, 0xc5, 0x00}, {0x16, 0x01, 0xc5, 0x01}, {0x01, 0x01, 0xe7, 0x00}, {0x16, 0x01, 0xe7, 0x01},
        {0x01, 0x01, 0xef, 0x00}, {0x16, 0x01, 0xef, 0x01}, {0x00, 0x01, 0x09, 0x01}, {0x00, 0x01, 0x8e, 0x01},
        {0x00, 0x01, 0x90, 0x01}, {0x00, 0x01, 0x91, 0x01}, {0x00, 0x01, 0x94, 0x01}, {0x00, 0x01, 0x9f, 0x01}
    }, {
        {0x02, 0x01, 0xbc, 0x00}, {0x09, 0x01, 0xbc, 0x00}, {0x17, 0x01, 0xbc, 0x00}, {0x28, 0x01, 0xbc, 0x01},
        {0x02, 0x01, 0xbf, 0x00}, {0x09, 0x01, 0xbf, 0x00}, {0x17, 0x01, 0xbf, 0x00}, {0x28, 0x01, 0xbf, 0x01},
        {0x02, 0x01, 0xc5, 0x00}, {0x09, 0x01, 0xc5, 0x00}, {0x17, 0x01, 0xc5, 0x00}, {0x28, 0x01, 0xc5, 0x01},
        {0x02, 0x01, 0xe7, 0x00}, {0x09, 0x01, 0xe7, 0x00}, {0x17, 0x01, 0xe7, 0x00}, {0x28, 0x01, 0xe7, 0x01}
    }, {
        {0x03, 0x01, 0xbc, 0x00}, {0x06, 0x01, 0xbc, 0x00}, {0x0a, 0x01, 0xbc, 0x00}, {0x0f, 0x01, 0xbc, 0x00},
        {0x18, 0x01, 0xbc, 0x00}, {0x1f, 0x01, 0xbc, 0x00}, {0x29, 0x01, 0xbc, 0x00}, {0x38, 0x01, 0xbc, 0x01},
        {0x03, 0x01, 0xbf, 0x00}, {0x06, 0x01, 0xbf, 0x00}, {0x0a, 0x01, 0xbf, 0x00}, {0x0f, 0x01, 0xbf, 0x00},
        {0x18, 0x01, 0xbf, 0x00}, {0x1f, 0x01, 0xbf, 0x00}, {0x29, 0x01, 0xbf, 0x00}, {0x38, 0x01, 0xbf, 0x01}
    }, {
        {0x03, 0x01, 0xc5, 0x00}, {0x06, 0x01, 0xc5, 0x00}, {0x0a, 0x01, 0xc5, 0x00}, {0x0f, 0x01, 0xc5, 0x00},
        {0x18, 0x01, 0xc5, 0x00}, {0x1f, 0x01, 0xc5, 0x00}, {0x29, 0x01, 0xc5, 0x00}, {0x38, 0x01, 0xc5, 0x01},
        {0x03, 0x01, 0xe7, 0x00}, {0x06, 0x01, 0xe7, 0x00}, {0x0a, 0x01, 0xe7, 0x00}, {0x0f, 0x01, 0xe7, 0x00},
        {0x18, 0x01, 0xe7, 0x00}, {0x1f, 0x01, 0xe7, 0x00}, {0x29, 0x01, 0xe7, 0x00}, {0x38, 0x01, 0xe7, 0x01}
    }, {
        {0x02, 0x01, 0xef, 0x00}, {0x09, 0x01, 0xef, 0x00}, {0x17, 0x01, 0xef, 0x00}, {0x28, 0x01, 0xef, 0x01},
        {0x01, 0x01, 0x09, 0x00}, {0x16, 0x01, 0x09, 0x01}, {0x01, 0x01, 0x8e, 0x00}, {0x16, 0x01, 0x8e, 0x01},
        {0x01, 0x01, 0x90, 0x00}, {0x16, 0x01, 0x90, 0x01}, {0x01, 0x01, 0x91, 0x00}, {0x16, 0x01, 0x91, 0x01},
        {0x01, 0x01, 0x94, 0x00}, {0x16, 0x01, 0x94, 0x01}, {0x01, 0x01, 0x9f, 0x00}, {0x16, 0x01, 0x9f, 0x01}
    }, {
        {0x03, 0x01, 0xef, 0x00}, {0x06, 0x01, 0xef, 0x00}, {0x0a, 0x01, 0xef, 0x00}, {0x0f, 0x01, 0xef, 0x00},
        {0x18, 0x01, 0xef, 0x00}, {0x1f, 0x01, 0xef, 0x00}, {0x29, 0x01, 0xef, 0x00}, {0x38, 0x01, 0xef, 0x01},
        {0x02, 0x01, 0x09, 0x00}, {0x09, 0x01, 0x09, 0x00}, {0x17, 0x01, 0x09, 0x00}, {0x28, 0x01, 0x09, 0x01},
        {0x02, 0x01, 0x8e, 0x00}, {0x09, 0x01, 0x8e, 0x00}, {0x17, 0x01, 0x8e, 0x00}, {0x28, 0x01, 0x8e, 0x01}
    }, {
        {0x03, 0x01, 0x09, 0x00}, {0x06, 0x01, 0x09, 0x00}, {0x0a, 0x01, 0x09, 0x00}, {0x0f, 0x01, 0x09, 0x00},
        {0x18, 0x01, 0x09, 0x00}, {0x1f, 0x01, 0x09, 0x00}, {0x29, 0x01, 0x09, 0x00}, {0x38, 0x01, 0x09, 0x01},
        {0x03, 0x01, 0x8e, 0x00}, {0x06, 0x01, 0x8e, 0x00}, {0x0a, 0x01, 0x8e, 0x00}, {0x0f, 0x01, 0x8e, 0x00},
        {0x18, 0x01, 0x8e, 0x00}, {0x1f, 0x01, 0x8e, 0x00}, {0x29, 0x01, 0x8e, 0x00}, {0x38, 0x01, 0x8e, 0x01}
    }, {
        {0x02, 0x01, 0x90, 0x00}, {0x09, 0x01, 0x90, 0x00}, {0x17, 0x01, 0x90, 0x00}, {0x28, 0x01, 0x90, 0x01},
        {0x02, 0x01, 0x91, 0x00}, {0x09, 0x01, 0x91, 0x00}, {0x17, 0x01, 0x91, 0x00}, {0x28, 0x01, 0x91, 0x01},
        {0x02, 0x01, 0x94, 0x00}, {0x09, 0x01, 0x94, 0x00}, {0x17, 0x01, 0x94, 0x00}, {0x28, 0x01, 0x94, 0x01},
        {0x02, 0x01, 0x9f, 0x00}, {0x09, 0x01, 0x9f, 0x00}, {0x17, 0x01, 0x9f, 0x00}, {0x28, 0x01, 0x9f, 0x01}
    }, {
        {0x03, 0x01, 0x90, 0x00}, {0x06, 0x01, 0x90, 0x00}, {0x0a, 0x01, 0x90, 0x00}, {0x0f, 0x01, 0x90, 0x00},
        {0x18, 0x01, 0x90, 0x00}, {0x1f, 0x01, 0x90, 0x00}, {0x29, 0x01, 0x90, 0x00}, {0x38, 0x01, 0x90, 0x01},
        {0x03, 0x01, 0x91, 0x00}, {0x06, 0x01, 0x91, 0x00}, {0x0a, 0x01, 0x91, 0x00}, {0x0f, 0x01, 0x91, 0x00},
        {0x18, 0x01, 0x91, 0x00}, {0x1f, 0x01, 0x91, 0x00}, {0x29, 0x01, 0x91, 0x00}, {0x38, 0x01, 0x91, 0x01}
    }, {
        {0x03, 0x01, 0x94, 0x00}, {0x06, 0x01, 0x94, 0x00}, {0x0a, 0x01, 0x94, 0x00}, {0x0f, 0x01, 0x94, 0x00},
        {0x18, 0x01, 0x94, 0x00}, {0x1f, 0x01, 0x94, 0x00}, {0x29, 0x01, 0x94, 0x00}, {0x38, 0x01, 0x94, 0x01},
        {0x03, 0x01, 0x9f, 0x00}, {0x06, 0x01, 0x9f, 0x00}, {0x0a, 0x01, 0x9f, 0x00}, {0x0f, 0x01, 0x9f, 0x00},
        {0x18, 0x01, 0x9f, 0x00}, {0x1f, 0x01, 0x9f, 0x00}, {0x29, 0x01, 0x9f, 0x00}, {0x38, 0x01, 0x9f, 0x01}
    }, {
        {0x00, 0x01, 0xab, 0x01}, {0x00, 0x01, 0xce, 0x01}, {0x00, 0x01, 0xd7, 0x01}, {0x00, 0x01, 0xe1, 0x01},
        {0x00, 0x01, 0xec, 0x01}, {0x00, 0x01, 0xed, 0x01}, {0xbc, 0x00, 0x00, 0x00}, {0xbd, 0x00, 0x00, 0x00},
        {0xc1, 0x00, 0x00, 0x00}, {0xc4, 0x00, 0x00, 0x00}, {0xc8, 0x00, 0x00, 0x00}, {0xcb, 0x00, 0x00, 0x00},
        {0xd1, 0x00, 0x00, 0x00}, {0xd8, 0x00, 0x00, 0x00}, {0xe0, 0x00, 0x00, 0x00}, {0xee, 0x00, 0x00, 0x01}
    }, {
        {0x01, 0x01, 0xab, 0x00}, {0x16, 0x01, 0xab, 0x01}, {0x01, 0x01, 0xce, 0x00}, {0x16, 0x01, 0xce, 0x01},
        {0x01, 0x01, 0xd7, 0x00}, {0x16, 0x01, 0xd7, 0x01}, {0x01, 0x01, 0xe1, 0x00}, {0x16, 0x01, 0xe1, 0x01},
        {0x01, 0x01, 0xec, 0x00}, {0x16, 0x01, 0xec, 0x01}, {0x01, 0x01, 0xed, 0x00}, {0x16, 0x01, 0xed, 0x01},
        {0x00, 0x01, 0xc7, 0x01}, {0x00, 0x01, 0xcf, 0x01}, {0x00, 0x01, 0xea, 0x01}, {0x00, 0x01, 0xeb, 0x01}
    }, {
        {0x02, 0x01, 0xab, 0x00}, {0x09, 0x01, 0xab, 0x00}, {0x17, 0x01, 0xab, 0x00}, {0x28, 0x01, 0xab, 0x01},
        {0x02, 0x01, 0xce, 0x00}, {0x09, 0x01, 0xce, 0x00}, {0x17, 0x01, 0xce, 0x00}, {0x28, 0x01, 0xce, 0x01},
        {0x02, 0x01, 0xd7, 0x00}, {0x09, 0x01, 0xd7, 0x00}, {0x17, 0x01, 0xd7, 0x00}, {0x28, 0x01, 0xd7, 0x01},
        {0x02, 0x01, 0xe1, 0x00}, {0x09, 0x01, 0xe1, 0x00}, {0x17, 0x01, 0xe1, 0x00}, {0x28, 0x01, 0xe1, 0x01}
    }, {
        {0x03, 0x01, 0xab, 0x00}, {0x06, 0x01, 0xab, 0x00}, {0x0a, 0x01, 0xab, 0x00}, {0x0f, 0x01, 0xab, 0x00},
        {0x18, 0x01, 0xab, 0x00}, {0x1f, 0x01, 0xab, 0x00}, {0x29, 0x01, 0xab, 0x00}, {0x38, 0x01, 0xab, 0x01},
        {0x03, 0x01, 0xce, 0x00}, {0x06, 0x01, 0xce, 0x00}, {0x0a, 0x01, 0xce, 0x00}, {0x0f, 0x01, 0xce, 0x00},
        {0x18, 0x01, 0xce, 0x00}, {0x1f, 0x01, 0xce, 0x00}, {0x29, 0x01, 0xce, 0x00}, {0x38, 0x01, 0xce, 0x01}
    }, {
        {0x03, 0x01, 0xd7, 0x00}, {0x06, 0x01, 0xd7, 0x00}, {0x0a, 0x01, 0xd7, 0x00}, {0x0f, 0x01, 0xd7, 0x00},
        {0x18, 0x01, 0xd7, 0x00}, {0x1f, 0x01, 0xd7, 0x00}, {0x29, 0x01, 0xd7, 0x00}, {0x38, 0x01, 0xd7, 0x01},
        {0x03, 0x01, 0xe1, 0x00}, {0x06, 0x01, 0xe1, 0x00}, {0x0a, 0x01, 0xe1, 0x00}, {0x0f, 0x01, 0xe1, 0x00},
        {0x18, 0x01, 0xe1, 0x00}, {0x1f, 0x01, 0xe1, 0x00}, {0x29, 0x01, 0xe1, 0x00}, {0x38, 0x01, 0xe1, 0x01}
    }, {
        {0x02, 0x01, 0xec, 0x00}, {0x09, 0x01, 0xec, 0x00}, {0x17, 0x01, 0xec, 0x00}, {0x28, 0x01, 0xec, 0x01},
        {0x02, 0x01, 0xed, 0x00}, {0x09, 0x01, 0xed, 0x00}, {0x17, 0x01, 0xed, 0x00}, {0x28, 0x01, 0xed, 0x01},
        {0x01, 0x01, 0xc7, 0x00}, {0x16, 0x01, 0xc7, 0x01}, {0x01, 0x01, 0xcf, 0x00}, {0x16, 0x01, 0xcf, 0x01},
        {0x01, 0x01, 0xea, 0x00}, {0x16, 0x01, 0xea, 0x01}, {0x01, 0x01, 0xeb, 0x00}, {0x16, 0x01, 0xeb, 0x01}
    }, {
        {0x03, 0x01, 0xec, 0x00}, {0x06, 0x01, 0xec, 0x00}, {0x0a, 0x01, 0xec, 0x00}, {0x0f, 0x01, 0xec, 0x00},
        {0x18, 0x01, 0xec, 0x00}, {0x1f, 0x01, 0xec, 0x00}, {0x29, 0x01, 0xec, 0x00}, {0x38, 0x01, 0xec, 0x01},
        {0x03, 0x01, 0xed, 0x00}, {0x06, 0x01, 0xed, 0x00}, {0x0a, 0x01, 0xed, 0x00}, {0x0f, 0x01, 0xed, 0x00},
        {0x18, 0x01, 0xed, 0x00}, {0x1f, 0x01, 0xed, 0x00}, {0x29, 0x01, 0xed, 0x00}, {0x38, 0x01, 0xed, 0x01}
    }, {
        {0x02, 0x01, 0xc7, 0x00}, {0x09, 0x01, 0xc7, 0x00}, {0x17, 0x01, 0xc7, 0x00}, {0x28, 0x01, 0xc7, 0x01},
        {0x02, 0x01, 0xcf, 0x00}, {0x09, 0x01, 0xcf, 0x00}, {0x17, 0x01, 0xcf, 0x00}, {0x28, 0x01, 0xcf, 0x01},
        {0x02, 0x01, 0xea, 0x00}, {0x09, 0x01, 0xea, 0x00}, {0x17, 0x01, 0xea, 0x00}, {0x28, 0x01, 0xea, 0x01},
        {0x02, 0x01, 0xeb, 0x00}, {0x09, 0x01, 0xeb, 0x00}, {0x17, 0x01, 0xeb, 0x00}, {0x28, 0x01, 0xeb, 0x01}
    }, {
        {0x03, 0x01, 0xc7, 0x00}, {0x06, 0x01, 0xc7, 0x00}, {0x0a, 0x01, 0xc7, 0x00}, {0x0f, 0x01, 0xc7, 0x00},
        {0x18, 0x01, 0xc7, 0x00}, {0x1f, 0x01, 0xc7, 0x00}, {0x29, 0x01, 0xc7, 0x00}, {0x38, 0x01, 0xc7, 0x01},
        {0x03, 0x01, 0xcf, 0x00}, {0x06, 0x01, 0xcf, 0x00}, {0x0a, 0x01, 0xcf, 0x00}, {0x0f, 0x01, 0xcf, 0x00},
        {0x18, 0x01, 0xcf, 0x00}, {0x1f, 0x01, 0xcf, 0x00}, {0x29, 0x01, 0xcf, 0x00}, {0x38, 0x01, 0xcf, 0x01}
    }, {
        {0x03, 0x01, 0xea, 0x00}, {0x06, 0x01, 0xea, 0x00}, {0x0a, 0x01, 0xea, 0x00}, {0x0f, 0x01, 0xea, 0x00},
        {0x18, 0x01, 0xea, 0x00}, {0x1f, 0x01, 0xea, 0x00}, {0x29, 0x01, 0xea, 0x00}, {0x38, 0x01, 0xea, 0x01},
        {0x03, 0x01, 0xeb, 0x00}, {0x06, 0x01, 0xeb, 0x00}, {0x0a, 0x01, 0xeb, 0x00}, {0x0f, 0x01, 0xeb, 0x00},
        {0x18, 0x01, 0xeb, 0x00}, {0x1f, 0x01, 0xeb, 0x00}, {0x29, 0x01, 0xeb, 0x00}, {0x38, 0x01, 0xeb, 0x01}
    }, {
        {0xc2, 0x00, 0x00, 0x00}, {0xc3, 0x00, 0x00, 0x00}, {0xc5, 0x00, 0x00, 0x00}, {0xc6, 0x00, 0x00, 0x00},
        {0xc9, 0x00, 0x00, 0x00}, {0xca, 0x00, 0x00, 0x00}, {0xcc, 0x00, 0x00, 0x00}, {0xcd, 0x00, 0x00, 0x00},
        {0xd2, 0x00, 0x00, 0x00}, {0xd5, 0x00, 0x00, 0x00}, {0xd9, 0x00, 0x00, 0x00}, {0xdc, 0x00, 0x00, 0x00},
        {0xe1, 0x00, 0x00, 0x00}, {0xe7, 0x00, 0x00, 0x00}, {0xef, 0x00, 0x00, 0x00}, {0xf6, 0x00, 0x00, 0x01}
    }, {
        {0x00, 0x01, 0xc0, 0x01}, {0x00, 0x01, 0xc1, 0x01}, {0x00, 0x01, 0xc8, 0x01}, {0x00, 0x01, 0xc9, 0x01},
        {0x00, 0x01, 0xca, 0x01}, {0x00, 0x01, 0xcd, 0x01}, {0x00, 0x01, 0xd2, 0x01}, {0x00, 0x01, 0xd5, 0x01},
        {0x00, 0x01, 0xda, 0x01}, {0x00, 0x01, 0xdb, 0x01}, {0x00, 0x01, 0xee, 0x01}, {0x00, 0x01, 0xf0, 0x01},
        {0x00, 0x01, 0xf2, 0x01}, {0x00, 0x01, 0xf3, 0x01}, {0x00, 0x01, 0xff, 0x01}, {0xce, 0x00, 0x00, 0x00}
    }, {
        {0x01, 0x01, 0xc0, 0x00}, {0x16, 0x01, 0xc0, 0x01}, {0x01, 0x01, 0xc1, 0x00}, {0x16, 0x01, 0xc1, 0x01},
        {0x01, 0x01, 0xc8, 0x00}, {0x16, 0x01, 0xc8, 0x01}, {0x01, 0x01, 0xc9, 0x00}, {0x16, 0x01, 0xc9, 0x01},
        {0x01, 0x01, 0xca, 0x00}, {0x16, 0x01, 0xca, 0x01}, {0x01, 0x01, 0xcd, 0x00}, {0x16, 0x01, 0xcd, 0x01},
        {0x01, 0x01, 0xd2, 0x00}, {0x16, 0x01, 0xd2, 0x01}, {0x01, 0x01, 0xd5, 0x00}, {0x16, 0x01, 0xd5, 0x01}
    }, {
        {0x02, 0x01, 0xc0, 0x00}, {0x09, 0x01, 0xc0, 0x00}, {0x17, 0x01, 0xc0, 0x00}, {0x28, 0x01, 0xc0, 0x01},
        {0x02, 0x01, 0xc1, 0x00}, {0x09, 0x01, 0xc1, 0x00}, {0x17, 0x01, 0xc1, 0x00}, {0x28, 0x01, 0xc1, 0x01},
        {0x02, 0x01, 0xc8, 0x00}, {0x09, 0x01, 0xc8, 0x00}, {0x17, 0x01, 0xc8, 0x00}, {0x28, 0x01, 0xc8, 0x01},
        {0x02, 0x01, 0xc9, 0x00}, {0x09, 0x01, 0xc9, 0x00}, {0x17, 0x01, 0xc9, 0x00}, {0x28, 0x01, 0xc9, 0x01}
    }, {
        {0x03, 0x01, 0xc0, 0x00}, {0x06, 0x01, 0xc0, 0x00}, {0x0a, 0x01, 0xc0, 0x00}, {0x0f, 0x01, 0xc0, 0x00},
        {0x18, 0x01, 0xc0, 0x00}, {0x1f, 0x01, 0xc0, 0x00}, {0x29, 0x01, 0xc0, 0x00}, {0x38, 0x01, 0xc0, 0x01},
        {0x03, 0x01, 0xc1, 0x00}, {0x06, 0x01, 0xc1, 0x00}, {0x0a, 0x01, 0xc1, 0x00}, {0x0f, 0x01, 0xc1, 0x00},
        {0x18, 0x01, 0xc1, 0x00}, {0x1f, 0x01, 0xc1, 0x00}, {0x29, 0x01, 0xc1, 0x00}, {0x38, 0x01, 0xc1, 0x01}
    }, {
        {0x03, 0x01, 0xc8, 0x00}, {0x06, 0x01, 0xc8, 0x00}, {0x0a, 0x01, 0xc8, 0x00}, {0x0f, 0x01, 0xc8, 0x00},
        {0x18, 0x01, 0xc8, 0x00}, {0x1f, 0x01, 0xc8, 0x00}, {0x29, 0x01, 0xc8, 0x00}, {0x38, 0x01, 0xc8, 0x01},
        {0x03, 0x01, 0xc9, 0x00}, {0x06, 0x01, 0xc9, 0x00}, {0x0a, 0x01, 0xc9, 0x00}, {0x0f, 0x01, 0xc9, 0x00},
        {0x18, 0x01, 0xc9, 0x00}, {0x1f, 0x01, 0xc9, 0x00}, {0x29, 0x01, 0xc9, 0x00}, {0x38, 0x01, 0xc9, 0x01}
    }, {
        {0x02, 0x01, 0xca, 0x00}, {0x09, 0x01, 0xca, 0x00}, {0x17, 0x01, 0xca, 0x00}, {0x28, 0x01, 0xca, 0x01},
        {0x02, 0x01, 0xcd, 0x00}, {0x09, 0x01, 0xcd, 0x00}, {0x17, 0x01, 0xcd, 0x00}, {0x28, 0x01, 0xcd, 0x01},
        {0x02, 0x01, 0xd2, 0x00}, {0x09, 0x01, 0xd2, 0x00}, {0x17, 0x01, 0xd2, 0x00}, {0x28, 0x01, 0xd2, 0x01},
        {0x02, 0x01, 0xd5, 0x00}, {0x09, 0x01, 0xd5, 0x00}, {0x17, 0x01, 0xd5, 0x00}, {0x28, 0x01, 0xd5, 0x01}
    }, {
        {0x03, 0x01, 0xca, 0x00}, {0x06, 0x01, 0xca, 0x00}, {0x0a, 0x01, 0xca, 0x00}, {0x0f, 0x01, 0xca, 0x00},
        {0x18, 0x01, 0xca, 0x00}, {0x1f, 0x01, 0xca, 0x00}, {0x29, 0x01, 0xca, 0x00}, {0x38, 0x01, 0xca, 0x01},
        {0x03, 0x01, 0xcd, 0x00}, {0x06, 0x01, 0xcd, 0x00}, {0x0a, 0x01, 0xcd, 0x00}, {0x0f, 0x01, 0xcd, 0x00},
        {0x18, 0x01, 0xcd, 0x00}, {0x1f, 0x01, 0xcd, 0x00}, {0x29, 0x01, 0xcd, 0x00}, {0x38, 0x01, 0xcd, 0x01}
    }, {
        {0x03, 0x01, 0xd2, 0x00}, {0x06, 0x01, 0xd2, 0x00}, {0x0a, 0x01, 0xd2, 0x00}, {0x0f, 0x01, 0xd2, 0x00},
        {0x18, 0x01, 0xd2, 0x00}, {0x1f, 0x01, 0xd2, 0x00}, {0x29, 0x01, 0xd2, 0x00}, {0x38, 0x01, 0xd2, 0x01},
        {0x03, 0x01, 0xd5, 0x00}, {0x06, 0x01, 0xd5, 0x00}, {0x0a, 0x01, 0xd5, 0x00}, {0x0f, 0x01, 0xd5, 0x00},
        {0x18, 0x01, 0xd5, 0x00}, {0x1f, 0x01, 0xd5, 0x00}, {0x29, 0x01, 0xd5, 0x00}, {0x38, 0x01, 0xd5, 0x01}
    }, {
        {0x01, 0x01, 0xda, 0x00}, {0x16, 0x01, 0xda, 0x01}, {0x01, 0x01, 0xdb, 0x00}, {0x16, 0x01, 0xdb, 0x01},
        {0x01, 0x01, 0xee, 0x00}, {0x16, 0x01, 0xee, 0x01}, {0x01, 0x01, 0xf0, 0x00}, {0x16, 0x01, 0xf0, 0x01},
        {0x01, 0x01, 0xf2, 0x00}, {0x16, 0x01, 0xf2, 0x01}, {0x01, 0x01, 0xf3, 0x00}, {0x16, 0x01, 0xf3, 0x01},
        {0x01, 0x01, 0xff, 0x00}, {0x16, 0x01, 0xff, 0x01}, {0x00, 0x01, 0xcb, 0x01}, {0x00, 0x01, 0xcc, 0x01}
    }, {
        {0x02, 0x01, 0xda, 0x00}, {0x09, 0x01, 0xda, 0x00}, {0x17, 0x01, 0xda, 0x00}, {0x28, 0x01, 0xda, 0x01},
        {0x02, 0x01, 0xdb, 0x00}, {0x09, 0x01, 0xdb, 0x00}, {0x17, 0x01, 0xdb, 0x00}, {0x28, 0x01, 0xdb, 0x01},
        {0x02, 0x01, 0xee, 0x00}, {0x09, 0x01, 0xee, 0x00}, {0x17, 0x01, 0xee, 0x00}, {0x28, 0x01, 0xee, 0x01},
        {0x02, 0x01, 0xf0, 0x00}, {0x09, 0x01, 0xf0, 0x00}, {0x17, 0x01, 0xf0, 0x00}, {0x28, 0x01, 0xf0, 0x01}
    }, {
        {0x03, 0x01, 0xda, 0x00}, {0x06, 0x01, 0xda, 0x00}, {0x0a, 0x01, 0xda, 0x00}, {0x0f, 0x01, 0xda, 0x00},
        {0x18, 0x01, 0xda, 0x00}, {0x1f, 0x01, 0xda, 0x00}, {0x29, 0x01, 0xda, 0x00}, {0x38, 0x01, 0xda, 0x01},
        {0x03, 0x01, 0xdb, 0x00}, {0x06, 0x01, 0xdb, 0x00}, {0x0a, 0x01, 0xdb, 0x00}, {0x0f, 0x01, 0xdb, 0x00},
        {0x18, 0x01, 0xdb, 0x00}, {0x1f, 0x01, 0xdb, 0x00}, {0x29, 0x01, 0xdb, 0x00}, {0x38, 0x01, 0xdb, 0x01}
    }, {
        {0x03, 0x01, 0xee, 0x00}, {0x06, 0x01, 0xee, 0x00}, {0x0a, 0x01, 0xee, 0x00}, {0x0f, 0x01, 0xee, 0x00},
        {0x18, 0x01, 0xee, 0x00}, {0x1f, 0x01, 0xee, 0x00}, {0x29, 0x01, 0xee, 0x00}, {0x38, 0x01, 0xee, 0x01},
        {0x03, 0x01, 0xf0, 0x00}, {0x06, 0x01, 0xf0, 0x00}, {0x0a, 0x01, 0xf0, 0x00}, {0x0f, 0x01, 0xf0, 0x00},
        {0x18, 0x01, 0xf0, 0x00}, {0x1f, 0x01, 0xf0, 0x00}, {0x29, 0x01, 0xf0, 0x00}, {0x38, 0x01, 0xf0, 0x01}
    }, {
        {0x02, 0x01, 0xf2, 0x00}, {0x09, 0x01, 0xf2, 0x00}, {0x17, 0x01, 0xf2, 0x00}, {0x28, 0x01, 0xf2, 0x01},
        {0x02, 0x01, 0xf3, 0x00}, {0x09, 0x01, 0xf3, 0x00}, {0x17, 0x01, 0xf3, 0x00}, {0x28, 0x01, 0xf3, 0x01},
        {0x02, 0x01, 0xff, 0x00}, {0x09, 0x01, 0xff, 0x00}, {0x17, 0x01, 0xff, 0x00}, {0x28, 0x01, 0xff, 0x01},
        {0x01, 0x01, 0xcb, 0x00}, {0x16, 0x01, 0xcb, 0x01}, {0x01, 0x01, 0xcc, 0x00}, {0x16, 0x01, 0xcc, 0x01}
    }, {
        {0x03, 0x01, 0xf2, 0x00}, {0x06, 0x01, 0xf2, 0x00}, {0x0a, 0x01, 0xf2, 0x00}, {0x0f, 0x01, 0xf2, 0x00},
        {0x18, 0x01, 0xf2, 0x00}, {0x1f, 0x01, 0xf2, 0x00}, {0x29, 0x01, 0xf2, 0x00}, {0x38, 0x01, 0xf2, 0x01},
        {0x03, 0x01, 0xf3, 0x00}, {0x06, 0x01, 0xf3, 0x00}, {0x0a, 0x01, 0xf3, 0x00}, {0x0f, 0x01, 0xf3, 0x00},
        {0x18, 0x01, 0xf3, 0x00}, {0x1f, 0x01, 0xf3, 0x00}, {0x29, 0x01, 0xf3, 0x00}, {0x38, 0x01, 0xf3, 0x01}
    }, {
        {0x03, 0x01, 0xff, 0x00}, {0x06, 0x01, 0xff, 0x00}, {0x0a, 0x01, 0xff, 0x00}, {0x0f, 0x01, 0xff, 0x00},
        {0x18, 0x01, 0xff, 0x00}, {0x1f, 0x01, 0xff, 0x00}, {0x29, 0x01, 0xff, 0x00}, {0x38, 0x01, 0xff, 0x01},
        {0x02, 0x01, 0xcb, 0x00}, {0x09, 0x01, 0xcb, 0x00}, {0x17, 0x01, 0xcb, 0x00}, {0x28, 0x01, 0xcb, 0x01},
        {0x02, 0x01, 0xcc, 0x00}, {0x09, 0x01, 0xcc, 0x00}, {0x17, 0x01, 0xcc, 0x00}, {0x28, 0x01, 0xcc, 0x01}
    }, {
        {0x03, 0x01, 0xcb, 0x00}, {0x06, 0x01, 0xcb, 0x00}, {0x0a, 0x01, 0xcb, 0x00}, {0x0f, 0x01, 0xcb, 0x00},
        {0x18, 0x01, 0xcb, 0x00}, {0x1f, 0x01, 0xcb, 0x00}, {0x29, 0x01, 0xcb, 0x00}, {0x38, 0x01, 0xcb, 0x01},
        {0x03, 0x01, 0xcc, 0x00}, {0x06, 0x01, 0xcc, 0x00}, {0x0a, 0x01, 0xcc, 0x00}, {0x0f, 0x01, 0xcc, 0x00},
        {0x18, 0x01, 0xcc, 0x00}, {0x1f, 0x01, 0xcc, 0x00}, {0x29, 0x01, 0xcc, 0x00}, {0x38, 0x01, 0xcc, 0x01}
    }, {
        {0xd3, 0x00, 0x00, 0x00}, {0xd4, 0x00, 0x00, 0x00}, {0xd6, 0x00, 0x00, 0x00}, {0xd7, 0x00, 0x00, 0x00},
        {0xda, 0x00, 0x00, 0x00}, {0xdb, 0x00, 0x00, 0x00}, {0xdd, 0x00, 0x00, 0x00}, {0xde, 0x00, 0x00, 0x00},
        {0xe2, 0x00, 0x00, 0x00}, {0xe4, 0x00, 0x00, 0x00}, {0xe8, 0x00, 0x00, 0x00}, {0xeb, 0x00, 0x00, 0x00},
        {0xf0, 0x00, 0x00, 0x00}, {0xf3, 0x00, 0x00, 0x00}, {0xf7, 0x00, 0x00, 0x00}, {0xfa, 0x00, 0x00, 0x01}
    }, {
        {0x00, 0x01, 0xd3, 0x01}, {0x00, 0x01, 0xd4, 0x01}, {0x00, 0x01, 0xd6, 0x01}, {0x00, 0x01, 0xdd, 0x01},
        {0x00, 0x01, 0xde, 0x01}, {0x00, 0x01, 0xdf, 0x01}, {0x00, 0x01, 0xf1, 0x01}, {0x00, 0x01, 0xf4, 0x01},
        {0x00, 0x01, 0xf5, 0x01}, {0x00, 0x01, 0xf6, 0x01}, {0x00, 0x01, 0xf7, 0x01}, {0x00, 0x01, 0xf8, 0x01},
        {0x00, 0x01, 0xfa, 0x01}, {0x00, 0x01, 0xfb, 0x01}, {0x00, 0x01, 0xfc, 0x01}, {0x00, 0x01, 0xfd, 0x01}
    }, {
        {0x01, 0x01, 0xd3, 0x00}, {0x16, 0x01, 0xd3, 0x01}, {0x01, 0x01, 0xd4, 0x00}, {0x16, 0x01, 0xd4, 0x01},
        {0x01, 0x01, 0xd6, 0x00}, {0x16, 0x01, 0xd6, 0x01}, {0x01, 0x01, 0xdd, 0x00}, {0x16, 0x01, 0xdd, 0x01},
        {0x01, 0x01, 0xde, 0x00}, {0x16, 0x01, 0xde, 0x01}, {0x01, 0x01, 0xdf, 0x00}, {0x16, 0x01, 0xdf, 0x01},
        {0x01, 0x01, 0xf1, 0x00}, {0x16, 0x01, 0xf1, 0x01}, {0x01, 0x01, 0xf4, 0x00}, {0x16, 0x01, 0xf4, 0x01}
    }, {
        {0x02, 0x01, 0xd3, 0x00}, {0x09, 0x01, 0xd3, 0x00}, {0x17, 0x01, 0xd3, 0x00}, {0x28, 0x01, 0xd3, 0x01},
        {0x02, 0x01, 0xd4, 0x00}, {0x09, 0x01, 0xd4, 0x00}, {0x17, 0x01, 0xd4, 0x00}, {0x28, 0x01, 0xd4, 0x01},
        {0x02, 0x01, 0xd6, 0x00}, {0x09, 0x01, 0xd6, 0x00}, {0x17, 0x01, 0xd6, 0x00}, {0x28, 0x01, 0xd6, 0x01},
        {0x02, 0x01, 0xdd, 0x00}, {0x09, 0x01, 0xdd, 0x00}, {0x17, 0x01, 0xdd, 0x00}, {0x28, 0x01, 0xdd, 0x01}
    }, {
        {0x03, 0x01, 0xd3, 0x00}, {0x06, 0x01, 0xd3, 0x00}, {0x0a, 0x01, 0xd3, 0x00}, {0x0f, 0x01, 0xd3, 0x00},
        {0x18, 0x01, 0xd3, 0x00}, {0x1f, 0x01, 0xd3, 0x00}, {0x29, 0x01, 0xd3, 0x00}, {0x38, 0x01, 0xd3, 0x01},
        {0x03, 0x01, 0xd4, 0x00}, {0x06, 0x01, 0xd4, 0x00}, {0x0a, 0x01, 0xd4, 0x00}, {0x0f, 0x01, 0xd4, 0x00},
        {0x18, 0x01, 0xd4, 0x00}, {0x1f, 0x01, 0xd4, 0x00}, {0x29, 0x01, 0xd4, 0x00}, {0x38, 0x01, 0xd4, 0x01}
    }, {
        {0x03, 0x01, 0xd6, 0x00}, {0x06, 0x01, 0xd6, 0x00}, {0x0a, 0x01, 0xd6, 0x00}, {0x0f, 0x01, 0xd6, 0x00},
        {0x18, 0x01, 0xd6, 0x00}, {0x1f, 0x01, 0xd6, 0x00}, {0x29, 0x01, 0xd6, 0x00}, {0x38, 0x01, 0xd6, 0x01},
        {0x03, 0x01, 0xdd, 0x00}, {0x06, 0x01, 0xdd, 0x00}, {0x0a, 0x01, 0xdd, 0x00}, {0x0f, 0x01, 0xdd, 0x00},
        {0x18, 0x01, 0xdd, 0x00}, {0x1f, 0x01, 0xdd, 0x00}, {0x29, 0x01, 0xdd, 0x00}, {0x38, 0x01, 0xdd, 0x01}
    }, {
        {0x02, 0x01, 0xde, 0x00}, {0x09, 0x01, 0xde, 0x00}, {0x17, 0x01, 0xde, 0x00}, {0x28, 0x01, 0xde, 0x01},
        {0x02, 0x01, 0xdf, 0x00}, {0x09, 0x01, 0xdf, 0x00}, {0x17, 0x01, 0xdf, 0x00}, {0x28, 0x01, 0xdf, 0x01},
        {0x02, 0x01, 0xf1, 0x00}, {0x09, 0x01, 0xf1, 0x00}, {0x17, 0x01, 0xf1, 0x00}, {0x28, 0x01, 0xf1, 0x01},
        {0x02, 0x01, 0xf4, 0x00}, {0x09, 0x01, 0xf4, 0x00}, {0x17, 0x01, 0xf4, 0x00}, {0x28, 0x01, 0xf4, 0x01}
    }, {
        {0x03, 0x01, 0xde, 0x00}, {0x06, 0x01, 0xde, 0x00}, {0x0a, 0x01, 0xde, 0x00}, {0x0f, 0x01, 0xde, 0x00},
        {0x18, 0x01, 0xde, 0x00}, {0x1f, 0x01, 0xde, 0x00}, {0x29, 0x01, 0xde, 0x00}, {0x38, 0x01, 0xde, 0x01},
        {0x03, 0x01, 0xdf, 0x00}, {0x06, 0x01, 0xdf, 0x00}, {0x0a, 0x01, 0xdf, 0x00}, {0x0f, 0x01, 0xdf, 0x00},
        {0x18, 0x01, 0xdf, 0x00}, {0x1f, 0x01, 0xdf, 0x00}, {0x29, 0x01, 0xdf, 0x00}, {0x38, 0x01, 0xdf, 0x01}
    }, {
        {0x03, 0x01, 0xf1, 0x00}, {0x06, 0x01, 0xf1, 0x00}, {0x0a, 0x01, 0xf1, 0x00}, {0x0f, 0x01, 0xf1, 0x00},
        {0x18, 0x01, 0xf1, 0x00}, {0x1f, 0x01, 0xf1, 0x00}, {0x29, 0x01, 0xf1, 0x00}, {0x38, 0x01, 0xf1, 0x01},
        {0x03, 0x01, 0xf4, 0x00}, {0x06, 0x01, 0xf4, 0x00}, {0x0a, 0x01, 0xf4, 0x00}, {0x0f, 0x01, 0xf4, 0x00},
        {0x18, 0x01, 0xf4, 0x00}, {0x1f, 0x01, 0xf4, 0x00}, {0x29, 0x01, 0xf4, 0x00}, {0x38, 0x01, 0xf4, 0x01}
    }, {
        {0x01, 0x01, 0xf5, 0x00}, {0x16, 0x01, 0xf5, 0x01}, {0x01, 0x01, 0xf6, 0x00}, {0x16, 0x01, 0xf6, 0x01},
        {0x01, 0x01, 0xf7, 0x00}, {0x16, 0x01, 0xf7, 0x01}, {0x01, 0x01, 0xf8, 0x00}, {0x16, 0x01, 0xf8, 0x01},
        {0x01, 0x01, 0xfa, 0x00}, {0x16, 0x01, 0xfa, 0x01}, {0x01, 0x01, 0xfb, 0x00}, {0x16, 0x01, 0xfb, 0x01},
        {0x01, 0x01, 0xfc, 0x00}, {0x16, 0x01, 0xfc, 0x01}, {0x01, 0x01, 0xfd, 0x00}, {0x16, 0x01, 0xfd, 0x01}
    }, {
        {0x02, 0x01, 0xf5, 0x00}, {0x09, 0x01, 0xf5, 0x00}, {0x17, 0x01, 0xf5, 0x00}, {0x28, 0x01, 0xf5, 0x01},
        {0x02, 0x01, 0xf6, 0x00}, {0x09, 0x01, 0xf6, 0x00}, {0x17, 0x01, 0xf6, 0x00}, {0x28, 0x01, 0xf6, 0x01},
        {0x02, 0x01, 0xf7, 0x00}, {0x09, 0x01, 0xf7, 0x00}, {0x17, 0x01, 0xf7, 0x00}, {0x28, 0x01, 0xf7, 0x01},
        {0x02, 0x01, 0xf8, 0x00}, {0x09, 0x01, 0xf8, 0x00}, {0x17, 0x01, 0xf8, 0x00}, {0x28, 0x01, 0xf8, 0x01}
    }, {
        {0x03, 0x01, 0xf5, 0x00}, {0x06, 0x01, 0xf5, 0x00}, {0x0a, 0x01, 0xf5, 0x00}, {0x0f, 0x01, 0xf5, 0x00},
        {0x18, 0x01, 0xf5, 0x00}, {0x1f, 0x01, 0xf5, 0x00}, {0x29, 0x01, 0xf5, 0x00}, {0x38, 0x01, 0xf5, 0x01},
        {0x03, 0x01, 0xf6, 0x00}, {0x06, 0x01, 0xf6, 0x00}, {0x0a, 0x01, 0xf6, 0x00}, {0x0f, 0x01, 0xf6, 0x00},
        {0x18, 0x01, 0xf6, 0x00}, {0x1f, 0x01, 0xf6, 0x00}, {0x29, 0x01, 0xf6, 0x00}, {0x38, 0x01, 0xf6, 0x01}
    }, {
        {0x03, 0x01, 0xf7, 0x00}, {0x06, 0x01, 0xf7, 0x00}, {0x0a, 0x01, 0xf7, 0x00}, {0x0f, 0x01, 0xf7, 0x00},
        {0x18, 0x01, 0xf7, 0x00}, {0x1f, 0x01, 0xf7, 0x00}, {0x29, 0x01, 0xf7, 0x00}, {0x38, 0x01, 0xf7, 0x01},
        {0x03, 0x01, 0xf8, 0x00}, {0x06, 0x01, 0xf8, 0x00}, {0x0a, 0x01, 0xf8, 0x00}, {0x0f, 0x01, 0xf8, 0x00},
        {0x18, 0x01, 0xf8, 0x00}, {0x1f, 0x01, 0xf8, 0x00}, {0x29, 0x01, 0xf8, 0x00}, {0x38, 0x01, 0xf8, 0x01}
    }, {
        {0x02, 0x01, 0xfa, 0x00}, {0x09, 0x01, 0xfa, 0x00}, {0x17, 0x01, 0xfa, 0x00}, {0x28, 0x01, 0xfa, 0x01},
        {0x02, 0x01, 0xfb, 0x00}, {0x09, 0x01, 0xfb, 0x00}, {0x17, 0x01, 0xfb, 0x00}, {0x28, 0x01, 0xfb, 0x01},
        {0x02, 0x01, 0xfc, 0x00}, {0x09, 0x01, 0xfc, 0x00}, {0x17, 0x01, 0xfc, 0x00}, {0x28, 0x01, 0xfc, 0x01},
        {0x02, 0x01, 0xfd, 0x00}, {0x09, 0x01, 0xfd, 0x00}, {0x17, 0x01, 0xfd, 0x00}, {0x28, 0x01, 0xfd, 0x01}
    }, {
        {0x03, 0x01, 0xfa, 0x00}, {0x06, 0x01, 0xfa, 0x00}, {0x0a, 0x01, 0xfa, 0x00}, {0x0f, 0x01, 0xfa, 0x00},
        {0x18, 0x01, 0xfa, 0x00}, {0x1f, 0x01, 0xfa, 0x00}, {0x29, 0x01, 0xfa, 0x00}, {0x38, 0x01, 0xfa, 0x01},
        {0x03, 0x01, 0xfb, 0x00}, {0x06, 0x01, 0xfb, 0x00}, {0x0a, 0x01, 0xfb, 0x00}, {0x0f, 0x01, 0xfb, 0x00},
        {0x18, 0x01, 0xfb, 0x00}, {0x1f, 0x01, 0xfb, 0x00}, {0x29, 0x01, 0xfb, 0x00}, {0x38, 0x01, 0xfb, 0x01}
    }, {
        {0x03, 0x01, 0xfc, 0x00}, {0x06, 0x01, 0xfc, 0x00}, {0x0a, 0x01, 0xfc, 0x00}, {0x0f, 0x01, 0xfc, 0x00},
        {0x18, 0x01, 0xfc, 0x00}, {0x1f, 0x01, 0xfc, 0x00}, {0x29, 0x01, 0xfc, 0x00}, {0x38, 0x01, 0xfc, 0x01},
        {0x03, 0x01, 0xfd, 0x00}, {0x06, 0x01, 0xfd, 0x00}, {0x0a, 0x01, 0xfd, 0x00}, {0x0f, 0x01, 0xfd, 0x00},
        {0x18, 0x01, 0xfd, 0x00}, {0x1f, 0x01, 0xfd, 0x00}, {0x29, 0x01, 0xfd, 0x00}, {0x38, 0x01, 0xfd, 0x01}
    }, {
        {0x00, 0x01, 0xfe, 0x01}, {0xe3, 0x00, 0x00, 0x00}, {0xe5, 0x00, 0x00, 0x00}, {0xe6, 0x00, 0x00, 0x00},
        {0xe9, 0x00, 0x00, 0x00}, {0xea, 0x00, 0x00, 0x00}, {0xec, 0x00, 0x00, 0x00}, {0xed, 0x00, 0x00, 0x00},
        {0xf1, 0x00, 0x00, 0x00}, {0xf2, 0x00, 0x00, 0x00}, {0xf4, 0x00, 0x00, 0x00}, {0xf5, 0x00, 0x00, 0x00},
        {0xf8, 0x00, 0x00, 0x00}, {0xf9, 0x00, 0x00, 0x00}, {0xfb, 0x00, 0x00, 0x00}, {0xfc, 0x00, 0x00, 0x01}
    }, {
        {0x01, 0x01, 0xfe, 0x00}, {0x16, 0x01, 0xfe, 0x01}, {0x00, 0x01, 0x02, 0x01}, {0x00, 0x01, 0x03, 0x01},
        {0x00, 0x01, 0x04, 0x01}, {0x00, 0x01, 0x05, 0x01}, {0x00, 0x01, 0x06, 0x01}, {0x00, 0x01, 0x07, 0x01},
        {0x00, 0x01, 0x08, 0x01}, {0x00, 0x01, 0x0b, 0x01}, {0x00, 0x01, 0x0c, 0x01}, {0x00, 0x01, 0x0e, 0x01},
        {0x00, 0x01, 0x0f, 0x01}, {0x00, 0x01, 0x10, 0x01}, {0x00, 0x01, 0x11, 0x01}, {0x00, 0x01, 0x12, 0x01}
    }, {
        {0x02, 0x01, 0xfe, 0x00}, {0x09, 0x01, 0xfe, 0x00}, {0x17, 0x01, 0xfe, 0x00}, {0x28, 0x01, 0xfe, 0x01},
        {0x01, 0x01, 0x02, 0x00}, {0x16, 0x01, 0x02, 0x01}, {0x01, 0x01, 0x03, 0x00}, {0x16, 0x01, 0x03, 0x01},
        {0x01, 0x01, 0x04, 0x00}, {0x16, 0x01, 0x04, 0x01}, {0x01, 0x01, 0x05, 0x00}, {0x16, 0x01, 0x05, 0x01},
        {0x01, 0x01, 0x06, 0x00}, {0x16, 0x01, 0x06, 0x01}, {0x01, 0x01, 0x07, 0x00}, {0x16, 0x01, 0x07, 0x01}
    }, {
        {0x03, 0x01, 0xfe, 0x00}, {0x06, 0x01, 0xfe, 0x00}, {0x0a, 0x01, 0xfe, 0x00}, {0x0f, 0x01, 0xfe, 0x00},
        {0x18, 0x01, 0xfe, 0x00}, {0x1f, 0x01, 0xfe, 0x00}, {0x29, 0x01, 0xfe, 0x00}, {0x38, 0x01, 0xfe, 0x01},
        {0x02, 0x01, 0x02, 0x00}, {0x09, 0x01, 0x02, 0x00}, {0x17, 0x01, 0x02, 0x00}, {0x28, 0x01, 0x02, 0x01},
        {0x02, 0x01, 0x03, 0x00}, {0x09, 0x01, 0x03, 0x00}, {0x17, 0x01, 0x03, 0x00}, {0x28, 0x01, 0x03, 0x01}
    }, {
        {0x03, 0x01, 0x02, 0x00}, {0x06, 0x01, 0x02, 0x00}, {0x0a, 0x01, 0x02, 0x00}, {0x0f, 0x01, 0x02, 0x00},
        {0x18, 0x01, 0x02, 0x00}, {0x1f, 0x01, 0x02, 0x00}, {0x29, 0x01, 0x02, 0x00}, {0x38, 0x01, 0x02, 0x01},
        {0x03, 0x01, 0x03, 0x00}, {0x06, 0x01, 0x03, 0x00}, {0x0a, 0x01, 0x03, 0x00}, {0x0f, 0x01, 0x03, 0x00},
        {0x18, 0x01, 0x03, 0x00}, {0x1f, 0x01, 0x03, 0x00}, {0x29, 0x01, 0x03, 0x00}, {0x38, 0x01, 0x03, 0x01}
    }, {
        {0x02, 0x01, 0x04, 0x00}, {0x09, 0x01, 0x04, 0x00}, {0x17, 0x01, 0x04, 0x00}, {0x28, 0x01, 0x04, 0x01},
        {0x02, 0x01, 0x05, 0x00}, {0x09, 0x01, 0x05, 0x00}, {0x17, 0x01, 0x05, 0x00}, {0x28, 0x01, 0x05, 0x01},
        {0x02, 0x01, 0x06, 0x00}, {0x09, 0x01, 0x06, 0x00}, {0x17, 0x01, 0x06, 0x00}, {0x28, 0x01, 0x06, 0x01},
        {0x02, 0x01, 0x07, 0x00}, {0x09, 0x01, 0x07, 0x00}, {0x17, 0x01, 0x07, 0x00}, {0x28, 0x01, 0x07, 0x01}
    }, {
        {0x03, 0x01, 0x04, 0x00}, {0x06, 0x01, 0x04, 0x00}, {0x0a, 0x01, 0x04, 0x00}, {0x0f, 0x01, 0x04, 0x00},
        {0x18, 0x01, 0x04, 0x00}, {0x1f, 0x01, 0x04, 0x00}, {0x29, 0x01, 0x04, 0x00}, {0x38, 0x01, 0x04, 0x01},
        {0x03, 0x01, 0x05, 0x00}, {0x06, 0x01, 0x05, 0x00}, {0x0a, 0x01, 0x05, 0x00}, {0x0f, 0x01, 0x05, 0x00},
        {0x18, 0x01, 0x05, 0x00}, {0x1f, 0x01, 0x05, 0x00}, {0x29, 0x01, 0x05, 0x00}, {0x38, 0x01, 0x05, 0x01}
    }, {
        {0x03, 0x01, 0x06, 0x00}, {0x06, 0x01, 0x06, 0x00}, {0x0a, 0x01, 0x06, 0x00}, {0x0f, 0x01, 0x06, 0x00},
        {0x18, 0x01, 0x06, 0x00}, {0x1f, 0x01, 0x06, 0x00}, {0x29, 0x01, 0x06, 0x00}, {0x38, 0x01, 0x06, 0x01},
        {0x03, 0x01, 0x07, 0x00}, {0x06, 0x01, 0x07, 0x00}, {0x0a, 0x01, 0x07, 0x00}, {0x0f, 0x01, 0x07, 0x00},
        {0x18, 0x01, 0x07, 0x00}, {0x1f, 0x01, 0x07, 0x00}, {0x29, 0x01, 0x07, 0x00}, {0x38, 0x01, 0x07, 0x01}
    }, {
        {0x01, 0x01, 0x08, 0x00}, {0x16, 0x01, 0x08, 0x01}, {0x01, 0x01, 0x0b, 0x00}, {0x16, 0x01, 0x0b, 0x01},
        {0x01, 0x01, 0x0c, 0x00}, {0x16, 0x01, 0x0c, 0x01}, {0x01, 0x01, 0x0e, 0x00}, {0x16, 0x01, 0x0e, 0x01},
        {0x01, 0x01, 0x0f, 0x00}, {0x16, 0x01, 0x0f, 0x01}, {0x01, 0x01, 0x10, 0x00}, {0x16, 0x01, 0x10, 0x01},
        {0x01, 0x01, 0x11, 0x00}, {0x16, 0x01, 0x11, 0x01}, {0x01, 0x01, 0x12, 0x00}, {0x16, 0x01, 0x12, 0x01}
    }, {
        {0x02, 0x01, 0x08, 0x00}, {0x09, 0x01, 0x08, 0x00}, {0x17, 0x01, 0x08, 0x00}, {0x28, 0x01, 0x08, 0x01},
        {0x02, 0x01, 0x0b, 0x00}, {0x09, 0x01, 0x0b, 0x00}, {0x17, 0x01, 0x0b, 0x00}, {0x28, 0x01, 0x0b, 0x01},
        {0x02, 0x01, 0x0c, 0x00}, {0x09, 0x01, 0x0c, 0x00}, {0x17, 0x01, 0x0c, 0x00}, {0x28, 0x01, 0x0c, 0x01},
        {0x02, 0x01, 0x0e, 0x00}, {0x09, 0x01, 0x0e, 0x00}, {0x17, 0x01, 0x0e, 0x00}, {0x28, 0x01, 0x0e, 0x01}
    }, {
        {0x03, 0x01, 0x08, 0x00}, {0x06, 0x01, 0x08, 0x00}, {0x0a, 0x01, 0x08, 0x00}, {0x0f, 0x01, 0x08, 0x00},
        {0x18, 0x01, 0x08, 0x00}, {0x1f, 0x01, 0x08, 0x00}, {0x29, 0x01, 0x08, 0x00}, {0x38, 0x01, 0x08, 0x01},
        {0x03, 0x01, 0x0b, 0x00}, {0x06, 0x01, 0x0b, 0x00}, {0x0a, 0x01, 0x0b, 0x00}, {0x0f, 0x01, 0x0b, 0x00},
        {0x18, 0x01, 0x0b, 0x00}, {0x1f, 0x01, 0x0b, 0x00}, {0x29, 0x01, 0x0b, 0x00}, {0x38, 0x01, 0x0b, 0x01}
    }, {
        {0x03, 0x01, 0x0c, 0x00}, {0x06, 0x01, 0x0c, 0x00}, {0x0a, 0x01, 0x0c, 0x00}, {0x0f, 0x01, 0x0c, 0x00},
        {0x18, 0x01, 0x0c, 0x00}, {0x1f, 0x01, 0x0c, 0x00}, {0x29, 0x01, 0x0c, 0x00}, {0x38, 0x01, 0x0c, 0x01},
        {0x03, 0x01, 0x0e, 0x00}, {0x06, 0x01, 0x0e, 0x00}, {0x0a, 0x01, 0x0e, 0x00}, {0x0f, 0x01, 0x0e, 0x00},
        {0x18, 0x01, 0x0e, 0x00}, {0x1f, 0x01, 0x0e, 0x00}, {0x29, 0x01, 0x0e, 0x00}, {0x38, 0x01, 0x0e, 0x01}
    }, {
        {0x02, 0x01, 0x0f, 0x00}, {0x09, 0x01, 0x0f, 0x00}, {0x17, 0x01, 0x0f, 0x00}, {0x28, 0x01, 0x0f, 0x01},
        {0x02, 0x01, 0x10, 0x00}, {0x09, 0x01, 0x10, 0x00}, {0x17, 0x01, 0x10, 0x00}, {0x28, 0x01, 0x10, 0x01},
        {0x02, 0x01, 0x11, 0x00}, {0x09, 0x01, 0x11, 0x00}, {0x17, 0x01, 0x11, 0x00}, {0x28, 0x01, 0x11, 0x01},
        {0x02, 0x01, 0x12, 0x00}, {0x09, 0x01, 0x12, 0x00}, {0x17, 0x01, 0x12, 0x00}, {0x28, 0x01, 0x12, 0x01}
    }, {
        {0x03, 0x01, 0x0f, 0x00}, {0x06, 0x01, 0x0f, 0x00}, {0x0a, 0x01, 0x0f, 0x00}, {0x0f, 0x01, 0x0f, 0x00},
        {0x18, 0x01, 0x0f, 0x00}, {0x1f, 0x01, 0x0f, 0x00}, {0x29, 0x01, 0x0f, 0x00}, {0x38, 0x01, 0x0f, 0x01},
        {0x03, 0x01, 0x10, 0x00}, {0x06, 0x01, 0x10, 0x00}, {0x0a, 0x01, 0x10, 0x00}, {0x0f, 0x01, 0x10, 0x00},
        {0x18, 0x01, 0x10, 0x00}, {0x1f, 0x01, 0x10, 0x00}, {0x29, 0x01, 0x10, 0x00}, {0x38, 0x01, 0x10, 0x01}
    }, {
        {0x03, 0x01, 0x11, 0x00}, {0x06, 0x01, 0x11, 0x00}, {0x0a, 0x01, 0x11, 0x00}, {0x0f, 0x01, 0x11, 0x00},
        {0x18, 0x01, 0x11, 0x00}, {0x1f, 0x01, 0x11, 0x00}, {0x29, 0x01, 0x11, 0x00}, {0x38, 0x01, 0x11, 0x01},
        {0x03, 0x01, 0x12, 0x00}, {0x06, 0x01, 0x12, 0x00}, {0x0a, 0x01, 0x12, 0x00}, {0x0f, 0x01, 0x12, 0x00},
        {0x18, 0x01, 0x12, 0x00}, {0x1f, 0x01, 0x12, 0x00}, {0x29, 0x01, 0x12, 0x00}, {0x38, 0x01, 0x12, 0x01}
    }, {
        {0x00, 0x01, 0x13, 0x01}, {0x00, 0x01, 0x14, 0x01}, {0x00, 0x01, 0x15, 0x01}, {0x00, 0x01, 0x17, 0x01},
        {0x00, 0x01, 0x18, 0x01}, {0x00, 0x01, 0x19, 0x01}, {0x00, 0x01, 0x1a, 0x01}, {0x00, 0x01, 0x1b, 0x01},
        {0x00, 0x01, 0x1c, 0x01}, {0x00, 0x01, 0x1d, 0x01}, {0x00, 0x01, 0x1e, 0x01}, {0x00, 0x01, 0x1f, 0x01},
        {0x00, 0x01, 0x7f, 0x01}, {0x00, 0x01, 0xdc, 0x01}, {0x00, 0x01, 0xf9, 0x01}, {0xfd, 0x00, 0x00, 0x01}
    }, {
        {0x01, 0x01, 0x13, 0x00}, {0x16, 0x01, 0x13, 0x01}, {0x01, 0x01, 0x14, 0x00}, {0x16, 0x01, 0x14, 0x01},
        {0x01, 0x01, 0x15, 0x00}, {0x16, 0x01, 0x15, 0x01}, {0x01, 0x01, 0x17, 0x00}, {0x16, 0x01, 0x17, 0x01},
        {0x01, 0x01, 0x18, 0x00}, {0x16, 0x01, 0x18, 0x01}, {0x01, 0x01, 0x19, 0x00}, {0x16, 0x01, 0x19, 0x01},
        {0x01, 0x01, 0x1a, 0x00}, {0x16, 0x01, 0x1a, 0x01}, {0x01, 0x01, 0x1b, 0x00}, {0x16, 0x01, 0x1b, 0x01}
    }, {
        {0x02, 0x01, 0x13, 0x00}, {0x09, 0x01, 0x13, 0x00}, {0x17, 0x01, 0x13, 0x00}, {0x28, 0x01, 0x13, 0x01},
        {0x02, 0x01, 0x14, 0x00}, {0x09, 0x01, 0x14, 0x00}, {0x17, 0x01, 0x14, 0x00}, {0x28, 0x01, 0x14, 0x01},
        {0x02, 0x01, 0x15, 0x00}, {0x09, 0x01, 0x15, 0x00}, {0x17, 0x01, 0x15, 0x00}, {0x28, 0x01, 0x15, 0x01},
        {0x02, 0x01, 0x17, 0x00}, {0x09, 0x01, 0x17, 0x00}, {0x17, 0x01, 0x17, 0x00}, {0x28, 0x01, 0x17, 0x01}
    }, {
        {0x03, 0x01, 0x13, 0x00}, {0x06, 0x01, 0x13, 0x00}, {0x0a, 0x01, 0x13, 0x00}, {0x0f, 0x01, 0x13, 0x00},
        {0x18, 0x01, 0x13, 0x00}, {0x1f, 0x01, 0x13, 0x00}, {0x29, 0x01, 0x13, 0x00}, {0x38, 0x01, 0x13, 0x01},
        {0x03, 0x01, 0x14, 0x00}, {0x06, 0x01, 0x14, 0x00}, {0x0a, 0x01, 0x14, 0x00}, {0x0f, 0x01, 0x14, 0x00},
        {0x18, 0x01, 0x14, 0x00}, {0x1f, 0x01, 0x14, 0x00}, {0x29, 0x01, 0x14, 0x00}, {0x38, 0x01, 0x14, 0x01}
    }, {
        {0x03, 0x01, 0x15, 0x00}, {0x06, 0x01, 0x15, 0x00}, {0x0a, 0x01, 0x15, 0x00}, {0x0f, 0x01, 0x15, 0x00},
        {0x18, 0x01, 0x15, 0x00}, {0x1f, 0x01, 0x15, 0x00}, {0x29, 0x01, 0x15, 0x00}, {0x38, 0x01, 0x15, 0x01},
        {0x03, 0x01, 0x17, 0x00}, {0x06, 0x01, 0x17, 0x00}, {0x0a, 0x01, 0x17, 0x00}, {0x0f, 0x01, 0x17, 0x00},
        {0x18, 0x01, 0x17, 0x00}, {0x1f, 0x01, 0x17, 0x00}, {0x29, 0x01, 0x17, 0x00}, {0x38, 0x01, 0x17, 0x01}
    }, {
        {0x02, 0x01, 0x18, 0x00}, {0x09, 0x01, 0x18, 0x00}, {0x17, 0x01, 0x18, 0x00}, {0x28, 0x01, 0x18, 0x01},
        {0x02, 0x01, 0x19, 0x00}, {0x09, 0x01, 0x19, 0x00}, {0x17, 0x01, 0x19, 0x00}, {0x28, 0x01, 0x19, 0x01},
        {0x02, 0x01, 0x1a, 0x00}, {0x09, 0x01, 0x1a, 0x00}, {0x17, 0x01, 0x1a, 0x00}, {0x28, 0x01, 0x1a, 0x01},
        {0x02, 0x01, 0x1b, 0x00}, {0x09, 0x01, 0x1b, 0x00}, {0x17, 0x01, 0x1b, 0x00}, {0x28, 0x01, 0x1b, 0x01}
    }, {
        {0x03, 0x01, 0x18, 0x00}, {0x06, 0x01, 0x18, 0x00}, {0x0a, 0x01, 0x18, 0x00}, {0x0f, 0x01, 0x18, 0x00},
        {0x18, 0x01, 0x18, 0x00}, {0x1f, 0x01, 0x18, 0x00}, {0x29, 0x01, 0x18, 0x00}, {0x38, 0x01, 0x18, 0x01},
        {0x03, 0x01, 0x19, 0x00}, {0x06, 0x01, 0x19, 0x00}, {0x0a, 0x01, 0x19, 0x00}, {0x0f, 0x01, 0x19, 0x00},
        {0x18, 0x01, 0x19, 0x00}, {0x1f, 0x01, 0x19, 0x00}, {0x29, 0x01, 0x19, 0x00}, {0x38, 0x01, 0x19, 0x01}
    }, {
        {0x03, 0x01, 0x1a, 0x00}, {0x06, 0x01, 0x1a, 0x00}, {0x0a, 0x01, 0x1a, 0x00}, {0x0f, 0x01, 0x1a, 0x00},
        {0x18, 0x01, 0x1a, 0x00}, {0x1f, 0x01, 0x1a, 0x00}, {0x29, 0x01, 0x1a, 0x00}, {0x38, 0x01, 0x1a, 0x01},
        {0x03, 0x01, 0x1b, 0x00}, {0x06, 0x01, 0x1b, 0x00}, {0x0a, 0x01, 0x1b, 0x00}, {0x0f, 0x01, 0x1b, 0x00},
        {0x18, 0x01, 0x1b, 0x00}, {0x1f, 0x01, 0x1b, 0x00}, {0x29, 0x01, 0x1b, 0x00}, {0x38, 0x01, 0x1b, 0x01}
    }, {
        {0x01, 0x01, 0x1c, 0x00}, {0x16, 0x01, 0x1c, 0x01}, {0x01, 0x01, 0x1d, 0x00}, {0x16, 0x01, 0x1d, 0x01},
        {0x01, 0x01, 0x1e, 0x00}, {0x16, 0x01, 0x1e, 0x01}, {0x01, 0x01, 0x1f, 0x00}, {0x16, 0x01, 0x1f, 0x01},
        {0x01, 0x01, 0x7f, 0x00}, {0x16, 0x01, 0x7f, 0x01}, {0x01, 0x01, 0xdc, 0x00}, {0x16, 0x01, 0xdc, 0x01},
        {0x01, 0x01, 0xf9, 0x00}, {0x16, 0x01, 0xf9, 0x01}, {0xfe, 0x00, 0x00, 0x00}, {0xff, 0x00, 0x00, 0x01}
    }, {
        {0x02, 0x01, 0x1c, 0x00}, {0x09, 0x01, 0x1c, 0x00}, {0x17, 0x01, 0x1c, 0x00}, {0x28, 0x01, 0x1c, 0x01},
        {0x02, 0x01, 0x1d, 0x00}, {0x09, 0x01, 0x1d, 0x00}, {0x17, 0x01, 0x1d, 0x00}, {0x28, 0x01, 0x1d, 0x01},
        {0x02, 0x01, 0x1e, 0x00}, {0x09, 0x01, 0x1e, 0x00}, {0x17, 0x01, 0x1e, 0x00}, {0x28, 0x01, 0x1e, 0x01},
        {0x02, 0x01, 0x1f, 0x00}, {0x09, 0x01, 0x1f, 0x00}, {0x17, 0x01, 0x1f, 0x00}, {0x28, 0x01, 0x1f, 0x01}
    }, {
        {0x03, 0x01, 0x1c, 0x00}, {0x06, 0x01, 0x1c, 0x00}, {0x0a, 0x01, 0x1c, 0x00}, {0x0f, 0x01, 0x1c, 0x00},
        {0x18, 0x01, 0x1c, 0x00}, {0x1f, 0x01, 0x1c, 0x00}, {0x29, 0x01, 0x1c, 0x00}, {0x38, 0x01, 0x1c, 0x01},
        {0x03, 0x01, 0x1d, 0x00}, {0x06, 0x01, 0x1d, 0x00}, {0x0a, 0x01, 0x1d, 0x00}, {0x0f, 0x01, 0x1d, 0x00},
        {0x18, 0x01, 0x1d, 0x00}, {0x1f, 0x01, 0x1d, 0x00}, {0x29, 0x01, 0x1d, 0x00}, {0x38, 0x01, 0x1d, 0x01}
    }, {
        {0x03, 0x01, 0x1e, 0x00}, {0x06, 0x01, 0x1e, 0x00}, {0x0a, 0x01, 0x1e, 0x00}, {0x0f, 0x01, 0x1e, 0x00},
        {0x18, 0x01, 0x1e, 0x00}, {0x1f, 0x01, 0x1e, 0x00}, {0x29, 0x01, 0x1e, 0x00}, {0x38, 0x01, 0x1e, 0x01},
        {0x03, 0x01, 0x1f, 0x00}, {0x06, 0x01, 0x1f, 0x00}, {0x0a, 0x01, 0x1f, 0x00}, {0x0f, 0x01, 0x1f, 0x00},
        {0x18, 0x01, 0x1f, 0x00}, {0x1f, 0x01, 0x1f, 0x00}, {0x29, 0x01, 0x1f, 0x00}, {0x38, 0x01, 0x1f, 0x01}
    }, {
        {0x02, 0x01, 0x7f, 0x00}, {0x09, 0x01, 0x7f, 0x00}, {0x17, 0x01, 0x7f, 0x00}, {0x28, 0x01, 0x7f, 0x01},
        {0x02, 0x01, 0xdc, 0x00}, {0x09, 0x01, 0xdc, 0x00}, {0x17, 0x01, 0xdc, 0x00}, {0x28, 0x01, 0xdc, 0x01},
        {0x02, 0x01, 0xf9, 0x00}, {0x09, 0x01, 0xf9, 0x00}, {0x17, 0x01, 0xf9, 0x00}, {0x28, 0x01, 0xf9, 0x01},
        {0x00, 0x01, 0x0a, 0x01}, {0x00, 0x01, 0x0d, 0x01}, {0x00, 0x01, 0x16, 0x01}, {0xfa, 0x00, 0x00, 0x00}
    }, {
        {0x03, 0x01, 0x7f, 0x00}, {0x06, 0x01, 0x7f, 0x00}, {0x0a, 0x01, 0x7f, 0x00}, {0x0f, 0x01, 0x7f, 0x00},
        {0x18, 0x01, 0x7f, 0x00}, {0x1f, 0x01, 0x7f, 0x00}, {0x29, 0x01, 0x7f, 0x00}, {0x38, 0x01, 0x7f, 0x01},
        {0x03, 0x01, 0xdc, 0x00}, {0x06, 0x01, 0xdc, 0x00}, {0x0a, 0x01, 0xdc, 0x00}, {0x0f, 0x01, 0xdc, 0x00},
        {0x18, 0x01, 0xdc, 0x00}, {0x1f, 0x01, 0xdc, 0x00}, {0x29, 0x01, 0xdc, 0x00}, {0x38, 0x01, 0xdc, 0x01}
    }, {
        {0x03, 0x01, 0xf9, 0x00}, {0x06, 0x01, 0xf9, 0x00}, {0x0a, 0x01, 0xf9, 0x00}, {0x0f, 0x01, 0xf9, 0x00},
        {0x18, 0x01, 0xf9, 0x00}, {0x1f, 0x01, 0xf9, 0x00}, {0x29, 0x01, 0xf9, 0x00}, {0x38, 0x01, 0xf9, 0x01},
        {0x01, 0x01, 0x0a, 0x00}, {0x16, 0x01, 0x0a, 0x01}, {0x01, 0x01, 0x0d, 0x00}, {0x16, 0x01, 0x0d, 0x01},
        {0x01, 0x01, 0x16, 0x00}, {0x16, 0x01, 0x16, 0x01}, {0xfc, 0x00, 0x00, 0x00}, {0xfc, 0x00, 0x00, 0x00}
    }, {
        {0x02, 0x01, 0x0a, 0x00}, {0x09, 0x01, 0x0a, 0x00}, {0x17, 0x01, 0x0a, 0x00}, {0x28, 0x01, 0x0a, 0x01},
        {0x02, 0x01, 0x0d, 0x00}, {0x09, 0x01, 0x0d, 0x00}, {0x17, 0x01, 0x0d, 0x00}, {0x28, 0x01, 0x0d, 0x01},
        {0x02, 0x01, 0x16, 0x00}, {0x09, 0x01, 0x16, 0x00}, {0x17, 0x01, 0x16, 0x00}, {0x28, 0x01, 0x16, 0x01},
        {0xfd, 0x00, 0x00, 0x00}, {0xfd, 0x00, 0x00, 0x00}, {0xfd, 0x00, 0x00, 0x00}, {0xfd, 0x00, 0x00, 0x00}
    }, {
        {0x03, 0x01, 0x0a, 0x00}, {0x06, 0x01, 0x0a, 0x00}, {0x0a, 0x01, 0x0a, 0x00}, {0x0f, 0x01, 0x0a, 0x00},
        {0x18, 0x01, 0x0a, 0x00}, {0x1f, 0x01, 0x0a, 0x00}, {0x29, 0x01, 0x0a, 0x00}, {0x38, 0x01, 0x0a, 0x01},
        {0x03, 0x01, 0x0d, 0x00}, {0x06, 0x01, 0x0d, 0x00}, {0x0a, 0x01, 0x0d, 0x00}, {0x0f, 0x01, 0x0d, 0x00},
        {0x18, 0x01, 0x0d, 0x00}, {0x1f, 0x01, 0x0d, 0x00}, {0x29, 0x01, 0x0d, 0x00}, {0x38, 0x01, 0x0d, 0x01}
    }, {
        {0x03, 0x01, 0x16, 0x00}, {0x06, 0x01, 0x16, 0x00}, {0x0a, 0x01, 0x16, 0x00}, {0x0f, 0x01, 0x16, 0x00},
        {0x18, 0x01, 0x16, 0x00}, {0x1f, 0x01, 0x16, 0x00}, {0x29, 0x01, 0x16, 0x00}, {0x38, 0x01, 0x16, 0x01},
        {0xff, 0x00, 0x00, 0x00}, {0xff, 0x00, 0x00, 0x00}, {0xff, 0x00, 0x00, 0x00}, {0xff, 0x00, 0x00, 0x00},
        {0xff, 0x00, 0x00, 0x00}, {0xff, 0x00, 0x00, 0x00}, {0xff, 0x00, 0x00, 0x00}, {0xff, 0x00, 0x00, 0x00}
    }
};

typedef struct Encodes {
    uint  code;
    uint  len;
} Encodes;

static Encodes  encodes[256] = {
    {0x00001ff8, 13}, {0x007fffd8, 23}, {0x0fffffe2, 28}, {0x0fffffe3, 28},
    {0x0fffffe4, 28}, {0x0fffffe5, 28}, {0x0fffffe6, 28}, {0x0fffffe7, 28},
    {0x0fffffe8, 28}, {0x00ffffea, 24}, {0x3ffffffc, 30}, {0x0fffffe9, 28},
    {0x0fffffea, 28}, {0x3ffffffd, 30}, {0x0fffffeb, 28}, {0x0fffffec, 28},
    {0x0fffffed, 28}, {0x0fffffee, 28}, {0x0fffffef, 28}, {0x0ffffff0, 28},
    {0x0ffffff1, 28}, {0x0ffffff2, 28}, {0x3ffffffe, 30}, {0x0ffffff3, 28},
    {0x0ffffff4, 28}, {0x0ffffff5, 28}, {0x0ffffff6, 28}, {0x0ffffff7, 28},
    {0x0ffffff8, 28}, {0x0ffffff9, 28}, {0x0ffffffa, 28}, {0x0ffffffb, 28},
    {0x00000014,  6}, {0x000003f8, 10}, {0x000003f9, 10}, {0x00000ffa, 12},
    {0x00001ff9, 13}, {0x00000015,  6}, {0x000000f8,  8}, {0x000007fa, 11},
    {0x000003fa, 10}, {0x000003fb, 10}, {0x000000f9,  8}, {0x000007fb, 11},
    {0x000000fa,  8}, {0x00000016,  6}, {0x00000017,  6}, {0x00000018,  6},
    {0x00000000,  5}, {0x00000001,  5}, {0x00000002,  5}, {0x00000019,  6},
    {0x0000001a,  6}, {0x0000001b,  6}, {0x0000001c,  6}, {0x0000001d,  6},
    {0x0000001e,  6}, {0x0000001f,  6}, {0x0000005c,  7}, {0x000000fb,  8},
    {0x00007ffc, 15}, {0x00000020,  6}, {0x00000ffb, 12}, {0x000003fc, 10},
    {0x00001ffa, 13}, {0x00000021,  6}, {0x0000005d,  7}, {0x0000005e,  7},
    {0x0000005f,  7}, {0x00000060,  7}, {0x00000061,  7}, {0x00000062,  7},
    {0x00000063,  7}, {0x00000064,  7}, {0x00000065,  7}, {0x00000066,  7},
    {0x00000067,  7}, {0x00000068,  7}, {0x00000069,  7}, {0x0000006a,  7},
    {0x0000006b,  7}, {0x0000006c,  7}, {0x0000006d,  7}, {0x0000006e,  7},
    {0x0000006f,  7}, {0x00000070,  7}, {0x00000071,  7}, {0x00000072,  7},
    {0x000000fc,  8}, {0x00000073,  7}, {0x000000fd,  8}, {0x00001ffb, 13},
    {0x0007fff0, 19}, {0x00001ffc, 13}, {0x00003ffc, 14}, {0x00000022,  6},
    {0x00007ffd, 15}, {0x00000003,  5}, {0x00000023,  6}, {0x00000004,  5},
    {0x00000024,  6}, {0x00000005,  5}, {0x00000025,  6}, {0x00000026,  6},
    {0x00000027,  6}, {0x00000006,  5}, {0x00000074,  7}, {0x00000075,  7},
    {0x00000028,  6}, {0x00000029,  6}, {0x0000002a,  6}, {0x00000007,  5},
    {0x0000002b,  6}, {0x00000076,  7}, {0x0000002c,  6}, {0x00000008,  5},
    {0x00000009,  5}, {0x0000002d,  6}, {0x00000077,  7}, {0x00000078,  7},
    {0x00000079,  7}, {0x0000007a,  7}, {0x0000007b,  7}, {0x00007ffe, 15},
    {0x000007fc, 11}, {0x00003ffd, 14}, {0x00001ffd, 13}, {0x0ffffffc, 28},
    {0x000fffe6, 20}, {0x003fffd2, 22}, {0x000fffe7, 20}, {0x000fffe8, 20},
    {0x003fffd3, 22}, {0x003fffd4, 22}, {0x003fffd5, 22}, {0x007fffd9, 23},
    {0x003fffd6, 22}, {0x007fffda, 23}, {0x007fffdb, 23}, {0x007fffdc, 23},
    {0x007fffdd, 23}, {0x007fffde, 23}, {0x00ffffeb, 24}, {0x007fffdf, 23},
    {0x00ffffec, 24}, {0x00ffffed, 24}, {0x003fffd7, 22}, {0x007fffe0, 23},
    {0x00ffffee, 24}, {0x007fffe1, 23}, {0x007fffe2, 23}, {0x007fffe3, 23},
    {0x007fffe4, 23}, {0x001fffdc, 21}, {0x003fffd8, 22}, {0x007fffe5, 23},
    {0x003fffd9, 22}, {0x007fffe6, 23}, {0x007fffe7, 23}, {0x00ffffef, 24},
    {0x003fffda, 22}, {0x001fffdd, 21}, {0x000fffe9, 20}, {0x003fffdb, 22},
    {0x003fffdc, 22}, {0x007fffe8, 23}, {0x007fffe9, 23}, {0x001fffde, 21},
    {0x007fffea, 23}, {0x003fffdd, 22}, {0x003fffde, 22}, {0x00fffff0, 24},
    {0x001fffdf, 21}, {0x003fffdf, 22}, {0x007fffeb, 23}, {0x007fffec, 23},
    {0x001fffe0, 21}, {0x001fffe1, 21}, {0x003fffe0, 22}, {0x001fffe2, 21},
    {0x007fffed, 23}, {0x003fffe1, 22}, {0x007fffee, 23}, {0x007fffef, 23},
    {0x000fffea, 20}, {0x003fffe2, 22}, {0x003fffe3, 22}, {0x003fffe4, 22},
    {0x007ffff0, 23}, {0x003fffe5, 22}, {0x003fffe6, 22}, {0x007ffff1, 23},
    {0x03ffffe0, 26}, {0x03ffffe1, 26}, {0x000fffeb, 20}, {0x0007fff1, 19},
    {0x003fffe7, 22}, {0x007ffff2, 23}, {0x003fffe8, 22}, {0x01ffffec, 25},
    {0x03ffffe2, 26}, {0x03ffffe3, 26}, {0x03ffffe4, 26}, {0x07ffffde, 27},
    {0x07ffffdf, 27}, {0x03ffffe5, 26}, {0x00fffff1, 24}, {0x01ffffed, 25},
    {0x0007fff2, 19}, {0x001fffe3, 21}, {0x03ffffe6, 26}, {0x07ffffe0, 27},
    {0x07ffffe1, 27}, {0x03ffffe7, 26}, {0x07ffffe2, 27}, {0x00fffff2, 24},
    {0x001fffe4, 21}, {0x001fffe5, 21}, {0x03ffffe8, 26}, {0x03ffffe9, 26},
    {0x0ffffffd, 28}, {0x07ffffe3, 27}, {0x07ffffe4, 27}, {0x07ffffe5, 27},
    {0x000fffec, 20}, {0x00fffff3, 24}, {0x000fffed, 20}, {0x001fffe6, 21},
    {0x003fffe9, 22}, {0x001fffe7, 21}, {0x001fffe8, 21}, {0x007ffff3, 23},
    {0x003fffea, 22}, {0x003fffeb, 22}, {0x01ffffee, 25}, {0x01ffffef, 25},
    {0x00fffff4, 24}, {0x00fffff5, 24}, {0x03ffffea, 26}, {0x007ffff4, 23},
    {0x03ffffeb, 26}, {0x07ffffe6, 27}, {0x03ffffec, 26}, {0x03ffffed, 26},
    {0x07ffffe7, 27}, {0x07ffffe8, 27}, {0x07ffffe9, 27}, {0x07ffffea, 27},
    {0x07ffffeb, 27}, {0x0ffffffe, 28}, {0x07ffffec, 27}, {0x07ffffed, 27},
    {0x07ffffee, 27}, {0x07ffffef, 27}, {0x07fffff0, 27}, {0x03ffffee, 26}
};

static Encodes  encodesLower[256] =
{
    {0x00001ff8, 13}, {0x007fffd8, 23}, {0x0fffffe2, 28}, {0x0fffffe3, 28},
    {0x0fffffe4, 28}, {0x0fffffe5, 28}, {0x0fffffe6, 28}, {0x0fffffe7, 28},
    {0x0fffffe8, 28}, {0x00ffffea, 24}, {0x3ffffffc, 30}, {0x0fffffe9, 28},
    {0x0fffffea, 28}, {0x3ffffffd, 30}, {0x0fffffeb, 28}, {0x0fffffec, 28},
    {0x0fffffed, 28}, {0x0fffffee, 28}, {0x0fffffef, 28}, {0x0ffffff0, 28},
    {0x0ffffff1, 28}, {0x0ffffff2, 28}, {0x3ffffffe, 30}, {0x0ffffff3, 28},
    {0x0ffffff4, 28}, {0x0ffffff5, 28}, {0x0ffffff6, 28}, {0x0ffffff7, 28},
    {0x0ffffff8, 28}, {0x0ffffff9, 28}, {0x0ffffffa, 28}, {0x0ffffffb, 28},
    {0x00000014,  6}, {0x000003f8, 10}, {0x000003f9, 10}, {0x00000ffa, 12},
    {0x00001ff9, 13}, {0x00000015,  6}, {0x000000f8,  8}, {0x000007fa, 11},
    {0x000003fa, 10}, {0x000003fb, 10}, {0x000000f9,  8}, {0x000007fb, 11},
    {0x000000fa,  8}, {0x00000016,  6}, {0x00000017,  6}, {0x00000018,  6},
    {0x00000000,  5}, {0x00000001,  5}, {0x00000002,  5}, {0x00000019,  6},
    {0x0000001a,  6}, {0x0000001b,  6}, {0x0000001c,  6}, {0x0000001d,  6},
    {0x0000001e,  6}, {0x0000001f,  6}, {0x0000005c,  7}, {0x000000fb,  8},
    {0x00007ffc, 15}, {0x00000020,  6}, {0x00000ffb, 12}, {0x000003fc, 10},
    {0x00001ffa, 13}, {0x00000003,  5}, {0x00000023,  6}, {0x00000004,  5},
    {0x00000024,  6}, {0x00000005,  5}, {0x00000025,  6}, {0x00000026,  6},
    {0x00000027,  6}, {0x00000006,  5}, {0x00000074,  7}, {0x00000075,  7},
    {0x00000028,  6}, {0x00000029,  6}, {0x0000002a,  6}, {0x00000007,  5},
    {0x0000002b,  6}, {0x00000076,  7}, {0x0000002c,  6}, {0x00000008,  5},
    {0x00000009,  5}, {0x0000002d,  6}, {0x00000077,  7}, {0x00000078,  7},
    {0x00000079,  7}, {0x0000007a,  7}, {0x0000007b,  7}, {0x00001ffb, 13},
    {0x0007fff0, 19}, {0x00001ffc, 13}, {0x00003ffc, 14}, {0x00000022,  6},
    {0x00007ffd, 15}, {0x00000003,  5}, {0x00000023,  6}, {0x00000004,  5},
    {0x00000024,  6}, {0x00000005,  5}, {0x00000025,  6}, {0x00000026,  6},
    {0x00000027,  6}, {0x00000006,  5}, {0x00000074,  7}, {0x00000075,  7},
    {0x00000028,  6}, {0x00000029,  6}, {0x0000002a,  6}, {0x00000007,  5},
    {0x0000002b,  6}, {0x00000076,  7}, {0x0000002c,  6}, {0x00000008,  5},
    {0x00000009,  5}, {0x0000002d,  6}, {0x00000077,  7}, {0x00000078,  7},
    {0x00000079,  7}, {0x0000007a,  7}, {0x0000007b,  7}, {0x00007ffe, 15},
    {0x000007fc, 11}, {0x00003ffd, 14}, {0x00001ffd, 13}, {0x0ffffffc, 28},
    {0x000fffe6, 20}, {0x003fffd2, 22}, {0x000fffe7, 20}, {0x000fffe8, 20},
    {0x003fffd3, 22}, {0x003fffd4, 22}, {0x003fffd5, 22}, {0x007fffd9, 23},
    {0x003fffd6, 22}, {0x007fffda, 23}, {0x007fffdb, 23}, {0x007fffdc, 23},
    {0x007fffdd, 23}, {0x007fffde, 23}, {0x00ffffeb, 24}, {0x007fffdf, 23},
    {0x00ffffec, 24}, {0x00ffffed, 24}, {0x003fffd7, 22}, {0x007fffe0, 23},
    {0x00ffffee, 24}, {0x007fffe1, 23}, {0x007fffe2, 23}, {0x007fffe3, 23},
    {0x007fffe4, 23}, {0x001fffdc, 21}, {0x003fffd8, 22}, {0x007fffe5, 23},
    {0x003fffd9, 22}, {0x007fffe6, 23}, {0x007fffe7, 23}, {0x00ffffef, 24},
    {0x003fffda, 22}, {0x001fffdd, 21}, {0x000fffe9, 20}, {0x003fffdb, 22},
    {0x003fffdc, 22}, {0x007fffe8, 23}, {0x007fffe9, 23}, {0x001fffde, 21},
    {0x007fffea, 23}, {0x003fffdd, 22}, {0x003fffde, 22}, {0x00fffff0, 24},
    {0x001fffdf, 21}, {0x003fffdf, 22}, {0x007fffeb, 23}, {0x007fffec, 23},
    {0x001fffe0, 21}, {0x001fffe1, 21}, {0x003fffe0, 22}, {0x001fffe2, 21},
    {0x007fffed, 23}, {0x003fffe1, 22}, {0x007fffee, 23}, {0x007fffef, 23},
    {0x000fffea, 20}, {0x003fffe2, 22}, {0x003fffe3, 22}, {0x003fffe4, 22},
    {0x007ffff0, 23}, {0x003fffe5, 22}, {0x003fffe6, 22}, {0x007ffff1, 23},
    {0x03ffffe0, 26}, {0x03ffffe1, 26}, {0x000fffeb, 20}, {0x0007fff1, 19},
    {0x003fffe7, 22}, {0x007ffff2, 23}, {0x003fffe8, 22}, {0x01ffffec, 25},
    {0x03ffffe2, 26}, {0x03ffffe3, 26}, {0x03ffffe4, 26}, {0x07ffffde, 27},
    {0x07ffffdf, 27}, {0x03ffffe5, 26}, {0x00fffff1, 24}, {0x01ffffed, 25},
    {0x0007fff2, 19}, {0x001fffe3, 21}, {0x03ffffe6, 26}, {0x07ffffe0, 27},
    {0x07ffffe1, 27}, {0x03ffffe7, 26}, {0x07ffffe2, 27}, {0x00fffff2, 24},
    {0x001fffe4, 21}, {0x001fffe5, 21}, {0x03ffffe8, 26}, {0x03ffffe9, 26},
    {0x0ffffffd, 28}, {0x07ffffe3, 27}, {0x07ffffe4, 27}, {0x07ffffe5, 27},
    {0x000fffec, 20}, {0x00fffff3, 24}, {0x000fffed, 20}, {0x001fffe6, 21},
    {0x003fffe9, 22}, {0x001fffe7, 21}, {0x001fffe8, 21}, {0x007ffff3, 23},
    {0x003fffea, 22}, {0x003fffeb, 22}, {0x01ffffee, 25}, {0x01ffffef, 25},
    {0x00fffff4, 24}, {0x00fffff5, 24}, {0x03ffffea, 26}, {0x007ffff4, 23},
    {0x03ffffeb, 26}, {0x07ffffe6, 27}, {0x03ffffec, 26}, {0x03ffffed, 26},
    {0x07ffffe7, 27}, {0x07ffffe8, 27}, {0x07ffffe9, 27}, {0x07ffffea, 27},
    {0x07ffffeb, 27}, {0x0ffffffe, 28}, {0x07ffffec, 27}, {0x07ffffed, 27},
    {0x07ffffee, 27}, {0x07ffffef, 27}, {0x07fffff0, 27}, {0x03ffffee, 26}
};


/*
    Return the size of the decoded string.
 */
static int decodeHuff(uchar *state, uchar *src, int len, uchar *dst, uint last)
{
    uchar  *end, *dstp, ch, ending, *marker;

    ch = 0;
    ending = 1;
    end = src + len;
    dstp = dst;

    while (src != end) {
        ch = *src++;
        marker = dstp;
        if (decodeBits(state, &ending, ch >> 4, &dstp) < 0) {
            return MPR_ERR_BAD_STATE;
        }
        if (decodeBits(state, &ending, ch & 0xf, &dstp) < 0) {
            return MPR_ERR_BAD_STATE;
        }
        if (dstp == marker && src == end) {
            //  More than 7 bits of padding at the end
            return MPR_ERR_BAD_STATE;
        }
    }
    if (last) {
        if (!ending) {
            return MPR_ERR_BAD_STATE;
        }
        *state = 0;
        *dstp = '\0';
    }
    return (int) (dstp - dst);
}


static int decodeBits(uchar *state, uchar *ending, uint bits, uchar **dstp)
{
    Decodes     code;

    code = decodes[*state][bits];
    if (code.next == *state) {
        return MPR_ERR_BAD_STATE;
    }
    if (code.emit) {
        *(*dstp)++ = code.sym;
    }
    *ending = code.ending;
    *state = code.next;
    return 0;
}


PUBLIC cchar *httpHuffDecode(uchar *src, int len)
{
    uchar   *value;
    uchar   state;
    ssize   alen, size;

    alen = len * 2 + 1;
    value = mprAlloc(alen);
    state = 0;
    if ((size = decodeHuff(&state, src, len, value, 1)) < 0) {
        return 0;
    }
    assert(size < alen);
    return (cchar*) value;
}


static uint encodeHuff(cchar *src, ssize len, uchar *dst, uint lower)
{
    cchar       *end;
    uint        hlen;
    uint        buf, pending, code;
    Encodes     *table, *next;

    table = lower ? encodesLower : encodes;
    hlen = 0;
    buf = 0;
    pending = 0;

    end = src + len;

    while (src != end) {
        next = &table[*src++ & 0xFF];
        code = next->code;
        pending += next->len;
        if (pending < sizeof(buf) * 8) {
            buf |= code << (sizeof(buf) * 8 - pending);
            continue;
        }
        if ((hlen + sizeof(buf)) >= (uint) len) {
            return 0;
        }
        pending -= sizeof(buf) * 8;
        buf |= code >> pending;

        encodeBuf(&dst[hlen], buf);
        hlen += sizeof(buf);
        buf = pending ? code << (sizeof(buf) * 8 - pending) : 0;
    }
    if (pending == 0) {
        return hlen;
    }
    buf |= (uint) -1 >> pending;
    pending = align(pending, 8);
    if (hlen + pending / 8 >= len) {
        return 0;
    }
    buf >>= sizeof(buf) * 8 - pending;
    do {
        pending -= 8;
        dst[hlen++] = (uchar) (buf >> pending);
    } while (pending);
    return hlen;
}


PUBLIC ssize httpHuffEncode(cchar *src, ssize size, char *dst, uint lower)
{
    return encodeHuff(src, size, (uchar*) dst, lower);
}

#endif /* ME_HTTP_HTTP2 */

/*
    Nginx -- Portions. See License.md.

    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/monitor.c ************/

/*
    monitor.c -- Monitor and defensive management.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.

    A note on locking. Unlike most of appweb which effectively runs single-threaded due to the dispatcher,
    this module typically runs the httpMonitorEvent and checkMonitor routines multi-threaded.
 */

/********************************* Includes ***********************************/



#if ME_HTTP_DEFENSE
/********************************** Forwards **********************************/

static HttpAddress *growCounters(HttpNet *net, HttpAddress *address, int counterIndex);
static MprTicks lookupTicks(MprHash *args, cchar *key, MprTicks defaultValue);
static void stopMonitors(void);

/************************************ Code ************************************/

PUBLIC int httpAddCounter(cchar *name)
{
    return mprAddItem(HTTP->counters, sclone(name));
}


PUBLIC void httpAddCounters()
{
    Http    *http;

    http = HTTP;
    mprSetItem(http->counters, HTTP_COUNTER_ACTIVE_CLIENTS, sclone("ActiveClients"));
    mprSetItem(http->counters, HTTP_COUNTER_ACTIVE_CONNECTIONS, sclone("ActiveConnections"));
    mprSetItem(http->counters, HTTP_COUNTER_ACTIVE_REQUESTS, sclone("ActiveRequests"));
    mprSetItem(http->counters, HTTP_COUNTER_ACTIVE_PROCESSES, sclone("ActiveProcesses"));
    mprSetItem(http->counters, HTTP_COUNTER_BAD_REQUEST_ERRORS, sclone("BadRequestErrors"));
    mprSetItem(http->counters, HTTP_COUNTER_ERRORS, sclone("Errors"));
    mprSetItem(http->counters, HTTP_COUNTER_LIMIT_ERRORS, sclone("LimitErrors"));
    mprSetItem(http->counters, HTTP_COUNTER_MEMORY, sclone("Memory"));
    mprSetItem(http->counters, HTTP_COUNTER_NOT_FOUND_ERRORS, sclone("NotFoundErrors"));
    mprSetItem(http->counters, HTTP_COUNTER_NETWORK_IO, sclone("NetworkIO"));
    mprSetItem(http->counters, HTTP_COUNTER_REQUESTS, sclone("Requests"));
    mprSetItem(http->counters, HTTP_COUNTER_SSL_ERRORS, sclone("SSLErrors"));
}


static void invokeDefenses(HttpMonitor *monitor, MprHash *args)
{
    Http            *http;
    HttpDefense     *defense;
    HttpRemedyProc  remedyProc;
    MprKey          *kp;
    MprHash         *extra;
    int             next;

    http = monitor->http;

    for (ITERATE_ITEMS(monitor->defenses, defense, next)) {
        if ((remedyProc = mprLookupKey(http->remedies, defense->remedy)) == 0) {
            continue;
        }
        extra = mprCloneHash(defense->args);
        for (ITERATE_KEYS(extra, kp)) {
            kp->data = stemplate(kp->data, args);
        }
        mprBlendHash(args, extra);

        if (defense->suppressPeriod) {
            typedef struct SuppressDefense {
                MprTicks    suppressUntil;
            } SuppressDefense;

            SuppressDefense *sd;
            cchar *str = mprHashToString(args, "");
            if (!defense->suppress) {
                defense->suppress = mprCreateHash(0, 0);
            }
            if ((sd = mprLookupKey(defense->suppress, str)) != 0) {
                if (sd->suppressUntil > http->now) {
                    continue;
                }
                sd->suppressUntil = http->now + defense->suppressPeriod;
            } else {
                if ((sd = mprAllocStruct(SuppressDefense)) != 0) {
                    mprAddKey(defense->suppress, str, sd);
                }
                sd->suppressUntil = http->now + defense->suppressPeriod;
            }
        }
        httpLog(http->trace, "monitor.defense.invoke", "context", "defense:%s, remedy:%s", defense->name, defense->remedy);

        /*  WARNING: yields */
        remedyProc(args);

#if FUTURE
        if (http->monitorCallback) {
            (http->monitorCallback)(monitor, defense, args);
        }
#endif
    }
}


static void checkCounter(HttpMonitor *monitor, HttpCounter *counter, cchar *ip)
{
    MprHash     *args;
    cchar       *address, *fmt, *msg, *subject;
    uint64      period;

    fmt = 0;

    if (monitor->expr == '>') {
        if (counter->value > monitor->limit) {
            fmt = "Monitor%s for \"%s\". Value %lld per %lld secs exceeds limit of %lld.";
        }

    } else if (monitor->expr == '<') {
        if (counter->value < monitor->limit) {
            fmt = "Monitor%s for \"%s\". Value %lld per %lld secs outside limit of %lld.";
        }
    }
    if (fmt) {
        period = monitor->period / 1000;
        address = ip ? sfmt(" %s", ip) : "";
        msg = sfmt(fmt, address, monitor->counterName, counter->value, period, monitor->limit);
        httpLog(HTTP->trace, "monitor.check", "context", "msg:%s", msg);

        subject = sfmt("Monitor %s Alert", monitor->counterName);
        args = mprDeserialize(
            sfmt("{ COUNTER: '%s', DATE: '%s', IP: '%s', LIMIT: %lld, MESSAGE: '%s', PERIOD: %lld, SUBJECT: '%s', VALUE: %lld }",
            monitor->counterName, mprGetDate(NULL), ip, monitor->limit, msg, period, subject, counter->value));
        /*
            WARNING: remedies may yield
         */
        mprAddRoot(args);
        invokeDefenses(monitor, args);
        mprRemoveRoot(args);
    }
    counter->value = 0;
}


PUBLIC void httpPruneMonitors()
{
    Http        *http;
    HttpAddress *address;
    MprTicks    period;
    MprKey      *kp;

    http = HTTP;
    period = max(http->monitorPeriod, ME_HTTP_MONITOR_PERIOD);
    lock(http->addresses);
    for (ITERATE_KEY_DATA(http->addresses, kp, address)) {
        if (address->banUntil && address->banUntil < http->now) {
            httpLog(http->trace, "monitor.ban.stop", "context", "client:%s", kp->key);
            address->banUntil = 0;
        }
        /*
            Prune inactive address if not banned. Banned addresses are persisted to enforce the ban
         */
        if ((address->updated + ME_HTTP_MONITOR_PERIOD) < http->now && address->banUntil == 0) {
            if (address->counters[HTTP_COUNTER_ACTIVE_CONNECTIONS].value == 0) {
                mprRemoveKey(http->addresses, kp->key);
                /* Safe to keep iterating after removal of key */
            }
        }
    }
    unlock(http->addresses);
}


/*
    WARNING: this routine may yield
 */
static void checkMonitor(HttpMonitor *monitor, MprEvent *event)
{
    Http            *http;
    HttpAddress     *address;
    HttpCounter     c, *counter;
    MprKey          *kp;

    http = HTTP;
    http->now = mprGetTicks();

    if (monitor->counterIndex == HTTP_COUNTER_MEMORY) {
        memset(&c, 0, sizeof(HttpCounter));
        c.value = mprGetMem();
        checkCounter(monitor, &c, NULL);

    } else if (monitor->counterIndex == HTTP_COUNTER_ACTIVE_PROCESSES) {
        memset(&c, 0, sizeof(HttpCounter));
        c.value = mprGetListLength(MPR->cmdService->cmds);
        checkCounter(monitor, &c, NULL);

    } else if (monitor->counterIndex == HTTP_COUNTER_ACTIVE_CLIENTS) {
        memset(&c, 0, sizeof(HttpCounter));
        c.value = mprGetHashLength(http->addresses);
        checkCounter(monitor, &c, NULL);

    } else {
        /*
            Check the monitor for each active client address
         */
        lock(http->addresses);
        for (ITERATE_KEY_DATA(http->addresses, kp, address)) {
            counter = &address->counters[monitor->counterIndex];

            /*
                WARNING: this may allow new addresses to be added or stale addresses to be removed.
                Regardless, because GC is paused, iterating is safe.
             */
            unlock(http->addresses);
            checkCounter(monitor, counter, kp->key);
            lock(http->addresses);
        }
        if (mprGetHashLength(http->addresses) == 0) {
            stopMonitors();
        }
        unlock(http->addresses);
        httpPruneMonitors();
    }
}


static int manageMonitor(HttpMonitor *monitor, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(monitor->counterName);
        mprMark(monitor->defenses);
        mprMark(monitor->timer);
    }
    return 0;
}


PUBLIC int httpAddMonitor(cchar *counterName, cchar *expr, uint64 limit, MprTicks period, cchar *defenses)
{
    Http            *http;
    HttpMonitor     *monitor, *mp;
    HttpDefense     *defense;
    MprList         *defenseList;
    cchar           *def;
    char            *tok;
    int             counterIndex, next;

    http = HTTP;
    if (period < HTTP_MONITOR_MIN_PERIOD) {
        return MPR_ERR_BAD_ARGS;
    }
    if ((counterIndex = mprLookupStringItem(http->counters, counterName)) < 0) {
        mprLog("error http monitor", 0, "Cannot find counter %s", counterName);
        return MPR_ERR_CANT_FIND;
    }
    for (ITERATE_ITEMS(http->monitors, mp, next)) {
        if (mp->counterIndex == counterIndex) {
            mprLog("error http monitor", 0, "Monitor already exists for counter %s", counterName);
            return MPR_ERR_ALREADY_EXISTS;
        }
    }
    if ((monitor = mprAllocObj(HttpMonitor, manageMonitor)) == 0) {
        return MPR_ERR_MEMORY;
    }
    if ((defenseList = mprCreateList(-1, MPR_LIST_STABLE)) == 0) {
        return MPR_ERR_MEMORY;
    }
    tok = sclone(defenses);
    while ((def = stok(tok, " \t", &tok)) != 0) {
        if ((defense = mprLookupKey(http->defenses, def)) == 0) {
            mprLog("error http monitor", 0, "Cannot find Defense \"%s\"", def);
            return MPR_ERR_CANT_FIND;
        }
        mprAddItem(defenseList, defense);
    }
    monitor->counterIndex = counterIndex;
    monitor->counterName = mprGetItem(http->counters, monitor->counterIndex);
    monitor->expr = (expr && *expr == '<') ? '<' : '>';
    monitor->limit = limit;
    monitor->period = period;
    monitor->defenses = defenseList;
    monitor->http = http;
    http->monitorPeriod = min(http->monitorPeriod, period);
    mprAddItem(http->monitors, monitor);
    return 0;
}


static void manageAddress(HttpAddress *address, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(address->banMsg);
    }
}


static void startMonitors()
{
    HttpMonitor     *monitor;
    Http            *http;
    int             next;

    if (mprGetDebugMode()) {
        return;
    }
    http = HTTP;
    lock(http);
    if (!http->monitorsStarted) {
        for (ITERATE_ITEMS(http->monitors, monitor, next)) {
            if (!monitor->timer) {
                monitor->timer = mprCreateTimerEvent(NULL, "monitor", monitor->period, checkMonitor, monitor, 0);
            }
        }
        http->monitorsStarted = 1;
    }
    unlock(http);
}


static void stopMonitors()
{
    HttpMonitor     *monitor;
    Http            *http;
    int             next;

    http = HTTP;
    lock(http);
    if (http->monitorsStarted) {
        for (ITERATE_ITEMS(http->monitors, monitor, next)) {
            if (monitor->timer) {
                mprStopContinuousEvent(monitor->timer);
                monitor->timer = 0;
            }
        }
        http->monitorsStarted = 0;
    }
    unlock(http);
}


/*
    Return the monitor counters for a network address based on the net->ip.
    Allocate or grow the counters sufficient to store the max counter index.
 */
PUBLIC HttpAddress *httpMonitorAddress(HttpNet *net, int counterIndex)
{
    Http            *http;
    HttpAddress     *address;
    int             count;
    static int      seqno = 0;

    address = net->address;
    if (address) {
        return address;
    }
    http = net->http;
    if (counterIndex <= 0) {
        counterIndex = HTTP_COUNTER_MAX;
    }
    lock(http->addresses);
    address = mprLookupKey(http->addresses, net->ip);
    if (!address) {
        count = mprGetHashLength(http->addresses);
        if (count >= net->limits->clientMax) {
            unlock(http->addresses);
            return 0;
        }
    }
    if ((address = growCounters(net, address, counterIndex)) == 0) {
        /* Cannot happen */
        unlock(http->addresses);
        return 0;
    }
    address->seqno = ++seqno;
    mprAddKey(http->addresses, net->ip, address);

    net->address = address;
    if (!http->monitorsStarted) {
        startMonitors();
    }
    unlock(http->addresses);
    return address;
}


static HttpAddress *growCounters(HttpNet *net, HttpAddress *address, int counterIndex)
{
    int     ncounters;

    if (!address || address->ncounters <= counterIndex) {
        //  Round to 16
        ncounters = ((counterIndex + 0xF) & ~0xF);
        if (address) {
            address = mprRealloc(address, sizeof(HttpAddress) * ncounters * sizeof(HttpCounter));
            memset(&address[address->ncounters], 0, (ncounters - address->ncounters) * sizeof(HttpCounter));
        } else {
            address = mprAllocBlock(sizeof(HttpAddress) * ncounters * sizeof(HttpCounter), MPR_ALLOC_MANAGER | MPR_ALLOC_ZERO);
            mprSetManager(address, (MprManager) manageAddress);
        }
        address->ncounters = ncounters;
    }
    return address;
}


PUBLIC int64 httpMonitorNetEvent(HttpNet *net, int counterIndex, int64 adj)
{
    HttpAddress     *address;
    HttpCounter     *counter;

    if ((address = httpMonitorAddress(net, counterIndex)) == 0) {
        return 0;
    }
    counter = &address->counters[counterIndex];
    mprAtomicAdd64((int64*) &counter->value, adj);

    if (adj < 0 && counter->value < 0) {
        counter->value = 0;
    }
    /*
        Tolerated race with "updated" and the return value
     */
    address->updated = net->http->now;
    return counter->value;
}


PUBLIC int64 httpMonitorEvent(HttpStream *stream, int counterIndex, int64 adj)
{
    return httpMonitorNetEvent(stream->net, counterIndex, adj);
}


static int manageDefense(HttpDefense *defense, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(defense->name);
        mprMark(defense->remedy);
        mprMark(defense->args);
        mprMark(defense->suppress);
    }
    return 0;
}


static HttpDefense *createDefense(cchar *name, cchar *remedy, MprHash *args)
{
    HttpDefense     *defense;

    if ((defense = mprAllocObj(HttpDefense, manageDefense)) == 0) {
        return 0;
    }
    defense->name = sclone(name);
    defense->remedy = sclone(remedy);
    defense->args = args;
    defense->suppressPeriod = lookupTicks(args, "SUPPRESS", 0);
    return defense;
}


/*
    Remedy can also be set via REMEDY= in the remedyArgs
 */
PUBLIC int httpAddDefense(cchar *name, cchar *remedy, cchar *remedyArgs)
{
    Http        *http;
    MprHash     *args;
    MprList     *list;
    char        *arg, *key, *value;
    int         next;

    assert(name && *name);

    http = HTTP;
    args = mprCreateHash(0, MPR_HASH_STABLE);
    list = stolist(remedyArgs);
    for (ITERATE_ITEMS(list, arg, next)) {
        key = ssplit(arg, "=", &value);
        mprAddKey(args, key, strim(value, "\"'", 0));
    }
    if (!remedy) {
        remedy = mprLookupKey(args, "REMEDY");
    }
    mprAddKey(http->defenses, name, createDefense(name, remedy, args));
    return 0;
}


PUBLIC int httpAddDefenseFromJson(cchar *name, cchar *remedy, MprJson *jargs)
{
    Http        *http;
    MprHash     *args;
    MprJson     *arg;
    int         next;

    assert(name && *name);

    http = HTTP;
    args = mprCreateHash(0, MPR_HASH_STABLE);
    for (ITERATE_JSON(jargs, arg, next)) {
        mprAddKey(args, arg->name, arg->value);
        if (smatch(arg->name, "remedy")) {
            remedy = arg->value;
        }
    }
    mprAddKey(http->defenses, name, createDefense(name, remedy, args));
    return 0;
}


PUBLIC void httpDumpCounters()
{
    Http            *http;
    HttpAddress     *address;
    HttpCounter     *counter;
    MprKey          *kp;
    cchar           *name;
    int             i;

    http = HTTP;
    mprLog(0, 0, "Monitor Counters:\n");
    mprLog(0, 0, "Memory counter     %'zd\n", mprGetMem());
    mprLog(0, 0, "Active processes   %d\n", mprGetListLength(MPR->cmdService->cmds));
    mprLog(0, 0, "Active clients     %d\n", mprGetHashLength(http->addresses));

    lock(http->addresses);
    for (ITERATE_KEY_DATA(http->addresses, kp, address)) {
        mprLog(0, 0, "Client             %s\n", kp->key);
        for (i = 0; i < address->ncounters; i++) {
            counter = &address->counters[i];
            name = mprGetItem(http->counters, i);
            if (name == NULL) {
                break;
            }
            if (i == HTTP_COUNTER_ACTIVE_CLIENTS) {
                counter->value = mprGetHashLength(http->addresses);
            } else if (i == HTTP_COUNTER_ACTIVE_PROCESSES) {
                counter->value = mprGetListLength(MPR->cmdService->cmds);
            } else if (i == HTTP_COUNTER_MEMORY) {
                counter->value = mprGetMem();
            }
            mprLog(0, 0, "  Counter          %s = %'lld\n", name, counter->value);
        }
    }
    unlock(http->addresses);
}


/************************************ Remedies ********************************/

PUBLIC int httpBanClient(cchar *ip, MprTicks period, int status, cchar *msg)
{
    Http            *http;
    HttpAddress     *address;
    MprTicks        banUntil;

    http = HTTP;
    if ((address = mprLookupKey(http->addresses, ip)) == 0) {
        mprLog("error http monitor", 1, "Cannot find client %s to ban", ip);
        return MPR_ERR_CANT_FIND;
    }
    if (address->banUntil < http->now) {
        httpLog(http->trace, "monitor.ban.start", "error", "client:%s, duration:%lld", ip, period / 1000);
    }
    banUntil = http->now + period;
    address->banUntil = max(banUntil, address->banUntil);
    if (msg && *msg) {
        address->banMsg = sclone(msg);
    }
    address->banStatus = status;
    return 0;
}


static MprTicks lookupTicks(MprHash *args, cchar *key, MprTicks defaultValue)
{
    cchar   *s;
    return ((s = mprLookupKey(args, key)) ? httpGetTicks(s) : defaultValue);
}


static void banRemedy(MprHash *args)
{
    MprTicks    period;
    cchar       *ip, *banStatus, *msg;
    int         status;

    if ((ip = mprLookupKey(args, "IP")) != 0) {
        period = lookupTicks(args, "PERIOD", ME_HTTP_BAN_PERIOD);
        msg = mprLookupKey(args, "MESSAGE");
        status = ((banStatus = mprLookupKey(args, "STATUS")) != 0) ? atoi(banStatus) : 0;
        httpBanClient(ip, period, status, msg);
    }
}


static void cmdRemedy(MprHash *args)
{
    MprCmd      *cmd;
    cchar       **argv;
    char        *command, *data;
    int         rc, status, argc, background;

#if DEBUG_IDE && ME_UNIX_LIKE
    unsetenv("DYLD_LIBRARY_PATH");
    unsetenv("DYLD_FRAMEWORK_PATH");
#endif
    if ((cmd = mprCreateCmd(NULL)) == 0) {
        return;
    }
    command = sclone(mprLookupKey(args, "CMD"));
    data = 0;
    if (scontains(command, "|")) {
        data = ssplit(command, "|", &command);
        data = stemplate(data, args);
    }
    command = strim(command, " \t", MPR_TRIM_BOTH);
    if ((background = ((sends(command, "&"))) != 0)) {
        command = strim(command, "&", MPR_TRIM_END);
    }
    argc = mprMakeArgv(command, &argv, 0);
    cmd->stdoutBuf = mprCreateBuf(ME_BUFSIZE, -1);
    cmd->stderrBuf = mprCreateBuf(ME_BUFSIZE, -1);

    httpLog(HTTP->trace, "monitor.remedy.cmd", "context", "remedy:%s", command);
    if (mprStartCmd(cmd, argc, argv, NULL, MPR_CMD_DETACH | MPR_CMD_IN) < 0) {
        httpLog(HTTP->trace, "monitor.rememdy.cmd.error", "error", "msg:Cannot start command. %s", command);
        return;
    }
    if (data) {
        if (mprWriteCmdBlock(cmd, MPR_CMD_STDIN, data, -1) < 0) {
            httpLog(HTTP->trace, "monitor.remedy.cmd.error", "error", "msg:Cannot write to command. %s", command);
            return;
        }
    }
    mprFinalizeCmd(cmd);
    if (!background) {
        rc = mprWaitForCmd(cmd, ME_HTTP_REMEDY_TIMEOUT);
        status = mprGetCmdExitStatus(cmd);
        if (rc < 0 || status != 0) {
            httpLog(HTTP->trace, "monitor.remedy.cmd.error", "error", "msg:Remedy failed. %s. %s, command: %s",
                mprGetBufStart(cmd->stderrBuf), mprGetBufStart(cmd->stdoutBuf), command);
            return;
        }
        mprDestroyCmd(cmd);
    }
}


static void delayRemedy(MprHash *args)
{
    Http            *http;
    HttpAddress     *address;
    MprTicks        delayUntil;
    cchar           *ip;
    int             delay;

    http = HTTP;
    if ((ip = mprLookupKey(args, "IP")) != 0) {
        if ((address = mprLookupKey(http->addresses, ip)) != 0) {
            delayUntil = http->now + lookupTicks(args, "PERIOD", ME_HTTP_DELAY_PERIOD);
            address->delayUntil = max(delayUntil, address->delayUntil);
            delay = (int) lookupTicks(args, "DELAY", ME_HTTP_DELAY);
            address->delay = max(delay, address->delay);
            httpLog(http->trace, "monitor.delay.start", "context", "client:%s, delay:%d", ip, address->delay);
        }
    }
}


static void emailRemedy(MprHash *args)
{
    if (!mprLookupKey(args, "FROM")) {
        mprAddKey(args, "FROM", "admin");
    }
    mprAddKey(args, "CMD", "To: ${TO}\nFrom: ${FROM}\nSubject: ${SUBJECT}\n${MESSAGE}\n\n| sendmail -t");
    cmdRemedy(args);
}


static void httpRemedy(MprHash *args)
{
    HttpStream  *stream;
    cchar       *uri, *msg, *method;
    char        *err;
    int         status;

    uri = mprLookupKey(args, "URI");
    if ((method = mprLookupKey(args, "METHOD")) == 0) {
        method = "POST";
    }
    msg = smatch(method, "POST") ? mprLookupKey(args, "MESSAGE") : 0;
    if ((stream = httpRequest(method, uri, msg, HTTP_1_1, &err)) == 0) {
        httpLog(HTTP->trace, "monitor.remedy.http.error", "error", "msg:%s", err);
        return;
    }
    status = httpGetStatus(stream);
    if (status != HTTP_CODE_OK) {
        httpLog(HTTP->trace, "monitor.remedy.http.error", "error", "status:%d, uri:%s", status, uri);
    }
}

/*
    Write to the error log
 */
static void logRemedy(MprHash *args)
{
    mprLog("error http monitor", 0, "%s", (char*) mprLookupKey(args, "MESSAGE"));
}


static void restartRemedy(MprHash *args)
{
    mprLog("info http monitor", 0, "RestartRemedy: Restarting ...");
    mprRestart();
}


PUBLIC int httpAddRemedy(cchar *name, HttpRemedyProc remedy)
{
    mprAddKey(HTTP->remedies, name, remedy);
    return 0;
}


PUBLIC int httpAddRemedies()
{
    httpAddRemedy("ban", banRemedy);
    httpAddRemedy("cmd", cmdRemedy);
    httpAddRemedy("delay", delayRemedy);
    httpAddRemedy("email", emailRemedy);
    httpAddRemedy("http", httpRemedy);
    httpAddRemedy("log", logRemedy);
    httpAddRemedy("restart", restartRemedy);
    return 0;
}


#else
PUBLIC int httpAddMonitor(cchar *counterName, cchar *expr, uint64 limit, MprTicks period, cchar *defenses)
{
    return 0;
}
PUBLIC HttpAddress *httpMonitorAddress(HttpNet *net, int counterIndex)
{
    return 0;
}
PUBLIC int64 httpMonitorNetEvent(HttpNet *net, int counterIndex, int64 adj)
{
    return 0;
}
PUBLIC int64 httpMonitorEvent(HttpStream *stream, int counterIndex, int64 adj)
{
    return 0;
}
#endif /* ME_HTTP_DEFENSE */
/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/net.c ************/

/*
    net.c -- Network I/O.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/***************************** Forward Declarations ***************************/

static void manageNet(HttpNet *net, int flags);
static void netTimeout(HttpNet *net, MprEvent *mprEvent);
static void secureNet(HttpNet *net, MprSsl *ssl, cchar *peerName);

#if ME_HTTP_HTTP2
static HttpHeaderTable *createHeaderTable(int maxSize);
static void manageHeaderTable(HttpHeaderTable *table, int flags);
#endif

/*********************************** Code *************************************/

PUBLIC HttpNet *httpCreateNet(MprDispatcher *dispatcher, HttpEndpoint *endpoint, int protocol, int flags)
{
    Http        *http;
    HttpNet     *net;
    HttpHost    *host;
    HttpRoute   *route;
    int         level;

    http = HTTP;

    if ((net = mprAllocObj(HttpNet, manageNet)) == 0) {
        return 0;
    }
    net->http = http;
    net->callback = http->netCallback;
    net->endpoint = endpoint;
    net->lastActivity = http->now = mprGetTicks();
    net->ioCallback = httpIOEvent;
    net->protocol = -1;

    if (endpoint) {
        net->async = endpoint->async;
        net->endpoint = endpoint;
        net->autoDestroy = 1;
        host = mprGetFirstItem(endpoint->hosts);
        if (host && (route = host->defaultRoute) != 0) {
            net->trace = route->trace;
            net->limits = route->limits;
        } else {
            net->limits = http->serverLimits;
            net->trace = http->trace;
        }
    } else {
        net->limits = http->clientLimits;
        net->trace = http->trace;
        net->nextStreamID = 1;
    }

    level = PTOI(mprLookupKey(net->trace->events, "packet"));
    net->tracing = (net->trace->level >= level) ? 1 : 0;

    net->port = -1;
    net->async = (flags & HTTP_NET_ASYNC) ? 1 : 0;

    net->socketq = httpCreateQueue(net, NULL, http->netConnector, HTTP_QUEUE_TX, NULL);
    net->socketq->name = sclone("socket-tx");

#if ME_HTTP_HTTP2
{
    /*
        The socket queue will typically send and accept packets of HTTP2_MIN_FRAME_SIZE plus the frame size overhead.
        Set the max to fit four packets. Note that HTTP/2 flow control happens on the http filters and not on the socketq.
        Other queues created in netConnector after the protocol is known.
     */
    ssize packetSize = max(HTTP2_MIN_FRAME_SIZE + HTTP2_FRAME_OVERHEAD, net->limits->packetSize);
    httpSetQueueLimits(net->socketq, net->limits, packetSize, -1, -1, -1);
    net->rxHeaders = createHeaderTable(HTTP2_TABLE_SIZE);
    net->txHeaders = createHeaderTable(HTTP2_TABLE_SIZE);
    net->http2 = HTTP->http2;
    net->window = HTTP2_MIN_WINDOW;
}
#endif

    /*
        Create queue of queues that require servicing
     */
    net->serviceq = httpCreateQueueHead(net, NULL, "serviceq", 0);
    httpInitSchedulerQueue(net->serviceq);

    if (dispatcher) {
        net->dispatcher = dispatcher;
    } else if (endpoint) {
        net->dispatcher = endpoint->dispatcher;
    } else {
        net->dispatcher = mprGetDispatcher();
    }
    net->streams = mprCreateList(0, 0);

    net->inputq = httpCreateQueueHead(net, NULL, "http", HTTP_QUEUE_RX);
    net->outputq = httpCreateQueueHead(net, NULL, "http", HTTP_QUEUE_TX);
    httpPairQueues(net->inputq, net->outputq);
    httpAppendQueue(net->socketq, net->outputq);

    if (protocol >= 0) {
        httpSetNetProtocol(net, protocol);
    }
    lock(http);
    net->seqno = ++http->totalConnections;
    unlock(http);
    httpAddNet(net);
    return net;
}


/*
    Destroy a network. This removes the network from the list of networks.
 */
PUBLIC void httpDestroyNet(HttpNet *net)
{
    HttpStream  *stream;
    int         next;

    if (net->callback) {
        net->callback(net, HTTP_NET_DESTROY);
    }
    if (!net->destroyed && !net->borrowed) {
        if (httpIsServer(net)) {
            for (ITERATE_ITEMS(net->streams, stream, next)) {
                mprRemoveItem(net->streams, stream);
                if (HTTP_STATE_BEGIN < stream->state && stream->state < HTTP_STATE_COMPLETE && !stream->destroyed) {
                    httpSetState(stream, HTTP_STATE_COMPLETE);
                }
                httpDestroyStream(stream);
                next--;
            }
            if (net->servicing) {
                httpMonitorNetEvent(net, HTTP_COUNTER_ACTIVE_CONNECTIONS, -1);
                net->servicing = 0;
            }
        }
        httpRemoveNet(net);
        if (net->sock) {
            mprCloseSocket(net->sock, 0);
            mprLog("net info", 5, "Close connection for IP %s:%d", net->ip, net->port);
            // Don't zero just incase another thread (in error) uses net->sock
        }
        if (net->dispatcher && !(net->sharedDispatcher) && net->dispatcher->flags & MPR_DISPATCHER_AUTO) {
            assert(net->streams->length == 0);
            mprDestroyDispatcher(net->dispatcher);
            // Don't NULL net->dispatcher just incase another thread (in error) uses net->dispatcher
        }
        net->destroyed = 1;
    }
}


static void manageNet(HttpNet *net, int flags)
{
    assert(net);

    if (flags & MPR_MANAGE_MARK) {
        mprMark(net->address);
        mprMark(net->streams);
        mprMark(net->stream);
        mprMark(net->context);
        mprMark(net->data);
        mprMark(net->dispatcher);
        mprMark(net->endpoint);
        mprMark(net->errorMsg);
        mprMark(net->holdq);
        mprMark(net->http);
        mprMark(net->inputq);
        mprMark(net->ip);
        mprMark(net->limits);
        mprMark(net->newDispatcher);
        mprMark(net->oldDispatcher);
        mprMark(net->outputq);
        mprMark(net->serviceq);
        mprMark(net->sock);
        mprMark(net->socketq);
        mprMark(net->trace);
        mprMark(net->timeoutEvent);
        mprMark(net->workerEvent);

#if ME_HTTP_HTTP2
        mprMark(net->frame);
        mprMark(net->rxHeaders);
        mprMark(net->txHeaders);
#endif
#if DEPRECATED
        mprMark(net->pool);
        mprMark(net->ejs);
#endif
    }
}


PUBLIC void httpBindSocket(HttpNet *net, MprSocket *sock)
{
    assert(net);
    if (sock) {
        sock->data = net;
        net->sock = sock;
        net->port = sock->port;
        net->ip = sclone(sock->ip);
    }
}


/*
    Client connect the network to a new peer.
    Existing streams are closed when the socket is closed.
 */
PUBLIC int httpConnectNet(HttpNet *net, cchar *ip, int port, MprSsl *ssl)
{
    MprSocket   *sp;

    assert(net);

    if (net->sock) {
        mprCloseSocket(net->sock, 0);
        net->sock = 0;
    }
    if ((sp = mprCreateSocket()) == 0) {
        mprLog("net error", 0, "Cannot create socket");
        return MPR_ERR_CANT_ALLOCATE;
    }
    net->error = 0;
    if (mprConnectSocket(sp, ip, port, MPR_SOCKET_REUSE_PORT) < 0) {
        return MPR_ERR_CANT_CONNECT;
    }
    net->sock = sp;
    net->ip = sclone(ip);
    net->port = port;

    if (ssl) {
        secureNet(net, ssl, ip);
    }
    httpLog(net->trace, "tx.net.peer", "context", "peer:%s:%d", net->ip, net->port);
    if (net->callback) {
        (net->callback)(net, HTTP_NET_CONNECT);
    }
    return 0;
}


static void secureNet(HttpNet *net, MprSsl *ssl, cchar *peerName)
{
#if ME_COM_SSL
    MprSocket   *sock;

    sock = net->sock;
    if (mprUpgradeSocket(sock, ssl, peerName) < 0) {
        httpNetError(net, "Cannot perform SSL upgrade. %s", sock->errorMsg);

    } else if (sock->peerCert) {
        httpLog(net->trace, "tx.net.ssl", "context",
            "msg:Connection secured with peer certificate, " \
            "secure:true, cipher:%s, peerName:%s, subject:%s, issuer:'%s'",
            sock->cipher, sock->peerName, sock->peerCert, sock->peerCertIssuer);
    }
#endif
}


PUBLIC void httpSetNetProtocol(HttpNet *net, int protocol)
{
    Http        *http;
    HttpStage   *stage;

    http = net->http;
    protocol = net->protocol = protocol;

#if ME_HTTP_HTTP2
    stage = protocol < 2 ? http->http1Filter : http->http2Filter;
    /*
        The input queue window is defined by the network limits value. Default of HTTP2_MIN_WINDOW but revised by config.
     */
    httpSetQueueLimits(net->inputq, net->limits, HTTP2_MIN_FRAME_SIZE, -1, -1, -1);
    /*
        The packetSize and window size will always be revised in Http2:parseSettingsFrame
     */
    httpSetQueueLimits(net->outputq, net->limits, HTTP2_MIN_FRAME_SIZE, -1, -1, -1);
#else
    stage = http->http1Filter;
#endif

    httpAssignQueueCallbacks(net->inputq, stage, HTTP_QUEUE_RX);
    httpAssignQueueCallbacks(net->outputq, stage, HTTP_QUEUE_TX);
    net->inputq->name = stage->name;
    net->outputq->name = stage->name;
}


#if ME_HTTP_HTTP2
static HttpHeaderTable *createHeaderTable(int maxsize)
{
    HttpHeaderTable     *table;

    table = mprAllocObj(HttpHeaderTable, manageHeaderTable);
    table->list = mprCreateList(256, 0);
    table->size = 0;
    table->max = maxsize;
    return table;
}


static void manageHeaderTable(HttpHeaderTable *table, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(table->list);
    }
}
#endif


PUBLIC void httpAddStream(HttpNet *net, HttpStream *stream)
{
    assert(!(net->dispatcher->flags & MPR_DISPATCHER_DESTROYED));
    if (mprLookupItem(net->streams, stream) < 0) {
        mprAddItem(net->streams, stream);
    }
    stream->net = net;
    if (net->protocol < 2) {
        net->stream = stream;
    }
}


PUBLIC void httpRemoveStream(HttpNet *net, HttpStream *stream)
{
    mprRemoveItem(net->streams, stream);
    if (net->protocol < 2) {
        net->stream = NULL;
    }
}


PUBLIC void httpNetTimeout(HttpNet *net)
{
    if (!net->timeoutEvent && !net->destroyed) {
        /*
            Will run on the HttpNet dispatcher unless shutting down and it is destroyed already
         */
        assert(net->dispatcher == NULL || !(net->dispatcher->flags & MPR_DISPATCHER_DESTROYED));
        net->timeoutEvent = mprCreateLocalEvent(net->dispatcher, "netTimeout", 0, netTimeout, net, 0);
    }
}


static void netTimeout(HttpNet *net, MprEvent *mprEvent)
{
    if (net->destroyed) {
        return;
    }
    /* This will trigger an I/O event which will then destroy the network */
    mprDisconnectSocket(net->sock);
    httpEnableNetEvents(net);
}


PUBLIC bool httpGetAsync(HttpNet *net)
{
    return net->async;
}


PUBLIC void httpSetAsync(HttpNet *net, bool async)
{
    net->async = async;
}


PUBLIC void httpSetIOCallback(HttpNet *net, HttpIOCallback fn)
{
    net->ioCallback = fn;
}


PUBLIC void httpSetNetCallback(HttpNet *net, HttpNetCallback callback)
{
    if (net) {
        net->callback = callback;
    } else if (HTTP) {
        HTTP->netCallback = callback;
    }
}


PUBLIC void httpSetNetContext(HttpNet *net, void *context)
{
    net->context = context;
}


#if DEPRECATED
/*
    Used by ejs
 */
PUBLIC void httpUseWorker(HttpNet *net, MprDispatcher *dispatcher, MprEvent *event)
{
    lock(net->http);
    net->oldDispatcher = net->dispatcher;
    net->dispatcher = dispatcher;
    net->worker = 1;
    assert(!net->workerEvent);
    net->workerEvent = event;
    unlock(net->http);
}


PUBLIC void httpUsePrimary(HttpNet *net)
{
    lock(net->http);
    assert(net->worker);
    assert(net->oldDispatcher && net->dispatcher != net->oldDispatcher);
    net->dispatcher = net->oldDispatcher;
    net->oldDispatcher = 0;
    net->worker = 0;
    unlock(net->http);
}
#endif


#if DEPRECATED
PUBLIC void httpBorrowNet(HttpNet *net)
{
    assert(!net->borrowed);
    if (!net->borrowed) {
        mprAddRoot(net);
        net->borrowed = 1;
    }
}


PUBLIC void httpReturnNet(HttpNet *net)
{
    assert(net->borrowed);
    if (net->borrowed) {
        net->borrowed = 0;
        mprRemoveRoot(net);
        httpEnableNetEvents(net);
    }
}
#endif


#if DEPRECATE || 1
/*
    Steal the socket object from a network. This disconnects the socket from management by the Http service.
    It is the callers responsibility to call mprCloseSocket when required.
    Harder than it looks. We clone the socket, steal the socket handle and set the socket handle to invalid.
    This preserves the HttpNet.sock object for the network and returns a new MprSocket for the caller.
 */
PUBLIC MprSocket *httpStealSocket(HttpNet *net)
{
    MprSocket   *sock;
    HttpStream  *stream;
    int         next;

    assert(net->sock);
    assert(!net->destroyed);

    if (!net->destroyed && !net->borrowed) {
        lock(net->http);
        for (ITERATE_ITEMS(net->streams, stream, next)) {
            httpDestroyStream(stream);
            next--;
        }
        sock = mprCloneSocket(net->sock);
        (void) mprStealSocketHandle(net->sock);
        mprRemoveSocketHandler(net->sock);
        httpRemoveNet(net);

        /* This will cause httpIOEvent to regard this as a client connection and not destroy this connection */
        net->endpoint = 0;
        net->async = 0;
        unlock(net->http);
        return sock;
    }
    return 0;
}


/*
    Steal the O/S socket handle. This disconnects the socket handle from management by the network.
    It is the callers responsibility to call close() on the Socket when required.
    Note: this does not change the state of the network.
 */
PUBLIC Socket httpStealSocketHandle(HttpNet *net)
{
    return mprStealSocketHandle(net->sock);
}
#endif


PUBLIC cchar *httpGetProtocol(HttpNet *net)
{
    if (net->protocol == 0) {
        return "HTTP/1.0";
    } else if (net->protocol >= 2) {
        return "HTTP/2.0";
    } else {
        return "HTTP/1.1";
    }
}


PUBLIC void httpSetNetEof(HttpNet *net)
{
    net->eof = 1;
    if (net->callback) {
        (net->callback)(net, HTTP_NET_EOF);
    }
}


PUBLIC void httpSetNetError(HttpNet *net)
{
    net->eof = 1;
    net->error = 1;
    if (net->callback) {
        (net->callback)(net, HTTP_NET_ERROR);
    }
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/netConnector.c ************/

/*
    netConnector.c -- General network connector.

    The Network connector handles I/O from upstream handlers and filters. It uses vectored writes to
    aggregate output packets into fewer actual I/O requests to the O/S.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/**************************** Forward Declarations ****************************/

static void addPacketForNet(HttpQueue *q, HttpPacket *packet);
static void addToNetVector(HttpNet *net, char *ptr, ssize bytes);
static void adjustNetVec(HttpNet *net, ssize written);
static MprOff buildNetVec(HttpQueue *q);
static void closeStreams(HttpNet *net);
static void freeNetPackets(HttpQueue *q, ssize written);
static HttpPacket *getPacket(HttpNet *net, ssize *size);
static void netOutgoing(HttpQueue *q, HttpPacket *packet);
static void netOutgoingService(HttpQueue *q);
static HttpPacket *readPacket(HttpNet *net);
static void resumeEvents(HttpNet *net, MprEvent *event);
static int sleuthProtocol(HttpNet *net, HttpPacket *packet);

/*********************************** Code *************************************/
/*
    Initialize the net connector
 */
PUBLIC int httpOpenNetConnector()
{
    HttpStage     *stage;

    if ((stage = httpCreateConnector("netConnector", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    stage->outgoing = netOutgoing;
    stage->outgoingService = netOutgoingService;
    HTTP->netConnector = stage;
    return 0;
}


/*
    Accept a new client connection on a new socket. This is invoked from acceptNet in endpoint.c
    and will come arrive on a worker thread with a new dispatcher dedicated to this connection.
 */
PUBLIC HttpNet *httpAccept(HttpEndpoint *endpoint, MprEvent *event)
{
    Http        *http;
    HttpNet     *net;
    HttpLimits  *limits;
    MprSocket   *sock;
    int64       value;
#if ME_HTTP_DEFENSE
    HttpAddress *address;
#endif

    assert(event);
    assert(event->dispatcher);
    assert(endpoint);

    if (mprShouldDenyNewRequests()) {
        return 0;
    }
    sock = event->sock;

    if ((net = httpCreateNet(event->dispatcher, endpoint, -1, HTTP_NET_ASYNC)) == 0) {
        mprCloseSocket(sock, 0);
        return 0;
    }
    httpBindSocket(net, sock);
    limits = net->limits;
    http = net->http;

    mprLog("net info", 5, "Connection received for IP %s:%d", net->ip, net->port);

#if KEEP
    //  Useful for debugging and simulating multiple clients
    net->ip = mprGetRandomString(16);
#endif

#if ME_HTTP_DEFENSE
    if ((address = httpMonitorAddress(net, 0)) == 0) {
        mprLog("net info", 1, "Connection denied for IP %s. Too many concurrent clients, active: %d, max:%d",
            net->ip, mprGetHashLength(http->addresses), net->limits->clientMax);
        httpMonitorNetEvent(net, HTTP_COUNTER_ACTIVE_CONNECTIONS, 1);
        httpDestroyNet(net);
        return 0;
    }
#endif

    net->servicing = 1;
    if ((value = httpMonitorNetEvent(net, HTTP_COUNTER_ACTIVE_CONNECTIONS, 1)) > limits->connectionsPerClientMax) {
        mprLog("net info", 1, "Connection denied for IP %s. Too many concurrent connections for client, active: %d, max: %d",
            net->ip, (int) (value - 1), limits->connectionsPerClientMax);
        httpDestroyNet(net);
        return 0;
    }
    if (mprGetListLength(http->networks) > limits->connectionsMax) {
        mprLog("net info", 1, "Connection denied for IP %s. Too many concurrent connections for the server, active: %d, max: %d",
            net->ip, mprGetListLength(http->networks), limits->connectionsMax);
        httpDestroyNet(net);
        return 0;
    }

#if ME_HTTP_DEFENSE
    address = net->address;
    if (address && address->banUntil) {
        if (address->banUntil < net->http->now) {
            mprLog("net info", 1, "Remove ban for client IP %s", net->ip);
            address->banUntil = 0;
        } else {
            mprLog("net info", 1, "Network connection refused for client IP %s, client banned: %s", net->ip,
                address->banMsg ? address->banMsg : "");
            httpDestroyNet(net);
            return 0;
        }
    }
#endif

#if ME_COM_SSL
    if (endpoint->ssl) {
        if (mprUpgradeSocket(sock, endpoint->ssl, 0) < 0) {
            httpMonitorNetEvent(net, HTTP_COUNTER_SSL_ERRORS, 1);
            mprLog("net error", 0, "Cannot upgrade socket, %s", sock->errorMsg);
            httpDestroyNet(net);
            return 0;
        }
    }
#endif

    event->mask = MPR_READABLE;
    event->timestamp = net->http->now;
    if (net->http->netCallback) {
        net->http->netCallback(net, HTTP_NET_ACCEPT);
    }
    (net->ioCallback)(net, event);
    return net;
}


PUBLIC bool httpReadIO(HttpNet *net)
{
    HttpPacket  *packet;
    HttpStream  *stream;

    stream = net->stream;
    if ((packet = readPacket(net)) != NULL) {
        if (stream) {
            if (stream->state >= HTTP_STATE_COMPLETE) {
                if (stream->keepAliveCount <= 0) {
                    //  Client should have closed first
                    mprLog("net info", 5, "Client sent request after keep-alive expired");
                    net->error = 1;
                    return 0;
                }
                if (httpServerStream(stream)) {
                    httpResetServerStream(stream);
                }
            }
        }
        if (net->protocol < 0) {
            if (strstr(packet->content->start, "\r\n\r\n") != NULL) {
                if (sleuthProtocol(net, packet) < 0) {
                    net->error = 1;
                    return 0;
                }
            }
        }
        httpPutPacket(net->inputq, packet);
        return 1;
    }
    return 0;
}


/*
    Handle IO on the network. Initially the dispatcher will be set to the server->dispatcher and the first
    I/O event will be handled on the server thread (or main thread). A request handler may create a new
    net->dispatcher and transfer execution to a worker thread if required.
 */
PUBLIC void httpIOEvent(HttpNet *net, MprEvent *event)
{
    if (net->destroyed) {
        return;
    }
    net->active = 1;
    net->lastActivity = net->http->now;

    if (event->mask & MPR_WRITABLE && (net->socketq->count + net->ioCount) > 0) {
        httpResumeQueue(net->socketq, 1);
    }
    if (event->mask & MPR_READABLE) {
        if (!mprIsSocketEof(net->sock)) {
            httpReadIO(net);
        } else {
            httpSetNetEof(net);
        }
    }
    /*
        Process packet read above. This will propagate the packet through configured queues for the net.
     */
    httpServiceNetQueues(net, HTTP_BLOCK);

    if (net->error || net->eof || (net->sentGoaway && !net->socketq->first)) {
        closeStreams(net);
        if (net->autoDestroy) {
            httpDestroyNet(net);
        }
    } else if (net->async && !net->delay) {
        httpEnableNetEvents(net);
    }
    net->active = 0;

    if (mprNeedYield()) {
        mprYield(0);
    }
}


/*
    Read data from the peer. This will use an existing packet on the inputq or allocate a new packet if required to
    hold the data. Socket error messages are stored in net->errorMsg.
 */
static HttpPacket *readPacket(HttpNet *net)
{
    HttpPacket  *packet;
    ssize       size, lastRead;

    if ((packet = getPacket(net, &size)) != 0) {
        lastRead = mprReadSocket(net->sock, mprGetBufEnd(packet->content), size);
        if (mprIsSocketEof(net->sock)) {
            httpSetNetEof(net);
        }
#if ME_COM_SSL
        if (net->sock->secured && !net->secure && net->sock->cipher) {
            MprSocket   *sock;
            net->secure = 1;
            sock = net->sock;
            if (sock->peerCert) {
                httpLog(net->trace, "rx.net.ssl", "context",
                    "msg:Connection secured, cipher:%s, peerName:%s, subject:%s, issuer:%s, session:%s",
                    sock->cipher, sock->peerName, sock->peerCert, sock->peerCertIssuer, sock->session);
            } else {
                httpLog(net->trace, "rx.net.ssl", "context", "msg:Connection secured, cipher:%s, session:%s", sock->cipher, sock->session);
            }
            if (mprGetLogLevel() >= 5) {
                mprLog("info http ssl", 6, "SSL State: %s", mprGetSocketState(sock));
            }
        }
#endif
        if (lastRead > 0) {
            if (net->tracing) {
                httpLogRxPacket(net, packet->content->end, lastRead);
            }
            mprAdjustBufEnd(packet->content, lastRead);
            mprAddNullToBuf(packet->content);
            return packet;
        }
        if (lastRead < 0 && net->eof) {
            if (net->sock->flags & (MPR_SOCKET_ERROR | MPR_SOCKET_CERT_ERROR)) {
                httpLog(net->trace, "rx.net", "error", "msg:Connection error %s", net->sock->errorMsg);
            }
            return 0;
        }
    }
    return 0;
}


static void netOutgoing(HttpQueue *q, HttpPacket *packet)
{
    assert(q == q->net->socketq);

    if (q->net->socketq) {
        httpPutForService(q->net->socketq, packet, HTTP_SCHEDULE_QUEUE);
    }
}


static void netOutgoingService(HttpQueue *q)
{
    HttpNet     *net;
    ssize       written;
    int         errCode;

    net = q->net;
    net->writeBlocked = 0;
    written = 0;

    while (q->first || net->ioIndex) {
        if (net->ioIndex == 0 && buildNetVec(q) <= (MprOff) 0) {
            freeNetPackets(q, 0);
            break;
        }
        if (net->ioFile) {
            written = mprSendFileToSocket(net->sock, net->ioFile, net->ioPos, net->ioCount, net->iovec, net->ioIndex, NULL, 0);
        } else {
            written = mprWriteSocketVector(net->sock, net->iovec, net->ioIndex);
        }
        if (written < 0) {
            errCode = mprGetError();
            if (errCode == EAGAIN || errCode == EWOULDBLOCK) {
                /*  Socket full, wait for an I/O event */
                net->writeBlocked = 1;
                break;
            }
            if (errCode == EPROTO && net->secure) {
                httpNetError(net, "Cannot negotiate SSL with server: %s", net->sock->errorMsg);
            } else {
                httpNetError(net, "netConnector: Cannot write. errno %d", errCode);
            }
            freeNetPackets(q, MAXINT);
            httpEnableNetEvents(net);
            break;

        } else if (written > 0) {
            if (net->tracing) {
                httpLogTxPacket(net, written);
            }
            freeNetPackets(q, written);
            adjustNetVec(net, written);

        } else {
            // Socket full or SSL negotiate
            net->writeBlocked = 1;
            break;
        }
    }
    if (net->writeBlocked) {
        if ((q->first || net->ioIndex) && !(net->eventMask & MPR_WRITABLE)) {
            httpEnableNetEvents(net);
        }
    } else if (q->count <= q->low && (net->outputq->flags & HTTP_QUEUE_SUSPENDED)) {
        httpResumeQueue(net->outputq, 0);
    }
}


/*
    Build the IO vector. Return the count of bytes to be written. Return -1 for EOF.
 */
static MprOff buildNetVec(HttpQueue *q)
{
    HttpNet     *net;
    HttpPacket  *packet;

    net = q->net;
    /*
        Examine each packet and accumulate as many packets into the I/O vector as possible. Leave the packets on
        the queue for now, they are removed after the IO is complete for the entire packet. mprWriteSocketVector will
        use O/S vectored writes or aggregate packets into a single write where appropriate.
     */
     net->ioCount = 0;
     for (packet = q->first; packet; packet = packet->next) {
        if (net->ioIndex >= (ME_MAX_IOVEC - 2)) {
            break;
        }
        if (httpGetPacketLength(packet) > 0 || packet->prefix || packet->esize > 0) {
            addPacketForNet(q, packet);
        }
    }
    return net->ioCount;
}



/*
    Add a packet to the io vector. Return the number of bytes added to the vector.
 */
static void addPacketForNet(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;
    HttpStream  *stream;

    net = q->net;
    assert(q->count >= 0);
    assert(net->ioIndex < (ME_MAX_IOVEC - 2));

    if (packet->prefix && mprGetBufLength(packet->prefix) > 0) {
        addToNetVector(net, mprGetBufStart(packet->prefix), mprGetBufLength(packet->prefix));
    }
    if (packet->content && mprGetBufLength(packet->content) > 0) {
        addToNetVector(net, mprGetBufStart(packet->content), mprGetBufLength(packet->content));

    } else if (packet->esize > 0) {
        stream = packet->stream;
        assert(!stream->error);
        assert(!net->stream->error);
        net->ioFile = stream->tx->file;
        net->ioCount += packet->esize;
    }
}


/*
    Add one entry to the io vector
 */
static void addToNetVector(HttpNet *net, char *ptr, ssize bytes)
{
    assert(bytes > 0);

    net->iovec[net->ioIndex].start = ptr;
    net->iovec[net->ioIndex].len = bytes;
    net->ioCount += bytes;
    net->ioIndex++;
}


static void freeNetPackets(HttpQueue *q, ssize bytes)
{
    HttpNet     *net;
    HttpPacket  *packet;
    HttpStream  *stream;
    ssize       len;

    net = q->net;
    assert(q->count >= 0);
    assert(bytes >= 0);

    while ((packet = q->first) != 0) {
        if (packet->flags & HTTP_PACKET_END) {
            if (packet->prefix) {
                len = mprGetBufLength(packet->prefix);
                len = min(len, bytes);
                mprAdjustBufStart(packet->prefix, len);
                bytes -= len;
                // Prefixes don't count in the q->count. No need to adjust
                if (mprGetBufLength(packet->prefix) == 0) {
                    // Ensure the prefix is not resent if all the content is not sent
                    packet->prefix = 0;
                }
            }
            if ((stream = packet->stream) != 0) {
                httpFinalizeConnector(stream);
            }
        } else if (bytes > 0) {
            if (packet->prefix) {
                len = mprGetBufLength(packet->prefix);
                len = min(len, bytes);
                mprAdjustBufStart(packet->prefix, len);
                bytes -= len;
                // Prefixes don't count in the q->count. No need to adjust
                if (mprGetBufLength(packet->prefix) == 0) {
                    // Ensure the prefix is not resent if all the content is not sent
                    packet->prefix = 0;
                }
            }
            if (packet->content) {
                len = mprGetBufLength(packet->content);
                len = min(len, bytes);
                mprAdjustBufStart(packet->content, len);
                bytes -= len;
                net->bytesWritten += len;
                q->count -= len;
                assert(q->count >= 0);


            } else if (packet->esize > 0) {
                len = min(packet->esize, bytes);
                bytes -= len;
                packet->esize -= len;
                packet->epos += len;
                net->ioPos += len;
                net->bytesWritten += len;
                assert(packet->esize >= 0);
            }
        }
        if ((packet->flags & HTTP_PACKET_END) || (httpGetPacketLength(packet) == 0 && !packet->prefix && packet->esize == 0)) {
            // Done with this packet - consume it
            httpGetPacket(q);
        } else {
            // Packet still has data to be written
            break;
        }
    }
}


/*
    Clear entries from the IO vector that have actually been transmitted. Support partial writes.
 */
static void adjustNetVec(HttpNet *net, ssize written)
{
    MprIOVec    *iovec;
    ssize       len;
    int         i, j;

    /*
        Cleanup the IO vector
     */
    if (written == net->ioCount) {
        /*
            Entire vector written. Just reset.
         */
        net->ioIndex = 0;
        net->ioCount = 0;
        net->ioPos = 0;
        net->ioFile = 0;

    } else {
        /*
            Partial write of an vector entry. Need to copy down the unwritten vector entries.
         */
        net->ioCount -= written;
        assert(net->ioCount >= 0);
        iovec = net->iovec;
        for (i = 0; i < net->ioIndex; i++) {
            len = iovec[i].len;
            if (written < len) {
                iovec[i].start += written;
                iovec[i].len -= written;
                break;
            } else {
                written -= len;
            }
        }
        /*
            Compact the vector
         */
        for (j = 0; i < net->ioIndex; ) {
            iovec[j++] = iovec[i++];
        }
        net->ioIndex = j;
    }
}


static int sleuthProtocol(HttpNet *net, HttpPacket *packet)
{
    MprBuf      *buf;
    ssize       len;
    cchar       *eol, *start, *ver;
    int         protocol;

    buf = packet->content;
    start = buf->start;
    protocol = 0;

    if ((eol = strstr(start, "\r\n")) == 0) {
        return MPR_ERR_BAD_STATE;
    }
    ver = strstr(start, "HTTP/");
    if (ver == NULL || ver > eol) {
        return MPR_ERR_BAD_STATE;
    }
    ver += 5;
    if (strncmp(ver, "2.", 2) == 0) {
        protocol = 2;
    } else if (strncmp(ver, "1.1", 3) == 0) {
        protocol = 1;
    } else {
        protocol = 0;
    }
    if (protocol == 2) {
        if (!net->http2 || ME_HTTP_HTTP2 == 0) {
            return MPR_ERR_BAD_STATE;
        }
        if ((len = mprGetBufLength(buf)) < (sizeof(HTTP2_PREFACE) - 1)) {
            // Insufficient data
            return 0;
        }
        if (memcmp(buf->start, HTTP2_PREFACE, sizeof(HTTP2_PREFACE) - 1) != 0) {
            protocol = MPR_ERR_BAD_STATE;
        }
        mprAdjustBufStart(buf, strlen(HTTP2_PREFACE));
        httpLog(net->trace, "rx.net", "context", "msg:Detected HTTP/2 preface");
    }
    httpSetNetProtocol(net, protocol);
    return protocol;
}


/*
    Get the packet into which to read data. Return in *size the length of data to attempt to read.
 */
static HttpPacket *getPacket(HttpNet *net, ssize *lenp)
{
    HttpPacket  *packet;
    MprBuf      *buf;
    ssize       size;

#if ME_HTTP_HTTP2
    if (net->protocol < 2) {
        size = net->inputq ? net->inputq->packetSize : ME_PACKET_SIZE;
    } else {
        size = (net->inputq ? net->inputq->packetSize : HTTP2_MIN_FRAME_SIZE) + HTTP2_FRAME_OVERHEAD;
    }
#else
    size = net->inputq ? net->inputq->packetSize : ME_PACKET_SIZE;
#endif
    if (!net->inputq || (packet = httpGetPacket(net->inputq)) == NULL) {
        if ((packet = httpCreateDataPacket(size)) == 0) {
            return 0;
        }
    }
    buf = packet->content;
    mprResetBufIfEmpty(buf);
    if (mprGetBufSpace(buf) < size && mprGrowBuf(buf, size) < 0) {
        return 0;
    }
    *lenp = mprGetBufSpace(buf);
    assert(*lenp > 0);
    return packet;
}


static bool netBanned(HttpNet *net)
{
    HttpAddress     *address;

    if ((address = net->address) != 0 && address->delay) {
        if (address->delayUntil > net->http->now) {
            /*
                Defensive counter measure - go slow
             */
            mprCreateLocalEvent(net->dispatcher, "delayConn", net->delay, resumeEvents, net, 0);
            httpLog(net->trace, "monitor.delay.stop", "context", "msg:Suspend I/O, client:%s", net->ip);
            return 1;
        } else {
            address->delay = 0;
            httpLog(net->trace, "monitor.delay.stop", "context", "msg:Resume I/O, client:%s", net->ip);
        }
    }
    return 0;
}


/*
    Defensive countermesasure - resume output after a delay
 */
static void resumeEvents(HttpNet *net, MprEvent *event)
{
    net->delay = 0;
    mprCreateLocalEvent(net->dispatcher, "resumeConn", 0, httpEnableNetEvents, net, 0);
}


/*
    Get the event mask to enable events.
    Important that this work for networks with net->error set.
 */
PUBLIC int httpGetNetEventMask(HttpNet *net)
{
    MprSocket   *sock;
    HttpStream  *stream;
    int         eventMask;

    if ((sock = net->sock) == 0) {
        return 0;
    }
    stream = net->stream;
    eventMask = 0;

    if (!mprSocketHandshaking(sock) && !(net->error || net->eof)) {
        if (httpQueuesNeedService(net) || mprSocketHasBufferedWrite(sock) || (net->socketq->count + net->ioCount) > 0) {
            // Must wait to write until handshaking is complete
            eventMask |= MPR_WRITABLE;
        }
    }
    if (net->protocol >= 2 || !stream || stream->state < HTTP_STATE_READY || stream->state == HTTP_STATE_COMPLETE ||
            mprSocketHasBufferedRead(sock) || mprSocketHandshaking(sock) || net->error) {
        if (! (net->inputq->flags & HTTP_QUEUE_SUSPENDED)) {
            eventMask |= MPR_READABLE;
        }
    }
    return eventMask;
}


/*
    Important that this work for networks with net->error set.
 */
PUBLIC void httpEnableNetEvents(HttpNet *net)
{
    if (mprShouldAbortRequests() || net->destroyed || net->borrowed || netBanned(net)) {
        return;
    }
    httpSetupWaitHandler(net, httpGetNetEventMask(net));
}


PUBLIC void httpSetupWaitHandler(HttpNet *net, int eventMask)
{
    MprSocket   *sp;

    if ((sp = net->sock) == 0) {
        return;
    }
    if (eventMask) {
        if (sp->handler == 0) {
            mprAddSocketHandler(sp, eventMask, net->dispatcher, net->ioCallback, net, 0);
        } else {
            mprSetSocketDispatcher(sp, net->dispatcher);
            mprEnableSocketEvents(sp, eventMask);
        }
        if (sp->flags & (MPR_SOCKET_BUFFERED_READ | MPR_SOCKET_BUFFERED_WRITE)) {
            mprRecallWaitHandler(sp->handler);
        }
    } else if (sp->handler) {
        mprWaitOn(sp->handler, eventMask);
    }
    net->eventMask = eventMask;
}


static void closeStreams(HttpNet *net)
{
    HttpStream  *stream;
    HttpRx      *rx;
    int         next;

    for (ITERATE_ITEMS(net->streams, stream, next)) {
        rx = stream->rx;
        httpSetEof(stream);
        if (stream->state < HTTP_STATE_PARSED) {
            httpError(stream, 0, "Peer closed connection before receiving a response");
        } else if (rx && !rx->eof && rx->chunkState != HTTP_CHUNK_UNCHUNKED && rx->chunkState != HTTP_CHUNK_EOF) {
            httpError(stream, 0, "Peer closed connection before full response received");
        }
        httpSetState(stream, HTTP_STATE_COMPLETE);
    }
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/packet.c ************/

/*
    packet.c -- Queue support routines. Queues are the bi-directional data flow channels for the pipeline.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/********************************** Forwards **********************************/

static void managePacket(HttpPacket *packet, int flags);

/************************************ Code ************************************/
/*
    Create a new packet. If size is -1, then also create a default growable buffer --
    used for incoming body content. If size > 0, then create a non-growable buffer
    of the requested size.
 */
PUBLIC HttpPacket *httpCreatePacket(ssize size)
{
    HttpPacket  *packet;

    if ((packet = mprAllocObj(HttpPacket, managePacket)) == 0) {
        return 0;
    }
    if (size != 0) {
        if ((packet->content = mprCreateBuf(size < 0 ? ME_PACKET_SIZE: (ssize) size, -1)) == 0) {
            return 0;
        }
    }
    return packet;
}


static void managePacket(HttpPacket *packet, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(packet->prefix);
        mprMark(packet->content);
        mprMark(packet->data);
        mprMark(packet->stream);
        /* Don't mark next packet, list owner will mark */
    }
}


PUBLIC HttpPacket *httpCreateDataPacket(ssize size)
{
    HttpPacket    *packet;

    if ((packet = httpCreatePacket(size)) == 0) {
        return 0;
    }
    packet->flags = HTTP_PACKET_DATA;
    return packet;
}


PUBLIC HttpPacket *httpCreateEntityPacket(MprOff pos, MprOff size, HttpFillProc fill)
{
    HttpPacket    *packet;

    if ((packet = httpCreatePacket(0)) == 0) {
        return 0;
    }
    packet->flags = HTTP_PACKET_DATA;
    packet->epos = pos;
    packet->esize = size;
    packet->fill = fill;
    return packet;
}


PUBLIC HttpPacket *httpCreateEndPacket()
{
    HttpPacket    *packet;

    if ((packet = httpCreatePacket(0)) == 0) {
        return 0;
    }
    packet->flags = HTTP_PACKET_END;
    return packet;
}


PUBLIC HttpPacket *httpCreateHeaderPacket()
{
    HttpPacket    *packet;

    if ((packet = httpCreatePacket(ME_BUFSIZE)) == 0) {
        return 0;
    }
    packet->flags = HTTP_PACKET_HEADER;
    return packet;
}


PUBLIC HttpPacket *httpClonePacket(HttpPacket *orig)
{
    HttpPacket  *packet;

    if ((packet = httpCreatePacket(0)) == 0) {
        return 0;
    }
    if (orig->content) {
        packet->content = mprCloneBuf(orig->content);
    }
    if (orig->prefix) {
        packet->prefix = mprCloneBuf(orig->prefix);
    }
    packet->flags = orig->flags;
    packet->type = orig->type;
    packet->last = orig->last;
    packet->fin = orig->fin;
    packet->esize = orig->esize;
    packet->epos = orig->epos;
    packet->fill = orig->fill;
    return packet;
}


PUBLIC void httpAdjustPacketStart(HttpPacket *packet, MprOff size)
{
    if (packet->esize) {
        packet->epos += size;
        packet->esize -= size;
    } else if (packet->content) {
        mprAdjustBufStart(packet->content, (ssize) size);
    }
}


PUBLIC void httpAdjustPacketEnd(HttpPacket *packet, MprOff size)
{
    if (packet->esize) {
        packet->esize += size;
    } else if (packet->content) {
        mprAdjustBufEnd(packet->content, (ssize) size);
    }
}


PUBLIC HttpPacket *httpGetPacket(HttpQueue *q)
{
    HttpQueue     *prev;
    HttpPacket    *packet;

    packet = 0;
    while (q->first) {
        if ((packet = q->first) != 0) {
            q->first = packet->next;
            packet->next = 0;
            q->count -= httpGetPacketLength(packet);
            assert(q->count >= 0);
            if (packet == q->last) {
                q->last = 0;
                assert(q->first == 0);
            }
            if (q->first == 0) {
                assert(q->last == 0);
            }
        }
        break;
    }
    //  Must do this regardless of whether there is a packet or not
    if (q->count < q->low) {
        prev = httpFindPreviousQueue(q);
        if (prev && prev->flags & HTTP_QUEUE_SUSPENDED) {
            /*
                This queue was full and now is below the low water mark. Back-enable the previous queue.
                Must only resume the queue if a packet was actually dequed.
             */
            httpResumeQueue(prev, 0);
        }
    }
    return packet;
}


PUBLIC void httpRemovePacket(HttpQueue *q, HttpPacket *prev, HttpPacket *packet)
{
    prev->next = packet->next;
    if (q->last == packet) {
        q->last = 0;
    }
    if (q->first == packet) {
        q->first = 0;
    }
    q->count -= httpGetPacketLength(packet);
}


PUBLIC char *httpGetPacketStart(HttpPacket *packet)
{
    if (!packet && !packet->content) {
        return 0;
    }
    return mprGetBufStart(packet->content);
}


PUBLIC char *httpGetPacketString(HttpPacket *packet)
{
    if (!packet && !packet->content) {
        return 0;
    }
    mprAddNullToBuf(packet->content);
    return mprGetBufStart(packet->content);
}


/*
    Test if the packet is too too large to be accepted by the downstream queue.
 */
PUBLIC bool httpIsPacketTooBig(HttpQueue *q, HttpPacket *packet)
{
    ssize   size;

    size = mprGetBufLength(packet->content);
    return size > q->max || size > q->packetSize;
}


/*
    Join a packet onto the service queue. This joins packet content data.
 */
PUBLIC void httpJoinPacketForService(HttpQueue *q, HttpPacket *packet, bool serviceQ)
{
    HttpPacket  *p, *last;

    if (q->last == 0 || !(packet->flags & HTTP_PACKET_DATA)) {
        httpPutForService(q, packet, HTTP_DELAY_SERVICE);

    } else {
        /*
            Find the last data packet and join with that
         */
        for (p = q->first, last = 0; p && !(p->flags & HTTP_PACKET_END); last = p, p = p->next);
        if (last) {
            httpJoinPacket(last, packet);
            q->count += httpGetPacketLength(packet);
        } else {
            httpPutBackPacket(q, packet);
        }
    }
    if (q->service && serviceQ && !(q->flags & HTTP_QUEUE_SUSPENDED))  {
        httpScheduleQueue(q);
    }
}


/*
    Join two packets by pulling the content from the second into the first.
    WARNING: this will not update the queue count. Assumes the either both are on the queue or neither.
 */
PUBLIC int httpJoinPacket(HttpPacket *packet, HttpPacket *p)
{
    ssize   len;

    assert(packet->esize == 0);
    assert(p->esize == 0);
    assert(!(packet->flags & HTTP_PACKET_SOLO));
    assert(!(p->flags & HTTP_PACKET_SOLO));

    len = httpGetPacketLength(p);
    if (mprPutBlockToBuf(packet->content, mprGetBufStart(p->content), len) != len) {
        return MPR_ERR_MEMORY;
    }
    return 0;
}


/*
    Join queue packets. Packets will not be split so the maximum size is advisory and may be exceeded.
    NOTE: this will not change the queue count.
 */
PUBLIC void httpJoinPackets(HttpQueue *q, ssize size)
{
    HttpPacket  *packet, *p;
    ssize       count, len, npackets;

    if (size < 0) {
        size = MAXINT;
    }
    if (q->first && q->first->next) {
        /*
            Get total length of data and create one packet for all the data, up to the size max
         */
        npackets = count = 0;
        for (p = q->first; p; p = p->next) {
            if (!(p->flags & (HTTP_PACKET_HEADER | HTTP_PACKET_END))) {
                count += httpGetPacketLength(p);
                npackets++;
            }
        }
        if (npackets <= 1) {
            return;
        }
        size = min(count, size);
        if ((packet = httpCreateDataPacket(size)) == 0) {
            return;
        }
        /*
            Insert the new packet as the first data packet
         */
        if (q->first->flags & HTTP_PACKET_HEADER) {
            /* Step over a header packet */
            packet->next = q->first->next;
            q->first->next = packet;
        } else {
            packet->next = q->first;
            q->first = packet;
        }
        /*
            Copy the data and free all other packets
         */
        for (p = packet->next; p && (p->flags & HTTP_PACKET_DATA) && size > 0; p = p->next) {
            len = httpGetPacketLength(p);
            httpJoinPacket(packet, p);
            // Unlink the packet
            packet->next = p->next;
            if (q->last == p) {
                q->last = packet;
            }
            size -= len;
        }
    }
}


PUBLIC void httpPutPacket(HttpQueue *q, HttpPacket *packet)
{
    assert(packet);

    if (!packet->stream) {
        packet->stream = q->stream;
    }
    if (q && q->put) {
        q->put(q, packet);
    } else {
        httpPutForService(q, packet, HTTP_SCHEDULE_QUEUE);
    }
}


/*
    Pass to the next stage in the pipeline
 */
PUBLIC void httpPutPacketToNext(HttpQueue *q, HttpPacket *packet)
{
    assert(packet);

    if (q->nextQ && q->nextQ->put && q != q->nextQ) {
        httpPutPacket(q->nextQ, packet);
    } else {
        httpPutForService(q->nextQ, packet, HTTP_SCHEDULE_QUEUE);
    }
}


PUBLIC bool httpNextQueueFull(HttpQueue *q)
{
    HttpQueue   *nextQ;

    nextQ = q->nextQ;
    return (nextQ && nextQ->count > nextQ->max) ? 1 : 0;
}


/*
    Put the packet back at the front of the queue
    PutPacket sends to recieving function, PutBack/PutForService put to the queue
    Also, PutBack does not have service option like PutForService does
 */
PUBLIC void httpPutBackPacket(HttpQueue *q, HttpPacket *packet)
{
    assert(packet);
    assert(packet->next == 0);
    assert(q->count >= 0);

    if (!packet->stream) {
        packet->stream = q->stream;
    }
    if (packet) {
        packet->next = q->first;
        if (q->first == 0) {
            q->last = packet;
        }
        q->first = packet;
        q->count += httpGetPacketLength(packet);
    }
}


/*
    Put a packet on the service queue.
 */
PUBLIC void httpPutForService(HttpQueue *q, HttpPacket *packet, bool serviceQ)
{
    assert(packet);

    if (!packet->stream) {
        packet->stream = q->stream;
    }
    q->count += httpGetPacketLength(packet);
    packet->next = 0;

    if (q->first) {
        q->last->next = packet;
        q->last = packet;
    } else {
        q->first = packet;
        q->last = packet;
    }
    if (q->service && serviceQ && !(q->flags & HTTP_QUEUE_SUSPENDED))  {
        httpScheduleQueue(q);
    }
}


/*
    Resize and possibly split a packet to be smaller than "size". Put back the 2nd portion of the split packet
    on the queue. Ensure that the packet is not larger than "size" if it is greater than zero. If size < 0, then
    use the default packet size. Return the tail packet.
 */
PUBLIC HttpPacket *httpResizePacket(HttpQueue *q, HttpPacket *packet, ssize size)
{
    HttpPacket  *tail;
    ssize       len;

    if (size <= 0) {
        size = MAXINT;
    }
    if (packet->esize > size) {
        if ((tail = httpSplitPacket(packet, size)) == 0) {
            return 0;
        }
    } else {
        /*
            Calculate the size that will fit downstream
         */
        len = packet->content ? httpGetPacketLength(packet) : 0;
        if (size < 0) {
            size = len;
        }
        size = min(size, len);
        if (size == 0 || size == len) {
            return 0;
        }
        if ((tail = httpSplitPacket(packet, size)) == 0) {
            return 0;
        }
    }
    httpPutBackPacket(q, tail);
    return tail;
}


/*
    Split a packet at a given offset and return the tail packet containing the data after the offset.
    The prefix data remains with the original packet, the tail does not inherit the prefix.
 */
PUBLIC HttpPacket *httpSplitPacket(HttpPacket *orig, ssize offset)
{
    HttpPacket  *tail;
    ssize       count, size;

    /* Must not be in a queue */
    assert(orig->next == 0);

    if (orig->esize) {
        if (offset >= orig->esize) {
            return 0;
        }
        if ((tail = httpCreateEntityPacket(orig->epos + offset, orig->esize - offset, orig->fill)) == 0) {
            return 0;
        }
        orig->esize = offset;

    } else {
        if (offset >= httpGetPacketLength(orig)) {
            return 0;
        }
        if (offset < (httpGetPacketLength(orig) / 2)) {
            /*
                A large packet will often be resized by splitting into chunks that the downstream queues will accept.
                To optimize, we allocate a new packet content buffer and the tail packet keeps the trimmed
                original packet buffer.
             */
            if ((tail = httpCreatePacket(0)) == 0) {
                return 0;
            }
            tail->content = orig->content;
            if ((orig->content = mprCreateBuf(offset, 0)) == 0) {
                return 0;
            }
            if (mprPutBlockToBuf(orig->content, mprGetBufStart(tail->content), offset) != offset) {
                return 0;
            }
            mprAdjustBufStart(tail->content, offset);

        } else {
            count = httpGetPacketLength(orig) - offset;
            size = max(count, ME_BUFSIZE);
            size = HTTP_PACKET_ALIGN(size);
            if ((tail = httpCreatePacket(size)) == 0) {
                return 0;
            }
            httpAdjustPacketEnd(orig, -count);
            if (mprPutBlockToBuf(tail->content, mprGetBufEnd(orig->content), count) != count) {
                return 0;
            }
        }
    }
    tail->stream = orig->stream;
    tail->flags = orig->flags;
    tail->type = orig->type;

    /*
        Keep the last/fin bits in the tail of the packet and clear in the first part
     */
    tail->last = orig->last;
    tail->fin = orig->fin;
    orig->last = 0;
    orig->fin = 0;
    return tail;
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/pam.c ************/

/*
    authPam.c - Authorization using PAM (Pluggable Authorization Module)

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



#if ME_COMPILER_HAS_PAM && ME_HTTP_PAM
 #include    <security/pam_appl.h>

/********************************* Defines ************************************/

typedef struct {
    char    *name;
    char    *password;
} UserInfo;

#if MACOSX
    typedef int Gid;
#else
    typedef gid_t Gid;
#endif

/********************************* Forwards ***********************************/

static int pamChat(int msgCount, const struct pam_message **msg, struct pam_response **resp, void *data);

/*********************************** Code *************************************/
/*
    Use PAM to verify a user. The password may be NULL if using auto-login.
 */
PUBLIC bool httpPamVerifyUser(HttpStream *stream, cchar *username, cchar *password)
{
    MprBuf              *abilities;
    pam_handle_t        *pamh;
    UserInfo            info;
    struct pam_conv     conv = { pamChat, &info };
    struct group        *gp;
    int                 res, i;

    assert(username);
    assert(!stream->encoded);

    info.name = (char*) username;

    if (password) {
        info.password = (char*) password;
        pamh = NULL;
        if ((res = pam_start("login", info.name, &conv, &pamh)) != PAM_SUCCESS) {
            return 0;
        }
        if ((res = pam_authenticate(pamh, PAM_DISALLOW_NULL_AUTHTOK)) != PAM_SUCCESS) {
            pam_end(pamh, PAM_SUCCESS);
            mprDebug("http pam", 5, "httpPamVerifyUser failed to verify %s", username);
            return 0;
        }
        pam_end(pamh, PAM_SUCCESS);
    }
    mprDebug("http pam", 5, "httpPamVerifyUser verified %s", username);

    if (!stream->user) {
        stream->user = mprLookupKey(stream->rx->route->auth->userCache, username);
    }
    if (!stream->user) {
        /*
            Create a temporary user with a abilities set to the groups
         */
        Gid     groups[32];
        int     ngroups;
        ngroups = sizeof(groups) / sizeof(Gid);
        if ((i = getgrouplist(username, 99999, groups, &ngroups)) >= 0) {
            abilities = mprCreateBuf(0, 0);
            for (i = 0; i < ngroups; i++) {
                if ((gp = getgrgid(groups[i])) != 0) {
                    mprPutToBuf(abilities, "%s ", gp->gr_name);
                }
            }
#if ME_DEBUG
            mprAddNullToBuf(abilities);
            mprDebug("http pam", 5, "Create temp user \"%s\" with abilities: %s", username, mprGetBufStart(abilities));
#endif
            /*
                Create a user and map groups to roles and expand to abilities
             */
            stream->user = httpAddUser(stream->rx->route->auth, username, 0, mprGetBufStart(abilities));
        }
    }
    return 1;
}

/*
    Callback invoked by the pam_authenticate function
 */
static int pamChat(int msgCount, const struct pam_message **msg, struct pam_response **resp, void *data)
{
    UserInfo                *info;
    struct pam_response     *reply;
    int                     i;

    i = 0;
    reply = 0;
    info = (UserInfo*) data;

    if (resp == 0 || msg == 0 || info == 0) {
        return PAM_CONV_ERR;
    }
    if ((reply = calloc(msgCount, sizeof(struct pam_response))) == 0) {
        return PAM_CONV_ERR;
    }
    for (i = 0; i < msgCount; i++) {
        reply[i].resp_retcode = 0;
        reply[i].resp = 0;

        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_ON:
            reply[i].resp = strdup(info->name);
            break;

        case PAM_PROMPT_ECHO_OFF:
            /* Retrieve the user password and pass onto pam */
            reply[i].resp = strdup(info->password);
            break;

        default:
            free(reply);
            return PAM_CONV_ERR;
        }
    }
    *resp = reply;
    return PAM_SUCCESS;
}
#endif /* ME_COMPILER_HAS_PAM */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/passHandler.c ************/

/*
    passHandler.c -- Pass through handler

    This handler simply relays all content to a network connector. It is used for the ErrorHandler and
    when there is no handler defined. It is configured as the "passHandler" and "errorHandler".
    It also handles OPTIONS and TRACE methods for all.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/********************************** Forwards **********************************/

static void handleTraceMethod(HttpStream *stream);
static void outgoingPassService(HttpQueue *q);
static void readyPass(HttpQueue *q);
static void startPass(HttpQueue *q);

/*********************************** Code *************************************/

PUBLIC int httpOpenPassHandler()
{
    HttpStage     *stage;

    if ((stage = httpCreateHandler("passHandler", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    HTTP->passHandler = stage;
    stage->start = startPass;
    stage->ready = readyPass;
    stage->outgoingService = outgoingPassService;
    return 0;
}


static void startPass(HttpQueue *q)
{
    HttpStream  *stream;

    stream = q->stream;

    if (stream->rx->flags & HTTP_TRACE && !stream->error) {
        handleTraceMethod(stream);
    }
}


static void readyPass(HttpQueue *q)
{
    httpFinalize(q->stream);
    httpScheduleQueue(q);
}


static void outgoingPassService(HttpQueue *q)
{
    HttpPacket  *packet;

    for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
        if (packet->flags & HTTP_PACKET_END) {
            httpPutPacketToNext(q, packet);
        }
    }
}


PUBLIC void httpHandleOptions(HttpStream *stream)
{
    httpSetHeaderString(stream, "Allow", httpGetRouteMethods(stream->rx->route));
    httpFinalize(stream);
}


static void handleTraceMethod(HttpStream *stream)
{
    HttpTx      *tx;
    HttpQueue   *q;
    HttpPacket  *traceData;

    tx = stream->tx;
    q = stream->writeq;


    /*
        Create a dummy set of headers to use as the response body. Then reset so the connector will create
        the headers in the normal fashion. Need to be careful not to have a content length in the headers in the body.
     */
    tx->flags |= HTTP_TX_NO_LENGTH;
    httpDiscardData(stream, HTTP_QUEUE_TX);
    traceData = httpCreateDataPacket(q->packetSize);
    httpCreateHeaders1(q, traceData);
    tx->flags &= ~(HTTP_TX_NO_LENGTH | HTTP_TX_HEADERS_CREATED | HTTP_TX_HEADERS_PREPARED);
    httpRemoveHeader(stream, "Content-Type");
    httpSetContentType(stream, "message/http");
    httpPutPacketToNext(q, traceData);
    httpFinalize(stream);
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/pipeline.c ************/

/*
    pipeline.c -- HTTP pipeline processing.
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/********************************** Forward ***********************************/

static int loadQueue(HttpQueue *q, ssize chunkSize);
static int matchFilter(HttpStream *stream, HttpStage *filter, HttpRoute *route, int dir);
static void openPipeQueues(HttpStream *stream, HttpQueue *qhead);
static void pairQueues(HttpQueue *head1, HttpQueue *head2);

/*********************************** Code *************************************/
/*
    Called after routing the request (httpRouteRequest)
 */
PUBLIC void httpCreatePipeline(HttpStream *stream)
{
    HttpRx      *rx;
    HttpRoute   *route;

    rx = stream->rx;
    route = rx->route;
    if (httpClientStream(stream) && !route) {
        route = stream->http->clientRoute;
    }
    httpCreateRxPipeline(stream, route);
    httpCreateTxPipeline(stream, route);
}


PUBLIC void httpCreateRxPipeline(HttpStream *stream, HttpRoute *route)
{
    HttpNet     *net;
    HttpTx      *tx;
    HttpRx      *rx;
    HttpQueue   *q;
    HttpStage   *stage, *filter;
    int         next;

    assert(stream);
    assert(route);

    net = stream->net;
    rx = stream->rx;
    tx = stream->tx;

    rx->inputPipeline = mprCreateList(-1, MPR_LIST_STABLE);
    if (route) {
        for (next = 0; (filter = mprGetNextItem(route->inputStages, &next)) != 0; ) {
            if (filter->flags & HTTP_STAGE_INTERNAL) {
                continue;
            }
            if (matchFilter(stream, filter, route, HTTP_STAGE_RX) == HTTP_ROUTE_OK) {
                mprAddItem(rx->inputPipeline, filter);
            }
        }
    }
    /*
        Convert the queue head to the tx handler
     */
    if (tx->handler) {
        httpAssignQueueCallbacks(stream->rxHead, tx->handler, HTTP_QUEUE_RX);
        stream->rxHead->name = tx->handler->name;
    }
    /*
        Insert the pipeline before the RxHead and after the existing stages (chunkFilter)
     */
    q = stream->rxHead->prevQ;
    stream->transferq = stream->rxHead;
    for (next = 0; (stage = mprGetNextItem(rx->inputPipeline, &next)) != 0; ) {
        q = httpCreateQueue(net, stream, stage, HTTP_QUEUE_RX, q);
        q->flags |= HTTP_QUEUE_REQUEST;
        if (!stream->transferq) {
            stream->transferq = q;
        }
    }
    stream->readq = stream->rxHead;

    if (httpClientStream(stream)) {
        pairQueues(stream->rxHead, stream->txHead);
        httpOpenQueues(stream);

    } else if (!rx->streaming) {
        q->max = stream->limits->rxFormSize;
    }
    if (net->protocol < 2) {
        net->inputq->stream = stream;
        net->inputq->pair->stream = stream;
    }
}


PUBLIC void httpCreateTxPipeline(HttpStream *stream, HttpRoute *route)
{
    Http        *http;
    HttpNet     *net;
    HttpTx      *tx;
    HttpRx      *rx;
    HttpQueue   *q;
    HttpStage   *stage, *filter;
    cchar       *pattern;
    int         next, simple;

    assert(stream);
    if (!route) {
        if (httpServerStream(stream)) {
            mprLog("error http", 0, "Missing route");
            return;
        }
        route = stream->http->clientRoute;
    }
    http = stream->http;
    net = stream->net;
    rx = stream->rx;
    tx = stream->tx;

    tx->charSet = route->charSet;

    tx->outputPipeline = mprCreateList(-1, MPR_LIST_STABLE);
    if (httpServerStream(stream)) {
        if (tx->handler == 0 || tx->finalized) {
            tx->handler = http->passHandler;
        }
        httpAssignQueueCallbacks(stream->txHead, tx->handler, HTTP_QUEUE_TX);
        stream->txHead->name = tx->handler->name;
    } else {
        //  No handler callbacks needed for the client side
        tx->started = 1;
    }
    simple = 1;
    if (route->outputStages) {
        for (next = 0; (filter = mprGetNextItem(route->outputStages, &next)) != 0; ) {
            if (filter->flags & HTTP_STAGE_INTERNAL) {
                continue;
            }
            if (matchFilter(stream, filter, route, HTTP_STAGE_TX) == HTTP_ROUTE_OK) {
                mprAddItem(tx->outputPipeline, filter);
                tx->flags |= HTTP_TX_HAS_FILTERS;
            }
        }
    }
    /*
        Create the outgoing queues linked from the tx queue head
     */
    q = stream->txHead;
    for (ITERATE_ITEMS(tx->outputPipeline, stage, next)) {
        q = httpCreateQueue(stream->net, stream, stage, HTTP_QUEUE_TX, q);
        q->flags |= HTTP_QUEUE_REQUEST;
    }
    stream->writeq = stream->txHead;
    pairQueues(stream->txHead, stream->rxHead);
    pairQueues(stream->rxHead, stream->txHead);
    tx->connector = http->netConnector;
    tx->simplePipeline = (net->protocol < 2 && !net->secure && !(tx->flags & HTTP_TX_HAS_FILTERS) &&
        tx->chunkSize < 0 && !stream->error);
    httpTraceQueues(stream);

    /*
        Open the pipeline stages. This calls the open entry points on all stages.
     */
    tx->flags |= HTTP_TX_PIPELINE;
    httpOpenQueues(stream);

    /*
        Incase we got an error in opening queues, need to now revert to the pass handler
     */
    if (httpServerStream(stream)) {
        if (stream->error && tx->handler != http->passHandler) {
            tx->handler = http->passHandler;
            httpAssignQueueCallbacks(stream->writeq, tx->handler, HTTP_QUEUE_TX);
            stream->txHead->name = tx->handler->name;
        }
        if (tx->handler && tx->handler == http->passHandler) {
            httpServiceQueue(stream->writeq);
        }
    }
    if (net->endpoint) {
        pattern = rx->route->pattern;
        httpLog(stream->trace, "pipeline", "context",
            "route:%s, handler:%s, target:%s, endpoint:%s:%d, host:%s, filename:%s",
            pattern && *pattern ? pattern : "*",
            tx->handler->name, rx->route->targetRule, net->endpoint->ip, net->endpoint->port,
            stream->host->name ? stream->host->name : "default", tx->filename ? tx->filename : "");
    }
}


static void pairQueues(HttpQueue *head1, HttpQueue *head2)
{
    HttpQueue   *q, *rq;

    q = head1;
    do {
        if (q->pair == 0) {
            rq = head2;
            do {
                if (q->stage == rq->stage) {
                    httpPairQueues(q, rq);
                }
                rq = rq->nextQ;
            } while (rq != head2);
        }
        q = q->nextQ;
    } while (q != head1);
}


PUBLIC void httpOpenQueues(HttpStream *stream)
{
    openPipeQueues(stream, stream->rxHead);
    openPipeQueues(stream, stream->txHead);
}


static void openPipeQueues(HttpStream *stream, HttpQueue *qhead)
{
    HttpTx      *tx;
    HttpQueue   *q;

    tx = stream->tx;
    q = qhead;
    do {
        if (q->open && !(q->flags & (HTTP_QUEUE_OPEN_TRIED))) {
            if (q->pair == 0 || !(q->pair->flags & HTTP_QUEUE_OPEN_TRIED)) {
                loadQueue(q, tx->chunkSize);
                if (q->open) {
                    q->flags |= HTTP_QUEUE_OPEN_TRIED;
                    if (q->stage->open(q) == 0) {
                        q->flags |= HTTP_QUEUE_OPENED;
                    } else if (!stream->error) {
                        httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot open stage %s", q->stage->name);
                    }
                }
            }
        }
        q = q->nextQ;
    } while (q != qhead);
}


static int loadQueue(HttpQueue *q, ssize chunkSize)
{
    Http        *http;
    HttpStream  *stream;
    HttpStage   *stage;
    MprModule   *module;

    stage = q->stage;
    stream = q->stream;
    http = q->stream->http;

    if (chunkSize > 0) {
        q->packetSize = min(q->packetSize, chunkSize);
    }
    if (stage->flags & HTTP_STAGE_UNLOADED && stage->module) {
        module = stage->module;
        module = mprCreateModule(module->name, module->path, module->entry, http);
        if (mprLoadModule(module) < 0) {
            httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot load module %s", module->name);
            return MPR_ERR_CANT_READ;
        }
        stage->module = module;
    }
    if (stage->module) {
        stage->module->lastActivity = http->now;
    }
    return 0;
}


/*
    Set the fileHandler as the selected handler for the request and invoke the open and start fileHandler callbacks.
    Called by ESP to render a document.
 */
PUBLIC void httpSetFileHandler(HttpStream *stream, cchar *path)
{
    HttpStage   *fp;

    HttpTx      *tx;

    tx = stream->tx;
    if (path && path != tx->filename) {
        httpSetFilename(stream, path, 0);
    }
    tx->entityLength = tx->fileInfo.size;
    fp = tx->handler = HTTP->fileHandler;
    stream->writeq->service = fp->outgoingService;
    stream->readq->put = fp->incoming;
    fp->open(stream->writeq);
    fp->start(stream->writeq);
    httpLog(stream->trace, "pipeline", "context", "Relay to file handler");
}


PUBLIC void httpClosePipeline(HttpStream *stream)
{
    HttpQueue   *q;

    q = stream->txHead;
    do {
        if (q->close && q->flags & HTTP_QUEUE_OPENED) {
            q->flags &= ~HTTP_QUEUE_OPENED;
            q->stage->close(q);
        }
         q = q->nextQ;
    } while (q != stream->txHead);

    q = stream->rxHead;
    do {
        if (q->close && q->flags & HTTP_QUEUE_OPENED) {
            q->flags &= ~HTTP_QUEUE_OPENED;
            q->stage->close(q);
        }
        q = q->nextQ;
    } while (q != stream->rxHead);
}


/*
    Start all queues, but do not start the handler
 */
PUBLIC void httpStartPipeline(HttpStream *stream)
{
    HttpQueue   *q, *prevQ, *nextQ;
    HttpRx      *rx;

    rx = stream->rx;
    assert(stream->net->endpoint);

    q = stream->txHead;
    do {
        prevQ = q->prevQ;
        if (q != stream->writeq && q->start && !(q->flags & HTTP_QUEUE_STARTED)) {
            q->flags |= HTTP_QUEUE_STARTED;
            q->stage->start(q);
        }
        q = prevQ;
    } while (q != stream->txHead);

    if (rx->needInputPipeline) {
        q = stream->rxHead;
        do {
            nextQ = q->nextQ;
            if (q != stream->readq && q->start && !(q->flags & HTTP_QUEUE_STARTED)) {
                /* Don't start if tx side already started */
                if (q->pair == 0 || !(q->pair->flags & HTTP_QUEUE_STARTED)) {
                    q->flags |= HTTP_QUEUE_STARTED;
                    q->stage->start(q);
                }
            }
            q = nextQ;
        } while (q != stream->rxHead);
    }
}


/*
    Called when all input data has been received
 */
PUBLIC void httpReadyHandler(HttpStream *stream)
{
    HttpQueue   *q;

    q = stream->writeq;
    if (q->stage && q->stage->ready && !(q->flags & HTTP_QUEUE_READY)) {
        q->flags |= HTTP_QUEUE_READY;
        q->stage->ready(q);
    }
}


PUBLIC void httpStartHandler(HttpStream *stream)
{
    HttpQueue   *q;
    HttpTx      *tx;

    tx = stream->tx;
    if (!tx->started) {
        tx->started = 1;
        q = stream->writeq;
        if (q->stage->start && !(q->flags & HTTP_QUEUE_STARTED)) {
            q->flags |= HTTP_QUEUE_STARTED;
            q->stage->start(q);
        }
        if (tx->pendingFinalize) {
            tx->finalizedOutput = 0;
            httpFinalizeOutput(stream);
        }
    }
}


PUBLIC bool httpQueuesNeedService(HttpNet *net)
{
    HttpQueue   *q;

    q = net->serviceq;
    return (q->scheduleNext != q);
}


PUBLIC void httpDiscardData(HttpStream *stream, int dir)
{
    HttpTx      *tx;
    HttpQueue   *q, *qhead;

    tx = stream->tx;
    if (tx == 0) {
        return;
    }
    qhead = (dir == HTTP_QUEUE_TX) ? stream->txHead : stream->rxHead;
    httpDiscardQueueData(qhead, 1);
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        httpDiscardQueueData(q, 1);
    }
}


static int matchFilter(HttpStream *stream, HttpStage *filter, HttpRoute *route, int dir)
{
    HttpTx      *tx;

    tx = stream->tx;
    if (filter->match) {
        return filter->match(stream, route, dir);
    }
    if (filter->extensions && tx->ext) {
        if (mprLookupKey(filter->extensions, tx->ext)) {
            return HTTP_ROUTE_OK;
        }
        if (mprLookupKey(filter->extensions, "")) {
            return HTTP_ROUTE_OK;
        }
        return HTTP_ROUTE_OMIT_FILTER;
    }
    return HTTP_ROUTE_OK;
}


PUBLIC void httpRemoveChunkFilter(HttpQueue *head)
{
    HttpQueue   *q;
    HttpStream  *stream;

    stream = head->stream;

    for (q = head->nextQ; q != head; q = q->nextQ) {
        if (q->stage == stream->http->chunkFilter) {
            httpRemoveQueue(q);
            httpTraceQueues(stream);
            return;
        }
    }
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/process.c ************/

/*
    process.c - Process HTTP request/response.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/********************************** Forwards **********************************/

static void addMatchEtag(HttpStream *stream, char *etag);
static bool parseRange(HttpStream *stream, char *value);
static void parseUri(HttpStream *stream);
static int prepErrorDoc(HttpStream *stream);
static int processContent(HttpStream *stream);
static void processComplete(HttpStream *stream, MprEvent *event);
static int processFinalized(HttpStream *stream);
static int processFirst(HttpStream *stream);
static void processHeaders(HttpStream *stream);
static int processParsed(HttpStream *stream);
static int processReady(HttpStream *stream);
static void routeRequest(HttpStream *stream);
static int sendContinue(HttpStream *stream);

/*********************************** Code *************************************/
/*
    HTTP Protocol state machine for server requests and client responses.
    Invoked from httpSetState to react to state changes.
    Process an incoming request/response and drive the state machine.
 */
static int httpStateMachine(HttpStream *stream)
{
    int     state;

    state = stream->state;

    switch (state) {
    case HTTP_STATE_BEGIN:
    case HTTP_STATE_CONNECTED:
        break;

    case HTTP_STATE_FIRST:
        state = processFirst(stream);
        break;

    case HTTP_STATE_PARSED:
        processHeaders(stream);
        state = processParsed(stream);
        break;

    case HTTP_STATE_CONTENT:
        state = processContent(stream);
        break;

    case HTTP_STATE_READY:
        state = processReady(stream);
        break;

    case HTTP_STATE_RUNNING:
        break;

    case HTTP_STATE_FINALIZED:
        state = processFinalized(stream);
        break;

    case HTTP_STATE_COMPLETE:
        break;
    }
    return state;
}


static int updateState(HttpStream *stream, int newState)
{
    int     state;

    state = stream->state;

    if (newState < state && stream->errorDoc) {
        state = newState;
    } else if (newState > state) {
        state = newState;
    } else {
        state = state + 1;
    }
    if (newState > stream->targetState) {
        stream->targetState = newState;
    }
    return state;
}


PUBLIC void httpSetState(HttpStream *stream, int targetState)
{
    HttpNet     *net;
    int         newState, state;

    if (stream->settingState) {
        //  Recursive invocation. Update the new target.
        if (targetState > stream->targetState) {
            stream->targetState = targetState;
        }
        return;
    }
    stream->settingState = 1;
    stream->targetState = targetState;
    net = stream->net;

    for (state = stream->state + 1; state <= stream->targetState; ) {
        stream->state = state;
        HTTP_NOTIFY(stream, HTTP_EVENT_STATE, state);

        newState = httpStateMachine(stream);
        state = updateState(stream, newState);
    }
    stream->settingState = 0;
    if (stream->state == HTTP_STATE_COMPLETE) {
        processComplete(stream, NULL);

    } else if (stream->tx->started) {
        httpPumpOutput(stream->inputq);
    }
    if (net->protocol < 2 && net->eventMask == 0 && !net->active) {
        //  net->active is true when invoked from httpIOEvent in which case we don't need to adjust net events
        httpEnableNetEvents(net);
    }
}


/*
    Process the first line of a request or response and enforce limits
 */
static int processFirst(HttpStream *stream)
{
    HttpNet     *net;
    HttpRx      *rx;
    int64       value;

    net = stream->net;
    rx = stream->rx;

    if (httpIsServer(net)) {
        stream->startMark = mprGetHiResTicks();
        stream->started = stream->http->now;
        stream->http->totalRequests++;
        if ((value = httpMonitorEvent(stream, HTTP_COUNTER_ACTIVE_REQUESTS, 1)) > net->limits->requestsPerClientMax) {
            httpError(stream, HTTP_ABORT | HTTP_CODE_SERVICE_UNAVAILABLE,
                "Request denied for IP %s. Too many concurrent requests for client, active: %d max: %d", stream->ip, (int) value, net->limits->requestsPerClientMax);
            return 0;
        }
        httpMonitorEvent(stream, HTTP_COUNTER_REQUESTS, 1);
        stream->counted = 1;
    }
    return stream->state;
}


static void processHeaders(HttpStream *stream)
{
    HttpNet     *net;
    HttpRx      *rx;
    HttpTx      *tx;
    MprKey      *kp;
    cchar       *hostname, *msg;
    char        *cp, *key, *value, *tok;
    int         keepAliveHeader;

    net = stream->net;
    rx = stream->rx;
    tx = stream->tx;
    keepAliveHeader = 0;

    if (stream->error) {
        return;
    }
    if (httpTracing(net)) {
        if (httpIsServer(net)) {
            httpLog(stream->trace, "rx.http.request", "request", "@%s %s %s", rx->originalMethod, rx->uri, rx->protocol);
            httpLog(stream->trace, "rx.http.headers", "headers", "@%s", httpTraceHeaders(stream->rx->headers));
        } else {
            msg = rx->statusMessage ? rx->statusMessage : "";
            httpLog(stream->trace, "rx.http.status", "result", "@%s %d %s", rx->protocol, rx->status, msg);
            httpLog(stream->trace, "rx.http.headers", "headers", "@%s", httpTraceHeaders(stream->rx->headers));
        }
    }
    for (ITERATE_KEYS(rx->headers, kp)) {
        key = kp->key;
        value = (char*) kp->data;
        switch (tolower((uchar) key[0])) {
        case 'a':
            if (strcasecmp(key, "authorization") == 0) {
                value = sclone(value);
                stream->authType = slower(stok(value, " \t", &tok));
                rx->authDetails = sclone(tok);

            } else if (strcasecmp(key, "accept-charset") == 0) {
                rx->acceptCharset = sclone(value);

            } else if (strcasecmp(key, "accept") == 0) {
                rx->accept = sclone(value);

            } else if (strcasecmp(key, "accept-encoding") == 0) {
                rx->acceptEncoding = sclone(value);

            } else if (strcasecmp(key, "accept-language") == 0) {
                rx->acceptLanguage = sclone(value);
            }
            break;

        case 'c':
            if (strcasecmp(key, "connection") == 0 && net->protocol < 2) {
                rx->connection = sclone(value);
                if (scaselesscmp(value, "KEEP-ALIVE") == 0) {
                    keepAliveHeader = 1;

                } else if (scaselesscmp(value, "CLOSE") == 0) {
                    stream->keepAliveCount = 0;
                }

            } else if (strcasecmp(key, "content-length") == 0) {
                if (rx->length >= 0) {
                    httpBadRequestError(stream, HTTP_CLOSE | HTTP_CODE_BAD_REQUEST, "Mulitple content length headers");
                    break;
                }
                rx->length = stoi(value);
                if (rx->length < 0) {
                    httpBadRequestError(stream, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad content length");
                    return;
                }
                rx->contentLength = sclone(value);
                assert(rx->length >= 0);
                if (httpServerStream(stream) || !scaselessmatch(tx->method, "HEAD")) {
                    rx->remainingContent = rx->length;
                    rx->needInputPipeline = 1;
                }

            } else if (strcasecmp(key, "content-range") == 0) {
                /*
                    The Content-Range header is used in the response. The Range header is used in the request.
                    This headers specifies the range of any posted body data
                    Format is:  Content-Range: bytes n1-n2/length
                    Where n1 is first byte pos and n2 is last byte pos
                 */
                char    *sp;
                MprOff  start, end, size;

                start = end = size = -1;
                sp = value;
                while (*sp && !isdigit((uchar) *sp)) {
                    sp++;
                }
                if (*sp) {
                    start = stoi(sp);
                    if ((sp = strchr(sp, '-')) != 0) {
                        end = stoi(++sp);
                        if ((sp = strchr(sp, '/')) != 0) {
                            /*
                                Note this is not the content length transmitted, but the original size of the input of which
                                the client is transmitting only a portion.
                             */
                            size = stoi(++sp);
                        }
                    }
                }
                if (start < 0 || end < 0 || size < 0 || end < start) {
                    httpBadRequestError(stream, HTTP_CLOSE | HTTP_CODE_RANGE_NOT_SATISFIABLE, "Bad content range");
                    break;
                }
                rx->inputRange = httpCreateRange(stream, start, end);

            } else if (strcasecmp(key, "content-type") == 0) {
                rx->mimeType = sclone(value);
                if (rx->flags & (HTTP_POST | HTTP_PUT)) {
                    if (httpServerStream(stream)) {
                        rx->form = scontains(rx->mimeType, "application/x-www-form-urlencoded") != 0;
                        rx->json = sstarts(rx->mimeType, "application/json");
                        rx->upload = scontains(rx->mimeType, "multipart/form-data") != 0;
                    }
                }
            } else if (strcasecmp(key, "cookie") == 0) {
                /* Should be only one cookie header really with semicolon delimmited key/value pairs */
                if (rx->cookie && *rx->cookie) {
                    rx->cookie = sjoin(rx->cookie, "; ", value, NULL);
                } else {
                    rx->cookie = sclone(value);
                }
            }
            break;

        case 'e':
            if (strcasecmp(key, "expect") == 0) {
                /*
                    Handle 100-continue for HTTP/1.1+ clients only. This is the only expectation that is currently supported.
                 */
                if (stream->net->protocol > 0) {
                    if (strcasecmp(value, "100-continue") != 0) {
                        httpBadRequestError(stream, HTTP_CODE_EXPECTATION_FAILED, "Expect header value is not supported");
                    } else {
                        rx->flags |= HTTP_EXPECT_CONTINUE;
                    }
                }
            }
            break;

        case 'h':
            if (strcasecmp(key, "host") == 0) {
                if ((int) strspn(value, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-.[]:")
                        < (int) slen(value)) {
                    httpBadRequestError(stream, HTTP_CODE_BAD_REQUEST, "Bad host header");
                } else {
                    rx->hostHeader = sclone(value);
                }
            }
            break;

        case 'i':
            if ((strcasecmp(key, "if-modified-since") == 0) || (strcasecmp(key, "if-unmodified-since") == 0)) {
                MprTime     newDate = 0;
                char        *cp;
                bool        ifModified = (tolower((uchar) key[3]) == 'm');

                if ((cp = strchr(value, ';')) != 0) {
                    *cp = '\0';
                }
                if (mprParseTime(&newDate, value, MPR_UTC_TIMEZONE, NULL) < 0) {
                    break;
                }
                if (newDate) {
                    rx->since = newDate;
                    rx->ifModified = ifModified;
                    rx->flags |= HTTP_IF_MODIFIED;
                }

            } else if ((strcasecmp(key, "if-match") == 0) || (strcasecmp(key, "if-none-match") == 0)) {
                char    *word, *tok;
                bool    ifMatch = (tolower((uchar) key[3]) == 'm');

                if ((tok = strchr(value, ';')) != 0) {
                    *tok = '\0';
                }
                rx->ifMatch = ifMatch;
                rx->flags |= HTTP_IF_MODIFIED;
                value = sclone(value);
                word = stok(value, " ,", &tok);
                while (word) {
                    addMatchEtag(stream, word);
                    word = stok(0, " ,", &tok);
                }

            } else if (strcasecmp(key, "if-range") == 0) {
                char    *word, *tok;
                if ((tok = strchr(value, ';')) != 0) {
                    *tok = '\0';
                }
                rx->ifMatch = 1;
                rx->flags |= HTTP_IF_MODIFIED;
                value = sclone(value);
                word = stok(value, " ,", &tok);
                while (word) {
                    addMatchEtag(stream, word);
                    word = stok(0, " ,", &tok);
                }
            }
            break;

        case 'k':
            /* Keep-Alive: timeout=N, max=1 */
            if (strcasecmp(key, "keep-alive") == 0) {
                if ((tok = scontains(value, "max=")) != 0) {
                    stream->keepAliveCount = atoi(&tok[4]);
                    if (stream->keepAliveCount < 0) {
                        stream->keepAliveCount = 0;
                    }
                    if (stream->keepAliveCount > ME_MAX_KEEP_ALIVE) {
                        stream->keepAliveCount = ME_MAX_KEEP_ALIVE;
                    }
                    /*
                        IMPORTANT: Deliberately close client connections one request early. This encourages a client-led
                        termination and may help relieve excessive server-side TIME_WAIT conditions.
                     */
                    if (httpClientStream(stream) && stream->keepAliveCount == 1) {
                        stream->keepAliveCount = 0;
                    }
                }
            }
            break;

        case 'l':
            if (strcasecmp(key, "location") == 0) {
                rx->redirect = sclone(value);
            }
            break;

        case 'o':
            if (strcasecmp(key, "origin") == 0) {
                rx->origin = sclone(value);
            }
            break;

        case 'p':
            if (strcasecmp(key, "pragma") == 0) {
                rx->pragma = sclone(value);
            }
            break;

        case 'r':
            if (strcasecmp(key, "range") == 0) {
                /*
                    The Content-Range header is used in the response. The Range header is used in the request.
                 */
                if (!parseRange(stream, value)) {
                    httpBadRequestError(stream, HTTP_CLOSE | HTTP_CODE_RANGE_NOT_SATISFIABLE, "Bad range");
                }
            } else if (strcasecmp(key, "referer") == 0) {
                /* NOTE: yes the header is misspelt in the spec */
                rx->referrer = sclone(value);
            }
            break;

        case 't':
#if DONE_IN_HTTP1
            if (strcasecmp(key, "transfer-encoding") == 0 && stream->net->protocol == 1) {
                if (scaselesscmp(value, "chunked") == 0) {
                    /*
                        remainingContent will be revised by the chunk filter as chunks are processed and will
                        be set to zero when the last chunk has been received.
                     */
                    rx->flags |= HTTP_CHUNKED;
                    rx->chunkState = HTTP_CHUNK_START;
                    rx->remainingContent = HTTP_UNLIMITED;
                    rx->needInputPipeline = 1;
                }
            }
#endif
            break;

        case 'x':
            if (strcasecmp(key, "x-http-method-override") == 0) {
                httpSetMethod(stream, value);

            } else if (strcasecmp(key, "x-own-params") == 0) {
                /*
                    Optimize and don't convert query and body content into params.
                    This is for those who want very large forms and to do their own custom handling.
                 */
                rx->ownParams = 1;
#if ME_DEBUG
            } else if (strcasecmp(key, "x-chunk-size") == 0 && net->protocol < 2) {
                tx->chunkSize = atoi(value);
                if (tx->chunkSize <= 0) {
                    tx->chunkSize = 0;
                } else if (tx->chunkSize > stream->limits->chunkSize) {
                    tx->chunkSize = stream->limits->chunkSize;
                }
#endif
            }
            break;

        case 'u':
            if (scaselesscmp(key, "upgrade") == 0) {
                rx->upgrade = sclone(value);
            } else if (strcasecmp(key, "user-agent") == 0) {
                rx->userAgent = sclone(value);
            }
            break;

        case 'w':
            if (strcasecmp(key, "www-authenticate") == 0) {
                cp = value;
                while (*value && !isspace((uchar) *value)) {
                    value++;
                }
                *value++ = '\0';
                stream->authType = slower(cp);
                rx->authDetails = sclone(value);
            }
            break;
        }
    }
    if (httpServerStream(stream)) {
        hostname = rx->hostHeader;
        if (schr(rx->hostHeader, ':')) {
            mprParseSocketAddress(rx->hostHeader, &hostname, NULL, NULL, 0);
        }
        if ((stream->host = httpMatchHost(net, hostname)) == 0) {
            stream->host = mprGetFirstItem(net->endpoint->hosts);
            httpError(stream, HTTP_CLOSE | HTTP_CODE_NOT_FOUND, "No listening endpoint for request for %s", rx->hostHeader);
            /* continue */
        }
        rx->streaming = httpGetStreaming(stream);
        if (!rx->upload) {
            if (rx->length >= stream->limits->rxBodySize && stream->limits->rxBodySize != HTTP_UNLIMITED) {
                httpLimitError(stream, HTTP_ABORT | HTTP_CODE_REQUEST_TOO_LARGE,
                    "Request content length %lld bytes is too big. Limit %lld", rx->length, stream->limits->rxBodySize);
            }
            if (rx->form && rx->length >= stream->limits->rxFormSize && stream->limits->rxFormSize != HTTP_UNLIMITED) {
                httpLimitError(stream, HTTP_CLOSE | HTTP_CODE_REQUEST_TOO_LARGE,
                    "Request form of %lld bytes is too big. Limit %lld", rx->length, stream->limits->rxFormSize);
            }
        }
        if (!rx->originalUri) {
            rx->originalUri = rx->uri;
        }
        if (rx->flags & HTTP_EXPECT_CONTINUE && !stream->error) {
            sendContinue(stream);
            rx->flags &= ~HTTP_EXPECT_CONTINUE;
        }
    }
    if (net->protocol == 0 && !keepAliveHeader) {
        stream->keepAliveCount = 0;
    }
}


/*
    Called once the HTTP request/response headers have been parsed
 */
static int processParsed(HttpStream *stream)
{
    HttpRx      *rx;

    rx = stream->rx;

    if (stream->disconnect) {
        return HTTP_STATE_READY;
    }
    if (httpServerStream(stream)) {
        parseUri(stream);
        httpAddQueryParams(stream);
        if (rx->streaming) {
            routeRequest(stream);
            httpStartHandler(stream);
        } else if (rx->upload) {
            routeRequest(stream);
        } else {
            stream->readq->max = stream->limits->rxFormSize;
        }
    } else {
        /*
            Google does responses with a body and without a Content-Length like this:
                Connection: close
                Location: URI
            Don't do this for HTTP/2 which must have remainingContent == HTTP_UNLIMITED
         */
        if (stream->keepAliveCount <= 0 && rx->length < 0 && rx->chunkState == HTTP_CHUNK_UNCHUNKED &&
                stream->net->protocol < 2 && rx->status != HTTP_CODE_NO_CONTENT) {
            rx->remainingContent = rx->redirect ? 0 : HTTP_UNLIMITED;
        }
    }
#if ME_HTTP_WEB_SOCKETS
    if (httpIsClient(stream->net) && stream->upgraded && !stream->proxied && !httpVerifyWebSocketsHandshake(stream)) {
        return HTTP_STATE_FINALIZED;
    }
#endif
    return HTTP_STATE_CONTENT;
}


static int processContent(HttpStream *stream)
{
    int     state;

    state = stream->state;

    /*
        For HTTP/1 if the request has no body, need to create the end packet here.
        Can't rely on the chunkFilter as there will be no packet flowing through it.
     */
    if (stream->rx->remainingContent == 0) {
        httpAddInputEndPacket(stream, stream->inputq);
        state = HTTP_STATE_READY;
    }
    return state;
}


/*
    In the ready state after all content has been received.
    If streaming, the request will have already been routed and the handler started.
 */
static int processReady(HttpStream *stream)
{
    HttpRx      *rx;

    rx = stream->rx;
    if (rx->eof) {
        if (httpServerStream(stream)) {
            if (httpAddBodyParams(stream) < 0) {
                httpError(stream, HTTP_CODE_BAD_REQUEST, "Bad request parameters");
            } else {
                routeRequest(stream);
                httpStartHandler(stream);
            }
        }
    }
    httpReadyHandler(stream);
    if (httpClientStream(stream) && !stream->upgraded) {
        httpFinalize(stream);
    }
    return HTTP_STATE_RUNNING;
}


static int processFinalized(HttpStream *stream)
{
    HttpRx      *rx;
    HttpTx      *tx;

    rx = stream->rx;
    tx = stream->tx;

#if ME_TRACE_MEM
    /*
        Enable during development to server memory footprint as requests are processed.
     */
    mprDebug(1, "Request complete, status %d, error %d, connError %d, %s%s, memsize %.2f MB",
        tx->status, stream->error, stream->net->error, rx->hostHeader, rx->uri, mprGetMem() / 1024 / 1024.0);
#endif

    if (httpServerStream(stream) && rx) {
        httpMonitorEvent(stream, HTTP_COUNTER_NETWORK_IO, tx->bytesWritten);
        httpLogCompleteRequest(stream);
    }
    httpClosePipeline(stream);

    if (tx->errorDocument && !smatch(tx->errorDocument, rx->uri)) {
        return prepErrorDoc(stream);
    }
    if (rx->session) {
        httpWriteSession(stream);
    }
    if (stream->net->eof && stream->net->protocol > 0) {
        if (!stream->errorMsg) {
            stream->errorMsg = stream->sock->errorMsg ? stream->sock->errorMsg : sclone("Server close");
        }
        httpLog(stream->trace, "http.connection.close", "network", "msg:%s", stream->errorMsg);
    }

    if (httpServerStream(stream)) {
        httpMonitorEvent(stream, HTTP_COUNTER_ACTIVE_REQUESTS, -1);
        stream->counted = 0;
    }
    return HTTP_STATE_COMPLETE;
}


/*
    This is done as a top level event incase triggered deep in the pipeline stack.
 */
static void processComplete(HttpStream *stream, MprEvent *event)
{
    HttpNet     *net;

    net = stream->net;

    if (stream->completed) {
        return;
    }
    stream->completed = 1;

    if (stream->http->requestCallback) {
        (stream->http->requestCallback)(stream);
    }
    HTTP_NOTIFY(stream, HTTP_EVENT_DONE, 0);

    if (net->protocol == 0) {
        net->eof = 1;
    }
}


static void routeRequest(HttpStream *stream)
{
    HttpRx      *rx;
    HttpTx      *tx;

    rx = stream->rx;
    tx = stream->tx;

    if (!rx->route) {
        httpRouteRequest(stream);
        httpCreatePipeline(stream);
        httpStartPipeline(stream);
        httpReplayPackets(stream->rxHead, stream->transferq);
    }
}


/*
    Invoke the WRITABLE notifier and the writable() callback.
 */
PUBLIC bool httpPumpOutput(HttpQueue *q)
{
    HttpStream  *stream;
    HttpTx      *tx;
    HttpQueue   *wq;
    ssize       count;

    stream = q->stream;
    tx = stream->tx;

    httpServiceNetQueues(q->net, HTTP_BLOCK);

    if (tx->started && !stream->net->writeBlocked) {
        wq = stream->writeq;
        count = wq->count;
        if (!tx->finalizedOutput) {
            HTTP_NOTIFY(stream, HTTP_EVENT_WRITABLE, 0);
            if (tx->handler && tx->handler->writable) {
                tx->handler->writable(wq);
            }
        }
        return (wq->count - count) ? 1 : 0;
    }
    return 0;
}


static int prepErrorDoc(HttpStream *stream)
{
    HttpRx      *rx;
    HttpTx      *tx;

    rx = stream->rx;
    tx = stream->tx;

    if (!rx->headerPacket || stream->errorDoc) {
        return stream->state;
    }
    httpLog(stream->trace, "tx.http.errordoc", "context", "location:%s, status:%d", tx->errorDocument, tx->status);

    stream->state = HTTP_STATE_COMPLETE;
    httpResetServerStream(stream);

    stream->rx->headers = rx->headers;
    stream->rx->method = rx->method;
    stream->rx->originalMethod = rx->originalMethod;
    stream->rx->originalUri = rx->uri;
    stream->rx->uri = (char*) tx->errorDocument;
    stream->tx->status = tx->status;
    stream->state = HTTP_STATE_PARSED;
    return processParsed(stream);
}


PUBLIC void httpSetMethod(HttpStream *stream, cchar *method)
{
    stream->rx->method = sclone(method);
    httpParseMethod(stream);
}


PUBLIC ssize httpGetReadCount(HttpStream *stream)
{
    return stream->readq->count;
}


PUBLIC cchar *httpGetBodyInput(HttpStream *stream)
{
    HttpQueue   *q;
    MprBuf      *content;

    if (!stream->rx->eof) {
        return 0;
    }
    q = stream->readq;
    if (q->first) {
        httpJoinPackets(q, -1);
        if ((content = q->first->content) != 0) {
            mprAddNullToBuf(content);
            return mprGetBufStart(content);
        }
    }
    return 0;
}


static void addMatchEtag(HttpStream *stream, char *etag)
{
    HttpRx   *rx;

    rx = stream->rx;
    if (rx->etags == 0) {
        rx->etags = mprCreateList(-1, MPR_LIST_STABLE);
    }
    mprAddItem(rx->etags, sclone(etag));
}


/*
    Format is:  Range: bytes=n1-n2,n3-n4,...
    Where n1 is first byte pos and n2 is last byte pos

    Examples:
        Range: bytes=0-49             first 50 bytes
        Range: bytes=50-99,200-249    Two 50 byte ranges from 50 and 200
        Range: bytes=-50              Last 50 bytes
        Range: bytes=1-               Skip first byte then emit the rest

    Return 1 if more ranges, 0 if end of ranges, -1 if bad range.
 */
static bool parseRange(HttpStream *stream, char *value)
{
    HttpTx      *tx;
    HttpRange   *range, *last, *next;
    char        *tok, *ep;

    tx = stream->tx;
    value = sclone(value);

    /*
        Step over the "bytes="
     */
    stok(value, "=", &value);

    for (last = 0; value && *value; ) {
        if ((range = httpCreateRange(stream, 0, 0)) == 0) {
            return 0;
        }
        /*
            A range "-7" will set the start to -1 and end to 8
         */
        if ((tok = stok(value, ",", &value)) == 0) {
            return 0;
        }
        if (*tok != '-') {
            range->start = (ssize) stoi(tok);
        } else {
            range->start = -1;
        }
        range->end = -1;

        if ((ep = strchr(tok, '-')) != 0) {
            if (*++ep != '\0') {
                /*
                    End is one beyond the range. Makes the math easier.
                 */
                range->end = (ssize) stoi(ep) + 1;
            }
        }
        if (range->start >= 0 && range->end >= 0) {
            range->len = (int) (range->end - range->start);
        }
        if (last == 0) {
            tx->outputRanges = range;
        } else {
            last->next = range;
        }
        last = range;
    }

    /*
        Validate ranges
     */
    for (range = tx->outputRanges; range; range = range->next) {
        if (range->end != -1 && range->start >= range->end) {
            return 0;
        }
        if (range->start < 0 && range->end < 0) {
            return 0;
        }
        next = range->next;
        if (range->start < 0 && next) {
            /* This range goes to the end, so cannot have another range afterwards */
            return 0;
        }
        if (next) {
            if (range->end < 0) {
                return 0;
            }
            if (next->start >= 0 && range->end > next->start) {
                return 0;
            }
        }
    }
    stream->tx->currentRange = tx->outputRanges;
    return (last) ? 1: 0;
}


static void parseUri(HttpStream *stream)
{
    HttpRx      *rx;
    HttpUri     *up;
    cchar       *hostname;

    rx = stream->rx;
    if (httpSetUri(stream, rx->uri) < 0) {
        httpBadRequestError(stream, HTTP_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad URL");
        rx->parsedUri = httpCreateUri("", 0);

    } else {
        /*
            Complete the URI based on the connection state. Must have a complete scheme, host, port and path.
         */
        up = rx->parsedUri;
        up->scheme = sclone(stream->secure ? "https" : "http");
        hostname = rx->hostHeader ? rx->hostHeader : stream->host->name;
        if (!hostname) {
            hostname = stream->sock->acceptIp;
        }
        if (mprParseSocketAddress(hostname, &up->host, NULL, NULL, 0) < 0 || up->host == 0 || *up->host == '\0') {
            if (!stream->error) {
                httpBadRequestError(stream, HTTP_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad host");
            }
        } else {
            up->port = stream->sock->listenSock->port;
        }
    }
}


/*
    Sends an 100 Continue response to the client. This bypasses the transmission pipeline, writing directly to the socket.
 */
static int sendContinue(HttpStream *stream)
{
    cchar   *response;
    int     mode;

    assert(stream);

    if (!stream->tx->finalized && !stream->tx->bytesWritten) {
        response = sfmt("%s 100 Continue\r\n\r\n", httpGetProtocol(stream->net));
        mode = mprGetSocketBlockingMode(stream->sock);
        mprWriteSocket(stream->sock, response, slen(response));
        mprSetSocketBlockingMode(stream->sock, mode);
        mprFlushSocket(stream->sock);
    }
    return 0;
}


PUBLIC cchar *httpTraceHeaders(MprHash *headers)
{
    MprBuf  *buf;
    MprKey  *kp;

    buf = mprCreateBuf(0, 0);
    for (ITERATE_KEYS(headers, kp)) {
        if (*kp->key == '=') {
            mprPutToBuf(buf, ":%s: %s\n", &kp->key[1], (char*) kp->data);
        } else {
            mprPutToBuf(buf, "%s: %s\n", kp->key, (char*) kp->data);
        }
    }
    mprAddNullToBuf(buf);
    return mprGetBufStart(buf);
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/queue.c ************/

/*
    queue.c -- Queue support routines. Queues are the bi-directional data flow channels for the pipeline.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/********************************** Forwards **********************************/

static void initQueue(HttpNet *net, HttpStream *stream, HttpQueue *q, cchar *name, int dir, int flags);
static void manageQueue(HttpQueue *q, int flags);

/************************************ Code ************************************/
/*
    Create a queue head that has no processing callbacks
 */
PUBLIC HttpQueue *httpCreateQueueHead(HttpNet *net, HttpStream *stream, cchar *name, int dir)
{
    HttpQueue   *q;

    assert(net);

    if ((q = mprAllocObj(HttpQueue, manageQueue)) == 0) {
        return 0;
    }
    initQueue(net, stream, q, name, dir, HTTP_QUEUE_HEAD);
    httpInitSchedulerQueue(q);
    return q;
}


/*
    If prev is supplied, this queue is appended after the given "prev" queue.
    Returns the new queue.
 */
PUBLIC HttpQueue *httpCreateQueue(HttpNet *net, HttpStream *stream, HttpStage *stage, int dir, HttpQueue *prev)
{
    HttpQueue   *q;
    int         flags;

    if ((q = mprAllocObj(HttpQueue, manageQueue)) == 0) {
        return 0;
    }
    if (stage) {
        flags = (stage->flags & HTTP_STAGE_QHEAD) ? HTTP_QUEUE_HEAD : 0;
        initQueue(net, stream, q, stage->name, dir, flags);
        httpAssignQueueCallbacks(q, stage, dir);
    }
    httpInitSchedulerQueue(q);
    if (prev) {
        httpAppendQueue(q, prev);
    }
    return q;
}


static void initQueue(HttpNet *net, HttpStream *stream, HttpQueue *q, cchar *name, int dir, int flags)
{
    if (dir == HTTP_QUEUE_TX) {
        flags |= HTTP_QUEUE_OUTGOING;
    }
    q->net = net;
    q->stream = stream;
    q->nextQ = q;
    q->prevQ = q;
    q->name = sclone(name);
    if (stream && stream->tx && stream->tx->chunkSize > 0) {
        q->packetSize = stream->tx->chunkSize;
    } else {
        q->packetSize = net->limits->packetSize;
    }
    q->max = q->packetSize * ME_QUEUE_MAX_FACTOR;
    q->low = q->packetSize;
    q->flags = flags;
}


static void manageQueue(HttpQueue *q, int flags)
{
    HttpPacket      *packet;

    if (flags & MPR_MANAGE_MARK) {
        mprMark(q->name);
        mprMark(q->nextQ);
        for (packet = q->first; packet; packet = packet->next) {
            mprMark(packet);
        }
        mprMark(q->stream);
        mprMark(q->last);
        mprMark(q->prevQ);
        mprMark(q->stage);
        mprMark(q->scheduleNext);
        mprMark(q->schedulePrev);
        mprMark(q->pair);
        mprMark(q->queueData);
        if (q->nextQ && q->nextQ->stage) {
            /* Not a queue head */
            mprMark(q->nextQ);
        }
    }
}


/*
    Assign stage callbacks to a queue
 */
PUBLIC void httpAssignQueueCallbacks(HttpQueue *q, HttpStage *stage, int dir)
{
    q->stage = stage;
    q->close = stage->close;
    q->open = stage->open;
    q->start = stage->start;
    if (dir == HTTP_QUEUE_TX) {
        q->put = stage->outgoing;
        q->service = stage->outgoingService;
    } else {
        q->put = stage->incoming;
        q->service = stage->incomingService;
    }
}


PUBLIC void httpSetQueueLimits(HttpQueue *q, HttpLimits *limits, ssize packetSize, ssize low, ssize max, ssize window)
{
    if (packetSize < 0) {
        if (limits) {
            packetSize = limits->packetSize;
        } else {
            packetSize = ME_PACKET_SIZE;
        }
    }
    if (max < 0) {
        max = q->packetSize * ME_QUEUE_MAX_FACTOR;
    }
    if (low < 0) {
        low = q->packetSize;
    }
    q->packetSize = packetSize;
    q->max = max;
    q->low = low;

#if ME_HTTP_HTTP2
    if (window < 0) {
        if (limits) {
            window = limits->window;
        } else {
            window = HTTP2_MIN_WINDOW;
        }
    }
    q->window = window;
#endif
}


PUBLIC void httpPairQueues(HttpQueue *q1, HttpQueue *q2)
{
    q1->pair = q2;
    q2->pair = q1;
}


PUBLIC bool httpIsQueueSuspended(HttpQueue *q)
{
    return (q->flags & HTTP_QUEUE_SUSPENDED) ? 1 : 0;
}


PUBLIC void httpSuspendQueue(HttpQueue *q)
{
    q->flags |= HTTP_QUEUE_SUSPENDED;
}


PUBLIC bool httpIsSuspendQueue(HttpQueue *q)
{
    return q->flags & HTTP_QUEUE_SUSPENDED;
}


/*
    Remove all data in the queue. If removePackets is true, actually remove the packet too.
    This preserves the header and EOT packets.
 */
PUBLIC void httpDiscardQueueData(HttpQueue *q, bool removePackets)
{
    HttpPacket  *packet, *prev, *next;
    ssize       len;

    if (q == 0) {
        return;
    }
    for (prev = 0, packet = q->first; packet; packet = next) {
        next = packet->next;
        if (packet->flags & (HTTP_PACKET_RANGE | HTTP_PACKET_DATA)) {
            if (removePackets) {
                if (prev) {
                    prev->next = next;
                } else {
                    q->first = next;
                }
                if (packet == q->last) {
                    q->last = prev;
                }
                q->count -= httpGetPacketLength(packet);
                assert(q->count >= 0);
                if (q->flags & HTTP_QUEUE_SUSPENDED && q->count < q->max) {
                    httpResumeQueue(q, 1);
                }
                continue;
            } else {
                len = httpGetPacketLength(packet);
                if (q->stream && q->stream->tx && q->stream->tx->length > 0) {
                    q->stream->tx->length -= len;
                }
                q->count -= len;
                assert(q->count >= 0);
                if (packet->content) {
                    mprFlushBuf(packet->content);
                } else if (packet->esize > 0) {
                    packet->esize = 0;
                }
                if (q->flags & HTTP_QUEUE_SUSPENDED && q->count < q->max) {
                    httpResumeQueue(q, 1);
                }
            }
        }
        prev = packet;
    }
}


/*
    Flush queue data toward the network connector by scheduling the queue and servicing all scheduled queues.
    Return true if there is room for more data. If blocking is requested, the call will block until
    the queue count falls below the queue max. NOTE: may return early if the inactivityTimeout expires.
    WARNING: may yield.
 */
PUBLIC bool httpFlushQueue(HttpQueue *q, int flags)
{
    HttpNet     *net;
    HttpStream  *stream;
    MprTicks    timeout;
    int         events, mask;

    net = q->net;
    stream = q->stream;

    /*
        Initiate flushing.
     */
    if (!(q->flags & HTTP_QUEUE_SUSPENDED)) {
        httpScheduleQueue(q);
    }
    httpServiceNetQueues(net, flags);

    /*
        For HTTP/2 we must process incoming window update frames, so run any pending IO events.
     */
    if (net->protocol == HTTP_2) {
        //  WARNING: may yield
        mprWaitForEvent(stream->dispatcher, 0, mprGetEventMark(stream->dispatcher));
    }
    if (net->error) {
        return 1;
    }

    while (q->count > 0 && !stream->error && !net->error) {
        timeout = (flags & HTTP_BLOCK) ? stream->limits->inactivityTimeout : 0;
        mask = MPR_READABLE;
        if (net->socketq->count > 0 || net->ioCount > 0) {
            mask = MPR_WRITABLE;
        }
        if ((events = mprWaitForSingleIO((int) net->sock->fd, mask, timeout)) != 0) {
            stream->lastActivity = net->lastActivity = net->http->now;
            if (events & MPR_READABLE) {
                httpReadIO(net);
            }
            if ((net->socketq->count > 0 || net->ioCount > 0) && (events & MPR_WRITABLE)) {
                net->lastActivity = net->http->now;
                httpResumeQueue(net->socketq, 1);
                httpServiceNetQueues(net, flags);
            }
            if (net->protocol == HTTP_2) {
                /*
                    Process HTTP/2 window update messages for flow control. WARNING: may yield.
                 */
                mprWaitForEvent(stream->dispatcher, 0, mprGetEventMark(stream->dispatcher));
            }
        }
        httpServiceNetQueues(net, flags);
        if (!(flags & HTTP_BLOCK)) {
            break;
        }
    }
    return (q->count < q->max) ? 1 : 0;
}


PUBLIC void httpFlush(HttpStream *stream)
{
    httpFlushQueue(stream->writeq, HTTP_NON_BLOCK);
}


/*
    Flush the write queue. In sync mode, this call may yield.
 */
PUBLIC void httpFlushAll(HttpStream *stream)
{
    httpFlushQueue(stream->writeq, stream->net->async ? HTTP_NON_BLOCK : HTTP_BLOCK);
}


PUBLIC bool httpResumeQueue(HttpQueue *q, bool schedule)
{
    HttpQueue   *prevQ;

    if (q) {
        if (q->flags & HTTP_QUEUE_SUSPENDED || schedule) {
            q->flags &= ~HTTP_QUEUE_SUSPENDED;
            httpScheduleQueue(q);
        }
        prevQ = httpFindPreviousQueue(q);
        if (q->count == 0 && prevQ && prevQ->flags & HTTP_QUEUE_SUSPENDED) {
            httpResumeQueue(prevQ, schedule);
            return 1;
        }
    }
    return 0;
}


PUBLIC HttpQueue *httpFindNextQueue(HttpQueue *q)
{
    for (q = q->nextQ; q; q = q->nextQ) {
        if (q->service || q->count) {
            return q;
        }
        if (q->flags & HTTP_QUEUE_HEAD) {
            return q;
        }
    }
    return 0;
}


PUBLIC HttpQueue *httpFindPreviousQueue(HttpQueue *q)
{
    for (q = q->prevQ; q; q = q->prevQ) {
        if (q->service || q->count) {
            return q;
        }
        if (q->flags & HTTP_QUEUE_HEAD) {
            return q;
        }
    }
    return 0;
}


PUBLIC HttpQueue *httpGetNextQueueForService(HttpQueue *q)
{
    HttpQueue     *next;

    if (q->scheduleNext != q) {
        next = q->scheduleNext;
        next->schedulePrev->scheduleNext = next->scheduleNext;
        next->scheduleNext->schedulePrev = next->schedulePrev;
        next->schedulePrev = next->scheduleNext = next;
        return next;
    }
    return 0;
}


/*
    Return the number of bytes the queue will accept. Always positive.
 */
PUBLIC ssize httpGetQueueRoom(HttpQueue *q)
{
    assert(q->max > 0);
    assert(q->count >= 0);

    if (q->count >= q->max) {
        return 0;
    }
    return q->max - q->count;
}


PUBLIC void httpInitSchedulerQueue(HttpQueue *q)
{
    q->scheduleNext = q;
    q->schedulePrev = q;
}


/*
    Append a queue after the previous element
 */
PUBLIC HttpQueue *httpAppendQueue(HttpQueue *q, HttpQueue *prev)
{
    q->nextQ = prev->nextQ;
    q->prevQ = prev;
    prev->nextQ->prevQ = q;
    prev->nextQ = q;
    return q;
}


PUBLIC bool httpIsQueueEmpty(HttpQueue *q)
{
    return q->first == 0;
}


PUBLIC void httpRemoveQueue(HttpQueue *q)
{
    q->prevQ->nextQ = q->nextQ;
    q->nextQ->prevQ = q->prevQ;
    q->prevQ = q->nextQ = q;
}


#if KEEP
PUBLIC void httpCheckQueues(HttpQueue *q)
{
    HttpQueue     *head;
    HttpQueue     *next;

    head = q->net->serviceq;

    for (next = head->scheduleNext; next != head; next = next->scheduleNext) {
        //  Some assert here
    }
}
#endif


PUBLIC void httpScheduleQueue(HttpQueue *q)
{
    HttpQueue     *head;

    head = q->net->serviceq;

    assert(q != head);
    if (q->service && q->scheduleNext == q && !(q->flags & HTTP_QUEUE_SUSPENDED)) {
        assert(q->service);
        q->scheduleNext = head;
        q->schedulePrev = head->schedulePrev;
        head->schedulePrev->scheduleNext = q;
        head->schedulePrev = q;
    }
}


PUBLIC void httpServiceQueue(HttpQueue *q)
{
    /*
        Hold the queue for GC while scheduling.
        Not typically required as the queue is typically linked into a pipeline.
     */
    if (q->servicing) {
        q->flags |= HTTP_QUEUE_RESERVICE;
        return;
    }
    if (q->net) {
        q->net->holdq = q;
    }
    /*
        Since we are servicing this "q" now, we can remove from the schedule queue if it is already queued.
     */
    if (q->net->serviceq->scheduleNext == q) {
        httpGetNextQueueForService(q->net->serviceq);
    }
    if (q->flags & HTTP_QUEUE_SUSPENDED) {
        return;
    }
    q->servicing = 1;
    q->service(q);

    if (q->flags & HTTP_QUEUE_RESERVICE) {
        q->flags &= ~HTTP_QUEUE_RESERVICE;
        httpScheduleQueue(q);
    }
    q->flags |= HTTP_QUEUE_SERVICED;
    q->servicing = 0;
}


PUBLIC void httpServiceQueues(HttpStream *stream, int flags)
{
    httpServiceNetQueues(stream->net, flags);
}


/*
    Run the queue service routines until there is no more work to be done.
    If flags & HTTP_BLOCK, this routine may block while yielding.  Return true if actual work was done.
 */
PUBLIC void httpServiceNetQueues(HttpNet *net, int flags)
{
    HttpQueue   *q;

    while ((q = httpGetNextQueueForService(net->serviceq)) != NULL) {
        /*
            When assigning queue callbacks in pipeline, may cause an already queued handler to have
            no service callback when it is actually invoked here.
         */
        if (q->service) {
            httpServiceQueue(q);
        }
        if ((flags & HTTP_BLOCK) && mprNeedYield()) mprYield(0);
    }
    /*
        Always do a yield if requested even if there are no queues to service
     */
    if ((flags & HTTP_BLOCK) && mprNeedYield()) mprYield(0);
}


/*
    Return true if the next queue will accept this packet. If not, then disable the queue's service procedure.
    This may split the packet if it exceeds the downstreams maximum packet size.
    If the nextQ does not have a service routine, use the next stage that does.
 */
PUBLIC bool httpWillQueueAcceptPacket(HttpQueue *q, HttpQueue *nextQ, HttpPacket *packet)
{
    ssize       room, size;

    size = httpGetPacketLength(packet);
    if (nextQ->service == 0) {
        nextQ = httpFindNextQueue(nextQ);
    }
    if (nextQ == 0) {
        return 1;
    }
    room = nextQ->max - nextQ->count;
    if (size <= room || (size - room) < HTTP_QUEUE_ALLOW) {
        return 1;
    }
    if (room > 0) {
        /*
            Resize the packet to fit downstream. This will putback the tail if required.
         */
        httpResizePacket(q, packet, room);
        size = httpGetPacketLength(packet);
        if (size > 0) {
            return 1;
        }
    }
    /*
        The downstream queue cannot accept this packet, so disable queue and mark the downstream queue as full and service
     */
    httpSuspendQueue(q);
    if (!(nextQ->flags & HTTP_QUEUE_SUSPENDED)) {
        httpScheduleQueue(nextQ);
    }
    return 0;
}


PUBLIC bool httpWillNextQueueAcceptPacket(HttpQueue *q, HttpPacket *packet)
{
    HttpQueue   *nextQ;

    if ((nextQ = httpFindNextQueue(q)) == 0) {
        return 1;
    }
    return httpWillQueueAcceptPacket(q, nextQ, packet);
}


/*
    Return true if the next queue will accept a certain amount of data. If not, then disable the queue's service procedure.
    Will not split the packet.
 */
PUBLIC bool httpWillNextQueueAcceptSize(HttpQueue *q, ssize size)
{
    HttpQueue   *nextQ;

    nextQ = q->nextQ;
    if (size <= nextQ->packetSize && (size + nextQ->count) <= nextQ->max) {
        return 1;
    }
    httpSuspendQueue(q);
    if (!(nextQ->flags & HTTP_QUEUE_SUSPENDED)) {
        httpScheduleQueue(nextQ);
    }
    return 0;
}


PUBLIC void httpTransferPackets(HttpQueue *inq, HttpQueue *outq)
{
    HttpPacket  *packet;

    if (inq != outq) {
        while ((packet = httpGetPacket(inq)) != 0) {
            httpPutPacket(outq, packet);
        }
    }
}


/*
    Replay packets through the untraversed portions of the pipeline.
    If the in and out queues are the same, the packes are replayed through the put() routine.
 */
PUBLIC void httpReplayPackets(HttpQueue *inq, HttpQueue *outq)
{
    HttpPacket  *chain, *next, *packet;

    if (inq != outq) {
        //  Different queues, can simply transfer
        httpTransferPackets(inq, outq);

    } else {
        //  Unlink the chain and then put back to the queue.
        chain = inq->first;
        inq->first = NULL;
        inq->count = 0;
        for (packet = chain; packet; packet = next) {
            next = packet->next;
            packet->next = 0;
            httpPutPacket(outq, packet);
        }
    }
}


#if ME_DEBUG
PUBLIC bool httpVerifyQueue(HttpQueue *q)
{
    HttpPacket  *packet;
    ssize       count;

    count = 0;
    for (packet = q->first; packet; packet = packet->next) {
        if (packet->next == 0) {
            assert(packet == q->last);
        }
        count += httpGetPacketLength(packet);
    }
    assert(count == q->count);
    return count <= q->count;
}
#endif

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/rangeFilter.c ************/

/*
    rangeFilter.c - Ranged request filter.

    This is an output only filter to select a subet range of data to transfer to the client.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/********************************** Defines ***********************************/

#define HTTP_RANGE_BUFSIZE 128              /* Packet size to hold range boundary */

/********************************** Forwards **********************************/

static HttpPacket *selectBytes(HttpQueue *q, HttpPacket *packet);
static void createRangeBoundary(HttpStream *stream);
static HttpPacket *createRangePacket(HttpStream *stream, HttpRange *range);
static HttpPacket *createFinalRangePacket(HttpStream *stream);
static void manageRange(HttpRange *range, int flags);
static void outgoingRangeService(HttpQueue *q);
static int fixRangeLength(HttpStream *stream, HttpQueue *q);
static int matchRange(HttpStream *stream, HttpRoute *route, int dir);
static void startRange(HttpQueue *q);

/*********************************** Code *************************************/

PUBLIC int httpOpenRangeFilter()
{
    HttpStage     *filter;

    if ((filter = httpCreateFilter("rangeFilter", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    HTTP->rangeFilter = filter;
    filter->match = matchRange;
    filter->start = startRange;
    filter->outgoingService = outgoingRangeService;
    return 0;
}


PUBLIC HttpRange *httpCreateRange(HttpStream *stream, MprOff start, MprOff end)
{
    HttpRange     *range;

    if ((range = mprAllocObj(HttpRange, manageRange)) == 0) {
        return 0;
    }
    range->start = start;
    range->end = end;
    range->len = end - start;
    return range;
}


static void manageRange(HttpRange *range, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(range->next);
    }
}


/*
    This is called twice: once for TX and once for RX
 */
static int matchRange(HttpStream *stream, HttpRoute *route, int dir)
{
    assert(stream->rx);

    httpSetHeader(stream, "Accept-Ranges", "bytes");
    if ((dir & HTTP_STAGE_TX) && stream->tx->outputRanges) {
        return HTTP_ROUTE_OK;
    }
    return HTTP_ROUTE_OMIT_FILTER;
}


static void startRange(HttpQueue *q)
{
    HttpStream  *stream;
    HttpTx      *tx;

    stream = q->stream;
    tx = stream->tx;
    /*
        The httpContentNotModified routine can set outputRanges to zero if returning not-modified.
     */
    if (tx->outputRanges == 0 || tx->status != HTTP_CODE_OK) {
        httpRemoveQueue(q);
        tx->outputRanges = 0;
    } else {
        tx->status = HTTP_CODE_PARTIAL;
        /*
            More than one range so create a range boundary (like chunking)
         */
        if (tx->outputRanges->next) {
            createRangeBoundary(stream);
        }
    }
}


static void outgoingRangeService(HttpQueue *q)
{
    HttpPacket  *packet;
    HttpStream  *stream;
    HttpTx      *tx;
    int         rc;

    stream = q->stream;
    tx = stream->tx;

    if (!(q->flags & HTTP_QUEUE_SERVICED)) {
        /*
            The httpContentNotModified routine can set outputRanges to zero if returning not-modified.
         */
        if ((rc = fixRangeLength(stream, q)) <= 0) {
            if (!q->servicing) {
                httpRemoveQueue(q);
            }
            tx->outputRanges = 0;
            if (rc == 0) {
                tx->status = HTTP_CODE_OK;
            }
        }
    }
    for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
        if (tx->outputRanges) {
            if (packet->flags & HTTP_PACKET_DATA) {
                if ((packet = selectBytes(q, packet)) == 0) {
                    continue;
                }
            } else if (packet->flags & HTTP_PACKET_END) {
                if (tx->rangeBoundary) {
                    httpPutPacketToNext(q, createFinalRangePacket(stream));
                }
            }
        }
        if (!httpWillNextQueueAcceptPacket(q, packet)) {
            httpPutBackPacket(q, packet);
            return;
        }
        httpPutPacketToNext(q, packet);
    }
}


static HttpPacket *selectBytes(HttpQueue *q, HttpPacket *packet)
{
    HttpRange   *range;
    HttpStream  *stream;
    HttpTx      *tx;
    MprOff      endPacket, length, gap, span;
    ssize       count;

    stream = q->stream;
    tx = stream->tx;

    if ((range = tx->currentRange) == 0) {
        return 0;
    }

    /*
        Process the data packet over multiple ranges ranges until all the data is processed or discarded.
     */
    while (range && packet) {
        length = httpGetPacketLength(packet);
        if (length <= 0) {
            return 0;
        }
        endPacket = tx->rangePos + length;
        if (endPacket < range->start) {
            /* Packet is before the next range, so discard the entire packet and seek forwards */
            tx->rangePos += length;
            return 0;

        } else if (tx->rangePos < range->start) {
            /*  Packet starts before range so skip some data, but some packet data is in range */
            gap = (range->start - tx->rangePos);
            tx->rangePos += gap;
            if (gap < length) {
                httpAdjustPacketStart(packet, (ssize) gap);
            }
            if (tx->rangePos >= range->end) {
                range = tx->currentRange = range->next;
            }
            /* Keep going and examine next range */

        } else {
            /* In range */
            assert(range->start <= tx->rangePos && tx->rangePos < range->end);
            span = min(length, (range->end - tx->rangePos));
            span = max(span, 0);
            count = (ssize) min(span, q->nextQ->packetSize);
            assert(count > 0);
            if (length > count) {
                /* Split packet if packet extends past range */
                httpPutBackPacket(q, httpSplitPacket(packet, count));
            }
            if (tx->rangeBoundary) {
                httpPutPacketToNext(q, createRangePacket(stream, range));
            }
            tx->rangePos += count;
            if (tx->rangePos >= range->end) {
                tx->currentRange = range->next;
            }
            break;
        }
    }
    return packet;
}


/*
    Create a range boundary packet
 */
static HttpPacket *createRangePacket(HttpStream *stream, HttpRange *range)
{
    HttpPacket  *packet;
    HttpTx      *tx;
    char        *length;

    tx = stream->tx;

    length = (tx->entityLength >= 0) ? itos(tx->entityLength) : "*";
    packet = httpCreatePacket(HTTP_RANGE_BUFSIZE);
    packet->flags |= HTTP_PACKET_RANGE | HTTP_PACKET_DATA;
    mprPutToBuf(packet->content,
        "\r\n--%s\r\n"
        "Content-Range: bytes %lld-%lld/%s\r\n\r\n",
        tx->rangeBoundary, range->start, range->end - 1, length);
    return packet;
}


/*
    Create a final range packet that follows all the data
 */
static HttpPacket *createFinalRangePacket(HttpStream *stream)
{
    HttpPacket  *packet;
    HttpTx      *tx;

    tx = stream->tx;

    packet = httpCreatePacket(HTTP_RANGE_BUFSIZE);
    packet->flags |= HTTP_PACKET_RANGE | HTTP_PACKET_DATA;
    mprPutToBuf(packet->content, "\r\n--%s--\r\n", tx->rangeBoundary);
    return packet;
}


/*
    Create a range boundary. This is required if more than one range is requested.
 */
static void createRangeBoundary(HttpStream *stream)
{
    HttpTx      *tx;
    int         when;

    tx = stream->tx;
    assert(tx->rangeBoundary == 0);
    when = (int) stream->http->now;
    tx->rangeBoundary = sfmt("%08X%08X", PTOI(tx) + PTOI(stream) * when, when);
}


/*
    Ensure all the range limits are within the entity size limits. Fixup negative ranges.
 */
static int fixRangeLength(HttpStream *stream, HttpQueue *q)
{
    HttpTx      *tx;
    HttpRange   *range;
    MprOff      length;
    cchar       *value;

    tx = stream->tx;
    length = tx->entityLength ? tx->entityLength : tx->length;
    if (length <= 0) {
        if ((value = mprLookupKey(tx->headers, "Content-Length")) != 0) {
            length = stoi(value);
        }
        if (length < 0 && tx->chunkSize < 0) {
            if (q->last->flags & HTTP_PACKET_END) {
                if (q->count > 0) {
                    length = q->count;
                }
            }
        }
        if (length < 0) {
            return 0;
        }
    }
    for (range = tx->outputRanges; range; range = range->next) {
        /*
                Range: 0-49             first 50 bytes
                Range: 50-99,200-249    Two 50 byte ranges from 50 and 200
                Range: -50              Last 50 bytes
                Range: 1-               Skip first byte then emit the rest
         */
        if (length) {
            if (range->end > length) {
                range->end = length;
            }
            if (range->start > length) {
                httpBadRequestError(stream, HTTP_CLOSE | HTTP_CODE_RANGE_NOT_SATISFIABLE, "Bad content range");
                return MPR_ERR_CANT_COMPLETE;
            }
        }
        if (range->start < 0) {
            if (length <= 0) {
                /*
                    Cannot compute an offset from the end as we don't know the entity length and it is not
                    always possible or wise to buffer all the output.
                 */
                return 0;
            }
            /* select last -range-end bytes */
            range->start = length - range->end + 1;
            range->end = length;
        }
        if (range->end < 0) {
            if (length <= 0) {
                return 0;
            }
            range->end = length - range->end - 1;
        }
        range->len = (int) (range->end - range->start);
    }
    return 1;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/route.c ************/

/*
    route.c -- Http request routing

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/


#include    "pcre.h"

/********************************** Forwards **********************************/

#undef  GRADUATE_LIST
#define GRADUATE_LIST(route, field) \
    if (route->field == 0) { \
        route->field = mprCreateList(-1, 0); \
    } else if (route->parent && route->field == route->parent->field) { \
        route->field = mprCloneList(route->parent->field); \
    }

#undef  GRADUATE_HASH
#define GRADUATE_HASH(route, field) \
    if (!route->field || (route->parent && route->field == route->parent->field)) { \
        route->field = mprCloneHash(route->parent->field); \
    }

/********************************** Forwards **********************************/

static void addUniqueItem(MprList *list, HttpRouteOp *op);
static int checkRoute(HttpStream *stream, HttpRoute *route);
static HttpLang *createLangDef(cchar *path, cchar *suffix, int flags);
static HttpRouteOp *createRouteOp(cchar *name, int flags);
static void definePathVars(HttpRoute *route);
static void defineHostVars(HttpRoute *route);
static char *expandTokens(HttpStream *stream, cchar *path);
static char *expandPatternTokens(cchar *str, cchar *replacement, int *matches, int matchCount);
static char *expandRequestTokens(HttpStream *stream, char *targetKey);
static void finalizePattern(HttpRoute *route);
static char *finalizeReplacement(HttpRoute *route, cchar *str);
static char *finalizeTemplate(HttpRoute *route);
static bool opPresent(MprList *list, HttpRouteOp *op);
static void manageRoute(HttpRoute *route, int flags);
static void manageLang(HttpLang *lang, int flags);
static void manageRouteOp(HttpRouteOp *op, int flags);
static int matchRequestUri(HttpStream *stream, HttpRoute *route);
static int matchRoute(HttpStream *stream, HttpRoute *route);
static int selectHandler(HttpStream *stream, HttpRoute *route);
static int testCondition(HttpStream *stream, HttpRoute *route, HttpRouteOp *condition);
static char *trimQuotes(char *str);
static int updateRequest(HttpStream *stream, HttpRoute *route, HttpRouteOp *update);

/************************************ Code ************************************/
/*
    Host may be null
 */
PUBLIC HttpRoute *httpCreateRoute(HttpHost *host)
{
    Http        *http;
    HttpRoute   *route;

    http = HTTP;
    if ((route = mprAllocObj(HttpRoute, manageRoute)) == 0) {
        return 0;
    }
    route->auth = httpCreateAuth();
    route->defaultLanguage = sclone("en");
    route->home = route->documents = mprGetCurrentPath();
    route->flags = HTTP_ROUTE_STEALTH;
    route->flags |= HTTP_ROUTE_ENV_ESCAPE;
    route->envPrefix = sclone("CGI_");
    route->host = host;
    route->http = HTTP;
    route->lifespan = ME_MAX_CACHE_DURATION;
    route->pattern = MPR->emptyString;
    route->targetRule = sclone("run");
    route->autoDelete = 1;
    route->autoFinalize = 1;
    route->prefix = MPR->emptyString;
    route->trace = http->trace;

#if DEPRECATED
    route->workers = -1;
#endif

    route->headers = mprCreateList(-1, MPR_LIST_STABLE);
    route->handlers = mprCreateList(-1, MPR_LIST_STABLE);
    route->indexes = mprCreateList(-1, MPR_LIST_STABLE);
    route->inputStages = mprCreateList(-1, MPR_LIST_STABLE);
    route->outputStages = mprCreateList(-1, MPR_LIST_STABLE);

    route->extensions = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS | MPR_HASH_STABLE);
    route->errorDocuments = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_STABLE);
    route->methods = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_STATIC_VALUES | MPR_HASH_STABLE);
    route->vars = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS | MPR_HASH_STABLE);

    httpAddRouteMethods(route, NULL);
    httpAddRouteFilter(route, http->rangeFilter->name, NULL, HTTP_STAGE_TX);

    /*
        Standard headers for all routes. These should not break typical content
        Users then vary via header directives
     */
    httpAddRouteResponseHeader(route, HTTP_ROUTE_ADD_HEADER, "Vary", "Accept-Encoding");
    httpAddRouteResponseHeader(route, HTTP_ROUTE_ADD_HEADER, "X-XSS-Protection", "1; mode=block");
    httpAddRouteResponseHeader(route, HTTP_ROUTE_ADD_HEADER, "X-Frame-Options", "SAMEORIGIN");
    httpAddRouteResponseHeader(route, HTTP_ROUTE_ADD_HEADER, "X-Content-Type-Options", "nosniff");

    if (MPR->httpService) {
        route->limits = mprMemdup(http->serverLimits ? http->serverLimits : http->clientLimits, sizeof(HttpLimits));
    }
    route->mimeTypes = MPR->mimeTypes;
    definePathVars(route);
    return route;
}


/*
    Create a new location block. Inherit from the parent. We use a copy-on-write scheme if these are modified later.
 */
PUBLIC HttpRoute *httpCreateInheritedRoute(HttpRoute *parent)
{
    HttpRoute  *route;

    if (!parent && (parent = httpGetDefaultRoute(0)) == 0) {
        return 0;
    }
    if ((route = mprAllocObj(HttpRoute, manageRoute)) == 0) {
        return 0;
    }
    route->auth = httpCreateInheritedAuth(parent->auth);
    route->autoDelete = parent->autoDelete;
    route->autoFinalize = parent->autoFinalize;
    route->caching = parent->caching;
    route->canonical = parent->canonical;
    route->charSet = parent->charSet;
    route->clientConfig = parent->clientConfig;
    route->conditions = parent->conditions;
    route->config = parent->config;
    route->connector = parent->connector;
    route->cookie = parent->cookie;
    route->corsAge = parent->corsAge;
    route->corsCredentials = parent->corsCredentials;
    route->corsHeaders = parent->corsHeaders;
    route->corsMethods = parent->corsMethods;
    route->corsOrigin = parent->corsOrigin;
    route->data = parent->data;
    route->database = parent->database;
    route->defaultLanguage = parent->defaultLanguage;
    route->documents = parent->documents;
    route->envPrefix = parent->envPrefix;
    route->eroute = parent->eroute;
    route->errorDocuments = parent->errorDocuments;
    route->extensions = parent->extensions;
    route->flags = parent->flags & ~(HTTP_ROUTE_FREE_PATTERN);
    route->handler = parent->handler;
    route->handlers = parent->handlers;
    route->headers = parent->headers;
    route->home = parent->home;
    route->host = parent->host;
    route->http = HTTP;
    route->indexes = parent->indexes;
    route->inputStages = parent->inputStages;
    route->json = parent->json;
    route->languages = parent->languages;
    route->lifespan = parent->lifespan;
    route->limits = parent->limits;
    route->map = parent->map;
    route->methods = parent->methods;
    route->mimeTypes = parent->mimeTypes;
    route->mode = parent->mode;
    route->optimizedPattern = parent->optimizedPattern;
    route->outputStages = parent->outputStages;
    route->params = parent->params;
    route->parent = parent;
    route->pattern = parent->pattern;
    route->patternCompiled = parent->patternCompiled;
    route->prefix = parent->prefix;
    route->prefixLen = parent->prefixLen;
    route->renameUploads = parent->renameUploads;
    route->requestHeaders = parent->requestHeaders;
    route->responseFormat = parent->responseFormat;
    route->responseStatus = parent->responseStatus;
    route->sourceName = parent->sourceName;
    route->ssl = parent->ssl;
    route->target = parent->target;
    route->targetRule = parent->targetRule;
    route->tokens = parent->tokens;
    route->trace = parent->trace;
    route->updates = parent->updates;
    route->vars = parent->vars;

#if DEPRECATED
    route->script = parent->script;
    route->scriptPath = parent->scriptPath;
    route->workers = parent->workers;
#endif
    return route;
}


static void manageRoute(HttpRoute *route, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(route->auth);
        mprMark(route->caching);
        mprMark(route->canonical);
        mprMark(route->charSet);
        mprMark(route->clientConfig);
        mprMark(route->conditions);
        mprMark(route->config);
        mprMark(route->connector);
        mprMark(route->context);
        mprMark(route->cookie);
        mprMark(route->corsHeaders);
        mprMark(route->corsMethods);
        mprMark(route->corsOrigin);
        mprMark(route->data);
        mprMark(route->database);
        mprMark(route->defaultLanguage);
        mprMark(route->documents);
        mprMark(route->envPrefix);
        mprMark(route->eroute);
        mprMark(route->errorDocuments);
        mprMark(route->extensions);
        mprMark(route->handler);
        mprMark(route->handlers);
        mprMark(route->headers);
        mprMark(route->home);
        mprMark(route->host);
        mprMark(route->http);
        mprMark(route->indexes);
        mprMark(route->inputStages);
        mprMark(route->languages);
        mprMark(route->limits);
        mprMark(route->map);
        mprMark(route->methods);
        mprMark(route->mimeTypes);
        mprMark(route->mode);
        mprMark(route->optimizedPattern);
        mprMark(route->outputStages);
        mprMark(route->params);
        mprMark(route->parent);
        mprMark(route->pattern);
        mprMark(route->prefix);
        mprMark(route->requestHeaders);
        mprMark(route->responseFormat);
        mprMark(route->source);
        mprMark(route->sourceName);
        mprMark(route->ssl);
        mprMark(route->startSegment);
        mprMark(route->startWith);
        mprMark(route->target);
        mprMark(route->targetRule);
        mprMark(route->tokens);
        mprMark(route->trace);
        mprMark(route->tplate);
        mprMark(route->updates);
        mprMark(route->vars);
        mprMark(route->webSocketsProtocol);
#if DEPRECATED
        mprMark(route->script);
        mprMark(route->scriptPath);
#endif
    } else if (flags & MPR_MANAGE_FREE) {
        if (route->patternCompiled && (route->flags & HTTP_ROUTE_FREE_PATTERN)) {
            free(route->patternCompiled);
        }
    }
}


PUBLIC HttpRoute *httpCreateDefaultRoute(HttpHost *host)
{
    HttpRoute   *route;

    assert(host);
    if ((route = httpCreateRoute(host)) == 0) {
        return 0;
    }
    httpFinalizeRoute(route);
    return route;
}


/*
    Create and configure a basic route. This is used for client side routes. Host may be null.
 */
PUBLIC HttpRoute *httpCreateConfiguredRoute(HttpHost *host, int serverSide)
{
    HttpRoute   *route;
    Http        *http;

    /*
        Create default incoming and outgoing pipelines. Order matters.
     */
    route = httpCreateRoute(host);
    http = route->http;
#if ME_HTTP_WEB_SOCKETS
    httpAddRouteFilter(route, http->webSocketFilter->name, NULL, HTTP_STAGE_RX | HTTP_STAGE_TX);
#endif
    return route;
}


PUBLIC HttpRoute *httpCreateAliasRoute(HttpRoute *parent, cchar *pattern, cchar *path, int status)
{
    HttpRoute   *route;

    assert(parent);
    assert(pattern && *pattern);

    if ((route = httpCreateInheritedRoute(parent)) == 0) {
        return 0;
    }
    httpSetRoutePattern(route, pattern, 0);
    if (path) {
        httpSetRouteDocuments(route, path);
    }
    route->responseStatus = status;
    return route;
}


/*
    This routine binds a new route to a URI. It creates a handler, route and binds a callback to that route.
 */
PUBLIC HttpRoute *httpCreateActionRoute(HttpRoute *parent, cchar *pattern, HttpAction action)
{
    HttpRoute   *route;
    cchar       *name;

    if (!pattern || !action) {
        return 0;
    }
    if ((route = httpCreateInheritedRoute(parent)) != 0) {
        route->handler = route->http->actionHandler;
        httpSetRoutePattern(route, pattern, 0);
        name = strim(pattern, "^$", 0);
        httpDefineAction(name, action);
        httpFinalizeRoute(route);
    }
    return route;
}


PUBLIC int httpStartRoute(HttpRoute *route)
{
#if !ME_ROM
    if (!(route->flags & HTTP_ROUTE_STARTED)) {
        route->flags |= HTTP_ROUTE_STARTED;
        if (route->trace != route->trace->parent) {
            httpOpenTraceLogFile(route->trace);
        }
    }
#endif
    return 0;
}


PUBLIC void httpStopRoute(HttpRoute *route)
{
}


/*
    Find the matching route and handler for a request. If any errors occur, the pass handler is used to
    pass errors via the net connector onto the client. This process may rewrite the request
    URI and may redirect the request.
 */
PUBLIC void httpRouteRequest(HttpStream *stream)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpRoute   *route;
    int         next, rewrites, match;

    rx = stream->rx;
    tx = stream->tx;
    route = 0;
    rewrites = 0;

    if (stream->error) {
        tx->handler = stream->http->passHandler;
        if (!stream->host) {
            stream->host = mprGetFirstItem(stream->net->endpoint->hosts);
        }
        route = rx->route = stream->host->defaultRoute;
    } else {
        for (next = rewrites = 0; rewrites < ME_MAX_REWRITE; ) {
            if (next >= stream->host->routes->length) {
                break;
            }
            route = stream->host->routes->items[next++];
            if (route->startSegment && strncmp(rx->pathInfo, route->startSegment, route->startSegmentLen) != 0) {
                /* Failed to match the first URI segment, skip to the next group */
                if (next < route->nextGroup) {
                    next = route->nextGroup;
                }

            } else if (route->startWith && strncmp(rx->pathInfo, route->startWith, route->startWithLen) != 0) {
                /* Failed to match starting literal segment of the route pattern, advance to test the next route */
                continue;

            } else if ((match = matchRoute(stream, route)) == HTTP_ROUTE_REROUTE) {
                next = 0;
                route = 0;
                rewrites++;

            } else if (match == HTTP_ROUTE_OK) {
                break;
            }
        }
    }
    if (route == 0 || tx->handler == 0) {
        rx->route = stream->host->defaultRoute;
        httpError(stream, HTTP_CODE_BAD_METHOD, "Cannot find suitable route for request method");
        return;
    }
    rx->route = route;
    stream->limits = route->limits;
    stream->trace = route->trace;

    if (rewrites >= ME_MAX_REWRITE) {
        httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Too many request rewrites");
    }
    if (tx->finalized || tx->pendingFinalize) {
        /* Pass handler can transmit the message */
        tx->handler = stream->http->passHandler;
    } else {
        assert(!stream->error);
    }
    if (tx->handler->module) {
        tx->handler->module->lastActivity = stream->lastActivity;
    }
}


static int matchRoute(HttpStream *stream, HttpRoute *route)
{
    HttpRx      *rx;
    cchar       *savePathInfo, *pathInfo;
    int         rc;

    assert(stream);
    assert(route);

    rx = stream->rx;
    savePathInfo = 0;

    assert(route->prefix);
    if (route->prefix && *route->prefix) {
        if (!sstarts(rx->pathInfo, route->prefix)) {
            return HTTP_ROUTE_REJECT;
        }
        savePathInfo = rx->pathInfo;
        pathInfo = &rx->pathInfo[route->prefixLen];
        if (*pathInfo == '\0') {
            pathInfo = "/";
        }
        rx->pathInfo = sclone(pathInfo);
        rx->scriptName = route->prefix;
    }
    if ((rc = matchRequestUri(stream, route)) == HTTP_ROUTE_OK) {
        rc = checkRoute(stream, route);
    }
    if (rc == HTTP_ROUTE_REJECT && savePathInfo) {
        /* Keep the modified pathInfo if OK or REWRITE */
        rx->pathInfo = savePathInfo;
        rx->scriptName = 0;
    }
    return rc;
}


static int matchRequestUri(HttpStream *stream, HttpRoute *route)
{
    HttpRx      *rx;

    assert(stream);
    assert(route);
    rx = stream->rx;

    if (route->patternCompiled) {
        rx->matchCount = pcre_exec(route->patternCompiled, NULL, rx->pathInfo, (int) slen(rx->pathInfo), 0, 0,
            rx->matches, sizeof(rx->matches) / sizeof(int));
        if (route->flags & HTTP_ROUTE_NOT) {
            if (rx->matchCount > 0) {
                return HTTP_ROUTE_REJECT;
            }
            rx->matchCount = 1;
            rx->matches[0] = 0;
            rx->matches[1] = (int) slen(rx->pathInfo);

        } else if (rx->matchCount <= 0) {
            return HTTP_ROUTE_REJECT;
        }
    } else if (route->pattern && *route->pattern) {
        /* Pattern compilation failed */
        return HTTP_ROUTE_REJECT;
    }
    if (!mprLookupKey(route->methods, rx->method)) {
        if (!mprLookupKey(route->methods, "*")) {
            if (!(rx->flags & HTTP_HEAD && mprLookupKey(route->methods, "GET"))) {
                return HTTP_ROUTE_REJECT;
            }
        }
    }
    rx->route = route;
    return HTTP_ROUTE_OK;
}


static int checkRoute(HttpStream *stream, HttpRoute *route)
{
    HttpRouteOp     *op, *condition, *update;
    HttpRouteProc   *proc;
    HttpRx          *rx;
    HttpTx          *tx;
    cchar           *token, *value, *header, *field;
    int             next, rc, matched[ME_MAX_ROUTE_MATCHES * 2], count, result;

    assert(stream);
    assert(route);
    rx = stream->rx;
    tx = stream->tx;
    assert(rx->pathInfo[0]);

    rx->target = route->target ? expandTokens(stream, route->target) : sclone(&rx->pathInfo[1]);

    if (route->requestHeaders) {
        for (next = 0; (op = mprGetNextItem(route->requestHeaders, &next)) != 0; ) {
            if ((header = httpGetHeader(stream, op->name)) != 0) {
                count = pcre_exec(op->mdata, NULL, header, (int) slen(header), 0, 0, matched, sizeof(matched) / sizeof(int));
                result = count > 0;
                if (op->flags & HTTP_ROUTE_NOT) {
                    result = !result;
                }
                if (!result) {
                    return HTTP_ROUTE_REJECT;
                }
            }
        }
    }
    if (route->params) {
        for (next = 0; (op = mprGetNextItem(route->params, &next)) != 0; ) {
            if ((field = httpGetParam(stream, op->name, "")) != 0) {
                count = pcre_exec(op->mdata, NULL, field, (int) slen(field), 0, 0, matched, sizeof(matched) / sizeof(int));
                result = count > 0;
                if (op->flags & HTTP_ROUTE_NOT) {
                    result = !result;
                }
                if (!result) {
                    return HTTP_ROUTE_REJECT;
                }
            }
        }
    }
    if (route->conditions) {
        for (next = 0; (condition = mprGetNextItem(route->conditions, &next)) != 0; ) {
            rc = testCondition(stream, route, condition);
            if (rc == HTTP_ROUTE_REROUTE) {
                return rc;
            }
            if (condition->flags & HTTP_ROUTE_NOT) {
                rc = !rc;
            }
            if (rc == HTTP_ROUTE_REJECT) {
                return rc;
            }
        }
    }
    if (route->updates) {
        for (next = 0; (update = mprGetNextItem(route->updates, &next)) != 0; ) {
            if ((rc = updateRequest(stream, route, update)) == HTTP_ROUTE_REROUTE) {
                return rc;
            }
        }
    }
    if (route->prefix[0]) {
        httpSetParam(stream, "prefix", route->prefix);
    }
    if ((rc = selectHandler(stream, route)) != HTTP_ROUTE_OK) {
        return rc;
    }
    if (route->tokens) {
        for (next = 0; (token = mprGetNextItem(route->tokens, &next)) != 0; ) {
            int index = rx->matches[next * 2];
            if (index >= 0) {
                value = snclone(&rx->pathInfo[index], rx->matches[(next * 2) + 1] - index);
                httpSetParam(stream, token, value);
            }
        }
    }
    if ((proc = mprLookupKey(stream->http->routeTargets, route->targetRule)) == 0) {
        httpError(stream, -1, "Cannot find route target rule \"%s\"", route->targetRule);
        return HTTP_ROUTE_REJECT;
    }
    if ((rc = (*proc)(stream, route, 0)) != HTTP_ROUTE_OK) {
        return rc;
    }
    if (tx->finalized || tx->pendingFinalize) {
        tx->handler = stream->http->passHandler;
    } else if (tx->handler->rewrite) {
        rc = tx->handler->rewrite(stream);
    }
    return rc;
}


static int selectHandler(HttpStream *stream, HttpRoute *route)
{
    HttpRx      *rx;
    HttpTx      *tx;
    int         next, rc;

    assert(stream);
    assert(route);

    rx = stream->rx;
    tx = stream->tx;
    if (route->handler) {
        tx->handler = route->handler;
        return HTTP_ROUTE_OK;
    }
    for (next = 0; (tx->handler = mprGetNextStableItem(route->handlers, &next)) != 0; ) {
        rc = tx->handler->match(stream, route, 0);
        if (rc == HTTP_ROUTE_OK || rc == HTTP_ROUTE_REROUTE) {
            return rc;
        }
    }
    if (!tx->handler) {
        /*
            Now match by extensions
         */
        if (!tx->ext || (tx->handler = mprLookupKey(route->extensions, tx->ext)) == 0) {
            tx->handler = mprLookupKey(route->extensions, "");
        }
    }
    if (rx->flags & HTTP_TRACE) {
        /*
            Trace method always processed for all requests by the passHandler
         */
        tx->handler = stream->http->passHandler;
    }
    if (tx->finalized) {
        tx->handler = stream->http->passHandler;
    }
    return tx->handler ? HTTP_ROUTE_OK : HTTP_ROUTE_REJECT;
}


PUBLIC void httpSetHandler(HttpStream *stream, HttpStage *handler)
{
    stream->tx->handler = handler;
}


PUBLIC cchar *httpMapContent(HttpStream *stream, cchar *filename)
{
    HttpRoute   *route;
    HttpRx      *rx;
    HttpTx      *tx;
    MprKey      *kp;
    MprList     *extensions;
    MprPath     info;
    bool        acceptGzip, zipped;
    cchar       *ext, *path;
    int         next;

    tx = stream->tx;
    rx = stream->rx;
    route = rx->route;

    if (route->map && !(tx->flags & HTTP_TX_NO_MAP)) {
        if ((kp = mprLookupKeyEntry(route->map, httpGetPathExt(filename))) == 0) {
            kp = mprLookupKeyEntry(route->map, "");
        }
        if (kp) {
            extensions = (MprList*) kp->data;
            acceptGzip = scontains(rx->acceptEncoding, "gzip") != 0;
            for (ITERATE_ITEMS(extensions, ext, next)) {
                zipped = sends(ext, "gz") != 0;
                if (zipped && !acceptGzip) {
                    continue;
                }
                if (kp->key[0]) {
                    path = mprReplacePathExt(filename, ext);
                } else {
                    path = sjoin(filename, ext, NULL);
                }
                if (mprGetPathInfo(path, &info) == 0) {
                    httpLog(stream->trace, "rx.route.map", "context", "originalFilename:%s, filename:%s", filename, path);
                    filename = path;
                    if (zipped) {
                        httpSetHeader(stream, "Content-Encoding", "gzip");
                    }
                    tx->fileInfo = info;
                    break;
                }
            }
        }
    }
    return filename;
}


PUBLIC void httpMapFile(HttpStream *stream)
{
    HttpTx      *tx;
    HttpLang    *lang;
    cchar       *filename;

    tx = stream->tx;
    if (tx->filename) {
        return;
    }
    filename = stream->rx->target;
    lang = stream->rx->lang;
    if (lang && lang->path) {
        filename = mprJoinPath(lang->path, filename);
    }
    /*
        Cannot use JoinPath as it will normalize the filename and remove trailing "/"
        filename = mprJoinPath(stream->rx->route->documents, filename);
     */
    if (!mprIsPathAbs(filename)) {
        filename = sjoin(stream->rx->route->documents, "/", filename, NULL);
    }

    /*
        Sets tx->fileInfo
        Set header for zipped
     */
    filename = httpMapContent(stream, filename);
    /*
        Sets tx->filename, tx->ext, tx->info, tx->tag
     */
    httpSetFilename(stream, filename, 0);
}


/************************************ API *************************************/

PUBLIC int httpAddRouteCondition(HttpRoute *route, cchar *name, cchar *details, int flags)
{
    HttpRouteOp *op;
    cchar       *errMsg;
    char        *pattern, *value;
    int         column;

    assert(route);

    GRADUATE_LIST(route, conditions);
    if ((op = createRouteOp(name, flags)) == 0) {
        return MPR_ERR_MEMORY;
    }
    if (scaselessmatch(name, "auth") || scaselessmatch(name, "unauthorized")) {
        /* Nothing to do. Route->auth has it all */

    } else if (scaselessmatch(name, "missing")) {
        op->details = finalizeReplacement(route, "${request:filename}");

    } else if (scaselessmatch(name, "directory")) {
        op->details = finalizeReplacement(route, details);

    } else if (scaselessmatch(name, "exists")) {
        op->details = finalizeReplacement(route, details);

    } else if (scaselessmatch(name, "match")) {
        /*
            Condition match string pattern
            String can contain matching ${tokens} from the route->pattern and can contain request ${tokens}
         */
        if (!httpTokenize(route, details, "%S %S", &value, &pattern)) {
            return MPR_ERR_BAD_SYNTAX;
        }
        if ((op->mdata = pcre_compile2(pattern, 0, 0, &errMsg, &column, NULL)) == 0) {
            mprLog("error http route", 0, "Cannot compile condition match pattern. Error %s at column %d", errMsg, column);
            return MPR_ERR_BAD_SYNTAX;
        }
        op->details = finalizeReplacement(route, value);
        op->flags |= HTTP_ROUTE_FREE;

    } else if (scaselessmatch(name, "secure")) {
        if (!details || *details == '\0') {
            mprLog("error http config", 0, "Secure route condition is missing a redirect target in route \"%s\"",
                route->pattern);
        }
        op->details = finalizeReplacement(route, details);
    }
    addUniqueItem(route->conditions, op);
    return 0;
}


PUBLIC int httpAddRouteFilter(HttpRoute *route, cchar *name, cchar *extensions, int direction)
{
    HttpStage   *stage;
    HttpStage   *filter;
    char        *extlist, *word, *tok;
    int         next;

    assert(route);

    if (smatch(name, "chunkFilter")) {
        /* Already pre-loaded */
        return 0;
    }
    for (ITERATE_ITEMS(route->outputStages, stage, next)) {
        if (smatch(stage->name, name)) {
            mprLog("warn http route", 0, "Stage \"%s\" is already configured for the route \"%s\". Ignoring.",
                name, route->pattern);
            return 0;
        }
    }
    if ((stage = httpLookupStage(name)) == 0) {
        mprLog("error http route", 0, "Cannot find filter %s", name);
        return MPR_ERR_CANT_FIND;
    }
    /*
        Clone an existing stage because each filter stores its own set of extensions to match against
     */
    filter = httpCloneStage(stage);

    if (extensions && *extensions) {
        filter->extensions = mprCreateHash(0, MPR_HASH_CASELESS | MPR_HASH_STABLE);
        extlist = sclone(extensions);
        word = stok(extlist, " \t\r\n", &tok);
        while (word) {
            if (*word == '*' && word[1] == '.') {
                word += 2;
            } else if (*word == '.') {
                word++;
            } else if (*word == '\"' && word[1] == '\"') {
                word = "";
            } else if (*word == '*' && word[1] == '\0') {
                word = "";
            }
            mprAddKey(filter->extensions, word, filter);
            word = stok(NULL, " \t\r\n", &tok);
        }
    }
    if (direction & HTTP_STAGE_RX && filter->incoming) {
        GRADUATE_LIST(route, inputStages);
        mprAddItem(route->inputStages, filter);
    }
    if (direction & HTTP_STAGE_TX && filter->outgoing) {
        GRADUATE_LIST(route, outputStages);
        mprAddItem(route->outputStages, filter);
    }
    return 0;
}


PUBLIC int httpAddRouteHandler(HttpRoute *route, cchar *name, cchar *extensions)
{
    HttpStage   *handler;
    char        *extlist, *word, *tok;

    assert(route);

    if ((handler = httpLookupStage(name)) == 0) {
        return MPR_ERR_CANT_FIND;
    }
    if (route->handler) {
        mprLog("error http route", 0, "Cannot add handler \"%s\" to route \"%s\" once SetHandler used.",
            handler->name, route->pattern);
    }
    if (!extensions && !handler->match) {
        mprLog("info http route", 2, "Adding handler \"%s\" without extensions to match", handler->name);
    }
    if (extensions) {
        /*
            Add to the handler extension hash. Skip over "*." and "."
         */
        GRADUATE_HASH(route, extensions);
        extlist = sclone(extensions);
        if ((word = stok(extlist, " \t\r\n", &tok)) == 0) {
            mprAddKey(route->extensions, "", handler);
        } else {
            while (word) {
                if (*word == '*' && word[1] == '\0') {
                    word++;
                } else if (*word == '*' && word[1] == '.') {
                    word += 2;
                } else if (*word == '.') {
                    word++;
                } else if (*word == '\"' && word[1] == '\"') {
                    word = "";
                }
                mprAddKey(route->extensions, word, handler);
                word = stok(NULL, " \t\r\n", &tok);
            }
        }
    }
    if (handler->match && mprLookupItem(route->handlers, handler) < 0) {
        GRADUATE_LIST(route, handlers);
        if (smatch(name, "cacheHandler")) {
            mprInsertItemAtPos(route->handlers, 0, handler);
        } else {
            mprAddItem(route->handlers, handler);
        }
    }
    return 0;
}


PUBLIC void httpAddRouteMapping(HttpRoute *route, cchar *extensions, cchar *mappings)
{
    MprList     *mapList;
    cchar       *map;
    char        *etok, *ext, *mtok;
    ssize       len;

    if (extensions == 0) {
        return;
    }
    if (*extensions == '[') {
        extensions = strim(extensions, "[]", 0);
    }
    if (smatch(extensions, "*") || *extensions == '\0') {
        extensions = ".";
    }
    if (!route->map) {
        route->map = mprCreateHash(ME_MAX_ROUTE_MAP_HASH, MPR_HASH_STABLE);
    }
    for (ext = stok(sclone(extensions), ", \t", &etok); ext; ext = stok(NULL, ", \t", &etok)) {
        if (*ext == '.' || *ext == '"' || *ext == '*') {
            ext++;
        }
        len = slen(ext);
        if (ext[len - 1] == '"') {
            ext[len - 1] = '\0';
        }
        mapList = mprCreateList(0, MPR_LIST_STABLE);
        for (map = stok(sclone(mappings), ", \t", &mtok); map; map = stok(NULL, ", \t", &mtok)) {
            mprAddItem(mapList, sreplace(map, "${1}", ext));
        }
        mprAddKey(route->map, ext, mapList);
    }
}


/*
    Param field valuePattern
 */
PUBLIC void httpAddRouteParam(HttpRoute *route, cchar *field, cchar *value, int flags)
{
    HttpRouteOp     *op;
    cchar           *errMsg;
    int             column;

    assert(route);
    assert(field && *field);
    assert(value && *value);

    GRADUATE_LIST(route, params);
    if ((op = createRouteOp(field, flags | HTTP_ROUTE_FREE)) == 0) {
        return;
    }
    if ((op->mdata = pcre_compile2(value, 0, 0, &errMsg, &column, NULL)) == 0) {
        mprLog("error http route", 0, "Cannot compile field pattern. Error %s at column %d", errMsg, column);
    } else {
        op->flags |= HTTP_ROUTE_FREE;
        mprAddItem(route->params, op);
    }
}


/*
    RequestHeader [!] header pattern
 */
PUBLIC void httpAddRouteRequestHeaderCheck(HttpRoute *route, cchar *header, cchar *pattern, int flags)
{
    HttpRouteOp     *op;
    cchar           *errMsg;
    int             column;

    assert(route);
    assert(header && *header);
    assert(pattern && *pattern);

    GRADUATE_LIST(route, requestHeaders);
    if ((op = createRouteOp(header, flags | HTTP_ROUTE_FREE)) == 0) {
        return;
    }
    if ((op->mdata = pcre_compile2(pattern, 0, 0, &errMsg, &column, NULL)) == 0) {
        mprLog("error http route", 0, "Cannot compile header pattern. Error %s at column %d", errMsg, column);
    } else {
        op->flags |= HTTP_ROUTE_FREE;
        mprAddItem(route->requestHeaders, op);
    }
}


/*
    Header [add|append|remove|set] header [value]
 */
PUBLIC void httpAddRouteResponseHeader(HttpRoute *route, int cmd, cchar *header, cchar *value)
{
    MprKeyValue     *pair;
    int             next;

    assert(route);
    assert(header && *header);

    GRADUATE_LIST(route, headers);
    if (cmd == HTTP_ROUTE_REMOVE_HEADER) {
        /*
            Remove existing route headers, but keep the remove record so that user headers will be removed too
         */
        for (ITERATE_ITEMS(route->headers, pair, next)) {
            if (smatch(pair->key, header)) {
                mprRemoveItem(route->headers, pair);
                next--;
            }
        }
    }
    mprAddItem(route->headers, mprCreateKeyPair(header, value, cmd));
}


/*
    Add a route update record. These run to modify a request.
        Update rule var value
        rule == "cmd|param"
        details == "var value"
    Value can contain pattern and request tokens.
 */
PUBLIC int httpAddRouteUpdate(HttpRoute *route, cchar *rule, cchar *details, int flags)
{
    HttpRouteOp *op;
    char        *value;

    assert(route);
    assert(rule && *rule);

    GRADUATE_LIST(route, updates);
    if ((op = createRouteOp(rule, flags)) == 0) {
        return MPR_ERR_MEMORY;
    }
    if (scaselessmatch(rule, "cmd")) {
        op->details = sclone(details);

    } else if (scaselessmatch(rule, "lang")) {
        /* Nothing to do */;

    } else if (scaselessmatch(rule, "param")) {
        if (!httpTokenize(route, details, "%S %S", &op->var, &value)) {
            return MPR_ERR_BAD_SYNTAX;
        }
        op->value = finalizeReplacement(route, value);

    } else {
        return MPR_ERR_BAD_SYNTAX;
    }
    addUniqueItem(route->updates, op);
    return 0;
}


PUBLIC void httpClearRouteStages(HttpRoute *route, int direction)
{
    assert(route);

    if (direction & HTTP_STAGE_RX) {
        route->inputStages = mprCreateList(-1, MPR_LIST_STABLE);
    }
    if (direction & HTTP_STAGE_TX) {
        route->outputStages = mprCreateList(-1, MPR_LIST_STABLE);
    }
}


PUBLIC void httpDefineRouteTarget(cchar *key, HttpRouteProc *proc)
{
    assert(key && *key);
    assert(proc);

    mprAddKey(HTTP->routeTargets, key, proc);
}


PUBLIC void httpDefineRouteCondition(cchar *key, HttpRouteProc *proc)
{
    assert(key && *key);
    assert(proc);

    mprAddKey(HTTP->routeConditions, key, proc);
}


PUBLIC void httpDefineRouteUpdate(cchar *key, HttpRouteProc *proc)
{
    assert(key && *key);
    assert(proc);

    mprAddKey(HTTP->routeUpdates, key, proc);
}


PUBLIC void *httpGetRouteData(HttpRoute *route, cchar *key)
{
    assert(route);
    assert(key && *key);

    if (!route->data) {
        return 0;
    }
    return mprLookupKey(route->data, key);
}


PUBLIC cchar *httpGetRouteDocuments(HttpRoute *route)
{
    assert(route);
    return route->documents;
}


PUBLIC cchar *httpGetRouteHome(HttpRoute *route)
{
    assert(route);
    return route->home;
}


PUBLIC cchar *httpGetRouteMethods(HttpRoute *route)
{
    assert(route);
    assert(route->methods);
    return mprHashKeysToString(route->methods, ",");
}


PUBLIC void httpResetRoutePipeline(HttpRoute *route)
{
    assert(route);

    if (!route->parent || route->caching != route->parent->caching) {
        route->caching = 0;
    }
    if (!route->parent || route->errorDocuments != route->parent->errorDocuments) {
        route->errorDocuments = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_STABLE);
    }
    if (!route->parent || route->extensions != route->parent->extensions) {
        route->extensions = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS | MPR_HASH_STABLE);
    }
    if (!route->parent || route->handlers != route->parent->handlers) {
        route->handlers = mprCreateList(-1, MPR_LIST_STABLE);
    }
    if (!route->parent || route->inputStages != route->parent->inputStages) {
        route->inputStages = mprCreateList(-1, MPR_LIST_STABLE);
    }
    if (!route->parent || route->indexes != route->parent->indexes) {
        route->indexes = mprCreateList(-1, MPR_LIST_STABLE);
    }
    if (!route->parent || route->outputStages != route->parent->outputStages) {
        route->outputStages = mprCreateList(-1, MPR_LIST_STABLE);
    }
    if (!route->parent || route->methods != route->parent->methods) {
        route->methods = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_STATIC_VALUES | MPR_HASH_STABLE);
        httpAddRouteMethods(route, NULL);
    }
    if (!route->parent || route->requestHeaders != route->parent->requestHeaders) {
        route->requestHeaders = 0;
    }
    if (!route->parent || route->params != route->parent->params) {
        route->params = 0;
    }
    if (!route->parent || route->updates != route->parent->updates) {
        route->updates = 0;
    }
    if (!route->parent || route->conditions != route->parent->conditions) {
        route->conditions = 0;
    }
    if (!route->parent || route->map != route->parent->map) {
        route->map = 0;
    }
    if (!route->parent || route->languages != route->parent->languages) {
        route->languages = 0;
    }
    if (!route->parent || route->headers != route->parent->headers) {
        route->headers = 0;
#if FUTURE
        httpAddRouteResponseHeader(route, HTTP_ROUTE_ADD_HEADER, "Content-Security-Policy", "default-src 'self'");
#endif
        httpAddRouteResponseHeader(route, HTTP_ROUTE_ADD_HEADER, "X-XSS-Protection", "1; mode=block");
        httpAddRouteResponseHeader(route, HTTP_ROUTE_ADD_HEADER, "X-Frame-Options", "SAMEORIGIN");
        httpAddRouteResponseHeader(route, HTTP_ROUTE_ADD_HEADER, "X-Content-Type-Options", "nosniff");
    }
}


PUBLIC void httpResetHandlers(HttpRoute *route)
{
    assert(route);
    route->handlers = mprCreateList(-1, MPR_LIST_STABLE);
}


PUBLIC void httpSetRouteAuth(HttpRoute *route, HttpAuth *auth)
{
    assert(route);
    route->auth = auth;
}


PUBLIC void httpSetRouteAutoDelete(HttpRoute *route, bool enable)
{
    assert(route);
    route->autoDelete = enable;
}


PUBLIC void httpSetRouteAutoFinalize(HttpRoute *route, bool enable)
{
    assert(route);
    route->autoFinalize = enable;
}


PUBLIC void httpSetRouteRenameUploads(HttpRoute *route, bool enable)
{
    assert(route);
    route->renameUploads = enable;
}


PUBLIC int httpSetRouteConnector(HttpRoute *route, cchar *name)
{
    HttpStage     *stage;

    assert(route);

    stage = httpLookupStage(name);
    if (stage == 0) {
        mprLog("error http route", 0, "Cannot find connector %s", name);
        return MPR_ERR_CANT_FIND;
    }
    route->connector = stage;
    return 0;
}


PUBLIC void httpSetRouteData(HttpRoute *route, cchar *key, void *data)
{
    assert(route);
    assert(key && *key);
    assert(data);

    if (route->data == 0) {
        route->data = mprCreateHash(-1, 0);
    } else {
        GRADUATE_HASH(route, data);
    }
    mprAddKey(route->data, key, data);
}


PUBLIC void httpSetRouteDocuments(HttpRoute *route, cchar *path)
{
    httpSetDir(route, "DOCUMENTS", path);
}


PUBLIC void httpSetRouteFlags(HttpRoute *route, int flags)
{
    assert(route);
    route->flags = flags;
}


PUBLIC void httpSetRouteEnvEscape(HttpRoute *route, bool on)
{
    route->flags &= ~(HTTP_ROUTE_ENV_ESCAPE);
    if (on) {
        route->flags |= HTTP_ROUTE_ENV_ESCAPE;
    }
}


PUBLIC void httpSetRouteEnvPrefix(HttpRoute *route, cchar *prefix)
{
    route->envPrefix = sclone(prefix);
}


PUBLIC int httpSetRouteHandler(HttpRoute *route, cchar *name)
{
    HttpStage     *handler;

    assert(route);
    assert(name && *name);

    if ((handler = httpLookupStage(name)) == 0) {
        mprLog("error http route", 0, "Cannot find handler %s", name);
        return MPR_ERR_CANT_FIND;
    }
    route->handler = handler;
    return 0;
}


PUBLIC void httpSetRouteHome(HttpRoute *route, cchar *path)
{
    httpSetDir(route, "HOME", path);
}


/*
    WARNING: internal API only.
 */
PUBLIC void httpSetRouteHost(HttpRoute *route, HttpHost *host)
{
    assert(route);
    assert(host);

    route->host = host;
    defineHostVars(route);
}


PUBLIC void httpSetRouteIgnoreEncodingErrors(HttpRoute *route, bool on)
{
    route->ignoreEncodingErrors = on;
}


PUBLIC void httpAddRouteIndex(HttpRoute *route, cchar *index)
{
    cchar   *item;
    int     next;

    assert(route);
    assert(index && *index);

    GRADUATE_LIST(route, indexes);
    for (ITERATE_ITEMS(route->indexes, item, next)) {
        if (smatch(index, item)) {
            return;
        }
    }
    mprAddItem(route->indexes, sclone(index));
}


PUBLIC void httpAddRouteMethods(HttpRoute *route, cchar *methods)
{
    char    *method, *tok;

    assert(route);

    if (methods == NULL || *methods == '\0') {
        methods = ME_HTTP_DEFAULT_METHODS;
    } else if (scaselessmatch(methods, "ALL")) {
       methods = "*";
    } else if (*methods == '[') {
        methods = strim(methods, "[]", 0);
    }
    if (!route->methods || (route->parent && route->methods == route->parent->methods)) {
        GRADUATE_HASH(route, methods);
    }
    tok = sclone(methods);
    while ((method = stok(tok, ", \t\n\r", &tok)) != 0) {
        mprAddKey(route->methods, method, LTOP(1));
    }
}


PUBLIC void httpRemoveRouteMethods(HttpRoute *route, cchar *methods)
{
    char    *method, *tok;

    assert(route);
    tok = sclone(methods);
    while ((method = stok(tok, ", \t\n\r", &tok)) != 0) {
        mprRemoveKey(route->methods, method);
    }
}


PUBLIC void httpResetRouteIndexes(HttpRoute *route)
{
    route->indexes = mprCreateList(-1, MPR_LIST_STABLE);
}


PUBLIC void httpSetRouteMethods(HttpRoute *route, cchar *methods)
{
    route->methods = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_STATIC_VALUES | MPR_HASH_STABLE);
    httpAddRouteMethods(route, methods);
}


PUBLIC void httpSetRouteCookie(HttpRoute *route, cchar *cookie)
{
    assert(route);
    assert(cookie && *cookie);
    route->cookie = sclone(cookie);
}


PUBLIC void httpSetRouteCookiePersist(HttpRoute *route, int enable)
{
    route->flags &= ~HTTP_ROUTE_PERSIST_COOKIE;
    if (enable) {
        route->flags |= HTTP_ROUTE_PERSIST_COOKIE;
    }
}


PUBLIC void httpSetRouteCookieSame(HttpRoute *route, cchar *value)
{
    route->flags &= ~(HTTP_ROUTE_NONE_COOKIE | HTTP_ROUTE_LAX_COOKIE | HTTP_ROUTE_STRICT_COOKIE);
    if (smatch(value, "none")) {
        route->flags |= HTTP_ROUTE_NONE_COOKIE;
    } else if (smatch(value, "lax")) {
        route->flags |= HTTP_ROUTE_LAX_COOKIE;
    } else if (smatch(value, "strict")) {
        route->flags |= HTTP_ROUTE_STRICT_COOKIE;
    }
}


PUBLIC void httpSetRoutePattern(HttpRoute *route, cchar *pattern, int flags)
{
    assert(route);
    assert(pattern);

    route->flags |= (flags & HTTP_ROUTE_NOT);
    route->pattern = sclone(pattern);
    finalizePattern(route);
}


/*
    Set the prefix to empty if no prefix
 */
PUBLIC void httpSetRoutePrefix(HttpRoute *route, cchar *prefix)
{
    assert(route);

    if (prefix && *prefix) {
        if (smatch(prefix, "/")) {
            route->prefix = MPR->emptyString;
            route->prefixLen = 0;
        } else {
            route->prefix = sclone(prefix);
            route->prefixLen = slen(prefix);
            httpSetRouteVar(route, "PREFIX", prefix);
        }
    } else {
        route->prefix = MPR->emptyString;
        route->prefixLen = 0;
        httpSetRouteVar(route, "PREFIX", "");
    }
    if (route->pattern) {
        finalizePattern(route);
    }
    assert(route->prefix);
}


PUBLIC void httpSetRoutePreserveFrames(HttpRoute *route, bool on)
{
    route->flags &= ~HTTP_ROUTE_PRESERVE_FRAMES;
    if (on) {
        route->flags |= HTTP_ROUTE_PRESERVE_FRAMES;
    }
}


PUBLIC void httpSetRouteSessionVisibility(HttpRoute *route, bool visible)
{
    route->flags &= ~HTTP_ROUTE_VISIBLE_SESSION;
    if (visible) {
        route->flags |= HTTP_ROUTE_VISIBLE_SESSION;
    }
}


PUBLIC void httpSetRouteShowErrors(HttpRoute *route, bool on)
{
    route->flags &= ~HTTP_ROUTE_SHOW_ERRORS;
    if (on) {
        route->flags |= HTTP_ROUTE_SHOW_ERRORS;
    }
}


PUBLIC void httpSetRouteSource(HttpRoute *route, cchar *source)
{
    assert(route);
    assert(source);

    /* Source can be empty */
    route->sourceName = sclone(source);
}


#if DEPRECATED
PUBLIC void httpSetRouteScript(HttpRoute *route, cchar *script, cchar *scriptPath)
{
    assert(route);

    if (script) {
        assert(*script);
        route->script = sclone(script);
    }
    if (scriptPath) {
        assert(*scriptPath);
        route->scriptPath = sclone(scriptPath);
    }
}
#endif


PUBLIC void httpSetRouteStealth(HttpRoute *route, bool on)
{
    route->flags &= ~HTTP_ROUTE_STEALTH;
    if (on) {
        route->flags |= HTTP_ROUTE_STEALTH;
    }
}


/*
    Target names are extensible and hashed in http->routeTargets.

        Target close
        Target redirect status [URI]
        Target run ${DOCUMENTS}/${request:uri}.gz
        Target run ${controller}-${action}
        Target write [-r] status "Hello World\r\n"
 */
PUBLIC int httpSetRouteTarget(HttpRoute *route, cchar *rule, cchar *details)
{
    char    *redirect, *msg;

    assert(route);
    assert(rule && *rule);

    route->targetRule = sclone(rule);
    route->target = sclone(details);

    if (scaselessmatch(rule, "close")) {
        route->target = sclone(details);

    } else if (scaselessmatch(rule, "redirect")) {
        if (!httpTokenize(route, details, "%N ?S", &route->responseStatus, &redirect)) {
            return MPR_ERR_BAD_SYNTAX;
        }
        route->target = finalizeReplacement(route, redirect);

    } else if (scaselessmatch(rule, "run")) {
        route->target = finalizeReplacement(route, details);

    } else if (scaselessmatch(rule, "write")) {
        /*
            Write [-r] status Message
         */
        if (sncmp(details, "-r", 2) == 0) {
            route->flags |= HTTP_ROUTE_RAW;
            details = &details[2];
        }
        if (!httpTokenize(route, details, "%N %S", &route->responseStatus, &msg)) {
            return MPR_ERR_BAD_SYNTAX;
        }
        route->target = finalizeReplacement(route, msg);

    } else {
        return MPR_ERR_BAD_SYNTAX;
    }
    return 0;
}


PUBLIC void httpSetRouteTemplate(HttpRoute *route, cchar *tplate)
{
    assert(route);
    assert(tplate && *tplate);

    route->tplate = sclone(tplate);
}


PUBLIC void httpSetRouteUploadDir(HttpRoute *route, cchar *dir)
{
    httpSetDir(route, "UPLOAD", dir);
}


#if DEPRECATED
PUBLIC void httpSetRouteWorkers(HttpRoute *route, int workers)
{
    assert(route);
    route->workers = workers;
}
#endif


PUBLIC void httpAddRouteErrorDocument(HttpRoute *route, int status, cchar *url)
{
    char    *code;

    assert(route);
    GRADUATE_HASH(route, errorDocuments);
    code = itos(status);
    mprAddKey(route->errorDocuments, code, sclone(url));
}


PUBLIC cchar *httpLookupRouteErrorDocument(HttpRoute *route, int code)
{
    char   *num;

    assert(route);
    if (route->errorDocuments == 0) {
        return 0;
    }
    num = itos(code);
    return (cchar*) mprLookupKey(route->errorDocuments, num);
}


/*
    Finalize the pattern.
        - Change "\{n[:m]}" to "{n[:m]}"
        - Change "\~" to "~"
        - Change "(~ PAT ~)" to "(?: PAT )?"
        - Extract the tokens and change tokens: "{word}" to "([^/]*)"
 */
static void finalizePattern(HttpRoute *route)
{
    MprBuf      *pattern;
    cchar       *errMsg;
    char        *startPattern, *cp, *ep, *token, *field;
    ssize       len;
    int         column;

    assert(route);
    route->tokens = mprCreateList(-1, MPR_LIST_STABLE);
    pattern = mprCreateBuf(-1, -1);
    startPattern = route->pattern[0] == '^' ? &route->pattern[1] : route->pattern;

    if (route->tplate == 0) {
        /* Do this while the prefix is still in the route pattern */
        route->tplate = finalizeTemplate(route);
    }
    /*
        Create an simple literal startWith string to optimize route rejection.
     */
    len = strcspn(startPattern, "^$*+?.(|{[\\");
    if (len) {
        /* Handle /pattern / * */
        if (startPattern[len] == '*' && len > 0) {
            len--;
        }
        route->startWith = snclone(startPattern, len);
        route->startWithLen = len;
        if ((cp = strchr(&route->startWith[1], '/')) != 0) {
            route->startSegment = snclone(route->startWith, cp - route->startWith);
        } else {
            route->startSegment = route->startWith;
        }
        route->startSegmentLen = slen(route->startSegment);
    } else {
        /* Pattern has special characters */
        route->startWith = 0;
        route->startWithLen = 0;
        route->startSegmentLen = 0;
        route->startSegment = 0;
    }

    /*
        Remove the route prefix from the start of the compiled pattern.
     */
    if (route->prefix && *route->prefix && sstarts(startPattern, route->prefix)) {
        assert(route->prefixLen <= route->startWithLen);
        startPattern = sfmt("^%s", &startPattern[route->prefixLen]);
    } else {
        startPattern = sjoin("^", startPattern, NULL);
    }
    for (cp = startPattern; *cp; cp++) {
        /* Alias for optional, non-capturing pattern:  "(?: PAT )?" */
        if (*cp == '(' && cp[1] == '~') {
            mprPutStringToBuf(pattern, "(?:");
            cp++;

        } else if (*cp == '(') {
            mprPutCharToBuf(pattern, *cp);
        } else if (*cp == '~' && cp[1] == ')') {
            mprPutStringToBuf(pattern, ")?");
            cp++;

        } else if (*cp == ')') {
            mprPutCharToBuf(pattern, *cp);

        } else if (*cp == '{') {
            if (cp > startPattern&& cp[-1] == '\\') {
                mprAdjustBufEnd(pattern, -1);
                mprPutCharToBuf(pattern, *cp);
            } else {
                if ((ep = schr(cp, '}')) != 0) {
                    /* Trim {} off the token and replace in pattern with "([^/]*)"  */
                    token = snclone(&cp[1], ep - cp - 1);
                    if ((field = schr(token, '=')) != 0) {
                        *field++ = '\0';
                        field = sfmt("(%s)", field);
                    } else {
                        field = "([^/]*)";
                    }
                    mprPutStringToBuf(pattern, field);
                    mprAddItem(route->tokens, token);
                    /* Params ends up looking like "$1:$2:$3:$4" */
                    cp = ep;
                }
            }
        } else if (*cp == '\\' && *cp == '~') {
            mprPutCharToBuf(pattern, *++cp);

        } else {
            mprPutCharToBuf(pattern, *cp);
        }
    }
    mprAddNullToBuf(pattern);
    route->optimizedPattern = sclone(mprGetBufStart(pattern));
    if (mprGetListLength(route->tokens) == 0) {
        route->tokens = 0;
    }
    if (route->patternCompiled && (route->flags & HTTP_ROUTE_FREE_PATTERN)) {
        free(route->patternCompiled);
    }
    if ((route->patternCompiled = pcre_compile2(route->optimizedPattern, 0, 0, &errMsg, &column, NULL)) == 0) {
        mprLog("error http route", 0, "Cannot compile route. Error %s at column %d", errMsg, column);
    }
    route->flags |= HTTP_ROUTE_FREE_PATTERN;
}


static char *finalizeReplacement(HttpRoute *route, cchar *str)
{
    MprBuf      *buf;
    cchar       *item;
    cchar       *tok, *cp, *ep, *token;
    int         next, braced;

    assert(route);

    /*
        Prepare a replacement string. Change $token to $N
     */
    buf = mprCreateBuf(-1, -1);
    if (str && *str) {
        for (cp = str; *cp; cp++) {
            if ((tok = schr(cp, '$')) != 0 && (tok == str || tok[-1] != '\\')) {
                if (tok > cp) {
                    mprPutBlockToBuf(buf, cp, tok - cp);
                }
                if ((braced = (*++tok == '{')) != 0) {
                    tok++;
                }
                if (*tok == '&' || *tok == '\'' || *tok == '`' || *tok == '$') {
                    mprPutCharToBuf(buf, '$');
                    mprPutCharToBuf(buf, *tok);
                    ep = tok + 1;
                } else {
                    if (braced) {
                        for (ep = tok; *ep && *ep != '}'; ep++) ;
                    } else {
                        for (ep = tok; *ep && isdigit((uchar) *ep); ep++) ;
                    }
                    token = snclone(tok, ep - tok);
                    if (schr(token, ':') || schr(token, '.')) {
                        /* Double quote to get through two levels of expansion in writeTarget */
                        mprPutStringToBuf(buf, "$${");
                        mprPutStringToBuf(buf, token);
                        mprPutCharToBuf(buf, '}');
                    } else {
                        for (next = 0; (item = mprGetNextItem(route->tokens, &next)) != 0; ) {
                            if (scmp(item, token) == 0) {
                                break;
                            }
                        }
                        /*  Insert "$" in front of "{token}" */
                        if (item) {
                            mprPutCharToBuf(buf, '$');
                            mprPutIntToBuf(buf, next);
                        } else if (snumber(token)) {
                            mprPutCharToBuf(buf, '$');
                            mprPutStringToBuf(buf, token);
                        } else {
                            mprLog("error http route", 0, "Cannot find token \"%s\" in template \"%s\"",
                                token, route->pattern);
                        }
                    }
                }
                if (braced) {
                    ep++;
                }
                cp = ep - 1;

            } else {
                if (*cp == '\\') {
                    if (cp[1] == 'r') {
                        mprPutCharToBuf(buf, '\r');
                        cp++;
                    } else if (cp[1] == 'n') {
                        mprPutCharToBuf(buf, '\n');
                        cp++;
                    } else {
                        mprPutCharToBuf(buf, *cp);
                    }
                } else {
                    mprPutCharToBuf(buf, *cp);
                }
            }
        }
    }
    mprAddNullToBuf(buf);
    return sclone(mprGetBufStart(buf));
}


/*
    Convert a route pattern into a usable template to construct URI links
    NOTE: this is heuristic and not perfect. Users can define the template via the httpSetTemplate API or in appweb via the
    EspURITemplate configuration directive.
 */
static char *finalizeTemplate(HttpRoute *route)
{
    MprBuf  *buf;
    char    *sp, *tplate;

    if ((buf = mprCreateBuf(0, 0)) == 0) {
        return 0;
    }
    /*
        Note: the route->pattern includes the prefix
     */
    for (sp = route->pattern; *sp; sp++) {
        switch (*sp) {
        default:
            mprPutCharToBuf(buf, *sp);
            break;
        case '$':
            if (sp[1]) {
                mprPutCharToBuf(buf, *sp);
            }
            break;
        case '^':
            if (sp > route->pattern) {
                mprPutCharToBuf(buf, *sp);
            }
            break;
        case '+':
        case '?':
        case '|':
        case '[':
        case ']':
        case '*':
        case '.':
            break;
        case '(':
            if (sp[1] == '~') {
                sp++;
            }
            break;
        case '~':
            if (sp[1] == ')') {
                sp++;
            } else {
                mprPutCharToBuf(buf, *sp);
            }
            break;
        case ')':
            break;
        case '\\':
            if (sp[1] == '\\') {
                mprPutCharToBuf(buf, *sp++);
            } else {
                mprPutCharToBuf(buf, *++sp);
            }
            break;
        case '{':
            mprPutCharToBuf(buf, '$');
            while (sp[1] && *sp != '}') {
                if (*sp == '=') {
                    while (sp[1] && *sp != '}') sp++;
                } else {
                    mprPutCharToBuf(buf, *sp++);
                }
            }
            mprPutCharToBuf(buf, '}');
            break;
        }
    }
    if (mprLookAtLastCharInBuf(buf) == '/') {
        mprAdjustBufEnd(buf, -1);
    }
    mprAddNullToBuf(buf);
    if (mprGetBufLength(buf) > 0) {
        tplate = sclone(mprGetBufStart(buf));
    } else {
        tplate = sclone("/");
    }
    return tplate;
}


PUBLIC void httpFinalizeRoute(HttpRoute *route)
{
    /*
        Add the route to the owning host. When using an Appweb configuration file, the order of route finalization
        will be from the inside out. This ensures that nested routes are defined BEFORE outer/enclosing routes.
        This is important as requests process routes in-order.
     */
    assert(route);
    if (mprGetListLength(route->indexes) == 0) {
        mprAddItem(route->indexes,  sclone("index.html"));
    }
    httpAddRoute(route->host, route);
}


PUBLIC cchar *httpGetRouteTop(HttpStream *stream)
{
    HttpRx  *rx;
    cchar   *pp, *top;
    int     count;

    rx = stream->rx;
    if (sstarts(rx->pathInfo, rx->route->prefix)) {
        pp = &rx->pathInfo[rx->route->prefixLen];
    } else {
        pp = rx->pathInfo;
    }
    top = MPR->emptyString;
    for (count = 0; *pp; pp++) {
        if (*pp == '/' && count++ > 0) {
            top = sjoin(top, "../", NULL);
        }
    }
    return top;
}


/*
    Expect a template with embedded tokens of the form: "/${controller}/${action}/${other}"
    Understands the following aliases:
        ~   For ${PREFIX}
    The options is a hash of token values.
 */
PUBLIC char *httpTemplate(HttpStream *stream, cchar *template, MprHash *options)
{
    MprBuf      *buf;
    HttpRx      *rx;
    HttpRoute   *route;
    cchar       *cp, *ep, *value;
    char        key[ME_BUFSIZE];

    rx = stream->rx;
    route = rx->route;
    if (template == 0 || *template == '\0') {
        return MPR->emptyString;
    }
    buf = mprCreateBuf(-1, -1);
    for (cp = template; *cp; cp++) {
        if (cp == template && *cp == '~') {
            mprPutStringToBuf(buf, httpGetRouteTop(stream));

        } else if (*cp == '$' && cp[1] == '{' && (cp == template || cp[-1] != '\\')) {
            cp += 2;
            if ((ep = strchr(cp, '}')) != 0) {
                sncopy(key, sizeof(key), cp, ep - cp);
                if (options && (value = httpGetOption(options, key, 0)) != 0) {
                    mprPutStringToBuf(buf, value);

                } else if ((value = mprReadJson(rx->params, key)) != 0) {
                    mprPutStringToBuf(buf, value);

                } else if ((value = mprLookupKey(route->vars, key)) != 0) {
                    mprPutStringToBuf(buf, value);
                }
                if (value == 0) {
                    /* Just emit the token name if the token cannot be found */
                    mprPutStringToBuf(buf, key);
                }
                cp = ep;
            }
        } else {
            mprPutCharToBuf(buf, *cp);
        }
    }
    mprAddNullToBuf(buf);
    return sclone(mprGetBufStart(buf));
}


PUBLIC void httpSetRouteVar(HttpRoute *route, cchar *key, cchar *value)
{
    assert(route);
    assert(key);

    GRADUATE_HASH(route, vars);
    if (schr(value, '$')) {
        value = stemplate(value, route->vars);
    }
    mprAddKey(route->vars, key, sclone(value));
}


PUBLIC cchar *httpGetRouteVar(HttpRoute *route, cchar *key)
{
    return mprLookupKey(route->vars, key);
}


PUBLIC cchar *httpExpandRouteVars(HttpRoute *route, cchar *str)
{
    return stemplate(str, route->vars);
}


/*
    Make a path name. This replaces $references, converts to an absolute path name, cleans the path and maps delimiters.
    Paths are resolved relative to the given directory or route home if "dir" is null.
 */
PUBLIC char *httpMakePath(HttpRoute *route, cchar *dir, cchar *path)
{
    assert(route);
    assert(path);

    if ((path = stemplate(path, route->vars)) == 0) {
        return 0;
    }
    if (mprIsPathRel(path)) {
        path = mprJoinPath(dir ? dir : route->home, path);
    }
    return mprGetAbsPath(path);
}


PUBLIC void httpSetRouteXsrf(HttpRoute *route, bool enable)
{
    route->flags &= ~HTTP_ROUTE_XSRF;
    if (enable) {
        route->flags |= HTTP_ROUTE_XSRF;
    }
}

/********************************* Language ***********************************/
/*
    Language can be an empty string
 */
PUBLIC int httpAddRouteLanguageSuffix(HttpRoute *route, cchar *language, cchar *suffix, int flags)
{
    HttpLang    *lp;

    assert(route);
    assert(language);
    assert(suffix && *suffix);

    if (route->languages == 0) {
        route->languages = mprCreateHash(-1, MPR_HASH_STABLE);
    } else {
        GRADUATE_HASH(route, languages);
    }
    if ((lp = mprLookupKey(route->languages, language)) != 0) {
        lp->suffix = sclone(suffix);
        lp->flags = flags;
    } else {
        mprAddKey(route->languages, language, createLangDef(0, suffix, flags));
    }
    return httpAddRouteUpdate(route, "lang", 0, 0);
}


PUBLIC int httpAddRouteLanguageDir(HttpRoute *route, cchar *language, cchar *path)
{
    HttpLang    *lp;

    assert(route);
    assert(language && *language);
    assert(path && *path);

    if (route->languages == 0) {
        route->languages = mprCreateHash(-1, MPR_HASH_STABLE);
    } else {
        GRADUATE_HASH(route, languages);
    }
    if ((lp = mprLookupKey(route->languages, language)) != 0) {
        lp->path = sclone(path);
    } else {
        mprAddKey(route->languages, language, createLangDef(path, 0, 0));
    }
    return httpAddRouteUpdate(route, "lang", 0, 0);
}


PUBLIC void httpSetRouteDefaultLanguage(HttpRoute *route, cchar *language)
{
    assert(route);
    assert(language && *language);

    route->defaultLanguage = sclone(language);
}


/********************************* Conditions *********************************/

static int testCondition(HttpStream *stream, HttpRoute *route, HttpRouteOp *condition)
{
    HttpRouteProc   *proc;

    assert(stream);
    assert(route);
    assert(condition);

    if ((proc = mprLookupKey(stream->http->routeConditions, condition->name)) == 0) {
        httpError(stream, -1, "Cannot find route condition rule %s", condition->name);
        return 0;
    }
    return (*proc)(stream, route, condition);
}


/*
    Allow/Deny authorization based on network IP
 */
static int allowDenyCondition(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    HttpRx      *rx;
    HttpAuth    *auth;
    int         allow, deny;

    assert(stream);
    assert(route);

    rx = stream->rx;
    auth = rx->route->auth;
    if (auth == 0) {
        return HTTP_ROUTE_OK;
    }
    allow = 0;
    deny = 0;
    if (auth->flags & HTTP_ALLOW_DENY) {
        if (auth->allow && mprLookupKey(auth->allow, stream->ip)) {
            allow++;
        } else {
            allow++;
        }
        if (auth->deny && mprLookupKey(auth->deny, stream->ip)) {
            deny++;
        }
        if (!allow || deny) {
            httpError(stream, HTTP_CODE_FORBIDDEN, "Access denied for this server %s", stream->ip);
            return HTTP_ROUTE_OK;
        }
    } else {
        if (auth->deny && mprLookupKey(auth->deny, stream->ip)) {
            deny++;
        }
        if (auth->allow && !mprLookupKey(auth->allow, stream->ip)) {
            deny = 0;
            allow++;
        } else {
            allow++;
        }
        if (deny || !allow) {
            httpError(stream, HTTP_CODE_FORBIDDEN, "Access denied for this server %s", stream->ip);
            return HTTP_ROUTE_OK;
        }
    }
    return HTTP_ROUTE_OK;
}


/*
    This condition is used to implement all user authentication for routes
    It accesses form body parameters for login.
 */
static int authCondition(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    HttpAuth    *auth;
    cchar       *username, *password;

    assert(stream);
    assert(route);

    auth = route->auth;
    if (!auth || !auth->type || !(auth->type->flags & HTTP_AUTH_TYPE_CONDITION)) {
        /* Authentication not required or not using authCondition */
        return HTTP_ROUTE_OK;
    }
    if (!httpIsAuthenticated(stream)) {
        if (!httpGetCredentials(stream, &username, &password) || !httpLogin(stream, username, password)) {
            if (!stream->tx->finalized) {
                if (auth && auth->type) {
                    (auth->type->askLogin)(stream);
                } else {
                    httpError(stream, HTTP_CODE_UNAUTHORIZED, "Access Denied. Login required");
                }
            }
            /*
                Request has been denied and a response generated. So OK to accept this route.
             */
            return HTTP_ROUTE_OK;
        }
    }
    if (!httpCanUser(stream, NULL)) {
        httpLog(stream->trace, "rx.auth.check", "error", "msg:Access denied. User is not authorized for access.");
        if (!stream->tx->finalized) {
            httpError(stream, HTTP_CODE_FORBIDDEN, "Access denied. User is not authorized for access.");
            /* Request has been denied and a response generated. So OK to accept this route. */
        }
    }
    /*
        OK to accept route. This does not mean the request was authenticated - an error may have been already generated
     */
    return HTTP_ROUTE_OK;
}


/*
    This condition is used for "Condition unauthorized"
    It accesses form body parameters for login.
 */
static int unauthorizedCondition(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    HttpAuth    *auth;
    cchar       *username, *password;

    auth = route->auth;
    if (!auth || !auth->type || !(auth->type->flags & HTTP_AUTH_TYPE_CONDITION)) {
        return HTTP_ROUTE_REJECT;
    }
    if (httpIsAuthenticated(stream)) {
        return HTTP_ROUTE_REJECT;
    }
    if (httpGetCredentials(stream, &username, &password) && httpLogin(stream, username, password)) {
        return HTTP_ROUTE_REJECT;
    }
    return HTTP_ROUTE_OK;
}


/*
    Test if the condition parameters evaluate to a directory
 */
static int directoryCondition(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    HttpTx      *tx;
    MprPath     info;
    cchar       *path, *saveExt, *saveFilename;

    assert(stream);
    assert(route);
    assert(op);
    tx = stream->tx;
    saveExt = tx->ext;
    saveFilename = tx->filename;

    httpMapFile(stream);
    path = mprJoinPath(route->documents, expandTokens(stream, op->details));
    tx->ext = saveExt;
    tx->filename = saveFilename;

    mprGetPathInfo(path, &info);
    if (info.isDir) {
        return HTTP_ROUTE_OK;
    }
    return HTTP_ROUTE_REJECT;
}


/*
    Test if a file exists
 */
static int existsCondition(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    HttpTx  *tx;
    cchar   *path, *saveExt, *saveFilename;

    assert(stream);
    assert(route);
    assert(op);

    tx = stream->tx;
    saveExt = tx->ext;
    saveFilename = tx->filename;

    httpMapFile(stream);
    path = mprJoinPath(route->documents, expandTokens(stream, op->details));
    tx->ext = saveExt;
    tx->filename = saveFilename;

    if (mprPathExists(path, R_OK)) {
        return HTTP_ROUTE_OK;
    }
    return HTTP_ROUTE_REJECT;
}


/*
    Test if a condition matches by regular expression
 */
static int matchCondition(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    char    *str;
    int     matched[ME_MAX_ROUTE_MATCHES * 2], count;

    assert(stream);
    assert(route);
    assert(op);

    str = expandTokens(stream, op->details);
    count = pcre_exec(op->mdata, NULL, str, (int) slen(str), 0, 0, matched, sizeof(matched) / sizeof(int));
    if (count > 0) {
        return HTTP_ROUTE_OK;
    }
    return HTTP_ROUTE_REJECT;
}


/*
    Test if the connection is secure.
    Set op->details to a non-zero "age" to emit a Strict-Transport-Security header
    A negative age signifies to "includeSubDomains"
 */
static int secureCondition(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    int64       age;

    assert(stream);
    if (op->flags & HTTP_ROUTE_STRICT_TLS) {
        /* Negative age means subDomains == true */
        age = stoi(op->details);
        if (age < 0) {
            httpAddHeader(stream, "Strict-Transport-Security", "max-age=%lld; includeSubDomains", -age / TPS);
        } else if (age > 0) {
            httpAddHeader(stream, "Strict-Transport-Security", "max-age=%lld", age / TPS);
        }
    }
    if (op->flags & HTTP_ROUTE_REDIRECT) {
        if (!stream->secure) {
            assert(op->details && *op->details);
            httpRedirect(stream, HTTP_CODE_MOVED_PERMANENTLY, expandTokens(stream, op->details));
        }
        return HTTP_ROUTE_OK;
    }
    if (!stream->secure) {
        return HTTP_ROUTE_REJECT;
    }
    return HTTP_ROUTE_OK;
}

/********************************* Updates ******************************/

static int updateRequest(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    HttpRouteProc   *proc;

    assert(stream);
    assert(route);
    assert(op);

    if ((proc = mprLookupKey(stream->http->routeUpdates, op->name)) == 0) {
        httpError(stream, -1, "Cannot find route update rule %s", op->name);
        return HTTP_ROUTE_OK;
    }
    return (*proc)(stream, route, op);
}


static int cmdUpdate(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    MprCmd  *cmd;
    char    *command, *out, *err;
    int     status;

    assert(stream);
    assert(route);
    assert(op);

    command = expandTokens(stream, op->details);
    cmd = mprCreateCmd(stream->dispatcher);
    httpLog(stream->trace, "route.run", "context", "command:%s", command);
    if ((status = mprRunCmd(cmd, command, NULL, NULL, &out, &err, -1, 0)) != 0) {
        /* Don't call httpError, just set errorMsg which can be retrieved via: ${request:error} */
        stream->errorMsg = sfmt("Command failed: %s\nStatus: %d\n%s\n%s", command, status, out, err);
        httpLog(stream->trace, "route.run", "error", "command:%s, error:%s", command, stream->errorMsg);
        /* Continue */
    }
    mprDestroyCmd(cmd);
    return HTTP_ROUTE_OK;
}


static int paramUpdate(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    assert(stream);
    assert(route);
    assert(op);

    httpSetParam(stream, op->var, expandTokens(stream, op->value));
    return HTTP_ROUTE_OK;
}


static int langUpdate(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    HttpUri     *prior;
    HttpRx      *rx;
    HttpLang    *lang;
    char        *ext, *pathInfo, *uri;

    assert(stream);
    assert(route);

    rx = stream->rx;
    prior = rx->parsedUri;
    assert(route->languages);

    if ((lang = httpGetLanguage(stream, route->languages, 0)) != 0) {
        rx->lang = lang;
        if (lang->suffix) {
            pathInfo = 0;
            if (lang->flags & HTTP_LANG_AFTER) {
                pathInfo = sjoin(rx->pathInfo, ".", lang->suffix, NULL);
            } else if (lang->flags & HTTP_LANG_BEFORE) {
                ext = httpGetExt(stream);
                if (ext && *ext) {
                    pathInfo = sjoin(mprJoinPathExt(mprTrimPathExt(rx->pathInfo), lang->suffix), ".", ext, NULL);
                } else {
                    pathInfo = mprJoinPathExt(mprTrimPathExt(rx->pathInfo), lang->suffix);
                }
            }
            if (pathInfo) {
                uri = httpFormatUri(prior->scheme, prior->host, prior->port, pathInfo, prior->reference, prior->query, 0);
                httpSetUri(stream, uri);
            }
        }
    }
    return HTTP_ROUTE_OK;
}


/*********************************** Targets **********************************/

static int closeTarget(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    assert(stream);
    assert(route);

    httpError(stream, HTTP_CODE_RESET | HTTP_ABORT, "Route target \"close\" is closing request");
    return HTTP_ROUTE_OK;
}


static int redirectTarget(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    cchar       *target;

    assert(stream);
    assert(route);
    assert(route->target);

    target = expandTokens(stream, route->target);
    httpRedirect(stream, route->responseStatus ? route->responseStatus : HTTP_CODE_MOVED_TEMPORARILY, target);
    return HTTP_ROUTE_OK;
}


static int runTarget(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    stream->rx->target = route->target ? expandTokens(stream, route->target) : sclone(&stream->rx->pathInfo[1]);
    return HTTP_ROUTE_OK;
}


static int writeTarget(HttpStream *stream, HttpRoute *route, HttpRouteOp *op)
{
    char    *str;

    assert(stream);
    assert(route);

    /*
        Need to re-compute output string as updates may have run to define params which affect the route->target tokens
     */
    str = route->target ? expandTokens(stream, route->target) : sclone(&stream->rx->pathInfo[1]);
    if (!(route->flags & HTTP_ROUTE_RAW)) {
        str = mprEscapeHtml(str);
    }
    httpSetStatus(stream, route->responseStatus);
    httpFormatResponse(stream, "%s", str);
    httpFinalize(stream);
    return HTTP_ROUTE_OK;
}


/************************************************** Route Convenience ****************************************************/

PUBLIC HttpRoute *httpDefineRoute(HttpRoute *parent, cchar *methods, cchar *pattern, cchar *target, cchar *source)
{
    HttpRoute   *route;

    if ((route = httpCreateInheritedRoute(parent)) == 0) {
        return 0;
    }
    httpSetRoutePattern(route, pattern, 0);
    if (methods) {
        httpSetRouteMethods(route, methods);
    }
    if (source) {
        httpSetRouteSource(route, source);
    }
    httpSetRouteTarget(route, "run", target);
    httpFinalizeRoute(route);
    return route;
}


PUBLIC HttpRoute *httpAddRestfulRoute(HttpRoute *parent, cchar *methods, cchar *pattern, cchar *target, cchar *resource)
{
    cchar   *source;

    if (*resource == '{') {
        pattern = sfmt("^%s/%s%s", parent->prefix, resource, pattern);
    } else {
        pattern = sfmt("^%s/{controller=%s}%s", parent->prefix, resource, pattern);
    }
    if (target && *target) {
        target = sjoin("/", target, NULL);
    }
    if (*resource == '{') {
        target = sfmt("$%s%s", resource, target);
        source = sfmt("$%s.c", resource);
    } else {
        target = sfmt("%s%s", resource, target);
        source = sfmt("%s.c", resource);
    }
    return httpDefineRoute(parent, methods, pattern, target, source);
}


/*
    Most routes are POST and params are in body.
 */
PUBLIC void httpAddPostGroup(HttpRoute *parent, cchar *resource)
{
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "$",                 "",          resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/create*$",         "create",    resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/edit$",            "edit",      resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/get$",             "get",       resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/find$",            "find",      resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/init$",            "init",      resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/remove$",          "remove",    resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/update$",          "update",    resource);

    httpAddWebSocketsRoute(parent, "stream");
    httpAddRestfulRoute(parent, "OPTIONS,POST",     "/{action}(/)*$",   "${action}", resource);
}


/*
    The different with ResourceGroup is that "list" is a POST and listg is a GET.
    Also provides a POST getp.
 */
PUBLIC void httpAddSpaGroup(HttpRoute *parent, cchar *resource)
{
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "$",                           "",          resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "/list$",                      "listg",     resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "/init$",                      "init",      resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "/{id=[0-9]+}/edit$",          "edit",      resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "/{id=[0-9]+}$",               "get",       resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/{id=[0-9]+}/get$",           "getp",      resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/list$",                      "list",      resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/{id=[0-9]+}/delete$",        "delete",    resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "(/)*$",                       "create",    resource);

    httpAddWebSocketsRoute(parent, "stream");
    httpAddRestfulRoute(parent, "OPTIONS,DELETE",  "/{id=[0-9]+}$",               "remove",    resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/{id=[0-9]+}$",               "update",    resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET,POST","/{id=[0-9]+}/{action}(/)*$",  "${action}", resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET,POST","/{action}(/)*$",              "${action}", resource);
}


PUBLIC void httpAddResourceGroup(HttpRoute *parent, cchar *resource)
{
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "$",                           "",          resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "/list$",                      "list",      resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "/init$",                      "init",      resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "/{id=[0-9]+}/edit$",          "edit",      resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "/{id=[0-9]+}$",               "get",       resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/list$",                      "listp",     resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/{id=[0-9]+}/delete$",        "delete",    resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "(/)*$",                       "create",    resource);

    httpAddWebSocketsRoute(parent, "stream");
    httpAddRestfulRoute(parent, "OPTIONS,DELETE",  "/{id=[0-9]+}$",               "remove",    resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/{id=[0-9]+}$",               "update",    resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET,POST","/{id=[0-9]+}/{action}(/)*$",  "${action}", resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET,POST","/{action}(/)*$",              "${action}", resource);
}


/*
    Singleton resource
 */
PUBLIC void httpAddResource(HttpRoute *parent, cchar *resource)
{
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "$",              "",           resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "/edit$",         "edit",       resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "/init$",         "init",       resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "(/)*$",          "get",        resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/delete$",       "delete",     resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "(/)*$",          "create",     resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "(/)*$",          "update",     resource);

    httpAddWebSocketsRoute(parent, "stream");
    httpAddRestfulRoute(parent, "OPTIONS,DELETE",  "(/)*$",          "remove",     resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET,POST","/{action}(/)*$", "${action}",  resource);
}


/*
    Add routes for a permanent resource. Cannot create or remove.
 */
PUBLIC void httpAddPermResource(HttpRoute *parent, cchar *resource)
{
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "$",              "",           resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "/get$",          "getp",        resource);
    httpAddRestfulRoute(parent, "OPTIONS,GET",     "(/)*$",          "get",        resource);
    httpAddRestfulRoute(parent, "OPTIONS,POST",    "(/)*$",          "update",     resource);
    httpAddWebSocketsRoute(parent, "stream");
    httpAddRestfulRoute(parent, "OPTIONS,GET,POST","/{action}(/)*$", "${action}",  resource);
}


PUBLIC HttpRoute *httpAddWebSocketsRoute(HttpRoute *parent, cchar *action)
{
    HttpRoute   *route;
    HttpLimits  *limits;
    cchar       *path, *pattern;

    pattern = sfmt("^%s/{controller}/%s", parent->prefix, action);
    path = sjoin("$1/", action, NULL);
    route = httpDefineRoute(parent, "OPTIONS,GET", pattern, path, "${controller}.c");
    httpAddRouteFilter(route, "webSocketFilter", "", HTTP_STAGE_RX | HTTP_STAGE_TX);

    /*
        Set some reasonable defaults. 5 minutes for inactivity and no request timeout limit.
     */
    limits = httpGraduateLimits(route, 0);
    limits->inactivityTimeout = ME_MAX_INACTIVITY_DURATION * 10;
    limits->requestTimeout = HTTP_UNLIMITED;
    limits->rxBodySize = HTTP_UNLIMITED;
    limits->txBodySize = HTTP_UNLIMITED;
    return route;
}

/*************************************************** Support Routines ****************************************************/
/*
    Route operations are used per-route for headers and fields
 */
static HttpRouteOp *createRouteOp(cchar *name, int flags)
{
    HttpRouteOp   *op;

    assert(name && *name);

    if ((op = mprAllocObj(HttpRouteOp, manageRouteOp)) == 0) {
        return 0;
    }
    op->name = sclone(name);
    op->flags = flags;
    return op;
}


static void manageRouteOp(HttpRouteOp *op, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(op->name);
        mprMark(op->details);
        mprMark(op->var);
        mprMark(op->value);

    } else if (flags & MPR_MANAGE_FREE) {
        if (op->flags & HTTP_ROUTE_FREE) {
            free(op->mdata);
        }
    }
}


static bool opPresent(MprList *list, HttpRouteOp *op)
{
    HttpRouteOp   *last;

    if ((last = mprGetLastItem(list)) == 0) {
        return 0;
    }
    if (smatch(last->name, op->name) &&
        smatch(last->details, op->details) &&
        smatch(last->var, op->var) &&
        smatch(last->value, op->value) &&
        last->mdata == op->mdata &&
        last->flags == op->flags) {
        return 1;
    }
    return 0;
}


static void addUniqueItem(MprList *list, HttpRouteOp *op)
{
    int     index;

    assert(list);
    assert(op);

    if (!opPresent(list, op)) {
        index = smatch(op->name, "secure") ? 0 : list->length;
        mprInsertItemAtPos(list, index, op);
    }
}


static HttpLang *createLangDef(cchar *path, cchar *suffix, int flags)
{
    HttpLang    *lang;

    if ((lang = mprAllocObj(HttpLang, manageLang)) == 0) {
        return 0;
    }
    if (path) {
        lang->path = sclone(path);
    }
    if (suffix) {
        lang->suffix = sclone(suffix);
    }
    lang->flags = flags;
    return lang;
}


static void manageLang(HttpLang *lang, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(lang->path);
        mprMark(lang->suffix);
    }
}


static void definePathVars(HttpRoute *route)
{
    assert(route);

    mprAddKey(route->vars, "PRODUCT", sclone(ME_NAME));
    mprAddKey(route->vars, "OS", sclone(ME_OS));
    mprAddKey(route->vars, "VERSION", sclone(ME_VERSION));
    mprAddKey(route->vars, "PLATFORM", sclone(ME_PLATFORM));
    mprAddKey(route->vars, "BIN_DIR", mprGetAppDir());
    if (route->host) {
        defineHostVars(route);
    }
}


static void defineHostVars(HttpRoute *route)
{
    assert(route);
    mprAddKey(route->vars, "DOCUMENTS", route->documents);
    mprAddKey(route->vars, "HOME", route->home);
    mprAddKey(route->vars, "HOST", route->host->name);
}


static char *expandTokens(HttpStream *stream, cchar *str)
{
    HttpRx      *rx;

    assert(stream);
    assert(str);

    rx = stream->rx;
    return expandRequestTokens(stream, expandPatternTokens(rx->pathInfo, str, rx->matches, rx->matchCount));
}


/*
    WARNING: str is modified. Result is allocated string.
 */
static char *expandRequestTokens(HttpStream *stream, char *str)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpRoute   *route;
    HttpUri     *uri;
    MprBuf      *buf;
    HttpLang    *lang;
    char        *tok, *cp, *key, *value, *field, *header, *defaultValue, *state, *v, *p;

    assert(stream);

    if (!str || *str == '\0') {
        return sclone("");
    }
    rx = stream->rx;
    route = rx->route;
    tx = stream->tx;
    buf = mprCreateBuf(-1, -1);
    tok = 0;
    uri = rx->parsedUri;

    for (cp = str; cp && *cp; ) {
        if ((tok = strstr(cp, "${")) == 0) {
            break;
        }
        if (tok > cp) {
            mprPutBlockToBuf(buf, cp, tok - cp);
        }
        if ((key = stok(&tok[2], ".:}", &value)) == 0) {
            continue;
        }
        if ((stok(value, "}", &p)) != 0) {
            cp = p;
        } else {
            continue;
        }
        if (smatch(key, "header")) {
            header = stok(value, "=", &defaultValue);
            if ((value = (char*) httpGetHeader(stream, header)) == 0) {
                value = defaultValue ? defaultValue : "";
            }
            mprPutStringToBuf(buf, value);

        } else if (smatch(key, "param")) {
            field = stok(value, "=", &defaultValue);
            if (defaultValue == 0) {
                defaultValue = "";
            }
            mprPutStringToBuf(buf, httpGetParam(stream, field, defaultValue));

        } else if (smatch(key, "request")) {
            value = stok(value, "=", &defaultValue);
            //  OPT with switch on first char

            if (smatch(value, "authenticated")) {
                mprPutStringToBuf(buf, rx->authenticated ? "true" : "false");

            } else if (smatch(value, "clientAddress")) {
                mprPutStringToBuf(buf, stream->ip);

            } else if (smatch(value, "clientPort")) {
                mprPutIntToBuf(buf, stream->port);

            } else if (smatch(value, "error")) {
                mprPutStringToBuf(buf, stream->errorMsg);

            } else if (smatch(value, "ext")) {
                mprPutStringToBuf(buf, uri->ext);

            } else if (smatch(value, "extraPath")) {
                mprPutStringToBuf(buf, rx->extraPath);

            } else if (smatch(value, "filename")) {
                mprPutStringToBuf(buf, tx->filename);

            } else if (scaselessmatch(value, "language")) {
                if (!defaultValue) {
                    defaultValue = route->defaultLanguage;
                }
                if ((lang = httpGetLanguage(stream, route->languages, defaultValue)) != 0) {
                    mprPutStringToBuf(buf, lang->suffix);
                } else {
                    mprPutStringToBuf(buf, defaultValue);
                }

            } else if (scaselessmatch(value, "languageDir")) {
                lang = httpGetLanguage(stream, route->languages, 0);
                if (!defaultValue) {
                    defaultValue = ".";
                }
                mprPutStringToBuf(buf, lang ? lang->path : defaultValue);

            } else if (smatch(value, "host")) {
                mprPutStringToBuf(buf, httpFormatUri(0, uri->host, uri->port, 0, 0, 0, 0));

            } else if (smatch(value, "method")) {
                mprPutStringToBuf(buf, rx->method);

            } else if (smatch(value, "origin")) {
                if (rx->origin) {
                    mprPutStringToBuf(buf, rx->origin);
                } else {
                    mprPutStringToBuf(buf, httpFormatUri(rx->parsedUri->scheme, uri->host, uri->port, 0, 0, 0, 0));
                }

            } else if (smatch(value, "originalUri")) {
                mprPutStringToBuf(buf, rx->originalUri);

            } else if (smatch(value, "pathInfo")) {
                mprPutStringToBuf(buf, rx->pathInfo);

            } else if (smatch(value, "prefix")) {
                mprPutStringToBuf(buf, route->prefix);

            } else if (smatch(value, "query")) {
                mprPutStringToBuf(buf, uri->query);

            } else if (smatch(value, "reference")) {
                mprPutStringToBuf(buf, uri->reference);

            } else if (smatch(value, "scheme")) {
                if (uri->scheme) {
                    mprPutStringToBuf(buf, uri->scheme);
                }  else {
                    mprPutStringToBuf(buf, (stream->secure) ? "https" : "http");
                }

            } else if (smatch(value, "scriptName")) {
                mprPutStringToBuf(buf, rx->scriptName);

            } else if (smatch(value, "serverAddress")) {
                /* Pure IP address, no port. See "serverPort" */
                mprPutStringToBuf(buf, stream->sock->acceptIp);

            } else if (smatch(value, "serverPort")) {
                mprPutIntToBuf(buf, stream->sock->acceptPort);

            } else if (smatch(value, "uri")) {
                mprPutStringToBuf(buf, rx->uri);
            }

        } else if (smatch(key, "ssl")) {
            value = stok(value, "=", &defaultValue);
            if (smatch(value, "state")) {
                mprPutStringToBuf(buf, mprGetSocketState(stream->sock));
            } else {
                state = mprGetSocketState(stream->sock);
                if ((p = scontains(state, value)) != 0) {
                    stok(p, "=", &v);
                    mprPutStringToBuf(buf, stok(v, ", ", NULL));
                }
            }
        }
    }
    if (tok) {
        if (cp && tok > cp) {
            mprPutBlockToBuf(buf, tok, tok - cp);
        }
    } else {
        mprPutStringToBuf(buf, cp);
    }
    mprAddNullToBuf(buf);
    return sclone(mprGetBufStart(buf));
}


PUBLIC char *httpExpandVars(HttpStream *stream, cchar *str)
{
    return expandRequestTokens(stream, stemplate(str, stream->rx->route->vars));
}


/*
    Replace text using pcre regular expression match indexes
 */
static char *expandPatternTokens(cchar *str, cchar *replacement, int *matches, int matchCount)
{
    MprBuf  *result;
    cchar   *end, *cp, *lastReplace;
    int     submatch;

    assert(str);
    assert(replacement);
    assert(matches);

    result = mprCreateBuf(-1, -1);
    lastReplace = replacement;
    end = &replacement[slen(replacement)];

    for (cp = replacement; cp < end; ) {
        if (*cp == '$') {
            if (lastReplace < cp) {
                mprPutSubStringToBuf(result, lastReplace, (int) (cp - lastReplace));
            }
            switch (*++cp) {
            case '$':
                mprPutCharToBuf(result, '$');
                break;
            case '&':
                /* Replace the matched string */
                if (matchCount > 0) {
                    mprPutSubStringToBuf(result, &str[matches[0]], matches[1] - matches[0]);
                }
                break;
            case '`':
                /* Insert the portion that preceeds the matched string */
                if (matchCount > 0) {
                    mprPutSubStringToBuf(result, str, matches[0]);
                }
                break;
            case '\'':
                /* Insert the portion that follows the matched string */
                if (matchCount > 0) {
                    mprPutSubStringToBuf(result, &str[matches[1]], slen(str) - matches[1]);
                }
                break;
            default:
                /* Insert the nth submatch */
                if (isdigit((uchar) *cp)) {
                    submatch = (int) atoi(cp);
                    while (isdigit((uchar) *++cp))
                        ;
                    cp--;
                    if (submatch < matchCount) {
                        submatch *= 2;
                        mprPutSubStringToBuf(result, &str[matches[submatch]], matches[submatch + 1] - matches[submatch]);
                    }
                } else {
                    mprDebug("http route", 5, "Bad replacement $ specification in page");
                    return 0;
                }
            }
            lastReplace = cp + 1;
        }
        cp++;
    }
    if (lastReplace < cp && lastReplace < end) {
        mprPutSubStringToBuf(result, lastReplace, (int) (cp - lastReplace));
    }
    mprAddNullToBuf(result);
    return sclone(mprGetBufStart(result));
}


PUBLIC void httpDefineRouteBuiltins()
{
    /*
        These are the conditions that can be selected. Use httpAddRouteCondition to add to a route.
        The allow and auth conditions are internal and are configured via various Auth APIs.
     */
    httpDefineRouteCondition("allowDeny", allowDenyCondition);
    httpDefineRouteCondition("auth", authCondition);
    httpDefineRouteCondition("directory", directoryCondition);
    httpDefineRouteCondition("exists", existsCondition);
    httpDefineRouteCondition("match", matchCondition);
    httpDefineRouteCondition("secure", secureCondition);
    httpDefineRouteCondition("unauthorized", unauthorizedCondition);

    httpDefineRouteUpdate("param", paramUpdate);
    httpDefineRouteUpdate("cmd", cmdUpdate);
    httpDefineRouteUpdate("lang", langUpdate);

    httpDefineRouteTarget("close", closeTarget);
    httpDefineRouteTarget("redirect", redirectTarget);
    httpDefineRouteTarget("run", runTarget);
    httpDefineRouteTarget("write", writeTarget);
}


/*
    Tokenizes a line using %formats. Mandatory tokens can be specified with %. Optional tokens are specified with ?.
    Supported tokens:
        %B - Boolean. Parses: on/off, true/false, yes/no.
        %N - Number. Parses numbers in base 10.
        %S - String. Removes quotes.
        %T - Template String. Removes quotes and expand ${PathVars}.
        %P - Path string. Removes quotes and expands ${PathVars}. Resolved relative to host->dir (Home).
        %W - Parse words into a list
        %! - Optional negate. Set value to HTTP_ROUTE_NOT present, otherwise zero.
    Values wrapped in quotes will have the outermost quotes trimmed.
 */
PUBLIC bool httpTokenize(HttpRoute *route, cchar *line, cchar *fmt, ...)
{
    va_list     args;
    bool        rc;

    assert(route);
    assert(line);
    assert(fmt);

    va_start(args, fmt);
    rc =  httpTokenizev(route, line, fmt, args);
    va_end(args);
    return rc;
}


PUBLIC bool httpTokenizev(HttpRoute *route, cchar *line, cchar *fmt, va_list args)
{
    MprList     *list;
    cchar       *f;
    char        *tok, *etok, *value, *word, *end;
    int         quote;

    assert(route);
    assert(fmt);

    if (line == 0) {
        line = "";
    }
    tok = sclone(line);
    end = &tok[slen(line)];

    for (f = fmt; *f && tok < end; f++) {
        for (; isspace((uchar) *tok); tok++) ;
        if (*tok == '\0' || *tok == '#') {
            break;
        }
        if (isspace((uchar) *f)) {
            continue;
        }
        if (*f == '%' || *f == '?') {
            f++;
            quote = 0;
            if (*f != '*' && (*tok == '"' || *tok == '\'')) {
                quote = *tok++;
            }
            if (*f == '!') {
                etok = &tok[1];
            } else {
                if (quote) {
                    for (etok = tok; *etok && !(*etok == quote && etok[-1] != '\\'); etok++) ;
                    *etok++ = '\0';
                } else if (*f == '*') {
                    for (etok = tok; *etok; etok++) {
                        if (*etok == '#') {
                            *etok = '\0';
                        }
                    }
                } else {
                    for (etok = tok; *etok && !isspace((uchar) *etok); etok++) ;
                }
                *etok++ = '\0';
            }
            if (*f == '*') {
                f++;
                tok = trimQuotes(tok);
                * va_arg(args, char**) = tok;
                tok = etok;
                break;
            }

            switch (*f) {
            case '!':
                if (*tok == '!') {
                    *va_arg(args, int*) = HTTP_ROUTE_NOT;
                } else {
                    *va_arg(args, int*) = 0;
                    continue;
                }
                break;
            case 'B':
                *va_arg(args, bool*) = httpGetBoolToken(tok);
                break;
            case 'N':
                *va_arg(args, int*) = (int) stoi(tok);
                break;
            case 'P':
                *va_arg(args, char**) = httpMakePath(route, route->home, strim(tok, "\"", MPR_TRIM_BOTH));
                break;
            case 'S':
                *va_arg(args, char**) = strim(tok, "\"", MPR_TRIM_BOTH);
                break;
            case 'T':
                value = strim(tok, "\"", MPR_TRIM_BOTH);
                *va_arg(args, char**) = stemplate(value, route->vars);
                break;
            case 'W':
                list = va_arg(args, MprList*);
                word = stok(tok, " \t\r\n", &tok);
                while (word) {
                    mprAddItem(list, word);
                    word = stok(0, " \t\r\n", &tok);
                }
                break;
            default:
                mprDebug("http route", 5, "Unknown token pattern %%\"%c\"", *f);
                break;
            }
            tok = etok;
        }
    }
    if (tok < end) {
        /*
            Extra unparsed text
         */
        for (; tok < end && isspace((uchar) *tok); tok++) ;
        if (*tok && *tok != '#') {
            mprDebug("http route", 5, "Extra unparsed text: \"%s\"", tok);
            return 0;
        }
    }
    if (*f) {
        /*
            Extra unparsed format tokens
         */
        for (; *f; f++) {
            if (*f == '%') {
                break;
            } else if (*f == '?') {
                switch (*++f) {
                case '!':
                case 'N':
                    *va_arg(args, int*) = 0;
                    break;
                case 'B':
                    *va_arg(args, bool*) = 0;
                    break;
                case 'D':
                case 'P':
                case 'S':
                case 'T':
                case '*':
                    *va_arg(args, char**) = 0;
                    break;
                case 'W':
                    break;
                default:
                    mprDebug("http route", 5, "Unknown token pattern %%\"%c\"", *f);
                    break;
                }
            }
        }
        if (*f) {
            mprDebug("http route", 5, "Missing directive parameters");
            return 0;
        }
    }
    va_end(args);
    return 1;
}


PUBLIC bool httpGetBoolToken(cchar *tok)
{
    return scaselessmatch(tok, "on") || scaselessmatch(tok, "true") || scaselessmatch(tok, "enable") ||
        scaselessmatch(tok, "yes") || smatch(tok, "1");
}


static char *trimQuotes(char *str)
{
    ssize   len;

    assert(str);
    len = slen(str);
    if (*str == '\"' && str[len - 1] == '\"' && len > 2 && str[1] != '\"') {
        return snclone(&str[1], len - 2);
    }
    return sclone(str);
}


PUBLIC cchar *httpGetDir(HttpRoute *route, cchar *name)
{
    cchar   *key;

    key = sjoin(supper(name), "_DIR", NULL);
    return httpGetRouteVar(route, key);
}


PUBLIC void httpSetDir(HttpRoute *route, cchar *name, cchar *value)
{
    cchar   *path, *rpath;

    if (value == 0) {
        value = slower(name);
    }
    path = httpMakePath(route, 0, value);
    path = mprJoinPath(route->home, path);
    name = supper(name);

    /*
        Define the variable as a relative path to the route home
     */
    rpath = mprGetRelPath(path, route->home);
    httpSetRouteVar(route, sjoin(name, "_DIR", NULL), rpath);

    /*
        Home and documents are stored as absolute paths
     */
    if (smatch(name, "HOME")) {
        httpSetRouteVar(route, name, rpath);
        route->home = path;
    } else if (smatch(name, "DOCUMENTS")) {
        httpSetRouteVar(route, name, rpath);
        route->documents = path;
    }
}


PUBLIC MprHash *httpGetOptions(cchar *options)
{
    if (options == 0) {
        return mprCreateHash(-1, MPR_HASH_STABLE);
    }
    if (*options == '@') {
        /* Allow embedded URIs as options */
        options = sfmt("{ data-click: '%s'}", options);
    }
    assert(*options == '{');
    if (*options != '{') {
        options = sfmt("{%s}", options);
    }
    return mprDeserialize(options);
}


PUBLIC void *httpGetOption(MprHash *options, cchar *field, cchar *defaultValue)
{
    MprKey      *kp;
    cchar       *value;

    if (options == 0) {
        value = defaultValue;
    } else if ((kp = mprLookupKeyEntry(options, field)) == 0) {
        value = defaultValue;
    } else {
        value = kp->data;
    }
    return (void*) value;
}


PUBLIC MprHash *httpGetOptionHash(MprHash *options, cchar *field)
{
    MprKey      *kp;

    if (options == 0) {
        return 0;
    }
    if ((kp = mprLookupKeyEntry(options, field)) == 0) {
        return 0;
    }
    return (MprHash*) kp->data;
}


/*
    Prepend an option
 */
PUBLIC void httpInsertOption(MprHash *options, cchar *field, cchar *value)
{
    MprKey      *kp;

    if (options == 0) {
        assert(options);
        return;
    }
    if ((kp = mprLookupKeyEntry(options, field)) != 0) {
        kp = mprAddKey(options, field, sjoin(value, " ", kp->data, NULL));
    } else {
        kp = mprAddKey(options, field, value);
    }
}


PUBLIC void httpAddOption(MprHash *options, cchar *field, cchar *value)
{
    MprKey      *kp;

    if (options == 0) {
        assert(options);
        return;
    }
    if ((kp = mprLookupKeyEntry(options, field)) != 0) {
        kp = mprAddKey(options, field, sjoin(kp->data, " ", value, NULL));
    } else {
        kp = mprAddKey(options, field, value);
    }
}


PUBLIC void httpRemoveOption(MprHash *options, cchar *field)
{
    if (options == 0) {
        assert(options);
        return;
    }
    mprRemoveKey(options, field);
}


PUBLIC bool httpOption(MprHash *hash, cchar *field, cchar *value, int useDefault)
{
    return smatch(value, httpGetOption(hash, field, useDefault ? value : 0));
}


PUBLIC void httpSetOption(MprHash *options, cchar *field, cchar *value)
{
    if (value == 0) {
        return;
    }
    if (options == 0) {
        assert(options);
        return;
    }
    mprAddKey(options, field, value);
}


PUBLIC void httpHideRoute(HttpRoute *route, bool on)
{
    route->flags &= ~HTTP_ROUTE_HIDDEN;
    if (on) {
        route->flags |= HTTP_ROUTE_HIDDEN;
    }
}


PUBLIC HttpLimits *httpGraduateLimits(HttpRoute *route, HttpLimits *limits)
{
    if (route->parent && route->limits == route->parent->limits) {
        if (limits == 0) {
            if (route->parent->limits) {
                limits = route->parent->limits;
            } else {
                limits = HTTP->serverLimits;
            }
        }
        route->limits = mprMemdup(limits, sizeof(HttpLimits));
    }
    return route->limits;
}


PUBLIC void httpSetRouteCallback(HttpRoute *route, HttpRouteCallback callback)
{
    route->callback = callback;
}


PUBLIC int httpSetRouteCanonicalName(HttpRoute *route, cchar *name)
{
    if (!name || *name == '\0') {
        mprLog("error http", 0, "Empty host name");
        return MPR_ERR_BAD_ARGS;
    }
    if (schr(name, ':')) {
        route->canonical = httpCreateUri(name, 0);
    } else {
        route->canonical = httpCreateUri(sjoin(name, ":", 0), 0);
    }
    return 0;
}

PUBLIC void httpSetRouteCharSet(HttpRoute *route, cchar *charSet)
{
    route->charSet = sclone(charSet);
}


#undef  GRADUATE_HASH
#undef  GRADUATE_LIST

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/rx.c ************/

/*
    rx.c -- Http receiver. Parses http requests and client responses.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/***************************** Forward Declarations ***************************/

static void manageRx(HttpRx *rx, int flags);

/*********************************** Code *************************************/

PUBLIC HttpRx *httpCreateRx(HttpStream *stream)
{
    HttpRx      *rx;
    int         peer;

    if ((rx = mprAllocObj(HttpRx, manageRx)) == 0) {
        return 0;
    }
    rx->stream = stream;
    rx->length = -1;
    rx->ifMatch = 1;
    rx->ifModified = 1;
    rx->pathInfo = sclone("/");
    rx->scriptName = mprEmptyString();
    rx->needInputPipeline = httpClientStream(stream);
    rx->headers = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS | MPR_HASH_STABLE);
    rx->chunkState = HTTP_CHUNK_UNCHUNKED;
    rx->remainingContent = 0;
    rx->http2ContentLength = -1;

    rx->seqno = ++stream->net->totalRequests;
    peer = stream->net->address ? stream->net->address->seqno : 0;
    rx->traceId = sfmt("%d-0-%lld-%d", peer, stream->net->seqno, rx->seqno);
    return rx;
}


static void manageRx(HttpRx *rx, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(rx->accept);
        mprMark(rx->acceptCharset);
        mprMark(rx->acceptEncoding);
        mprMark(rx->acceptLanguage);
        mprMark(rx->authDetails);
        mprMark(rx->authType);
        mprMark(rx->stream);
        mprMark(rx->connection);
        mprMark(rx->contentLength);
        mprMark(rx->cookie);
        mprMark(rx->etags);
        mprMark(rx->extraPath);
        mprMark(rx->files);
        mprMark(rx->headerPacket);
        mprMark(rx->headers);
        mprMark(rx->hostHeader);
        mprMark(rx->inputPipeline);
        mprMark(rx->inputRange);
        mprMark(rx->lang);
        mprMark(rx->method);
        mprMark(rx->mimeType);
        mprMark(rx->origin);
        mprMark(rx->originalMethod);
        mprMark(rx->originalUri);
        mprMark(rx->paramString);
        mprMark(rx->params);
        mprMark(rx->parsedUri);
        mprMark(rx->passwordDigest);
        mprMark(rx->pathInfo);
        mprMark(rx->pragma);
        mprMark(rx->protocol);
        mprMark(rx->redirect);
        mprMark(rx->referrer);
        mprMark(rx->requestData);
        mprMark(rx->route);
        mprMark(rx->scriptName);
        mprMark(rx->scheme);
        mprMark(rx->securityToken);
        mprMark(rx->session);
        mprMark(rx->statusMessage);
        mprMark(rx->svars);
        mprMark(rx->target);
        mprMark(rx->traceId);
        mprMark(rx->upgrade);
        mprMark(rx->uri);
        mprMark(rx->userAgent);
        mprMark(rx->webSocket);
    }
}


PUBLIC void httpDestroyRx(HttpRx *rx)
{
    if (rx->stream) {
        rx->stream->rx = 0;
        rx->stream = 0;
    }
}


/*
    Set the global request callback
 */
PUBLIC void httpSetRequestCallback(HttpRequestCallback callback)
{
    if (HTTP) {
        HTTP->requestCallback = callback;
    }
}


PUBLIC void httpCloseRx(HttpStream *stream)
{
    if (stream->rx && !stream->rx->remainingContent) {
        /* May not have consumed all read data, so cannot be assured the next request will be okay */
        stream->keepAliveCount = 0;
    }
    if (httpClientStream(stream)) {
        httpEnableNetEvents(stream->net);
    }
}


PUBLIC bool httpContentNotModified(HttpStream *stream)
{
    HttpRx      *rx;
    HttpTx      *tx;
    MprTime     modified;
    bool        same;

    rx = stream->rx;
    tx = stream->tx;

    if (rx->flags & HTTP_IF_MODIFIED) {
        /*
            If both checks, the last modification time and etag, claim that the request doesn't need to be
            performed, skip the transfer.
         */
        assert(tx->fileInfo.valid);
        modified = (MprTime) tx->fileInfo.mtime * TPS;
        same = httpMatchModified(stream, modified) && httpMatchEtag(stream, tx->etag);
        if (tx->outputRanges && !same) {
            tx->outputRanges = 0;
        }
        return same;
    }
    return 0;
}


PUBLIC MprOff httpGetContentLength(HttpStream *stream)
{
    if (stream->rx == 0) {
        assert(stream->rx);
        return 0;
    }
    return stream->rx->length;
}


PUBLIC cchar *httpGetCookies(HttpStream *stream)
{
    if (stream->rx == 0) {
        assert(stream->rx);
        return 0;
    }
    return stream->rx->cookie;
}


/*
    Extract a cookie.
    The rx->cookies contains a list of header cookies. A site may submit multiple cookies separated by ";"
 */
PUBLIC cchar *httpGetCookie(HttpStream *stream, cchar *name)
{
    HttpRx  *rx;
    cchar   *cookie;
    char    *cp, *value;
    ssize   nlen;
    int     quoted;

    assert(stream);
    rx = stream->rx;
    assert(rx);

    if ((cookie = rx->cookie) == 0 || name == 0 || *name == '\0') {
        return 0;
    }
    nlen = slen(name);
    while ((value = strstr(cookie, name)) != 0) {
        /* Ignore corrupt cookies of the form "name=;" */
        if ((value == rx->cookie || value[-1] == ' ' || value[-1] == ';') && value[nlen] == '=' && value[nlen+1] != ';') {
            break;
        }
        cookie += (value - cookie) + nlen;

    }
    if (value == 0) {
        return 0;
    }
    value += nlen;
    while (isspace((uchar) *value) || *value == '=') {
        value++;
    }
    quoted = 0;
    if (*value == '"') {
        value++;
        quoted++;
    }
    for (cp = value; *cp; cp++) {
        if (quoted) {
            if (*cp == '"' && cp[-1] != '\\') {
                break;
            }
        } else {
            if ((*cp == ',' || *cp == ';') && cp[-1] != '\\') {
                break;
            }
        }
    }
    return snclone(value, cp - value);
}


PUBLIC cchar *httpGetHeader(HttpStream *stream, cchar *key)
{
    if (stream->rx == 0) {
        assert(stream->rx);
        return 0;
    }
    return mprLookupKey(stream->rx->headers, slower(key));
}


PUBLIC char *httpGetHeadersFromHash(MprHash *hash)
{
    MprKey      *kp;
    char        *headers, *cp;
    ssize       len;

    for (len = 0, kp = 0; (kp = mprGetNextKey(hash, kp)) != 0; ) {
        len += strlen(kp->key) + 2 + strlen(kp->data) + 1;
    }
    if ((headers = mprAlloc(len + 1)) == 0) {
        return 0;
    }
    for (kp = 0, cp = headers; (kp = mprGetNextKey(hash, kp)) != 0; ) {
        strcpy(cp, kp->key);
        cp += strlen(cp);
        *cp++ = ':';
        *cp++ = ' ';
        strcpy(cp, kp->data);
        cp += strlen(cp);
        *cp++ = '\n';
    }
    *cp = '\0';
    return headers;
}


PUBLIC char *httpGetHeaders(HttpStream *stream)
{
    return httpGetHeadersFromHash(stream->rx->headers);
}


PUBLIC MprHash *httpGetHeaderHash(HttpStream *stream)
{
    if (stream->rx == 0) {
        assert(stream->rx);
        return 0;
    }
    return stream->rx->headers;
}


PUBLIC cchar *httpGetQueryString(HttpStream *stream)
{
    return (stream->rx && stream->rx->parsedUri) ? stream->rx->parsedUri->query : 0;
}


PUBLIC int httpGetStatus(HttpStream *stream)
{
    return (stream->rx) ? stream->rx->status : 0;
}


PUBLIC cchar *httpGetStatusMessage(HttpStream *stream)
{
    return (stream->rx) ? stream->rx->statusMessage : 0;
}


PUBLIC int httpSetUri(HttpStream *stream, cchar *uri)
{
    HttpRx      *rx;
    HttpUri     *parsedUri;
    char        *pathInfo;

    rx = stream->rx;
    if ((parsedUri = httpCreateUri(uri, 0)) == 0 || !parsedUri->valid) {
        return MPR_ERR_BAD_ARGS;
    }
    if (parsedUri->host && !rx->hostHeader) {
        rx->hostHeader = parsedUri->host;
    }
    /*
        Decode and validate the URI path
     */
    if ((pathInfo = httpValidateUriPath(parsedUri->path)) == 0) {
        return MPR_ERR_BAD_ARGS;
    }
    rx->pathInfo = pathInfo;
    rx->uri = parsedUri->path;
    stream->tx->ext = httpGetExt(stream);

    /*
        Start out with no scriptName and the entire URI in the pathInfo. Stages may rewrite.
     */
    rx->scriptName = mprEmptyString();
    rx->parsedUri = parsedUri;
    return 0;
}


PUBLIC bool httpIsEof(HttpStream *stream)
{
    return stream->rx == 0 || stream->rx->eof;
}


/*
    Match the entity's etag with the client's provided etag.
 */
PUBLIC bool httpMatchEtag(HttpStream *stream, char *requestedEtag)
{
    HttpRx  *rx;
    char    *tag;
    int     next;

    rx = stream->rx;
    if (rx->etags == 0) {
        return 1;
    }
    if (requestedEtag == 0) {
        return 0;
    }
    for (next = 0; (tag = mprGetNextItem(rx->etags, &next)) != 0; ) {
        if (strcmp(tag, requestedEtag) == 0) {
            return (rx->ifMatch) ? 0 : 1;
        }
    }
    return (rx->ifMatch) ? 1 : 0;
}


/*
    If an IF-MODIFIED-SINCE was specified, then return true if the resource has not been modified. If using
    IF-UNMODIFIED, then return true if the resource was modified.
 */
PUBLIC bool httpMatchModified(HttpStream *stream, MprTime time)
{
    HttpRx   *rx;

    rx = stream->rx;

    if (rx->since == 0) {
        /*  If-Modified or UnModified not supplied. */
        return 1;
    }
    if (rx->ifModified) {
        /*  Return true if the file has not been modified.  */
        return !(time > rx->since);
    } else {
        /*  Return true if the file has been modified.  */
        return (time > rx->since);
    }
}


PUBLIC void httpSetEof(HttpStream *stream)
{
    if (stream) {
        stream->rx->eof = 1;
    }
}


PUBLIC void httpSetStageData(HttpStream *stream, cchar *key, cvoid *data)
{
    HttpRx      *rx;

    rx = stream->rx;
    if (rx->requestData == 0) {
        rx->requestData = mprCreateHash(-1, 0);
    }
    mprAddKey(rx->requestData, key, data);
}


PUBLIC cvoid *httpGetStageData(HttpStream *stream, cchar *key)
{
    HttpRx      *rx;

    rx = stream->rx;
    if (rx->requestData == 0) {
        return NULL;
    }
    return mprLookupKey(rx->requestData, key);
}


PUBLIC char *httpGetPathExt(cchar *path)
{
    char    *ep, *ext;

    if ((ext = strrchr(path, '.')) != 0) {
        ext = sclone(++ext);
        for (ep = ext; *ep && isalnum((uchar) *ep); ep++) {
            ;
        }
        *ep = '\0';
    }
    return ext;
}


/*
    Get the request extension. Look first at the URI pathInfo. If no extension, look at the filename if defined.
    Return NULL if no extension.
 */
PUBLIC char *httpGetExt(HttpStream *stream)
{
    HttpRx          *rx;
    MprFileSystem   *fs;
    char            *ext;

    rx = stream->rx;
    if ((ext = httpGetPathExt(rx->pathInfo)) == 0) {
        if (stream->tx->filename) {
            ext = httpGetPathExt(stream->tx->filename);
        }
    }
    fs = mprLookupFileSystem("/");
    if (!fs->caseSensitive) {
        ext = slower(ext);
    }
    return ext;
}


static int compareLang(char **s1, char **s2)
{
    return scmp(*s2, *s1);
}


PUBLIC HttpLang *httpGetLanguage(HttpStream *stream, MprHash *spoken, cchar *defaultLang)
{
    HttpRx      *rx;
    HttpLang    *lang;
    MprList     *list;
    cchar       *accept;
    char        *nextTok, *tok, *quality, *language;
    int         next;

    rx = stream->rx;
    if (rx->lang) {
        return rx->lang;
    }
    if (spoken == 0) {
        return 0;
    }
    list = mprCreateList(-1, MPR_LIST_STABLE);
    if ((accept = httpGetHeader(stream, "Accept-Language")) != 0) {
        for (tok = stok(sclone(accept), ",", &nextTok); tok; tok = stok(nextTok, ",", &nextTok)) {
            language = stok(tok, ";q=", &quality);
            if (quality == 0) {
                quality = "1";
            }
            mprAddItem(list, sfmt("%03d %s", (int) (atof(quality) * 100), language));
        }
        mprSortList(list, (MprSortProc) compareLang, 0);
        for (next = 0; (language = mprGetNextItem(list, &next)) != 0; ) {
            if ((lang = mprLookupKey(rx->route->languages, &language[4])) != 0) {
                rx->lang = lang;
                return lang;
            }
        }
    }
    if (defaultLang && (lang = mprLookupKey(rx->route->languages, defaultLang)) != 0) {
        rx->lang = lang;
        return lang;
    }
    return 0;
}


/*
    Trim extra path information after the uri extension. This is used by CGI and PHP only. The strategy is to
    heuristically find the script name in the uri. This is assumed to be the original uri up to and including
    first path component containing a "." Any path information after that is regarded as extra path.
    WARNING: Extra path is an old, unreliable, CGI specific technique. Do not use directories with embedded periods.
 */
PUBLIC void httpTrimExtraPath(HttpStream *stream)
{
    HttpRx      *rx;
    char        *cp, *extra;
    ssize       len;

    rx = stream->rx;
    if (!(rx->flags & (HTTP_OPTIONS | HTTP_TRACE))) {
        if ((cp = schr(rx->pathInfo, '.')) != 0 && (extra = schr(cp, '/')) != 0) {
            len = extra - rx->pathInfo;
            if (0 < len && len < slen(rx->pathInfo)) {
                rx->extraPath = sclone(&rx->pathInfo[len]);
                rx->pathInfo = snclone(rx->pathInfo, len);
            }
        }
        if ((cp = schr(rx->target, '.')) != 0 && (extra = schr(cp, '/')) != 0) {
            len = extra - rx->target;
            if (0 < len && len < slen(rx->target)) {
                rx->target[len] = '\0';
            }
        }
    }
}


PUBLIC void httpParseMethod(HttpStream *stream)
{
    HttpRx      *rx;
    cchar       *method;
    int         methodFlags;

    rx = stream->rx;
    method = rx->method;
    methodFlags = 0;

    switch (method[0]) {
    case 'D':
        if (strcmp(method, "DELETE") == 0) {
            methodFlags = HTTP_DELETE;
        }
        break;

    case 'G':
        if (strcmp(method, "GET") == 0) {
            methodFlags = HTTP_GET;
        }
        break;

    case 'H':
        if (strcmp(method, "HEAD") == 0) {
            methodFlags = HTTP_HEAD;
        }
        break;

    case 'O':
        if (strcmp(method, "OPTIONS") == 0) {
            methodFlags = HTTP_OPTIONS;
        }
        break;

    case 'P':
        if (strcmp(method, "POST") == 0) {
            methodFlags = HTTP_POST;
            rx->needInputPipeline = 1;

        } else if (strcmp(method, "PUT") == 0) {
            methodFlags = HTTP_PUT;
            rx->needInputPipeline = 1;
        }
        break;

    case 'T':
        if (strcmp(method, "TRACE") == 0) {
            methodFlags = HTTP_TRACE;
        }
        break;
    }
    rx->flags |= methodFlags;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/session.c ************/

/**
    session.c - Session data storage

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/



/********************************** Forwards  *********************************/

static cchar *createSecurityToken(HttpStream *stream);
static void manageSession(HttpSession *sp, int flags);

/************************************* Code ***********************************/
/*
    Allocate a http session state object. This keeps a local hash for session state items.
    This is written via httpWriteSession to the backend session state store.
 */
static HttpSession *allocSessionObj(HttpStream *stream, cchar *id, cchar *data)
{
    HttpSession *sp;

    assert(stream);
    assert(id && *id);
    assert(stream->http);
    assert(stream->http->sessionCache);

    if ((sp = mprAllocObj(HttpSession, manageSession)) == 0) {
        return 0;
    }
    sp->lifespan = stream->limits->sessionTimeout;
    sp->id = sclone(id);
    sp->cache = stream->http->sessionCache;
    if (data) {
        sp->data = mprDeserialize(data);
    }
    if (!sp->data) {
        sp->data = mprCreateHash(ME_MAX_SESSION_HASH, 0);
    }
    return sp;
}


PUBLIC bool httpLookupSessionID(cchar *id)
{
    return mprLookupCache(HTTP->sessionCache, id, 0, 0) != 0;
}


/*
    Public API to create or re-create a session. Always returns with a new session store.
 */
PUBLIC HttpSession *httpCreateSession(HttpStream *stream)
{
    httpDestroySession(stream);
    return httpGetSession(stream, 1);
}


PUBLIC void httpSetSessionNotify(MprCacheProc callback)
{
    mprSetCacheNotify(HTTP->sessionCache, callback);
}


PUBLIC void httpDestroySession(HttpStream *stream)
{
    Http        *http;
    HttpRx      *rx;
    HttpSession *sp;
    cchar       *cookie;

    http = stream->http;
    rx = stream->rx;
    assert(http);

    lock(http);
    if ((sp = httpGetSession(stream, 0)) != 0) {
        cookie = rx->route->cookie ? rx->route->cookie : HTTP_SESSION_COOKIE;
        httpRemoveCookie(stream, cookie);
        mprExpireCacheItem(sp->cache, sp->id, 0);
        sp->id = 0;
        rx->session = 0;
    }
    rx->sessionProbed = 0;
    unlock(http);
}


static void manageSession(HttpSession *sp, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(sp->id);
        mprMark(sp->cache);
        mprMark(sp->data);
    }
}


/*
    Optionally create if "create" is true. Will not re-create.
 */
PUBLIC HttpSession *httpGetSession(HttpStream *stream, int create)
{
    Http        *http;
    HttpNet     *net;
    HttpRx      *rx;
    HttpRoute   *route;
    MprTicks    lifespan;
    cchar       *cookie, *data, *id, *url;
    static int  seqno = 0;
    int         flags, thisSeqno, activeSessions, addressSeqno;

    assert(stream);
    net = stream->net;
    rx = stream->rx;
    route = rx->route;
    http = stream->http;
    assert(rx);

    addressSeqno = net->address ? net->address->seqno : 0;
    if (!rx->session) {
        if ((id = httpGetSessionID(stream)) != 0) {
            if ((data = mprReadCache(stream->http->sessionCache, id, 0, 0)) != 0) {
                rx->session = allocSessionObj(stream, id, data);
                rx->traceId = sfmt("%d-%d-%llu-%d", seqno, rx->session->seqno, stream->net->seqno, rx->seqno);
            }
        }
        if (!rx->session && create) {
            lock(http);
            thisSeqno = ++seqno;
            id = sfmt("%08x%08x%d", PTOI(stream->seqno) + PTOI(stream), (int) mprGetTicks(), thisSeqno);
            id = mprGetMD5WithPrefix(id, slen(id), "-http.session-");
            id = sfmt("%d%s", thisSeqno, mprGetMD5WithPrefix(id, slen(id), "::http.session::"));

            mprGetCacheStats(http->sessionCache, &activeSessions, NULL);
            if (activeSessions >= stream->limits->sessionMax) {
                unlock(http);
                httpLimitError(stream, HTTP_CODE_SERVICE_UNAVAILABLE,
                    "Too many sessions %d/%d", activeSessions, stream->limits->sessionMax);
                return 0;
            }
            unlock(http);

            rx->session = allocSessionObj(stream, id, NULL);
            rx->traceId = sfmt("%d-%d-%llu-%d", addressSeqno, rx->session->seqno, stream->net->seqno, rx->seqno);
            flags = (route->flags & HTTP_ROUTE_VISIBLE_SESSION) ? 0 : HTTP_COOKIE_HTTP;
            if (stream->secure) {
                flags |= HTTP_COOKIE_SECURE;
            }
            if (route->flags & HTTP_ROUTE_NONE_COOKIE) {
                flags |= HTTP_COOKIE_SAME_NONE;
            } else if (route->flags & HTTP_ROUTE_LAX_COOKIE) {
                flags |= HTTP_COOKIE_SAME_LAX;
            } else if (route->flags & HTTP_ROUTE_STRICT_COOKIE) {
                flags |= HTTP_COOKIE_SAME_STRICT;
            }
            cookie = route->cookie ? route->cookie : HTTP_SESSION_COOKIE;
            lifespan = (route->flags & HTTP_ROUTE_PERSIST_COOKIE) ? rx->session->lifespan : 0;
            url = (route->prefix && *route->prefix) ? route->prefix : "/";
            httpSetCookie(stream, cookie, rx->session->id, url, NULL, lifespan, flags);
            httpLog(stream->trace, "session.create", "context", "cookie:%s, session:%s", cookie, rx->session->id);

            if ((route->flags & HTTP_ROUTE_XSRF) && rx->securityToken) {
                httpSetSessionVar(stream, ME_XSRF_COOKIE, rx->securityToken);
            }
        }
    }
    return rx->session;
}


PUBLIC MprHash *httpGetSessionObj(HttpStream *stream, cchar *key)
{
    HttpSession *sp;
    MprKey      *kp;

    assert(stream);
    assert(key && *key);

    if ((sp = httpGetSession(stream, 0)) != 0) {
        if ((kp = mprLookupKeyEntry(sp->data, key)) != 0) {
            return mprDeserialize(kp->data);
        }
    }
    return 0;
}


PUBLIC cchar *httpGetSessionVar(HttpStream *stream, cchar *key, cchar *defaultValue)
{
    HttpSession *sp;
    MprKey      *kp;
    cchar       *result;

    assert(stream);
    assert(key && *key);

    result = 0;
    if ((sp = httpGetSession(stream, 0)) != 0) {
        if ((kp = mprLookupKeyEntry(sp->data, key)) != 0) {
            if (kp->type == MPR_JSON_OBJ) {
                /* Wrong type */
                mprDebug("http session", 0, "Session var is an object");
                return defaultValue;
            } else {
                result = kp->data;
            }
        }
    }
    return result ? result : defaultValue;
}


PUBLIC int httpSetSessionObj(HttpStream *stream, cchar *key, MprHash *obj)
{
    HttpSession *sp;

    assert(stream);
    assert(key && *key);

    if ((sp = httpGetSession(stream, 1)) == 0) {
        return MPR_ERR_CANT_FIND;
    }
    if (obj == 0) {
        httpRemoveSessionVar(stream, key);
    } else {
        mprAddKey(sp->data, key, mprSerialize(obj, 0));
    }
    sp->dirty = 1;
    return 0;
}


/*
    Set a session variable. This will create the session store if it does not already exist.
    Note: If the headers have been emitted, the chance to set a cookie header has passed. So this value will go
    into a session that will be lost. Solution is for apps to create the session first.
    Value of null means remove the session.
 */
PUBLIC int httpSetSessionVar(HttpStream *stream, cchar *key, cchar *value)
{
    HttpSession  *sp;

    assert(stream);
    assert(key && *key);

    if ((sp = httpGetSession(stream, 1)) == 0) {
        return MPR_ERR_CANT_FIND;
    }
    if (value == 0) {
        httpRemoveSessionVar(stream, key);
    } else {
        mprAddKey(sp->data, key, sclone(value));
    }
    sp->dirty = 1;
    return 0;
}


PUBLIC int httpSetSessionLink(HttpStream *stream, void *link)
{
    HttpSession  *sp;

    assert(stream);

    if ((sp = httpGetSession(stream, 1)) == 0) {
        return MPR_ERR_CANT_FIND;
    }
    mprSetCacheLink(sp->cache, sp->id, link);
    return 0;
}


PUBLIC int httpRemoveSessionVar(HttpStream *stream, cchar *key)
{
    HttpSession  *sp;

    assert(stream);
    assert(key && *key);

    if ((sp = httpGetSession(stream, 0)) == 0) {
        return 0;
    }
    sp->dirty = 1;
    return mprRemoveKey(sp->data, key);
}


PUBLIC int httpWriteSession(HttpStream *stream)
{
    HttpSession     *sp;

    if ((sp = stream->rx->session) != 0) {
        if (sp->dirty) {
            if (mprWriteCache(sp->cache, sp->id, mprSerialize(sp->data, 0), 0, sp->lifespan, 0, MPR_CACHE_SET) == 0) {
                mprLog("error http session", 0, "Cannot persist session cache");
                return MPR_ERR_CANT_WRITE;
            }
            sp->dirty = 0;
        }
    }
    return 0;
}


PUBLIC cchar *httpGetSessionID(HttpStream *stream)
{
    HttpRx  *rx;
    cchar   *cookie;

    assert(stream);
    rx = stream->rx;
    assert(rx);

    if (rx->session) {
        assert(rx->session->id);
        assert(*rx->session->id);
        return rx->session->id;
    }
    if (rx->sessionProbed) {
        return 0;
    }
    rx->sessionProbed = 1;
    cookie = rx->route->cookie ? rx->route->cookie : HTTP_SESSION_COOKIE;
    return httpGetCookie(stream, cookie);
}


/*
    Create a security token to use to mitiate CSRF threats. Security tokens are expected to be sent with POST requests to
    verify the request is not being forged.

    Note: the HttpSession API prevents session hijacking by pairing with the client IP
 */
static cchar *createSecurityToken(HttpStream *stream)
{
    HttpRx      *rx;

    rx = stream->rx;
    if (!rx->securityToken) {
        rx->securityToken = mprGetRandomString(32);
    }
    return rx->securityToken;
}


/*
    Get the security token from the session. Create one if one does not exist. Store the token in session store.
    Recreate if required.
 */
PUBLIC cchar *httpGetSecurityToken(HttpStream *stream, bool recreate)
{
    HttpRx      *rx;

    rx = stream->rx;

    if (recreate) {
        rx->securityToken = 0;
    } else {
        rx->securityToken = (char*) httpGetSessionVar(stream, ME_XSRF_COOKIE, 0);
    }
    if (rx->securityToken == 0) {
        createSecurityToken(stream);
        httpSetSessionVar(stream, ME_XSRF_COOKIE, rx->securityToken);
    }
    return rx->securityToken;
}


/*
    Add the security token to a XSRF cookie and response header
    Set recreate to true to force a recreation of the token.
 */
PUBLIC int httpAddSecurityToken(HttpStream *stream, bool recreate)
{
    HttpRoute   *route;
    cchar       *securityToken, *url;
    int         flags;

    route = stream->rx->route;
    securityToken = httpGetSecurityToken(stream, recreate);
    url = (route->prefix && *route->prefix) ? route->prefix : "/";
    flags = (route->flags & HTTP_ROUTE_VISIBLE_SESSION) ? 0 : HTTP_COOKIE_HTTP;
    if (stream->secure) {
        flags |= HTTP_COOKIE_SECURE;
    }
    httpSetCookie(stream, ME_XSRF_COOKIE, securityToken, url, NULL,  0, flags);
    httpSetHeaderString(stream, ME_XSRF_HEADER, securityToken);
    return 0;
}


/*
    Check the security token with the request. This must match the last generated token stored in the session state.
    It is expected the client will set the X-XSRF-TOKEN header with the token.
 */
PUBLIC bool httpCheckSecurityToken(HttpStream *stream)
{
    cchar   *requestToken, *sessionToken;

    if ((sessionToken = httpGetSessionVar(stream, ME_XSRF_COOKIE, 0)) != 0) {
        requestToken = httpGetHeader(stream, ME_XSRF_HEADER);
        if (!requestToken) {
            requestToken = httpGetParam(stream, ME_XSRF_PARAM, 0);
            if (!requestToken) {
                httpLog(stream->trace, "session.xsrf.error", "error", "msg:Missing security token in request");
            }
        }
        if (!smatch(sessionToken, requestToken)) {
            /*
                Potential CSRF attack. Deny request. Re-create a new security token so legitimate clients can retry.
             */
            httpLog(stream->trace, "session.xsrf.error", "error",
                "msg:Security token in request does not match session token, xsrf:%s, sessionXsrf:%s",
                requestToken, sessionToken);
            httpAddSecurityToken(stream, 1);
            return 0;
        }
    }
    return 1;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/stage.c ************/

/*
    stage.c -- Stages are the building blocks of the Http request pipeline.

    Stages support the extensible and modular processing of HTTP requests. Handlers are a kind of stage that are the
    first line processing of a request. Connectors are the last stage in a chain to send/receive data over a network.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/********************************* Forwards ***********************************/

static void manageStage(HttpStage *stage, int flags);

/*********************************** Code *************************************/

PUBLIC HttpStage *httpCreateStage(cchar *name, int flags, MprModule *module)
{
    HttpStage     *stage;

    assert(name && *name);

    if ((stage = httpLookupStage(name)) != 0) {
        if (!(stage->flags & HTTP_STAGE_UNLOADED)) {
            mprLog("error http", 0, "Stage %s already exists", name);
            return 0;
        }
    } else if ((stage = mprAllocObj(HttpStage, manageStage)) == 0) {
        return 0;
    }
    stage->flags = flags;
    stage->name = sclone(name);
    stage->incoming = httpDefaultIncoming;
    stage->outgoing = httpDefaultOutgoing;
    stage->module = module;
    httpAddStage(stage);
    return stage;
}


static void manageStage(HttpStage *stage, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(stage->name);
        mprMark(stage->path);
        mprMark(stage->stageData);
        mprMark(stage->module);
        mprMark(stage->extensions);
    }
}


PUBLIC HttpStage *httpCloneStage(HttpStage *stage)
{
    HttpStage   *clone;

    if ((clone = mprAllocObj(HttpStage, manageStage)) == 0) {
        return 0;
    }
    *clone = *stage;
    return clone;
}


PUBLIC HttpStage *httpCreateHandler(cchar *name, MprModule *module)
{
    return httpCreateStage(name, HTTP_STAGE_HANDLER, module);
}

/*
    Put packets on the service queue.
 */
PUBLIC void httpDefaultOutgoing(HttpQueue *q, HttpPacket *packet)
{
    if (q->service) {
        httpPutForService(q, packet, HTTP_SCHEDULE_QUEUE);
    } else {
        httpPutPacketToNext(q, packet);
    }
}


/*
    Default incoming data routine for all stages. Transfer the data upstream to the next stage.
    This will join incoming packets at the queue head. Most handlers which are also queue heads use this routine.
 */
PUBLIC void httpDefaultIncoming(HttpQueue *q, HttpPacket *packet)
{
    HttpStream  *stream;

    assert(q);
    assert(packet);
    stream = q->stream;

    if (q->flags & HTTP_QUEUE_HEAD) {
        /*
            End of the pipeline
         */
        if (packet->flags & HTTP_PACKET_END) {
            httpPutForService(q, packet, HTTP_SCHEDULE_QUEUE);
            httpFinalizeInput(stream);

        } else if (packet->flags & HTTP_PACKET_SOLO) {
            httpPutForService(q, packet, HTTP_SCHEDULE_QUEUE);

        } else {
            httpJoinPacketForService(q, packet, HTTP_SCHEDULE_QUEUE);
        }
        HTTP_NOTIFY(stream, HTTP_EVENT_READABLE, 0);

    } else {
        if (q->service) {
            httpPutForService(q, packet, HTTP_SCHEDULE_QUEUE);
        } else {
            httpPutPacketToNext(q, packet);
        }
    }
}


/*
    Default service callback. Copy packets to the next stage if flow-control permits.
    This will resume upstream queues and suspend the "q" if required.
 */
PUBLIC void httpDefaultService(HttpQueue *q)
{
    HttpPacket    *packet;

    if (q->direction == HTTP_QUEUE_RX && q->flags & HTTP_QUEUE_HEAD) {
        return;
    }
    for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
        if (!httpWillNextQueueAcceptPacket(q, packet)) {
            httpPutBackPacket(q, packet);
            break;
        }
        httpPutPacketToNext(q, packet);
    }
}


PUBLIC void httpDiscardService(HttpQueue *q)
{
    HttpPacket  *packet;

    for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) { }
}


/*
    The Queue head stage accepts incoming body data and initiates routing for form requests
 */
PUBLIC int httpOpenQueueHead()
{
    HttpStage   *stage;

    if ((stage = httpCreateStage("QueueHead", 0, NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    HTTP->queueHead = stage;
    stage->flags |= HTTP_STAGE_QHEAD | HTTP_STAGE_INTERNAL;
    stage->incoming = httpDefaultIncoming;
    return 0;
}


PUBLIC HttpStage *httpCreateFilter(cchar *name, MprModule *module)
{
    return httpCreateStage(name, HTTP_STAGE_FILTER, module);
}


PUBLIC HttpStage *httpCreateConnector(cchar *name, MprModule *module)
{
    return httpCreateStage(name, HTTP_STAGE_CONNECTOR, module);
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/stream.c ************/

/*
    stream.c -- Request / Response Stream module

    Streams are multiplexed on top of HttpNet connections.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */
/********************************* Includes ***********************************/



/********************************** Locals ************************************/
/*
    Invocation structure for httpCreateEvent
 */
typedef struct HttpInvoke {
    HttpEventProc   callback;
    uint64          seqno;          // Stream sequence number
    void            *data;          //  User data - caller must free if required in callback
    int             hasRun;
} HttpInvoke;

/***************************** Forward Declarations ***************************/

static void prepareStream(HttpStream *stream, MprHash *headers);
static void createInvokeEvent(HttpStream *stream, void *data);
static void manageInvoke(HttpInvoke *event, int flags);
static void manageStream(HttpStream *stream, int flags);
static void resetQueues(HttpStream *stream);

/*********************************** Code *************************************/
/*
    Create a new stream object. These are multiplexed onto network objects.

    Use httpCreateNet() to create a network object.
 */

PUBLIC HttpStream *httpCreateStream(HttpNet *net, bool peerCreated)
{
    Http        *http;
    HttpStream  *stream;
    HttpLimits  *limits;
    HttpHost    *host;
    HttpRoute   *route;

    assert(net);
    http = HTTP;
    if ((stream = mprAllocObj(HttpStream, manageStream)) == 0) {
        return 0;
    }
    stream->http = http;
    stream->port = -1;
    stream->started = http->now;
    stream->lastActivity = http->now;
    stream->net = net;
    stream->endpoint = net->endpoint;
    stream->sock = net->sock;
    stream->port = net->port;
    stream->ip = net->ip;
    stream->secure = net->secure;
    stream->peerCreated = peerCreated;
    if (stream->endpoint) {
        stream->notifier = net->endpoint->notifier;
    }
    if (net->endpoint) {
        host = mprGetFirstItem(net->endpoint->hosts);
        if (host && (route = host->defaultRoute) != 0) {
            stream->limits = route->limits;
            stream->trace = route->trace;
        } else {
            stream->limits = http->serverLimits;
            stream->trace = http->trace;
        }
    } else {
        stream->limits = http->clientLimits;
        stream->trace = http->trace;
    }
    limits = stream->limits;

    stream->keepAliveCount = (net->protocol >= 2) ? 0 : stream->limits->keepAliveMax;
    stream->dispatcher = net->dispatcher;

    resetQueues(stream);

#if ME_HTTP_HTTP2
    stream->h2State = HTTP2_STATE_IDLE;
    if (!peerCreated && ((net->ownStreams >= limits->txStreamsMax) || (net->ownStreams >= limits->streamsMax))) {
        httpNetError(net, "Attempting to create too many streams for network connection: %d/%d/%d", net->ownStreams,
            limits->txStreamsMax, limits->streamsMax);
        return 0;
    }
#endif
    httpSetState(stream, HTTP_STATE_BEGIN);

    lock(http);
    stream->seqno = ++http->totalStreams;
    unlock(http);

    httpAddStream(net, stream);
    if (!peerCreated) {
        net->ownStreams++;
    }
    return stream;
}


/*
    Destroy a stream. This removes the stream from the list of streams.
 */
PUBLIC void httpDestroyStream(HttpStream *stream)
{
    if (!stream->destroyed && !stream->net->borrowed) {
        HTTP_NOTIFY(stream, HTTP_EVENT_DESTROY, 0);
        if (stream->tx) {
            httpClosePipeline(stream);
        }
        if (!stream->peerCreated) {
            stream->net->ownStreams--;
        }
        if (stream->streamID == stream->net->lastStreamID) {
            stream->net->lastStreamID++;
        }
        if (stream->counted) {
            httpMonitorEvent(stream, HTTP_COUNTER_ACTIVE_REQUESTS, -1);
            stream->counted = 0;
        }
        httpRemoveStream(stream->net, stream);
        stream->state = HTTP_STATE_COMPLETE;
        stream->destroyed = 1;
    }
}


static void manageStream(HttpStream *stream, int flags)
{
    assert(stream);

    if (flags & MPR_MANAGE_MARK) {
        mprMark(stream->authType);
        mprMark(stream->authData);
        mprMark(stream->boundary);
        mprMark(stream->context);
        mprMark(stream->data);
        mprMark(stream->dispatcher);
        mprMark(stream->endpoint);
        mprMark(stream->errorMsg);
        mprMark(stream->grid);
        mprMark(stream->headersCallbackArg);
        mprMark(stream->http);
        mprMark(stream->host);
        mprMark(stream->inputq);
        mprMark(stream->ip);
        mprMark(stream->limits);
        mprMark(stream->mark);
        mprMark(stream->net);
        mprMark(stream->outputq);
        mprMark(stream->password);
        mprMark(stream->pool);
        mprMark(stream->protocols);
        mprMark(stream->readq);
        mprMark(stream->record);
        mprMark(stream->reqData);
        mprMark(stream->rx);
        mprMark(stream->rxHead);
        mprMark(stream->sock);
        mprMark(stream->timeoutEvent);
        mprMark(stream->trace);
        mprMark(stream->tx);
        mprMark(stream->txHead);
        mprMark(stream->user);
        mprMark(stream->username);
        mprMark(stream->writeq);
#if DEPRECATED
        mprMark(stream->ejs);
#endif
    }
}


/*
    Prepare for another request for server
    Return true if there is another request ready for serving
 */
PUBLIC void httpResetServerStream(HttpStream *stream)
{
    assert(httpServerStream(stream));
    assert(stream->state == HTTP_STATE_COMPLETE);

    if (stream->net->borrowed) {
        return;
    }
    if (stream->keepAliveCount <= 0) {
        stream->state = HTTP_STATE_BEGIN;
        return;
    }
    HTTP_NOTIFY(stream, HTTP_EVENT_DESTROY, 0);

    if (stream->tx) {
        stream->tx->stream = 0;
    }
    if (stream->rx) {
        stream->rx->stream = 0;
    }
    stream->authType = 0;
    stream->username = 0;
    stream->password = 0;
    stream->user = 0;
    stream->authData = 0;
    stream->encoded = 0;

    prepareStream(stream, NULL);
}


PUBLIC void httpResetClientStream(HttpStream *stream, bool keepHeaders)
{
    assert(stream);

    if (stream->net->protocol < 2) {
        if (stream->state > HTTP_STATE_BEGIN && stream->state < HTTP_STATE_COMPLETE &&
                stream->keepAliveCount > 0 && stream->sock && !httpIsEof(stream)) {
            /* Residual data from past request, cannot continue on this socket */
            stream->sock = 0;
        }
    }
    if (stream->tx) {
        stream->tx->stream = 0;
    }
    if (stream->rx) {
        stream->rx->stream = 0;
    }
    prepareStream(stream, (keepHeaders && stream->tx) ? stream->tx->headers: NULL);
}


static void prepareStream(HttpStream *stream, MprHash *headers)
{
    stream->tx = httpCreateTx(stream, headers);
    stream->rx = httpCreateRx(stream);

    if (stream->timeoutEvent) {
        mprRemoveEvent(stream->timeoutEvent);
        stream->timeoutEvent = 0;
    }
    stream->started = stream->http->now;
    stream->lastActivity = stream->http->now;
    stream->error = 0;
    stream->errorMsg = 0;
    stream->h2State = 0;
    stream->authRequested = 0;
    stream->completed = 0;
    stream->destroyed = 0;
    stream->streamID = 0;
    stream->state = 0;

    resetQueues(stream);
    httpSetState(stream, HTTP_STATE_BEGIN);
}


static void resetQueues(HttpStream *stream)
{
    HttpQueue   *q, *tailq;
    HttpNet     *net;
    HttpLimits  *limits;
    Http        *http;

    net = stream->net;
    http = net->http;
    limits = stream->limits;

    stream->rx = httpCreateRx(stream);
    stream->tx = httpCreateTx(stream, NULL);

    /*
        Create the queues with the default (mandatory) filters.
     */
    q = stream->rxHead = httpCreateQueue(net, stream, http->queueHead, HTTP_QUEUE_RX, NULL);
    q = tailq = httpCreateQueue(net, stream, http->tailFilter, HTTP_QUEUE_RX, q);
    if (net->protocol < 2) {
        q = httpCreateQueue(net, stream, http->chunkFilter, HTTP_QUEUE_RX, q);
    }
    stream->inputq = tailq;
    stream->readq = stream->rxHead;

    /*
        Create the TX queue head
     */
    q = stream->txHead = httpCreateQueue(net, stream, http->queueHead, HTTP_QUEUE_TX, NULL);
    if (net->protocol < 2) {
        q = httpCreateQueue(net, stream, http->chunkFilter, HTTP_QUEUE_TX, q);
    } else {
        stream->rx->remainingContent = HTTP_UNLIMITED;
    }
    q = tailq = httpCreateQueue(net, stream, http->tailFilter, HTTP_QUEUE_TX, q);
    stream->outputq = tailq;
    stream->writeq = stream->txHead;

#if ME_HTTP_HTTP2
    /*
        The stream->outputq queue window limit is updated on receipt of the peer settings frame and this defines the maximum amount of
        data we can send without receipt of a window flow control update message.
        The stream->inputq window is defined by net->limits and will be
     */
    httpSetQueueLimits(stream->inputq, limits, -1, -1, -1, -1);
    httpSetQueueLimits(stream->outputq, limits, -1, -1, -1, net->window);
#endif
    httpOpenQueues(stream);
}


/*
    This will disconnect the socket for HTTP/1 only. HTTP/2 streams are destroyed for each request.
    For HTTP/1 they are reused via keep-alive.
 */
PUBLIC void httpDisconnectStream(HttpStream *stream)
{
    HttpRx      *rx;
    HttpTx      *tx;

    rx = stream->rx;
    tx = stream->tx;

    stream->error++;
    if (tx) {
        tx->responded = 1;
        tx->finalizedOutput = 1;
        tx->finalizedConnector = 1;
        httpFinalize(stream);
    }
    if (rx && !rx->eof) {
        httpSetEof(stream);
    }
    if (stream->net->protocol < 2) {
        mprDisconnectSocket(stream->sock);
    }
}


static void streamTimeout(HttpStream *stream, MprEvent *mprEvent)
{
    HttpLimits  *limits;
    cchar       *event, *msg, *prefix;
    int         status;

    if (stream->destroyed) {
        return;
    }
    assert(stream->tx);
    assert(stream->rx);
    limits = stream->limits;

    msg = 0;
    event = 0;

    if (stream->timeoutCallback) {
        (stream->timeoutCallback)(stream);
        HTTP_NOTIFY(stream, HTTP_EVENT_TIMEOUT, 0);
    }

    prefix = (stream->state == HTTP_STATE_BEGIN) ? "Idle stream" : "Request";
    if (stream->timeout == HTTP_PARSE_TIMEOUT) {
        msg = sfmt("%s exceeded parse headers timeout of %lld sec", prefix, limits->requestParseTimeout  / 1000);
        event = "timeout.parse";

    } else if (stream->timeout == HTTP_INACTIVITY_TIMEOUT) {
        if (httpClientStream(stream) /* too noisy || (stream->rx && stream->rx->uri) */) {
            msg = sfmt("%s exceeded inactivity timeout of %lld sec", prefix, limits->inactivityTimeout / 1000);
            event = "timeout.inactivity";
        }

    } else if (stream->timeout == HTTP_REQUEST_TIMEOUT) {
        msg = sfmt("%s exceeded timeout %lld sec", prefix, limits->requestTimeout / 1000);
        event = "timeout.duration";
    }
    if (stream->state < HTTP_STATE_FIRST) {
        /*
            Connection is idle
         */
        if (msg && event) {
            httpLog(stream->trace, event, "error", "msg:%s", msg);
            stream->errorMsg = msg;
        }
        httpDisconnectStream(stream);
    } else {
        /*
            For HTTP/2, we error and complete the stream and keep the connection open
            For HTTP/1, we close the connection as the request is partially complete
         */
        status = HTTP_CODE_REQUEST_TIMEOUT | (stream->net->protocol < 2 ? HTTP_CLOSE : 0);
        httpError(stream, status, "%s", msg ? msg : "Timeout");
    }
}


PUBLIC void httpStreamTimeout(HttpStream *stream)
{
    if (!stream->timeoutEvent && !stream->destroyed) {
        /*
            Will run on the HttpStream dispatcher unless shutting down and it is destroyed already
         */
        assert(!(stream->dispatcher->flags & MPR_DISPATCHER_DESTROYED));
        stream->timeoutEvent = mprCreateLocalEvent(stream->dispatcher, "streamTimeout", 0, streamTimeout, stream, 0);
    }
}


PUBLIC void httpFollowRedirects(HttpStream *stream, bool follow)
{
    stream->followRedirects = follow;
}


PUBLIC ssize httpGetChunkSize(HttpStream *stream)
{
    if (stream->tx) {
        return stream->tx->chunkSize;
    }
    return 0;
}


PUBLIC void *httpGetStreamContext(HttpStream *stream)
{
    return stream->context;
}


PUBLIC void *httpGetStreamHost(HttpStream *stream)
{
    return stream->host;
}


PUBLIC ssize httpGetWriteQueueCount(HttpStream *stream)
{
    return stream->writeq ? stream->writeq->count : 0;
}


PUBLIC void httpResetCredentials(HttpStream *stream)
{
    stream->authType = 0;
    stream->username = 0;
    stream->password = 0;
    httpRemoveHeader(stream, "Authorization");
}


PUBLIC void httpSetStreamNotifier(HttpStream *stream, HttpNotifier notifier)
{
    stream->notifier = notifier;
    /*
        Only issue a readable event if streaming or already routed
     */
    if (stream->readq->first && stream->rx->route) {
        HTTP_NOTIFY(stream, HTTP_EVENT_READABLE, 0);
    }
}


/*
    Password and authType can be null
    User may be a combined user:password
 */
PUBLIC void httpSetCredentials(HttpStream *stream, cchar *username, cchar *password, cchar *authType)
{
    char    *ptok;

    httpResetCredentials(stream);
    if (password == NULL && strchr(username, ':') != 0) {
        stream->username = ssplit(sclone(username), ":", &ptok);
        stream->password = sclone(ptok);
    } else {
        stream->username = sclone(username);
        stream->password = sclone(password);
    }
    if (authType) {
        stream->authType = sclone(authType);
    }
}


PUBLIC void httpSetKeepAliveCount(HttpStream *stream, int count)
{
    stream->keepAliveCount = count;
}


PUBLIC void httpSetChunkSize(HttpStream *stream, ssize size)
{
    if (stream->tx) {
        stream->tx->chunkSize = size;
    }
}


PUBLIC void httpSetHeadersCallback(HttpStream *stream, HttpHeadersCallback fn, void *arg)
{
    stream->headersCallback = fn;
    stream->headersCallbackArg = arg;
}


PUBLIC void httpSetStreamContext(HttpStream *stream, void *context)
{
    stream->context = context;
}


PUBLIC void httpSetStreamHost(HttpStream *stream, void *host)
{
    stream->host = host;
}


PUBLIC void httpNotify(HttpStream *stream, int event, int arg)
{
    if (stream->notifier) {
        (stream->notifier)(stream, event, arg);
    }
}


/*
    Set each timeout arg to -1 to skip. Set to zero for no timeout. Otherwise set to number of msecs.
 */
PUBLIC void httpSetTimeout(HttpStream *stream, MprTicks requestTimeout, MprTicks inactivityTimeout)
{
    if (requestTimeout >= 0) {
        if (requestTimeout == 0) {
            stream->limits->requestTimeout = HTTP_UNLIMITED;
        } else {
            stream->limits->requestTimeout = requestTimeout;
        }
    }
    if (inactivityTimeout >= 0) {
        if (inactivityTimeout == 0) {
            stream->limits->inactivityTimeout = HTTP_UNLIMITED;
            stream->net->limits->inactivityTimeout = HTTP_UNLIMITED;
        } else {
            stream->limits->inactivityTimeout = inactivityTimeout;
            stream->net->limits->inactivityTimeout = inactivityTimeout;
        }
    }
}


PUBLIC HttpLimits *httpSetUniqueStreamLimits(HttpStream *stream)
{
    HttpLimits      *limits;

    if ((limits = mprAllocStruct(HttpLimits)) != 0) {
        *limits = *stream->limits;
        stream->limits = limits;
    }
    return limits;
}


/*
    Test if a request has expired relative to the default inactivity and request timeout limits.
    Set timeout to a non-zero value to apply an overriding smaller timeout
    Set timeout to a value in msec. If timeout is zero, override default limits and wait forever.
    If timeout is < 0, use default inactivity and duration timeouts.
    If timeout is > 0, then use this timeout as an additional timeout.
 */
PUBLIC bool httpRequestExpired(HttpStream *stream, MprTicks timeout)
{
    HttpLimits  *limits;
    MprTicks    inactivityTimeout, requestTimeout;

    limits = stream->limits;
    if (mprGetDebugMode() || timeout == 0) {
        inactivityTimeout = requestTimeout = MPR_MAX_TIMEOUT;

    } else if (timeout < 0) {
        inactivityTimeout = limits->inactivityTimeout;
        requestTimeout = limits->requestTimeout;

    } else {
        inactivityTimeout = min(limits->inactivityTimeout, timeout);
        requestTimeout = min(limits->requestTimeout, timeout);
    }

    if (mprGetRemainingTicks(stream->started, requestTimeout) < 0) {
        if (requestTimeout != timeout) {
            httpLog(stream->trace, "timeout.duration", "error",
                "msg:Request cancelled exceeded max duration, timeout:%lld", requestTimeout / 1000);
        }
        return 1;
    }
    if (mprGetRemainingTicks(stream->lastActivity, inactivityTimeout) < 0) {
        if (inactivityTimeout != timeout) {
            httpLog(stream->trace, "timeout.inactivity", "error",
                "msg:Request cancelled due to inactivity, timeout:%lld", inactivityTimeout / 1000);
        }
        return 1;
    }
    return 0;
}


PUBLIC void httpSetStreamData(HttpStream *stream, void *data)
{
    stream->data = data;
}


PUBLIC void httpSetStreamReqData(HttpStream *stream, void *data)
{
    stream->reqData = data;
}


/*
    Invoke the callback. This routine is run on the streams dispatcher on an MPR thread.
    If the stream is destroyed, the event will be NULL.
 */
static void invokeWrapper(HttpInvoke *invoke, MprEvent *event)
{
    HttpStream  *stream;

    if (event && (stream = httpFindStream(invoke->seqno, NULL, NULL)) != NULL) {
        invoke->callback(stream, invoke->data);
    } else {
        invoke->callback(NULL, invoke->data);
    }
    invoke->hasRun = 1;
    mprRelease(invoke);
}


/*
    Create an event on a stream identified by its sequence number.
    This routine is foreign thread-safe.
 */
PUBLIC int httpCreateEvent(uint64 seqno, HttpEventProc callback, void *data)
{
    HttpInvoke  *invoke;

    /*
        Must allocate memory immune from GC. The hold is released in the invokeWrapper
        callback which is always invoked eventually.
     */
    if ((invoke = mprAllocMem(sizeof(HttpInvoke), MPR_ALLOC_ZERO | MPR_ALLOC_MANAGER | MPR_ALLOC_HOLD)) != 0) {
        mprSetManager(mprSetAllocName(invoke, "HttpInvoke@" MPR_LOC), (MprManager) manageInvoke);
        invoke->seqno = seqno;
        invoke->callback = callback;
        invoke->data = data;
    }
    /*
        Find the stream by "seqno" and then call createInvokeEvent() to create an MPR event.
     */
    if (httpFindStream(seqno, createInvokeEvent, invoke)) {
        return 0;
    }
    mprRelease(invoke);
    return MPR_ERR_CANT_FIND;
}


/*
    Destructor for Invoke to make sure the callback is always called.
 */
static void manageInvoke(HttpInvoke *invoke, int flags)
{
    if (flags & MPR_MANAGE_FREE) {
        if (!invoke->hasRun) {
            invokeWrapper(invoke, NULL);
        }
    }
}


static void createInvokeEvent(HttpStream *stream, void *data)
{
    mprCreateLocalEvent(stream->dispatcher, "httpEvent", 0, (MprEventProc) invokeWrapper, data, MPR_EVENT_ALWAYS);
}


/*
    If invoking on a foreign thread, provide a callback proc and data so the stream can be safely accessed while locked.
    Do not use the returned value except to test for NULL in a foreign thread.
 */
PUBLIC HttpStream *httpFindStream(uint64 seqno, HttpEventProc proc, void *data)
{
    HttpNet     *net;
    HttpStream  *stream;
    int         nextNet, nextStream;

    if (!proc && !mprGetCurrentThread()) {
        assert(proc || mprGetCurrentThread());
        return NULL;
    }
    /*
        WARNING: GC can be running here in a foreign thread. The locks will ensure that networks and
        streams are not destroyed while locked. Event service lock is needed for the manageDispatcher
        which then marks networks.
     */
    stream = 0;
    lock(MPR->eventService);
    lock(HTTP->networks);
    for (ITERATE_ITEMS(HTTP->networks, net, nextNet)) {
        /*
            This lock prevents the stream being freed and from being removed from HttpNet.networks
         */
        lock(net->streams);
        for (ITERATE_ITEMS(net->streams, stream, nextStream)) {
            if (stream->seqno == seqno && !stream->destroyed) {
                if (proc) {
                    (proc)(stream, data);
                }
                nextNet = HTTP->networks->length;
                break;
            }
        }
        unlock(net->streams);
    }
    unlock(HTTP->networks);
    unlock(MPR->eventService);
    return stream;
}


PUBLIC void httpAddInputEndPacket(HttpStream *stream, HttpQueue *q)
{
    HttpRx  *rx;

    rx = stream->rx;
    if (!rx->inputEnded) {
        rx->inputEnded = 1;
        if (!q) {
            q = stream->inputq;
        }
        httpSetEof(stream);
        httpPutPacket(q, httpCreateEndPacket());
    }
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/tailFilter.c ************/

/*
    tailFilter.c -- Filter for the start/end of request pipeline.

    The TailFilter is the last stage in a request pipline. After this is the Http*Filter and NetConnector.
    This filter multiplexes onto the net->outputq which is the http1Filter or http2Filter.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/********************************** Forwards **********************************/

static bool willQueueAcceptPacket(HttpQueue *q, HttpPacket *packet);
static HttpPacket *createAltBodyPacket(HttpQueue *q);
static void incomingTail(HttpQueue *q, HttpPacket *packet);
static void outgoingTail(HttpQueue *q, HttpPacket *packet);
static void outgoingTailService(HttpQueue *q);

/*********************************** Code *************************************/

PUBLIC int httpOpenTailFilter()
{
    HttpStage     *filter;

    if ((filter = httpCreateFilter("tailFilter", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    HTTP->tailFilter = filter;
    filter->incoming = incomingTail;
    filter->outgoing = outgoingTail;
    filter->outgoingService = outgoingTailService;
    return 0;
}


static void incomingTail(HttpQueue *q, HttpPacket *packet)
{
    HttpStream  *stream;
    HttpRx      *rx;
    ssize       count;

    stream = q->stream;
    rx = stream->rx;

    count = stream->readq->count + httpGetPacketLength(packet);
    if (rx->upload && count >= stream->limits->uploadSize && stream->limits->uploadSize != HTTP_UNLIMITED) {
        httpLimitError(stream, HTTP_CLOSE | HTTP_CODE_REQUEST_TOO_LARGE,
            "Request upload of %d bytes is too big. Limit %lld", (int) count, stream->limits->uploadSize);

    } else if (rx->form && count >= stream->limits->rxFormSize && stream->limits->rxFormSize != HTTP_UNLIMITED) {
        httpLimitError(stream, HTTP_CLOSE | HTTP_CODE_REQUEST_TOO_LARGE,
            "Request form of %d bytes is too big. Limit %lld", (int) count, stream->limits->rxFormSize);

    } else if (count >= stream->limits->rxBodySize && stream->limits->rxBodySize != HTTP_UNLIMITED) {
        httpLimitError(stream, HTTP_CLOSE | HTTP_CODE_REQUEST_TOO_LARGE,
            "Request body of %d bytes is too big. Limit %lld", (int) count, stream->limits->rxFormSize);

    } else {
        httpDefaultIncoming(q, packet);
        if (q->net->eof) {
            httpAddInputEndPacket(stream, q);
        }
    }
}


static void outgoingTail(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;
    HttpStream  *stream;
    HttpTx      *tx;
    HttpPacket  *headers, *tail;

    stream = q->stream;
    tx = stream->tx;
    net = q->net;

    if (httpIsServer(stream) && stream->state < HTTP_STATE_PARSED) {
        assert(stream->state >= HTTP_STATE_PARSED);
        return;
    }
    stream->lastActivity = stream->http->now;

    if (!(tx->flags & HTTP_TX_HEADERS_CREATED)) {
        headers = httpCreateHeaders(q, NULL);
        while (httpGetPacketLength(headers) > net->outputq->packetSize) {
            tail = httpSplitPacket(headers, net->outputq->packetSize);
            httpPutForService(q, headers, 1);
            headers = tail;
        }
        httpPutForService(q, headers, 1);
        if (tx->altBody) {
            httpPutForService(q, createAltBodyPacket(q), 1);
        }
    }
    if (packet->flags & HTTP_PACKET_DATA) {
        tx->bytesWritten += httpGetPacketLength(packet);
        if (tx->bytesWritten > stream->limits->txBodySize) {
            httpLimitError(stream, HTTP_CODE_REQUEST_TOO_LARGE | ((tx->bytesWritten) ? HTTP_ABORT : 0),
                "Http transmission aborted. Exceeded transmission max body of %lld bytes", stream->limits->txBodySize);
        }
    }
    httpPutForService(q, packet, HTTP_SCHEDULE_QUEUE);
}


static void outgoingTailService(HttpQueue *q)
{
    HttpPacket  *packet;

    for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
        if (!willQueueAcceptPacket(q, packet)) {
            httpPutBackPacket(q, packet);
            return;
        }
        //  Onto the HttpFilter
        httpPutPacket(q->net->outputq, packet);
    }
}

/*
    Similar to httpWillQueueAcceptPacket, but also handles HTTP/2 flow control.
    This MUST be done here because this is the last per-stream control point. The Http2Filter has all streams
    multiplexed together.
 */
static bool willQueueAcceptPacket(HttpQueue *q, HttpPacket *packet)
{
    HttpNet     *net;
    HttpStream  *stream;
    HttpQueue   *nextQ;
    HttpPacket  *next, *tail;
    ssize       room, size;
    int         last;

    net = q->net;
    stream = q->stream;

    /*
        Determine if this is the last packet in the HTTP/2 stream.
        If packet is END, or if packet is data and the next packet is END, or packet is header and it is
        followed by NOT a header.

        Should be in HTTP/2. But must not flow control headers.
     */
    next = q->first;
    last = (
        (packet->flags & HTTP_PACKET_HEADER && !(!next || next->flags & HTTP_PACKET_HEADER)) ||
        (packet->flags & HTTP_PACKET_DATA && next && next->flags & HTTP_PACKET_END) ||
        (packet->flags & HTTP_PACKET_END));

    /*
        Get the maximum the output stream can absorb that is less than the downstream
        queue packet size and the per-stream window size if HTTP/2.
     */
    nextQ = stream->net->outputq;
    room = nextQ->max - nextQ->count;
    if (packet->flags & HTTP_PACKET_DATA && net->protocol >= 2) {
        room = min(stream->outputq->window, room);
    }
    size = httpGetPacketLength(packet);
    if (size <= room) {
        //  Packet fits
        packet->last = last;
        return 1;
    }
    if (net->protocol < 2 && (size - room) < HTTP_QUEUE_ALLOW) {
        //  Packet almost fits. Allow for HTTP/1. HTTP/2 we must strictly observe the flow control window.
        packet->last = last;
        return 1;
    }
    if (room > 0) {
        /*
            Resize the packet to fit downstream. This will putback the tail if required.
         */
        tail = httpResizePacket(q, packet, room);
        size = httpGetPacketLength(packet);
        if (size > 0) {
            if (tail == 0) {
                packet->last = last;
            }
            return 1;
        }
    }
    /*
        The downstream queue cannot accept this packet, so suspend this queue and schedule the next if required.
     */
    httpSuspendQueue(q);
    if (!(nextQ->flags & HTTP_QUEUE_SUSPENDED)) {
        httpScheduleQueue(nextQ);
    }
    return 0;
}


/*
    Create an alternate response body for error responses.
 */
static HttpPacket *createAltBodyPacket(HttpQueue *q)
{
    HttpTx      *tx;
    HttpPacket  *packet;

    tx = q->stream->tx;
    packet = httpCreateDataPacket(slen(tx->altBody));
    mprPutStringToBuf(packet->content, tx->altBody);
    return packet;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/trace.c ************/

/*
    trace.c -- Trace data
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.

    HTTP trace is configured per route.

    Event types and trace levels:
    1: debug, error
    2: request, result
    3: headers
    4: context
    5: packet
    6: detail
 */

/********************************* Includes ***********************************/



/********************************* Forwards ***********************************/

static void emitTraceValues(MprBuf *buf, char *str);
static void flushTrace(HttpTrace *trace);
static void formatTrace(HttpTrace *trace, cchar *event, cchar *type, int flags, cchar *buf, ssize len, cchar *fmt, ...);
static cchar *getTraceTag(cchar *event, cchar *type);

static void traceData(HttpTrace *trace, cchar *data, ssize len, int flags);

/*********************************** Code *************************************/

static void manageTrace(HttpTrace *trace, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(trace->buf);
        mprMark(trace->events);
        mprMark(trace->file);
        mprMark(trace->format);
        mprMark(trace->lastTime);
        mprMark(trace->mutex);
        mprMark(trace->parent);
        mprMark(trace->path);
    }
}

/*
    Parent may be null
 */
PUBLIC HttpTrace *httpCreateTrace(HttpTrace *parent)
{
    HttpTrace   *trace;

    if ((trace = mprAllocObj(HttpTrace, manageTrace)) == 0) {
        return 0;
    }
    if (parent) {
        *trace = *parent;
        trace->parent = parent;
    } else {
        if ((trace->events = mprCreateHash(0, MPR_HASH_STATIC_VALUES)) == 0) {
            return 0;
        }
        mprAddKey(trace->events, "debug", ITOP(1));
        mprAddKey(trace->events, "error", ITOP(1));
        mprAddKey(trace->events, "request", ITOP(2));
        mprAddKey(trace->events, "result", ITOP(2));
        mprAddKey(trace->events, "headers", ITOP(3));
        mprAddKey(trace->events, "context", ITOP(4));
        mprAddKey(trace->events, "packet", ITOP(5));
        mprAddKey(trace->events, "detail", ITOP(6));

        /*
            Max log file size
         */
        trace->size = HTTP_TRACE_MAX_SIZE;
        trace->maxContent = MAXINT;
        trace->formatter = httpPrettyFormatter;
        trace->logger = httpWriteTraceLogFile;
        trace->mutex = mprCreateLock();
    }
    return trace;
}


PUBLIC void httpSetTraceContentSize(HttpTrace *trace, ssize size)
{
    trace->maxContent = size;
}


PUBLIC void httpSetTraceEventLevel(HttpTrace *trace, cchar *type, int level)
{
    assert(trace);
    mprAddKey(trace->events, type, ITOP(level));
}


PUBLIC int httpGetTraceLevel(HttpTrace *trace)
{
    return trace->level;
}


PUBLIC void httpSetTraceFormat(HttpTrace *trace, cchar *format)
{
    trace->format = sclone(format);
}


PUBLIC HttpTraceFormatter httpSetTraceFormatter(HttpTrace *trace, HttpTraceFormatter callback)
{
    HttpTraceFormatter  prior;

    prior = trace->formatter;
    trace->formatter = callback;
    return prior;
}


PUBLIC void httpSetTraceFormatterName(HttpTrace *trace, cchar *name)
{
    HttpTraceFormatter  formatter;

    if (name && smatch(name, "common")) {
        if ((trace->events = mprCreateHash(0, MPR_HASH_STATIC_VALUES)) == 0) {
            return;
        }
        mprAddKey(trace->events, "result", ITOP(0));
        formatter = httpCommonFormatter;

    } else if (smatch(name, "pretty")) {
       formatter = httpPrettyFormatter;

    } else if (smatch(name, "detail")) {
       formatter = httpDetailFormatter;
    } else {
        mprLog("trace error", 0, "Unknown trace formatter %s", name)
       formatter = httpDetailFormatter;
    }
    httpSetTraceFormatter(trace, formatter);
}


PUBLIC void httpSetTraceLevel(HttpTrace *trace, int level)
{
    if (level < 0) {
        level = 0;
    } else if (level > 10) {
        level = 10;
    }
    trace->level = level;
}


PUBLIC void httpSetTraceLogger(HttpTrace *trace, HttpTraceLogger callback)
{
    trace->logger = callback;
}


/*
    Inner routine for httpLog()
 */
PUBLIC void httpLogProc(HttpTrace *trace, cchar *event, cchar *type, int flags, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    httpFormatTrace(trace, event, type, flags, NULL, 0, fmt, args);
    va_end(args);
}


PUBLIC void httpLogRxPacket(HttpNet *net, cchar *buf, ssize len)
{
    formatTrace(net->trace, "rx.net", "packet", HTTP_TRACE_HEX, buf, len, "length:%lld", len);
}


PUBLIC void httpLogTxPacket(HttpNet *net, ssize len)
{
    HttpTrace   *trace;
    HttpQueue   *q;
    ssize       bytes;
    int         flags, i;

    q = net->socketq;
    flags = HTTP_TRACE_CONT;
    trace = net->trace;

    if (!net->skipTrace && net->bytesWritten >= trace->maxContent) {
        httpLog(trace, "tx.http1", "packet", "msg:Abbreviating packet trace");
        net->skipTrace = 1;
    }
    if (net->ioFile) {
        formatTrace(trace, "tx.net", "packet", flags, NULL, 0, "path:%s, length:%zd, written:%d\n",
            net->ioFile->path, net->ioCount, len);
    } else {
        formatTrace(trace, "tx.net", "packet", flags, NULL, 0, "length:%zd, written:%d\n", net->ioCount, len);
    }
    flags |= HTTP_TRACE_HEX;
    for (i = 0; i < net->ioIndex; i++) {
        bytes = min(len, net->iovec[i].len);
        formatTrace(trace, "tx.net", "packet", flags, net->iovec[i].start, bytes, NULL);
        len -= bytes;
    }
    if (net->ioFile) {
        //  MOB - need to read file data here
        formatTrace(trace, "tx.net", "packet", flags, "FILE", 4, NULL);
    }
    flushTrace(trace);
}


PUBLIC void httpLogCompleteRequest(HttpStream *stream)
{
    HttpRx      *rx;
    HttpTx      *tx;
    MprTicks    elapsed;
    MprOff      received;

    rx = stream->rx;
    tx = stream->tx;

    elapsed = mprGetTicks() - stream->started;
    if (httpTracing(stream->net)) {
        received = httpGetPacketLength(rx->headerPacket) + rx->bytesRead;
#if MPR_HIGH_RES_TIMER
        formatTrace(stream->trace, "tx.complete", "context", 0, (void*) stream, 0,
            "elapsed:%llu, ticks:%llu, received:%lld, sent:%lld",
            elapsed, mprGetHiResTicks() - stream->startMark, received, tx->bytesWritten);
#else
        formatTrace(stream->trace, "tx.complete", "context", 0, (void*) stream, 0,
            "elapsed:%llu, received:%lld, sent:%lld", elapsed, received, tx->bytesWritten);
#endif
    }
}


/*
    Format and emit trace
 */
PUBLIC void httpFormatTrace(HttpTrace *trace, cchar *event, cchar *type, int flags, cchar *buf, ssize len, cchar *fmt, va_list args)
{
    (trace->formatter)(trace, event, type, flags, buf, len, fmt, args);
}


/*
    Internal format end emit trace with literal args
 */
static void formatTrace(HttpTrace *trace, cchar *event, cchar *type, int flags, cchar *buf, ssize len, cchar *fmt, ...)
{
    va_list    args;

    va_start(args, fmt);
    (trace->formatter)(trace, event, type, flags, buf, len, fmt, args);
    va_end(args);
}


/*
    Low-level write routine to be used only by formatters
 */
PUBLIC void httpWriteTrace(HttpTrace *trace, cchar *buf, ssize len)
{
    (trace->logger)(trace, buf, len);
}


/*
    Format a detailed request message
 */
PUBLIC void httpDetailFormatter(HttpTrace *trace, cchar *event, cchar *type, int flags,
    cchar *data, ssize len, cchar *fmt, va_list args)
{
    MprBuf      *buf;
    MprTime     now;
    char        *msg;

    assert(trace);
    lock(trace);

    if (!trace->buf) {
        trace->buf = mprCreateBuf(ME_PACKET_SIZE, 0);
    }
    buf = trace->buf;

    if (fmt) {
        now = mprGetTime();
        if (trace->lastMark < (now + TPS) || trace->lastTime == 0) {
            trace->lastTime = mprGetDate("%D %T");
            trace->lastMark = now;
        }
        mprPutToBuf(buf, "%s %s event=%s type=%s\n", trace->lastTime, getTraceTag(event, type), event, type);
    }
    if (fmt) {
        msg = sfmtv(fmt, args);
        if (*msg == '@') {
            mprPutStringToBuf(buf, &msg[1]);
        } else {
            mprPutStringToBuf(buf, msg);
        }
        mprPutStringToBuf(buf, "\n");
    }
    if (data && len > 0) {
        traceData(trace, data, len, flags);
        mprPutStringToBuf(buf, "\n");
    }
    flushTrace(trace);
    unlock(trace);
}


PUBLIC void httpPrettyFormatter(HttpTrace *trace, cchar *event, cchar *type, int flags,
        cchar *data, ssize len, cchar *fmt, va_list args)
{
    MprBuf      *buf;
    MprTicks    now;
    char        *msg;

    assert(trace);
    assert(event);
    assert(type);

    lock(trace);
    if (!trace->buf) {
        trace->buf = mprCreateBuf(ME_PACKET_SIZE, 0);
    }
    buf = trace->buf;

    if (fmt) {
        now = mprGetTime();
        if (trace->lastMark < (now + TPS) || trace->lastTime == 0) {
            trace->lastTime = mprGetDate("%D %T");
            trace->lastMark = now;
        }
        mprPutToBuf(buf, "\n%s %s (%s)\n", trace->lastTime, getTraceTag(event, type), event);
        if (fmt) {
            mprPutCharToBuf(buf, '\n');
            msg = sfmtv(fmt, args);
            if (flags & HTTP_TRACE_RAW) {
                mprPutStringToBuf(buf, msg);
            } else if (*msg == '@') {
                mprPutStringToBuf(buf, &msg[1]);
            } else {
                emitTraceValues(buf, msg);
            }
        }
        if (data && len > 0) {
            mprPutCharToBuf(buf, '\n');
        }
    }
    if (data && len > 0) {
        traceData(trace, data, len, flags);
    }
    if (!(flags & HTTP_TRACE_CONT)) {
        flushTrace(trace);
    }
    unlock(trace);
}


/*
    Trace a data buffer. Will emit in hex if flags & HTTP_TRACE_HEX.
 */
static void traceData(HttpTrace *trace, cchar *data, ssize len, int flags)
{
    MprBuf  *buf;
    cchar   *cp, *digits, *sol;
    char    *end, *ep;
    ssize   bsize, lines, need, space;
    int     i, j;

    buf = trace->buf;

    if (flags & HTTP_TRACE_HEX) {
        /*
            Round up lines, 4 chars per byte plush 3 chars per line (||\n)
         */
        lines = len / 16 + 1;
        bsize = ((lines * 16) * 4) + (lines * 5) + 2;
        space = mprGetBufSpace(buf);
        if (bsize > space) {
            need = bsize - space;
            need = max(need, ME_PACKET_SIZE);
            mprGrowBuf(buf, need);
        }
        end = mprGetBufEnd(buf);
        digits = "0123456789ABCDEF";

        for (i = 0, cp = data, ep = end; cp < &data[len]; ) {
            sol = cp;
            for (j = 0; j < 16 && cp < &data[len]; j++, cp++) {
                *ep++ = digits[(*cp >> 4) & 0x0f];
                *ep++ = digits[*cp & 0x0f];
                *ep++ = ' ';
            }

            for (; j < 16; j++) {
                *ep++ = ' '; *ep++ = ' '; *ep++ = ' ';
            }
            *ep++ = ' '; *ep++ = ' '; *ep++ = '|';
            for (j = 0, cp = sol; j < 16 && cp < &data[len]; j++, cp++) {
                *ep++ = isprint(*cp) ? *cp : '.';
            }
            for (; j < 16; j++) {
                *ep++ = ' ';
            }
            *ep++ = '|';
            *ep++ = '\n';
            assert((ep - end) <= bsize);
        }
        *ep = '\0';
        mprAdjustBufEnd(buf, ep - end);

    } else {
        mprPutBlockToBuf(buf, data, len);
    }
}


static void flushTrace(HttpTrace *trace)
{
    MprBuf      *buf;

    buf = trace->buf;
    mprPutStringToBuf(buf, "\n");
    httpWriteTrace(trace, mprGetBufStart(buf), mprGetBufLength(buf));
    mprFlushBuf(buf);
}


static void emitTraceValues(MprBuf *buf, char *str)
{
    char    *key, *tok, *value;

    for (key = stok(str, ",", &tok); key; key = stok(0, ",", &tok)) {
        if (key[0] != ':' && schr(key, ':')) {
            key = stok(key, ":", &value);
        } else if (schr(key, '=')) {
            key = stok(key, "=", &value);
        } else {
            value = NULL;
        }
        key = strim(key, " \t'", MPR_TRIM_BOTH);
        if (value) {
            value = strim(value, "\t'", MPR_TRIM_BOTH);
        } else {
            value = "";
        }
        if (smatch(key, "msg")) {
            mprPutToBuf(buf, "    %14s:  %s\n", "message", value);
        } else {
            mprPutToBuf(buf, "    %14s:  %s\n", key, value);
        }
    }
}


static cchar *getTraceTag(cchar *event, cchar *type)
{
    if (sstarts(event, "tx")) {
        return "OUTGOING";
    } else if (sstarts(event, "rx")) {
        return "INCOMING";
    }
    return type;
}

/*
    Common Log Formatter (NCSA)
    This formatter only emits messages only for connections at their complete event.
 */
PUBLIC void httpCommonFormatter(HttpTrace *trace, cchar *event, cchar *type, int flags, cchar *data, ssize len,
        cchar *msg, va_list args)
{
    HttpStream  *stream;
    HttpRx      *rx;
    HttpTx      *tx;
    MprBuf      *buf;
    cchar       *fmt, *cp, *qualifier, *timeText, *value;
    char        c, keyBuf[256];

    assert(trace);
    assert(type && *type);
    assert(event && *event);

    stream = (HttpStream*) data;
    if (!stream) {
        return;
    }
    if (!smatch(event, "rx.complete")) {
        return;
    }
    rx = stream->rx;
    tx = stream->tx;
    fmt = trace->format;
    if (fmt == 0 || fmt[0] == '\0') {
        fmt = ME_HTTP_LOG_FORMAT;
    }
    if (!trace->buf) {
        trace->buf = mprCreateBuf(ME_PACKET_SIZE, 0);
    }
    buf = trace->buf;

    while ((c = *fmt++) != '\0') {
        if (c != '%' || (c = *fmt++) == '%') {
            mprPutCharToBuf(buf, c);
            continue;
        }
        switch (c) {
        case 'a':                           /* Remote IP */
            mprPutStringToBuf(buf, stream->ip);
            break;

        case 'A':                           /* Local IP */
            mprPutStringToBuf(buf, stream->sock->listenSock->ip);
            break;

        case 'b':
            if (tx->bytesWritten == 0) {
                mprPutCharToBuf(buf, '-');
            } else {
                mprPutIntToBuf(buf, tx->bytesWritten);
            }
            break;

        case 'B':                           /* Bytes written (minus headers) */
            mprPutIntToBuf(buf, (tx->bytesWritten - tx->headerSize));
            break;

        case 'h':                           /* Remote host */
            mprPutStringToBuf(buf, stream->ip);
            break;

        case 'l':                           /* user identity - unknown */
            mprPutCharToBuf(buf, '-');
            break;

        case 'n':                           /* Local host */
            mprPutStringToBuf(buf, rx->parsedUri->host);
            break;

        case 'O':                           /* Bytes written (including headers) */
            mprPutIntToBuf(buf, tx->bytesWritten);
            break;

        case 'r':                           /* First line of request */
            mprPutToBuf(buf, "%s %s %s", rx->method, rx->uri, httpGetProtocol(stream->net));
            break;

        case 's':                           /* Response code */
            mprPutIntToBuf(buf, tx->status);
            break;

        case 't':                           /* Time */
            mprPutCharToBuf(buf, '[');
            timeText = mprFormatLocalTime(MPR_DEFAULT_DATE, mprGetTime());
            mprPutStringToBuf(buf, timeText);
            mprPutCharToBuf(buf, ']');
            break;

        case 'u':                           /* Remote username */
            mprPutStringToBuf(buf, stream->username ? stream->username : "-");
            break;

        case '{':                           /* Header line "{header}i" */
            qualifier = fmt;
            if ((cp = schr(qualifier, '}')) != 0) {
                fmt = &cp[1];
                switch (*fmt++) {
                case 'i':
                    sncopy(keyBuf, sizeof(keyBuf), qualifier, cp - qualifier);
                    value = (char*) mprLookupKey(rx->headers, keyBuf);
                    mprPutStringToBuf(buf, value ? value : "-");
                    break;
                default:
                    mprPutSubStringToBuf(buf, qualifier, qualifier - cp);
                }

            } else {
                mprPutCharToBuf(buf, c);
            }
            break;

        case '>':
            if (*fmt == 's') {
                fmt++;
                mprPutIntToBuf(buf, tx->status);
            }
            break;

        default:
            mprPutCharToBuf(buf, c);
            break;
        }
    }
    mprPutCharToBuf(buf, '\n');
    flushTrace(trace);
}

/************************************** TraceLogFile **************************/

static int backupTraceLogFile(HttpTrace *trace)
{
    MprPath     info;

    assert(trace->path);

    if (trace->file == MPR->logFile) {
        return 0;
    }
    if (trace->backupCount > 0 || (trace->flags & MPR_LOG_ANEW)) {
        lock(trace);
        if (trace->path && trace->parent && smatch(trace->parent->path, trace->path)) {
            unlock(trace);
            return backupTraceLogFile(trace->parent);
        }
        mprGetPathInfo(trace->path, &info);
        if (info.valid && ((trace->flags & MPR_LOG_ANEW) || info.size > trace->size)) {
            if (trace->file) {
                mprCloseFile(trace->file);
                trace->file = 0;
            }
            if (trace->backupCount > 0) {
                mprBackupLog(trace->path, trace->backupCount);
            }
        }
        unlock(trace);
    }
    return 0;
}


/*
    Open the request log file
 */
PUBLIC int httpOpenTraceLogFile(HttpTrace *trace)
{
    MprFile     *file;
    int         mode;

    if (!trace->file && trace->path) {
        if (smatch(trace->path, "-")) {
            file = MPR->logFile;
        } else {
            backupTraceLogFile(trace);
            mode = O_CREAT | O_WRONLY | O_TEXT;
            mode |= (trace->flags & MPR_LOG_ANEW) ? O_TRUNC : O_APPEND;
            if (smatch(trace->path, "stdout")) {
                file = MPR->stdOutput;
            } else if (smatch(trace->path, "stderr")) {
                file = MPR->stdError;
            } else if ((file = mprOpenFile(trace->path, mode, 0664)) == 0) {
                mprLog("error http trace", 0, "Cannot open log file %s", trace->path);
                return MPR_ERR_CANT_OPEN;
            }
        }
        trace->file = file;
        trace->flags &= ~MPR_LOG_ANEW;
    }
    return 0;
}


/*
    Start tracing when instructed via a command line option.
 */
PUBLIC int httpStartTracing(cchar *traceSpec)
{
    HttpTrace   *trace;
    char        *lspec;

    if (HTTP == 0 || HTTP->trace == 0 || traceSpec == 0 || *traceSpec == '\0') {
        assert(HTTP);
        return MPR_ERR_BAD_STATE;
    }
    trace = HTTP->trace;
    trace->flags = MPR_LOG_ANEW | MPR_LOG_CMDLINE;
    trace->path = stok(sclone(traceSpec), ":", &lspec);
    trace->level = (int) stoi(lspec);
    return httpOpenTraceLogFile(trace);
}


/*
    Configure the trace log file
 */
PUBLIC int httpSetTraceLogFile(HttpTrace *trace, cchar *path, ssize size, int backup, cchar *format, int flags)
{
    assert(trace);
    assert(path && *path);

    if (format == NULL || *format == '\0') {
        format = ME_HTTP_LOG_FORMAT;
    }
    trace->backupCount = backup;
    trace->flags = flags;
    trace->format = sclone(format);
    trace->size = size;
    trace->path = sclone(path);
    trace->file = 0;
    return httpOpenTraceLogFile(trace);
}


/*
    Write a message to the trace log
 */
PUBLIC void httpWriteTraceLogFile(HttpTrace *trace, cchar *buf, ssize len)
{
    static int  skipCheck = 0;

    lock(trace);
    if (trace->backupCount > 0) {
        if ((++skipCheck % 50) == 0) {
            backupTraceLogFile(trace);
        }
    }
    if (!trace->file && trace->path && httpOpenTraceLogFile(trace) < 0) {
        unlock(trace);
        return;
    }
	if (trace->file) {
		mprWriteFile(trace->file, buf, len);
	}
    unlock(trace);
}


PUBLIC bool httpShouldTrace(HttpTrace *trace, cchar *type)
{
    int         level;

    assert(type && *type);
    level = PTOI(mprLookupKey(trace->events, type));
    if (level >= 0 && level <= trace->level) {
        return 1;
    }
    return 0;
}


PUBLIC void httpTraceQueues(HttpStream *stream)
{
    HttpNet     *net;
    HttpQueue   *q;
    HttpTrace   *trace;
    cchar       *pipeline;

    net = stream->net;
    trace = stream->trace;
    if (!httpShouldTrace(trace, "detail")) {
        return;
    }
    pipeline = stream->rxHead->name;
    for (q = stream->rxHead->prevQ; q != stream->rxHead; q = q->prevQ) {
        pipeline = sjoin(pipeline, " < ", q->name, NULL);
    }
    pipeline = sjoin(pipeline, " < ", net->inputq->name, " < ", net->socketq->name, NULL);
    httpLog(trace, "stream.queues", "detail", "msg:read-pipeline: %s ", pipeline);

    pipeline = stream->txHead->name;
    for (q = stream->txHead->nextQ; q != stream->txHead; q = q->nextQ) {
        pipeline = sjoin(pipeline, " > ", q->name, NULL);
    }
    pipeline = sjoin(pipeline, " > ", net->outputq->name, NULL);
    for (q = net->outputq->nextQ; q != net->outputq; q = q->prevQ) {
        pipeline = sjoin(pipeline, " > ", q->name, NULL);
    }
    httpLog(trace, "stream.queues", "detail", "msg:write-pipeline: %s ", pipeline);
    httpLog(trace, "stream.queues", "detail", "readq: %s, writeq: %s, inputq: %s, outputq: %s",
        stream->readq->name, stream->writeq->name, stream->inputq->name, stream->outputq->name);
}


PUBLIC char *httpStatsReport(int flags)
{
    Http                *http;
    HttpNet             *net;
    HttpStream          *stream;
    HttpRx              *rx;
    HttpTx              *tx;
    MprTime             now;
    MprBuf              *buf;
    HttpStats           s;
    double              elapsed, mb;
    static MprTime      lastTime = 0;
    static HttpStats    last;
    int                 nextNet, nextStream;

    mb = 1024.0 * 1024;
    now = mprGetTime();
    if (lastTime == 0) {
        lastTime = MPR->start;
    }
    elapsed = (now - lastTime) / 1000.0;
    httpGetStats(&s);
    buf = mprCreateBuf(ME_PACKET_SIZE, 0);

    mprPutToBuf(buf, "\nHttp Report: at %s\n\n", mprGetDate("%D %T"));
    if (flags & HTTP_STATS_MEMORY) {
        mprPutToBuf(buf, "Memory       %8.1f MB, %5.1f%% max\n", s.mem / mb, s.mem / (double) s.memMax * 100.0);
        mprPutToBuf(buf, "Heap         %8.1f MB, %5.1f%% mem\n", s.heap / mb, s.heap / (double) s.mem * 100.0);
        mprPutToBuf(buf, "Heap-peak    %8.1f MB\n", s.heapPeak / mb);
        mprPutToBuf(buf, "Heap-used    %8.1f MB, %5.1f%% used\n", s.heapUsed / mb, s.heapUsed / (double) s.heap * 100.0);
        mprPutToBuf(buf, "Heap-free    %8.1f MB, %5.1f%% free\n", s.heapFree / mb, s.heapFree / (double) s.heap * 100.0);

        if (s.memMax == (size_t) -1) {
            mprPutToBuf(buf, "Heap limit          -\n");
            mprPutToBuf(buf, "Heap readline       -\n");
        } else {
            mprPutToBuf(buf, "Heap limit   %8.1f MB\n", s.memMax / mb);
            mprPutToBuf(buf, "Heap redline %8.1f MB\n", s.memRedline / mb);
        }
    }

    mprPutToBuf(buf, "Connections  %8.1f per/sec\n", (s.totalConnections - last.totalConnections) / elapsed);
    mprPutToBuf(buf, "Requests     %8.1f per/sec\n", (s.totalRequests - last.totalRequests) / elapsed);
    mprPutToBuf(buf, "Sweeps       %8.1f per/sec\n", (s.totalSweeps - last.totalSweeps) / elapsed);
    mprPutCharToBuf(buf, '\n');

    mprPutToBuf(buf, "Clients      %8d active\n", s.activeClients);
    mprPutToBuf(buf, "Connections  %8d active\n", s.activeConnections);
    mprPutToBuf(buf, "Processes    %8d active\n", s.activeProcesses);
    mprPutToBuf(buf, "Requests     %8d active\n", s.activeRequests);
    mprPutToBuf(buf, "Sessions     %8d active\n", s.activeSessions);
    mprPutToBuf(buf, "Workers      %8d busy - %d yielded, %d idle, %d max\n",
        s.workersBusy, s.workersYielded, s.workersIdle, s.workersMax);
    mprPutToBuf(buf, "Sessions     %8.1f MB\n", s.memSessions / mb);
    mprPutCharToBuf(buf, '\n');
    last = s;


    http = HTTP;
    mprPutToBuf(buf, "\nActive Streams:\n");
    for (ITERATE_ITEMS(http->networks, net, nextNet)) {
        for (ITERATE_ITEMS(net->streams, stream, nextStream)) {
            rx = stream->rx;
            tx = stream->tx;
            mprPutToBuf(buf,
                "State %d (%d), error %d, eof %d, finalized input %d, output %d, connector %d, seqno %lld, mask %x, uri %s\n",
                stream->state, stream->h2State, stream->error, rx->eof, tx->finalizedInput, tx->finalizedOutput,
                tx->finalizedConnector, stream->seqno, net->eventMask, rx->uri);
        }
    }
    mprPutCharToBuf(buf, '\n');

    lastTime = now;
    mprAddNullToBuf(buf);
    return sclone(mprGetBufStart(buf));
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/tx.c ************/

/*
    tx.c - Http transmitter for server responses and client requests.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/***************************** Forward Declarations ***************************/

static void checkFinalized(HttpStream *stream);
static bool flushPipe(HttpQueue *q, int flags);
static void manageTx(HttpTx *tx, int flags);

/*********************************** Code *************************************/

PUBLIC HttpTx *httpCreateTx(HttpStream *stream, MprHash *headers)
{
    HttpTx      *tx;

    assert(stream);
    assert(stream->net);

    if ((tx = mprAllocObj(HttpTx, manageTx)) == 0) {
        return 0;
    }
    stream->tx = tx;
    tx->stream = stream;
    tx->status = HTTP_CODE_OK;
    tx->length = -1;
    tx->entityLength = -1;
    tx->chunkSize = -1;
    tx->cookies = mprCreateHash(HTTP_SMALL_HASH_SIZE, 0);

    if (headers) {
        tx->headers = headers;
    } else {
        tx->headers = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS | MPR_HASH_STABLE);
        if (httpClientStream(stream)) {
            httpAddHeaderString(stream, "User-Agent", sclone(ME_HTTP_SOFTWARE));
        }
    }
    return tx;
}


PUBLIC void httpDestroyTx(HttpTx *tx)
{
    if (tx->file) {
        mprCloseFile(tx->file);
        tx->file = 0;
    }
    if (tx->stream) {
        tx->stream->tx = 0;
        tx->stream = 0;
    }
}


static void manageTx(HttpTx *tx, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(tx->altBody);
        mprMark(tx->cache);
        mprMark(tx->cacheBuffer);
        mprMark(tx->cachedContent);
        mprMark(tx->charSet);
        mprMark(tx->stream);
        mprMark(tx->connector);
        mprMark(tx->cookies);
        mprMark(tx->currentRange);
        mprMark(tx->ext);
        mprMark(tx->etag);
        mprMark(tx->errorDocument);
        mprMark(tx->file);
        mprMark(tx->filename);
        mprMark(tx->handler);
        mprMark(tx->headers);
        mprMark(tx->method);
        mprMark(tx->mimeType);
        mprMark(tx->outputPipeline);
        mprMark(tx->outputRanges);
        mprMark(tx->parsedUri);
        mprMark(tx->rangeBoundary);
        mprMark(tx->webSockKey);
    }
}


/*
    Add key/value to the header hash. If already present, update the value
*/
static void updateHdr(HttpStream *stream, cchar *key, cchar *value)
{
    assert(key && *key);
    assert(value);

    if (schr(value, '$')) {
        value = httpExpandVars(stream, value);
    }
    mprAddKey(stream->tx->headers, key, value);
}


PUBLIC int httpRemoveHeader(HttpStream *stream, cchar *key)
{
    assert(key && *key);
    if (stream->tx == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    return mprRemoveKey(stream->tx->headers, key);
}


/*
    Add a http header if not already defined
 */
PUBLIC void httpAddHeader(HttpStream *stream, cchar *key, cchar *fmt, ...)
{
    char        *value;
    va_list     vargs;

    assert(key && *key);
    assert(fmt && *fmt);

    if (fmt) {
        va_start(vargs, fmt);
        value = sfmtv(fmt, vargs);
        va_end(vargs);
    } else {
        value = MPR->emptyString;
    }
    if (stream->tx && !mprLookupKey(stream->tx->headers, key)) {
        updateHdr(stream, key, value);
    }
}


/*
    Add a header string if not already defined
 */
PUBLIC void httpAddHeaderString(HttpStream *stream, cchar *key, cchar *value)
{
    assert(key && *key);
    assert(value);

    if (stream->tx && !mprLookupKey(stream->tx->headers, key)) {
        updateHdr(stream, key, sclone(value));
    }
}


/*
   Append a header. If already defined, the value is catenated to the pre-existing value after a ", " separator.
   As per the HTTP/1.1 spec. Except for Set-Cookie which HTTP permits multiple headers but not of the same cookie. Ugh!
 */
PUBLIC void httpAppendHeader(HttpStream *stream, cchar *key, cchar *fmt, ...)
{
    va_list     vargs;
    MprKey      *kp;
    char        *value;
    cchar       *cookie;

    if (!stream->tx) {
        return;
    }
    assert(key && *key);
    assert(fmt && *fmt);

    va_start(vargs, fmt);
    value = sfmtv(fmt, vargs);
    va_end(vargs);

    /*
        HTTP permits Set-Cookie to have multiple cookies. Other headers must comma separate multiple values.
        For Set-Cookie, must allow duplicates but not of the same cookie.
     */
    kp = mprLookupKeyEntry(stream->tx->headers, key);
    if (kp) {
        if (scaselessmatch(key, "Set-Cookie")) {
            cookie = stok(sclone(value), "=", NULL);
            while (kp) {
                if (scaselessmatch(kp->key, "Set-Cookie")) {
                    if (sstarts(kp->data, cookie)) {
                        kp->data = value;
                        break;
                    }
                }
                kp = kp->next;
            }
            if (!kp) {
                mprAddDuplicateKey(stream->tx->headers, key, value);
            }
        } else {
            updateHdr(stream, key, sfmt("%s, %s", (char*) kp->data, value));
        }
    } else {
        updateHdr(stream, key, value);
    }
}


/*
   Append a header string. If already defined, the value is catenated to the pre-existing value after a ", " separator.
   As per the HTTP/1.1 spec.
 */
PUBLIC void httpAppendHeaderString(HttpStream *stream, cchar *key, cchar *value)
{
    cchar   *oldValue;

    assert(key && *key);
    assert(value && *value);

    if (!stream->tx) {
        return;
    }
    oldValue = mprLookupKey(stream->tx->headers, key);
    if (oldValue) {
        if (scaselessmatch(key, "Set-Cookie")) {
            mprAddDuplicateKey(stream->tx->headers, key, sclone(value));
        } else {
            updateHdr(stream, key, sfmt("%s, %s", oldValue, value));
        }
    } else {
        updateHdr(stream, key, sclone(value));
    }
}


PUBLIC cchar *httpGetTxHeader(HttpStream *stream, cchar *key)
{
    if (stream->tx == 0) {
        assert(stream->tx);
        return 0;
    }
    return mprLookupKey(stream->tx->headers, key);
}


/*
    Set a http header. Overwrite if present.
 */
PUBLIC void httpSetHeader(HttpStream *stream, cchar *key, cchar *fmt, ...)
{
    char        *value;
    va_list     vargs;

    assert(key && *key);
    assert(fmt && *fmt);

    va_start(vargs, fmt);
    value = sfmtv(fmt, vargs);
    va_end(vargs);
    updateHdr(stream, key, value);
}


PUBLIC void httpSetHeaderString(HttpStream *stream, cchar *key, cchar *value)
{
    assert(key && *key);
    assert(value);

    updateHdr(stream, key, sclone(value));
}


/*
    Called by connectors (ONLY) when writing the entire output transmission is complete
 */
PUBLIC void httpFinalizeConnector(HttpStream *stream)
{
    HttpTx      *tx;

    tx = stream->tx;
    tx->finalizedConnector = 1;
    checkFinalized(stream);

    if (stream->net->protocol >= 2) {
        httpFinalizeHttp2Stream(stream);
    }
}


/*
    This means the handler has processed all the input
 */
PUBLIC void httpFinalizeInput(HttpStream *stream)
{
    HttpTx      *tx;

    tx = stream->tx;
    if (tx && !tx->finalizedInput) {
        tx->finalizedInput = 1;
        httpSetEof(stream);
        httpSetState(stream, HTTP_STATE_READY);
        checkFinalized(stream);
    }
}


/*
    The handler has generated the entire transmit body. Note: the data may not yet have drained from
    the pipeline or socket and the caller may not have read all the input body content.
 */
PUBLIC void httpFinalizeOutput(HttpStream *stream)
{
    HttpTx      *tx;

    tx = stream->tx;
    if (!tx || tx->finalizedOutput) {
        return;
    }
    tx->responded = 1;
    tx->finalizedOutput = 1;
    if (!(tx->flags & HTTP_TX_PIPELINE)) {
        /* Tx Pipeline not yet created */
        tx->pendingFinalize = 1;
        return;
    }
    if (!tx->putEndPacket) {
        httpPutPacket(stream->writeq, httpCreateEndPacket());
        httpScheduleQueue(stream->writeq);
    }
    checkFinalized(stream);
}


/*
    Finalize both input and ouput. This means the caller is totally completed with the request. They have sent all
    output and have read all input. Further input can be discarded. Note that output may not yet have drained from the socket.
 */
PUBLIC void httpFinalize(HttpStream *stream)
{
    //  Must finalize output first so finalizedOutput is set when httpFinalizeInput will set state to READY
    httpFinalizeOutput(stream);
    httpFinalizeInput(stream);
}


static void checkFinalized(HttpStream *stream)
{
    HttpTx      *tx;

    tx = stream->tx;
    if (!tx || tx->finalized) {
        return;
    }
    if (tx->finalizedInput && tx->finalizedOutput && tx->finalizedConnector) {
        if (stream->rx->session) {
            httpWriteSession(stream);
        }
        httpSetState(stream, HTTP_STATE_FINALIZED);
        tx->finalized = 1;
    }
}


PUBLIC int httpIsFinalized(HttpStream *stream)
{
    return stream->tx->finalized;
}


PUBLIC int httpIsOutputFinalized(HttpStream *stream)
{
    return stream->tx->finalizedOutput;
}


PUBLIC int httpIsInputFinalized(HttpStream *stream)
{
    return stream->tx->finalizedInput;
}


/*
    This formats a response and sets the altBody. The response is not HTML escaped.
    This is the lowest level for formatResponse.
 */
PUBLIC ssize httpFormatResponsev(HttpStream *stream, cchar *fmt, va_list args)
{
    HttpTx      *tx;
    cchar       *body;

    tx = stream->tx;
    tx->responded = 1;
    body = fmt ? sfmtv(fmt, args) : stream->errorMsg;
    tx->altBody = body;
    tx->length = slen(tx->altBody);
    tx->flags |= HTTP_TX_NO_BODY;
    httpDiscardData(stream, HTTP_QUEUE_TX);
    return (ssize) tx->length;
}


/*
    This formats a response and sets the altBody. The response is not HTML escaped.
 */
PUBLIC ssize httpFormatResponse(HttpStream *stream, cchar *fmt, ...)
{
    va_list     args;
    ssize       rc;

    va_start(args, fmt);
    rc = httpFormatResponsev(stream, fmt, args);
    va_end(args);
    return rc;
}


/*
    This formats a complete response. Depending on the Accept header, the response will be either HTML or plain text.
    The response is not HTML escaped. This calls httpFormatResponse.
 */
PUBLIC ssize httpFormatResponseBody(HttpStream *stream, cchar *title, cchar *fmt, ...)
{
    va_list     args;
    cchar       *msg, *body;

    va_start(args, fmt);
    body = fmt ? sfmtv(fmt, args) : stream->errorMsg;

    if (scmp(stream->rx->accept, "text/plain") == 0) {
        msg = body;
    } else {
        msg = sfmt(
            "<!DOCTYPE html>\r\n"
            "<html><head><title>%s</title></head>\r\n"
            "<body>\r\n%s\r\n</body>\r\n</html>\r\n",
            title, body);
    }
    va_end(args);
    return httpFormatResponse(stream, "%s", msg);
}


PUBLIC void *httpGetQueueData(HttpStream *stream)
{
    HttpQueue     *q;

    q = stream->writeq;
    return q->queueData;
}


PUBLIC void httpOmitBody(HttpStream *stream)
{
    HttpTx  *tx;

    tx = stream->tx;
    if (tx && !(tx->flags & HTTP_TX_HEADERS_CREATED)) {
        tx->flags |= HTTP_TX_NO_BODY;
        tx->length = -1;
        httpDiscardData(stream, HTTP_QUEUE_TX);
    }
}


/*
    Redirect the user to another URI. The targetUri may or may not have a scheme or hostname.
 */
PUBLIC void httpRedirect(HttpStream *stream, int status, cchar *targetUri)
{
    HttpTx          *tx;
    HttpRx          *rx;
    HttpUri         *base, *canonical;
    cchar           *msg;

    assert(targetUri);
    rx = stream->rx;
    tx = stream->tx;

    if (tx->flags & HTTP_TX_HEADERS_CREATED) {
        mprLog("error", 0, "Headers already created, so redirect ignored: %s", targetUri);
        return;
    }
    if (HTTP->redirectCallback) {
        targetUri = (HTTP->redirectCallback)(stream, &status, targetUri);
    }
    tx->status = status;
    msg = httpLookupStatus(status);

    canonical = rx->route->canonical;
    if (canonical) {
        base = httpCloneUri(rx->parsedUri, 0);
        if (canonical->host) {
            base->host = canonical->host;
        }
        if (canonical->port) {
            base->port = canonical->port;
        }
    } else {
        base = rx->parsedUri;
    }
    /*
        Expand the target for embedded tokens. Resolve relative to the current request URI.
     */
    targetUri = httpUriToString(httpResolveUri(stream, base, httpLinkUri(stream, targetUri, 0)), 0);

    if (300 <= status && status <= 399) {
        httpSetHeader(stream, "Location", "%s", targetUri);
        httpFormatResponse(stream,
            "<!DOCTYPE html>\r\n"
            "<html><head><title>%s</title></head>\r\n"
            "<body><h1>%s</h1>\r\n<p>The document has moved <a href=\"%s\">here</a>.</p></body></html>\r\n",
            msg, msg, targetUri);
        httpLog(stream->trace, "tx.http.redirect", "context", "status:%d, location:%s", status, targetUri);
    } else {
        httpFormatResponse(stream,
            "<!DOCTYPE html>\r\n"
            "<html><head><title>%s</title></head>\r\n"
            "<body><h1>%s</h1>\r\n</body></html>\r\n",
            msg, msg);
    }
    httpFinalize(stream);
    tx->handler = stream->http->passHandler;
}


PUBLIC void httpSetContentLength(HttpStream *stream, MprOff length)
{
    HttpTx      *tx;

    tx = stream->tx;
    if (tx->flags & HTTP_TX_HEADERS_CREATED) {
        return;
    }
    tx->length = length;
}


/*
    Set lifespan < 0 to delete the cookie in the client.
    Set lifespan == 0 for no expiry.
    WARNING: Some browsers (Chrome, Firefox) do not delete session cookies when you exit the browser.
 */
PUBLIC void httpSetCookie(HttpStream *stream, cchar *name, cchar *value, cchar *path, cchar *cookieDomain,
    MprTicks lifespan, int flags)
{
    HttpRx      *rx;
    cchar       *domain, *domainAtt;
    char        *cp, *expiresAtt, *expires, *secure, *httpOnly, *sameSite;

    rx = stream->rx;
    if (path == 0) {
        path = "/";
    }
    /*
        Note: Cookies do not respect port numbers, so we ignore them here.
        Note: Modern browsers will give subdomains the cookies defined for a top-level domain.
        Note: A leading dot in the top-level domain is not required anymore.
        Note: Browsers may store top-level domain cookies with a leading dot in their cooke store (chrome).
     */
    domain = 0;
    if (cookieDomain) {
        /*
            Omit domain if set to empty string
        */
        if (*cookieDomain) {
            domain = (char*) cookieDomain;
        }
    } else if (rx->hostHeader) {
        if (mprParseSocketAddress(rx->hostHeader, &domain, NULL, NULL, 0) < 0) {
            mprLog("error http", 4, "Bad host header for cookie: %s", rx->hostHeader);
            return;
        }
    }
    domainAtt = domain ? "; domain=" : "";
    if (domain) {
        /*
            Domains must have at least one dot, so we prefix with a dot here if one is not present.
         */
        if (!strchr(domain, '.')) {
            if (smatch(domain, "localhost")) {
                domainAtt = domain = "";
            } else {
                domain = sjoin(".", domain, NULL);
            }
        }
    } else {
        domain = "";
    }
    if (lifespan) {
        expiresAtt = "; expires=";
        expires = mprFormatUniversalTime(MPR_HTTP_DATE, mprGetTime() + lifespan);
    } else {
        expires = expiresAtt = "";
    }
    secure = (stream->secure & (flags & HTTP_COOKIE_SECURE)) ? "; secure" : "";
    httpOnly = (flags & HTTP_COOKIE_HTTP) ?  "; httponly" : "";
    sameSite = "";
    if (flags & HTTP_COOKIE_SAME_NONE) {
        sameSite = "; SameSite=None";
    } else if (flags & HTTP_COOKIE_SAME_LAX) {
        sameSite = "; SameSite=Lax";
    } else if (flags & HTTP_COOKIE_SAME_STRICT) {
        sameSite = "; SameSite=Strict";
    }
    mprAddKey(stream->tx->cookies, name,
        sjoin(value, "; path=", path, domainAtt, domain, expiresAtt, expires, secure, httpOnly, sameSite, NULL));

    if ((cp = mprLookupKey(stream->tx->headers, "Cache-Control")) == 0 || !scontains(cp, "no-cache")) {
        httpAppendHeader(stream, "Cache-Control", "no-cache=\"set-cookie\"");
    }
}


PUBLIC void httpRemoveCookie(HttpStream *stream, cchar *name)
{
    HttpRoute   *route;
    cchar       *cookie, *url;

    route = stream->rx->route;
    url = (route->prefix && *route->prefix) ? route->prefix : "/";
    cookie = route->cookie ? route->cookie : HTTP_SESSION_COOKIE;
    httpSetCookie(stream, cookie, "", url, NULL, 1, 0);
}


static void setCorsHeaders(HttpStream *stream)
{
    HttpRoute   *route;
    cchar       *origin;

    route = stream->rx->route;

    /*
        Cannot use wildcard origin response if allowing credentials
     */
    if (*route->corsOrigin && !route->corsCredentials) {
        httpSetHeaderString(stream, "Access-Control-Allow-Origin", route->corsOrigin);
    } else {
        origin = httpGetHeader(stream, "Origin");
        httpSetHeaderString(stream, "Access-Control-Allow-Origin", origin ? origin : "*");
    }
    if (route->corsCredentials) {
        httpSetHeaderString(stream, "Access-Control-Allow-Credentials", "true");
    }
    if (route->corsHeaders) {
        httpSetHeaderString(stream, "Access-Control-Allow-Headers", route->corsHeaders);
    }
    if (route->corsMethods) {
        httpSetHeaderString(stream, "Access-Control-Allow-Methods", route->corsMethods);
    }
    if (route->corsAge) {
        httpSetHeader(stream, "Access-Control-Max-Age", "%d", route->corsAge);
    }
}


PUBLIC HttpPacket *httpCreateHeaders(HttpQueue *q, HttpPacket *packet)
{
    if (!packet) {
        packet = httpCreateHeaderPacket();
        packet->stream = q->stream;
    }
#if ME_HTTP_HTTP2
    if (q->net->protocol >= 2) {
        httpCreateHeaders2(q, packet);
    } else {
        httpCreateHeaders1(q, packet);
    }
#else
    httpCreateHeaders1(q, packet);
#endif
    return packet;
}


/*
    Define headers for httpWriteHeaders. This defines standard headers.
 */
PUBLIC void httpPrepareHeaders(HttpStream *stream)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpRoute   *route;
    HttpRange   *range;
    MprKeyValue *item;
    MprKey      *kp;
    MprOff      length;
    cchar       *value;
    int         next;

    rx = stream->rx;
    tx = stream->tx;
    route = rx->route;

    if (tx->flags & HTTP_TX_HEADERS_PREPARED) {
        return;
    }
    tx->flags |= HTTP_TX_HEADERS_PREPARED;

    if (stream->headersCallback) {
        /* Must be before headers below */
        (stream->headersCallback)(stream->headersCallbackArg);
    }

    /*
        Create headers for cookies
     */
    for (ITERATE_KEYS(tx->cookies, kp)) {
        httpAppendHeaderString(stream, "Set-Cookie", sjoin(kp->key, "=", kp->data, NULL));
    }

    /*
        Mandatory headers that must be defined here use httpSetHeader which overwrites existing values.
     */
    httpAddHeaderString(stream, "Date", stream->http->currentDate);

    if (tx->mimeType == 0 && route) {
        if (stream->error) {
            tx->mimeType = sclone("text/html");
        } else if (!route || (tx->mimeType = (char*) mprLookupMime(route->mimeTypes, tx->ext)) == 0) {
            tx->mimeType = sclone("text/html");
        }
    }
    if (tx->mimeType) {
        if (tx->charSet) {
            httpAddHeader(stream, "Content-Type", "%s; charset=%s", tx->mimeType, tx->charSet);
        } else {
            httpAddHeaderString(stream, "Content-Type", tx->mimeType);
        }
    }
    if (tx->etag) {
        httpAddHeader(stream, "ETag", "%s", tx->etag);
    }
    if (!stream->upgraded) {
        length = tx->length > 0 ? tx->length : 0;
        if (rx->flags & HTTP_HEAD) {
            stream->tx->flags |= HTTP_TX_NO_BODY;
            httpDiscardData(stream, HTTP_QUEUE_TX);
            if (tx->chunkSize <= 0) {
                httpAddHeader(stream, "Content-Length", "%lld", length);
            }

        } else if (tx->chunkSize > 0) {
            httpSetHeaderString(stream, "Transfer-Encoding", "chunked");

        } else if (httpServerStream(stream)) {
            /* Server must not emit a content length header for 1XX, 204 and 304 status */
            if (!((100 <= tx->status && tx->status <= 199) || tx->status == 204 || tx->status == 304 || tx->flags & HTTP_TX_NO_LENGTH)) {
                if (length > 0 || (length == 0 && stream->net->protocol < 2)) {
                    httpAddHeader(stream, "Content-Length", "%lld", length);
                }
            }

        } else if (tx->length > 0) {
            /* client with body */
            httpAddHeader(stream, "Content-Length", "%lld", length);
        }
    }
    if (tx->outputRanges) {
        if (tx->outputRanges->next == 0) {
            range = tx->outputRanges;
            if (tx->entityLength > 0) {
                httpSetHeader(stream, "Content-Range", "bytes %lld-%lld/%lld", range->start, range->end - 1, tx->entityLength);
            } else {
                httpSetHeader(stream, "Content-Range", "bytes %lld-%lld/*", range->start, range->end - 1);
            }
        } else {
            tx->mimeType = sfmt("multipart/byteranges; boundary=%s", tx->rangeBoundary);
            if (tx->charSet) {
                httpSetHeader(stream, "Content-Type", "%s; charset=%s", tx->mimeType, tx->charSet);
            } else {
                httpSetHeaderString(stream, "Content-Type", tx->mimeType);
            }
        }
        httpSetHeader(stream, "Accept-Ranges", "bytes");
    }
    if (httpServerStream(stream)) {
        if (!(route->flags & HTTP_ROUTE_STEALTH)) {
            httpAddHeaderString(stream, "Server", stream->http->software);
        }
        if (!stream->upgraded && stream->net->protocol < 2) {
            /*
                If keepAliveCount == 1
             */
            if (--stream->keepAliveCount > 0) {
                assert(stream->keepAliveCount >= 1);
                httpAddHeaderString(stream, "Connection", "Keep-Alive");
                if (!(route->flags & HTTP_ROUTE_STEALTH) || 1) {
                    httpAddHeader(stream, "Keep-Alive", "timeout=%lld, max=%d", stream->limits->inactivityTimeout / 1000,
                        stream->keepAliveCount);
                }
            } else {
                /* Tell the peer to close the connection */
                httpAddHeaderString(stream, "Connection", "close");
            }
        }
        if (route->flags & HTTP_ROUTE_CORS) {
            setCorsHeaders(stream);
        }
        /*
            Apply route headers
         */
        for (ITERATE_ITEMS(route->headers, item, next)) {
            if (item->flags == HTTP_ROUTE_ADD_HEADER) {
                value = httpExpandVars(stream, item->value);
                httpAddHeaderString(stream, item->key, value);

            } else if (item->flags == HTTP_ROUTE_APPEND_HEADER) {
                value = httpExpandVars(stream, item->value);
                httpAppendHeaderString(stream, item->key, value);

            } else if (item->flags == HTTP_ROUTE_REMOVE_HEADER) {
                httpRemoveHeader(stream, item->key);

            } else if (item->flags == HTTP_ROUTE_SET_HEADER) {
                value = httpExpandVars(stream, item->value);
                httpSetHeaderString(stream, item->key, value);
            }
        }
    }
    if (stream->net->protocol >= 2) {
        httpRemoveHeader(stream, "Connection");
        httpRemoveHeader(stream, "Keep-Alive");
    }
}


/*
    Low level routine to set the filename to serve. The filename may be outside the route documents, so caller
    must take care if the HTTP_TX_NO_CHECK flag is used.  This will update HttpTx.ext and HttpTx.fileInfo.
    This does not implement per-language directories. For that, see httpMapFile.
 */
PUBLIC bool httpSetFilename(HttpStream *stream, cchar *filename, int flags)
{
    HttpTx      *tx;
    MprPath     *info;

    assert(stream);

    tx = stream->tx;
    info = &tx->fileInfo;
    tx->flags &= ~(HTTP_TX_NO_CHECK | HTTP_TX_NO_MAP);
    tx->flags |= (flags & (HTTP_TX_NO_CHECK | HTTP_TX_NO_MAP));

    if (filename == 0) {
        tx->filename = 0;
        tx->ext = 0;
        info->checked = info->valid = 0;
        return 0;
    }
#if !ME_ROM
    if (!(tx->flags & HTTP_TX_NO_CHECK)) {
        if (!mprIsAbsPathContained(filename, stream->rx->route->documents)) {
            info->checked = 1;
            info->valid = 0;
            httpError(stream, HTTP_CODE_BAD_REQUEST, "Filename outside published documents");
            return 0;
        }
    }
#endif
    if (!tx->ext || tx->ext[0] == '\0') {
        tx->ext = httpGetPathExt(filename);
    }
    mprGetPathInfo(filename, info);
    if (info->valid) {
        tx->etag = itos(info->inode + info->size + info->mtime);
    }
    tx->filename = sclone(filename);

    if (tx->flags & HTTP_TX_PIPELINE) {
        /* Filename being revised after pipeline created */
        httpLog(stream->trace, "tx.http.document", "context", "filename:%s", tx->filename);
    }
    return info->valid;
}


PUBLIC void httpSetResponded(HttpStream *stream)
{
    stream->tx->responded = 1;
}


PUBLIC void httpSetStatus(HttpStream *stream, int status)
{
    stream->tx->status = status;
    stream->tx->responded = 1;
}


PUBLIC void httpSetCharSet(HttpStream *stream, cchar *charSet)
{
    stream->tx->charSet = sclone(charSet);
}


PUBLIC void httpSetContentType(HttpStream *stream, cchar *mimeType)
{
    stream->tx->mimeType = sclone(mimeType);
}


PUBLIC bool httpFileExists(HttpStream *stream)
{
    HttpTx      *tx;

    tx = stream->tx;
    if (!tx->fileInfo.checked) {
        mprGetPathInfo(tx->filename, &tx->fileInfo);
    }
    return tx->fileInfo.valid;
}


/*
    Write a block of data. This is the lowest level write routine for data. This will buffer the data and flush if
    the queue buffer is full. Flushing is done by calling httpFlushQueue which will service queues as required.
    Flags can be HTTP_BUFFER, HTTP_BLOCK, HTTP_NON_BLOCK.
    WARNING: may yield if using HTTP_BLOCK.
 */
PUBLIC ssize httpWriteBlock(HttpQueue *q, cchar *buf, ssize len, int flags)
{
    HttpNet     *net;
    HttpPacket  *packet;
    HttpStream  *stream;
    HttpTx      *tx;
    ssize       totalWritten, packetSize, thisWrite;

    assert(q == q->stream->writeq);

    stream = q->stream;
    if (!stream) {
        return 0;
    }
    net = stream->net;
    tx = stream->tx;

    if (flags == 0) {
        flags = HTTP_BUFFER;
    }
    if (tx == 0 || tx->finalizedOutput) {
        return MPR_ERR_CANT_WRITE;
    }
    tx->responded = 1;

    if (mprNeedYield() && (flags & HTTP_BLOCK)) {
        mprYield(0);
    }
    for (totalWritten = 0; len > 0; ) {
        if (stream->state >= HTTP_STATE_FINALIZED || stream->net->error) {
            return MPR_ERR_CANT_WRITE;
        }
        if (q->last && (q->last != q->first) && (q->last->flags & HTTP_PACKET_DATA) && mprGetBufSpace(q->last->content) > 0) {
            packet = q->last;
        } else {
            packetSize = (tx->chunkSize > 0) ? tx->chunkSize : q->packetSize;
            if ((packet = httpCreateDataPacket(packetSize)) == 0) {
                return MPR_ERR_MEMORY;
            }
        }
        thisWrite = min(len, mprGetBufSpace(packet->content));
        if (flags & (HTTP_BLOCK | HTTP_NON_BLOCK) && q->count < q->max) {
            thisWrite = min(thisWrite, q->max - q->count);
        }
        if (thisWrite > 0) {
            if ((thisWrite = mprPutBlockToBuf(packet->content, buf, thisWrite)) == 0) {
                return MPR_ERR_MEMORY;
            }
            buf += thisWrite;
            len -= thisWrite;
            totalWritten += thisWrite;
            if (packet == q->last) {
                q->count += thisWrite;
            } else {
                httpPutPacket(q, packet);
            }
        }
        if (!flushPipe(q, flags) && flags & HTTP_NON_BLOCK) {
            break;
        }
    }
    if (stream->error) {
        return MPR_ERR_CANT_WRITE;
    }
    return totalWritten;
}


static bool flushPipe(HttpQueue *q, int flags)
{
    if (!q->service) {
        q = httpFindNextQueue(q);
    }
    if (q->count >= q->max || q->flags & HTTP_QUEUE_SUSPENDED) {
        //  WARNING: may yield if flags are HTTP_BLOCK
        httpFlushQueue(q, flags);
        if (q->count >= q->max) {
            return 0;
        }
    }
    return 1;
}


PUBLIC ssize httpWriteString(HttpQueue *q, cchar *s)
{
    return httpWriteBlock(q, s, strlen(s), HTTP_BUFFER);
}


PUBLIC ssize httpWriteSafeString(HttpQueue *q, cchar *s)
{
    return httpWriteString(q, mprEscapeHtml(s));
}


PUBLIC ssize httpWrite(HttpQueue *q, cchar *fmt, ...)
{
    va_list     vargs;
    char        *buf;

    va_start(vargs, fmt);
    buf = sfmtv(fmt, vargs);
    va_end(vargs);
    return httpWriteString(q, buf);
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/uploadFilter.c ************/

/*
    uploadFilter.c - Upload file filter.

    The upload filter processes post data according to RFC-1867 ("multipart/form-data" post data).
    It saves the uploaded files in a configured upload directory.

    The upload filter is configured in the standard pipeline before the request is parsed and routed.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



#if ME_HTTP_UPLOAD
/*********************************** Locals ***********************************/
/*
    Upload state machine states
 */
#define HTTP_UPLOAD_REQUEST_HEADER        1   /* Request header */
#define HTTP_UPLOAD_BOUNDARY              2   /* Boundary divider */
#define HTTP_UPLOAD_CONTENT_HEADER        3   /* Content part header */
#define HTTP_UPLOAD_CONTENT_DATA          4   /* Content encoded data */
#define HTTP_UPLOAD_CONTENT_END           5   /* End of multipart message */

/*
    Per upload context
 */
typedef struct Upload {
    HttpUploadFile  *currentFile;       /* Current file context */
    MprFile         *file;              /* Current file I/O object */
    char            *boundary;          /* Boundary signature */
    ssize           boundaryLen;        /* Length of boundary */
    int             contentState;       /* Input states */
    char            *clientFilename;    /* Current file filename (optional) */
    char            *contentType;       /* Content type for next item */
    char            *tmpPath;           /* Current temp filename for upload data */
    char            *name;              /* Form field name keyword value */
} Upload;

/********************************** Forwards **********************************/

static void addUploadFile(HttpStream *stream, HttpUploadFile *upfile);
static Upload *allocUpload(HttpQueue *q);
static void cleanuploadedFiles(HttpStream *stream);
static void closeUpload(HttpQueue *q);
static int createUploadFile(HttpStream *stream, Upload *up);
static char *getBoundary(char *buf, ssize bufLen, char *boundary, ssize boundaryLen, bool *pureData);
static char *getNextUploadToken(MprBuf *content);
static cchar *getUploadDir(HttpStream *stream);
static void incomingUpload(HttpQueue *q, HttpPacket *packet);
static void incomingUploadService(HttpQueue *q);
static void manageHttpUploadFile(HttpUploadFile *file, int flags);
static void manageUpload(Upload *up, int flags);
static int openUpload(HttpQueue *q);
static int  processUploadBoundary(HttpQueue *q, char *line);
static int  processUploadHeader(HttpQueue *q, char *line);
static int  processUploadData(HttpQueue *q);
static void renameUploadedFiles(HttpStream *stream);
static bool validUploadChars(cchar *uri);

/************************************* Code ***********************************/

PUBLIC int httpOpenUploadFilter()
{
    HttpStage     *filter;

    if ((filter = httpCreateFilter("uploadFilter", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    HTTP->uploadFilter = filter;
    filter->open = openUpload;
    filter->close = closeUpload;
    filter->incoming = incomingUpload;
    filter->incomingService = incomingUploadService;
    return 0;
}


/*
    Initialize the upload filter for a new request
 */
static Upload *allocUpload(HttpQueue *q)
{
    HttpStream  *stream;
    HttpRx      *rx;
    Upload      *up;
    cchar       *uploadDir;
    char        *boundary;

    stream = q->stream;
    rx = stream->rx;
    if ((up = mprAllocObj(Upload, manageUpload)) == 0) {
        return 0;
    }
    q->queueData = up;
    up->contentState = HTTP_UPLOAD_BOUNDARY;

    uploadDir = getUploadDir(stream);
    httpSetParam(stream, "UPLOAD_DIR", uploadDir);

    if ((boundary = strstr(rx->mimeType, "boundary=")) != 0) {
        boundary += 9;
        up->boundary = sjoin("--", boundary, NULL);
        up->boundaryLen = strlen(up->boundary);
    }
    if (up->boundaryLen == 0 || *up->boundary == '\0') {
        httpError(stream, HTTP_CODE_BAD_REQUEST, "Bad boundary");
        return 0;
    }
    return up;
}


static void manageUpload(Upload *up, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(up->currentFile);
        mprMark(up->file);
        mprMark(up->boundary);
        mprMark(up->clientFilename);
        mprMark(up->contentType);
        mprMark(up->tmpPath);
        mprMark(up->name);
    }
}


static void freeUpload(HttpQueue *q)
{
    HttpUploadFile  *file;
    Upload          *up;

    if ((up = q->queueData) != 0) {
        if (up->currentFile) {
            file = up->currentFile;
            file->filename = 0;
        }
        q->queueData = 0;
    }
}


static int openUpload(HttpQueue *q)
{
    /* Necessary because we want closeUpload to be able to clean files */
    return 0;
}


static void closeUpload(HttpQueue *q)
{
    Upload      *up;

    if ((up = q->queueData) != 0) {
        cleanuploadedFiles(q->stream);
        freeUpload(q);
    }
}


static void incomingUpload(HttpQueue *q, HttpPacket *packet)
{
    HttpStream  *stream;

    assert(packet);
    stream = q->stream;

    if (!stream->rx->upload) {
        httpPutPacketToNext(q, packet);
        httpRemoveQueue(q);
        return;
    }
    if (stream->error) {
        //  Discard packet
        return;
    }
    httpJoinPacketForService(q, packet, HTTP_SCHEDULE_QUEUE);
}


/*
    Incoming data acceptance routine. The service queue is used, but not a service routine as the data is processed
    immediately. Partial data is buffered on the service queue until a correct mime boundary is seen.
 */
static void incomingUploadService(HttpQueue *q)
{
    HttpStream  *stream;
    HttpPacket  *packet;
    HttpRx      *rx;
    MprBuf      *content;
    Upload      *up;
    char        *line;
    ssize       mark;
    int         done, rc;

    if ((up = q->queueData) == 0 && ((up = allocUpload(q)) == 0)) {
        return;
    }
    stream = q->stream;
    rx = stream->rx;

    for (packet = q->first; packet; packet = q->first) {
        if (packet->flags & HTTP_PACKET_END) {
            if (up->contentState != HTTP_UPLOAD_CONTENT_END) {
                httpError(stream, HTTP_CODE_BAD_REQUEST, "Client supplied insufficient upload data");
            }
            renameUploadedFiles(q->stream);
            httpPutPacketToNext(q, packet);
            break;
        }
        content = packet->content;
        mark = httpGetPacketLength(packet);

        for (done = 0; !done; ) {
            switch (up->contentState) {
            case HTTP_UPLOAD_BOUNDARY:
            case HTTP_UPLOAD_CONTENT_HEADER:
                if ((line = getNextUploadToken(content)) == 0) {
                    /* Incomplete line */
                    break;
                }
                if (up->contentState == HTTP_UPLOAD_BOUNDARY) {
                    if (processUploadBoundary(q, line) < 0) {
                        done++;
                    }
                } else if (processUploadHeader(q, line) < 0) {
                    done++;
                }
                break;

            case HTTP_UPLOAD_CONTENT_DATA:
                rc = processUploadData(q);
                if (rc < 0) {
                    done++;
                }
                if (httpGetPacketLength(packet) < up->boundaryLen) {
                    /*  Incomplete boundary - return to get more data */
                    done++;
                }
                break;

            case HTTP_UPLOAD_CONTENT_END:
                //  May have already consumed the trailing CRLF
                if (mprGetBufLength(content) >= 2) {
                    mprAdjustBufStart(content, 2);
                }
                done++;
                break;
            }
        }
        q->count -= (mark - httpGetPacketLength(packet));
        assert(q->count >= 0);

        /*
            Remove or compact the buffer. Often residual data after the boundary for the next block.
         */
        if (httpGetPacketLength(packet) == 0) {
            httpGetPacket(q);
        } else if (packet != rx->headerPacket) {
            mprCompactBuf(content);
        }
    }
}


static char *getNextUploadToken(MprBuf *content)
{
    char    *line, *nextTok;
    /*
        Parse the next input line
     */
    line = mprGetBufStart(content);
    if ((nextTok = memchr(line, '\n', mprGetBufLength(content))) == 0) {
        /* Incomplete line */
        return 0;
    }
    *nextTok++ = '\0';
    mprAdjustBufStart(content, (int) (nextTok - line));
    return strim(line, "\r", MPR_TRIM_END);
}


/*
    Process the mime boundary division
    Returns  < 0 on a request or state error
            == 0 if successful
 */
static int processUploadBoundary(HttpQueue *q, char *line)
{
    HttpStream  *stream;
    Upload      *up;

    stream = q->stream;
    up = q->queueData;

    /*
        Expecting a multipart boundary string
     */
    if (strncmp(up->boundary, line, up->boundaryLen) != 0) {
        httpError(stream, HTTP_CODE_BAD_REQUEST, "Bad upload state. Incomplete boundary");
        return MPR_ERR_BAD_STATE;
    }
    if (line[up->boundaryLen] && strcmp(&line[up->boundaryLen], "--") == 0) {
        up->contentState = HTTP_UPLOAD_CONTENT_END;
    } else {
        up->contentState = HTTP_UPLOAD_CONTENT_HEADER;
    }
    return 0;
}


/*
    Expecting content headers. A blank line indicates the start of the data.
    Returns  < 0  Request or state error
    Returns == 0  Successfully parsed the input line.
 */
static int processUploadHeader(HttpQueue *q, char *line)
{
    HttpStream      *stream;
    Upload          *up;
    char            *key, *headerTok, *rest, *nextPair, *value;

    stream = q->stream;
    up = q->queueData;

    if (line[0] == '\0') {
        up->contentState = HTTP_UPLOAD_CONTENT_DATA;
        return 0;
    }

    headerTok = line;
    stok(line, ": ", &rest);

    if (scaselesscmp(headerTok, "Content-Disposition") == 0) {
        /*
            The content disposition header describes either a form
            variable or an uploaded file.

            Content-Disposition: form-data; name="field1"
            >>blank line
            Field Data
            ---boundary

            Content-Disposition: form-data; name="field1" ->
                filename="user.file"
            >>blank line
            File data
            ---boundary
         */
        key = rest;
        up->name = up->clientFilename = 0;

        while (key && stok(key, ";\r\n", &nextPair)) {

            key = strim(key, " ", MPR_TRIM_BOTH);
            stok(key, "= ", &value);
            value = strim(value, "\"", MPR_TRIM_BOTH);

            if (scaselesscmp(key, "form-data") == 0) {
                /* Nothing to do */

            } else if (scaselesscmp(key, "name") == 0) {
                up->name = sclone(value);

            } else if (scaselesscmp(key, "filename") == 0) {
                if (up->name == 0) {
                    httpError(stream, HTTP_CODE_BAD_REQUEST, "Bad upload state. Missing name field");
                    return MPR_ERR_BAD_STATE;
                }
                /*
                    Client filenames must be simple filenames without illegal characters or path separators.
                    We are deliberately restrictive here to assist users that may use the clientFilename in shell scripts.
                    They MUST still sanitize for their environment, but some extra caution is worthwhile.
                 */
                if (*value == '.' || !validUploadChars(value)) {
                    httpError(stream, HTTP_CODE_BAD_REQUEST, "Bad upload client filename.");
                    return MPR_ERR_BAD_STATE;
                }
                up->clientFilename = mprNormalizePath(value);
            }
            key = nextPair;
        }

    } else if (scaselesscmp(headerTok, "Content-Type") == 0) {
        up->contentType = sclone(rest);
    }
    return 0;
}


static int createUploadFile(HttpStream *stream, Upload *up)
{
    HttpUploadFile  *file;
    cchar           *uploadDir;

    /*
        Create the file to hold the uploaded data
     */
    uploadDir = getUploadDir(stream);
    up->tmpPath = mprGetTempPath(uploadDir);
    if (up->tmpPath == 0) {
        if (!mprPathExists(uploadDir, X_OK)) {
            mprLog("http error", 0, "Cannot access upload directory %s", uploadDir);
        }
        httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR,
            "Cannot create upload temp file %s. Check upload temp dir %s", up->tmpPath, uploadDir);
        return MPR_ERR_CANT_OPEN;
    }
    httpLog(stream->trace, "rx.upload.file", "context", "name:%s, clientFilename:%s, filename:%s",
        up->name, up->clientFilename, up->tmpPath);

    up->file = mprOpenFile(up->tmpPath, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600);
    if (up->file == 0) {
        httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot open upload temp file %s", up->tmpPath);
        return MPR_ERR_BAD_STATE;
    }
    /*
        Create the file
     */
    file = up->currentFile = mprAllocObj(HttpUploadFile, manageHttpUploadFile);
    file->clientFilename = up->clientFilename;
    file->contentType = up->contentType;
    file->filename = up->tmpPath;
    file->name = up->name;
    addUploadFile(stream, file);
    return 0;
}


static void manageHttpUploadFile(HttpUploadFile *file, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(file->name);
        mprMark(file->filename);
        mprMark(file->clientFilename);
        mprMark(file->contentType);
    }
}


static void defineFileFields(HttpQueue *q, Upload *up)
{
    HttpStream      *stream;
    HttpUploadFile  *file;
    char            *key;

    stream = q->stream;
#if DEPRECATED
    if (stream->tx->handler && stream->tx->handler == stream->http->ejsHandler) {
        return;
    }
#endif
    up = q->queueData;
    file = up->currentFile;
    key = sjoin("FILE_CLIENT_FILENAME_", up->name, NULL);

    if (file->clientFilename) {
        httpSetParam(stream, key, file->clientFilename);
    }
    key = sjoin("FILE_CONTENT_TYPE_", up->name, NULL);
    httpSetParam(stream, key, file->contentType);

    key = sjoin("FILE_FILENAME_", up->name, NULL);
    httpSetParam(stream, key, file->filename);

    key = sjoin("FILE_SIZE_", up->name, NULL);
    httpSetIntParam(stream, key, (int) file->size);
}


static int writeToFile(HttpQueue *q, char *data, ssize len)
{
    HttpStream      *stream;
    HttpUploadFile  *file;
    HttpLimits      *limits;
    Upload          *up;
    ssize           rc;

    stream = q->stream;
    limits = stream->limits;
    up = q->queueData;
    file = up->currentFile;

    if ((file->size + len) > limits->uploadSize) {
        /*
            Abort the connection as we don't want the load of receiving the entire body
         */
        httpLimitError(stream, HTTP_ABORT | HTTP_CODE_REQUEST_TOO_LARGE, "Uploaded file exceeds maximum %lld",
            limits->uploadSize);
        return MPR_ERR_CANT_WRITE;
    }
    if (len > 0) {
        /*
            File upload. Write the file data.
         */
        rc = mprWriteFile(up->file, data, len);
        if (rc != len) {
            httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR,
                "Cannot write to upload temp file %s, rc %zd, errno %d", up->tmpPath, rc, mprGetOsError());
            return MPR_ERR_CANT_WRITE;
        }
        file->size += len;
        stream->rx->bytesUploaded += len;
    }
    return 0;
}


/*
    Process the content data.
    Returns < 0 on error
            == 0 when more data is needed
            == 1 when data successfully written
 */
static int processUploadData(HttpQueue *q)
{
    HttpStream      *stream;
    HttpPacket      *packet;
    MprBuf          *content;
    Upload          *up;
    ssize           size, dataLen;
    bool            pureData;
    char            *data, *bp;

    stream = q->stream;
    up = q->queueData;
    content = q->first->content;
    packet = 0;

    if (up->clientFilename && !up->tmpPath) {
        if (createUploadFile(stream, up) < 0) {
            return MPR_ERR_BAD_STATE;
        }
    }

    size = mprGetBufLength(content);
    if (size < up->boundaryLen) {
        /*  Incomplete boundary. Return and get more data */
        return 0;
    }
    /*
        Expect a boundary at the end of the data
     */
    bp = getBoundary(mprGetBufStart(content), size, up->boundary, up->boundaryLen, &pureData);
    if (bp == 0) {
        if (up->tmpPath) {
            /*
                No signature found yet. probably more data to come. Must handle split boundaries.
             */
            data = mprGetBufStart(content);
            dataLen = pureData ? size : (size - (up->boundaryLen - 1));
            if (dataLen > 0) {
                if (writeToFile(q, mprGetBufStart(content), dataLen) < 0) {
                    return MPR_ERR_CANT_WRITE;
                }
            }
            mprAdjustBufStart(content, dataLen);
        }
        /*
            Can't see boundary to mark the end of the data. Return and get more data.
         */
        return -1;
    }

    /*
        Have a complete data part
     */
    data = mprGetBufStart(content);
    dataLen = (bp) ? (bp - data) : mprGetBufLength(content);

    if (dataLen > 0) {
        mprAdjustBufStart(content, dataLen);
        /*
            This is the CRLF before the boundary
         */
        if (dataLen >= 2 && data[dataLen - 2] == '\r' && data[dataLen - 1] == '\n') {
            dataLen -= 2;
        }
        if (up->tmpPath) {
            /*
                Write the last bit of file data and add to the list of files and define environment variables
             */
            if (writeToFile(q, data, dataLen) < 0) {
                return MPR_ERR_CANT_WRITE;
            }
            defineFileFields(q, up);

        } else {
            if (packet == 0) {
                packet = httpCreatePacket(ME_BUFSIZE);
            }
            /*
                Normal string form data variables. Copy data to post content and then decode and set as parameter.
             */
            data[dataLen] = '\0';
            httpSetParam(stream, up->name, data);

            if (httpGetPacketLength(packet) > 0 || q->nextQ->count > 0) {
                /*
                    Need to add back www-form-urlencoding separators so CGI/PHP can see normal POST body
                 */
                mprPutCharToBuf(packet->content, '&');
            } else {
                stream->rx->mimeType = sclone("application/x-www-form-urlencoded");
            }
            mprPutToBuf(packet->content, "%s=%s", up->name, data);
        }
    }
    if (up->tmpPath) {
        /*
            Now have all the data (we've seen the boundary)
         */
        mprCloseFile(up->file);
        up->file = 0;
        up->tmpPath = 0;
    }
    if (packet) {
        httpPutPacketToNext(q, packet);
    }
    up->contentState = HTTP_UPLOAD_BOUNDARY;
    return 0;
}


/*
    Find the boundary signature in memory. Returns pointer to the first match.
 */
static char *getBoundary(char *buf, ssize bufLen, char *boundary, ssize boundaryLen, bool *pureData)
{
    char    *cp, *endp;
    char    first;

    assert(buf);
    assert(boundary);
    assert(boundaryLen > 0);

    first = *boundary & 0xff;
    endp = &buf[bufLen];

    for (cp = buf; cp < endp; cp++) {
        if ((cp = memchr(cp, first, endp - cp)) == 0) {
            *pureData = 1;
            return 0;
        }
        /* Potential boundary */
        if ((endp - cp) < boundaryLen) {
            *pureData = 0;
            return 0;
        }
        if (memcmp(cp, boundary, boundaryLen) == 0) {
            *pureData = 0;
            return cp;
        }
    }
    *pureData = 0;
    return 0;
}


static void addUploadFile(HttpStream *stream, HttpUploadFile *upfile)
{
    HttpRx   *rx;

    rx = stream->rx;
    if (rx->files == 0) {
        rx->files = mprCreateList(0, MPR_LIST_STABLE);
    }
    mprAddItem(rx->files, upfile);
}


static void cleanuploadedFiles(HttpStream *stream)
{
    HttpRx          *rx;
    HttpUploadFile  *file;
    int             index;

    rx = stream->rx;

    for (ITERATE_ITEMS(rx->files, file, index)) {
        if (file->filename && rx->route) {
            if (rx->route->autoDelete && !rx->route->renameUploads) {
                mprDeletePath(file->filename);
            }
            file->filename = 0;
        }
    }
}


static void renameUploadedFiles(HttpStream *stream)
{
    HttpRx          *rx;
    HttpUploadFile  *file;
    cchar           *path, *uploadDir;
    int             index;

    rx = stream->rx;
    uploadDir = getUploadDir(stream);

    for (ITERATE_ITEMS(rx->files, file, index)) {
        if (file->filename && rx->route) {
            if (rx->route->renameUploads) {
                if (file->clientFilename) {
                    path = mprJoinPath(uploadDir, file->clientFilename);
                    if (rename(file->filename, path) != 0) {
                        mprLog("http error", 0, "Cannot rename %s to %s", file->filename, path);
                    }
                    file->filename = path;
                }
            }
        }
    }
}


static cchar *getUploadDir(HttpStream *stream)
{
    cchar   *uploadDir;

    if ((uploadDir = httpGetDir(stream->rx->route, "upload")) == 0) {
#if ME_WIN_LIKE
        uploadDir = mprNormalizePath(getenv("TEMP"));
#else
        uploadDir = sclone("/tmp");
#endif
    }
    return uploadDir;
}


static bool validUploadChars(cchar *uri)
{
    ssize   pos;

    if (uri == 0) {
        return 1;
    }
    pos = strspn(uri, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#[]@!$&'()*+,;=% ");
    if (pos < slen(uri)) {
        return 0;
    }
    return 1;
}

#endif /* ME_HTTP_UPLOAD */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/uri.c ************/

/*
    uri.c - URI manipulation routines
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/********************************** Forwards **********************************/

static cchar *expandRouteName(HttpStream *stream, cchar *routeName);
static int getPort(HttpUri *uri);
static int getDefaultPort(cchar *scheme);
static void manageUri(HttpUri *uri, int flags);
static void trimPathToDirname(HttpUri *uri);
static char *actionRoute(HttpRoute *route, cchar *controller, cchar *action);

/************************************ Code ************************************/
/*
    Create and initialize a URI. This accepts full URIs with schemes (http:) and partial URLs
    Support IPv4 and [IPv6]. Supported forms:

        SCHEME://[::]:PORT/URI
        SCHEME://HOST:PORT/URI
        [::]:PORT/URI
        :PORT/URI
        HOST:PORT/URI
        PORT/URI
        /URI
        URI

        NOTE: HOST/URI is not supported and requires a scheme prefix. This is because it is ambiguous with a
        relative uri path.

    Missing fields are null or zero.
 */
PUBLIC HttpUri *httpCreateUri(cchar *uri, int flags)
{
    HttpUri     *up;
    char        *tok, *next;

    if ((up = mprAllocObj(HttpUri, manageUri)) == 0) {
        return 0;
    }
    tok = sclone(uri);

    /*
        [scheme://][hostname[:port]][/path[.ext]][#ref][?query]
        First trim query and then reference from the end
     */
    if ((next = schr(tok, '?')) != 0) {
        *next++ = '\0';
        up->query = sclone(next);
    }
    if ((next = schr(tok, '#')) != 0) {
        *next++ = '\0';
        up->reference = sclone(next);
    }

    /*
        [scheme://][hostname[:port]][/path]
     */
    if ((next = scontains(tok, "://")) != 0) {
        up->scheme = snclone(tok, (next - tok));
        if (smatch(up->scheme, "http")) {
            if (flags & HTTP_COMPLETE_URI) {
                up->port = 80;
            }
        } else if (smatch(up->scheme, "ws")) {
            if (flags & HTTP_COMPLETE_URI) {
                up->port = 80;
            }
            up->webSockets = 1;
        } else if (smatch(up->scheme, "https")) {
            if (flags & HTTP_COMPLETE_URI) {
                up->port = 443;
            }
            up->secure = 1;
        } else if (smatch(up->scheme, "wss")) {
            if (flags & HTTP_COMPLETE_URI) {
                up->port = 443;
            }
            up->secure = 1;
            up->webSockets = 1;
        }
        tok = &next[3];
    }

    /*
        [hostname[:port]][/path]
     */
    if (*tok == '[' && ((next = strchr(tok, ']')) != 0)) {
        /* IPv6  [::]:port/uri */
        up->host = snclone(&tok[1], (next - tok) - 1);
        tok = ++next;

    } else if (*tok && *tok != '/' && *tok != ':' && (up->scheme || strchr(tok, ':'))) {
        /*
            Supported forms:
                scheme://hostname
                hostname:port/
         */
        if ((next = spbrk(tok, ":/")) == 0) {
            next = &tok[slen(tok)];
        }
        up->host = snclone(tok, next - tok);
        tok = next;
    }
    assert(tok);

    /* [:port][/path] */
    if (*tok == ':') {
        up->port = atoi(++tok);
        if ((tok = schr(tok, '/')) == 0) {
            tok = "";
        }
        if (up->port == 4443 || up->port == 443) {
            up->secure = 1;
        }
    }
    assert(tok);

    /* [/path] */
    if (*tok) {
        up->path = sclone(tok);
        /* path[.ext[/extra]] */
        if ((tok = srchr(up->path, '.')) != 0) {
            if (tok[1]) {
                if ((next = srchr(up->path, '/')) != 0) {
                    if (next < tok) {
                        up->ext = sclone(++tok);
                    }
                } else {
                    up->ext = sclone(++tok);
                }
            }
        }
    }
    if (flags & (HTTP_COMPLETE_URI | HTTP_COMPLETE_URI_PATH)) {
        if (up->path == 0 || *up->path == '\0') {
            up->path = sclone("/");
        }
    }
    up->secure = smatch(up->scheme, "https") || smatch(up->scheme, "wss");
    up->webSockets = smatch(up->scheme, "ws") || smatch(up->scheme, "wss");

    if (flags & HTTP_COMPLETE_URI) {
        if (!up->scheme) {
            up->scheme = sclone("http");
        }
        if (!up->host) {
            up->host = sclone("localhost");
        }
        if (!up->port) {
            up->port = up->secure ? 443 : 80;
        }
    }
    up->valid = httpValidUriChars(uri);
    return up;
}


static void manageUri(HttpUri *uri, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(uri->scheme);
        mprMark(uri->host);
        mprMark(uri->path);
        mprMark(uri->ext);
        mprMark(uri->reference);
        mprMark(uri->query);
    }
}


/*
    Create and initialize a URI. This accepts full URIs with schemes (http:) and partial URLs
 */
PUBLIC HttpUri *httpCreateUriFromParts(cchar *scheme, cchar *host, int port, cchar *path, cchar *reference, cchar *query, int flags)
{
    HttpUri     *up;
    char        *cp, *tok;

    if ((up = mprAllocObj(HttpUri, manageUri)) == 0) {
        up->valid = 0;
        return 0;
    }
    if (!httpValidUriChars(scheme) || !httpValidUriChars(host) || !httpValidUriChars(path) ||
        !httpValidUriChars(reference) || !httpValidUriChars(query)) {
        up->valid = 0;
        return up;
    }
    if (scheme) {
        up->scheme = sclone(scheme);
        up->secure = (smatch(up->scheme, "https") || smatch(up->scheme, "wss"));
        up->webSockets = (smatch(up->scheme, "ws") || smatch(up->scheme, "wss"));

    } else if (flags & HTTP_COMPLETE_URI) {
        up->scheme = "http";
    }
    if (host) {
        if (*host == '[' && ((cp = strchr(host, ']')) != 0)) {
            up->host = snclone(&host[1], (cp - host) - 2);
            if ((cp = schr(++cp, ':')) && port == 0) {
                port = (int) stoi(++cp);
            }
        } else {
            up->host = sclone(host);
            if ((cp = schr(up->host, ':')) && port == 0) {
                port = (int) stoi(++cp);
            }
        }
    } else if (flags & HTTP_COMPLETE_URI) {
        up->host = sclone("localhost");
    }
    if (port) {
        up->port = port;
    }
    if (path) {
        while (path[0] == '/' && path[1] == '/') {
            path++;
        }
        up->path = sclone(path);
    }
    if (flags & (HTTP_COMPLETE_URI | HTTP_COMPLETE_URI_PATH)) {
        if (up->path == 0 || *up->path == '\0') {
            up->path = sclone("/");
        }
    }
    if (reference) {
        up->reference = sclone(reference);
    }
    if (query) {
        up->query = sclone(query);
    }
    if ((tok = srchr(up->path, '.')) != 0) {
        if ((cp = srchr(up->path, '/')) != 0) {
            if (cp <= tok) {
                up->ext = sclone(&tok[1]);
            }
        } else {
            up->ext = sclone(&tok[1]);
        }
    }
    up->valid = 1;
    return up;
}


PUBLIC HttpUri *httpCloneUri(HttpUri *base, int flags)
{
    HttpUri     *up;
    cchar       *path, *cp, *tok;

    if ((up = mprAllocObj(HttpUri, manageUri)) == 0) {
        up->valid = 0;
        return 0;
    }
    if (!base || !base->valid) {
        up->valid = 0;
        return up;
    }
    if (base->scheme) {
        up->scheme = sclone(base->scheme);
    } else if (flags & HTTP_COMPLETE_URI) {
        up->scheme = sclone("http");
    }
    up->secure = (smatch(up->scheme, "https") || smatch(up->scheme, "wss"));
    up->webSockets = (smatch(up->scheme, "ws") || smatch(up->scheme, "wss"));
    if (base->host) {
        up->host = sclone(base->host);
    } else if (flags & HTTP_COMPLETE_URI) {
        up->host = sclone("localhost");
    }
    if (base->port) {
        up->port = base->port;
    } else if (flags & HTTP_COMPLETE_URI) {
        up->port = up->secure ? 443 : 80;
    }
    path = base->path;
    if (path) {
        while (path[0] == '/' && path[1] == '/') {
            path++;
        }
        up->path = sclone(path);
    }
    if (flags & (HTTP_COMPLETE_URI | HTTP_COMPLETE_URI_PATH)) {
        if (up->path == 0 || *up->path == '\0') {
            up->path = sclone("/");
        }
    }
    if (base->reference) {
        up->reference = sclone(base->reference);
    }
    if (base->query) {
        up->query = sclone(base->query);
    }
    if (up->path && (tok = srchr(up->path, '.')) != 0) {
        if ((cp = srchr(up->path, '/')) != 0) {
            if (cp <= tok) {
                up->ext = sclone(&tok[1]);
            }
        } else {
            up->ext = sclone(&tok[1]);
        }
    }
    up->valid = 1;
    return up;
}


/*
    Complete the "uri" using missing parts from base
 */
PUBLIC HttpUri *httpCompleteUri(HttpUri *uri, HttpUri *base)
{
    if (!uri) {
        return 0;
    }
    if (base) {
        if (!uri->host) {
            uri->host = base->host;
            if (!uri->port) {
                uri->port = base->port;
            }
        }
        if (!uri->scheme) {
            uri->scheme = base->scheme;
        }
        if (!uri->path) {
            uri->path = base->path;
            if (!uri->query) {
                uri->query = base->query;
            }
            if (!uri->reference) {
                uri->reference = base->reference;
            }
        }
    }
    if (!uri->scheme) {
        uri->scheme = sclone("http");
    }
    if (!uri->host) {
        uri->host = sclone("localhost");
    }
    if (!uri->path) {
        uri->path = sclone("/");
    }
    uri->secure = (smatch(uri->scheme, "https") || smatch(uri->scheme, "wss"));
    uri->webSockets = (smatch(uri->scheme, "ws") || smatch(uri->scheme, "wss"));
    return uri;
}


/*
    Format a string URI from parts
 */
PUBLIC char *httpFormatUri(cchar *scheme, cchar *host, int port, cchar *path, cchar *reference, cchar *query, int flags)
{
    char    *uri;
    cchar   *portStr, *hostDelim, *portDelim, *pathDelim, *queryDelim, *referenceDelim, *cp;

    portDelim = "";
    portStr = "";
    hostDelim = "";

    if (flags & HTTP_COMPLETE_URI) {
        if (scheme == 0 || *scheme == '\0') {
            scheme = "http";
        }
        if (host == 0 || *host == '\0') {
            if (port || path || reference || query) {
                host = "localhost";
            }
        }
    }
    if (scheme) {
        hostDelim = "://";
    }
    if (!host) {
        host = "";
    }
    if (mprIsIPv6(host)) {
        if (*host != '[') {
            host = sfmt("[%s]", host);
        } else if ((cp = scontains(host, "]:")) != 0) {
            port = 0;
        }
    } else if (schr(host, ':')) {
        port = 0;
    }
    if (port != 0 && port != getDefaultPort(scheme)) {
        portStr = itos(port);
        portDelim = ":";
    }
    if (scheme == 0) {
        scheme = "";
    }
    if (path && *path) {
        if (*host) {
            pathDelim = (*path == '/') ? "" :  "/";
        } else {
            pathDelim = "";
        }
    } else {
        pathDelim = path = "";
    }
    if (reference && *reference) {
        referenceDelim = "#";
    } else {
        referenceDelim = reference = "";
    }
    if (query && *query) {
        queryDelim = "?";
    } else {
        queryDelim = query = "";
    }
    if (*portDelim) {
        uri = sjoin(scheme, hostDelim, host, portDelim, portStr, pathDelim, path, referenceDelim, reference,
            queryDelim, query, NULL);
    } else {
        uri = sjoin(scheme, hostDelim, host, pathDelim, path, referenceDelim, reference, queryDelim, query, NULL);
    }
    return uri;
}


/*
    This returns a URI relative to the base for the given target

    uri = target.relative(base)
 */
PUBLIC HttpUri *httpGetRelativeUri(HttpUri *base, HttpUri *target, int clone)
{
    HttpUri     *uri;
    cchar       *bp, *startDiff, *tp;
    char        *basePath, *cp, *path;
    int         i, baseSegments, commonSegments;

    if (base == 0) {
        return clone ? httpCloneUri(target, 0) : target;
    }
    if (target == 0) {
        return clone ? httpCloneUri(base, 0) : base;
    }
    if (!(target->path && target->path[0] == '/') || !((base->path && base->path[0] == '/'))) {
        /* If target is relative, just use it. If base is relative, cannot use it because we don't know where it is */
        return (clone) ? httpCloneUri(target, 0) : target;
    }
    if (base->scheme && target->scheme && scmp(base->scheme, target->scheme) != 0) {
        return (clone) ? httpCloneUri(target, 0) : target;
    }
    if (base->host && target->host && (base->host && scmp(base->host, target->host) != 0)) {
        return (clone) ? httpCloneUri(target, 0) : target;
    }
    if (getPort(base) != getPort(target)) {
        return (clone) ? httpCloneUri(target, 0) : target;
    }
    if ((basePath = httpNormalizeUriPath(base->path)) == 0) {
        return 0;
    }
    /* Count trailing "/" */
    for (baseSegments = 0, bp = basePath; *bp; bp++) {
        if (*bp == '/') {
            baseSegments++;
        }
    }

    /*
        Find portion of path that matches the base, if any.
     */
    commonSegments = 0;
    for (bp = base->path, tp = startDiff = target->path; *bp && *tp; bp++, tp++) {
        if (*bp == '/') {
            if (*tp == '/') {
                commonSegments++;
                startDiff = tp;
            }
        } else {
            if (*bp != *tp) {
                break;
            }
        }
    }
    if (*startDiff == '/') {
        startDiff++;
    }

    if ((uri = httpCloneUri(target, 0)) == 0) {
        return 0;
    }
    uri->host = 0;
    uri->scheme = 0;
    uri->port = 0;

    uri->path = path = cp = mprAlloc(baseSegments * 3 + (int) slen(target->path) + 2);
    for (i = commonSegments; i < baseSegments; i++) {
        *cp++ = '.';
        *cp++ = '.';
        *cp++ = '/';
    }
    if (*startDiff) {
        strcpy(cp, startDiff);
    } else if (cp > uri->path) {
        /*
            Cleanup trailing separators ("../" is the end of the new path)
         */
        cp[-1] = '\0';
    } else {
        strcpy(path, ".");
    }
    return uri;
}


PUBLIC HttpUri *httpJoinUriPath(HttpUri *result, HttpUri *base, HttpUri *other)
{
    char    *sep;

    if (other->path) {
        if (other->path[0] == '/' || !base->path) {
            result->path = sclone(other->path);
        } else {
            sep = ((base->path[0] == '\0' || base->path[slen(base->path) - 1] == '/') ||
                   (other->path[0] == '\0' || other->path[0] == '/'))  ? "" : "/";
            result->path = sjoin(base->path, sep, other->path, NULL);
        }
    }
    return result;
}


PUBLIC HttpUri *httpJoinUri(HttpUri *uri, int argc, HttpUri **others)
{
    HttpUri     *other;
    int         i;

    if ((uri = httpCloneUri(uri, 0)) == 0) {
        return 0;
    }
    if (!uri->valid) {
        return 0;
    }
    for (i = 0; i < argc; i++) {
        other = others[i];
        if (other->scheme) {
            uri->scheme = sclone(other->scheme);
            uri->port = other->port;
        }
        if (other->host) {
            uri->host = sclone(other->host);
            uri->port = other->port;
        }
        if (other->path) {
            httpJoinUriPath(uri, uri, other);
        }
        if (other->reference) {
            uri->reference = sclone(other->reference);
        }
        if (other->query) {
            uri->query = sclone(other->query);
        }
    }
    uri->ext = mprGetPathExt(uri->path);
    return uri;
}


/*
    Create and resolve a URI link given a set of options.
 */
PUBLIC HttpUri *httpMakeUriLocal(HttpUri *uri)
{
    if (uri) {
        uri->host = 0;
        uri->scheme = 0;
        uri->port = 0;
    }
    return uri;
}


PUBLIC HttpUri *httpNormalizeUri(HttpUri *uri)
{
    if (!uri) {
        return 0;
    }
    uri->path = httpNormalizeUriPath(uri->path);
    return uri;
}


/*
    Normalize a URI path to remove redundant "./", "../" and make separators uniform.
    This will not permit leading '../' segments.
    Does not make an abs path, map separators or change case.
 */
PUBLIC char *httpNormalizeUriPath(cchar *pathArg)
{
    char    *dupPath, *path, *sp, *dp, *mark, **segments;
    int     firstc, j, i, nseg, len;

    if (pathArg == 0 || *pathArg == '\0') {
        return mprEmptyString();
    }
    len = (int) slen(pathArg);
    if ((dupPath = mprAlloc(len + 2)) == 0) {
        return 0;
    }
    strcpy(dupPath, pathArg);

    if ((segments = mprAlloc(sizeof(char*) * (len + 1))) == 0) {
        return 0;
    }
    nseg = len = 0;
    firstc = *dupPath;
    for (mark = sp = dupPath; *sp; sp++) {
        if (*sp == '/') {
            *sp = '\0';
            while (sp[1] == '/') {
                sp++;
            }
            segments[nseg++] = mark;
            len += (int) (sp - mark);
            mark = sp + 1;
        }
    }
    segments[nseg++] = mark;
    len += (int) (sp - mark);
    for (j = i = 0; i < nseg; i++, j++) {
        sp = segments[i];
        if (sp[0] == '.') {
            if (sp[1] == '\0')  {
                if ((i+1) == nseg) {
                    /* Trim trailing "." */
                    segments[j] = "";
                } else {
                    /* Trim intermediate "." */
                    j--;
                }
            } else if (sp[1] == '.' && sp[2] == '\0')  {
                j = max(j - 2, -1);
                if ((i+1) == nseg) {
                    nseg--;
                }
            } else {
                /* .more-chars */
                segments[j] = segments[i];
            }
        } else {
            segments[j] = segments[i];
        }
    }
    nseg = j;
    assert(nseg >= 0);
    if ((path = mprAlloc(len + nseg + 1)) != 0) {
        for (i = 0, dp = path; i < nseg; ) {
            strcpy(dp, segments[i]);
            len = (int) slen(segments[i]);
            dp += len;
            if (++i < nseg || (nseg == 1 && *segments[0] == '\0' && firstc == '/')) {
                *dp++ = '/';
            }
        }
        *dp = '\0';
    }
    return path;
}


PUBLIC HttpUri *httpResolveUri(HttpStream *stream, HttpUri *base, HttpUri *other)
{
    HttpHost        *host;
    HttpEndpoint    *endpoint;
    HttpUri         *current;

    if (!base || !base->valid) {
        return other;
    }
    if (!other || !other->valid) {
        return base;
    }
    current = httpCloneUri(base, 0);

    /*
        Must not inherit the query or reference
     */
    current->query = 0;
    current->reference = 0;

    if (other->scheme && !smatch(current->scheme, other->scheme)) {
        current->scheme = sclone(other->scheme);
        /*
            If the scheme is changed (test above), then accept an explict port.
            If no port, then must not use the current port as the scheme has changed.
         */
        if (other->port) {
            current->port = other->port;
        } else {
            host = stream ? stream->host : httpGetDefaultHost();
            endpoint = smatch(current->scheme, "https") ? host->secureEndpoint : host->defaultEndpoint;
            if (endpoint) {
                current->port = endpoint->port;
            } else {
                current->port = 0;
            }
        }
    }
    if (other->host) {
        current->host = sclone(other->host);
    }
    if (other->port) {
        current->port = other->port;
    }
    if (other->path) {
        trimPathToDirname(current);
        httpJoinUriPath(current, current, other);
        current->path = httpNormalizeUriPath(current->path);
    }
    if (other->reference) {
        current->reference = sclone(other->reference);
    }
    if (other->query) {
        current->query = sclone(other->query);
    }
    current->ext = mprGetPathExt(current->path);
    return current;
}


PUBLIC HttpUri *httpLinkUri(HttpStream *stream, cchar *target, MprHash *options)
{
    HttpRoute       *route, *lroute;
    HttpRx          *rx;
    HttpUri         *uri;
    cchar           *routeName, *action, *controller, *originalAction, *tplate;
    char            *rest;

    assert(stream);

    rx = stream->rx;
    route = rx->route;
    controller = 0;

    if (target == 0) {
        target = "";
    }
    if (*target == '@') {
        target = sjoin("{action: '", target, "'}", NULL);
    }
    if (*target != '{') {
        tplate = target;
        if (!options) {
            options = route->vars;
        }
    } else  {
        if (options) {
            options = mprBlendHash(httpGetOptions(target), options);
        } else {
            options = httpGetOptions(target);
        }
        options = mprBlendHash(options, route->vars);

        /*
            Prep the action. Forms are:
                . @action               # Use the current controller
                . @controller/          # Use "index" as the action
                . @controller/action
         */
        if ((action = httpGetOption(options, "action", 0)) != 0) {
            originalAction = action;
            if (*action == '@') {
                action = &action[1];
            }
            if (strchr(action, '/')) {
                controller = stok((char*) action, "/", (char**) &action);
                action = stok((char*) action, "/", &rest);
            }
            if (controller) {
                httpSetOption(options, "controller", controller);
            } else {
                controller = httpGetParam(stream, "controller", 0);
            }
            if (action == 0 || *action == '\0') {
                action = "list";
            }
            if (action != originalAction) {
                httpSetOption(options, "action", action);
            }
        }
        /*
            Find the template to use. Strategy is this order:
                . options.template
                . options.route.template
                . options.action mapped to a route.template, via:
                . /app/STAR/action
                . /app/controller/action
                . /app/STAR/default
                . /app/controller/default
         */
        if ((tplate = httpGetOption(options, "template", 0)) == 0) {
            if ((routeName = httpGetOption(options, "route", 0)) != 0) {
                routeName = expandRouteName(stream, routeName);
                lroute = httpLookupRoute(stream->host, routeName);
            } else {
                lroute = 0;
            }
            if (!lroute) {
                if ((lroute = httpLookupRoute(stream->host, actionRoute(route, controller, action))) == 0) {
                    if ((lroute = httpLookupRoute(stream->host, actionRoute(route, "{controller}", action))) == 0) {
                        if ((lroute = httpLookupRoute(stream->host, actionRoute(route, controller, "default"))) == 0) {
                            lroute = httpLookupRoute(stream->host, actionRoute(route, "{controller}", "default"));
                        }
                    }
                }
            }
            if (lroute) {
                tplate = lroute->tplate;
            }
        }
        if (!tplate) {
            mprLog("error http", 0, "Cannot find template for URI %s", target);
            target = "/";
        }
    }
    target = httpTemplate(stream, tplate, options);

    if ((uri = httpCreateUri(target, 0)) == 0) {
        return 0;
    }
    return uri;
}


PUBLIC char *httpLink(HttpStream *stream, cchar *target)
{
    return httpLinkEx(stream, target, 0);
}


PUBLIC char *httpLinkEx(HttpStream *stream, cchar *target, MprHash *options)
{
    return httpUriToString(httpLinkUri(stream, target, options), 0);
}


PUBLIC char *httpLinkAbs(HttpStream *stream, cchar *target)
{
    return httpUriToString(httpResolveUri(stream, stream->rx->parsedUri, httpLinkUri(stream, target, 0)), 0);
}


PUBLIC char *httpUriToString(HttpUri *uri, int flags)
{
    if (!uri) {
        return "";
    }
    return httpFormatUri(uri->scheme, uri->host, uri->port, uri->path, uri->reference, uri->query, flags);
}


/*
    Validate a URI path for use in a HTTP request line
    The URI must contain only valid characters and must being with "/" both before and after decoding.
    A decoded, normalized URI path is returned.
 */
PUBLIC char *httpValidateUriPath(cchar *uri)
{
    char    *up;

    if (uri == 0 || *uri != '/') {
        return 0;
    }
    if (!httpValidUriChars(uri)) {
        return 0;
    }
    up = mprUriDecode(uri);
    if ((up = httpNormalizeUriPath(up)) == 0) {
        return 0;
    }
    if (*up != '/' || strchr(up, '\\')) {
        return 0;
    }
    return up;
}


/*
    This tests if the URI has only characters valid to use in a URI before decoding. i.e. It will permit %NN encodings.
 */
PUBLIC bool httpValidUriChars(cchar *uri)
{
    ssize   pos;

    if (uri == 0) {
        return 1;
    }
    pos = strspn(uri, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~:/?#[]@!$&'()*+,;=%");
    if (pos < slen(uri)) {
        return 0;
    }
    return 1;
}


static int getPort(HttpUri *uri)
{
    if (!uri) {
        return 0;
    }
    if (uri->port) {
        return uri->port;
    }
    return (uri->scheme && (smatch(uri->scheme, "https") || smatch(uri->scheme, "wss"))) ? 443 : 80;
}


static int getDefaultPort(cchar *scheme)
{
    return (scheme && (smatch(scheme, "https") || smatch(scheme, "wss"))) ? 443 : 80;
}


static void trimPathToDirname(HttpUri *uri)
{
    char    *path, *cp;
    int     len;

    path = (char*) uri->path;
    len = (int) slen(path);
    if (path[len - 1] == '/') {
        if (len > 1) {
            path[len - 1] = '\0';
        }
    } else {
        if ((cp = srchr(path, '/')) != 0) {
            if (cp > path) {
                *cp = '\0';
            } else {
                cp[1] = '\0';
            }
        } else if (*path) {
            path[0] = '\0';
        }
    }
}


/*
    Limited expansion of route names. Support ~ and ${app} at the start of the route name
 */
static cchar *expandRouteName(HttpStream *stream, cchar *routeName)
{
    if (routeName[0] == '~') {
        return sjoin(httpGetRouteTop(stream), &routeName[1], NULL);
    }
    if (sstarts(routeName, "${app}")) {
        return sjoin(httpGetRouteTop(stream), &routeName[6], NULL);
    }
    return routeName;
}


/*
    Calculate a qualified route name. The form is: /{app}/{controller}/action
 */
static char *actionRoute(HttpRoute *route, cchar *controller, cchar *action)
{
    cchar   *controllerPrefix;

    if (action == 0 || *action == '\0') {
        action = "default";
    }
    if (controller) {
        controllerPrefix = (controller && smatch(controller, "{controller}")) ? "*" : controller;
        return sjoin("^", route->prefix, "/", controllerPrefix, "/", action, NULL);
    } else {
        return sjoin("^", route->prefix, "/", action, NULL);
    }
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/user.c ************/

/*
    user.c - User and Role management

    An internal cache of users is kept for authenticated users.
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



/********************************* Forwards ***********************************/

#undef  GRADUATE_HASH
#define GRADUATE_HASH(auth, field) \
    if (!auth->field) { \
        if (auth->parent && auth->field && auth->field == auth->parent->field) { \
            auth->field = mprCloneHash(auth->parent->field); \
        } else { \
            auth->field = mprCreateHash(0, MPR_HASH_STABLE); \
        } \
    }

static void manageRole(HttpRole *role, int flags);
static void manageUser(HttpUser *user, int flags);

/*********************************** Code *************************************/

static void manageRole(HttpRole *role, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(role->name);
        mprMark(role->abilities);
    }
}


/*
    Define a role with a given name with specified abilities
 */
PUBLIC HttpRole *httpAddRole(HttpAuth *auth, cchar *name, cchar *abilities)
{
    HttpRole    *role;
    char        *ability, *tok;

    GRADUATE_HASH(auth, roles);
    if ((role = mprLookupKey(auth->roles, name)) == 0) {
        if ((role = mprAllocObj(HttpRole, manageRole)) == 0) {
            return 0;
        }
        role->name = sclone(name);
    }
    role->abilities = mprCreateHash(0, 0);
    for (ability = stok(sclone(abilities), " \t", &tok); ability; ability = stok(NULL, " \t", &tok)) {
        mprAddKey(role->abilities, ability, role);
    }
    if (mprAddKey(auth->roles, name, role) == 0) {
        return 0;
    }
    return role;
}


/*
    Compute a set of abilities given a role defined on an auth route. A role can be a role or an ability.
    The role as defined in the auth route specifies the abilities for the role. These are added to the
    given abilities hash.
 */
PUBLIC void httpComputeRoleAbilities(HttpAuth *auth, MprHash *abilities, cchar *role)
{
    MprKey      *ap;
    HttpRole    *rp;

    if ((rp = mprLookupKey(auth->roles, role)) != 0) {
        /* Interpret as a role */
        for (ITERATE_KEYS(rp->abilities, ap)) {
            if (!mprLookupKey(abilities, ap->key)) {
                mprAddKey(abilities, ap->key, MPR->oneString);
            }
        }
    } else {
        /* Not found as a role: Interpret role as an ability */
        mprAddKey(abilities, role, MPR->oneString);
    }
}


/*
    Compute the set of user abilities from the user roles. User ability strings can be either roles or abilities. Expand
    roles into the equivalent set of abilities.
 */
PUBLIC void httpComputeUserAbilities(HttpAuth *auth, HttpUser *user)
{
    MprKey      *rp;

    user->abilities = mprCreateHash(0, 0);
    for (ITERATE_KEYS(user->roles, rp)) {
        httpComputeRoleAbilities(auth, user->abilities, rp->key);
    }
}


/*
    Recompute all user abilities. Used if the role definitions change
 */
PUBLIC void httpComputeAllUserAbilities(HttpAuth *auth)
{
    MprKey      *kp;
    HttpUser    *user;

    for (ITERATE_KEY_DATA(auth->userCache, kp, user)) {
        httpComputeUserAbilities(auth, user);
    }
}


PUBLIC char *httpRolesToAbilities(HttpAuth *auth, cchar *roles, cchar *separator)
{
    MprKey      *ap;
    HttpRole    *rp;
    MprBuf      *buf;
    char        *role, *tok;

    buf = mprCreateBuf(0, 0);
    for (role = stok(sclone(roles), " \t,", &tok); role; role = stok(NULL, " \t,", &tok)) {
        if ((rp = mprLookupKey(auth->roles, role)) != 0) {
            /* Interpret as a role */
            for (ITERATE_KEYS(rp->abilities, ap)) {
                mprPutStringToBuf(buf, ap->key);
                mprPutStringToBuf(buf, separator);
            }
        } else {
            /* Not found as a role: Interpret role as an ability */
            mprPutStringToBuf(buf, role);
            mprPutStringToBuf(buf, separator);
        }
    }
    if (mprGetBufLength(buf) > 0) {
        mprAdjustBufEnd(buf, - slen(separator));
        mprAddNullToBuf(buf);
    }
    return mprBufToString(buf);
}


PUBLIC HttpRole *httpLookupRole(HttpAuth *auth, cchar *role)
{
    return mprLookupKey(auth->roles, role);
}


PUBLIC int httpRemoveRole(HttpAuth *auth, cchar *role)
{
    if (auth->roles == 0 || !mprLookupKey(auth->roles, role)) {
        return MPR_ERR_CANT_ACCESS;
    }
    mprRemoveKey(auth->roles, role);
    return 0;
}


static void manageUser(HttpUser *user, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(user->password);
        mprMark(user->name);
        mprMark(user->abilities);
        mprMark(user->roles);
    }
}


PUBLIC HttpUser *httpAddUser(HttpAuth *auth, cchar *name, cchar *password, cchar *roles)
{
    HttpUser    *user;
    char        *role, *tok;

    if (!auth->userCache) {
        auth->userCache = mprCreateHash(0, 0);
    }
    if ((user = mprLookupKey(auth->userCache, name)) == 0) {
        if ((user = mprAllocObj(HttpUser, manageUser)) == 0) {
            return 0;
        }
        user->name = sclone(name);
    }
    user->password = sclone(password);
    if (roles) {
        user->roles = mprCreateHash(0, 0);
        for (role = stok(sclone(roles), " \t,", &tok); role; role = stok(NULL, " \t,", &tok)) {
            mprAddKey(user->roles, role, MPR->oneString);
        }
        httpComputeUserAbilities(auth, user);
    }
    if (mprAddKey(auth->userCache, name, user) == 0) {
        return 0;
    }
    return user;
}


PUBLIC HttpUser *httpLookupUser(HttpAuth *auth, cchar *name)
{
    return mprLookupKey(auth->userCache, name);
}


PUBLIC int httpRemoveUser(HttpAuth *auth, cchar *name)
{
    if (!mprLookupKey(auth->userCache, name)) {
        return MPR_ERR_CANT_ACCESS;
    }
    mprRemoveKey(auth->userCache, name);
    return 0;
}


PUBLIC void httpSetConnUser(HttpStream *stream, HttpUser *user)
{
    stream->user = user;
}

#undef  GRADUATE_HASH

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/var.c ************/

/*
    var.c -- Manage the request variables
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/



/********************************** Defines ***********************************/

#define HTTP_VAR_HASH_SIZE  61           /* Hash size for vars and params */

/*********************************** Code *************************************/
/*
    Define standard CGI variables
 */
PUBLIC void httpCreateCGIParams(HttpStream *stream)
{
    HttpRx          *rx;
    HttpTx          *tx;
    HttpHost        *host;
    HttpUploadFile  *file;
    MprSocket       *sock;
    MprHash         *svars;
    MprJson         *params;
    int             index;

    rx = stream->rx;
    if ((svars = rx->svars) != 0) {
        /* Do only once */
        return;
    }
    svars = rx->svars = mprCreateHash(HTTP_VAR_HASH_SIZE, MPR_HASH_STABLE);
    tx = stream->tx;
    host = stream->host;
    sock = stream->sock;

    mprAddKey(svars, "ROUTE_HOME", rx->route->home);

    mprAddKey(svars, "AUTH_TYPE", stream->authType);
    mprAddKey(svars, "AUTH_USER", stream->username);
    mprAddKey(svars, "AUTH_ACL", MPR->emptyString);
    mprAddKey(svars, "CONTENT_LENGTH", rx->contentLength);
    mprAddKey(svars, "CONTENT_TYPE", rx->mimeType);
    mprAddKey(svars, "DOCUMENTS", rx->route->documents);
    mprAddKey(svars, "GATEWAY_INTERFACE", sclone("CGI/1.1"));
    mprAddKey(svars, "QUERY_STRING", rx->parsedUri->query);
    mprAddKey(svars, "REMOTE_ADDR", stream->ip);
    mprAddKeyFmt(svars, "REMOTE_PORT", "%d", stream->port);

    /*
        Set to the same as AUTH_USER
     */
    mprAddKey(svars, "REMOTE_USER", stream->username);
    mprAddKey(svars, "REQUEST_METHOD", rx->method);
    mprAddKey(svars, "REQUEST_TRANSPORT", sclone((char*) ((stream->secure) ? "https" : "http")));
    mprAddKey(svars, "SERVER_ADDR", sock->acceptIp);
    mprAddKey(svars, "SERVER_NAME", host->name);
    mprAddKeyFmt(svars, "SERVER_PORT", "%d", sock->acceptPort);
    mprAddKey(svars, "SERVER_PROTOCOL", sclone(httpGetProtocol(stream->net)));
    mprAddKey(svars, "SERVER_SOFTWARE", stream->http->software);

    /*
        For PHP, REQUEST_URI must be the original URI. The SCRIPT_NAME will refer to the new pathInfo
     */
    mprAddKey(svars, "REQUEST_URI", rx->originalUri);

    /*
        URIs are broken into the following: http://{SERVER_NAME}:{SERVER_PORT}{SCRIPT_NAME}{PATH_INFO}
        NOTE: Appweb refers to pathInfo as the app relative URI and scriptName as the app address before the pathInfo.
        In CGI|PHP terms, the scriptName is the appweb rx->pathInfo and the PATH_INFO is the extraPath.
     */
    mprAddKey(svars, "PATH_INFO", rx->extraPath);
    mprAddKeyFmt(svars, "SCRIPT_NAME", "%s%s", rx->scriptName, rx->pathInfo);
    mprAddKey(svars, "SCRIPT_FILENAME", tx->filename);
    if (rx->extraPath) {
        /*
            Only set PATH_TRANSLATED if extraPath is set (CGI spec)
         */
        assert(rx->extraPath[0] == '/');
        mprAddKey(svars, "PATH_TRANSLATED", mprNormalizePath(sfmt("%s%s", rx->route->documents, rx->extraPath)));
    }
    if (rx->files) {
        params = httpGetParams(stream);
        assert(params);
        for (ITERATE_ITEMS(rx->files, file, index)) {
            mprWriteJson(params, sfmt("FILE_%d_FILENAME", index), file->filename, MPR_JSON_STRING);
            mprWriteJson(params, sfmt("FILE_%d_CLIENT_FILENAME", index), file->clientFilename, MPR_JSON_STRING);
            mprWriteJson(params, sfmt("FILE_%d_CONTENT_TYPE", index), file->contentType, MPR_JSON_STRING);
            mprWriteJson(params, sfmt("FILE_%d_NAME", index), file->name, MPR_JSON_STRING);
            mprWriteJson(params, sfmt("FILE_%d_SIZE", index), sfmt("%zd", file->size), MPR_JSON_NUMBER);
        }
    }
    if (stream->http->envCallback) {
        stream->http->envCallback(stream);
    }
}


/*
    Add variables to the params. This comes from the query string and urlencoded post data.
    Make variables for each keyword in a query string. The buffer must be url encoded
    (ie. key=value&key2=value2..., spaces converted to '+' and all else should be %HEX encoded).
 */
static void addParamsFromBuf(HttpStream *stream, cchar *buf, ssize len)
{
    MprJson     *params, *prior;
    char        *newValue, *decoded, *keyword, *value, *tok;
    bool        json;

    assert(stream);
    params = httpGetParams(stream);

    /*
        Json encoded parameters tunneled via the query string. This is used to
        provide additional parameters on GET requests.
     */
    json = scontains(buf, "_encoded_json_") ? 1 : 0;
    if (json) {
        value = mprUriDecode(buf);
        mprParseJsonInto(value, params);
        return;
    }

    decoded = mprAlloc(len + 1);
    decoded[len] = '\0';
    memcpy(decoded, buf, len);

    keyword = stok(decoded, "&", &tok);

    while (keyword != 0) {
        if ((value = strchr(keyword, '=')) != 0) {
            *value++ = '\0';
            value = mprUriDecode(value);
        } else {
            value = MPR->emptyString;
        }
        keyword = mprUriDecode(keyword);
        if (*keyword) {
            /*
                Append to existing keywords
             */
            prior = mprGetJsonObj(params, keyword);
#if (ME_EJS_PRODUCT || ME_EJSCRIPT_PRODUCT)
            if (prior && prior->type == MPR_JSON_VALUE) {
                if (*value) {
                    newValue = sjoin(prior->value, " ", value, NULL);
                    //  Uses SetJson instead of WriteJson which permits embedded . and []
                    mprSetJson(params, keyword, newValue, MPR_JSON_STRING);
                }
            } else {
                mprSetJson(params, keyword, value, MPR_JSON_STRING);
            }
#else
            if (prior && prior->type == MPR_JSON_VALUE) {
                if (*value) {
                    newValue = sjoin(prior->value, " ", value, NULL);
                    mprWriteJson(params, keyword, newValue, MPR_JSON_STRING);
                }
            } else {
                mprWriteJson(params, keyword, value, MPR_JSON_STRING);
            }
#endif
        }
        keyword = stok(0, "&", &tok);
    }
}


PUBLIC void httpAddQueryParams(HttpStream *stream)
{
    HttpRx      *rx;

    rx = stream->rx;
    if (rx->parsedUri->query && !(rx->flags & HTTP_ADDED_QUERY_PARAMS) && !stream->error) {
        addParamsFromBuf(stream, rx->parsedUri->query, slen(rx->parsedUri->query));
        rx->flags |= HTTP_ADDED_QUERY_PARAMS;
    }
}


PUBLIC int httpAddBodyParams(HttpStream *stream)
{
    HttpRx      *rx;
    HttpQueue   *q;
    MprBuf      *content;
    cchar       *method;

    rx = stream->rx;
    q = stream->readq;

    if (rx->eof && (rx->form || rx->upload || rx->json) && !(rx->flags & HTTP_ADDED_BODY_PARAMS) && !rx->route && !stream->error) {
        httpJoinPackets(q, -1);
        if (q->first && q->first->content) {
            content = q->first->content;
            mprAddNullToBuf(content);
            if (rx->json) {
                if (mprParseJsonInto(httpGetBodyInput(stream), httpGetParams(stream)) == 0) {
                    return MPR_ERR_BAD_FORMAT;
                }
            } else {
                addParamsFromBuf(stream, mprGetBufStart(content), mprGetBufLength(content));
            }
        }
        rx->flags |= HTTP_ADDED_BODY_PARAMS;

        if (rx->flags & HTTP_POST && (method = httpGetParam(stream, "-http-method-", 0)) != 0 && !rx->route) {
            if (!scaselessmatch(method, rx->method)) {
                httpLog(stream->trace, "rx.http.mapMethod", "context", "originalMethod:%s, method:%s", rx->method, method);
                httpSetMethod(stream, method);
            }
        }
    }
    return 0;
}


PUBLIC void httpAddJsonParams(HttpStream *stream)
{
    HttpRx      *rx;

    rx = stream->rx;
    if (rx->eof && sstarts(rx->mimeType, "application/json") && !stream->error) {
        if (!(rx->flags & HTTP_ADDED_BODY_PARAMS)) {
            mprParseJsonInto(httpGetBodyInput(stream), httpGetParams(stream));
            rx->flags |= HTTP_ADDED_BODY_PARAMS;
        }
    }
}


PUBLIC MprJson *httpGetParams(HttpStream *stream)
{
    if (stream->rx->params == 0) {
        stream->rx->params = mprCreateJson(MPR_JSON_OBJ);
    }
    return stream->rx->params;
}


PUBLIC int httpTestParam(HttpStream *stream, cchar *var)
{
    return mprGetJsonObj(httpGetParams(stream), var) != 0;
}


PUBLIC MprJson *httpGetParamObj(HttpStream *stream, cchar *var)
{
    return mprGetJsonObj(httpGetParams(stream), var);
}


PUBLIC int httpGetIntParam(HttpStream *stream, cchar *var, int defaultValue)
{
    cchar       *value;

    value = mprGetJson(httpGetParams(stream), var);
    return (value) ? (int) stoi(value) : defaultValue;
}


PUBLIC cchar *httpGetParam(HttpStream *stream, cchar *var, cchar *defaultValue)
{
    cchar       *value;

    value = mprGetJson(httpGetParams(stream), var);
    return (value) ? value : defaultValue;
}


static int sortParam(MprJson **j1, MprJson **j2)
{
    return scmp((*j1)->name, (*j2)->name);
}


/*
    Return the request parameters as a string.
    This will return the exact same string regardless of the order of form parameters.
 */
PUBLIC cchar *httpGetParamsString(HttpStream *stream)
{
    HttpRx      *rx;
    MprJson     *jp, *params;
    MprList     *list;
    char        *buf, *cp;
    ssize       len;
    int         ji, next;

    assert(stream);
    rx = stream->rx;

    if (rx->paramString == 0) {
        if ((params = stream->rx->params) != 0) {
            if ((list = mprCreateList(params->length, 0)) != 0) {
                len = 0;
                for (ITERATE_JSON(params, jp, ji)) {
                    if (jp->type & MPR_JSON_VALUE) {
                        mprAddItem(list, jp);
                        len += slen(jp->name) + slen(jp->value) + 2;
                    }
                }
                if ((buf = mprAlloc(len + 1)) != 0) {
                    mprSortList(list, (MprSortProc) sortParam, 0);
                    cp = buf;
                    for (next = 0; (jp = mprGetNextItem(list, &next)) != 0; ) {
                        strcpy(cp, jp->name); cp += slen(jp->name);
                        *cp++ = '=';
                        strcpy(cp, jp->value); cp += slen(jp->value);
                        *cp++ = '&';
                    }
                    cp[-1] = '\0';
                    rx->paramString = buf;
                }
            }
        }
    }
    return rx->paramString;
}


PUBLIC void httpRemoveParam(HttpStream *stream, cchar *var)
{
    mprRemoveJson(httpGetParams(stream), var);
}


PUBLIC void httpSetParam(HttpStream *stream, cchar *var, cchar *value)
{
    mprSetJson(httpGetParams(stream), var, value, 0);
}


PUBLIC void httpSetIntParam(HttpStream *stream, cchar *var, int value)
{
    mprSetJson(httpGetParams(stream), var, sfmt("%d", value), MPR_JSON_NUMBER);
}


PUBLIC bool httpMatchParam(HttpStream *stream, cchar *var, cchar *value)
{
    return smatch(value, httpGetParam(stream, var, " __UNDEF__ "));
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */


/********* Start of file src/webSockFilter.c ************/

/*
    webSockFilter.c - WebSockets filter support

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/



#if ME_HTTP_WEB_SOCKETS
/********************************** Locals ************************************/
/*
    Message frame states
 */
#define WS_BEGIN       0
#define WS_EXT_DATA    1
#define WS_MSG         2
#define WS_CLOSED      3

static char *codetxt[16] = {
    "cont", "text", "binary", "reserved", "reserved", "reserved", "reserved", "reserved",
    "close", "ping", "pong", "reserved", "reserved", "reserved", "reserved", "reserved",
};

/*
    Frame format

     Byte 0          Byte 1          Byte 2          Byte 3
     0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
    +-+-+-+-+-------+-+-------------+-------------------------------+
    |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
    |I|S|S|S|  (4)  |A|     (7)     |             (16/63)           |
    |N|V|V|V|       |S|             |   (if payload len==126/127)   |
    | |1|2|3|       |K|             |                               |
    +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
    |     Extended payload length continued, if payload len == 127  |
    + - - - - - - - - - - - - - - - +-------------------------------+
    |                               |Masking-key, if MASK set to 1  |
    +-------------------------------+-------------------------------+
    | Masking-key (continued)       |          Payload Data         |
    +-------------------------------- - - - - - - - - - - - - - - - +
    :                     Payload Data continued ...                :
    + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
    |                     Payload Data continued ...                |
    +---------------------------------------------------------------+

    Single message has
        fin == 1
    Fragmented message has
        fin == 0, opcode != 0
        fin == 0, opcode == 0
        fin == 1, opcode == 0

    Common first byte codes:
        0x9B    Fin | /SET

    NOTE: control frames (opcode >= 8) can be sent between fragmented frames
 */
#define GET_FIN(v)              (((v) >> 7) & 0x1)          /* Final fragment */
#define GET_RSV(v)              (((v) >> 4) & 0x7)          /* Reserved (used for extensions) */
#define GET_CODE(v)             ((v) & 0xf)                 /* Packet opcode */
#define GET_MASK(v)             (((v) >> 7) & 0x1)          /* True if dataMask in frame (client send) */
#define GET_LEN(v)              ((v) & 0x7f)                /* Low order 7 bits of length */

#define SET_FIN(v)              (((v) & 0x1) << 7)
#define SET_MASK(v)             (((v) & 0x1) << 7)
#define SET_CODE(v)             ((v) & 0xf)
#define SET_LEN(len, n)         ((uchar)(((len) >> ((n) * 8)) & 0xff))

/*
    Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
    See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
 */
#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static const uchar utfTable[] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
    8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
    0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
    0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
    0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
    1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
    1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
    1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

/********************************** Forwards **********************************/

static void closeWebSock(HttpQueue *q);
static bool flushWsPipe(HttpQueue *q, int flags);
static void incomingWebSockData(HttpQueue *q, HttpPacket *packet);
static void manageWebSocket(HttpWebSocket *ws, int flags);
static int matchWebSock(HttpStream *stream, HttpRoute *route, int dir);
static int openWebSock(HttpQueue *q);
static void outgoingWebSockService(HttpQueue *q);
static int processWebSocketFrame(HttpQueue *q, HttpPacket *packet);
static void readyWebSock(HttpQueue *q);
static int validUTF8(HttpStream *stream, cchar *str, ssize len);
static bool validateText(HttpStream *stream, HttpPacket *packet);
static void webSockPing(HttpStream *stream);
static void webSockTimeout(HttpStream *stream);

static void traceErrorProc(HttpStream *stream, cchar *fmt, ...);

#define traceError(stream, ...) \
    if (stream->trace->level > 0 && PTOI(mprLookupKey(stream->trace->events, "error")) <= stream->trace->level) { \
        traceErrorProc(stream, __VA_ARGS__); \
    } else

/*********************************** Code *************************************/
/*
   WebSocket Filter initialization
 */
PUBLIC int httpOpenWebSockFilter()
{
    HttpStage     *filter;

    if ((filter = httpCreateFilter("webSocketFilter", NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    HTTP->webSocketFilter = filter;
    filter->match = matchWebSock;
    filter->open = openWebSock;
    filter->ready = readyWebSock;
    filter->close = closeWebSock;
    filter->outgoingService = outgoingWebSockService;
    filter->incoming = incomingWebSockData;
    return 0;
}


/*
    Match if the filter is required for this request. This is called twice: once for TX and once for RX. RX first.
 */
static int matchWebSock(HttpStream *stream, HttpRoute *route, int dir)
{
    HttpWebSocket   *ws;
    HttpRx          *rx;
    HttpTx          *tx;
    char            *kind, *tok;
    cchar           *key, *protocols;
    int             version;

    assert(stream);
    assert(route);
    rx = stream->rx;
    tx = stream->tx;
    assert(rx);
    assert(tx);

    if (stream->error) {
        return HTTP_ROUTE_OMIT_FILTER;
    }
    if (httpClientStream(stream)) {
        if (rx->webSocket) {
            return HTTP_ROUTE_OK;
        } else if (tx->parsedUri && tx->parsedUri->webSockets) {
            /* ws:// URI. Client web sockets */
            if ((ws = mprAllocObj(HttpWebSocket, manageWebSocket)) == 0) {
                httpMemoryError(stream);
                return HTTP_ROUTE_OMIT_FILTER;
            }
            rx->webSocket = ws;
            ws->state = WS_STATE_CONNECTING;
            return HTTP_ROUTE_OK;
        }
        return HTTP_ROUTE_OMIT_FILTER;
    }
    if (dir & HTTP_STAGE_TX) {
        return rx->webSocket ? HTTP_ROUTE_OK : HTTP_ROUTE_OMIT_FILTER;
    }
    if (!rx->upgrade || !scaselessmatch(rx->upgrade, "websocket")) {
        return HTTP_ROUTE_OMIT_FILTER;
    }
    if (!rx->hostHeader || !smatch(rx->method, "GET")) {
        return HTTP_ROUTE_OMIT_FILTER;
    }
    if (tx->flags & HTTP_TX_HEADERS_CREATED) {
        return HTTP_ROUTE_OMIT_FILTER;
    }
    version = (int) stoi(httpGetHeader(stream, "sec-websocket-version"));
    if (version < WS_VERSION) {
        httpSetHeader(stream, "Sec-WebSocket-Version", "%d", WS_VERSION);
        httpError(stream, HTTP_CLOSE | HTTP_CODE_BAD_REQUEST, "Unsupported Sec-WebSocket-Version");
        return HTTP_ROUTE_OMIT_FILTER;
    }
    if ((key = httpGetHeader(stream, "sec-websocket-key")) == 0) {
        httpError(stream, HTTP_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad Sec-WebSocket-Key");
        return HTTP_ROUTE_OMIT_FILTER;
    }
    protocols = httpGetHeader(stream, "sec-websocket-protocol");

    if (dir & HTTP_STAGE_RX) {
        if ((ws = mprAllocObj(HttpWebSocket, manageWebSocket)) == 0) {
            httpMemoryError(stream);
            return HTTP_ROUTE_OMIT_FILTER;
        }
        rx->webSocket = ws;
        ws->state = WS_STATE_OPEN;
        ws->preserveFrames = (rx->route->flags & HTTP_ROUTE_PRESERVE_FRAMES) ? 1 : 0;

        /* Just select the first protocol */
        if (route->webSocketsProtocol) {
            for (kind = stok(sclone(protocols), " \t,", &tok); kind; kind = stok(NULL, " \t,", &tok)) {
                if (smatch(route->webSocketsProtocol, kind)) {
                    break;
                }
            }
            if (!kind) {
                httpError(stream, HTTP_CLOSE | HTTP_CODE_BAD_REQUEST, "Unsupported Sec-WebSocket-Protocol");
                return HTTP_ROUTE_OMIT_FILTER;
            }
            ws->subProtocol = sclone(kind);
        } else {
            /* Just pick the first protocol */
            ws->subProtocol = stok(sclone(protocols), " ,", NULL);
        }
        httpSetStatus(stream, HTTP_CODE_SWITCHING);
        httpSetHeader(stream, "Connection", "Upgrade");
        httpSetHeader(stream, "Upgrade", "WebSocket");
        httpSetHeaderString(stream, "Sec-WebSocket-Accept", mprGetSHABase64(sjoin(key, WS_MAGIC, NULL)));
        if (ws->subProtocol && *ws->subProtocol) {
            httpSetHeaderString(stream, "Sec-WebSocket-Protocol", ws->subProtocol);
        }
#if !ME_HTTP_WEB_SOCKETS_STEALTH
        httpSetHeader(stream, "X-Request-Timeout", "%lld", stream->limits->requestTimeout / TPS);
        httpSetHeader(stream, "X-Inactivity-Timeout", "%lld", stream->limits->inactivityTimeout / TPS);
#endif
        if (route->webSocketsPingPeriod) {
            ws->pingEvent = mprCreateLocalEvent(stream->dispatcher, "webSocket", route->webSocketsPingPeriod,
                webSockPing, stream, MPR_EVENT_CONTINUOUS);
        }
        stream->keepAliveCount = 0;
        stream->upgraded = 1;
        rx->eof = 0;
        rx->remainingContent = HTTP_UNLIMITED;
        return HTTP_ROUTE_OK;
    }
    return HTTP_ROUTE_OMIT_FILTER;
}


/*
    Open the filter for a new request
 */
static int openWebSock(HttpQueue *q)
{
    HttpStream      *stream;
    HttpWebSocket   *ws;

    assert(q);
    stream = q->stream;
    ws = stream->rx->webSocket;
    assert(ws);

    q->packetSize = min(stream->limits->packetSize, q->max);
    ws->closeStatus = WS_STATUS_NO_STATUS;
    stream->timeoutCallback = webSockTimeout;

    /*
        Create an empty data packet to force the headers out
     */
    httpPutPacketToNext(q->pair, httpCreateDataPacket(0));
    stream->tx->responded = 0;
    return 0;
}


static void manageWebSocket(HttpWebSocket *ws, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(ws->currentFrame);
        mprMark(ws->currentMessage);
        mprMark(ws->tailMessage);
        mprMark(ws->pingEvent);
        mprMark(ws->subProtocol);
        mprMark(ws->errorMsg);
        mprMark(ws->closeReason);
        mprMark(ws->data);
    }
}


static void closeWebSock(HttpQueue *q)
{
    HttpWebSocket   *ws;

    if (q->stream && q->stream->rx) {
        ws = q->stream->rx->webSocket;
        assert(ws);
        if (ws) {
            ws->state = WS_STATE_CLOSED;
           if (ws->pingEvent) {
                mprRemoveEvent(ws->pingEvent);
                ws->pingEvent = 0;
           }
        }
    }
}


static void readyWebSock(HttpQueue *q)
{
    if (httpServerStream(q->stream)) {
        HTTP_NOTIFY(q->stream, HTTP_EVENT_APP_OPEN, 0);
    }
}


static void incomingWebSockData(HttpQueue *q, HttpPacket *packet)
{
    HttpStream      *stream;
    HttpWebSocket   *ws;
    HttpPacket      *tail;
    HttpLimits      *limits;
    MprBuf          *content;
    char            *fp, *cp;
    ssize           len, currentFrameLen, offset, frameLen;
    int             i, error, mask, lenBytes, opcode;

    assert(packet);
    stream = q->stream;
    assert(stream->rx);
    ws = stream->rx->webSocket;
    assert(ws);
    limits = stream->limits;

    if (packet->flags & HTTP_PACKET_DATA) {
        /*
            The service queue is used to hold data that is yet to be analyzed.
            The ws->currentFrame holds the current frame that is being read from the service queue.
         */
        httpJoinPacketForService(q, packet, 0);
    }
    httpLog(stream->trace, "request.websockets.data", "packet", "state:%d, frame:%d, length:%zu",
        ws->state, ws->frameState, httpGetPacketLength(packet));

    if (packet->flags & HTTP_PACKET_END) {
        /*
            EOF packet means the socket has been abortively closed
         */
        if (ws->state != WS_STATE_CLOSED) {
            ws->closing = 1;
            ws->frameState = WS_CLOSED;
            ws->state = WS_STATE_CLOSED;
            ws->closeStatus = WS_STATUS_COMMS_ERROR;
            HTTP_NOTIFY(stream, HTTP_EVENT_APP_CLOSE, ws->closeStatus);
            httpError(stream, HTTP_ABORT | HTTP_CODE_COMMS_ERROR, "Connection lost");
        }
    }
    while ((packet = httpGetPacket(q)) != 0) {
        content = packet->content;
        error = 0;
        switch (ws->frameState) {
        case WS_CLOSED:
            if (httpGetPacketLength(packet) > 0) {
                traceError(stream, "Closed, ignore incoming packet");
            }
            httpFinalize(stream);
            httpSetState(stream, HTTP_STATE_FINALIZED);
            break;

        case WS_BEGIN:
            if (httpGetPacketLength(packet) < 2) {
                /* Need more data */
                httpPutBackPacket(q, packet);
                return;
            }
            fp = content->start;
            if (GET_RSV(*fp) != 0) {
                error = WS_STATUS_PROTOCOL_ERROR;
                traceError(stream, "Protocol error, bad reserved field");
                break;
            }
            packet->fin = GET_FIN(*fp);
            opcode = GET_CODE(*fp);
            if (opcode == WS_MSG_CONT) {
                if (!ws->currentMessageType) {
                    traceError(stream, "Protocol error, continuation frame but not prior message");
                    error = WS_STATUS_PROTOCOL_ERROR;
                    break;
                }
            } else if (opcode < WS_MSG_CONTROL && ws->currentMessageType) {
                traceError(stream, "Protocol error, data frame received but expected a continuation frame");
                error = WS_STATUS_PROTOCOL_ERROR;
                break;
            }
            if (opcode > WS_MSG_PONG) {
                traceError(stream, "Protocol error, bad frame opcode");
                error = WS_STATUS_PROTOCOL_ERROR;
                break;
            }
            packet->type = opcode;
            if (opcode >= WS_MSG_CONTROL && !packet->fin) {
                /* Control frame, must not be fragmented */
                traceError(stream, "Protocol error, fragmented control frame");
                error = WS_STATUS_PROTOCOL_ERROR;
                break;
            }
            fp++;
            len = GET_LEN(*fp);
            mask = GET_MASK(*fp);
            lenBytes = 1;
            if (len == 126) {
                lenBytes += 2;
                len = 0;
            } else if (len == 127) {
                lenBytes += 8;
                len = 0;
            }
            if (httpGetPacketLength(packet) < (lenBytes + 1 + (mask * 4))) {
                /* Return if we don't have the required packet control fields */
                httpPutBackPacket(q, packet);
                return;
            }
            fp++;
            while (--lenBytes > 0) {
                len <<= 8;
                len += (uchar) *fp++;
            }
            if (packet->type >= WS_MSG_CONTROL && len > WS_MAX_CONTROL) {
                /* Too big */
                traceError(stream, "Protocol error, control frame too big");
                error = WS_STATUS_PROTOCOL_ERROR;
                break;
            }
            ws->frameLength = len;
            ws->frameState = WS_MSG;
            ws->maskOffset = mask ? 0 : -1;
            if (mask) {
                for (i = 0; i < 4; i++) {
                    ws->dataMask[i] = *fp++;
                }
            }
            assert(content);
            assert(fp >= content->start);
            mprAdjustBufStart(content, fp - content->start);
            assert(q->count >= 0);
            /*
                Put packet onto the service queue
             */
            httpPutBackPacket(q, packet);
            ws->frameState = WS_MSG;
            break;

        case WS_MSG:
            currentFrameLen = httpGetPacketLength(ws->currentFrame);
            len = httpGetPacketLength(packet);
            if ((currentFrameLen + len) > ws->frameLength) {
                /*
                    Split packet if it contains data for the next frame. Do this even if this frame has no data.
                 */
                offset = ws->frameLength - currentFrameLen;
                if ((tail = httpSplitPacket(packet, offset)) != 0) {
                    content = packet->content;
                    httpPutBackPacket(q, tail);
                    len = httpGetPacketLength(packet);
                }
            }
            if ((currentFrameLen + len) > stream->limits->webSocketsMessageSize) {
                if (httpServerStream(stream)) {
                    httpMonitorEvent(stream, HTTP_COUNTER_LIMIT_ERRORS, 1);
                }
                traceError(stream, "Incoming message is too large, length %zd, max %zd", len, limits->webSocketsMessageSize);
                error = WS_STATUS_MESSAGE_TOO_LARGE;
                break;
            }
            if (ws->maskOffset >= 0) {
                for (cp = content->start; cp < content->end; cp++) {
                    *cp = *cp ^ ws->dataMask[ws->maskOffset++ & 0x3];
                }
            }
            if (packet->type == WS_MSG_CONT && ws->currentFrame) {
                httpJoinPacket(ws->currentFrame, packet);
                packet = ws->currentFrame;
                content = packet->content;
            }
#if FUTURE
            if (packet->type == WS_MSG_TEXT) {
                /*
                    Validate the frame for fast-fail, provided the last frame does not have a partial codepoint.
                 */
                if (!ws->partialUTF) {
                    if (!validateText(stream, packet)) {
                        error = WS_STATUS_INVALID_UTF8;
                        break;
                    }
                    ws->partialUTF = 0;
                }
            }
#endif
            frameLen = httpGetPacketLength(packet);
            assert(frameLen <= ws->frameLength);
            if (frameLen == ws->frameLength) {
                if ((error = processWebSocketFrame(q, packet)) != 0) {
                    break;
                }
                if (ws->state == WS_STATE_CLOSED) {
                    HTTP_NOTIFY(stream, HTTP_EVENT_APP_CLOSE, ws->closeStatus);
                    httpFinalize(stream);
                    ws->frameState = WS_CLOSED;
                    httpSetState(stream, HTTP_STATE_FINALIZED);
                    break;
                }
                ws->currentFrame = 0;
                ws->frameState = WS_BEGIN;
            } else {
                ws->currentFrame = packet;
            }
            break;

        default:
            traceError(stream, "Protocol error, unknown frame state");
            error = WS_STATUS_PROTOCOL_ERROR;
            break;
        }
        if (error) {
            /*
                Notify of the error and send a close to the peer. The peer may or may not be still there.
             */
            HTTP_NOTIFY(stream, HTTP_EVENT_ERROR, error);
            httpSendClose(stream, error, NULL);
            ws->frameState = WS_CLOSED;
            ws->state = WS_STATE_CLOSED;
            httpFinalize(stream);
            if (!stream->rx->eof) {
                httpSetEof(stream);
            }
            httpSetState(stream, HTTP_STATE_FINALIZED);
            return;
        }
    }
}


static int processWebSocketFrame(HttpQueue *q, HttpPacket *packet)
{
    HttpStream      *stream;
    HttpRx          *rx;
    HttpWebSocket   *ws;
    HttpLimits      *limits;
    MprBuf          *content;
    ssize           len;
    char            *cp;
    int             validated;

    stream = q->stream;
    limits = stream->limits;
    ws = stream->rx->webSocket;
    assert(ws);
    rx = stream->rx;
    assert(packet);
    content = packet->content;
    validated = 0;
    assert(content);

    mprAddNullToBuf(content);
    httpLog(stream->trace, "rx.websockets", "packet",
        "wsSeq:%d, wsTypeName:%s, wsType:%d, wsLast:%d, wsLength:%zu",
         ws->rxSeq++, codetxt[packet->type], packet->type, packet->fin, mprGetBufLength(content));

    switch (packet->type) {
    case WS_MSG_TEXT:
        /* Fall through */

    case WS_MSG_BINARY:
        ws->messageLength = 0;
        ws->currentMessageType = packet->type;
        /* Fall through */

    case WS_MSG_CONT:
        if (ws->closing) {
            break;
        }
        if (packet->type == WS_MSG_CONT) {
            if (!ws->currentMessageType) {
                traceError(stream, "Bad continuation packet");
                return WS_STATUS_PROTOCOL_ERROR;
            }
            packet->type = ws->currentMessageType;
        }
        /*
            Validate this frame if we don't have a partial codepoint from a prior frame.
         */
        if (packet->type == WS_MSG_TEXT && !ws->partialUTF) {
            if (!validateText(stream, packet)) {
                return WS_STATUS_INVALID_UTF8;
            }
            validated++;
        }
        if (ws->currentMessage && !ws->preserveFrames) {
            httpJoinPacket(ws->currentMessage, packet);
            ws->currentMessage->fin = packet->fin;
            packet = ws->currentMessage;
            content = packet->content;
            if (packet->type == WS_MSG_TEXT && !validated) {
                if (!validateText(stream, packet)) {
                    return WS_STATUS_INVALID_UTF8;
                }
            }
        }
        /*
            Send what we have if preserving frames or the current messages is over the packet limit size.
            Otherwise, keep buffering.
         */
        for (ws->tailMessage = 0; packet; packet = ws->tailMessage, ws->tailMessage = 0) {
            if (!ws->preserveFrames && (httpGetPacketLength(packet) > limits->webSocketsPacketSize)) {
                ws->tailMessage = httpSplitPacket(packet, limits->webSocketsPacketSize);
                content = packet->content;
                packet->fin = 0;
            }
            if (packet->fin || ws->tailMessage || ws->preserveFrames) {
                packet->flags |= HTTP_PACKET_SOLO;
                ws->messageLength += httpGetPacketLength(packet);
                if (packet->type == WS_MSG_TEXT) {
                    mprAddNullToBuf(packet->content);
                }
                httpPutPacketToNext(q, packet);
                ws->currentMessage = 0;
            } else {
                ws->currentMessage = packet;
                break;
            }
            if (packet->fin) {
                ws->currentMessageType = 0;
            }
        }
        if (stream->readq->first) {
            //  Notify handler
            HTTP_NOTIFY(stream, HTTP_EVENT_READABLE, 0);
        }
        break;

    case WS_MSG_CLOSE:
        cp = content->start;
        if (httpGetPacketLength(packet) == 0) {
            ws->closeStatus = WS_STATUS_OK;
        } else if (httpGetPacketLength(packet) < 2) {
            traceError(stream, "Missing close status");
            return WS_STATUS_PROTOCOL_ERROR;
        } else {
            ws->closeStatus = ((uchar) cp[0]) << 8 | (uchar) cp[1];

            /*
                WebSockets is a hideous spec, as if UTF validation wasn't bad enough, we must invalidate these codes:
                    1004, 1005, 1006, 1012-1016, 2000-2999
             */
            if (ws->closeStatus < 1000 || ws->closeStatus >= 5000 ||
                (1004 <= ws->closeStatus && ws->closeStatus <= 1006) ||
                (1012 <= ws->closeStatus && ws->closeStatus <= 1016) ||
                (1100 <= ws->closeStatus && ws->closeStatus <= 2999)) {
                traceError(stream, "Bad close status %d", ws->closeStatus);
                return WS_STATUS_PROTOCOL_ERROR;
            }
            mprAdjustBufStart(content, 2);
            if (httpGetPacketLength(packet) > 0) {
                ws->closeReason = mprCloneBufMem(content);
                if (!rx->route || !rx->route->ignoreEncodingErrors) {
                    if (validUTF8(stream, ws->closeReason, slen(ws->closeReason)) != UTF8_ACCEPT) {
                        traceError(stream, "Text packet has invalid UTF8");
                        return WS_STATUS_INVALID_UTF8;
                    }
                }
            }
        }
        httpLog(stream->trace, "rx.websockets.close", "packet",
            "wsCloseStatus:%d, wsCloseReason:'%s', wsClosing:%d", ws->closeStatus, ws->closeReason, ws->closing);
        if (ws->closing) {
            httpDisconnectStream(stream);
        } else {
            /* Acknowledge the close. Echo the received status */
            httpSendClose(stream, WS_STATUS_OK, "OK");
            if (!stream->rx->eof) {
                httpSetEof(stream);
            }
            rx->remainingContent = 0;
            stream->keepAliveCount = 0;
        }
        ws->state = WS_STATE_CLOSED;
        break;

    case WS_MSG_PING:
        /* Respond with the same content as specified in the ping message */
        len = mprGetBufLength(content);
        len = min(len, WS_MAX_CONTROL);
        httpSendBlock(stream, WS_MSG_PONG, mprGetBufStart(content), mprGetBufLength(content), HTTP_BUFFER);
        break;

    case WS_MSG_PONG:
        /* Do nothing */
        break;

    default:
        traceError(stream, "Bad message type %d", packet->type);
        ws->state = WS_STATE_CLOSED;
        return WS_STATUS_PROTOCOL_ERROR;
    }
    return 0;
}


/*
    Send a text message. Caller must submit valid UTF8.
    Returns the number of data message bytes written. Should equal the length.
 */
PUBLIC ssize httpSend(HttpStream *stream, cchar *fmt, ...)
{
    va_list     args;
    char        *buf;

    va_start(args, fmt);
    buf = sfmtv(fmt, args);
    va_end(args);
    return httpSendBlock(stream, WS_MSG_TEXT, buf, slen(buf), HTTP_BUFFER);
}


/*
    Send a block of data with the specified message type. Set flags to HTTP_MORE to indicate there is more data
    for this message.
 */
PUBLIC ssize httpSendBlock(HttpStream *stream, int type, cchar *buf, ssize len, int flags)
{
    HttpWebSocket   *ws;
    HttpPacket      *packet;
    HttpQueue       *q;
    ssize           room, thisWrite, totalWritten;

    assert(stream);

    ws = stream->rx->webSocket;
    stream->tx->responded = 1;

    /*
        Note: we can come here before the handshake is complete. The data is queued and if the connection handshake
        succeeds, then the data is sent.
     */
    if (!(HTTP_STATE_CONNECTED <= stream->state && stream->state < HTTP_STATE_FINALIZED) || !stream->upgraded || stream->error) {
        return MPR_ERR_BAD_STATE;
    }
    if (type != WS_MSG_CONT && type != WS_MSG_TEXT && type != WS_MSG_BINARY && type != WS_MSG_CLOSE &&
            type != WS_MSG_PING && type != WS_MSG_PONG) {
        traceError(stream, "Bad message type %d", type);
        return MPR_ERR_BAD_ARGS;
    }
    q = stream->writeq;
    if (flags == 0) {
        flags = HTTP_BUFFER;
    }
    if (len < 0) {
        len = slen(buf);
    }
    if (len > stream->limits->webSocketsMessageSize) {
        if (httpServerStream(stream)) {
            httpMonitorEvent(stream, HTTP_COUNTER_LIMIT_ERRORS, 1);
        }
        traceError(stream, "Outgoing message is too large, length %zd max %zd", len, stream->limits->webSocketsMessageSize);
        return MPR_ERR_WONT_FIT;
    }
    totalWritten = 0;
    do {
        if ((room = q->max - q->count) == 0) {
            if (flags & HTTP_NON_BLOCK) {
                break;
            }
        }
        /*
            Break into frames if the user is not preserving frames and has not explicitly specified "more".
            The outgoingWebSockService will encode each packet as a frame.
         */
        if (ws->preserveFrames || (flags & HTTP_MORE)) {
            thisWrite = len;
        } else {
            thisWrite = min(len, stream->limits->webSocketsFrameSize);
        }
        thisWrite = min(thisWrite, q->packetSize);
        if (flags & (HTTP_BLOCK | HTTP_NON_BLOCK)) {
            thisWrite = min(thisWrite, room);
        }
        /*
            Must still send empty packets of zero length
         */
        if ((packet = httpCreateDataPacket(thisWrite)) == 0) {
            return MPR_ERR_MEMORY;
        }
        /*
            Spec requires type to be set only on the first frame
         */
        if (ws->more) {
            type = 0;
        }
        packet->type = type;
        type = 0;
        if (ws->preserveFrames || (flags & HTTP_MORE)) {
            packet->flags |= HTTP_PACKET_SOLO;
        }
        if (thisWrite > 0) {
            if (mprPutBlockToBuf(packet->content, buf, thisWrite) != thisWrite) {
                return MPR_ERR_MEMORY;
            }
        }
        len -= thisWrite;
        buf += thisWrite;
        totalWritten += thisWrite;
        packet->fin = (len > 0) ? 0 : !(flags & HTTP_MORE);
        ws->more = !packet->fin;
        httpPutPacket(q, packet);

        if (!flushWsPipe(q, flags) && flags & HTTP_NON_BLOCK) {
            break;
        }
        if (httpRequestExpired(stream, 0)) {
            return MPR_ERR_TIMEOUT;
        }
    } while (len > 0);

    httpFlushQueue(q, flags);
    if (httpClientStream(stream)) {
        httpEnableNetEvents(stream->net);
    }
    return totalWritten;
}


static bool flushWsPipe(HttpQueue *q, int flags)
{
    if (!q->service) {
        q = httpFindNextQueue(q);
    }
    if (q->count >= q->max || q->flags & HTTP_QUEUE_SUSPENDED) {
        //  WARNING: may yield if flags are HTTP_BLOCK
        httpFlushQueue(q, flags);
        if (q->count >= q->max) {
            return 0;
        }
    }
    return 1;
}


/*
    The reason string is optional
 */
PUBLIC ssize httpSendClose(HttpStream *stream, int status, cchar *reason)
{
    HttpWebSocket   *ws;
    char            msg[128];
    ssize           len;

    assert(0 <= status && status <= WS_STATUS_MAX);
    ws = stream->rx->webSocket;
    assert(ws);
    if (ws->closing) {
        return 0;
    }
    ws->closing = 1;
    ws->state = WS_STATE_CLOSING;

    if (!(HTTP_STATE_CONNECTED <= stream->state && stream->state < HTTP_STATE_FINALIZED) || !stream->upgraded) {
        /* Ignore closes when already finalized or not yet connected */
        return 0;
    }
    len = 2;
    if (reason) {
        if (slen(reason) >= 124) {
            reason = "WebSockets close message was too big";
            traceError(stream, reason);
        }
        len += slen(reason) + 1;
    }
    msg[0] = (status >> 8) & 0xff;
    msg[1] = status & 0xff;
    if (reason) {
        scopy(&msg[2], len - 2, reason);
    }
    httpLog(stream->trace, "tx.websockets.close", "packet", "wsCloseStatus:%d, wsCloseReason:%s", status, reason);
    return httpSendBlock(stream, WS_MSG_CLOSE, msg, len, HTTP_BUFFER);
}


/*
    This is the outgoing filter routine. It services packets on the outgoing queue and transforms them into
    WebSockets frames.
 */
static void outgoingWebSockService(HttpQueue *q)
{
    HttpStream      *stream;
    HttpPacket      *packet, *tail;
    HttpWebSocket   *ws;
    char            *ep, *fp, *prefix, dataMask[4];
    ssize           len;
    int             i, mask;

    stream = q->stream;
    ws = stream->rx->webSocket;
    for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
        if (!(packet->flags & (HTTP_PACKET_END | HTTP_PACKET_HEADER))) {
            if (!(packet->flags & HTTP_PACKET_SOLO)) {
                if (packet->esize > stream->limits->packetSize) {
                    if ((tail = httpResizePacket(q, packet, stream->limits->packetSize)) != 0) {
                        assert(tail->fin == packet->fin);
                        packet->fin = 0;
                    }
                }
                if (!httpWillNextQueueAcceptPacket(q, packet)) {
                    httpPutBackPacket(q, packet);
                    return;
                }
            }
            if (packet->type < 0 || packet->type > WS_MSG_MAX) {
                httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Bad WebSocket packet type %d", packet->type);
                break;
            }
            len = httpGetPacketLength(packet);
            packet->prefix = mprCreateBuf(16, 16);
            prefix = packet->prefix->start;
            /*
                Server-side does not mask outgoing data
             */
            mask = httpServerStream(stream) ? 0 : 1;
            *prefix++ = SET_FIN(packet->fin) | SET_CODE(packet->type);
            if (len <= WS_MAX_CONTROL) {
                *prefix++ = SET_MASK(mask) | SET_LEN(len, 0);
            } else if (len <= 65535) {
                *prefix++ = SET_MASK(mask) | 126;
                *prefix++ = SET_LEN(len, 1);
                *prefix++ = SET_LEN(len, 0);
            } else {
                *prefix++ = SET_MASK(mask) | 127;
                for (i = 7; i >= 0; i--) {
                    *prefix++ = SET_LEN(len, i);
                }
            }
            if (httpClientStream(stream)) {
                mprGetRandomBytes(dataMask, sizeof(dataMask), 0);
                for (i = 0; i < 4; i++) {
                    *prefix++ = dataMask[i];
                }
                fp = packet->content->start;
                ep = packet->content->end;
                for (i = 0; fp < ep; fp++) {
                    *fp = *fp ^ dataMask[i++ & 0x3];
                }
            }
            *prefix = '\0';
            mprAdjustBufEnd(packet->prefix, prefix - packet->prefix->start);
            httpLog(stream->trace, "websockets.tx.packet", "packet",
                "wsSeqno:%d, wsTypeName:\"%s\", wsType:%d, wsLast:%d, wsLength:%zd",
                ws->txSeq++, codetxt[packet->type], packet->type, packet->fin, httpGetPacketLength(packet));
        }
        httpPutPacketToNext(q, packet);
    }
}


PUBLIC cchar *httpGetWebSocketCloseReason(HttpStream *stream)
{
    HttpWebSocket   *ws;

    if (!stream || !stream->rx) {
        return 0;
    }
    if ((ws = stream->rx->webSocket) == 0) {
        return 0;
    }
    assert(ws);
    return ws->closeReason;
}


PUBLIC void *httpGetWebSocketData(HttpStream *stream)
{
    return (stream->rx && stream->rx->webSocket) ? stream->rx->webSocket->data : NULL;
}


PUBLIC ssize httpGetWebSocketMessageLength(HttpStream *stream)
{
    HttpWebSocket   *ws;

    if (!stream || !stream->rx) {
        return 0;
    }
    if ((ws = stream->rx->webSocket) == 0) {
        return 0;
    }
    assert(ws);
    return ws->messageLength;
}


PUBLIC char *httpGetWebSocketProtocol(HttpStream *stream)
{
    HttpWebSocket   *ws;

    if (!stream || !stream->rx) {
        return 0;
    }
    if ((ws = stream->rx->webSocket) == 0) {
        return 0;
    }
    assert(ws);
    return ws->subProtocol;
}


PUBLIC ssize httpGetWebSocketState(HttpStream *stream)
{
    HttpWebSocket   *ws;

    if (!stream || !stream->rx) {
        return 0;
    }
    if ((ws = stream->rx->webSocket) == 0) {
        return 0;
    }
    assert(ws);
    return ws->state;
}


PUBLIC bool httpWebSocketOrderlyClosed(HttpStream *stream)
{
    HttpWebSocket   *ws;

    if (!stream || !stream->rx) {
        return 0;
    }
    if ((ws = stream->rx->webSocket) == 0) {
        return 0;
    }
    assert(ws);
    return ws->closeStatus != WS_STATUS_COMMS_ERROR;
}


PUBLIC void httpSetWebSocketData(HttpStream *stream, void *data)
{
    if (stream->rx && stream->rx->webSocket) {
        stream->rx->webSocket->data = data;
    }
}


PUBLIC void httpSetWebSocketProtocols(HttpStream *stream, cchar *protocols)
{
    assert(stream);
    assert(protocols && *protocols);
    stream->protocols = sclone(protocols);
}


PUBLIC void httpSetWebSocketPreserveFrames(HttpStream *stream, bool on)
{
    HttpWebSocket   *ws;

    if ((ws = stream->rx->webSocket) != 0) {
        ws->preserveFrames = on;
    }
}


/*
    Test if a string is a valid unicode string.
    The return state may be UTF8_ACCEPT if all codepoints validate and are complete.
    Return UTF8_REJECT if an invalid codepoint was found.
    Otherwise, return the state for a partial codepoint.
 */
static int validUTF8(HttpStream *stream, cchar *str, ssize len)
{
    uchar   *cp, c;
    uint    state, type;

    state = UTF8_ACCEPT;
    for (cp = (uchar*) str; cp < (uchar*) &str[len]; cp++) {
        c = *cp;
        type = utfTable[c];
        /*
            codepoint = (*state != UTF8_ACCEPT) ? (byte & 0x3fu) | (*codep << 6) : (0xff >> type) & (byte);
         */
        state = utfTable[256 + (state * 16) + type];
        if (state == UTF8_REJECT) {
            traceError(stream, "Invalid UTF8 at offset %d", cp - (uchar*) str);
            break;
        }
    }
    return state;
}


/*
    Validate the UTF8 in a packet. Return false if an invalid codepoint is found.
    If the packet is not the last packet, we alloc incomplete codepoints.
    Set ws->partialUTF if the last codepoint was incomplete.
 */
static bool validateText(HttpStream *stream, HttpPacket *packet)
{
    HttpWebSocket   *ws;
    HttpRx          *rx;
    MprBuf          *content;
    int             state;
    bool            valid;

    rx = stream->rx;
    ws = rx->webSocket;

    /*
        Skip validation if ignoring errors or some frames have already been sent to the callback
     */
    if ((rx->route && rx->route->ignoreEncodingErrors) || ws->messageLength > 0) {
        return 1;
    }
    content = packet->content;
    state = validUTF8(stream, content->start, mprGetBufLength(content));
    ws->partialUTF = state != UTF8_ACCEPT;

    if (packet->fin) {
        valid =  state == UTF8_ACCEPT;
    } else {
        valid = state != UTF8_REJECT;
    }
    if (!valid) {
        traceError(stream, "Text packet has invalid UTF8");
    }
    return valid;
}


static void webSockPing(HttpStream *stream)
{
    assert(stream);
    assert(stream->rx);
    /*
        Send a ping. Optimze by sending no data message with it.
     */
    httpSendBlock(stream, WS_MSG_PING, NULL, 0, HTTP_BUFFER);
}


static void webSockTimeout(HttpStream *stream)
{
    assert(stream);
    httpSendClose(stream, WS_STATUS_POLICY_VIOLATION, "Request timeout");
}


/*
    Upgrade a client socket to use Web Sockets. This is called by the client to request a web sockets upgrade.
 */
PUBLIC int httpUpgradeWebSocket(HttpStream *stream)
{
    HttpTx  *tx;
    char    num[16];

    tx = stream->tx;
    assert(httpClientStream(stream));

    httpSetStatus(stream, HTTP_CODE_SWITCHING);
    httpSetHeader(stream, "Upgrade", "websocket");
    httpSetHeader(stream, "Connection", "Upgrade");
    mprGetRandomBytes(num, sizeof(num), 0);
    tx->webSockKey = mprEncode64Block(num, sizeof(num));
    httpSetHeaderString(stream, "Sec-WebSocket-Key", tx->webSockKey);
    httpSetHeaderString(stream, "Sec-WebSocket-Protocol", stream->protocols ? stream->protocols : "chat");
    httpSetHeaderString(stream, "Sec-WebSocket-Version", "13");
    httpSetHeader(stream, "X-Request-Timeout", "%lld", stream->limits->requestTimeout / TPS);
    httpSetHeader(stream, "X-Inactivity-Timeout", "%lld", stream->limits->inactivityTimeout / TPS);

    stream->upgraded = 1;
    stream->keepAliveCount = 0;
    stream->rx->remainingContent = HTTP_UNLIMITED;
    return 0;
}


/*
    Client verification of the server WebSockets handshake response
 */
PUBLIC bool httpVerifyWebSocketsHandshake(HttpStream *stream)
{
    HttpRx          *rx;
    HttpTx          *tx;
    cchar           *key, *expected;

    rx = stream->rx;
    tx = stream->tx;
    assert(rx);
    assert(rx->webSocket);
    assert(stream->upgraded);
    assert(httpClientStream(stream));

    rx->webSocket->state = WS_STATE_CLOSED;

    if (rx->status != HTTP_CODE_SWITCHING) {
        httpError(stream, HTTP_CODE_BAD_HANDSHAKE, "Bad WebSocket handshake status %d", rx->status);
        return 0;
    }
    if (!smatch(httpGetHeader(stream, "connection"), "Upgrade")) {
        httpError(stream, HTTP_CODE_BAD_HANDSHAKE, "Bad WebSocket Connection header");
        return 0;
    }
    if (!smatch(httpGetHeader(stream, "upgrade"), "WebSocket")) {
        httpError(stream, HTTP_CODE_BAD_HANDSHAKE, "Bad WebSocket Upgrade header");
        return 0;
    }
    expected = mprGetSHABase64(sjoin(tx->webSockKey, WS_MAGIC, NULL));
    key = httpGetHeader(stream, "sec-websocket-accept");
    if (!smatch(key, expected)) {
        httpError(stream, HTTP_CODE_BAD_HANDSHAKE, "Bad WebSocket handshake key\n%s\n%s", key, expected);
        return 0;
    }
    rx->webSocket->state = WS_STATE_OPEN;
    return 1;
}


static void traceErrorProc(HttpStream *stream, cchar *fmt, ...)
{
    HttpWebSocket   *ws;
    va_list         args;

    ws = stream->rx->webSocket;
    va_start(args, fmt);
    ws->errorMsg = sfmtv(fmt, args);
    va_end(args);

    httpLog(stream->trace, "tx.websockets.error", "error", "msg:%s", ws->errorMsg);
}


/*
    Test if the packet is the last packet in a logical message
 */
bool httpIsLastPacket(HttpPacket *packet)
{
    return packet->fin;
}


#endif /* ME_HTTP_WEB_SOCKETS */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */

