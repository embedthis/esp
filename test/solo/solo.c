/*
    solo.c - Stand-alone ESP controller
 */
#include "esp.h"

/*
    Test streaming input
 */
static void soloStreamCallback(HttpConn *conn, int event, int arg)
{
    HttpPacket      *packet;

    if (event == HTTP_EVENT_READABLE) {
        while ((packet = httpGetPacket(conn->readq)) != 0) {
            if (packet->flags & HTTP_PACKET_END) {
                render("-done-");
                finalize();
            }
        }
    }
}

static void soloStream() {
    dontAutoFinalize();
    espSetNotifier(getConn(), soloStreamCallback);
}


ESP_EXPORT int esp_controller_esptest_solo(HttpRoute *route, MprModule *module) {
    espDefineAction(route, "solo-stream", soloStream);
    return 0;
}
