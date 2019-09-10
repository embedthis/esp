/*
    websockets.c - Test WebSockets
 */
#include "esp.h"

/*
    Diagnostic trace for tests
 */
static void traceEvent(HttpStream *stream, int event, int arg)
{
    HttpPacket  *packet;

    if (event == HTTP_EVENT_READABLE) {
        /*
            Peek at the readq rather than doing httpGetPacket()
            The last frame in a message has packet->last == true
         */
        packet = stream->readq->first;
        mprDebug("webock test", 5, "read %s event, last %d", packet->type == WS_MSG_TEXT ? "text" : "binary",
            packet->last);
        mprDebug("webock test", 5, "read: (start of data only) \"%s\"",
            snclone(mprGetBufStart(packet->content), 40));

    } else if (event == HTTP_EVENT_APP_CLOSE) {
        mprDebug("webock test", 5, "close event. Status status %d, orderly closed %d, reason %s", arg,
            httpWebSocketOrderlyClosed(stream), httpGetWebSocketCloseReason(stream));

    } else if (event == HTTP_EVENT_ERROR) {
        mprDebug("webock test", 2, "error event");
    }
}

static void dummy_callback(HttpStream *stream, int event, int arg)
{
}

static void dummy_action() {
    dontAutoFinalize();
    espSetNotifier(getStream(), dummy_callback);
}

static void len_callback(HttpStream *stream, int event, int arg)
{
    HttpPacket      *packet;
    HttpWebSocket   *ws;

    traceEvent(stream, event, arg);
    if (event == HTTP_EVENT_READABLE) {
        /*
            Get and discard the packet. traceEvent will have traced it for us.
         */
        packet = httpGetPacket(stream->readq);
        assert(packet);
        /*
            Ignore precedding packets and just respond and echo the last
         */
        if (packet->last) {
            ws = stream->rx->webSocket;
            httpSend(stream, "{type: %d, last: %d, length: %d, data: \"%s\"}\n", packet->type, packet->last,
                ws->messageLength, snclone(mprGetBufStart(packet->content), 10));
        }
    }
}

static void len_action() {
    dontAutoFinalize();
    espSetNotifier(getStream(), len_callback);
}


/*
    Autobahn test echo server
    Must configure LimitWebSocketsPacket to be larger than the biggest expected message so we receive complete messages.
    Otherwise, we need to buffer and aggregate messages here.
 */
static void echo_callback(HttpStream *stream, int event, int arg)
{
    HttpPacket  *packet;

    if (event == HTTP_EVENT_READABLE) {
        packet = httpGetPacket(stream->readq);
        if (packet->type == WS_MSG_TEXT || packet->type == WS_MSG_BINARY) {
            httpSendBlock(stream, packet->type, httpGetPacketStart(packet), httpGetPacketLength(packet), 0);
        }
    }
}

static void echo_action() {
    dontAutoFinalize();
    espSetNotifier(getStream(), echo_callback);
}


/*
    Test sending an empty text message, followed by an orderly close
 */
static void empty_response()
{
    httpSendBlock(getStream(), WS_MSG_TEXT, "", 0, 0);
    httpSendClose(getStream(), WS_STATUS_OK, "OK");
}


/*
    Big single message written with one send(). The WebSockets filter will break this into frames as required.
 */
static void big_response()
{
    HttpStream    *stream;
    MprBuf      *buf;
    ssize       wrote;
    int         i, count;

    stream = getStream();
    /*
        First message is big, but in a single send. The middleware should break this into frames unless you call:
            httpSetWebSocketPreserveFrames(stream, 1);
        This will regard each call to httpSendBlock as a frame.
        NOTE: this is not an ideal pattern (so don't copy it).
     */
    buf = mprCreateBuf(51000, 0);
    mprAddRoot(buf);
    count = 1000;
    for (i = 0; i < count; i++) {
        mprPutToBuf(buf, "%8d:01234567890123456789012345678901234567890\n", i);
        mprYield(0);
    }
    mprRemoveRoot(buf);
    mprAddNullToBuf(buf);
    /* Retain just for GC */
    httpSetWebSocketData(stream, buf);

    /*
        Note: this will block while writing the entire message. It may be quicker to use HTTP_BUFFER but will
        use more memory.  Not point using HTTP_NON_BLOCK as we need to close after sending the message.
     */
    if ((wrote = httpSendBlock(stream, WS_MSG_TEXT, mprGetBufStart(buf), mprGetBufLength(buf), HTTP_BLOCK)) < 0) {
        httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot send big message");
        return;
    }
    httpSendClose(stream, WS_STATUS_OK, "OK");
}

