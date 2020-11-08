/*
    espFramework.c -- ESP Web Framework API

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "esp.h"

/************************************* Locals *********************************/

#define ITERATE_CONFIG(route, obj, child, index) \
    index = 0, child = obj ? obj->children: 0; obj && index < obj->length && !route->error; child = child->next, index++

/*********************************** Fowards **********************************/

static EspAction *createAction(cchar *target, cchar *abilities, void *callback);

/************************************* Code ***********************************/

#if DEPRECATED
PUBLIC void espAddPak(HttpRoute *route, cchar *name, cchar *version)
{
    if (!version || !*version || smatch(version, "0.0.0")) {
        version = "*";
    }
    mprSetJson(route->config, sfmt("dependencies.%s", name), version, MPR_JSON_STRING);
}
#endif


/*
    Add a http header if not already defined
 */
PUBLIC void espAddHeader(HttpStream *stream, cchar *key, cchar *fmt, ...)
{
    va_list     vargs;

    assert(key && *key);
    assert(fmt && *fmt);

    va_start(vargs, fmt);
    httpAddHeaderString(stream, key, sfmt(fmt, vargs));
    va_end(vargs);
}


/*
    Add a header string if not already defined
 */
PUBLIC void espAddHeaderString(HttpStream *stream, cchar *key, cchar *value)
{
    httpAddHeaderString(stream, key, value);
}


PUBLIC void espAddParam(HttpStream *stream, cchar *var, cchar *value)
{
    if (!httpGetParam(stream, var, 0)) {
        httpSetParam(stream, var, value);
    }
}


/*
   Append a header. If already defined, the value is catenated to the pre-existing value after a ", " separator.
   As per the HTTP/1.1 spec.
 */
PUBLIC void espAppendHeader(HttpStream *stream, cchar *key, cchar *fmt, ...)
{
    va_list     vargs;

    assert(key && *key);
    assert(fmt && *fmt);

    va_start(vargs, fmt);
    httpAppendHeaderString(stream, key, sfmt(fmt, vargs));
    va_end(vargs);
}


/*
   Append a header string. If already defined, the value is catenated to the pre-existing value after a ", " separator.
   As per the HTTP/1.1 spec.
 */
PUBLIC void espAppendHeaderString(HttpStream *stream, cchar *key, cchar *value)
{
    httpAppendHeaderString(stream, key, value);
}


PUBLIC void espAutoFinalize(HttpStream *stream)
{
    EspReq  *req;

    req = stream->reqData;
    if (req->autoFinalize) {
        httpFinalize(stream);
    }
}


PUBLIC int espCache(HttpRoute *route, cchar *uri, int lifesecs, int flags)
{
    httpAddCache(route, NULL, uri, NULL, NULL, 0, lifesecs * TPS, flags);
    return 0;
}


PUBLIC cchar *espCreateSession(HttpStream *stream)
{
    HttpSession *session;

    if ((session = httpCreateSession(getStream())) != 0) {
        return session->id;
    }
    return 0;
}


#if DEPRECATED || 1
PUBLIC void espDefineAction(HttpRoute *route, cchar *target, EspProc callback)
{
    espAction(route, target, NULL, callback);
}
#endif


PUBLIC void espAction(HttpRoute *route, cchar *target, cchar *abilities, EspProc callback)
{
    EspRoute    *eroute;
    EspAction   *action;

    assert(route);
    assert(target && *target);
    assert(callback);

    eroute = ((EspRoute*) route->eroute)->top;
    if (target) {
#if DEPRECATED
        /* Keep till version 6 */
        if (scontains(target, "-cmd-")) {
            target = sreplace(target, "-cmd-", "/");
        } else if (schr(target, '-')) {
            controller = ssplit(sclone(target), "-", (char**) &action);
            target = sjoin(controller, "/", action, NULL);
        }
#endif
        if (!eroute->actions) {
            eroute->actions = mprCreateHash(-1, 0);
        }
        if ((action = createAction(target, abilities, callback)) == 0) {
            /* Memory errors centrally reported */
            return;
        }
        mprAddKey(eroute->actions, target, action);
    }
}


static void manageAction(EspAction *action, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(action->target);
        mprMark(action->abilities);
    }
}


static EspAction *createAction(cchar *target, cchar *abilities, void *callback)
{
    EspAction   *action;

    if ((action = mprAllocObj(EspAction, manageAction)) == 0) {
        return NULL;
    }
    action->target = sclone(target);
    action->abilities = abilities ? sclone(abilities) : NULL;
    action->callback = callback;
    return action;
}


