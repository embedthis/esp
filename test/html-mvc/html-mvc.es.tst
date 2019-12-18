/*
    html-mvc.tst - ESP html-mvc tests
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:4100"
let http: Http = new Http
let prefix = HTTP

if (false && Config.OS == 'macosx') {
    tskip("skip on macosx")
} else {
    //  /html
    http.followRedirects = true
    http.get(prefix)
// print("STATUS", http.status)
// print("RESPONSE", http.response)
    ttrue(http.status == 200)
    ttrue(http.response.contains("<h1>Welcome to Embedded Server Pages</h1>"))
    http.close()

    //  /html/
    http.get(prefix + "/")
    ttrue(http.status == 200)
    ttrue(http.response.contains("<h1>Welcome to Embedded Server Pages</h1>"))
    http.close()

    //  /html/index.esp
    http.get(prefix + "/index.esp")
    ttrue(http.status == 200)
    ttrue(http.response.contains("<h1>Welcome to Embedded Server Pages</h1>"))
    http.close()

    //  /html/all.css
    http.get(prefix + "/css/all.css")
    ttrue(http.status == 200)
    ttrue(http.response.contains("Aggregate all stylesheets"))
    http.close()

    //  /html/post/init - this tests a controller without view
    http.get(prefix + "/post/init")
    ttrue(http.status == 200)
    ttrue(http.response.contains('<h1>Create Post</h1>'))
    http.close()
}
