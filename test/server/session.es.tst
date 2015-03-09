/*
    session.tst - ESP session tests
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:5100"
let http: Http = new Http

//  GET /session/login
http.get(HTTP + "/session/login")
ttrue(http.status == 200)
ttrue(http.response.contains("Please Login"))
let securityToken = http.header("X-XSRF-TOKEN")
let cookie = http.header("Set-Cookie")
if (cookie) {
    cookie = cookie.match(/(esp-esptest=.*);/)[1]
}

ttrue(cookie && cookie.contains("esp-esptest="))
http.reset()

//  POST /session/login
http.setCookie(cookie)
http.setHeader("X-XSRF-TOKEN", securityToken)
http.form(HTTP + "/session/login", { 
    username: "admin", 
    password: "secret", 
    color: "blue" 
})
ttrue(http.status == 200)
ttrue(http.response.contains("Valid Login"))
if (http.header("Set-Cookie")) {
    cookie = http.header("Set-Cookie");
    cookie = cookie.match(/(esp-esptest=.*);/)[1]
}
http.reset()


//  GET /session/login
http.setCookie(cookie)
http.get(HTTP + "/session/login")
ttrue(http.status == 200)
if (!http.response.contains("Logged in")) {
    print("RESPONSE", http.response)
    print("COOKIE WAS", cookie)
    dump(http.headers)
}
ttrue(http.response.contains("Logged in"))
http.close()
