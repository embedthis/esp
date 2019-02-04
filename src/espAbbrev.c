/*
    espAbbrev.c -- ESP Abbreviated API

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "esp.h"

/*************************************** Code *********************************/

PUBLIC void addHeader(cchar *key, cchar *fmt, ...)
{
    va_list     args;
    cchar       *value;

    va_start(args, fmt);
    value = sfmtv(fmt, args);
    espAddHeaderString(getStream(), key, value);
    va_end(args);
}


PUBLIC void addParam(cchar *key, cchar *value)
{
    if (!param(key)) {
        setParam(key, value);
    }
}


PUBLIC bool canUser(cchar *abilities, bool warn)
{
    HttpStream    *stream;

    stream = getStream();
    if (httpCanUser(stream, abilities)) {
        return 1;
    }
    if (warn) {
        setStatus(HTTP_CODE_UNAUTHORIZED);
        sendResult(feedback("error", "Access Denied. Insufficient Privilege."));
    }
    return 0;
}


PUBLIC EdiRec *createRec(cchar *tableName, MprJson *params)
{
    return setRec(ediSetFields(ediCreateRec(getDatabase(), tableName), params));
}


PUBLIC bool createRecFromParams(cchar *table)
{
    return updateRec(createRec(table, params()));
}


/*
    Return the session ID
 */
PUBLIC cchar *createSession()
{
    return espCreateSession(getStream());
}


/*
    Destroy a session and erase the session state data.
    This emits an expired Set-Cookie header to the browser to force it to erase the cookie.
 */
PUBLIC void destroySession()
{
    httpDestroySession(getStream());
}


PUBLIC void dontAutoFinalize()
{
    espSetAutoFinalizing(getStream(), 0);
}


PUBLIC bool feedback(cchar *kind, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    espSetFeedbackv(getStream(), kind, fmt, args);
    va_end(args);

    /*
        Return true if there is not an error feedback message
     */
    return getFeedback("error") == 0;
}


PUBLIC void finalize()
{
    espFinalize(getStream());
}


#if DEPRECATED || 1
PUBLIC void flash(cchar *kind, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    espSetFeedbackv(getStream(), kind, fmt, args);
    va_end(args);
}
#endif


PUBLIC void flush()
{
    espFlush(getStream());
}


PUBLIC HttpAuth *getAuth()
{
    return espGetAuth(getStream());
}


PUBLIC MprList *getColumns(EdiRec *rec)
{
    if (rec == 0) {
        if ((rec = getRec()) == 0) {
            return 0;
        }
    }
    return ediGetColumns(getDatabase(), rec->tableName);
}


PUBLIC cchar *getConfig(cchar *field)
{
    HttpRoute   *route;
    cchar       *value;

    route = getStream()->rx->route;
    if ((value = mprGetJson(route->config, field)) == 0) {
        return "";
    }
    return value;
}


PUBLIC HttpStream *getStream()
{
    HttpStream    *stream;

    stream = mprGetThreadData(((Esp*) MPR->espService)->local);
    if (stream == 0) {
        mprLog("error esp", 0, "Stream is not defined in thread local storage.\n"
        "If using a callback, make sure you invoke espSetStream with the connection before using the ESP abbreviated API");
    }
    return stream;
}


PUBLIC cchar *getCookies()
{
    return espGetCookies(getStream());
}


PUBLIC MprOff getContentLength()
{
    return espGetContentLength(getStream());
}


PUBLIC cchar *getContentType()
{
    return getStream()->rx->mimeType;
}


PUBLIC void *getData()
{
    return espGetData(getStream());
}


PUBLIC Edi *getDatabase()
{
    return espGetDatabase(getStream());
}


PUBLIC MprDispatcher *getDispatcher()
{
    HttpStream    *stream;

    if ((stream = getStream()) == 0) {
        return 0;
    }
    return stream->dispatcher;
}


PUBLIC cchar *getDocuments()
{
    return getStream()->rx->route->documents;
}


PUBLIC EspRoute *getEspRoute()
{
    return espGetEspRoute(getStream());
}


PUBLIC cchar *getFeedback(cchar *kind)
{
    return espGetFeedback(getStream(), kind);
}


PUBLIC cchar *getField(EdiRec *rec, cchar *field)
{
    return ediGetFieldValue(rec, field);
}