#if DEPRECATED || 1
/*
    The base procedure is invoked prior to calling any and all actions on this route
 */
PUBLIC void espDefineBase(HttpRoute *route, EspLegacyProc baseProc)
{
    HttpRoute   *rp;
    EspRoute    *eroute;
    int         next;

    for (ITERATE_ITEMS(route->host->routes, rp, next)) {
        if ((eroute = rp->eroute) != 0) {
            if (smatch(httpGetDir(rp, "CONTROLLERS"), httpGetDir(route, "CONTROLLERS"))) {
                eroute->commonController = (EspProc) baseProc;
            }
        }
    }
}
#endif


/*
    Define a common base controller to invoke prior to calling any and all actions on this route
 */
PUBLIC void espController(HttpRoute *route, EspProc baseProc)
{
    HttpRoute   *rp;
    EspRoute    *eroute;
    int         next;

    for (ITERATE_ITEMS(route->host->routes, rp, next)) {
        if ((eroute = rp->eroute) != 0) {
            if (smatch(httpGetDir(rp, "CONTROLLERS"), httpGetDir(route, "CONTROLLERS"))) {
                eroute->commonController = baseProc;
            }
        }
    }
}


/*
    Path should be a relative path from route->documents to the view file (relative-path.esp)
 */
PUBLIC void espDefineView(HttpRoute *route, cchar *path, void *view)
{
    EspRoute    *eroute;

    assert(path && *path);
    assert(view);

    if (route->eroute) {
        eroute = ((EspRoute*) route->eroute)->top;
    } else {
        if ((eroute = espRoute(route, 1)) == 0) {
            /* Should never happen */
            return;
        }
    }
    eroute = eroute->top;
    if (route) {
        path = mprGetPortablePath(path);
    }
    if (!eroute->views) {
        eroute->views = mprCreateHash(-1, MPR_HASH_STATIC_VALUES);
    }
    mprAddKey(eroute->views, path, view);
}


PUBLIC void espDestroySession(HttpStream *stream)
{
    httpDestroySession(stream);
}


PUBLIC void espFinalize(HttpStream *stream)
{
    httpFinalize(stream);
}


PUBLIC void espFlush(HttpStream *stream)
{
    httpFlush(stream);
}


PUBLIC HttpAuth *espGetAuth(HttpStream *stream)
{
    return stream->rx->route->auth;
}


PUBLIC cchar *espGetConfig(HttpRoute *route, cchar *key, cchar *defaultValue)
{
    cchar       *value;

    if (sstarts(key, "app.")) {
        mprLog("warn esp", 0, "Using legacy \"app\" configuration property");
    }
    if ((value = mprGetJson(route->config, key)) != 0) {
        return value;
    }
    return defaultValue;
}


PUBLIC MprOff espGetContentLength(HttpStream *stream)
{
    return httpGetContentLength(stream);
}


PUBLIC cchar *espGetContentType(HttpStream *stream)
{
    return stream->rx->mimeType;
}


PUBLIC cchar *espGetCookie(HttpStream *stream, cchar *name)
{
    return httpGetCookie(stream, name);
}


PUBLIC cchar *espGetCookies(HttpStream *stream)
{
    return httpGetCookies(stream);
}


PUBLIC void *espGetData(HttpStream *stream)
{
    EspReq  *req;

    req = stream->reqData;
    return req->data;
}


PUBLIC Edi *espGetDatabase(HttpStream *stream)
{
    HttpRx      *rx;
    EspReq      *req;
    EspRoute    *eroute;
    Edi         *edi;

    rx = stream->rx;
    req = stream->reqData;
    edi = req ? req->edi : 0;
    if (edi == 0 && rx && rx->route) {
        if ((eroute = rx->route->eroute) != 0) {
            edi = eroute->edi;
        }
    }
    if (edi == 0) {
        httpError(stream, 0, "Cannot get database instance");
        return 0;
    }
    return edi;
}


PUBLIC cchar *espGetDocuments(HttpStream *stream)
{
    return stream->rx->route->documents;
}


PUBLIC EspRoute *espGetEspRoute(HttpStream *stream)
{
    return stream->rx->route->eroute;
}


