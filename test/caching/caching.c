/*
    caching.c - Test caching
 
    Assumes configuration of: LimitCache 64K, CacheItem 16K
 */
#include "esp.h"

//  This is configured for caching by API below
static void api() {
    render("{ when: %lld, uri: '%s', query: '%s' }\r\n", mprGetTicks(), getUri(), getQuery());
}

static void sml() {
    int     i;
    for (i = 0; i < 1; i++) {
        render("Line: %05d %s", i, "aaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccccddddddd<br/>\r\n");
        mprYield(0);
    }
    render("{ when: %lld, uri: '%s', query: '%s' }\r\n", mprGetTicks(), getUri(), getQuery());
}

static void medium() {
    int     i;
    //  This will emit ~8K (under the item limit)
    for (i = 0; i < 100; i++) {
        render("Line: %05d %s", i, "aaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccccddddddd<br/>\r\n");
        mprYield(0);
    }
    render("{ when: %lld, uri: '%s', query: '%s' }\r\n", mprGetTicks(), getUri(), getQuery());
}

static void big() {
    int     i;
    //  This will emit ~39K (under the item limit)
    for (i = 0; i < 500; i++) {
        render("Line: %05d %s", i, "aaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccccddddddd<br/>\r\n");
        mprYield(0);
    }
}

static void huge() { 
    int     i;
    //  This will emit ~390K (over the item limit)
    for (i = 0; i < 10000; i++) {
        render("Line: %05d %s", i, "aaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbccccccccccccccccccddddddd<br/>\r\n");
        mprYield(0);
    }
    render("{ when: %lld, uri: '%s', query: '%s' }\r\n", mprGetTicks(), getUri(), getQuery());
}

static void clear() { 
    espUpdateCache(getConn(), "/caching/manual", 0, 0);
    espUpdateCache(getConn(), "/caching/big", 0, 0);
    espUpdateCache(getConn(), "/caching/medium", 0, 0);
    espUpdateCache(getConn(), "/caching/small", 0, 0);
    espUpdateCache(getConn(), "/caching/api", 0, 0);
    espUpdateCache(getConn(), "/caching/api", 0, 0);
    render("cleared");
}

static void client() { 
    render("{ when: %lld, uri: '%s', query: '%s' }\r\n", mprGetTicks(), getUri(), getQuery());
}

static void manual() { 
    if (smatch(getQuery(), "send")) {
        setHeader("X-SendCache", "true");
        finalize();
    } else if (!espRenderCached(getConn())) {
        render("{ when: %lld, uri: '%s', query: '%s' }\r\n", mprGetTicks(), getUri(), getQuery());
    }
}

static void update() { 
    cchar   *data = sfmt("{ when: %lld, uri: '%s', query: '%s' }\r\n", mprGetTicks(), getUri(), getQuery());
    espUpdateCache(getConn(), "/caching/manual", data, 86400);
    render("done");
}

ESP_EXPORT int esp_controller_esptest_caching(HttpRoute *route, MprModule *module) {
    HttpRoute   *rp;

    espDefineAction(route, "caching/api", api);
    espDefineAction(route, "caching/big", big);
    espDefineAction(route, "caching/small", sml);
    espDefineAction(route, "caching/medium", medium);
    espDefineAction(route, "caching/clear", clear);
    espDefineAction(route, "caching/client", client);
    espDefineAction(route, "caching/huge", huge);
    espDefineAction(route, "caching/manual", manual);
    espDefineAction(route, "caching/update", update);

    //  This is not required for unit tests
    if ((rp = httpLookupRoute(route->host, "/caching/")) != 0) {
        espCache(rp, "/caching/{action}", 0, 0);
    }
    return 0;
}
