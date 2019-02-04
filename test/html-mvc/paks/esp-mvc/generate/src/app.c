/*
    app.c -- ${UAPP} Application Module (esp-server)

    This module is loaded when Appweb starts.
 */
#include "esp.h"

/*
    This base for controllers is called before processing each request
 */
static void base(HttpStream *stream) {
}

ESP_EXPORT int esp_app_${APP}(HttpRoute *route, MprModule *module) {
    espDefineBase(route, base);
    return 0;
}