PUBLIC cchar *espGetFeedback(HttpStream *stream, cchar *kind)
{
    EspReq      *req;
    MprKey      *kp;
    cchar       *msg;

    req = stream->reqData;
    if (kind == 0 || req == 0 || req->feedback == 0 || mprGetHashLength(req->feedback) == 0) {
        return 0;
    }
    for (kp = 0; (kp = mprGetNextKey(req->feedback, kp)) != 0; ) {
        msg = kp->data;
        //  DEPRECATE "all"
        if (smatch(kind, kp->key) || smatch(kind, "all") || smatch(kind, "*")) {
            return msg;
        }
    }
    return 0;
}


PUBLIC EdiGrid *espGetGrid(HttpStream *stream)
{
    return stream->grid;
}


PUBLIC cchar *espGetHeader(HttpStream *stream, cchar *key)
{
    return httpGetHeader(stream, key);
}


PUBLIC MprHash *espGetHeaderHash(HttpStream *stream)
{
    return httpGetHeaderHash(stream);
}


PUBLIC char *espGetHeaders(HttpStream *stream)
{
    return httpGetHeaders(stream);
}


PUBLIC int espGetIntParam(HttpStream *stream, cchar *var, int defaultValue)
{
    return httpGetIntParam(stream, var, defaultValue);
}


PUBLIC cchar *espGetMethod(HttpStream *stream)
{
    return stream->rx->method;
}


PUBLIC cchar *espGetParam(HttpStream *stream, cchar *var, cchar *defaultValue)
{
    return httpGetParam(stream, var, defaultValue);
}


PUBLIC MprJson *espGetParams(HttpStream *stream)
{
    return httpGetParams(stream);
}


PUBLIC cchar *espGetPath(HttpStream *stream)
{
    return stream->rx->pathInfo;
}


PUBLIC cchar *espGetQueryString(HttpStream *stream)
{
    return httpGetQueryString(stream);
}


PUBLIC cchar *espGetReferrer(HttpStream *stream)
{
    if (stream->rx->referrer) {
        return stream->rx->referrer;
    }
    return httpLink(stream, "~");
}


PUBLIC HttpRoute *espGetRoute(HttpStream *stream)
{
    return stream->rx->route;
}


PUBLIC Edi *espGetRouteDatabase(HttpRoute *route)
{
    EspRoute    *eroute;

    eroute = route->eroute;
    if (eroute == 0 || eroute->edi == 0) {
        return 0;
    }
    return eroute->edi;
}


PUBLIC cchar *espGetRouteVar(HttpStream *stream, cchar *var)
{
    return httpGetRouteVar(stream->rx->route, var);
}


PUBLIC cchar *espGetSessionID(HttpStream *stream, int create)
{
    HttpSession *session;

    if ((session = httpGetSession(getStream(), create)) != 0) {
        return session->id;
    }
    return 0;
}


PUBLIC int espGetStatus(HttpStream *stream)
{
    return httpGetStatus(stream);
}


PUBLIC cchar *espGetStatusMessage(HttpStream *stream)
{
    return httpGetStatusMessage(stream);
}


PUBLIC MprList *espGetUploads(HttpStream *stream)
{
    return stream->rx->files;
}


PUBLIC cchar *espGetUri(HttpStream *stream)
{
    return stream->rx->uri;
}


#if DEPRECATED

PUBLIC bool espHasPak(HttpRoute *route, cchar *name)
{
    return mprGetJsonObj(route->config, sfmt("dependencies.%s", name)) != 0;
}
#endif


PUBLIC bool espHasGrid(HttpStream *stream)
{
    return stream->grid != 0;
}


PUBLIC bool espHasRec(HttpStream *stream)
{
    EdiRec  *rec;

    rec = stream->record;
    return (rec && rec->id) ? 1 : 0;
}


PUBLIC bool espIsAuthenticated(HttpStream *stream)
{
    return httpIsAuthenticated(stream);
}


PUBLIC bool espIsEof(HttpStream *stream)
{
    return httpIsEof(stream);
}


PUBLIC bool espIsFinalized(HttpStream *stream)
{
    return httpIsFinalized(stream);
}


PUBLIC bool espIsSecure(HttpStream *stream)
{
    return stream->secure;
}


PUBLIC bool espMatchParam(HttpStream *stream, cchar *var, cchar *value)
{
    return httpMatchParam(stream, var, value);
}


/*
    Read rx data in non-blocking mode. Use standard connection timeouts.
 */
PUBLIC ssize espReceive(HttpStream *stream, char *buf, ssize len)
{
    return httpRead(stream, buf, len);
}


PUBLIC void espRedirect(HttpStream *stream, int status, cchar *target)
{
    httpRedirect(stream, status, target);
}