/*
    Multiple-frame response message with explicit continuations.
    The WebSockets filter will encode each call to httpSendBlock into a frame.
    Even if large blocks are written, HTTP_MORE assures that the block will be encoded as a single frame.
 */
static void frames_response()
{
    HttpStream    *stream;
    cchar       *str;
    int         i, more, count;

    stream = getStream();
    count = 1000;

    for (i = 0; i < count; i++) {
        str = sfmt("%8d: Hello\n", i);
        more = (i < (count - 1)) ? HTTP_MORE : 0;
        if (httpSendBlock(stream, WS_MSG_TEXT, str, slen(str), HTTP_BUFFER | more) < 0) {
            httpError(stream, HTTP_CODE_INTERNAL_SERVER_ERROR, "Cannot send message: %d", i);
            return;
        }
    }
    httpSendClose(stream, WS_STATUS_OK, "OK");
}


/*
    Chat server
 */
static MprList  *clients;

typedef struct Msg {
    HttpStream    *stream;
    HttpPacket  *packet;
} Msg;

static void manageMsg(Msg *msg, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(msg->stream);
        mprMark(msg->packet);
    }
}

static void chat(Msg *msg)
{
    HttpStream    *stream;
    HttpPacket  *packet;

    stream = msg->stream;
    packet = msg->packet;
    httpSendBlock(stream, packet->type, httpGetPacketStart(packet), httpGetPacketLength(packet), 0);
}


/*
    Event callback. Invoked for incoming web socket messages and other events of interest.
 */
static void chat_callback(HttpStream *stream, int event, int arg)
{
    HttpPacket  *packet;
    HttpStream    *client;
    Msg         *msg;
    int         next;

    if (event == HTTP_EVENT_READABLE) {
        packet = httpGetPacket(stream->readq);
        if (packet->type == WS_MSG_TEXT || packet->type == WS_MSG_BINARY) {
            for (ITERATE_ITEMS(clients, client, next)) {
                msg = mprAllocObj(Msg, manageMsg);
                msg->stream = client;
                msg->packet = packet;
                mprCreateEvent(client->dispatcher, "chat", 0, chat, msg, 0);
            }
        }
    } else if (event == HTTP_EVENT_APP_CLOSE) {
        mprDebug("websock", 5, "close event. Status status %d, orderly closed %d, reason %s", arg,
        httpWebSocketOrderlyClosed(stream), httpGetWebSocketCloseReason(stream));
        mprRemoveItem(clients, stream);
    }
}


/*
    Action to run in response to the "test/chat" URI
 */
static void chat_action()
{
    HttpStream    *stream;

    stream = getStream();
    mprAddItem(clients, stream);
    dontAutoFinalize();
    espSetNotifier(getStream(), chat_callback);
}


ESP_EXPORT int esp_controller_esptest_websockets(HttpRoute *route) {
    clients = mprCreateList(0, 0);
    mprAddRoot(clients);
    espAction(route, "basic/construct", NULL, dummy_action);
    espAction(route, "basic/open", NULL, dummy_action);
    espAction(route, "basic/send", NULL, dummy_action);
    espAction(route, "basic/echo", NULL, echo_action);
    espAction(route, "basic/ssl", NULL, len_action);
    espAction(route, "basic/len", NULL, len_action);
    espAction(route, "basic/echo", NULL, echo_action);
    espAction(route, "basic/empty", NULL, empty_response);
    espAction(route, "basic/big", NULL, big_response);
    espAction(route, "basic/frames", NULL, frames_response);
    espAction(route, "basic/chat", NULL, chat_action);
    return 0;
}
