/*
    espHtml.c -- ESP HTML controls 

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "esp.h"
#include    "edi.h"

/************************************* Local **********************************/

static cchar *getValue(HttpStream *stream, cchar *fieldName, MprHash *options);
static cchar *map(HttpStream *stream, MprHash *options);

/************************************* Code ***********************************/

PUBLIC void input(cchar *field, cchar *optionString)
{
    HttpStream    *stream;
    MprHash     *choices, *options;
    MprKey      *kp;
    EdiRec      *rec;
    cchar       *rows, *cols, *etype, *value, *checked, *style, *error, *errorMsg;
    int         type, flags;

    stream = getStream();
    rec = stream->record;
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
        value = getValue(stream, field, options);
        for (kp = 0; (kp = mprGetNextKey(choices, kp)) != 0; ) {
            checked = (smatch(kp->data, value)) ? " checked" : "";
            espRender(stream, "%s <input type='radio' name='%s' value='%s'%s%s class='%s'/>\r\n",
                stitle(kp->key), field, kp->data, checked, map(stream, options), style);
        }
        break;
        /* Fall through */
    case EDI_TYPE_BINARY:
    default:
        httpError(stream, 0, "espInput: unknown field type %d", type);
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
        value = getValue(stream, field, options);
        if (value == 0 || *value == '\0') {
            value = espGetParam(stream, field, "");
        }
        if (httpGetOption(options, "password", 0)) {
            etype = "password";
        } else if (httpGetOption(options, "hidden", 0)) {
            etype = "hidden";
        }
        if ((rows = httpGetOption(options, "rows", 0)) != 0) {
            cols = httpGetOption(options, "cols", "60");
            espRender(stream, "<textarea name='%s' type='%s' cols='%s' rows='%s'%s class='%s'>%s</textarea>", 
                field, etype, cols, rows, map(stream, options), style, value);
        } else {
            espRender(stream, "<input name='%s' type='%s' value='%s'%s class='%s'/>", field, etype, value, 
                map(stream, options), style);
        }
        if (error) {
            espRenderString(stream, error);
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
    HttpStream    *stream;

    stream = getStream();
    espRender(stream, "    <input name='%s' type='hidden' value='%s' />\r\n", ME_XSRF_PARAM, httpGetSecurityToken(stream, 0));
}


/**************************************** Support *************************************/ 

static cchar *getValue(HttpStream *stream, cchar *fieldName, MprHash *options)
{
    EdiRec      *record;
    cchar       *value;

    record = stream->record;
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
static cchar *map(HttpStream *stream, MprHash *options)
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
    This software is distributed under a commercial license. Consult the LICENSE.md
    distributed with this software for full details and copyrights.
 */