PUBLIC cchar *getFieldError(cchar *field)
{
    return mprLookupKey(getRec()->errors, field);
}


PUBLIC EdiGrid *getGrid()
{
    return getStream()->grid;
}


PUBLIC cchar *getHeader(cchar *key)
{
    return espGetHeader(getStream(), key);
}


PUBLIC cchar *getMethod()
{
    return espGetMethod(getStream());
}


PUBLIC cchar *getQuery()
{
    return getStream()->rx->parsedUri->query;
}


PUBLIC EdiRec *getRec()
{
    return getStream()->record;
}


PUBLIC cchar *getReferrer()
{
    return espGetReferrer(getStream());
}


PUBLIC EspReq *getReq()
{
    return getStream()->data;
}


PUBLIC HttpRoute *getRoute()
{
    return espGetRoute(getStream());
}


PUBLIC cchar *getSecurityToken()
{
    return httpGetSecurityToken(getStream(), 0);
}


/*
    Get a session and return the session ID. Creates a session if one does not already exist.
 */
PUBLIC cchar *getSessionID()
{
    return espGetSessionID(getStream(), 1);
}


PUBLIC cchar *getSessionVar(cchar *key)
{
    return httpGetSessionVar(getStream(), key, 0);
}


PUBLIC cchar *getPath()
{
    return espGetPath(getStream());
}


PUBLIC MprList *getUploads()
{
    return espGetUploads(getStream());
}


PUBLIC cchar *getUri()
{
    return espGetUri(getStream());
}


PUBLIC bool hasGrid()
{
    return espHasGrid(getStream());
}


PUBLIC bool hasRec()
{
    return espHasRec(getStream());
}


PUBLIC bool isEof()
{
    return httpIsEof(getStream());
}


PUBLIC bool isFinalized()
{
    return espIsFinalized(getStream());
}


PUBLIC bool isSecure()
{
    return espIsSecure(getStream());
}


PUBLIC EdiGrid *makeGrid(cchar *contents)
{
    return ediMakeGrid(contents);
}


PUBLIC MprHash *makeHash(cchar *fmt, ...)
{
    va_list     args;
    cchar       *str;

    va_start(args, fmt);
    str = sfmtv(fmt, args);
    va_end(args);
    return mprDeserialize(str);
}


PUBLIC MprJson *makeJson(cchar *fmt, ...)
{
    va_list     args;
    cchar       *str;

    va_start(args, fmt);
    str = sfmtv(fmt, args);
    va_end(args);
    return mprParseJson(str);
}


PUBLIC EdiRec *makeRec(cchar *contents)
{
    return ediMakeRec(contents);
}


PUBLIC cchar *makeUri(cchar *target)
{
    return espUri(getStream(), target);
}


PUBLIC cchar *md5(cchar *str)
{
    return mprGetMD5(str);
}


PUBLIC bool modeIs(cchar *kind)
{
    HttpRoute   *route;

    route = getStream()->rx->route;
    return smatch(route->mode, kind);
}


PUBLIC cchar *nonce()
{
    return mprGetMD5(itos(mprRandom()));
}


PUBLIC cchar *param(cchar *key)
{
    return espGetParam(getStream(), key, 0);
}


PUBLIC MprJson *params()
{
    return espGetParams(getStream());
}


PUBLIC ssize receive(char *buf, ssize len)
{
    return httpRead(getStream(), buf, len);
}


PUBLIC EdiRec *readRecWhere(cchar *tableName, cchar *fieldName, cchar *operation, cchar *value)
{
    return setRec(ediReadRecWhere(getDatabase(), tableName, fieldName, operation, value));
}


PUBLIC EdiRec *readRec(cchar *tableName, cchar *key)
{
    if (key == 0 || *key == 0) {
        key = "1";
    }
    return setRec(ediReadRec(getDatabase(), tableName, key));
}


PUBLIC EdiRec *readRecByKey(cchar *tableName, cchar *key)
{
    return setRec(ediReadRec(getDatabase(), tableName, key));
}


PUBLIC EdiGrid *readWhere(cchar *tableName, cchar *fieldName, cchar *operation, cchar *value)
{
    return setGrid(ediReadWhere(getDatabase(), tableName, fieldName, operation, value));
}


