/*
    len-150.tst - Test a message requires multiple bytes for its length

    WebSockets uses one byte for a length <= 125 bytes
 */

const PORT = tget('TM_HTTP_PORT') || "4100"
const WS = "ws://127.0.0.1:" + PORT + "/websockets/basic/len"
const TIMEOUT = 5000
const LEN = 150

ttrue(WebSocket)
let ws = new WebSocket(WS)
ttrue(ws)
ttrue(ws.readyState == WebSocket.CONNECTING)

let response
ws.onmessage = function (event) {
    ttrue(event.data is String)
    response = event.data
    ws.close()
}

ws.wait(WebSocket.OPEN, TIMEOUT)
let msg = "0123456789".times(LEN / 10)
ttrue(msg.length >= 126)
ws.send(msg)

ws.wait(WebSocket.CLOSED, TIMEOUT)
ttrue(ws.readyState == WebSocket.CLOSED)

//  Text == 1, last == 1, first 10 data bytes
let info = deserialize(response)
ttrue(info.type == 1)
ttrue(info.last == 1)
ttrue(info.length == LEN)
ttrue(info.data == "0123456789")
