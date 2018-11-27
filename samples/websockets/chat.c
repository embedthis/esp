/*
    chat.c - WebSockets chat server
 */
#include "esp.h"

/*
    This is a reference to a thread safe list of client connections
 */
static MprList  *clients;

/*
    Hold a message destined for clients
 */
typedef struct Msg {
    HttpConn    *conn;
    HttpPacket  *packet;
} Msg;


/*
    ESP garbage collection rention callback.
 */
static void manageMsg(Msg *msg, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(msg->conn);
        mprMark(msg->packet);
    }
}

/*
    Send message to a connection
    This is the callback function invoked from the connection event dispatcher.
 */
static void chat(Msg *msg)
{
    HttpConn    *conn;
    HttpPacket  *packet;

    conn = msg->conn;
    packet = msg->packet;
    httpSendBlock(conn, packet->type, httpGetPacketStart(packet), httpGetPacketLength(packet), 0);
}

/*
    Event callback. Invoked for incoming web socket messages and other events of interest.
    Running on the connection event dispatcher using an Mpr worker thread.
 */
static void chat_callback(HttpConn *conn, int event, int arg)
{
    HttpPacket  *packet;
    HttpConn    *client;
    Msg         *msg;
    int         flags, next;

    if (event == HTTP_EVENT_READABLE) {
        packet = httpGetPacket(conn->readq);
        if (packet->type == WS_MSG_TEXT || packet->type == WS_MSG_BINARY) {
            for (ITERATE_ITEMS(clients, client, next)) {
                /*
                    Send the message to each connection. This must be done using each connections event dispatcher
                    to ensure we don't conflict with other activity on the connection that may happen on another worker
                    thread at the same time. Set flags = MPR_EVENT_WAIT if calling mprCreateEvent from a non-esp thread and
                    you want to wait until the event has completed before returning.
                    This will then wait till the event is run before returning.
                 */
                msg = mprAllocObj(Msg, manageMsg);
                msg->conn = client;
                msg->packet = packet;
                flags = 0;
                mprCreateEvent(client->dispatcher, "chat", 0, chat, msg, flags);
            }
        }
    } else if (event == HTTP_EVENT_APP_CLOSE) {
        /*
            This event is in response to a web sockets close event
         */
        mprLog("chat info", 0, "Close event. Status status %d, orderly closed %d, reason %s", arg,
        httpWebSocketOrderlyClosed(conn), httpGetWebSocketCloseReason(conn));

    } else if (event == HTTP_EVENT_DESTROY) {
        /*
            This is invoked when the client connection is closed. This API is thread safe.
         */
        mprRemoveItem(clients, conn);
    }
}

/*
    Action to run in response to the "test/chat" URI
 */
static void chat_action()
{
    HttpConn    *conn;

    /*
        Add the client connect to the list of clients. This API is thread-safe.
     */
    conn = getConn();
    mprAddItem(clients, conn);

    /*
        Don't automatically finalize (complete) the request when this routine returns. This keeps the connection open.
     */
    dontAutoFinalize();

    /*
        Establish the event callback that will be called for I/O events of interest for all connections.
     */
    espSetNotifier(conn, chat_callback);
}


/*
    Initialize the "chat" loadable module
 */
ESP_EXPORT int esp_controller_websockets_chat(HttpRoute *route)
{
    /*
        Create a list of client connections. Add as a root object to preserve from GC.
     */
    clients = mprCreateList(0, 0);
    mprAddRoot(clients);

    /*
        Define the "chat" action that will run when the "test/chat" URI is invoked
     */
    espDefineAction(route, "test/chat", chat_action);
    return 0;
}
