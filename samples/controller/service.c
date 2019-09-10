/*
    service.c - Test ESP controller
 */
#include    "esp.h"

/*
    Controller action
 */
static void hello() {
    render("Hello World\n");
    finalize();
}

/*
    Controller initialization. Invoked when the controller is loaded.
 */
ESP_EXPORT int esp_controller_test_service(HttpRoute *route) {
    espAction(route, "hello", NULL, hello);
    return 0;
}