PUBLIC EdiGrid *readTable(cchar *tableName)
{
    return setGrid(ediReadWhere(getDatabase(), tableName, 0, 0, 0));
}


PUBLIC void redirect(cchar *target)
{
    espRedirect(getStream(), 302, target);
}


PUBLIC void redirectBack()
{
    espRedirectBack(getStream());
}


PUBLIC void removeCookie(cchar *name)
{
    espRemoveCookie(getStream(), name);
}


PUBLIC bool removeRec(cchar *tableName, cchar *key)
{
    if (ediRemoveRec(getDatabase(), tableName, key) < 0) {
        feedback("error", "Cannot delete %s", stitle(tableName));
        return 0;
    }
    feedback("info", "Deleted %s", stitle(tableName));
    return 1;
}


PUBLIC void removeSessionVar(cchar *key)
{
    httpRemoveSessionVar(getStream(), key);
}


PUBLIC ssize render(cchar *fmt, ...)
{
    va_list     args;
    ssize       count;
    cchar       *msg;

    va_start(args, fmt);
    msg = sfmtv(fmt, args);
    count = espRenderString(getStream(), msg);
    va_end(args);
    return count;
}


PUBLIC ssize renderCached()
{
    return espRenderCached(getStream());;
}


PUBLIC ssize renderConfig()
{
    return espRenderConfig(getStream());;
}


PUBLIC void renderError(int status, cchar *fmt, ...)
{
    va_list     args;
    cchar       *msg;

    va_start(args, fmt);
    msg = sfmt(fmt, args);
    espRenderError(getStream(), status, "%s", msg);
    va_end(args);
}


PUBLIC ssize renderFile(cchar *path)
{
    return espRenderFile(getStream(), path);
}


PUBLIC void renderFeedback(cchar *kind)
{
    espRenderFeedback(getStream(), kind);
}


PUBLIC ssize renderSafe(cchar *fmt, ...)
{
    va_list     args;
    ssize       count;
    cchar       *msg;

    va_start(args, fmt);
    msg = sfmtv(fmt, args);
    count = espRenderSafeString(getStream(), msg);
    va_end(args);
    return count;
}


PUBLIC ssize renderString(cchar *s)
{
    return espRenderString(getStream(), s);
}


PUBLIC void renderView(cchar *view)
{
    espRenderDocument(getStream(), view);
}


#if KEEP
PUBLIC int runCmd(cchar *command, char *input, char **output, char **error, MprTime timeout, int flags)
{
    return mprRun(getDispatcher(), command, input, output, error, timeout, MPR_CMD_IN  | MPR_CMD_OUT | MPR_CMD_ERR | flags);
}
#endif


PUBLIC int runCmd(cchar *command, char *input, char **output, char **error, MprTime timeout, int flags)
{
    MprCmd  *cmd;

    cmd = mprCreateCmd(getDispatcher());
    return mprRunCmd(cmd, command, NULL, input, output, error, timeout, MPR_CMD_IN  | MPR_CMD_OUT | MPR_CMD_ERR | flags);
}


/*
    Add a security token to the response. The token is generated as a HTTP header and session cookie.
 */
PUBLIC void securityToken()
{
    httpAddSecurityToken(getStream(), 0);
}


PUBLIC ssize sendGrid(EdiGrid *grid)
{
    return espSendGrid(getStream(), grid, 0);
}


PUBLIC ssize sendRec(EdiRec *rec)
{
    return espSendRec(getStream(), rec, 0);
}


PUBLIC void sendResult(bool status)
{
    espSendResult(getStream(), status);
}


PUBLIC void setStream(HttpStream *stream)
{
    espSetStream(stream);
}


PUBLIC void setContentType(cchar *mimeType)
{
    espSetContentType(getStream(), mimeType);
}


PUBLIC void setCookie(cchar *name, cchar *value, cchar *path, cchar *cookieDomain, MprTicks lifespan, bool isSecure)
{
    espSetCookie(getStream(), name, value, path, cookieDomain, lifespan, isSecure);
}


PUBLIC void setData(void *data)
{
    espSetData(getStream(), data);
}


PUBLIC EdiRec *setField(EdiRec *rec, cchar *fieldName, cchar *value)
{
    return ediSetField(rec, fieldName, value);
}


