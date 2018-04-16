/*
    user.c - User login
 */
#include "esp.h"

/*
    Action to login a user. Redirects to /public/login.esp if login fails
 */
static void loginUser() {
    if (httpLogin(getConn(), param("username"), param("password"))) {
        redirect("/index.esp");
    } else {
        feedback("error", "Invalid Login");
        redirect("/public/login.esp");
    }       
}

/*
    Logout the user and redirect to the login page
 */
static void logoutUser() {                                                                             
    httpLogout(getConn());
    redirect("/public/login.esp");
}

/*
    Common controller run for every action invoked
    This tests if the user is logged in and authenticated.
    Access to certain pages are permitted without authentication so the user can login
 */
static void commonController(HttpConn *conn)
{
    cchar   *uri;

    if (!httpIsAuthenticated(conn)) {
        uri = getUri();
        if (sstarts(uri, "/public/") || smatch(uri, "/user/login") || smatch(uri, "/user/logout")) {
            return;
        }
        httpError(conn, HTTP_CODE_UNAUTHORIZED, "Access Denied. Login required");
    }
}

/*
    Callback from httpLogin to verify credentials using the password defined in the database. 
 */
static bool verifyUser(HttpConn *conn, cchar *username, cchar *password)
{
    HttpAuth    *auth;
    HttpUser    *user;
    HttpRx      *rx;
    EdiRec      *urec;

    rx = conn->rx;
    auth = rx->route->auth;

    if ((urec = readRecWhere("user", "username", "==", username)) == 0) {
        httpTrace(conn, "auth.login.error", "error", "msg: 'Cannot verify user', username: '%s'", username);
        return 0;
    }
    if (!mprCheckPassword(password, getField(urec, "password"))) {
        httpTrace(conn, "auth.login.error", "error", "msg: 'Password failed to authenticate', username: '%s'", username);
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
    httpSetConnUser(conn, user);

    httpTrace(conn, "auth.login.authenticated", "context", "msg: 'User authenticated', username: '%s'", username);
    return 1;
}


/*
    Dynamic module initialization
 */
ESP_EXPORT int esp_controller_login_custom_user(HttpRoute *route) 
{
    /*
        Define a custom authentication verification callback for the "app" auth store
     */
    httpSetAuthVerify(route->auth, verifyUser);

    /*
        Define the common controller called for all requests
     */
    espDefineBase(route, commonController);

    /*
        Define the login / logout actions
     */
    espDefineAction(route, "user-login", loginUser);
    espDefineAction(route, "user-logout", logoutUser);
    return 0;
}
