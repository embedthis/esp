/*
    espConfig.c -- ESP Configuration

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/*********************************** Includes *********************************/

#include    "esp.h"

/************************************* Locals *********************************/

#define ITERATE_CONFIG(route, obj, child, index) \
    index = 0, child = obj ? obj->children: 0; obj && index < obj->length && !route->error; child = child->next, index++

/************************************** Code **********************************/

static void loadApp(HttpRoute *parent, MprJson *prop)
{
    HttpRoute   *route;
    MprList     *files;
    cchar       *config, *prefix;
    int         next;

    if (prop->type & MPR_JSON_OBJ) {
        prefix = mprGetJson(prop, "prefix"); 
        config = mprGetJson(prop, "config");
        route = httpCreateInheritedRoute(parent);
        if (espLoadApp(route, prefix, config) < 0) {
            httpParseError(route, "Cannot define ESP application at: %s", config);
            return;
        }
        httpFinalizeRoute(route);

    } else if (prop->type & MPR_JSON_STRING) {
        files = mprGlobPathFiles(".", prop->value, MPR_PATH_RELATIVE);
        for (ITERATE_ITEMS(files, config, next)) {
            route = httpCreateInheritedRoute(parent);
            prefix = mprGetPathBase(mprGetPathDir(mprGetAbsPath(config)));
            if (espLoadApp(route, prefix, config) < 0) {
                httpParseError(route, "Cannot define ESP application at: %s", config);
                return;
            }
            httpFinalizeRoute(route);
        }
    }
}       


/*
    esp: {
        apps: 'myapp/esp.json',
        apps: [
            'apps/STAR/esp.json'
        ],
        apps: [
            { prefix: '/blog', config: 'blog/esp.json' }
        ],
    }
 */
static void parseApps(HttpRoute *route, cchar *key, MprJson *prop)
{
    MprJson     *child;
    int         ji;

    if (prop->type & MPR_JSON_STRING) {
        loadApp(route, prop);

    } else if (prop->type & MPR_JSON_OBJ) {
        loadApp(route, prop);
        
    } else if (prop->type & MPR_JSON_ARRAY) {
        for (ITERATE_CONFIG(route, prop, child, ji)) {
            loadApp(route, child);
        }
    }
}


static void parseCombine(HttpRoute *route, cchar *key, MprJson *prop)
{
    EspRoute    *eroute;

    eroute = route->eroute;
    if (smatch(prop->value, "true")) {
        eroute->combine = 1;
    } else {
        eroute->combine = 0;
    }
}


#if KEEP
/*
    Define Visual Studio environment if not already present
 */
static void defineVisualStudioEnv(HttpRoute *route)
{
    Http    *http;
    int     is64BitSystem;

    http = MPR->httpService;
    if (scontains(getenv("LIB"), "Visual Studio") &&
        scontains(getenv("INCLUDE"), "Visual Studio") &&
        scontains(getenv("PATH"), "Visual Studio")) {
        return;
    }
    if (scontains(http->platform, "-x64-")) {
        is64BitSystem = smatch(getenv("PROCESSOR_ARCHITECTURE"), "AMD64") || getenv("PROCESSOR_ARCHITEW6432");
        defineEnv(route, "LIB", "${WINSDK}\\LIB\\${WINVER}\\um\\x64;${WINSDK}\\LIB\\x64;${VS}\\VC\\lib\\amd64");
        if (is64BitSystem) {
            defineEnv(route, "PATH",
                "${VS}\\Common7\\IDE;${VS}\\VC\\bin\\amd64;${VS}\\Common7\\Tools;${VS}\\SDK\\v3.5\\bin;"
                "${VS}\\VC\\VCPackages;${WINSDK}\\bin\\x64");

        } else {
            /* Cross building on x86 for 64-bit */
            defineEnv(route, "PATH",
                "${VS}\\Common7\\IDE;${VS}\\VC\\bin\\x86_amd64;"
                "${VS}\\Common7\\Tools;${VS}\\SDK\\v3.5\\bin;${VS}\\VC\\VCPackages;${WINSDK}\\bin\\x86");
        }

    } else if (scontains(http->platform, "-arm-")) {
        /* Cross building on x86 for arm. No winsdk 7 support for arm */
        defineEnv(route, "LIB", "${WINSDK}\\LIB\\${WINVER}\\um\\arm;${VS}\\VC\\lib\\arm");
        defineEnv(route, "PATH", "${VS}\\Common7\\IDE;${VS}\\VC\\bin\\x86_arm;${VS}\\Common7\\Tools;"
            "${VS}\\SDK\\v3.5\\bin;${VS}\\VC\\VCPackages;${WINSDK}\\bin\\arm");

    } else {
        /* Building for X86 */
        defineEnv(route, "LIB", "${WINSDK}\\LIB\\${WINVER}\\um\\x86;${WINSDK}\\LIB\\x86;"
            "${WINSDK}\\LIB;${VS}\\VC\\lib");
        defineEnv(route, "PATH", "${VS}\\Common7\\IDE;${VS}\\VC\\bin;${VS}\\Common7\\Tools;"
            "${VS}\\SDK\\v3.5\\bin;${VS}\\VC\\VCPackages;${WINSDK}\\bin");
    }
    defineEnv(route, "INCLUDE", "${VS}\\VC\\INCLUDE;${WINSDK}\\include;${WINSDK}\\include\\um;"
        "${WINSDK}\\include\\shared");
}
#endif


