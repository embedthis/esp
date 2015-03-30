/*
    manual.tst - Test manual caching mode
 */

const HTTP = tget('TM_HTTP') || "127.0.0.1:4100"
let http: Http = new Http

//  Prep and clear the cache
http.get(HTTP + "/caching/clear")
ttrue(http.status == 200)

//  1. Test that content is being cached
//  Initial get
http.get(HTTP + "/caching/manual")
ttrue(http.status == 200)
let resp = deserialize(http.response)
let first = resp.number
ttrue(resp.uri == "/caching/manual")
ttrue(resp.query == "null")

//  Second get, should get the same content (number must not change)
//  This is being done manually by the "manual" method in the cache controller
http.get(HTTP + "/caching/manual")
ttrue(http.status == 200)
resp = deserialize(http.response)
ttrue(resp.number == first)
ttrue(resp.uri == "/caching/manual")
ttrue(resp.query == "null")

//  Update the cache
http.get(HTTP + "/caching/update?updated=true")
ttrue(http.status == 200)
ttrue(http.response == "done")

//  Get again, should get updated cached data
http.get(HTTP + "/caching/manual")
ttrue(http.status == 200)
resp = deserialize(http.response)
ttrue(resp.query == "updated=true") 


//  Test X-SendCache
http.get(HTTP + "/caching/manual?send")
ttrue(http.status == 200)
resp = deserialize(http.response)
ttrue(resp.query == "updated=true") 

http.close()
