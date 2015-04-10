/*
    big.tst - Test caching a big file
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:4100"
let http: Http = new Http

//  Clear cached data
http.get(HTTP + "/caching/clear")
ttrue(http.status == 200)

//  Get a document that will normally require chunking
http.get(HTTP + "/caching/big")
ttrue(http.status == 200)
ttrue(http.header("Transfer-Encoding") == "chunked")
ttrue(!http.header("Content-Length"))
ttrue(http.response.contains("Line: 00499 aaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccccddddddd<br/>"))

//  Get again, this time will be cached and will be sent not chunked
http.get(HTTP + "/caching/big")
ttrue(http.status == 200)
ttrue(!http.header("Transfer-Encoding"))
ttrue(http.header("Content-Length") == 39000)
ttrue(http.response.contains("Line: 00499 aaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccccddddddd<br/>"))

http.close()