static void defineEnv(HttpRoute *route, cchar *key, cchar *value)
{
    EspRoute    *eroute;
    MprJson     *child, *set;
    cchar       *arch;
    int         ji;

    eroute = route->eroute;
    if (smatch(key, "set")) {
        httpParsePlatform(HTTP->platform, NULL, &arch, NULL);
#if ME_WIN_LIKE
        if (smatch(value, "VisualStudio")) {
            if (scontains(getenv("LIB"), "Visual Studio") &&
                scontains(getenv("INCLUDE"), "Visual Studio") &&
                scontains(getenv("PATH"), "Visual Studio")) {
                return;
            }
        }
        if (scontains(HTTP->platform, "-x64-") &&
            !(smatch(getenv("PROCESSOR_ARCHITECTURE"), "AMD64") || getenv("PROCESSOR_ARCHITEW6432"))) {
            /* Cross 64 */
            arch = sjoin(arch, "-cross", NULL);
        }
#endif
        if ((set = mprGetJsonObj(route->config, sfmt("esp.build.env.%s.default", value))) != 0) {
            for (ITERATE_CONFIG(route, set, child, ji)) {
                defineEnv(route, child->name, child->value);
            }
        }
        if ((set = mprGetJsonObj(route->config, sfmt("esp.build.env.%s.%s", value, arch))) == 0) {
            httpParseError(route, "Cannnot find environment set %s.%s", value, arch);
            return;
        } else {
            for (ITERATE_CONFIG(route, set, child, ji)) {
                defineEnv(route, child->name, child->value);
            }
        }

    } else {
        value = espExpandCommand(route, value, "", "");
        mprAddKey(eroute->env, key, value);
        if (scaselessmatch(key, "PATH")) {
            if (eroute->searchPath) {
                eroute->searchPath = sclone(value);
            } else {
                eroute->searchPath = sjoin(eroute->searchPath, MPR_SEARCH_SEP, value, NULL);
            }
        }
    }
}


static void parseBuild(HttpRoute *route, cchar *key, MprJson *prop)
{
    EspRoute    *eroute;
    MprJson     *child, *env, *rules;
    cchar       *buildType, *os, *rule, *stem;
    int         ji;

    eroute = route->eroute;
    buildType = HTTP->staticLink ? "static" : "dynamic";
    httpParsePlatform(HTTP->platform, &os, NULL, NULL);

    stem = sfmt("esp.build.rules.%s.%s", buildType, os);
    if ((rules = mprGetJsonObj(route->config, stem)) == 0) {
        stem = sfmt("esp.build.rules.%s.default", buildType);
        rules = mprGetJsonObj(route->config, stem);
    }
    if (rules) {
        if ((rule = mprGetJson(route->config, sfmt("%s.%s", stem, "compile"))) != 0) {
            eroute->compile = rule;
        }
        if ((rule = mprGetJson(route->config, sfmt("%s.%s", stem, "link"))) != 0) {
            eroute->link = rule;
        }
        if ((env = mprGetJsonObj(route->config, sfmt("%s.%s", stem, "env"))) != 0) {
            if (eroute->env == 0) {
                eroute->env = mprCreateHash(-1, MPR_HASH_STABLE);
            }
            for (ITERATE_CONFIG(route, env, child, ji)) {
                defineEnv(route, child->name, child->value);
            }
        }
    } else {
        httpParseError(route, "Cannot find esp-compile rules for O/S \"%s\"", os);
    }
}


static void parseOptimize(HttpRoute *route, cchar *key, MprJson *prop)
{
    EspRoute    *eroute;

    eroute = route->eroute;
    eroute->compileMode = smatch(prop->value, "true") ? ESP_COMPILE_OPTIMIZED : ESP_COMPILE_SYMBOLS;
}

static void serverRouteSet(HttpRoute *route, cchar *set)
{
    httpAddRestfulRoute(route, "GET,POST", "/{action}(/)*$", "${action}", "{controller}");
}


static void restfulRouteSet(HttpRoute *route, cchar *set)
{
    httpAddResourceGroup(route, "{controller}");
}


#if DEPRECATED || 1
static void legacyRouteSet(HttpRoute *route, cchar *set)
{
    restfulRouteSet(route, "restful");
}
#endif


PUBLIC int espInitParser() 
{
    HttpRoute   *route;
    cchar       *path;

    httpDefineRouteSet("esp-server", serverRouteSet);
    httpDefineRouteSet("esp-restful", restfulRouteSet);
#if DEPRECATED || 1
    httpDefineRouteSet("esp-angular-mvc", legacyRouteSet);
    httpDefineRouteSet("esp-html-mvc", legacyRouteSet);
#endif
    
    httpAddConfig("esp", httpParseAll);
    httpAddConfig("esp.apps", parseApps);
    httpAddConfig("esp.build", parseBuild);
    httpAddConfig("esp.combine", parseCombine);
    httpAddConfig("esp.optimize", parseOptimize);

    path = mprJoinPath(mprGetAppDir(), "esp-compile.json");
    if (mprPathExists(path, R_OK)) {
        route = httpGetDefaultRoute(0);
        espRoute(route);
        if (httpLoadConfig(route, path) < 0) {
            mprLog("error esp", 0, "Cannot parse %s", path);
            return MPR_ERR_CANT_OPEN;
        }
    }
    return 0;
} 

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
