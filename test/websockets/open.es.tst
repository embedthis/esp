/*
    open.tst - WebSocket open test
 */

const PORT = tget('TM_HTTP_PORT') || "4100"
const WS = "ws://127.0.0.1:" + PORT + "/websockets/basic/open"
const TIMEOUT = 5000

ttrue(WebSocket)
let ws = new WebSocket(WS)
ttrue(ws)
ttrue(ws.readyState == WebSocket.CONNECTING)

let opened = false
ws.onopen = function (event) {
    opened = true
}
ws.wait(WebSocket.OPEN, TIMEOUT)
ttrue(opened)
ttrue(ws.readyState == WebSocket.OPEN)

//  Cleanup
ws.close()
ttrue(ws.readyState == WebSocket.CLOSING)
ws.wait(WebSocket.CLOSED, TIMEOUT)
ttrue(ws.readyState == WebSocket.CLOSED)
