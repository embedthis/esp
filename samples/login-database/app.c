/*
    app.c - Application code to manage authentication
 */

#include "esp.h"

/*
    Common base run for every request.
 */
static void commonBase(HttpStream *stream)
{
    cchar   *uri;

    if (!httpIsAuthenticated(stream)) {
        /*
            Access to certain pages are permitted without authentication so the user can login and logout.
         */
        uri = getUri();
        if (sstarts(uri, "/public/") || smatch(uri, "/user/login") || smatch(uri, "/user/logout")) {
            return;
        }
        feedback("error", "Access Denied. Login required.");
        redirect("/public/login.esp");
    }
}

/*
    Callback from httpLogin to verify credentials using the password defined in the database.
 */
static bool verifyUser(HttpStream *stream, cchar *username, cchar *password)
{
    HttpAuth    *auth;
    HttpUser    *user;
    HttpRx      *rx;
    EdiRec      *urec;

    rx = stream->rx;
    auth = rx->route->auth;

    if ((urec = readRecWhere("user", "username", "==", username)) == 0) {
        httpTrace(stream->trace, "auth.login.error", "error", "msg: 'Cannot verify user', username: '%s'", username);
        return 0;
    }
    if (!mprCheckPassword(password, getField(urec, "password"))) {
        httpTrace(stream->trace, "auth.login.error", "error", "msg: 'Password failed to authenticate', username: '%s'", username);
        mprSleep(500);
        return 0;
    }
    /*
        Cache the user and define the user roles. Thereafter, the app can use "httpCanUser" to test if the user
        has the required abilities (defined by their roles) to perform a given request or operation.
     */
    if ((user = httpLookupUser(auth, username)) == 0) {
        user = httpAddUser(auth, username, 0, ediGetFieldValue(urec, "roles"));
    }
    /*
        Define this as the authenticated and authorized user for this session
     */
    httpSetStreamUser(stream, user);

    httpTrace(stream->trace, "auth.login.authenticated", "context", "msg: 'User authenticated', username: '%s'", username);
    return 1;
}

/*
    Dynamic module initialization
    If using with a static link, call this function from your main program after initializing ESP.
 */
ESP_EXPORT int esp_app_login_database(HttpRoute *route)
{
    /*
        Define a custom authentication verification callback for the "app" auth store
        If we had other routes which are not children, we could use httpSetAuthStoreVerify to define
        the callback for all routes using the store.
     */
    httpSetAuthVerify(route->auth, verifyUser);

    /*
        Define the common base which is called for all requests before the action function is invoked.
        This base will check if the client is authenticated.
     */
    espDefineBase(route, commonBase);
    return 0;
}
