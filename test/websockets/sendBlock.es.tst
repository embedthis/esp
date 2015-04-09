/*
    sendBlock.tst - WebSocket sendBlock API test
 */

const PORT = tget('TM_HTTP_PORT') || "4100"
const WS = "ws://127.0.0.1:" + PORT + "/websockets/basic/len"
const TIMEOUT = 5000
const LEN = 10 * 1024 * 1024

let data = new ByteArray(LEN)
for (i in LEN / 50) {
    data.write("01234567890123456789012345678901234567890123456789")
}
let dataLength = data.length

ttrue(WebSocket)
let ws = new WebSocket(WS)
ttrue(ws)
ttrue(ws.readyState == WebSocket.CONNECTING)


let opened
ws.onopen = function (event) {
    opened = true
}
ws.onerror = function (event) {
    opened = false
}

let msg
ws.onmessage = function (event) {
    ttrue(event.data is String)
    msg = event.data
}

function sendBlock(data, options) {
    let mark = new Date
    msg = null
    options.type = WebSocket.MSG_TEXT
    do {
        let rc = ws.sendBlock(data, options)
        data.readPosition += rc
        options.type = WebSocket.MSG_CONT
        /*
            Normally this would be event based, but for a simple unit test, we just wait
         */
        App.sleep(10);
    } while (data.length > 0 && opened)
    while (opened && !msg && mark.elapsed < TIMEOUT) {
        App.run(10, true)
    }
    data.readPosition = 0
    return msg
}

ws.wait(WebSocket.OPEN, TIMEOUT)
ttrue(opened)

/*
    Non-blocking test
 */
response = sendBlock(data, { mode: WebSocket.NON_BLOCK })
let info = deserialize(response)
ttrue(info.length == dataLength)
ttrue(info.type == 1)
ttrue(info.last == 1)

/*
    Buffered test
 */
response = sendBlock(data, { mode: WebSocket.BUFFER })
let info = deserialize(response)
ttrue(info.length == dataLength)
ttrue(info.type == 1)
ttrue(info.last == 1)

/*
    Buffered test
 */
response = sendBlock(data, { mode: WebSocket.BLOCK })
let info = deserialize(response)
ttrue(info.length == dataLength)
ttrue(info.type == 1)
ttrue(info.last == 1)

ws.close()
ttrue(ws.readyState == WebSocket.CLOSING)
ws.wait(WebSocket.CLOSED, TIMEOUT)
ttrue(ws.readyState == WebSocket.CLOSED)
