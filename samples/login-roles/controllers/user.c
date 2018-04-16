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
    Dynamic module initialization
 */
ESP_EXPORT int esp_controller_login_form_user(HttpRoute *route) 
{
    espDefineBase(route, commonController);
    espDefineAction(route, "user-login", loginUser);
    espDefineAction(route, "user-logout", logoutUser);
    return 0;
}
