/*
    version.c -- SemVer version support

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "mpr-version.h"
#include    "pcre.h"

/*********************************** Defines *********************************/
/*
    Each version number portion is limited to 64K
 */
#define MAX_VER         UINT64(0xFFFF)
#define MAX_VER_STR     "65535"
#define VER_FACTOR      UINT64(0x10000)

#define SEM_STRICT      "(\\d+\\.\\d+\\.\\d+)(-.*)*$"
#define SEM_VER         "(\\d[^.\\-]*\\.\\d[^.\\-]*\\.\\d[^.\\-]*)(-.*)*"
#define SEM_CRITERIA    "((?:\\d[^.\\-]*|[xX*])\\.(?:\\d[^.\\-]*|[xX*])\\.(?:\\d[^.\\-]*|[xX*]))(-.*)*|(\\*)|(^$)"
#define SEM_EXPR        "([~^]|<=|<|>=|>|==)*(.+)"

static void *semVer;        /* Parse SemVer version strings */
static void *semCriteria;   /* Parse version criteria expressions */
static void *semExpr;       /* Parse a SemVer expression */

/********************************** Forwards **********************************/

static void manageVersion(MprVersion *vp, int flags);
static char *cleanVersion(cchar *version);
static char *completeVersion(cchar *version, cchar *fill);
static bool inRange(MprVersion *vp, cchar *expr);
static cchar *numberToVersion(uint64 num);
static int partCount(cchar *version);
static void semVerInit();
static void *srcompile(cchar *pattern);
static int srmatch(cchar *s, void *pattern, ...);
static void versionTerminate();
static int64 versionToNumber(cchar *version);

/*********************************** Methods **********************************/

MprVersion *mprCreateVersion(cchar *version)
{
    MprVersion  *vp;
    char        *all;

    if ((vp = mprAllocObj(MprVersion, manageVersion)) == 0) {
        return vp;
    }
    if (!version) {
        version = "0";
    }
    vp->full = cleanVersion(version);
    version = completeVersion(vp->full, "0");

    semVerInit();
    if (srmatch(version, semVer, &all, &vp->baseVersion, &vp->preVersion, NULL) <= 0) {
        /* Do not return null - just leave ok as false */
        return vp;
    }
    vp->numberVersion = versionToNumber(vp->baseVersion);
    vp->major = (int) (vp->numberVersion / VER_FACTOR / VER_FACTOR);
    vp->minor = (int) (vp->numberVersion / VER_FACTOR % VER_FACTOR);
    vp->patch = (int) (vp->numberVersion % VER_FACTOR);
    vp->ok = 1;
    return vp;
}


static void manageVersion(MprVersion *vp, int flags) 
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(vp->full);
        mprMark(vp->baseVersion);
        mprMark(vp->preVersion);
    }
}


static void semVerInit()
{
    if (!semVer) {
        mprGlobalLock();
        if (!semVer) {
            semVer = srcompile(SEM_VER);
            semCriteria = srcompile(SEM_CRITERIA);
            semExpr = srcompile(SEM_EXPR);
            mprAddTerminator((MprTerminator) versionTerminate);
        }
        mprGlobalUnlock();
    }
}


static void versionTerminate()
{
    if (semVer) {
        free(semVer);
        semVer = 0;
    }
    if (semCriteria) {
        free(semCriteria);
        semCriteria = 0;
    }
    if (semExpr) {
        free(semExpr);
        semExpr = 0;
    }
}


PUBLIC cchar *mprBumpVersion(cchar *version)
{
    MprVersion  *vp;

    if ((vp = mprCreateVersion(version)) == 0) {
        return 0;
    }
    return numberToVersion(vp->numberVersion + 1);
}


PUBLIC cchar *mprGetCompatibleVersion(cchar *version)
{
    MprVersion  *vp;

    if ((vp = mprCreateVersion(version)) == 0) {
        return 0;
    }
    return sfmt("%d.%d", vp->major, vp->minor);
}


PUBLIC int mprGetMajorVersion(cchar *version)
{
    MprVersion  *vp;

    if ((vp = mprCreateVersion(version)) == 0) {
        return 0;
    }
    return vp->major;
}


PUBLIC int mprGetMinorVersion(cchar *version)
{
    MprVersion  *vp;

    if ((vp = mprCreateVersion(version)) == 0) {
        return 0;
    }
    return vp->minor;
}


