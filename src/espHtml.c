/*
    espHtml.c -- ESP HTML controls 

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "esp.h"
#include    "edi.h"

/************************************* Local **********************************/

static cchar *getValue(HttpConn *conn, cchar *fieldName, MprHash *options);
static cchar *map(HttpConn *conn, MprHash *options);

/************************************* Code ***********************************/

PUBLIC void input(cchar *field, cchar *optionString)
{
    HttpConn    *conn;
    MprHash     *choices, *options;
    MprKey      *kp;
    EdiRec      *rec;
    cchar       *rows, *cols, *etype, *value, *checked, *style, *error, *errorMsg;
    int         type, flags;

    conn = getConn();
    rec = conn->record;
    if (ediGetColumnSchema(rec->edi, rec->tableName, field, &type, &flags, NULL) < 0) {
        type = -1;
    }
    options = httpGetOptions(optionString);
    style = httpGetOption(options, "class", "");
    errorMsg = rec->errors ? mprLookupKey(rec->errors, field) : 0;
    error = errorMsg ? sfmt("<span class=\"field-error\">%s</span>", errorMsg) : ""; 

    switch (type) {
    case EDI_TYPE_BOOL:
        choices = httpGetOptions("{off: 0, on: 1}");
        value = getValue(conn, field, options);
        for (kp = 0; (kp = mprGetNextKey(choices, kp)) != 0; ) {
            checked = (smatch(kp->data, value)) ? " checked" : "";
            espRender(conn, "%s <input type='radio' name='%s' value='%s'%s%s class='%s'/>\r\n",
                stitle(kp->key), field, kp->data, checked, map(conn, options), style);
        }
        break;
        /* Fall through */
    case EDI_TYPE_BINARY:
    default:
        httpError(conn, 0, "espInput: unknown field type %d", type);
        /* Fall through */
    case EDI_TYPE_FLOAT:
    case EDI_TYPE_TEXT:

    case EDI_TYPE_INT:
    case EDI_TYPE_DATE:
    case EDI_TYPE_STRING:        
        if (type == EDI_TYPE_TEXT && !httpGetOption(options, "rows", 0)) {
            httpSetOption(options, "rows", "10");
        }
        etype = "text";
        value = getValue(conn, field, options);
        if (value == 0 || *value == '\0') {
            value = espGetParam(conn, field, "");
        }
        if (httpGetOption(options, "password", 0)) {
            etype = "password";
        } else if (httpGetOption(options, "hidden", 0)) {
            etype = "hidden";
        }
        if ((rows = httpGetOption(options, "rows", 0)) != 0) {
            cols = httpGetOption(options, "cols", "60");
            espRender(conn, "<textarea name='%s' type='%s' cols='%s' rows='%s'%s class='%s'>%s</textarea>", 
                field, etype, cols, rows, map(conn, options), style, value);
        } else {
            espRender(conn, "<input name='%s' type='%s' value='%s'%s class='%s'/>", field, etype, value, 
                map(conn, options), style);
        }
        if (error) {
            espRenderString(conn, error);
        }
        break;
    }
}


/*
    Render an input field with a hidden security token
    Used by esp-html-mvc to add XSRF tokens to a form
 */
PUBLIC void inputSecurityToken()
{
    HttpConn    *conn;

    conn = getConn();
    espRender(conn, "    <input name='%s' type='hidden' value='%s' />\r\n", ME_XSRF_PARAM, httpGetSecurityToken(conn, 0));
}


/**************************************** Support *************************************/ 

static cchar *getValue(HttpConn *conn, cchar *fieldName, MprHash *options)
{
    EdiRec      *record;
    cchar       *value;

    record = conn->record;
    value = 0;
    if (record) {
        value = ediGetFieldValue(record, fieldName);
    }
    if (value == 0) {
        value = httpGetOption(options, "value", 0);
    }
    if (!httpGetOption(options, "noescape", 0)) {
        value = mprEscapeHtml(value);
    }
    return value;
}


/*
    Map options to an attribute string.
 */
static cchar *map(HttpConn *conn, MprHash *options)
{
    MprKey      *kp;
    MprBuf      *buf;

    if (options == 0 || mprGetHashLength(options) == 0) {
        return MPR->emptyString;
    }
    buf = mprCreateBuf(-1, -1);
    for (kp = 0; (kp = mprGetNextKey(options, kp)) != 0; ) {
        if (kp->type != MPR_JSON_OBJ && kp->type != MPR_JSON_ARRAY) {
            mprPutCharToBuf(buf, ' ');
            mprPutStringToBuf(buf, kp->key);
            mprPutStringToBuf(buf, "='");
            mprPutStringToBuf(buf, kp->data);
            mprPutCharToBuf(buf, '\'');
        }
    }
    mprAddNullToBuf(buf);
    return mprGetBufStart(buf);
}

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */
