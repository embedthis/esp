/*
    big.tst - Test a big response

    This tests a multiple-frame response that will be received in multiple parts
 */

const PORT = tget('TM_HTTP_PORT') || "4100"
const WS = "ws://127.0.0.1:" + PORT + "/websockets/basic/big"
const TIMEOUT = 5000 * 1000

ttrue(WebSocket)

let ws = new WebSocket(WS)
ttrue(ws)
ttrue(ws.readyState == WebSocket.CONNECTING)

let received = 0, gotClose, gotError, msgCount = 0, lastEvent

ws.onmessage = function (event) {
    /*
        Will receive data here that may correspond to one or more frames of the message
        The data will be broken into packetSizes limited by the http max packet size.
        event.last will be true on the last frame
     */
    ttrue(event.data is String)
    received += event.data.length
    msgCount++
    lastEvent = event
}

ws.onclose = function (event) {
    gotClose = true
    ws.close()
}
ws.onerror = function (event) {
    print("ON ERROR")
}

ws.wait(WebSocket.OPEN, TIMEOUT)
ws.wait(WebSocket.CLOSED, TIMEOUT)
ttrue(ws.readyState == WebSocket.CLOSED)

ttrue(received == 51000)
ttrue(gotClose)
ttrue(msgCount > 5)
ttrue(lastEvent.last == true)
ttrue(!gotError)