PUBLIC EdiRec *setFields(EdiRec *rec, MprJson *params)
{
    return ediSetFields(rec, params);
}


PUBLIC EdiGrid *setGrid(EdiGrid *grid)
{
    getStream()->grid = grid;
    return grid;
}


PUBLIC void setHeader(cchar *key, cchar *fmt, ...)
{
    va_list     args;
    cchar       *value;

    va_start(args, fmt);
    value = sfmtv(fmt, args);
    espSetHeaderString(getStream(), key, value);
    va_end(args);
}


PUBLIC void setIntParam(cchar *key, int value)
{
    espSetIntParam(getStream(), key, value);
}


PUBLIC void setNotifier(HttpNotifier notifier)
{
    espSetNotifier(getStream(), notifier);
}


PUBLIC void setParam(cchar *key, cchar *value)
{
    espSetParam(getStream(), key, value);
}


PUBLIC EdiRec *setRec(EdiRec *rec)
{
    return espSetRec(getStream(), rec);
}


PUBLIC void setSessionVar(cchar *key, cchar *value)
{
    httpSetSessionVar(getStream(), key, value);
}


PUBLIC void setStatus(int status)
{
    espSetStatus(getStream(), status);
}


PUBLIC cchar *session(cchar *key)
{
    return getSessionVar(key);
}


PUBLIC void setTimeout(void *proc, MprTicks timeout, void *data)
{
    mprCreateEvent(getStream()->dispatcher, "setTimeout", (int) timeout, proc, data, 0);
}


PUBLIC void showRequest()
{
    espShowRequest(getStream());
}


PUBLIC EdiGrid *sortGrid(EdiGrid *grid, cchar *sortColumn, int sortOrder)
{
    return ediSortGrid(grid, sortColumn, sortOrder);
}


PUBLIC void updateCache(cchar *uri, cchar *data, int lifesecs)
{
    espUpdateCache(getStream(), uri, data, lifesecs);
}


PUBLIC bool updateField(cchar *tableName, cchar *key, cchar *fieldName, cchar *value)
{
    return ediUpdateField(getDatabase(), tableName, key, fieldName, value) == 0;
}


PUBLIC bool updateFields(cchar *tableName, MprJson *params)
{
    EdiRec  *rec;
    cchar   *key;

    key = mprReadJson(params, "id");
    if ((rec = ediSetFields(ediReadRec(getDatabase(), tableName, key), params)) == 0) {
        return 0;
    }
    return updateRec(rec);
}


PUBLIC bool updateRec(EdiRec *rec)
{
    if (!rec) {
        feedback("error", "Cannot save record");
        return 0;
    }
    setRec(rec);
    if (ediUpdateRec(getDatabase(), rec) < 0) {
        feedback("error", "Cannot save %s", stitle(rec->tableName));
        return 0;
    }
    feedback("info", "Saved %s", stitle(rec->tableName));
    return 1;
}


PUBLIC bool updateRecFromParams(cchar *table)
{
    return updateRec(setFields(readRec(table, param("id")), params()));
}


PUBLIC cchar *uri(cchar *target, ...)
{
    va_list     args;
    cchar       *uri;

    va_start(args, target);
    uri = sfmtv(target, args);
    va_end(args);
    return httpLink(getStream(), uri);
}


PUBLIC cchar *absuri(cchar *target, ...)
{
    va_list     args;
    cchar       *uri;

    va_start(args, target);
    uri = sfmtv(target, args);
    va_end(args);
    return httpLinkAbs(getStream(), uri);
}


#if DEPRECATED || 1
/*
    <% stylesheets(patterns); %>

    Where patterns may contain *, ** and !pattern for exclusion
 */
