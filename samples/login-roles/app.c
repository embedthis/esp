/*
    user.c - User login using the "config" AuthStore and "app" AuthType.

    Validate passwords using app logic below. Passwords defined in the esp.json.
 */
#include "esp.h"

/*
    Common base run for every request.
 */
static void commonBase(HttpConn *conn)
{
    cchar   *uri;

    if (!httpIsAuthenticated(conn)) {
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
    Dynamic module initialization
    If using with a static link, call this function from your main program after initializing ESP.
 */
ESP_EXPORT int esp_app_login_roles(HttpRoute *route)
{
    espDefineBase(route, commonBase);
    return 0;
}
