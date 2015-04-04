/*
    html-mvc.tst - ESP html-mvc tests
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:4100"

let http: Http = new Http
let prefix = HTTP

//  /html
http.followRedirects = true
http.get(prefix)
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

//  /html/all.less
http.get(prefix + "/css/all.less")
ttrue(http.status == 200)
ttrue(http.response.contains("Aggregate all stylesheets"))
http.close()

//  /html/do/post/init - this tests a controller without view
http.get(prefix + "/do/post/init")
ttrue(http.status == 200)
ttrue(http.response.contains('<h1>Create Post</h1>'))
if (http.status != 200) {
    print("FAILED", http.status)
    dump(http.headers)
    print(http.response)
    print("SLEEPING")
    App.sleep(1000)
    print('RETRY')
    http.get(prefix + "/do/post/init")
    print(http.status)
    dump(http.headers)
    print(http.response)
}
http.close()
