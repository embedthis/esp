/*
    Combined.tst - Test combined caching. Params are ignored for caching URIs.
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:4100"
let http: Http = new Http

//  Clear cache
http.setHeader("Cache-Control", "no-cache")
http.get(HTTP + "/combined/caching.esp")
http.wait()

//  1. Test that content is being cached
//  Initial get
http.get(HTTP + "/combined/caching.esp")
ttrue(http.status == 200)
let resp = deserialize(http.response)
let first = resp.number
ttrue(resp.uri == "/combined/caching.esp")
ttrue(resp.query == "null")

//  Second get, should get the same content (number must not change)
http.get(HTTP + "/combined/caching.esp")
ttrue(http.status == 200)
resp = deserialize(http.response)
ttrue(resp.number == first)
ttrue(resp.uri == "/combined/caching.esp")
ttrue(resp.query == "null")

/*

//  2. Test that different request parameters cache the same
http.get(HTTP + "/combined/caching.esp?a=b&c=d")
ttrue(http.status == 200)
resp = deserialize(http.response)
let firstQuery = resp.number
ttrue(resp.number == first)
ttrue(resp.uri == "/combined/caching.esp")
ttrue(resp.query == "null")
*/

http.close()