PUBLIC void espRedirectBack(HttpStream *stream)
{
    if (stream->rx->referrer) {
        espRedirect(stream, HTTP_CODE_MOVED_TEMPORARILY, stream->rx->referrer);
    }
}


PUBLIC ssize espRender(HttpStream *stream, cchar *fmt, ...)
{
    va_list     vargs;
    char        *buf;

    va_start(vargs, fmt);
    buf = sfmtv(fmt, vargs);
    va_end(vargs);
    return espRenderString(stream, buf);
}


PUBLIC ssize espRenderBlock(HttpStream *stream, cchar *buf, ssize size)
{
    /*
        Must not yield as render() has dynamic allocations.
        If callers is generating a lot of data, they must call mprYield themselves or monitor the stream->writeq->count.
     */
    return httpWriteBlock(stream->writeq, buf, size, HTTP_BUFFER);
}


PUBLIC ssize espRenderCached(HttpStream *stream)
{
    return httpWriteCached(stream);
}


static void copyMappings(HttpRoute *route, MprJson *dest, MprJson *obj)
{
    MprJson     *child, *job, *jvalue;
    cchar       *key, *value;
    int         ji;

    for (ITERATE_CONFIG(route, obj, child, ji)) {
        if (child->type & MPR_JSON_OBJ) {
            job = mprCreateJson(MPR_JSON_OBJ);
            copyMappings(route, job, child);
            mprSetJsonObj(dest, child->name, job);
        } else {
            key = child->value;
            if (sends(key, "|time")) {
                key = ssplit(sclone(key), " \t|", NULL);
                if ((value = mprGetJson(route->config, key)) != 0) {
                    mprSetJson(dest, child->name, itos(httpGetTicks(value)), MPR_JSON_NUMBER);
                }
            } else {
                if ((jvalue = mprGetJsonObj(route->config, key)) != 0) {
                    mprSetJsonObj(dest, child->name, mprCloneJson(jvalue));
                }
            }
        }
    }
}


static cchar *getClientConfig(HttpStream *stream)
{
    HttpRoute   *route;
    MprJson     *mappings, *obj;

    stream = getStream();
    for (route = stream->rx->route; route; route = route->parent) {
        if (route->clientConfig) {
            return route->clientConfig;
        }
    }
    route = stream->rx->route;
    if ((obj = mprGetJsonObj(route->config, "esp.mappings")) != 0) {
        mappings = mprCreateJson(MPR_JSON_OBJ);
        copyMappings(route, mappings, obj);
        mprWriteJson(mappings, "prefix", route->prefix, 0);
        route->clientConfig = mprJsonToString(mappings, MPR_JSON_QUOTES);
    }
    return route->clientConfig;
}


PUBLIC ssize espRenderConfig(HttpStream *stream)
{
    cchar       *config;

    if ((config = getClientConfig(stream)) != 0) {
        return renderString(config);
    }
    return 0;
}


PUBLIC ssize espRenderError(HttpStream *stream, int status, cchar *fmt, ...)
{
    va_list     args;
    HttpRx      *rx;
    ssize       written;
    cchar       *msg, *title, *text;

    va_start(args, fmt);

    rx = stream->rx;
    if (rx->route->json) {
        mprLog("warn esp", 0, "Calling espRenderFeedback in JSON app");
        return 0 ;
    }
    written = 0;

    if (!httpIsFinalized(stream)) {
        if (status == 0) {
            status = HTTP_CODE_INTERNAL_SERVER_ERROR;
        }
        title = sfmt("Request Error for \"%s\"", rx->pathInfo);
        msg = mprEscapeHtml(sfmtv(fmt, args));
        if (rx->route->flags & HTTP_ROUTE_SHOW_ERRORS) {
            text = sfmt(\
                "<!DOCTYPE html>\r\n<html>\r\n<head><title>%s</title></head>\r\n" \
                "<body>\r\n<h1>%s</h1>\r\n" \
                "    <pre>%s</pre>\r\n" \
                "    <p>To prevent errors being displayed in the browser, " \
                "       set <b>http.showErrors off</b> in the JSON configuration file.</p>\r\n" \
                "</body>\r\n</html>\r\n", title, title, msg);
            httpSetContentType(stream, "text/html");
            written += espRenderString(stream, text);
            espFinalize(stream);
            httpLog(stream->trace, "esp.error", "error", "msg=%s, status=%d, uri=%s", msg, status, rx->pathInfo);
        }
    }
    va_end(args);
    return written;
}