PUBLIC int mprGetPatchVersion(cchar *version)
{
    MprVersion  *vp;

    if ((vp = mprCreateVersion(version)) == 0) {
        return 0;
    }
    return vp->patch;
}


PUBLIC cchar *mprGetPrereleaseVersion(cchar *version)
{
    MprVersion  *vp;

    if ((vp = mprCreateVersion(version)) == 0) {
        return 0;
    }
    return vp->preVersion ? vp->preVersion : 0;
}


PUBLIC cchar *mprGetVersionBase(cchar *version)
{
    MprVersion  *vp;

    if ((vp = mprCreateVersion(version)) == 0) {
        return 0;
    }
    return vp->baseVersion;
}


PUBLIC int64 mprGetVersionNumber(cchar *version)
{
    MprVersion  *vp;

    if ((vp = mprCreateVersion(version)) == 0) {
        return 0;
    }
    return vp->numberVersion;
}


/*
    Test if the version is acceptable based on the supplied critera.
    The acceptable formats for criteria are:

    VER                             Allows prereleases
    1.2.[*xX]                       Wild card version portion. (allows prereleases).
    ~VER                            Compatible with VER at the least significant level.
                                    ~1.2.3 == (>=1.2.3 <1.3.0) Compatible at the patch level
                                    ~1.2 == 1.2.x   Compatible at the minor level
                                    ~1 == 1.x   Compatible at the major level
    ^VER                            Compatible with VER at the most significant level.
                                    ^0.2.3 == 0.2.3 <= VER < 0.3.0
                                    ^1.2.3 == 1.2.3 <= VER < 2.0.0
    [>, >=, <, <=, ==, !=]VER       Create range relative to given version
    EXPR1 - EXPR2                   <=EXPR1 <=EXPR2
    EXPR1 || EXPR2 ...              EXPR1 OR EXPR2
    EXPR1 && EXPR2 ...              EXPR1 AND EXPR2
    EXPR1 EXPR2 ...                 EXPR1 AND EXPR2

    Pre-release versions will only match if the criteria contains a "-.*" prerelease suffix
*/
PUBLIC bool mprIsVersionObjAcceptable(MprVersion *vp, cchar *criteria)
{
    char        *expr, *exprTok, *range, *rangeTok, *low, *high;
    bool        allMatched;

    if (!vp->ok) {
        return 0;
    }
    if (!criteria || *criteria == '\0') {
        return 1;
    }
    criteria = cleanVersion(criteria);
    for (range = (char*) criteria; stok(range, "||", &rangeTok) != 0; range = rangeTok) {
        range = strim(range, " \t", 0);
        allMatched = 1;
        for (expr = (char*) range; sptok(expr, "&&", &exprTok) != 0; expr = exprTok) {
            if (scontains(expr, " - ")) {
                low = sptok(expr, " - ", &high);
                return inRange(vp, sjoin(">=", low, NULL)) && inRange(vp, sjoin("<=", high, NULL));
            }
            if (!inRange(vp, expr)) {
                allMatched = 0;
                break;
            }
        }
        if (allMatched) {
            return 1;
        }
    }
    return 0;
}


PUBLIC bool mprIsVersionAcceptable(cchar *version, cchar *criteria)
{
    MprVersion  *vp;

    if ((vp = mprCreateVersion(version)) == 0) {
        return 0;
    }
    return mprIsVersionObjAcceptable(vp, criteria);
}


PUBLIC bool mprIsVersionSame(cchar *version, cchar *other)
{
    MprVersion  *vp, *op;

    if ((vp = mprCreateVersion(version)) == 0 || !vp->ok) {
        return 0;
    }
    if ((op = mprCreateVersion(other)) == 0 || !op->ok) {
        return 0;
    }
    return smatch(vp->full, op->full) ? 1 : 0;
}


PUBLIC bool mprIsVersionValid(cchar *version)
{
    MprVersion  *vp;

    if ((vp = mprCreateVersion(version)) == 0) {
        return 0;
    }
    return vp->ok;
}