PUBLIC void stylesheets(cchar *patterns)
{
    HttpStream    *stream;
    HttpRx      *rx;
    HttpRoute   *route;
    EspRoute    *eroute;
    MprList     *files;
    cchar       *filename, *ext, *uri, *path, *kind, *version, *clientDir;
    int         next;

    stream = getStream();
    rx = stream->rx;
    route = rx->route;
    eroute = route->eroute;
    patterns = httpExpandRouteVars(route, patterns);
    clientDir = httpGetDir(route, "documents");

    if (!patterns || !*patterns) {
        version = espGetConfig(route, "version", "1.0.0");
        if (eroute->combineSheet) {
            /* Previously computed combined stylesheet filename */
            stylesheets(eroute->combineSheet);

        } else if (espGetConfig(route, "http.content.combine[@=css]", 0)) {
            if (espGetConfig(route, "http.content.minify[@=css]", 0)) {
                eroute->combineSheet = sfmt("css/all-%s.min.css", version);
            } else {
                eroute->combineSheet = sfmt("css/all-%s.css", version);
            }
            stylesheets(eroute->combineSheet);

        } else {
            /*
                Not combining into a single stylesheet, so give priority to all.less over all.css if present
                Load a pure CSS incase some styles need to be applied before the lesssheet is parsed
             */
            ext = espGetConfig(route, "http.content.stylesheets", "css");
            filename = mprJoinPathExt("css/all", ext);
            path = mprJoinPath(clientDir, filename);
            if (mprPathExists(path, R_OK)) {
                stylesheets(filename);
            } else if (!smatch(ext, "less")) {
                path = mprJoinPath(clientDir, "css/all.less");
                if (mprPathExists(path, R_OK)) {
                    stylesheets("css/all.less");
                }
            }
        }
    } else {
        if (sends(patterns, "all.less")) {
            path = mprJoinPath(clientDir, "css/fix.css");
            if (mprPathExists(path, R_OK)) {
                stylesheets("css/fix.css");
            }
        }
        if ((files = mprGlobPathFiles(clientDir, patterns, MPR_PATH_RELATIVE)) == 0 || mprGetListLength(files) == 0) {
            files = mprCreateList(0, 0);
            mprAddItem(files, patterns);
        }
        for (ITERATE_ITEMS(files, path, next)) {
            path = sjoin("~/", strim(path, ".gz", MPR_TRIM_END), NULL);
            uri = httpLink(stream, path);
            kind = mprGetPathExt(path);
            if (smatch(kind, "css")) {
                espRender(stream, "<link rel='stylesheet' type='text/css' href='%s' />\n", uri);
            } else {
                espRender(stream, "<link rel='stylesheet/%s' type='text/css' href='%s' />\n", kind, uri);
            }
        }
    }
}


/*
    <% scripts(patterns); %>

    Where patterns may contain *, ** and !pattern for exclusion
 */
PUBLIC void scripts(cchar *patterns)
{
    HttpStream    *stream;
    HttpRx      *rx;
    HttpRoute   *route;
    EspRoute    *eroute;
    MprList     *files;
    MprJson     *cscripts, *script;
    cchar       *uri, *path, *version;
    int         next, ci;

    stream = getStream();
    rx = stream->rx;
    route = rx->route;
    eroute = route->eroute;
    patterns = httpExpandRouteVars(route, patterns);

    if (!patterns || !*patterns) {
        version = espGetConfig(route, "version", "1.0.0");
        if (eroute->combineScript) {
            scripts(eroute->combineScript);
        } else if (espGetConfig(route, "http.content.combine[@=js]", 0)) {
            if (espGetConfig(route, "http.content.minify[@=js]", 0)) {
                eroute->combineScript = sfmt("all-%s.min.js", version);
            } else {
                eroute->combineScript = sfmt("all-%s.js", version);
            }
            scripts(eroute->combineScript);
        } else {
            if ((cscripts = mprGetJsonObj(route->config, "client.scripts")) != 0) {
                for (ITERATE_JSON(cscripts, script, ci)) {
                    scripts(script->value);
                }
            }
        }
        return;
    }
    if ((files = mprGlobPathFiles(httpGetDir(route, "client"), patterns, MPR_PATH_RELATIVE)) == 0 ||
            mprGetListLength(files) == 0) {
        files = mprCreateList(0, 0);
        mprAddItem(files, patterns);
    }
    for (ITERATE_ITEMS(files, path, next)) {
        if (schr(path, '$')) {
            path = stemplateJson(path, route->config);
        }
        path = sjoin("~/", strim(path, ".gz", MPR_TRIM_END), NULL);
        uri = httpLink(stream, path);
        espRender(stream, "<script src='%s' type='text/javascript'></script>\n", uri);
    }
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
