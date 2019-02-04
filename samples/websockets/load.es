/*
    This is a simple (manual) web socket load script.

    It will send and receive WS messages every 1/10 sec, forever.
 */

let ws = new WebSocket("ws://127.0.0.1:4000/ws/test/chat")

ws.onmessage = function (event) {
    print("GOT", event.data)
}

ws.onopen = function (event) {
    print("ONOPEN")
}

ws.onclose = function () {
    if (ws.readyState == WebSocket.Loaded) {
        App.emitter.fire("complete")
    }
}

let i = 0
while (true) {
    ws.send("Hello WebSocket World: " + i++)
    App.sleep(50)
}

ws.close()