static int sortVersions(MprVersion **v1, MprVersion **v2)
{
    MprVersion  *a, *b;

    a = *v1;
    b = *v2;
    if (a->major < b->major) {
        return -1;
    } else if (a->major > b->major) {
        return 1;
    } else if (a->minor < b->minor) {
        return -1;
    } else if (a->minor > b->minor) {
        return 1;
    } else if (a->patch < b->patch) {
        return -1;
    } else if (a->patch > b->patch) {
        return 1;
    } else if (a->preVersion) {
        if (b->preVersion) {
            return strcmp(a->preVersion, b->preVersion);
        } else {
            return -1;
        }
    } else if (b->preVersion) {
        return 1;
    }
    return strcmp(a->full, b->full);
}


PUBLIC void mprSortVersions(MprVersion **versions, ssize nelt, int direction)
{
    MprVersion  *vtmp;
    ssize       i, j;

    mprSort(versions, nelt, sizeof(MprVersion*), (MprSortProc) sortVersions, 0);
    if (direction < 0) {
        if (nelt > 1) {
            i = (nelt - 2) / 2;
            j = (nelt + 1) / 2;
            for (; i >= 0; i--, j++) {
                vtmp = versions[i];
                versions[i] = versions[j];
                versions[j] = vtmp;
            }
        }
    }
}


static bool inRange(MprVersion *vp, cchar *expr)
{
    char    *cp, *ver, *op, *base, *pre, *all;
    cchar   *high, *low;
    uint64  factor, min, max, num;

    while (isspace((uchar) *expr)) expr++;

    if (srmatch(expr, semExpr, &all, &op, &ver, NULL) <= 0) {
        mprLog("error", 5, "Bad version expression: %s", expr);
        return 0;
    }
    if (smatch(op, "~")) {
        /*
            ~VER   Compatible with VER at the level of specificity given.
                   ~1.2.3 == (>=1.2.3 <1.3.0) Compatible at the patch level
                   ~1.2 == 1.2.x   Compatible at the minor level
                   ~1 == 1.x   Compatible at the major level
         */
        if (partCount(ver) == 3 && (cp = srchr(ver, '.')) != 0) {
            high = sjoin(snclone(ver, cp - ver), ".", MAX_VER_STR, NULL);
            if ((cp = schr(ver, '-')) != 0) {
                high = sjoin(high, cp, NULL);
            }
            return inRange(vp, sjoin(">=", ver, NULL)) &&
                   inRange(vp, sjoin("<", high, NULL));
        }
        return inRange(vp, completeVersion(ver, "x"));
    } 
    if (smatch(op, "^")) {
        /*
            ^VER  Compatible with VER at the most significant level.
                  ^0.2.3 == 0.2.3 <= VER < 0.3.0
                  ^1.2.3 == 1.2.3 <= VER < 2.0.0
            So convert to a range
         */
        high = ver;
        for (cp = ver, factor = VER_FACTOR * VER_FACTOR; *cp; cp++) {
            if (*cp == '.') {
                factor /= VER_FACTOR;
            } else if (isdigit((uchar) *cp) && *cp != '0') {
                num = (stoi(cp) + 1) * factor;
                high = numberToVersion(num);
                if ((cp = schr(ver, '-')) != 0) {
                    high = sjoin(high, cp, NULL);
                }
                break;
            }
        }
        return inRange(vp, sjoin(">=", ver, NULL)) && inRange(vp, sjoin("<", high, NULL));
    }
    ver = completeVersion(ver, "x");
    if (srmatch(ver, semCriteria, &all, &base, &pre, NULL) <= 0) {
        mprLog("error", 5, "Cannot match version %s", ver);
        return 0;
    }
    if (vp->preVersion) {
        if (!pre) {
            return 0;
        }
        if (snumber(vp->preVersion)) {
            if (stoi(pre) < stoi(vp->preVersion)) {
                return 0;
            }
        } else {
            if (scmp(pre, vp->preVersion) < 0 && !smatch(pre, "-")) {
                return 0;
            }
        }
    }
    min = 0;
    max = MAX_VER * VER_FACTOR * VER_FACTOR;
    if (schr(ver, 'x')) {
        if (smatch(op, ">=")) {
            // 1.2.3 >= 2.x.x ... 1.2.3 >= 2.0.0
            low = sreplace(ver, "x", "0");
            min = versionToNumber(low);
        } else if (smatch(op, "<=")) {
            // 1.2.3 < 2.x.x ... 1.2.3 <2.MAX.MAX
            high = sreplace(ver, "x", MAX_VER_STR);
            max = versionToNumber(high);
        } else if (*op == '>') {
            // 1.2.3 > 2.x.x ... 1.2.3 > 2.0.0
            low = sreplace(ver, "x", "0");
            min = versionToNumber(low) + 1;
        } else if (*op == '<') {
            // 1.2.3 < 2.x.x ... 1.2.3 <2.MAX.MAX
            high = sreplace(ver, "x", MAX_VER_STR);
            max = versionToNumber(high) - 1;
        } else {
            low = sreplace(ver, "x", "0");
            high = sreplace(ver, "x", MAX_VER_STR);
            return inRange(vp, sjoin(">=", low, NULL)) && inRange(vp, sjoin("<", high, NULL));
        }
    } else if (smatch(op, ">=")) {
        min = versionToNumber(base);
    } else if (smatch(op, "<=")) {
        max = versionToNumber(base);
    } else if (*op == '>') {
        min = versionToNumber(base) + 1;
    } else if (*op == '<') {
        max = versionToNumber(base) - 1;
    } else {
        min = max = versionToNumber(base);
    }
    if (min <= vp->numberVersion && vp->numberVersion <= max) {
        return 1;
    }
    return 0;
}


