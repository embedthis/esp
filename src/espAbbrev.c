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
    espAddHeaderString(getConn(), key, value);
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
    HttpConn    *conn;

    conn = getConn();
    if (httpCanUser(conn, abilities)) {
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
    return espCreateSession(getConn());
}


/*
    Destroy a session and erase the session state data.
    This emits an expired Set-Cookie header to the browser to force it to erase the cookie.
 */
PUBLIC void destroySession()
{
    httpDestroySession(getConn());
}


PUBLIC void dontAutoFinalize()
{
    espSetAutoFinalizing(getConn(), 0);
}


PUBLIC bool feedback(cchar *kind, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    espSetFeedbackv(getConn(), kind, fmt, args);
    va_end(args);
    /*
        Return true if there is not an error feedback message
     */
    return getFeedback("error") == 0;
}


PUBLIC void finalize()
{
    espFinalize(getConn());
}


PUBLIC void flash(cchar *kind, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    espSetFlashv(getConn(), kind, fmt, args);
    va_end(args);
}


PUBLIC void flush()
{
    espFlush(getConn());
}


PUBLIC HttpAuth *getAuth()
{
    return espGetAuth(getConn());
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


PUBLIC HttpConn *getConn()
{
    HttpConn    *conn;

    conn = mprGetThreadData(((Esp*) MPR->espService)->local);
    if (conn == 0) {
        mprLog("error esp", 0, "Connection is not defined in thread local storage.\n"
        "If using a callback, make sure you invoke espSetConn with the connection before using the ESP abbreviated API");
    }
    return conn;
}


PUBLIC cchar *getCookies()
{
    return espGetCookies(getConn());
}


PUBLIC MprOff getContentLength()
{
    return espGetContentLength(getConn());
}


PUBLIC cchar *getContentType()
{
    return getConn()->rx->mimeType;
}


PUBLIC void *getData()
{
    return espGetData(getConn());
}


PUBLIC Edi *getDatabase()
{
    return espGetDatabase(getConn());
}


PUBLIC MprDispatcher *getDispatcher()
{
    HttpConn    *conn;

    if ((conn = getConn()) == 0) {
        return 0;
    }
    return conn->dispatcher;
}


PUBLIC cchar *getDocuments()
{
    return getConn()->rx->route->documents;
}


PUBLIC EspRoute *getEspRoute()
{
    return espGetEspRoute(getConn());
}


PUBLIC cchar *getFeedback(cchar *kind)
{
    return espGetFeedback(getConn(), kind);
}


PUBLIC cchar *getFlash(cchar *kind)
{
    return espGetFlash(getConn(), kind);
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
    return getConn()->grid;
}


PUBLIC cchar *getHeader(cchar *key)
{
    return espGetHeader(getConn(), key);
}


PUBLIC cchar *getMethod()
{
    return espGetMethod(getConn());
}


PUBLIC cchar *getQuery()
{
    return getConn()->rx->parsedUri->query;
}


PUBLIC EdiRec *getRec()
{
    return getConn()->record;
}


PUBLIC cchar *getReferrer()
{
    return espGetReferrer(getConn());
}


PUBLIC EspReq *getReq()
{
    return getConn()->data;
}


PUBLIC HttpRoute *getRoute()
{
    return espGetRoute(getConn());
}


PUBLIC cchar *getSecurityToken()
{
    return httpGetSecurityToken(getConn(), 0);
}


/*
    Get a session and return the session ID. Creates a session if one does not already exist.
 */
PUBLIC cchar *getSessionID()
{
    return espGetSessionID(getConn(), 1);
}


PUBLIC cchar *getSessionVar(cchar *key)
{
    return httpGetSessionVar(getConn(), key, 0);
}


PUBLIC cchar *getConfig(cchar *field)
{
    HttpRoute   *route;
    cchar       *value;

    route = getConn()->rx->route;
    if ((value = mprGetJson(route->config, field)) == 0) {
        return "";
    }
    return value;
}


PUBLIC MprList *getUploads()
{
    return espGetUploads(getConn());
}


PUBLIC cchar *getUri()
{
    return espGetUri(getConn());
}


PUBLIC bool hasGrid()
{
    return espHasGrid(getConn());
}


PUBLIC bool hasRec()
{
    return espHasRec(getConn());
}


PUBLIC bool isEof()
{
    return httpIsEof(getConn());
}


PUBLIC bool isFinalized()
{
    return espIsFinalized(getConn());
}


PUBLIC bool isSecure()
{
    return espIsSecure(getConn());
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
    return espUri(getConn(), target);
}


PUBLIC bool modeIs(cchar *kind)
{
    HttpRoute   *route;

    route = getConn()->rx->route;
    return smatch(route->mode, kind);
}


PUBLIC cchar *param(cchar *key)
{
    return espGetParam(getConn(), key, 0);
}


PUBLIC MprJson *params()
{
    return espGetParams(getConn());
}


PUBLIC ssize receive(char *buf, ssize len)
{
    return httpRead(getConn(), buf, len);
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
    espRedirect(getConn(), 302, target);
}


PUBLIC void redirectBack()
{
    espRedirectBack(getConn());
}


PUBLIC void removeCookie(cchar *name)
{
    espRemoveCookie(getConn(), name);
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
    httpRemoveSessionVar(getConn(), key);
}


PUBLIC ssize render(cchar *fmt, ...)
{
    va_list     args;
    ssize       count;
    cchar       *msg;

    va_start(args, fmt);
    msg = sfmtv(fmt, args);
    count = espRenderString(getConn(), msg);
    va_end(args);
    return count;
}


PUBLIC ssize renderCached()
{
    return espRenderCached(getConn());;
}


PUBLIC ssize renderConfig()
{
    return espRenderConfig(getConn());;
}


PUBLIC void renderError(int status, cchar *fmt, ...)
{
    va_list     args;
    cchar       *msg;

    va_start(args, fmt);
    msg = sfmt(fmt, args);
    espRenderError(getConn(), status, "%s", msg);
    va_end(args);
}


PUBLIC ssize renderFile(cchar *path)
{
    return espRenderFile(getConn(), path);
}


PUBLIC void renderFlash(cchar *kind)
{
    espRenderFlash(getConn(), kind);
}


PUBLIC ssize renderSafe(cchar *fmt, ...)
{
    va_list     args;
    ssize       count;
    cchar       *msg;

    va_start(args, fmt);
    msg = sfmtv(fmt, args);
    count = espRenderSafeString(getConn(), msg);
    va_end(args);
    return count;
}


PUBLIC ssize renderString(cchar *s)
{
    return espRenderString(getConn(), s);
}


PUBLIC void renderView(cchar *view)
{
    espRenderDocument(getConn(), view);
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
    httpAddSecurityToken(getConn(), 0);
}


PUBLIC ssize sendGrid(EdiGrid *grid)
{
    return espSendGrid(getConn(), grid, 0);
}


PUBLIC ssize sendRec(EdiRec *rec)
{
    return espSendRec(getConn(), rec, 0);
}


PUBLIC void sendResult(bool status)
{
    espSendResult(getConn(), status);
}


PUBLIC void setConn(HttpConn *conn)
{
    espSetConn(conn);
}


PUBLIC void setContentType(cchar *mimeType)
{
    espSetContentType(getConn(), mimeType);
}


PUBLIC void setCookie(cchar *name, cchar *value, cchar *path, cchar *cookieDomain, MprTicks lifespan, bool isSecure)
{
    espSetCookie(getConn(), name, value, path, cookieDomain, lifespan, isSecure);
}


PUBLIC void setData(void *data)
{
    espSetData(getConn(), data);
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
    getConn()->grid = grid;
    return grid;
}


PUBLIC void setHeader(cchar *key, cchar *fmt, ...)
{
    va_list     args;
    cchar       *value;

    va_start(args, fmt);
    value = sfmtv(fmt, args);
    espSetHeaderString(getConn(), key, value);
    va_end(args);
}


PUBLIC void setIntParam(cchar *key, int value)
{
    espSetIntParam(getConn(), key, value);
}


PUBLIC void setNotifier(HttpNotifier notifier)
{
    espSetNotifier(getConn(), notifier);
}


PUBLIC void setParam(cchar *key, cchar *value)
{
    espSetParam(getConn(), key, value);
}


PUBLIC EdiRec *setRec(EdiRec *rec)
{
    return espSetRec(getConn(), rec);
}


PUBLIC void setSessionVar(cchar *key, cchar *value)
{
    httpSetSessionVar(getConn(), key, value);
}


PUBLIC void setStatus(int status)
{
    espSetStatus(getConn(), status);
}


PUBLIC cchar *session(cchar *key)
{
    return getSessionVar(key);
}


PUBLIC void setTimeout(void *proc, MprTicks timeout, void *data)
{
    mprCreateEvent(getConn()->dispatcher, "setTimeout", (int) timeout, proc, data, 0);
}


PUBLIC void showRequest()
{
    espShowRequest(getConn());
}


PUBLIC void updateCache(cchar *uri, cchar *data, int lifesecs)
{
    espUpdateCache(getConn(), uri, data, lifesecs);
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
    return httpLink(getConn(), uri);
}


PUBLIC cchar *absuri(cchar *target, ...)
{
    va_list     args;
    cchar       *uri;

    va_start(args, target);
    uri = sfmtv(target, args);
    va_end(args);
    return httpLinkAbs(getConn(), uri);
}


#if DEPRECATED || 1
/*
    <% stylesheets(patterns); %>

    Where patterns may contain *, ** and !pattern for exclusion
 */
PUBLIC void stylesheets(cchar *patterns)
{
    HttpConn    *conn;
    HttpRx      *rx;
    HttpRoute   *route;
    EspRoute    *eroute;
    MprList     *files;
    cchar       *filename, *ext, *uri, *path, *kind, *version, *clientDir;
    int         next;

    conn = getConn();
    rx = conn->rx;
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
#if UNUSED
            uri = httpUriToString(httpGetRelativeUri(rx->parsedUri, httpLinkUri(conn, path, 0), 0), 0);
#else
            uri = httpLink(conn, path);
#endif
            kind = mprGetPathExt(path);
            if (smatch(kind, "css")) {
                espRender(conn, "<link rel='stylesheet' type='text/css' href='%s' />\n", uri);
            } else {
                espRender(conn, "<link rel='stylesheet/%s' type='text/css' href='%s' />\n", kind, uri);
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
    HttpConn    *conn;
    HttpRx      *rx;
    HttpRoute   *route;
    EspRoute    *eroute;
    MprList     *files;
    MprJson     *cscripts, *script;
    cchar       *uri, *path, *version;
    int         next, ci;

    conn = getConn();
    rx = conn->rx;
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
#if FUTURE
    client => public
#endif
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
#if UNUSED
        uri = httpUriToString(httpGetRelativeUri(rx->parsedUri, httpLinkUri(conn, path, 0), 0), 0);
#else
        uri = httpLink(conn, path);
#endif
        espRender(conn, "<script src='%s' type='text/javascript'></script>\n", uri);
    }
}


#endif
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
