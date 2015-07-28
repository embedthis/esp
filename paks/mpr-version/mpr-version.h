/*
    mpr-version.h -- Header for the Embedthis Version Library

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

#ifndef _h_VERSION
#define _h_VERSION 1

/********************************* Includes ***********************************/

#include    "mpr.h"

#ifdef __cplusplus
extern "C" {
#endif

/********************************** Defines ***********************************/
/**
    Version object
    @description The Version class provides the ability to manage SemVer version numbers
    @defgroup Version Version
    @stability Prototype
 */
typedef struct MprVersion {
    cchar   *full;              /**< Original version string */
    cchar   *baseVersion;       /**< Base version portion without pre-release suffix */
    cchar   *preVersion;        /**< Pre-release version suffix */
    uint64  numberVersion;      /**< Version string as a number (excludes prerelease suffix) */
    int     major;
    int     minor;
    int     patch;
    bool    ok;                 /**< Version is a valid a SemVer */
} MprVersion;


PUBLIC cchar *mprBumpVersion(cchar *version);
MprVersion *mprCreateVersion(cchar *version);
PUBLIC cchar *mprGetCompatibleVersion(cchar *version);
PUBLIC int mprGetMajorVersion(cchar *version);
PUBLIC int mprGetMinorVersion(cchar *version);
PUBLIC int mprGetPatchVersion(cchar *version);
PUBLIC cchar *mprGetPrereleaseVersion(cchar *version);
PUBLIC cchar *mprGetVersionBase(cchar *version);
PUBLIC int64 mprGetVersionNumber(cchar *version);
PUBLIC bool mprIsVersionAcceptable(cchar *version, cchar *criteria);
PUBLIC bool mprIsVersionSame(cchar *version, cchar *other);
PUBLIC bool mprIsVersionValid(cchar *version);
PUBLIC void mprSortVersions(MprVersion **versions, ssize nelt, int direction);

PUBLIC bool mprIsVersionObjAcceptable(MprVersion *vp, cchar *criteria);

#ifdef __cplusplus
} /* extern C */
#endif

#endif /* _h_VERSION */

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