static int64 versionToNumber(cchar *version)
{
    int     major, minor, patch;

    major = (int) stoi(version);
    while (*version++ != '.') {}
    minor = (int) stoi(version);
    while (*version++ != '.') {}
    patch = (int) stoi(version);
    return ((major * VER_FACTOR) + minor) * VER_FACTOR + patch;
}


static cchar *numberToVersion(uint64 num)
{
    int     major, minor, patch;

    patch = (int) num % VER_FACTOR;
    num /= VER_FACTOR;
    minor = (int) num % VER_FACTOR;
    num /= VER_FACTOR;
    major = (int) num % VER_FACTOR;
    return sfmt("%d.%d.%d", major, minor, patch);
}


static char *cleanVersion(cchar *version) 
{
    char    *cp;

    cp = strim(version, " \tv=", 0);
    if (schr(cp, 'X')) {
        cp = sreplace(cp, "X", "x");
    }
    if (schr(cp, '*')) {
        cp = sreplace(cp, "*", "x");
    }
    return cp;
}


static int partCount(cchar *version)
{
    cchar   *cp;
    int     count;

    for (cp = version, count = 1; *cp; cp++) {
        if (*cp == '.') {
            count++;
        }
    }
    return count;
}


/*
    Make the version a full three version numbers by adding ".0"
 */
static char *completeVersion(cchar *version, cchar *fill)
{
    char    *pre, *result;
    int     count;

    if (!version || smatch(version, "*")) {
        version = fill;
    }
    count = partCount(version);
    result = sclone(version);
    if ((pre = schr(result, '-')) != 0) {
        *pre++ = '\0';
    }
    while (count++ < 3) {
        result = sjoin(result, ".", fill, NULL);
    }
    if (pre) {
        result = sjoin(result, "-", pre, NULL);
    }
    return result;
}


static void *srcompile(cchar *pattern)
{
    pcre    *pp;
    cchar   *err;
    int     column, options;

    options = PCRE_JAVASCRIPT_COMPAT;
    if ((pp = pcre_compile2(pattern, options, 0, &err, &column, NULL)) == 0) {
        mprLog("error version", 0, "Cannot compile pattern %s. Error %s at column %d", pattern, err, column);
        return 0;
    }
    return pp;
}


static int srmatch(cchar *s, void *pattern, ...)
{
    va_list     ap;
    char        **str;
    ssize       len;
    int         count, i, index, matches[64 * 2];

    va_start(ap, pattern);
    count = pcre_exec(pattern, NULL, s, (int) slen(s), 0, 0, matches, sizeof(matches) / sizeof(int));
    for (i = 0, str = 0; i < count; i++) {
        str = va_arg(ap, char**);
        if (str == NULL) {
            break;
        }
        index = i * 2;
        len = matches[index + 1] - matches[index];
        *str = snclone(&s[matches[index]], len);
    }
    if (str) {
        while ((str = va_arg(ap, char**)) != 0) {
            *str = 0;
        }
    }
    va_end(ap);
    return count / 2;
}


/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
