/*
    user.c - User login
 */
#include "esp.h"

/*
    Action to login a user. Redirects to /public/login.esp if login fails
    this is bound to the /user/login URI.
 */
static void loginUser() {
    if (httpLogin(getStream(), param("username"), param("password"))) {
        redirect("/index.esp");
    } else {
        feedback("error", "Invalid Login");
        redirect("/public/login.esp");
    }
}

/*
    Logout the user and redirect to the login page
    this is bound to the /user/logout URI.
 */
static void logoutUser() {
    httpLogout(getStream());
    redirect("/public/login.esp");
}

/*
    Dynamic module initialization.
    If using with a static link, call this function from your main program after initializing ESP.
 */
ESP_EXPORT int esp_controller_login_database_user(HttpRoute *route)
{
    /*
        Define the login / logout actions
     */
    espAction(route, "user/login", NULL, loginUser);
    espAction(route, "user/logout", NULL, logoutUser);
    return 0;
}
