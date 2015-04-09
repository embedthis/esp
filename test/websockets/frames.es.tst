/*
    frames.tst - Test a multi-frame response

    This tests a receiving each frame separately.
 */

const PORT = tget('TM_HTTP_PORT') || "4100"
const WS = "ws://127.0.0.1:" + PORT + "/websockets/basic/frames"
const TIMEOUT = 5000 * 1000

/*
    We expect one big message, but by setting "frames" to true, we will see each frame individually.
 */
let ws = new WebSocket(WS, null, {frames: true})
ttrue(ws)
ttrue(ws.readyState == WebSocket.CONNECTING)

let received = 0, gotClose, gotError, msgCount = 0, lastEvent

ws.onmessage = function (event) {
    /*
        Will receive data here that may correspond to one or more frames of the message
        event.last will be true on the last frame
     */
    ttrue(event.data is String)
    ttrue(event.data.length == 16)
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

ttrue(received == 16000)
ttrue(gotClose)
ttrue(msgCount > 1)
ttrue(lastEvent.last == true)
ttrue(!gotError)