PUBLIC ssize espRenderFile(HttpStream *stream, cchar *path)
{
    MprFile     *from;
    ssize       count, written, nbytes;
    char        buf[ME_BUFSIZE];

    if ((from = mprOpenFile(path, O_RDONLY | O_BINARY, 0)) == 0) {
        return MPR_ERR_CANT_OPEN;
    }
    written = 0;
    while ((count = mprReadFile(from, buf, sizeof(buf))) > 0) {
        if ((nbytes = espRenderBlock(stream, buf, count)) < 0) {
            return nbytes;
        }
        written += nbytes;
    }
    mprCloseFile(from);
    return written;
}


PUBLIC ssize espRenderFeedback(HttpStream *stream, cchar *kinds)
{
    EspReq      *req;
    MprKey      *kp;
    cchar       *msg;
    ssize       written;

    req = stream->reqData;
    if (req->route->json) {
        mprLog("warn esp", 0, "Calling espRenderFeedback in JSON app");
        return 0;
    }
    if (kinds == 0 || req->feedback == 0 || mprGetHashLength(req->feedback) == 0) {
        return 0;
    }
    written = 0;
    for (kp = 0; (kp = mprGetNextKey(req->feedback, kp)) != 0; ) {
        msg = kp->data;
        //  DEPRECATE "all"
        if (strstr(kinds, kp->key) || strstr(kinds, "all") || strstr(kinds, "*")) {
            written += espRender(stream, "<span class='feedback-%s animate'>%s</span>", kp->key, msg);
        }
    }
    return written;
}


PUBLIC ssize espRenderSafe(HttpStream *stream, cchar *fmt, ...)
{
    va_list     args;
    cchar       *s;

    va_start(args, fmt);
    s = mprEscapeHtml(sfmtv(fmt, args));
    va_end(args);
    return espRenderBlock(stream, s, slen(s));
}


PUBLIC ssize espRenderSafeString(HttpStream *stream, cchar *s)
{
    s = mprEscapeHtml(s);
    return espRenderBlock(stream, s, slen(s));
}


PUBLIC ssize espRenderString(HttpStream *stream, cchar *s)
{
    return espRenderBlock(stream, s, slen(s));
}


/*
    Render a request variable. If a param by the given name is not found, consult the session.
 */
PUBLIC ssize espRenderVar(HttpStream *stream, cchar *name)
{
    cchar   *value;

    if ((value = espGetParam(stream, name, 0)) == 0) {
        value = httpGetSessionVar(stream, name, "");
    }
    return espRenderSafeString(stream, value);
}


PUBLIC int espRemoveHeader(HttpStream *stream, cchar *key)
{
    assert(key && *key);
    if (stream->tx == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    return mprRemoveKey(stream->tx->headers, key);
}


PUBLIC void espRemoveSessionVar(HttpStream *stream, cchar *var)
{
    httpRemoveSessionVar(stream, var);
}


PUBLIC void espRemoveCookie(HttpStream *stream, cchar *name)
{
    HttpRoute   *route;
    cchar       *url;

    route = stream->rx->route;
    url = (route->prefix && *route->prefix) ? route->prefix : "/";
    httpSetCookie(stream, name, "", url, NULL, 0, 0);
}


PUBLIC void espSetStream(HttpStream *stream)
{
    mprSetThreadData(((Esp*) MPR->espService)->local, stream);
}


static void espNotifier(HttpStream *stream, int event, int arg)
{
    EspReq      *req;

    if ((req = stream->reqData) != 0) {
        espSetStream(stream);
        (req->notifier)(stream, event, arg);
    }
}


PUBLIC void espSetNotifier(HttpStream *stream, HttpNotifier notifier)
{
    EspReq      *req;

    if ((req = stream->reqData) != 0) {
        req->notifier = notifier;
        httpSetStreamNotifier(stream, espNotifier);
    }
}


#if DEPRECATED
PUBLIC int espSaveConfig(HttpRoute *route)
{
    cchar       *path;

    path = mprJoinPath(route->home, "esp.json");
#if KEEP
    mprBackupLog(path, 3);
#endif
    return mprSaveJson(route->config, path, MPR_JSON_PRETTY | MPR_JSON_QUOTES);
}
#endif


/*
    Send a grid with schema
 */
PUBLIC ssize espSendGrid(HttpStream *stream, EdiGrid *grid, int flags)
{
    HttpRoute   *route;
    EspRoute    *eroute;

    route = stream->rx->route;

    if (route->json) {
        httpSetContentType(stream, "application/json");
        if (grid) {
            eroute = route->eroute;
            flags = flags | (eroute->encodeTypes ? MPR_JSON_ENCODE_TYPES : 0);
            return espRender(stream, "{\n  \"data\": %s, \"count\": %d, \"schema\": %s}\n",
                ediGridAsJson(grid, flags), grid->count, ediGetGridSchemaAsJson(grid));
        }
        return espRender(stream, "{data:[]}");
    }
    return 0;
}


PUBLIC ssize espSendRec(HttpStream *stream, EdiRec *rec, int flags)
{
    HttpRoute   *route;
    EspRoute    *eroute;

    route = stream->rx->route;
    if (route->json) {
        httpSetContentType(stream, "application/json");
        if (rec) {
            eroute = route->eroute;
            flags = flags | (eroute->encodeTypes ? MPR_JSON_ENCODE_TYPES : 0);
            return espRender(stream, "{\n  \"data\": %s, \"schema\": %s}\n", ediRecAsJson(rec, flags), ediGetRecSchemaAsJson(rec));
        }
        return espRender(stream, "{}");
    }
    return 0;
}


PUBLIC ssize espSendResult(HttpStream *stream, bool success)
{
    EspReq      *req;
    EdiRec      *rec;
    ssize       written;

    req = stream->reqData;
    written = 0;
    if (req->route->json) {
        rec = getRec();
        if (rec && rec->errors) {
            written = espRender(stream, "{\"error\": %d, \"feedback\": %s, \"fieldErrors\": %s}", !success,
                req->feedback ? mprSerialize(req->feedback, MPR_JSON_QUOTES) : "{}",
                mprSerialize(rec->errors, MPR_JSON_QUOTES));
        } else {
            written = espRender(stream, "{\"error\": %d, \"feedback\": %s}", !success,
                req->feedback ? mprSerialize(req->feedback, MPR_JSON_QUOTES) : "{}");
        }
        espFinalize(stream);
    } else {
        /* Noop */
    }
    return written;
}


PUBLIC bool espSetAutoFinalizing(HttpStream *stream, bool on)
{
    EspReq  *req;
    bool    old;

    req = stream->reqData;
    old = req->autoFinalize;
    req->autoFinalize = on;
    return old;
}


PUBLIC int espSetConfig(HttpRoute *route, cchar *key, cchar *value)
{
    return mprSetJson(route->config, key, value, 0);
}


PUBLIC void espSetContentLength(HttpStream *stream, MprOff length)
{
    httpSetContentLength(stream, length);
}


PUBLIC void espSetCookie(HttpStream *stream, cchar *name, cchar *value, cchar *path, cchar *cookieDomain, MprTicks lifespan,
        bool isSecure)
{
    httpSetCookie(stream, name, value, path, cookieDomain, lifespan, isSecure);
}


PUBLIC void espSetContentType(HttpStream *stream, cchar *mimeType)
{
    httpSetContentType(stream, mimeType);
}


PUBLIC void espSetData(HttpStream *stream, void *data)
{
    EspReq  *req;

    req = stream->reqData;
    req->data = data;
}


PUBLIC void espSetFeedback(HttpStream *stream, cchar *kind, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    espSetFeedbackv(stream, kind, fmt, args);
    va_end(args);
}


PUBLIC void espSetFeedbackv(HttpStream *stream, cchar *kind, cchar *fmt, va_list args)
{
    EspReq      *req;
    cchar       *msg;

    if ((req = stream->reqData) == 0) {
        return;
    }
    if (!req->route->json) {
        /*
            Create a session as early as possible so a Set-Cookie header can be omitted.
         */
        httpGetSession(stream, 1);
    }
    if (req->feedback == 0) {
        req->feedback = mprCreateHash(0, MPR_HASH_STABLE);
    }
    msg = sfmtv(fmt, args);

#if KEEP
    MprKey      *current, *last;
    if ((current = mprLookupKeyEntry(req->feedback, kind)) != 0) {
        if ((last = mprLookupKey(req->lastFeedback, current->key)) != 0 && current->data == last->data) {
            /* Overwrite prior feedback messages */
            mprAddKey(req->feedback, kind, msg);
        } else {
            /* Append to existing feedback messages */
            mprAddKey(req->feedback, kind, sjoin(current->data, ", ", msg, NULL));
        }
    } else
#endif
    mprAddKey(req->feedback, kind, msg);
}


#if DEPRECATED
PUBLIC void espSetFlash(HttpStream *stream, cchar *kind, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    espSetFeedbackv(stream, kind, fmt, args);
    va_end(args);
}
#endif


PUBLIC EdiGrid *espSetGrid(HttpStream *stream, EdiGrid *grid)
{
    return stream->grid = grid;
}


/*
    Set a http header. Overwrite if present.
 */
PUBLIC void espSetHeader(HttpStream *stream, cchar *key, cchar *fmt, ...)
{
    va_list     vargs;

    assert(key && *key);
    assert(fmt && *fmt);

    va_start(vargs, fmt);
    httpSetHeaderString(stream, key, sfmtv(fmt, vargs));
    va_end(vargs);
}


PUBLIC void espSetHeaderString(HttpStream *stream, cchar *key, cchar *value)
{
    httpSetHeaderString(stream, key, value);
}


PUBLIC void espSetIntParam(HttpStream *stream, cchar *var, int value)
{
    httpSetIntParam(stream, var, value);
}


PUBLIC void espSetParam(HttpStream *stream, cchar *var, cchar *value)
{
    httpSetParam(stream, var, value);
}


PUBLIC EdiRec *espSetRec(HttpStream *stream, EdiRec *rec)
{
    return stream->record = rec;
}


PUBLIC int espSetSessionVar(HttpStream *stream, cchar *var, cchar *value)
{
    return httpSetSessionVar(stream, var, value);
}


PUBLIC void espSetStatus(HttpStream *stream, int status)
{
    httpSetStatus(stream, status);
}


PUBLIC void espShowRequest(HttpStream *stream)
{
    MprHash     *env;
    MprJson     *params, *param;
    MprKey      *kp;
    MprJson     *jkey;
    HttpRx      *rx;
    int         i;

    rx = stream->rx;
    httpAddHeaderString(stream, "Cache-Control", "no-cache");
    httpCreateCGIParams(stream);
    espRender(stream, "\r\n");

    /*
        Query
     */
    for (ITERATE_JSON(rx->params, jkey, i)) {
        espRender(stream, "PARAMS %s=%s\r\n", jkey->name, jkey->value ? jkey->value : "null");
    }
    espRender(stream, "\r\n");

    /*
        Http Headers
     */
    env = espGetHeaderHash(stream);
    for (ITERATE_KEYS(env, kp)) {
        espRender(stream, "HEADER %s=%s\r\n", kp->key, kp->data ? kp->data: "null");
    }
    espRender(stream, "\r\n");

    /*
        Server vars
     */
    for (ITERATE_KEYS(stream->rx->svars, kp)) {
        espRender(stream, "SERVER %s=%s\r\n", kp->key, kp->data ? kp->data: "null");
    }
    espRender(stream, "\r\n");

    /*
        Form vars
     */
    if ((params = espGetParams(stream)) != 0) {
        for (ITERATE_JSON(params, param, i)) {
            espRender(stream, "FORM %s=%s\r\n", param->name, param->value);
        }
        espRender(stream, "\r\n");
    }

#if KEEP
    /*
        Body
     */
    q = stream->readq;
    if (q->first && rx->bytesRead > 0 && scmp(rx->mimeType, "application/x-www-form-urlencoded") == 0) {
        buf = q->first->content;
        mprAddNullToBuf(buf);
        if ((numKeys = getParams(&keys, mprGetBufStart(buf), (int) mprGetBufLength(buf))) > 0) {
            for (i = 0; i < (numKeys * 2); i += 2) {
                value = keys[i+1];
                espRender(stream, "BODY %s=%s\r\n", keys[i], value ? value: "null");
            }
        }
        espRender(stream, "\r\n");
    }
#endif
}


PUBLIC bool espTestConfig(HttpRoute *route, cchar *key, cchar *desired)
{
    cchar       *value;

    if ((value = mprGetJson(route->config, key)) != 0) {
        return smatch(value, desired);
    }
    return 0;
}


PUBLIC void espUpdateCache(HttpStream *stream, cchar *uri, cchar *data, int lifesecs)
{
    httpUpdateCache(stream, uri, data, lifesecs * TPS);
}


PUBLIC cchar *espUri(HttpStream *stream, cchar *target)
{
    return httpLink(stream, target);
}


PUBLIC int espEmail(HttpStream *stream, cchar *to, cchar *from, cchar *subject, MprTime date, cchar *mime,
    cchar *message, MprList *files)
{
    MprList         *lines;
    MprCmd          *cmd;
    cchar           *body, *boundary, *contents, *encoded, *file;
    char            *out, *err;
    ssize           length;
    int             i, next, status;

    if (!from || !*from) {
        from = "anonymous";
    }
    if (!subject || !*subject) {
        subject = "Mail message";
    }
    if (!mime || !*mime) {
        mime = "text/plain";
    }
    if (!date) {
        date = mprGetTime();
    }
    boundary = sjoin("esp.mail=", mprGetMD5("BOUNDARY"), NULL);
    lines = mprCreateList(0, 0);

    mprAddItem(lines, sfmt("To: %s", to));
    mprAddItem(lines, sfmt("From: %s", from));
    mprAddItem(lines, sfmt("Date: %s", mprFormatLocalTime(0, date)));
    mprAddItem(lines, sfmt("Subject: %s", subject));
    mprAddItem(lines, "MIME-Version: 1.0");
    mprAddItem(lines, sfmt("Content-Type: multipart/mixed; boundary=%s", boundary));
    mprAddItem(lines, "");

    boundary = sjoin("--", boundary, NULL);

    mprAddItem(lines, boundary);
    mprAddItem(lines, sfmt("Content-Type: %s", mime));
    mprAddItem(lines, "");
    mprAddItem(lines, "");
    mprAddItem(lines, message);

    for (ITERATE_ITEMS(files, file, next)) {
        mprAddItem(lines, boundary);
        if ((mime = mprLookupMime(NULL, file)) == 0) {
            mime = "application/octet-stream";
        }
        mprAddItem(lines, "Content-Transfer-Encoding: base64");
        mprAddItem(lines, sfmt("Content-Disposition: inline; filename=\"%s\"", mprGetPathBase(file)));
        mprAddItem(lines, sfmt("Content-Type: %s; name=\"%s\"", mime, mprGetPathBase(file)));
        mprAddItem(lines, "");
        contents = mprReadPathContents(file, &length);
        encoded = mprEncode64Block(contents, length);
        for (i = 0; i < length; i += 76) {
            mprAddItem(lines, snclone(&encoded[i], i + 76));
        }
    }
    mprAddItem(lines, sfmt("%s--", boundary));

    body = mprListToString(lines, "\n");
    httpLog(stream->trace, "esp.email", "context", "%s", body);

    cmd = mprCreateCmd(stream->dispatcher);
    if (mprRunCmd(cmd, "sendmail -t", NULL, body, &out, &err, -1, 0) < 0) {
        mprDestroyCmd(cmd);
        return MPR_ERR_CANT_OPEN;
    }
    if (mprWaitForCmd(cmd, ME_ESP_EMAIL_TIMEOUT) < 0) {
        httpLog(stream->trace, "esp.email.error", "error",
            "msg=\"Timeout waiting for command to complete\", timeout=%d, command=\"%s\"",
            ME_ESP_EMAIL_TIMEOUT, cmd->argv[0]);
        mprDestroyCmd(cmd);
        return MPR_ERR_CANT_COMPLETE;
    }
    if ((status = mprGetCmdExitStatus(cmd)) != 0) {
        httpLog(stream->trace, "esp.email.error", "error", "msg=\"Sendmail failed\", status=%d, error=\"%s\"", status, err);
        mprDestroyCmd(cmd);
        return MPR_ERR_CANT_WRITE;
    }
    mprDestroyCmd(cmd);
    return 0;
}


PUBLIC void espClearCurrentSession(HttpStream *stream)
{
    EspRoute    *eroute;

    eroute = stream->rx->route->eroute;
    if (eroute->currentSession) {
        httpLog(stream->trace, "esp.singular.clear", "context", "session=%s", eroute->currentSession);
    }
    eroute->currentSession = 0;
}


/*
    Remember this connections session as the current session. Use for single login tracking.
 */
PUBLIC void espSetCurrentSession(HttpStream *stream)
{
    EspRoute    *eroute;

    eroute = stream->rx->route->eroute;
    eroute->currentSession = httpGetSessionID(stream);
    httpLog(stream->trace, "esp.singular.set", "context", "msg=\"Set singluar user\", session=%s", eroute->currentSession);
}


/*
    Test if this connection is the current session. Use for single login tracking.
 */
PUBLIC bool espIsCurrentSession(HttpStream *stream)
{
    EspRoute    *eroute;

    eroute = stream->rx->route->eroute;
    if (eroute->currentSession) {
        if (smatch(httpGetSessionID(stream), eroute->currentSession)) {
            return 1;
        }
        if (httpLookupSessionID(eroute->currentSession)) {
            /* Session is still current */
            return 0;
        }
        /* Session has expired */
        eroute->currentSession = 0;
    }
    return 1;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
