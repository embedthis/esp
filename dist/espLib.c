/*
 * Embedthis ESP Library Source
 */
#include "esp.h"

#if ME_COM_ESP

/*
    edi.c -- Embedded Database Interface (EDI)

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/


#include    "pcre.h"

/************************************* Local **********************************/

static void addValidations();
static void formatFieldForJson(MprBuf *buf, EdiField *fp);
static void manageEdiService(EdiService *es, int flags);
static void manageEdiGrid(EdiGrid *grid, int flags);
static bool validateField(Edi *edi, EdiRec *rec, cchar *columnName, cchar *value);

/************************************* Code ***********************************/

PUBLIC EdiService *ediCreateService()
{
    EdiService      *es;

    if ((es = mprAllocObj(EdiService, manageEdiService)) == 0) {
        return 0;
    }
    MPR->ediService = es;
    es->providers = mprCreateHash(0, MPR_HASH_STATIC_VALUES | MPR_HASH_STABLE);
    addValidations();
    return es;
}


static void manageEdiService(EdiService *es, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(es->providers);
        mprMark(es->validations);
    }
}


PUBLIC int ediAddColumn(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags)
{
    mprRemoveKey(edi->schemaCache, tableName);
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    return edi->provider->addColumn(edi, tableName, columnName, type, flags);
}


PUBLIC int ediAddIndex(Edi *edi, cchar *tableName, cchar *columnName, cchar *indexName)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    return edi->provider->addIndex(edi, tableName, columnName, indexName);
}


PUBLIC void ediAddProvider(EdiProvider *provider)
{
    EdiService  *es;

    es = MPR->ediService;
    mprAddKey(es->providers, provider->name, provider);
}


static EdiProvider *lookupProvider(cchar *providerName)
{
    EdiService  *es;

    es = MPR->ediService;
    return mprLookupKey(es->providers, providerName);
}


PUBLIC int ediAddTable(Edi *edi, cchar *tableName)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    return edi->provider->addTable(edi, tableName);
}


static void manageValidation(EdiValidation *vp, int flags) 
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(vp->name);
        mprMark(vp->data);
    }
}


PUBLIC int ediAddValidation(Edi *edi, cchar *name, cchar *tableName, cchar *columnName, cvoid *data)
{
    EdiService          *es;
    EdiValidation       *prior, *vp; 
    MprList             *validations;
    cchar               *errMsg, *vkey;
    int                 column, next;

    //  FUTURE - should not be attached to "es". Should be unique per database.
    es = MPR->ediService;
    if ((vp = mprAllocObj(EdiValidation, manageValidation)) == 0) {
        return MPR_ERR_MEMORY;
    }
    vp->name = sclone(name);
    if ((vp->vfn = mprLookupKey(es->validations, name)) == 0) {
        mprLog("error esp edi", 0, "Cannot find validation '%s'", name);
        return MPR_ERR_CANT_FIND;
    }
    if (smatch(name, "format") || smatch(name, "banned")) {
        if (!data || ((char*) data)[0] == '\0') {
            mprLog("error esp edi", 0, "Bad validation format pattern for %s", name);
            return MPR_ERR_BAD_SYNTAX;
        }
        if ((vp->mdata = pcre_compile2(data, 0, 0, &errMsg, &column, NULL)) == 0) {
            mprLog("error esp edi", 0, "Cannot compile validation pattern. Error %s at column %d", errMsg, column); 
            return MPR_ERR_BAD_SYNTAX;
        }
        data = 0;
    }
    vp->data = data;
    vkey = sfmt("%s.%s", tableName, columnName);

    lock(edi);
    if ((validations = mprLookupKey(edi->validations, vkey)) == 0) {
        validations = mprCreateList(0, MPR_LIST_STABLE);
        mprAddKey(edi->validations, vkey, validations);
    }
    for (ITERATE_ITEMS(validations, prior, next)) {
        if (prior->vfn == vp->vfn) {
            break;
        }
    }
    if (!prior) {
        mprAddItem(validations, vp);
    }
    unlock(edi);
    return 0;
}


static bool validateField(Edi *edi, EdiRec *rec, cchar *columnName, cchar *value)
{
    EdiValidation   *vp;
    MprList         *validations;
    cchar           *error, *vkey;
    int             next;
    bool            pass;

    assert(edi);
    assert(rec);
    assert(columnName && *columnName);

    pass = 1;
    vkey = sfmt("%s.%s", rec->tableName, columnName);
    if ((validations = mprLookupKey(edi->validations, vkey)) != 0) {
        for (ITERATE_ITEMS(validations, vp, next)) {
            if ((error = (*vp->vfn)(vp, rec, columnName, value)) != 0) {
                if (rec->errors == 0) {
                    rec->errors = mprCreateHash(0, MPR_HASH_STABLE);
                }
                mprAddKey(rec->errors, columnName, sfmt("%s %s", columnName, error));
                pass = 0;
            }
        }
    }
    return pass;
}


PUBLIC int ediChangeColumn(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    mprRemoveKey(edi->schemaCache, tableName);
    return edi->provider->changeColumn(edi, tableName, columnName, type, flags);
}


PUBLIC void ediClose(Edi *edi)
{
    if (!edi || !edi->provider) {
        return;
    }
    edi->provider->close(edi);
}


PUBLIC EdiRec *ediCreateRec(Edi *edi, cchar *tableName)
{
    if (!edi || !edi->provider) {
        return 0;
    }
    return edi->provider->createRec(edi, tableName);
}


PUBLIC int ediDelete(Edi *edi, cchar *path)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    return edi->provider->deleteDatabase(path);
}


//  FUTURE - rename edi
PUBLIC void espDumpGrid(EdiGrid *grid)
{
    mprLog("info esp edi", 0, "Grid: %s\nschema: %s,\ndata: %s", grid->tableName, 
        ediGetTableSchemaAsJson(grid->edi, grid->tableName), ediGridAsJson(grid, MPR_JSON_PRETTY));
}


PUBLIC EdiGrid *ediFilterGridFields(EdiGrid *grid, cchar *fields, int include)
{
    EdiRec      *first, *rec;
    MprList     *fieldList;
    int         f, r, inlist;

    if (!grid || grid->nrecords == 0) {
        return grid;
    }
    first = grid->records[0];
    fieldList = mprCreateListFromWords(fields);

    /* Process list of fields to remove from the grid */
    for (f = 0; f < first->nfields; f++) {
        inlist = mprLookupStringItem(fieldList, first->fields[f].name) >= 0;
        if ((inlist && !include) || (!inlist && include)) {
            for (r = 0; r < grid->nrecords; r++) {
                rec = grid->records[r];
                memmove(&rec->fields[f], &rec->fields[f+1], (rec->nfields - f - 1) * sizeof(EdiField));
                rec->nfields--;
                /* Ensure never saved to database */
                rec->id = 0;
            }
            f--;
        }
    }
    return grid;
}


PUBLIC EdiRec *ediFilterRecFields(EdiRec *rec, cchar *fields, int include)
{
    EdiField    *fp;
    MprList     *fieldList;
    int         inlist;

    if (rec == 0 || rec->nfields == 0) {
        return rec;
    }
    fieldList = mprCreateListFromWords(fields);

    for (fp = rec->fields; fp < &rec->fields[rec->nfields]; fp++) {
        inlist = mprLookupStringItem(fieldList, fp->name) >= 0;
        if ((inlist && !include) || (!inlist && include)) {
            fp[0] = fp[1];
            rec->nfields--;
            fp--;
        }
    }
    /*
        Ensure never saved to database
     */
    rec->id = 0;
    return rec;
}


PUBLIC MprList *ediGetColumns(Edi *edi, cchar *tableName)
{
    if (!edi || !edi->provider) {
        return 0;
    }
    return edi->provider->getColumns(edi, tableName);
}


PUBLIC int ediGetColumnSchema(Edi *edi, cchar *tableName, cchar *columnName, int *type, int *flags, int *cid)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    return edi->provider->getColumnSchema(edi, tableName, columnName, type, flags, cid);
}


PUBLIC EdiField *ediGetNextField(EdiRec *rec, EdiField *fp, int offset)
{
    if (rec == 0 || rec->nfields <= 0) {
        return 0;
    }
    if (fp == 0) {
        if (offset >= rec->nfields) {
            return 0;
        }
        fp = &rec->fields[offset];
    } else {
        if (++fp >= &rec->fields[rec->nfields]) {
            fp = 0;  
        }
    }
    return fp;
}


PUBLIC EdiRec *ediGetNextRec(EdiGrid *grid, EdiRec *rec)
{
    int     index;

    if (grid == 0 || grid->nrecords <= 0) {
        return 0;
    }
    if (rec == 0) {
        rec = grid->records[0];
        rec->index = 0;
    } else {
        index = rec->index + 1;
        if (index >= grid->nrecords) {
            rec = 0;  
        } else {
            rec = grid->records[index];
            rec->index = index;
        }
    }
    return rec;
}


PUBLIC cchar *ediGetTableSchemaAsJson(Edi *edi, cchar *tableName)
{
    MprBuf      *buf;
    MprList     *columns;
    cchar       *schema, *s;
    int         c, type, flags, cid, ncols, next;

    if (tableName == 0 || *tableName == '\0') {
        return 0;
    }
    if ((schema = mprLookupKey(edi->schemaCache, tableName)) != 0) {
        return schema;
    }
    buf = mprCreateBuf(0, 0);
    ediGetTableDimensions(edi, tableName, NULL, &ncols);
    columns = ediGetColumns(edi, tableName);
    mprPutStringToBuf(buf, "{\n    \"types\": {\n");
    for (c = 0; c < ncols; c++) {
        ediGetColumnSchema(edi, tableName, mprGetItem(columns, c), &type, &flags, &cid);
        mprPutToBuf(buf, "      \"%s\": {\n        \"type\": \"%s\"\n      },\n", 
            (char*) mprGetItem(columns, c), ediGetTypeString(type));
    }
    if (ncols > 0) {
        mprAdjustBufEnd(buf, -2);
    }
    mprRemoveItemAtPos(columns, 0);
    mprPutStringToBuf(buf, "\n    },\n    \"columns\": [ ");
    for (ITERATE_ITEMS(columns, s, next)) {
        mprPutToBuf(buf, "\"%s\", ", s);
    }
    if (columns->length > 0) {
        mprAdjustBufEnd(buf, -2);
    }
    mprPutStringToBuf(buf, " ]\n  }");
    mprAddNullToBuf(buf);
    schema = mprGetBufStart(buf);
    mprAddKey(edi->schemaCache, tableName, schema);
    return schema;
}


PUBLIC cchar *ediGetGridSchemaAsJson(EdiGrid *grid)
{
    if (grid) {
        return ediGetTableSchemaAsJson(grid->edi, grid->tableName);
    }
    return 0;
}


PUBLIC cchar *ediGetRecSchemaAsJson(EdiRec *rec)
{
    if (rec) {
        return ediGetTableSchemaAsJson(rec->edi, rec->tableName);
    }
    return 0;
}


PUBLIC MprHash *ediGetRecErrors(EdiRec *rec)
{
    return rec->errors;
}


PUBLIC MprList *ediGetGridColumns(EdiGrid *grid)
{
    MprList     *cols;
    EdiRec      *rec;
    EdiField    *fp;

    cols = mprCreateList(0, 0);
    rec = grid->records[0];
    for (fp = rec->fields; fp < &rec->fields[rec->nfields]; fp++) {
        mprAddItem(cols, fp->name);
    }
    return cols;
}


PUBLIC EdiField *ediGetField(EdiRec *rec, cchar *fieldName)
{
    EdiField    *fp;

    for (fp = rec->fields; fp < &rec->fields[rec->nfields]; fp++) {
        if (smatch(fp->name, fieldName)) {
            return fp;
        }
    }
    return 0;
}


PUBLIC cchar *ediGetFieldValue(EdiRec *rec, cchar *fieldName)
{
    EdiField    *fp;

    if (rec == 0) {
        return 0;
    }
    for (fp = rec->fields; fp < &rec->fields[rec->nfields]; fp++) {
        if (smatch(fp->name, fieldName)) {
            return fp->value;
        }
    }
    return 0;
}


PUBLIC int ediGetFieldType(EdiRec *rec, cchar *fieldName)
{
    int     type;
    
    if (ediGetColumnSchema(rec->edi, rec->tableName, fieldName, &type, NULL, NULL) < 0) {
        return 0;
    }
    return type;
}


PUBLIC MprList *ediGetTables(Edi *edi)
{
    if (!edi || !edi->provider) {
        return 0;
    }
    return edi->provider->getTables(edi);
}


PUBLIC int ediGetTableDimensions(Edi *edi, cchar *tableName, int *numRows, int *numCols)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    return edi->provider->getTableDimensions(edi, tableName, numRows, numCols);
}


PUBLIC char *ediGetTypeString(int type)
{
    switch (type) {
    case EDI_TYPE_BINARY:
        return "binary";
    case EDI_TYPE_BOOL:
        return "bool";
    case EDI_TYPE_DATE:
        return "date";
    case EDI_TYPE_FLOAT:
        return "float";
    case EDI_TYPE_INT:
        return "int";
    case EDI_TYPE_STRING:
        return "string";
    case EDI_TYPE_TEXT:
        return "text";
    }
    return 0;
}


PUBLIC cchar *ediGridAsJson(EdiGrid *grid, int flags)
{
    EdiRec      *rec;
    EdiField    *fp;
    MprBuf      *buf;
    bool        pretty;
    int         r, f;

    pretty = flags & MPR_JSON_PRETTY;
    buf = mprCreateBuf(0, 0);
    mprPutStringToBuf(buf, "[");
    if (grid) {
        if (pretty) mprPutCharToBuf(buf, '\n');
        for (r = 0; r < grid->nrecords; r++) {
            if (pretty) mprPutStringToBuf(buf, "    ");
            mprPutStringToBuf(buf, "{");
            rec = grid->records[r];
            for (f = 0; f < rec->nfields; f++) {
                fp = &rec->fields[f];
                mprFormatJsonName(buf, fp->name, MPR_JSON_QUOTES);
                if (pretty) {
                    mprPutStringToBuf(buf, ": ");
                } else {
                    mprPutCharToBuf(buf, ':');
                }
                formatFieldForJson(buf, fp);
                if ((f+1) < rec->nfields) {
                    mprPutStringToBuf(buf, ",");
                }
            }
            mprPutStringToBuf(buf, "}");
            if ((r+1) < grid->nrecords) {
                mprPutCharToBuf(buf, ',');
            }
            if (pretty) mprPutCharToBuf(buf, '\n');
        }
    }
    mprPutStringToBuf(buf, "]");
    if (pretty) mprPutCharToBuf(buf, '\n');
    mprAddNullToBuf(buf);
    return mprGetBufStart(buf);
}


PUBLIC int ediLoad(Edi *edi, cchar *path)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    return edi->provider->load(edi, path);
}


PUBLIC int ediLookupField(Edi *edi, cchar *tableName, cchar *fieldName)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    return edi->provider->lookupField(edi, tableName, fieldName);
}


PUBLIC EdiProvider *ediLookupProvider(cchar *providerName)
{
    return lookupProvider(providerName);
}


PUBLIC Edi *ediOpen(cchar *path, cchar *providerName, int flags)
{
    EdiProvider     *provider;

    if ((provider = lookupProvider(providerName)) == 0) {
        mprLog("error esp edi", 0, "Cannot find EDI provider '%s'", providerName);
        return 0;
    }
    return provider->open(path, flags);
}


PUBLIC Edi *ediClone(Edi *edi)
{
    Edi     *cloned;

    if (!edi || !edi->provider) {
        return 0;
    }
    if ((cloned = edi->provider->open(edi->path, edi->flags)) != 0) {
        cloned->validations = edi->validations;
    }
    return cloned;
}


PUBLIC EdiGrid *ediQuery(Edi *edi, cchar *cmd, int argc, cchar **argv, va_list vargs)
{
    if (!edi || !edi->provider) {
        return 0;
    }
    return edi->provider->query(edi, cmd, argc, argv, vargs);
}


PUBLIC cchar *ediReadFieldValue(Edi *edi, cchar *fmt, cchar *tableName, cchar *key, cchar *columnName, cchar *defaultValue)
{
    EdiField    field;

    field = ediReadField(edi, tableName, key, columnName);
    if (!field.valid) {
        return defaultValue;
    }
    return field.value;
}


PUBLIC EdiRec *ediReadRecWhere(Edi *edi, cchar *tableName, cchar *fieldName, cchar *operation, cchar *value)
{
    EdiGrid *grid;
    
    /* OPT - slow to read entire table. Need optimized query in providers */
    if ((grid = ediReadWhere(edi, tableName, fieldName, operation, value)) == 0) {
        return 0;
    }
    if (grid->nrecords > 0) {
        return grid->records[0];
    }
    return 0;
}


PUBLIC EdiField ediReadField(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName)
{
    if (!edi || !edi->provider) {
        EdiField    field;
        memset(&field, 0, sizeof(EdiField));
        return field;
    }
    return edi->provider->readField(edi, tableName, key, fieldName);
}


PUBLIC EdiRec *ediReadRec(Edi *edi, cchar *tableName, cchar *key)
{
    if (!edi || !edi->provider) {
        return 0;
    }
    return edi->provider->readRec(edi, tableName, key);
}


PUBLIC EdiGrid *ediReadWhere(Edi *edi, cchar *tableName, cchar *fieldName, cchar *operation, cchar *value)
{
    if (!edi || !edi->provider) {
        return 0;
    }
    return edi->provider->readWhere(edi, tableName, fieldName, operation, value);
}


PUBLIC EdiGrid *ediReadTable(Edi *edi, cchar *tableName)
{
    if (!edi || !edi->provider) {
        return 0;
    }
    return edi->provider->readWhere(edi, tableName, 0, 0, 0);
}


PUBLIC cchar *ediRecAsJson(EdiRec *rec, int flags)
{
    MprBuf      *buf;
    EdiField    *fp;
    bool        pretty;
    int         f;

    pretty = flags & MPR_JSON_PRETTY;
    buf = mprCreateBuf(0, 0);
    mprPutStringToBuf(buf, "{ ");
    if (rec) {
        for (f = 0; f < rec->nfields; f++) {
            fp = &rec->fields[f];
            mprFormatJsonName(buf, fp->name, MPR_JSON_QUOTES);
            if (pretty) {
                mprPutStringToBuf(buf, ": ");
            } else {
                mprPutCharToBuf(buf, ':');
            }
            formatFieldForJson(buf, fp);
            if ((f+1) < rec->nfields) {
                mprPutStringToBuf(buf, ",");
            }
        }
    }
    mprPutStringToBuf(buf, "}");
    if (pretty) mprPutCharToBuf(buf, '\n');
    mprAddNullToBuf(buf);
    return mprGetBufStart(buf);;
}


PUBLIC int edRemoveColumn(Edi *edi, cchar *tableName, cchar *columnName)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    mprRemoveKey(edi->schemaCache, tableName);
    return edi->provider->removeColumn(edi, tableName, columnName);
}


PUBLIC int ediRemoveIndex(Edi *edi, cchar *tableName, cchar *indexName)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    return edi->provider->removeIndex(edi, tableName, indexName);
}


PUBLIC int ediRemoveRec(Edi *edi, cchar *tableName, cchar *key)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    return edi->provider->removeRec(edi, tableName, key);
}


PUBLIC int ediRemoveTable(Edi *edi, cchar *tableName)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    return edi->provider->removeTable(edi, tableName);
}


PUBLIC int ediRenameTable(Edi *edi, cchar *tableName, cchar *newTableName)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    mprRemoveKey(edi->schemaCache, tableName);
    return edi->provider->renameTable(edi, tableName, newTableName);
}


PUBLIC int ediRenameColumn(Edi *edi, cchar *tableName, cchar *columnName, cchar *newColumnName)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    mprRemoveKey(edi->schemaCache, tableName);
    return edi->provider->renameColumn(edi, tableName, columnName, newColumnName);
}


PUBLIC int ediSave(Edi *edi)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    if (edi->flags & EDI_PRIVATE) {
        /* Skip saving for in-memory private databases */
        return 0;
    }
    return edi->provider->save(edi);
}


PUBLIC int ediUpdateField(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName, cchar *value)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    return edi->provider->updateField(edi, tableName, key, fieldName, value);
}


PUBLIC int ediUpdateRec(Edi *edi, EdiRec *rec)
{
    if (!edi || !edi->provider) {
        return MPR_ERR_BAD_STATE;
    }
    return edi->provider->updateRec(edi, rec);
}


PUBLIC bool ediValidateRec(EdiRec *rec)
{
    EdiField    *fp;
    bool        pass;
    int         c;

    assert(rec->edi);
    if (rec == 0 || rec->edi == 0) {
        return 0;
    }
    pass = 1;
    for (c = 0; c < rec->nfields; c++) {
        fp = &rec->fields[c];
        if (!validateField(rec->edi, rec, fp->name, fp->value)) {
            pass = 0;
            /* Keep going */
        }
    }
    return pass;
}


/********************************* Convenience *****************************/

/*
    Create a free-standing grid. Not saved to the database
    The edi and tableName parameters can be null
 */
PUBLIC EdiGrid *ediCreateBareGrid(Edi *edi, cchar *tableName, int nrows)
{
    EdiGrid  *grid;

    if ((grid = mprAllocBlock(sizeof(EdiGrid) + sizeof(EdiRec*) * nrows, MPR_ALLOC_MANAGER | MPR_ALLOC_ZERO)) == 0) {
        return 0;
    }
    mprSetManager(grid, (MprManager) manageEdiGrid);
    grid->nrecords = nrows;
    grid->edi = edi;
    grid->tableName = tableName? sclone(tableName) : 0;
    return grid;
}


/*
    Create a free-standing record. Not saved to the database.
    The tableName parameter can be null. The fields are not initialized (no schema).
 */
PUBLIC EdiRec *ediCreateBareRec(Edi *edi, cchar *tableName, int nfields)
{
    EdiRec      *rec;

    if ((rec = mprAllocBlock(sizeof(EdiRec) + sizeof(EdiField) * nfields, MPR_ALLOC_MANAGER | MPR_ALLOC_ZERO)) == 0) {
        return 0;
    }
    mprSetManager(rec, (MprManager) ediManageEdiRec);
    rec->edi = edi;
    rec->tableName = sclone(tableName);
    rec->nfields = nfields;
    return rec;
}


PUBLIC cchar *ediFormatField(cchar *fmt, EdiField *fp)
{
    MprTime     when;

    if (fp->value == 0) {
        return "null";
    }
    switch (fp->type) {
    case EDI_TYPE_BINARY:
    case EDI_TYPE_BOOL:
        return fp->value;

    case EDI_TYPE_DATE:
        if (fmt == 0) {
            fmt = MPR_DEFAULT_DATE;
        }
        if (mprParseTime(&when, fp->value, MPR_UTC_TIMEZONE, 0) == 0) {
            return mprFormatLocalTime(fmt, when);
        }
        return fp->value;

    case EDI_TYPE_FLOAT:
        if (fmt == 0) {
            return fp->value;
        }
        return sfmt(fmt, atof(fp->value));

    case EDI_TYPE_INT:
        if (fmt == 0) {
            return fp->value;
        }
        return sfmt(fmt, stoi(fp->value));

    case EDI_TYPE_STRING:
    case EDI_TYPE_TEXT:
        if (fmt == 0) {
            return fp->value;
        }
        return sfmt(fmt, fp->value);

    default:
        mprLog("error esp edi", 0, "Unknown field type %d", fp->type);
    }
    return 0;
}


static void formatFieldForJson(MprBuf *buf, EdiField *fp)
{
    MprTime     when;
    cchar       *value;

    value = fp->value;

    if (value == 0) {
        mprPutStringToBuf(buf, "null");
        return;
    } 
    switch (fp->type) {
    case EDI_TYPE_BINARY:
        mprPutToBuf(buf, "-binary-");
        return;

    case EDI_TYPE_STRING:
    case EDI_TYPE_TEXT:
        mprFormatJsonValue(buf, MPR_JSON_STRING, fp->value, 0);
        return;

    case EDI_TYPE_BOOL:
    case EDI_TYPE_FLOAT:
    case EDI_TYPE_INT:
        mprPutStringToBuf(buf, fp->value);
        return;

    case EDI_TYPE_DATE:
        if (mprParseTime(&when, fp->value, MPR_UTC_TIMEZONE, 0) == 0) {
            mprPutToBuf(buf, "\"%s\"", mprFormatUniversalTime(MPR_RFC822_DATE, when));
        } else {
            mprPutToBuf(buf, "\"%s\"", value);
        }
        return;

    default:
        mprLog("error esp edi", 0, "Unknown field type %d", fp->type);
        mprPutStringToBuf(buf, "null");
    }
}


typedef struct Col {
    EdiGrid     *grid;          /* Source grid for this column */
    EdiField    *fp;            /* Field definition for this column */
    int         joinField;      /* Foreign key field index */
    int         field;          /* Field index in the foreign table */
} Col;


/*
    Create a list of columns to use for a joined table
    For all foreign key columns (ends with "Id"), join the columns from the referenced table.
 */
static MprList *joinColumns(MprList *cols, EdiGrid *grid, MprHash *grids, int joinField, int follow)
{
    EdiGrid     *foreignGrid;
    EdiRec      *rec;
    EdiField    *fp;
    Col         *col;
    cchar       *tableName;
    
    if (grid->nrecords == 0) {
        return cols;
    }
    rec = grid->records[0];
    for (fp = rec->fields; fp < &rec->fields[rec->nfields]; fp++) {
#if KEEP
        if (fp->flags & EDI_FOREIGN && follow)
#else
        if (sends(fp->name, "Id") && follow) 
#endif
        {
            tableName = strim(fp->name, "Id", MPR_TRIM_END);
            if (!(foreignGrid = mprLookupKey(grids, tableName))) {
                col = mprAllocObj(Col, 0);
                col->grid = grid;
                col->field = (int) (fp - rec->fields);
                col->joinField = joinField;
                col->fp = fp;
                mprAddItem(cols, col);
            } else {
                cols = joinColumns(cols, foreignGrid, grids, (int) (fp - rec->fields), 0);
            }
        } else {
#if 0
            if (fp->flags & EDI_KEY && joinField >= 0) {
                /* Don't include ID fields from joined tables */
                continue;
            }
#endif
            col = mprAllocObj(Col, 0);
            col->grid = grid;
            col->field = (int) (fp - rec->fields);
            col->joinField = joinField;
            col->fp = fp;
            mprAddItem(cols, col);
        }
    }
    return cols;
}


/*
    Join grids using an INNER JOIN. All rows are returned.
    List of grids to join must be null terminated.
 */
PUBLIC EdiGrid *ediJoin(Edi *edi, ...)
{
    EdiGrid     *primary, *grid, *result, *current;
    EdiRec      *rec;
    EdiField    *dest, *fp;
    MprList     *cols, *rows;
    MprHash     *grids;
    Col         *col;
    va_list     vgrids;
    cchar       *keyValue;
    int         r, next, nfields, nrows;

    va_start(vgrids, edi);
    if ((primary = va_arg(vgrids, EdiGrid*)) == 0) {
        return 0;
    }
    if (primary->nrecords == 0) {
        return ediCreateBareGrid(edi, NULL, 0);
    }
    /*
        Build list of grids to join
     */
    grids = mprCreateHash(0, MPR_HASH_STABLE);
    for (;;) {
        if ((grid = va_arg(vgrids, EdiGrid*)) == 0) {
            break;
        }
        mprAddKey(grids, grid->tableName, grid);
    }
    va_end(vgrids);

    /*
        Get list of columns for the result. Each col object records the target grid and field index.
     */
    cols = joinColumns(mprCreateList(0, 0), primary, grids, -1, 1);
    nfields = mprGetListLength(cols);
    rows = mprCreateList(0, 0);

    for (r = 0; r < primary->nrecords; r++) {
        if ((rec = ediCreateBareRec(edi, NULL, nfields)) == 0) {
            assert(0);
            return 0;
        }
        mprAddItem(rows, rec);
        dest = rec->fields;
        current = 0;
        for (ITERATE_ITEMS(cols, col, next)) { 
            if (col->grid == primary) {
                *dest = primary->records[r]->fields[col->field];
            } else {
                if (col->grid != current) {
                    current = col->grid;
                    keyValue = primary->records[r]->fields[col->joinField].value;
                    rec = ediReadRecWhere(edi, col->grid->tableName, "id", "==", keyValue);
                }
                if (rec) {
                    fp = &rec->fields[col->field];
                    *dest = *fp;
                    dest->name = sfmt("%s.%s", col->grid->tableName, fp->name);
                } else {
                    dest->name = sclone("UNKNOWN");
                }
            }
            dest++;
        }
    }
    nrows = mprGetListLength(rows);
    if ((result = ediCreateBareGrid(edi, NULL, nrows)) == 0) {
        return 0;
    }
    for (r = 0; r < nrows; r++) {
        result->records[r] = mprGetItem(rows, r);
    }
    result->nrecords = nrows;
    return result;
}


PUBLIC void ediManageEdiRec(EdiRec *rec, int flags)
{
    int     fid;

    if (flags & MPR_MANAGE_MARK) {
        mprMark(rec->edi);
        mprMark(rec->errors);
        mprMark(rec->tableName);
        mprMark(rec->id);

        for (fid = 0; fid < rec->nfields; fid++) {
            mprMark(rec->fields[fid].value);
            mprMark(rec->fields[fid].name);
        }
    }
}


static void manageEdiGrid(EdiGrid *grid, int flags)
{
    int     r;

    if (flags & MPR_MANAGE_MARK) {
        mprMark(grid->edi);
        mprMark(grid->tableName);
        for (r = 0; r < grid->nrecords; r++) {
            mprMark(grid->records[r]);
        }
    }
}


/*
    grid = ediMakeGrid("[ \
        { id: '1', country: 'Australia' }, \
        { id: '2', country: 'China' }, \
    ]");
 */
PUBLIC EdiGrid *ediMakeGrid(cchar *json)
{
    MprJson     *obj, *row, *rp, *cp;
    EdiGrid     *grid;
    EdiRec      *rec;
    EdiField    *fp;
    int         col, r, nrows, nfields;

    if ((obj = mprParseJson(json)) == 0) {
        assert(0);
        return 0;
    }
    nrows = (int) mprGetJsonLength(obj);
    if ((grid = ediCreateBareGrid(NULL, "", nrows)) == 0) {
        assert(0);
        return 0;
    }
    if (nrows <= 0) {
        return grid;
    }
    for (ITERATE_JSON(obj, rp, r)) {
        if (rp->type == MPR_JSON_VALUE) {
            nfields = 1;
            if ((rec = ediCreateBareRec(NULL, "", nfields)) == 0) {
                return 0;
            }
            fp = rec->fields;
            fp->valid = 1;
            fp->name = sclone("value");
            fp->value = rp->value;
            fp->type = EDI_TYPE_STRING;
            fp->flags = 0;
        } else {
            row = rp;
            nfields = (int) mprGetJsonLength(row);
            if ((rec = ediCreateBareRec(NULL, "", nfields)) == 0) {
                return 0;
            }
            fp = rec->fields;
            for (ITERATE_JSON(row, cp, col)) {
                if (fp >= &rec->fields[nfields]) {
                    break;
                }
                fp->valid = 1;
                fp->name = cp->name;
                fp->type = EDI_TYPE_STRING;
                fp->flags = 0;
                fp++;
            }
            if (ediSetFields(rec, row) == 0) {
                assert(0);
                return 0;
            }
        }
        grid->records[r] = rec;
    }
    return grid;
}


PUBLIC MprHash *ediMakeHash(cchar *fmt, ...)
{
    MprHash     *obj;
    va_list     args;

    va_start(args, fmt);
    obj = mprDeserialize(sfmtv(fmt, args));
    va_end(args);
    return obj;
}


PUBLIC MprJson *ediMakeJson(cchar *fmt, ...)
{
    MprJson     *obj;
    va_list     args;

    va_start(args, fmt);
    obj = mprParseJson(sfmtv(fmt, args));
    va_end(args);
    return obj;
}


/*
    rec = ediMakeRec("{ id: 1, title: 'Message One', body: 'Line one' }");
 */
PUBLIC EdiRec *ediMakeRec(cchar *json)
{
    MprHash     *obj;
    MprKey      *kp;
    EdiRec      *rec;
    EdiField    *fp;
    int         f, nfields;

    if ((obj = mprDeserialize(json)) == 0) {
        return 0;
    }
    nfields = mprGetHashLength(obj);
    if ((rec = ediCreateBareRec(NULL, "", nfields)) == 0) {
        return 0;
    }
    for (f = 0, ITERATE_KEYS(obj, kp)) {
        if (kp->type == MPR_JSON_ARRAY || kp->type == MPR_JSON_OBJ) {
            continue;
        }
        fp = &rec->fields[f++];
        fp->valid = 1;
        fp->name = kp->key;
        fp->value = kp->data;
        fp->type = EDI_TYPE_STRING;
        fp->flags = 0;
    }
    return rec;
}


PUBLIC int ediParseTypeString(cchar *type)
{
    if (smatch(type, "binary")) {
        return EDI_TYPE_BINARY;
    } else if (smatch(type, "bool") || smatch(type, "boolean")) {
        return EDI_TYPE_BOOL;
    } else if (smatch(type, "date")) {
        return EDI_TYPE_DATE;
    } else if (smatch(type, "float") || smatch(type, "double") || smatch(type, "number")) {
        return EDI_TYPE_FLOAT;
    } else if (smatch(type, "int") || smatch(type, "integer") || smatch(type, "fixed")) {
        return EDI_TYPE_INT;
    } else if (smatch(type, "string")) {
        return EDI_TYPE_STRING;
    } else if (smatch(type, "text")) {
        return EDI_TYPE_TEXT;
    } else {
        return MPR_ERR_BAD_ARGS;
    }
}


/*
    Swap rows for columns. The key field for each record is set to the prior column name.
 */
PUBLIC EdiGrid *ediPivotGrid(EdiGrid *grid, int flags)
{
    EdiGrid     *result;
    EdiRec      *rec, *first;
    EdiField    *src, *fp;
    int         r, c, nfields, nrows;

    if (grid->nrecords == 0) {
        return grid;
    }
    first = grid->records[0];
    nrows = first->nfields;
    nfields = grid->nrecords;
    result = ediCreateBareGrid(grid->edi, grid->tableName, nrows);
    
    for (c = 0; c < nrows; c++) {
        result->records[c] = rec = ediCreateBareRec(grid->edi, grid->tableName, nfields);
        fp = rec->fields;
        rec->id = first->fields[c].name;
        for (r = 0; r < grid->nrecords; r++) {
            src = &grid->records[r]->fields[c];
            fp->valid = 1;
            fp->name = src->name;
            fp->type = src->type;
            fp->value = src->value;
            fp->flags = src->flags;
            fp++; src++;
        }
    }
    return result;
}

PUBLIC EdiGrid *ediCloneGrid(EdiGrid *grid)
{
    EdiGrid     *result;
    EdiRec      *rec;
    EdiField    *src, *dest;
    int         r, c;

    if (grid->nrecords == 0) {
        return grid;
    }
    result = ediCreateBareGrid(grid->edi, grid->tableName, grid->nrecords);
    for (r = 0; r < grid->nrecords; r++) {
        rec = ediCreateBareRec(grid->edi, grid->tableName, grid->records[r]->nfields);
        result->records[r] = rec;
        rec->id = grid->records[r]->id;
        src = grid->records[r]->fields;
        dest = rec->fields;
        for (c = 0; c < rec->nfields; c++) {
            dest->valid = 1;
            dest->name = src->name;
            dest->value = src->value;
            dest->type = src->type;
            dest->flags = src->flags;
            dest++; src++;
        }
    }
    return result;
}


static cchar *mapEdiValue(cchar *value, int type)
{
    MprTime     time;

    if (value == 0) {
        return value;
    }
    switch (type) {
    case EDI_TYPE_DATE:
        if (!snumber(value)) {
            mprParseTime(&time, value, MPR_UTC_TIMEZONE, 0);
            value = itos(time);
        }
        break;

    case EDI_TYPE_BINARY:
    case EDI_TYPE_BOOL:
    case EDI_TYPE_FLOAT:
    case EDI_TYPE_INT:
    case EDI_TYPE_STRING:
    case EDI_TYPE_TEXT:
    default:
        break;
    }
    return sclone(value);
}


PUBLIC EdiRec *ediSetField(EdiRec *rec, cchar *fieldName, cchar *value)
{
    EdiField    *fp;

    if (rec == 0) {
        return 0;
    }
    if (fieldName == 0) {
        return 0;
    }
    for (fp = rec->fields; fp < &rec->fields[rec->nfields]; fp++) {
        if (smatch(fp->name, fieldName)) {
            fp->value = mapEdiValue(value, fp->type);
            return rec;
        }
    }
    return rec;
}


PUBLIC EdiRec *ediSetFields(EdiRec *rec, MprJson *params)
{
    MprJson     *param;
    int         i;

    if (rec == 0) {
        return 0;
    }
    for (ITERATE_JSON(params, param, i)) {
        if (param->type & MPR_JSON_VALUE) {
            if (!ediSetField(rec, param->name, param->value)) {
                return 0;
            }
        }
    }
    return rec;
}


typedef struct GridSort {
    int     sortColumn;         /**< Column to sort on */
    int     sortOrder;          /**< Sort order: ascending == 1, descending == -1 */
} GridSort;

static int sortRec(EdiRec **r1, EdiRec **r2, GridSort *gs)
{
    EdiField    *f1, *f2;
    int64       v1, v2;

    f1 = &(*r1)->fields[gs->sortColumn];
    f2 = &(*r2)->fields[gs->sortColumn];
    if (f1->type == EDI_TYPE_INT) {
        v1 = stoi(f1->value);
        v2 = stoi(f2->value);
        if (v1 < v2) {
            return - gs->sortOrder;
        } else if (v1 > v2) {
            return gs->sortOrder;
        }
        return 0;
    } else {
        return scmp(f1->value, f2->value) * gs->sortOrder;
    }
}


static int lookupGridField(EdiGrid *grid, cchar *name)
{
    EdiRec      *rec;
    EdiField    *fp;

    if (grid->nrecords == 0) {
        return MPR_ERR_CANT_FIND;
    }
    rec = grid->records[0];
    for (fp = rec->fields; fp < &rec->fields[rec->nfields]; fp++) {
        if (smatch(name, fp->name)) {
            return (int) (fp - rec->fields);
        }
    }
    return MPR_ERR_CANT_FIND;
}


//  FUTURE - document
PUBLIC EdiGrid *ediSortGrid(EdiGrid *grid, cchar *sortColumn, int sortOrder)
{
    GridSort    gs;

    if (grid->nrecords == 0) {
        return grid;
    }
    grid = ediCloneGrid(grid);
    gs.sortColumn = lookupGridField(grid, sortColumn);
    gs.sortOrder = sortOrder;
    mprSort(grid->records, grid->nrecords, sizeof(EdiRec*), (MprSortProc) sortRec, &gs);
    return grid;
}


/********************************* Validations *****************************/

static cchar *checkBoolean(EdiValidation *vp, EdiRec *rec, cchar *fieldName, cchar *value)
{
    if (value && *value) {
        if (scaselessmatch(value, "true") || scaselessmatch(value, "false")) {
            return 0;
        }
    }
    return "is not a number";
}


static cchar *checkDate(EdiValidation *vp, EdiRec *rec, cchar *fieldName, cchar *value)
{
    MprTime     time;

    if (value && *value) {
        if (mprParseTime(&time, value, MPR_UTC_TIMEZONE, NULL) < 0) {
            return 0;
        }
    }
    return "is not a date or time";
}


static cchar *checkFormat(EdiValidation *vp, EdiRec *rec, cchar *fieldName, cchar *value)
{
    int     matched[ME_MAX_ROUTE_MATCHES * 2];

    if (pcre_exec(vp->mdata, NULL, value, (int) slen(value), 0, 0, matched, sizeof(matched) / sizeof(int)) > 0) {
        return 0;
    }
    return "is in the wrong format";
}


static cchar *checkBanned(EdiValidation *vp, EdiRec *rec, cchar *fieldName, cchar *value)
{
    int     matched[ME_MAX_ROUTE_MATCHES * 2];

    if (pcre_exec(vp->mdata, NULL, value, (int) slen(value), 0, 0, matched, sizeof(matched) / sizeof(int)) > 0) {
        return "contains banned content";
    }
    return 0;
}


static cchar *checkInteger(EdiValidation *vp, EdiRec *rec, cchar *fieldName, cchar *value)
{
    if (value && *value) {
        if (snumber(value)) {
            return 0;
        }
    }
    return "is not an integer";
}


static cchar *checkNumber(EdiValidation *vp, EdiRec *rec, cchar *fieldName, cchar *value)
{
    if (value && *value) {
        if (strspn(value, "1234567890+-.") == strlen(value)) {
            return 0;
        }
    }
    return "is not a number";
}


static cchar *checkPresent(EdiValidation *vp, EdiRec *rec, cchar *fieldName, cchar *value)
{
    if (value && *value) {
        return 0;
    }
    return "is missing";
}


static cchar *checkUnique(EdiValidation *vp, EdiRec *rec, cchar *fieldName, cchar *value)
{
    EdiRec  *other;

    //  OPT Could require an index to enforce this.
    if ((other = ediReadRecWhere(rec->edi, rec->tableName, fieldName, "==", value)) == 0) {
        return 0;
    }
    if (smatch(other->id, rec->id)) {
        return 0;
    }
    return "is not unique";
}


PUBLIC void ediAddFieldError(EdiRec *rec, cchar *field, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    if (rec->errors == 0) {
        rec->errors = mprCreateHash(0, MPR_HASH_STABLE);
    }
    mprAddKey(rec->errors, field, sfmtv(fmt, args));
    va_end(args);
}


PUBLIC void ediDefineValidation(cchar *name, EdiValidationProc vfn)
{
    EdiService  *es;

    es = MPR->ediService;
    mprAddKey(es->validations, name, vfn);
}


PUBLIC void ediDefineMigration(Edi *edi, EdiMigration forw, EdiMigration back)
{
    edi->forw = forw;
    edi->back = back;
}


PUBLIC void ediSetPrivate(Edi *edi, bool on)
{
    edi->flags &= ~EDI_PRIVATE;
    if (on) {
        edi->flags |= EDI_PRIVATE;
    }
}


PUBLIC void ediSetReadonly(Edi *edi, bool on)
{
    edi->flags &= ~EDI_NO_SAVE;
    if (on) {
        edi->flags |= EDI_NO_SAVE;
    }
}


static void addValidations()
{
    EdiService  *es;

    es = MPR->ediService;
    /* Thread safe */
    es->validations = mprCreateHash(0, MPR_HASH_STATIC_VALUES);
    ediDefineValidation("boolean", checkBoolean);
    ediDefineValidation("format", checkFormat);
    ediDefineValidation("banned", checkBanned);
    ediDefineValidation("integer", checkInteger);
    ediDefineValidation("number", checkNumber);
    ediDefineValidation("present", checkPresent);
    ediDefineValidation("date", checkDate);
    ediDefineValidation("unique", checkUnique);
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

/*
    espAbbrev.c -- ESP Abbreviated API

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/*************************************** Code *********************************/

PUBLIC void addHeader(cchar *key, cchar *fmt, ...)
{
    va_list     args;
    cchar       *value;

    va_start(args, fmt);
    value = sfmtv(fmt, args);
    espAddHeaderString(getConn(), key, value);
    va_end(args);
}


PUBLIC void addParam(cchar *key, cchar *value)
{
    if (!param(key)) {
        setParam(key, value);
    }
}


PUBLIC bool canUser(cchar *abilities, bool warn)
{
    HttpConn    *conn;

    conn = getConn();
    if (httpCanUser(conn, abilities)) {
        return 1;
    }
    if (warn) {
        setStatus(HTTP_CODE_UNAUTHORIZED);
        sendResult(feedback("error", "Access Denied. Insufficient Privilege."));
    }
    return 0;
}


PUBLIC EdiRec *createRec(cchar *tableName, MprJson *params)
{
    return setRec(ediSetFields(ediCreateRec(getDatabase(), tableName), params));
}


PUBLIC bool createRecFromParams(cchar *table)
{
    return updateRec(createRec(table, params()));
}


/*
    Return the session ID
 */
PUBLIC cchar *createSession()
{
    return espCreateSession(getConn());
}


/*
    Destroy a session and erase the session state data.
    This emits an expired Set-Cookie header to the browser to force it to erase the cookie.
 */
PUBLIC void destroySession()
{
    httpDestroySession(getConn());
}


PUBLIC void dontAutoFinalize()
{
    espSetAutoFinalizing(getConn(), 0);
}


PUBLIC bool feedback(cchar *kind, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    espSetFeedbackv(getConn(), kind, fmt, args);
    va_end(args);

    /*
        Return true if there is not an error feedback message
     */
    return getFeedback("error") == 0;
}


PUBLIC void finalize()
{
    espFinalize(getConn());
}


#if DEPRECATED || 1
PUBLIC void flash(cchar *kind, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    espSetFeedbackv(getConn(), kind, fmt, args);
    va_end(args);
}
#endif


PUBLIC void flush()
{
    espFlush(getConn());
}


PUBLIC HttpAuth *getAuth()
{
    return espGetAuth(getConn());
}


PUBLIC MprList *getColumns(EdiRec *rec)
{
    if (rec == 0) {
        if ((rec = getRec()) == 0) {
            return 0;
        }
    }
    return ediGetColumns(getDatabase(), rec->tableName);
}


PUBLIC cchar *getConfig(cchar *field)
{
    HttpRoute   *route;
    cchar       *value;

    route = getConn()->rx->route;
    if ((value = mprGetJson(route->config, field)) == 0) {
        return "";
    }
    return value;
}


PUBLIC HttpConn *getConn()
{
    HttpConn    *conn;

    conn = mprGetThreadData(((Esp*) MPR->espService)->local);
    if (conn == 0) {
        mprLog("error esp", 0, "Connection is not defined in thread local storage.\n"
        "If using a callback, make sure you invoke espSetConn with the connection before using the ESP abbreviated API");
    }
    return conn;
}


PUBLIC cchar *getCookies()
{
    return espGetCookies(getConn());
}


PUBLIC MprOff getContentLength()
{
    return espGetContentLength(getConn());
}


PUBLIC cchar *getContentType()
{
    return getConn()->rx->mimeType;
}


PUBLIC void *getData()
{
    return espGetData(getConn());
}


PUBLIC Edi *getDatabase()
{
    return espGetDatabase(getConn());
}


PUBLIC MprDispatcher *getDispatcher()
{
    HttpConn    *conn;

    if ((conn = getConn()) == 0) {
        return 0;
    }
    return conn->dispatcher;
}


PUBLIC cchar *getDocuments()
{
    return getConn()->rx->route->documents;
}


PUBLIC EspRoute *getEspRoute()
{
    return espGetEspRoute(getConn());
}


PUBLIC cchar *getFeedback(cchar *kind)
{
    return espGetFeedback(getConn(), kind);
}


PUBLIC cchar *getField(EdiRec *rec, cchar *field)
{
    return ediGetFieldValue(rec, field);
}


PUBLIC cchar *getFieldError(cchar *field)
{
    return mprLookupKey(getRec()->errors, field);
}


PUBLIC EdiGrid *getGrid()
{
    return getConn()->grid;
}


PUBLIC cchar *getHeader(cchar *key)
{
    return espGetHeader(getConn(), key);
}


PUBLIC cchar *getMethod()
{
    return espGetMethod(getConn());
}


PUBLIC cchar *getQuery()
{
    return getConn()->rx->parsedUri->query;
}


PUBLIC EdiRec *getRec()
{
    return getConn()->record;
}


PUBLIC cchar *getReferrer()
{
    return espGetReferrer(getConn());
}


PUBLIC EspReq *getReq()
{
    return getConn()->data;
}


PUBLIC HttpRoute *getRoute()
{
    return espGetRoute(getConn());
}


PUBLIC cchar *getSecurityToken()
{
    return httpGetSecurityToken(getConn(), 0);
}


/*
    Get a session and return the session ID. Creates a session if one does not already exist.
 */
PUBLIC cchar *getSessionID()
{
    return espGetSessionID(getConn(), 1);
}


PUBLIC cchar *getSessionVar(cchar *key)
{
    return httpGetSessionVar(getConn(), key, 0);
}


PUBLIC cchar *getPath()
{
    return espGetPath(getConn());
}


PUBLIC MprList *getUploads()
{
    return espGetUploads(getConn());
}


PUBLIC cchar *getUri()
{
    return espGetUri(getConn());
}


PUBLIC bool hasGrid()
{
    return espHasGrid(getConn());
}


PUBLIC bool hasRec()
{
    return espHasRec(getConn());
}


PUBLIC bool isEof()
{
    return httpIsEof(getConn());
}


PUBLIC bool isFinalized()
{
    return espIsFinalized(getConn());
}


PUBLIC bool isSecure()
{
    return espIsSecure(getConn());
}


PUBLIC EdiGrid *makeGrid(cchar *contents)
{
    return ediMakeGrid(contents);
}


PUBLIC MprHash *makeHash(cchar *fmt, ...)
{
    va_list     args;
    cchar       *str;

    va_start(args, fmt);
    str = sfmtv(fmt, args);
    va_end(args);
    return mprDeserialize(str);
}


PUBLIC MprJson *makeJson(cchar *fmt, ...)
{
    va_list     args;
    cchar       *str;

    va_start(args, fmt);
    str = sfmtv(fmt, args);
    va_end(args);
    return mprParseJson(str);
}


PUBLIC EdiRec *makeRec(cchar *contents)
{
    return ediMakeRec(contents);
}


PUBLIC cchar *makeUri(cchar *target)
{
    return espUri(getConn(), target);
}


PUBLIC cchar *md5(cchar *str)
{
    return mprGetMD5(str);
}


PUBLIC bool modeIs(cchar *kind)
{
    HttpRoute   *route;

    route = getConn()->rx->route;
    return smatch(route->mode, kind);
}


PUBLIC cchar *nonce()
{
    return mprGetMD5(itos(mprRandom()));
}


PUBLIC cchar *param(cchar *key)
{
    return espGetParam(getConn(), key, 0);
}


PUBLIC MprJson *params()
{
    return espGetParams(getConn());
}


PUBLIC ssize receive(char *buf, ssize len)
{
    return httpRead(getConn(), buf, len);
}


PUBLIC EdiRec *readRecWhere(cchar *tableName, cchar *fieldName, cchar *operation, cchar *value)
{
    return setRec(ediReadRecWhere(getDatabase(), tableName, fieldName, operation, value));
}


PUBLIC EdiRec *readRec(cchar *tableName, cchar *key)
{
    if (key == 0 || *key == 0) {
        key = "1";
    }
    return setRec(ediReadRec(getDatabase(), tableName, key));
}


PUBLIC EdiRec *readRecByKey(cchar *tableName, cchar *key)
{
    return setRec(ediReadRec(getDatabase(), tableName, key));
}


PUBLIC EdiGrid *readWhere(cchar *tableName, cchar *fieldName, cchar *operation, cchar *value)
{
    return setGrid(ediReadWhere(getDatabase(), tableName, fieldName, operation, value));
}


PUBLIC EdiGrid *readTable(cchar *tableName)
{
    return setGrid(ediReadWhere(getDatabase(), tableName, 0, 0, 0));
}


PUBLIC void redirect(cchar *target)
{
    espRedirect(getConn(), 302, target);
}


PUBLIC void redirectBack()
{
    espRedirectBack(getConn());
}


PUBLIC void removeCookie(cchar *name)
{
    espRemoveCookie(getConn(), name);
}


PUBLIC bool removeRec(cchar *tableName, cchar *key)
{
    if (ediRemoveRec(getDatabase(), tableName, key) < 0) {
        feedback("error", "Cannot delete %s", stitle(tableName));
        return 0;
    }
    feedback("info", "Deleted %s", stitle(tableName));
    return 1;
}


PUBLIC void removeSessionVar(cchar *key)
{
    httpRemoveSessionVar(getConn(), key);
}


PUBLIC ssize render(cchar *fmt, ...)
{
    va_list     args;
    ssize       count;
    cchar       *msg;

    va_start(args, fmt);
    msg = sfmtv(fmt, args);
    count = espRenderString(getConn(), msg);
    va_end(args);
    return count;
}


PUBLIC ssize renderCached()
{
    return espRenderCached(getConn());;
}


PUBLIC ssize renderConfig()
{
    return espRenderConfig(getConn());;
}


PUBLIC void renderError(int status, cchar *fmt, ...)
{
    va_list     args;
    cchar       *msg;

    va_start(args, fmt);
    msg = sfmt(fmt, args);
    espRenderError(getConn(), status, "%s", msg);
    va_end(args);
}


PUBLIC ssize renderFile(cchar *path)
{
    return espRenderFile(getConn(), path);
}


PUBLIC void renderFeedback(cchar *kind)
{
    espRenderFeedback(getConn(), kind);
}


PUBLIC ssize renderSafe(cchar *fmt, ...)
{
    va_list     args;
    ssize       count;
    cchar       *msg;

    va_start(args, fmt);
    msg = sfmtv(fmt, args);
    count = espRenderSafeString(getConn(), msg);
    va_end(args);
    return count;
}


PUBLIC ssize renderString(cchar *s)
{
    return espRenderString(getConn(), s);
}


PUBLIC void renderView(cchar *view)
{
    espRenderDocument(getConn(), view);
}


#if KEEP
PUBLIC int runCmd(cchar *command, char *input, char **output, char **error, MprTime timeout, int flags)
{
    return mprRun(getDispatcher(), command, input, output, error, timeout, MPR_CMD_IN  | MPR_CMD_OUT | MPR_CMD_ERR | flags);
}
#endif


PUBLIC int runCmd(cchar *command, char *input, char **output, char **error, MprTime timeout, int flags)
{
    MprCmd  *cmd;

    cmd = mprCreateCmd(getDispatcher());
    return mprRunCmd(cmd, command, NULL, input, output, error, timeout, MPR_CMD_IN  | MPR_CMD_OUT | MPR_CMD_ERR | flags);
}


/*
    Add a security token to the response. The token is generated as a HTTP header and session cookie.
 */
PUBLIC void securityToken()
{
    httpAddSecurityToken(getConn(), 0);
}


PUBLIC ssize sendGrid(EdiGrid *grid)
{
    return espSendGrid(getConn(), grid, 0);
}


PUBLIC ssize sendRec(EdiRec *rec)
{
    return espSendRec(getConn(), rec, 0);
}


PUBLIC void sendResult(bool status)
{
    espSendResult(getConn(), status);
}


PUBLIC void setConn(HttpConn *conn)
{
    espSetConn(conn);
}


PUBLIC void setContentType(cchar *mimeType)
{
    espSetContentType(getConn(), mimeType);
}


PUBLIC void setCookie(cchar *name, cchar *value, cchar *path, cchar *cookieDomain, MprTicks lifespan, bool isSecure)
{
    espSetCookie(getConn(), name, value, path, cookieDomain, lifespan, isSecure);
}


PUBLIC void setData(void *data)
{
    espSetData(getConn(), data);
}


PUBLIC EdiRec *setField(EdiRec *rec, cchar *fieldName, cchar *value)
{
    return ediSetField(rec, fieldName, value);
}


PUBLIC EdiRec *setFields(EdiRec *rec, MprJson *params)
{
    return ediSetFields(rec, params);
}


PUBLIC EdiGrid *setGrid(EdiGrid *grid)
{
    getConn()->grid = grid;
    return grid;
}


PUBLIC void setHeader(cchar *key, cchar *fmt, ...)
{
    va_list     args;
    cchar       *value;

    va_start(args, fmt);
    value = sfmtv(fmt, args);
    espSetHeaderString(getConn(), key, value);
    va_end(args);
}


PUBLIC void setIntParam(cchar *key, int value)
{
    espSetIntParam(getConn(), key, value);
}


PUBLIC void setNotifier(HttpNotifier notifier)
{
    espSetNotifier(getConn(), notifier);
}


PUBLIC void setParam(cchar *key, cchar *value)
{
    espSetParam(getConn(), key, value);
}


PUBLIC EdiRec *setRec(EdiRec *rec)
{
    return espSetRec(getConn(), rec);
}


PUBLIC void setSessionVar(cchar *key, cchar *value)
{
    httpSetSessionVar(getConn(), key, value);
}


PUBLIC void setStatus(int status)
{
    espSetStatus(getConn(), status);
}


PUBLIC cchar *session(cchar *key)
{
    return getSessionVar(key);
}


PUBLIC void setTimeout(void *proc, MprTicks timeout, void *data)
{
    mprCreateEvent(getConn()->dispatcher, "setTimeout", (int) timeout, proc, data, 0);
}


PUBLIC void showRequest()
{
    espShowRequest(getConn());
}


PUBLIC EdiGrid *sortGrid(EdiGrid *grid, cchar *sortColumn, int sortOrder)
{
    return ediSortGrid(grid, sortColumn, sortOrder);
}


PUBLIC void updateCache(cchar *uri, cchar *data, int lifesecs)
{
    espUpdateCache(getConn(), uri, data, lifesecs);
}


PUBLIC bool updateField(cchar *tableName, cchar *key, cchar *fieldName, cchar *value)
{
    return ediUpdateField(getDatabase(), tableName, key, fieldName, value) == 0;
}


PUBLIC bool updateFields(cchar *tableName, MprJson *params)
{
    EdiRec  *rec;
    cchar   *key;

    key = mprReadJson(params, "id");
    if ((rec = ediSetFields(ediReadRec(getDatabase(), tableName, key), params)) == 0) {
        return 0;
    }
    return updateRec(rec);
}


PUBLIC bool updateRec(EdiRec *rec)
{
    if (!rec) {
        feedback("error", "Cannot save record");
        return 0;
    }
    setRec(rec);
    if (ediUpdateRec(getDatabase(), rec) < 0) {
        feedback("error", "Cannot save %s", stitle(rec->tableName));
        return 0;
    }
    feedback("info", "Saved %s", stitle(rec->tableName));
    return 1;
}


PUBLIC bool updateRecFromParams(cchar *table)
{
    return updateRec(setFields(readRec(table, param("id")), params()));
}


PUBLIC cchar *uri(cchar *target, ...)
{
    va_list     args;
    cchar       *uri;

    va_start(args, target);
    uri = sfmtv(target, args);
    va_end(args);
    return httpLink(getConn(), uri);
}


PUBLIC cchar *absuri(cchar *target, ...)
{
    va_list     args;
    cchar       *uri;

    va_start(args, target);
    uri = sfmtv(target, args);
    va_end(args);
    return httpLinkAbs(getConn(), uri);
}


#if DEPRECATED || 1
/*
    <% stylesheets(patterns); %>

    Where patterns may contain *, ** and !pattern for exclusion
 */
PUBLIC void stylesheets(cchar *patterns)
{
    HttpConn    *conn;
    HttpRx      *rx;
    HttpRoute   *route;
    EspRoute    *eroute;
    MprList     *files;
    cchar       *filename, *ext, *uri, *path, *kind, *version, *clientDir;
    int         next;

    conn = getConn();
    rx = conn->rx;
    route = rx->route;
    eroute = route->eroute;
    patterns = httpExpandRouteVars(route, patterns);
    clientDir = httpGetDir(route, "documents");

    if (!patterns || !*patterns) {
        version = espGetConfig(route, "version", "1.0.0");
        if (eroute->combineSheet) {
            /* Previously computed combined stylesheet filename */
            stylesheets(eroute->combineSheet);

        } else if (espGetConfig(route, "http.content.combine[@=css]", 0)) {
            if (espGetConfig(route, "http.content.minify[@=css]", 0)) {
                eroute->combineSheet = sfmt("css/all-%s.min.css", version);
            } else {
                eroute->combineSheet = sfmt("css/all-%s.css", version);
            }
            stylesheets(eroute->combineSheet);

        } else {
            /*
                Not combining into a single stylesheet, so give priority to all.less over all.css if present
                Load a pure CSS incase some styles need to be applied before the lesssheet is parsed
             */
            ext = espGetConfig(route, "http.content.stylesheets", "css");
            filename = mprJoinPathExt("css/all", ext);
            path = mprJoinPath(clientDir, filename);
            if (mprPathExists(path, R_OK)) {
                stylesheets(filename);
            } else if (!smatch(ext, "less")) {
                path = mprJoinPath(clientDir, "css/all.less");
                if (mprPathExists(path, R_OK)) {
                    stylesheets("css/all.less");
                }
            }
        }
    } else {
        if (sends(patterns, "all.less")) {
            path = mprJoinPath(clientDir, "css/fix.css");
            if (mprPathExists(path, R_OK)) {
                stylesheets("css/fix.css");
            }
        }
        if ((files = mprGlobPathFiles(clientDir, patterns, MPR_PATH_RELATIVE)) == 0 || mprGetListLength(files) == 0) {
            files = mprCreateList(0, 0);
            mprAddItem(files, patterns);
        }
        for (ITERATE_ITEMS(files, path, next)) {
            path = sjoin("~/", strim(path, ".gz", MPR_TRIM_END), NULL);
            uri = httpLink(conn, path);
            kind = mprGetPathExt(path);
            if (smatch(kind, "css")) {
                espRender(conn, "<link rel='stylesheet' type='text/css' href='%s' />\n", uri);
            } else {
                espRender(conn, "<link rel='stylesheet/%s' type='text/css' href='%s' />\n", kind, uri);
            }
        }
    }
}


/*
    <% scripts(patterns); %>

    Where patterns may contain *, ** and !pattern for exclusion
 */
PUBLIC void scripts(cchar *patterns)
{
    HttpConn    *conn;
    HttpRx      *rx;
    HttpRoute   *route;
    EspRoute    *eroute;
    MprList     *files;
    MprJson     *cscripts, *script;
    cchar       *uri, *path, *version;
    int         next, ci;

    conn = getConn();
    rx = conn->rx;
    route = rx->route;
    eroute = route->eroute;
    patterns = httpExpandRouteVars(route, patterns);

    if (!patterns || !*patterns) {
        version = espGetConfig(route, "version", "1.0.0");
        if (eroute->combineScript) {
            scripts(eroute->combineScript);
        } else if (espGetConfig(route, "http.content.combine[@=js]", 0)) {
            if (espGetConfig(route, "http.content.minify[@=js]", 0)) {
                eroute->combineScript = sfmt("all-%s.min.js", version);
            } else {
                eroute->combineScript = sfmt("all-%s.js", version);
            }
            scripts(eroute->combineScript);
        } else {
            if ((cscripts = mprGetJsonObj(route->config, "client.scripts")) != 0) {
                for (ITERATE_JSON(cscripts, script, ci)) {
                    scripts(script->value);
                }
            }
        }
        return;
    }
    if ((files = mprGlobPathFiles(httpGetDir(route, "client"), patterns, MPR_PATH_RELATIVE)) == 0 || 
            mprGetListLength(files) == 0) {
        files = mprCreateList(0, 0);
        mprAddItem(files, patterns);
    }
    for (ITERATE_ITEMS(files, path, next)) {
        if (schr(path, '$')) {
            path = stemplateJson(path, route->config);
        }
        path = sjoin("~/", strim(path, ".gz", MPR_TRIM_END), NULL);
        uri = httpLink(conn, path);
        espRender(conn, "<script src='%s' type='text/javascript'></script>\n", uri);
    }
}


#endif
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

/*
    espConfig.c -- ESP Configuration

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/*********************************** Includes *********************************/



/************************************* Locals *********************************/

#define ITERATE_CONFIG(route, obj, child, index) \
    index = 0, child = obj ? obj->children: 0; obj && index < obj->length && !route->error; child = child->next, index++
static void defineEnv(HttpRoute *route, cchar *key, cchar *value);

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
        if (espInit(route, prefix, config) < 0) {
            httpParseError(route, "Cannot define ESP application at: %s", config);
            return;
        }
        httpFinalizeRoute(route);

    } else if (prop->type & MPR_JSON_STRING) {
        files = mprGlobPathFiles(".", prop->value, MPR_PATH_RELATIVE);
        for (ITERATE_ITEMS(files, config, next)) {
            route = httpCreateInheritedRoute(parent);
            prefix = mprGetPathBase(mprGetPathDir(mprGetAbsPath(config)));
            if (espInit(route, prefix, config) < 0) {
                httpParseError(route, "Cannot define ESP application at: %s", config);
                return;
            }
            httpFinalizeRoute(route);
        }
    }
}       


static void parseEsp(HttpRoute *route, cchar *key, MprJson *prop)
{
    EspRoute    *eroute;

    eroute = route->eroute;

    if (smatch(mprGetJson(prop, "app"), "true")) {
        eroute->app = 1;
#if DEPRECATE || 1
    } else if (mprGetJson(prop, "server.listen") || mprGetJson(prop, "generate")) {
        eroute->app = 1;
        /* Here for legacy apps without esp.app */
#endif
    }
    if (eroute->app) {
        /*
            Set some defaults before parsing "esp". This permits user overrides.
         */
        httpSetRouteXsrf(route, 1);
        httpAddRouteHandler(route, "espHandler", "");
        eroute->keep = smatch(route->mode, "release") == 0;
    }
    espSetDefaultDirs(route, eroute->app);
    httpParseAll(route, key, prop);
    if (eroute->app) {
        if (!mprLookupStringItem(route->indexes, "index.esp")) {
            httpAddRouteIndex(route, "index.esp");
        }
        if (!mprLookupStringItem(route->indexes, "index.html")) {
            httpAddRouteIndex(route, "index.html");
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


static void parseCompile(HttpRoute *route, cchar *key, MprJson *prop)
{
    EspRoute    *eroute;

    eroute = route->eroute;
    eroute->compile = (prop->type & MPR_JSON_TRUE) ? 1 : 0;
}


#if ME_WIN_LIKE
PUBLIC cchar *espGetVisualStudio()
{
    cchar   *path;
    int     v;

    if ((path = getenv("VSINSTALLDIR")) != 0) {
        return path;
    }
    /* VS 2015 == 14.0 */
    for (v = 16; v >= 8; v--) {
        if ((path = mprReadRegistry(ESP_VSKEY, sfmt("%d.0", v))) != 0) {
            path = strim(path, "\\", MPR_TRIM_END);
            break;
        }
    }
    if (!path) {
        path = "${VS}";
    }
    return path;
}


/*
    WARNING: yields
 */
PUBLIC int getVisualStudioEnv(Esp *esp)
{
    char        *error, *output, *next, *line, *key, *value;
    cchar       *arch, *cpu, *command, *vs;

    /*
        Get the real system architecture, not whether this app is 32 or 64 bit.
        On native 64 bit systems, PA is amd64 for 64 bit apps and is PAW6432 is amd64 for 32 bit apps 
     */
    if (smatch(getenv("PROCESSOR_ARCHITECTURE"), "AMD64") || getenv("PROCESSOR_ARCHITEW6432")) {
        cpu = "x64";
    } else {
        cpu = "x86";
    }
    httpParsePlatform(HTTP->platform, NULL, &arch, NULL);
    if (smatch(arch, "x64")) {
        arch = smatch(cpu, "x86") ? "x86_amd64" : "amd64";

    } else if (smatch(arch, "x86")) {
        arch = smatch(cpu, "x64") ? "amd64_x86" : "x86";

    } else if (smatch(arch, "arm")) {
        arch = smatch(cpu, "x86") ? "x86_arm" : "amd64_arm";

    } else {
        mprLog("error esp", 0, "Unsupported architecture %s", arch);
        return MPR_ERR_CANT_FIND;
    }
    vs = espGetVisualStudio();
    command = sfmt("\"%s\\vcvars.bat\" \"%s\" %s", mprGetAppDir(), mprJoinPath(vs, "VC/vcvarsall.bat"), arch);
    if (mprRun(NULL, command, 0, &output, &error, -1) < 0) {
        mprLog("error esp", 0, "Cannot run command: %s, error %s", command, error);
        return MPR_ERR_CANT_READ;
    }
    esp->vstudioEnv = mprCreateHash(0, 0);

    next = output;
    while ((line = stok(next, "\r\n", &next)) != 0) {
        key = stok(line, "=", &value);
        if (scaselessmatch(key, "LIB") ||
            scaselessmatch(key, "INCLUDE") ||
            scaselessmatch(key, "PATH") ||
            scaselessmatch(key, "VSINSTALLDIR") ||
            scaselessmatch(key, "WindowsSdkDir") ||
            scaselessmatch(key, "WindowsSdkLibVersion")) {
            mprAddKey(esp->vstudioEnv, key, value);
        }
    }
    return 0;
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
            Esp     *esp;
            MprKey  *kp;

            /*
                Already set in users environment
             */
            if (scontains(getenv("LIB"), "Microsoft Visual Studio") &&
                scontains(getenv("INCLUDE"), "Microsoft Visual Studio") &&
                scontains(getenv("PATH"), "Microsoft Visual Studio")) {
                return;
            }
            /*
                By default, we use vsinstallvars.bat. However users can override by defining their own.
                WARNING: yields
             */
            esp = MPR->espService;
            if (!esp->vstudioEnv && getVisualStudioEnv(esp) < 0) {
                return;
            }
            for (ITERATE_KEYS(esp->vstudioEnv, kp)) {
                mprLog("info esp", 0, "define env %s %s", kp->key, kp->data);
                defineEnv(route, kp->key, kp->data);
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
        if ((set = mprGetJsonObj(route->config, sfmt("esp.build.env.%s.%s", value, arch))) != 0) {
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
            eroute->compileCmd = rule;
        }
        if ((rule = mprGetJson(route->config, sfmt("%s.%s", stem, "link"))) != 0) {
            eroute->linkCmd = rule;
        }
        if ((env = mprGetJsonObj(route->config, sfmt("%s.%s", stem, "env"))) != 0) {
            if (eroute->env == 0) {
                eroute->env = mprCreateHash(-1, MPR_HASH_STABLE);
            }
            for (ITERATE_CONFIG(route, env, child, ji)) {
                /* WARNING: yields */
                defineEnv(route, child->name, child->value);
            }
        }
    } else {
        httpParseError(route, "Cannot find compile rules for O/S \"%s\"", os);
    }
}


static void parseKeep(HttpRoute *route, cchar *key, MprJson *prop)
{
    EspRoute    *eroute;

    eroute = route->eroute;
    eroute->keep = (prop->type & MPR_JSON_TRUE) ? 1 : 0;
}


static void parseOptimize(HttpRoute *route, cchar *key, MprJson *prop)
{
    EspRoute    *eroute;

    eroute = route->eroute;
    eroute->compileMode = smatch(prop->value, "true") ? ESP_COMPILE_OPTIMIZED : ESP_COMPILE_SYMBOLS;
}


static void parseUpdate(HttpRoute *route, cchar *key, MprJson *prop)
{
    EspRoute    *eroute;

    eroute = route->eroute;
    eroute->update = (prop->type & MPR_JSON_TRUE) ? 1 : 0;
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
    httpDefineRouteSet("esp-server", serverRouteSet);
    httpDefineRouteSet("esp-restful", restfulRouteSet);
#if DEPRECATED || 1
    httpDefineRouteSet("esp-angular-mvc", legacyRouteSet);
    httpDefineRouteSet("esp-html-mvc", legacyRouteSet);
#endif
    httpAddConfig("esp", parseEsp);
    httpAddConfig("esp.apps", parseApps);
    httpAddConfig("esp.build", parseBuild);
    httpAddConfig("esp.combine", parseCombine);
    httpAddConfig("esp.compile", parseCompile);
    httpAddConfig("esp.keep", parseKeep);
    httpAddConfig("esp.optimize", parseOptimize);
    httpAddConfig("esp.update", parseUpdate);
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

/*
    espFramework.c -- ESP Web Framework API

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************* Locals *********************************/

#define ITERATE_CONFIG(route, obj, child, index) \
    index = 0, child = obj ? obj->children: 0; obj && index < obj->length && !route->error; child = child->next, index++

/************************************* Code ***********************************/

#if DEPRECATED || 1
PUBLIC void espAddPak(HttpRoute *route, cchar *name, cchar *version)
{
    if (!version || !*version || smatch(version, "0.0.0")) {
        version = "*";
    }
    mprSetJson(route->config, sfmt("dependencies.%s", name), version, MPR_JSON_STRING);
}
#endif


/*
    Add a http header if not already defined
 */
PUBLIC void espAddHeader(HttpConn *conn, cchar *key, cchar *fmt, ...)
{
    va_list     vargs;

    assert(key && *key);
    assert(fmt && *fmt);

    va_start(vargs, fmt);
    httpAddHeaderString(conn, key, sfmt(fmt, vargs));
    va_end(vargs);
}


/*
    Add a header string if not already defined
 */
PUBLIC void espAddHeaderString(HttpConn *conn, cchar *key, cchar *value)
{
    httpAddHeaderString(conn, key, value);
}


PUBLIC void espAddParam(HttpConn *conn, cchar *var, cchar *value)
{
    if (!httpGetParam(conn, var, 0)) {
        httpSetParam(conn, var, value);
    }
}


/*
   Append a header. If already defined, the value is catenated to the pre-existing value after a ", " separator.
   As per the HTTP/1.1 spec.
 */
PUBLIC void espAppendHeader(HttpConn *conn, cchar *key, cchar *fmt, ...)
{
    va_list     vargs;

    assert(key && *key);
    assert(fmt && *fmt);

    va_start(vargs, fmt);
    httpAppendHeaderString(conn, key, sfmt(fmt, vargs));
    va_end(vargs);
}


/*
   Append a header string. If already defined, the value is catenated to the pre-existing value after a ", " separator.
   As per the HTTP/1.1 spec.
 */
PUBLIC void espAppendHeaderString(HttpConn *conn, cchar *key, cchar *value)
{
    httpAppendHeaderString(conn, key, value);
}


PUBLIC void espAutoFinalize(HttpConn *conn)
{
    EspReq  *req;

    req = conn->reqData;
    if (req->autoFinalize) {
        httpFinalize(conn);
    }
}


PUBLIC int espCache(HttpRoute *route, cchar *uri, int lifesecs, int flags)
{
    httpAddCache(route, NULL, uri, NULL, NULL, 0, lifesecs * TPS, flags);
    return 0;
}


PUBLIC cchar *espCreateSession(HttpConn *conn)
{
    HttpSession *session;

    if ((session = httpCreateSession(getConn())) != 0) {
        return session->id;
    }
    return 0;
}


PUBLIC void espDefineAction(HttpRoute *route, cchar *target, void *callback)
{
    EspRoute    *eroute;
    char        *action, *controller;

    assert(route);
    assert(target && *target);
    assert(callback);

    eroute = ((EspRoute*) route->eroute)->top;
    if (target) {
#if DEPRECATED || 1 
        /* 
            Keep till version 6
         */
        if (scontains(target, "-cmd-")) {
            target = sreplace(target, "-cmd-", "/");
        } else if (schr(target, '-')) {
            controller = ssplit(sclone(target), "-", (char**) &action);
            target = sjoin(controller, "/", action, NULL);
        }
#endif
        if (!eroute->actions) {
            eroute->actions = mprCreateHash(-1, MPR_HASH_STATIC_VALUES);
        }
        mprAddKey(eroute->actions, target, callback);
    }
}


/*
    The base procedure is invoked prior to calling any and all actions on this route
 */
PUBLIC void espDefineBase(HttpRoute *route, EspProc baseProc)
{
    HttpRoute   *rp;
    EspRoute    *eroute;
    int         next;

    for (ITERATE_ITEMS(route->host->routes, rp, next)) {
        if ((eroute = rp->eroute) != 0) {
            if (smatch(httpGetDir(rp, "CONTROLLERS"), httpGetDir(route, "CONTROLLERS"))) {
                eroute->commonController = baseProc;
            }
        }
    }
}


/*
    Path should be a relative path from route->documents to the view file (relative-path.esp)
 */
PUBLIC void espDefineView(HttpRoute *route, cchar *path, void *view)
{
    EspRoute    *eroute;

    assert(path && *path);
    assert(view);

    if (route->eroute) {
        eroute = ((EspRoute*) route->eroute)->top;
    } else {
        if ((eroute = espRoute(route)) == 0) {
            /* Should never happen */
            return;
        }
    }
    eroute = eroute->top;
    if (route) {
        path = mprGetPortablePath(path);
    }
    if (!eroute->views) {
        eroute->views = mprCreateHash(-1, MPR_HASH_STATIC_VALUES);
    }
    mprAddKey(eroute->views, path, view);
}


PUBLIC void espDestroySession(HttpConn *conn)
{
    httpDestroySession(conn);
}


PUBLIC void espFinalize(HttpConn *conn)
{
    httpFinalize(conn);
}


PUBLIC void espFlush(HttpConn *conn)
{
    httpFlush(conn);
}


PUBLIC HttpAuth *espGetAuth(HttpConn *conn)
{
    return conn->rx->route->auth;
}


PUBLIC cchar *espGetConfig(HttpRoute *route, cchar *key, cchar *defaultValue)
{
    cchar       *value;

    if (sstarts(key, "app.")) {
        mprLog("warn esp", 0, "Using legacy \"app\" configuration property");
    }
    if ((value = mprGetJson(route->config, key)) != 0) {
        return value;
    }
    return defaultValue;
}


PUBLIC MprOff espGetContentLength(HttpConn *conn)
{
    return httpGetContentLength(conn);
}


PUBLIC cchar *espGetContentType(HttpConn *conn)
{
    return conn->rx->mimeType;
}


PUBLIC cchar *espGetCookie(HttpConn *conn, cchar *name)
{
    return httpGetCookie(conn, name);
}


PUBLIC cchar *espGetCookies(HttpConn *conn)
{
    return httpGetCookies(conn);
}


PUBLIC void *espGetData(HttpConn *conn)
{
    EspReq  *req;

    req = conn->reqData;
    return req->data;
}


PUBLIC Edi *espGetDatabase(HttpConn *conn)
{
    HttpRx      *rx;
    EspReq      *req;
    EspRoute    *eroute;
    Edi         *edi;

    rx = conn->rx;
    req = conn->reqData;
    edi = req ? req->edi : 0;
    if (edi == 0 && rx && rx->route) {
        if ((eroute = rx->route->eroute) != 0) {
            edi = eroute->edi;
        }
    }
    if (edi == 0) {
        httpError(conn, 0, "Cannot get database instance");
        return 0;
    }
    return edi;
}


PUBLIC cchar *espGetDocuments(HttpConn *conn)
{
    return conn->rx->route->documents;
}


PUBLIC EspRoute *espGetEspRoute(HttpConn *conn)
{
    return conn->rx->route->eroute;
}


PUBLIC cchar *espGetFeedback(HttpConn *conn, cchar *kind)
{
    EspReq      *req;
    MprKey      *kp;
    cchar       *msg;

    req = conn->reqData;
    if (kind == 0 || req == 0 || req->feedback == 0 || mprGetHashLength(req->feedback) == 0) {
        return 0;
    }
    for (kp = 0; (kp = mprGetNextKey(req->feedback, kp)) != 0; ) {
        msg = kp->data;
        //  DEPRECATE "all"
        if (smatch(kind, kp->key) || smatch(kind, "all") || smatch(kind, "*")) {
            return msg;
        }
    }
    return 0;
}


PUBLIC EdiGrid *espGetGrid(HttpConn *conn)
{
    return conn->grid;
}


PUBLIC cchar *espGetHeader(HttpConn *conn, cchar *key)
{
    return httpGetHeader(conn, key);
}


PUBLIC MprHash *espGetHeaderHash(HttpConn *conn)
{
    return httpGetHeaderHash(conn);
}


PUBLIC char *espGetHeaders(HttpConn *conn)
{
    return httpGetHeaders(conn);
}


PUBLIC int espGetIntParam(HttpConn *conn, cchar *var, int defaultValue)
{
    return httpGetIntParam(conn, var, defaultValue);
}


PUBLIC cchar *espGetMethod(HttpConn *conn)
{
    return conn->rx->method;
}


PUBLIC cchar *espGetParam(HttpConn *conn, cchar *var, cchar *defaultValue)
{
    return httpGetParam(conn, var, defaultValue);
}


PUBLIC MprJson *espGetParams(HttpConn *conn)
{
    return httpGetParams(conn);
}


PUBLIC cchar *espGetPath(HttpConn *conn)
{
    return conn->rx->pathInfo;
}


PUBLIC cchar *espGetQueryString(HttpConn *conn)
{
    return httpGetQueryString(conn);
}


PUBLIC char *espGetReferrer(HttpConn *conn)
{
    if (conn->rx->referrer) {
        return conn->rx->referrer;
    }
    return httpLink(conn, "~");
}


PUBLIC HttpRoute *espGetRoute(HttpConn *conn)
{
    return conn->rx->route;
}


PUBLIC Edi *espGetRouteDatabase(HttpRoute *route)
{
    EspRoute    *eroute;

    eroute = route->eroute;
    if (eroute == 0 || eroute->edi == 0) {
        return 0;
    }
    return eroute->edi;
}


PUBLIC cchar *espGetRouteVar(HttpConn *conn, cchar *var)
{
    return httpGetRouteVar(conn->rx->route, var);
}


PUBLIC cchar *espGetSessionID(HttpConn *conn, int create)
{
    HttpSession *session;

    if ((session = httpGetSession(getConn(), create)) != 0) {
        return session->id;
    }
    return 0;
}


PUBLIC int espGetStatus(HttpConn *conn)
{
    return httpGetStatus(conn);
}


PUBLIC char *espGetStatusMessage(HttpConn *conn)
{
    return httpGetStatusMessage(conn);
}


PUBLIC MprList *espGetUploads(HttpConn *conn)
{
    return conn->rx->files;
}


PUBLIC cchar *espGetUri(HttpConn *conn)
{
    return conn->rx->uri;
}


#if DEPRECATED || 1

PUBLIC bool espHasPak(HttpRoute *route, cchar *name)
{
    return mprGetJsonObj(route->config, sfmt("dependencies.%s", name)) != 0;
}
#endif


PUBLIC bool espHasGrid(HttpConn *conn)
{
    return conn->grid != 0;
}


PUBLIC bool espHasRec(HttpConn *conn)
{
    EdiRec  *rec;

    rec = conn->record;
    return (rec && rec->id) ? 1 : 0;
}


PUBLIC bool espIsEof(HttpConn *conn)
{
    return httpIsEof(conn);
}


PUBLIC bool espIsFinalized(HttpConn *conn)
{
    return httpIsFinalized(conn);
}


PUBLIC bool espIsSecure(HttpConn *conn)
{
    return conn->secure;
}


PUBLIC bool espMatchParam(HttpConn *conn, cchar *var, cchar *value)
{
    return httpMatchParam(conn, var, value);
}


/*
    Read rx data in non-blocking mode. Use standard connection timeouts.
 */
PUBLIC ssize espReceive(HttpConn *conn, char *buf, ssize len)
{
    return httpRead(conn, buf, len);
}


PUBLIC void espRedirect(HttpConn *conn, int status, cchar *target)
{
    httpRedirect(conn, status, target);
}


PUBLIC void espRedirectBack(HttpConn *conn)
{
    if (conn->rx->referrer) {
        espRedirect(conn, HTTP_CODE_MOVED_TEMPORARILY, conn->rx->referrer);
    }
}


PUBLIC ssize espRender(HttpConn *conn, cchar *fmt, ...)
{
    va_list     vargs;
    char        *buf;

    va_start(vargs, fmt);
    buf = sfmtv(fmt, vargs);
    va_end(vargs);
    return espRenderString(conn, buf);
}


PUBLIC ssize espRenderBlock(HttpConn *conn, cchar *buf, ssize size)
{
    return httpWriteBlock(conn->writeq, buf, size, HTTP_BUFFER);
}


PUBLIC ssize espRenderCached(HttpConn *conn)
{
    return httpWriteCached(conn);
}


static void copyMappings(HttpRoute *route, MprJson *dest, MprJson *obj)
{
    MprJson     *child, *job, *jvalue;
    cchar       *key, *value;
    int         ji;

    for (ITERATE_CONFIG(route, obj, child, ji)) {
        if (child->type & MPR_JSON_OBJ) {
            job = mprCreateJson(MPR_JSON_OBJ);
            copyMappings(route, job, child);
            mprSetJsonObj(dest, child->name, job);
        } else {
            key = child->value;
            if (sends(key, "|time")) {
                key = ssplit(sclone(key), " \t|", NULL);
                if ((value = mprGetJson(route->config, key)) != 0) {
                    mprSetJson(dest, child->name, itos(httpGetTicks(value)), MPR_JSON_NUMBER);
                }
            } else {
                if ((jvalue = mprGetJsonObj(route->config, key)) != 0) {
                    mprSetJsonObj(dest, child->name, mprCloneJson(jvalue));
                }
            }
        }
    }
}


static cchar *getClientConfig(HttpConn *conn)
{
    HttpRoute   *route;
    MprJson     *mappings, *obj;

    conn = getConn();
    for (route = conn->rx->route; route; route = route->parent) {
        if (route->clientConfig) {
            return route->clientConfig;
        }
    }
    route = conn->rx->route;
    if ((obj = mprGetJsonObj(route->config, "esp.mappings")) != 0) {
        mappings = mprCreateJson(MPR_JSON_OBJ);
        copyMappings(route, mappings, obj);
        mprWriteJson(mappings, "prefix", route->prefix, 0);
        route->clientConfig = mprJsonToString(mappings, MPR_JSON_QUOTES);
    }
    return route->clientConfig;
}


PUBLIC ssize espRenderConfig(HttpConn *conn)
{
    cchar       *config;

    if ((config = getClientConfig(conn)) != 0) {
        return renderString(config);
    }
    return 0;
}


PUBLIC ssize espRenderError(HttpConn *conn, int status, cchar *fmt, ...)
{
    va_list     args;
    HttpRx      *rx;
    ssize       written;
    cchar       *msg, *title, *text;

    va_start(args, fmt);

    rx = conn->rx;
    if (rx->route->json) {
        mprLog("warn esp", 0, "Calling espRenderFeedback in JSON app");
        return 0 ;
    }
    written = 0;

    if (!httpIsFinalized(conn)) {
        if (status == 0) {
            status = HTTP_CODE_INTERNAL_SERVER_ERROR;
        }
        title = sfmt("Request Error for \"%s\"", rx->pathInfo);
        msg = mprEscapeHtml(sfmtv(fmt, args));
        if (rx->route->flags & HTTP_ROUTE_SHOW_ERRORS) {
            text = sfmt(\
                "<!DOCTYPE html>\r\n<html>\r\n<head><title>%s</title></head>\r\n" \
                "<body>\r\n<h1>%s</h1>\r\n" \
                "    <pre>%s</pre>\r\n" \
                "    <p>To prevent errors being displayed in the browser, " \
                "       set <b>ShowErrors off</b> in the appweb.conf file.</p>\r\n" \
                "</body>\r\n</html>\r\n", title, title, msg);
            httpSetContentType(conn, "text/html");
            written += espRenderString(conn, text);
            espFinalize(conn);
            httpTrace(conn, "esp.error", "error", "msg=\"%s\", status=%d, uri=\"%s\"", msg, status, rx->pathInfo);
        }
    }
    va_end(args);
    return written;
}


PUBLIC ssize espRenderFile(HttpConn *conn, cchar *path)
{
    MprFile     *from;
    ssize       count, written, nbytes;
    char        buf[ME_MAX_BUFFER];

    if ((from = mprOpenFile(path, O_RDONLY | O_BINARY, 0)) == 0) {
        return MPR_ERR_CANT_OPEN;
    }
    written = 0;
    while ((count = mprReadFile(from, buf, sizeof(buf))) > 0) {
        if ((nbytes = espRenderBlock(conn, buf, count)) < 0) {
            return nbytes;
        }
        written += nbytes;
    }
    mprCloseFile(from);
    return written;
}


PUBLIC ssize espRenderFeedback(HttpConn *conn, cchar *kinds)
{
    EspReq      *req;
    MprKey      *kp;
    cchar       *msg;
    ssize       written;

    req = conn->reqData;
    if (req->route->json) {
        mprLog("warn esp", 0, "Calling espRenderFeedback in JSON app");
        return 0;
    }
    if (kinds == 0 || req->feedback == 0 || mprGetHashLength(req->feedback) == 0) {
        return 0;
    }
    written = 0;
    for (kp = 0; (kp = mprGetNextKey(req->feedback, kp)) != 0; ) {
        msg = kp->data;
        //  DEPRECATE "all"
        if (strstr(kinds, kp->key) || strstr(kinds, "all") || strstr(kinds, "*")) {
            written += espRender(conn, "<span class='feedback-%s animate'>%s</span>", kp->key, msg);
        }
    }
    return written;
}


PUBLIC ssize espRenderSafe(HttpConn *conn, cchar *fmt, ...)
{
    va_list     args;
    cchar       *s;

    va_start(args, fmt);
    s = mprEscapeHtml(sfmtv(fmt, args));
    va_end(args);
    return espRenderBlock(conn, s, slen(s));
}


PUBLIC ssize espRenderSafeString(HttpConn *conn, cchar *s)
{
    s = mprEscapeHtml(s);
    return espRenderBlock(conn, s, slen(s));
}


PUBLIC ssize espRenderString(HttpConn *conn, cchar *s)
{
    return espRenderBlock(conn, s, slen(s));
}


/*
    Render a request variable. If a param by the given name is not found, consult the session.
 */
PUBLIC ssize espRenderVar(HttpConn *conn, cchar *name)
{
    cchar   *value;

    if ((value = espGetParam(conn, name, 0)) == 0) {
        value = httpGetSessionVar(conn, name, "");
    }
    return espRenderSafeString(conn, value);
}


PUBLIC int espRemoveHeader(HttpConn *conn, cchar *key)
{
    assert(key && *key);
    if (conn->tx == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    return mprRemoveKey(conn->tx->headers, key);
}


PUBLIC void espRemoveSessionVar(HttpConn *conn, cchar *var)
{
    httpRemoveSessionVar(conn, var);
}


PUBLIC void espRemoveCookie(HttpConn *conn, cchar *name)
{
    HttpRoute   *route;
    cchar       *url;

    route = conn->rx->route;
    url = (route->prefix && *route->prefix) ? route->prefix : "/";
    httpSetCookie(conn, name, "", url, NULL, 0, 0);
}


PUBLIC void espSetConn(HttpConn *conn)
{
    mprSetThreadData(((Esp*) MPR->espService)->local, conn);
}


static void espNotifier(HttpConn *conn, int event, int arg)
{
    EspReq      *req;

    if ((req = conn->reqData) != 0) {
        espSetConn(conn);
        (req->notifier)(conn, event, arg);
    }
}


PUBLIC void espSetNotifier(HttpConn *conn, HttpNotifier notifier)
{
    EspReq      *req;

    if ((req = conn->reqData) != 0) {
        req->notifier = notifier;
        httpSetConnNotifier(conn, espNotifier);
    }
}


#if DEPRECATED || 1
PUBLIC int espSaveConfig(HttpRoute *route)
{
    cchar       *path;

    path = mprJoinPath(route->home, "esp.json");
#if KEEP
    mprBackupLog(path, 3);
#endif
    return mprSaveJson(route->config, path, MPR_JSON_PRETTY | MPR_JSON_QUOTES);
}
#endif


PUBLIC ssize espSendGrid(HttpConn *conn, EdiGrid *grid, int flags)
{
    if (conn->rx->route->json) {
        httpSetContentType(conn, "application/json");
        if (grid) {
            return espRender(conn, "{\n  \"data\": %s, \"schema\": %s}\n", ediGridAsJson(grid, flags), 
                ediGetGridSchemaAsJson(grid));
        }
        return espRender(conn, "{}");
    }
    return 0;
}


PUBLIC ssize espSendRec(HttpConn *conn, EdiRec *rec, int flags)
{
    if (conn->rx->route->json) {
        httpSetContentType(conn, "application/json");
        if (rec) {
            return espRender(conn, "{\n  \"data\": %s, \"schema\": %s}\n", 
                ediRecAsJson(rec, flags), ediGetRecSchemaAsJson(rec));
        }
        return espRender(conn, "{}");
    }
    return 0;
}


PUBLIC ssize espSendResult(HttpConn *conn, bool success)
{
    EspReq      *req;
    EdiRec      *rec;
    ssize       written;

    req = conn->reqData;
    written = 0;
    if (req->route->json) {
        rec = getRec();
        if (rec && rec->errors) {
            written = espRender(conn, "{\"error\": %d, \"feedback\": %s, \"fieldErrors\": %s}", !success,
                req->feedback ? mprSerialize(req->feedback, MPR_JSON_QUOTES) : "{}",
                mprSerialize(rec->errors, MPR_JSON_QUOTES));
        } else {
            written = espRender(conn, "{\"error\": %d, \"feedback\": %s}", !success,
                req->feedback ? mprSerialize(req->feedback, MPR_JSON_QUOTES) : "{}");
        }
        espFinalize(conn);
    } else {
        /* Noop */
    }
    return written;
}


PUBLIC bool espSetAutoFinalizing(HttpConn *conn, bool on)
{
    EspReq  *req;
    bool    old;

    req = conn->reqData;
    old = req->autoFinalize;
    req->autoFinalize = on;
    return old;
}


PUBLIC int espSetConfig(HttpRoute *route, cchar *key, cchar *value)
{
    return mprSetJson(route->config, key, value, 0);
}


PUBLIC void espSetContentLength(HttpConn *conn, MprOff length)
{
    httpSetContentLength(conn, length);
}


PUBLIC void espSetCookie(HttpConn *conn, cchar *name, cchar *value, cchar *path, cchar *cookieDomain, MprTicks lifespan,
        bool isSecure)
{
    httpSetCookie(conn, name, value, path, cookieDomain, lifespan, isSecure);
}


PUBLIC void espSetContentType(HttpConn *conn, cchar *mimeType)
{
    httpSetContentType(conn, mimeType);
}


PUBLIC void espSetData(HttpConn *conn, void *data)
{
    EspReq  *req;

    req = conn->reqData;
    req->data = data;
}


PUBLIC void espSetFeedback(HttpConn *conn, cchar *kind, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    espSetFeedbackv(conn, kind, fmt, args);
    va_end(args);
}


PUBLIC void espSetFeedbackv(HttpConn *conn, cchar *kind, cchar *fmt, va_list args)
{
    EspReq      *req;
    cchar       *msg;

    if ((req = conn->reqData) == 0) {
        return;
    }
    if (!req->route->json) {
        /*
            Create a session as early as possible so a Set-Cookie header can be omitted.
         */
        httpGetSession(conn, 1);
    }
    if (req->feedback == 0) {
        req->feedback = mprCreateHash(0, MPR_HASH_STABLE);
    }
    msg = sfmtv(fmt, args);

#if KEEP
    MprKey      *current, *last;
    if ((current = mprLookupKeyEntry(req->feedback, kind)) != 0) {
        if ((last = mprLookupKey(req->lastFeedback, current->key)) != 0 && current->data == last->data) {
            /* Overwrite prior feedback messages */
            mprAddKey(req->feedback, kind, msg);
        } else {
            /* Append to existing feedback messages */
            mprAddKey(req->feedback, kind, sjoin(current->data, ", ", msg, NULL));
        }
    } else
#endif
    mprAddKey(req->feedback, kind, msg);
}


#if DEPRECATED || 1
PUBLIC void espSetFlash(HttpConn *conn, cchar *kind, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    espSetFeedbackv(conn, kind, fmt, args);
    va_end(args);
}
#endif


PUBLIC EdiGrid *espSetGrid(HttpConn *conn, EdiGrid *grid)
{
    return conn->grid = grid;
}


/*
    Set a http header. Overwrite if present.
 */
PUBLIC void espSetHeader(HttpConn *conn, cchar *key, cchar *fmt, ...)
{
    va_list     vargs;

    assert(key && *key);
    assert(fmt && *fmt);

    va_start(vargs, fmt);
    httpSetHeaderString(conn, key, sfmtv(fmt, vargs));
    va_end(vargs);
}


PUBLIC void espSetHeaderString(HttpConn *conn, cchar *key, cchar *value)
{
    httpSetHeaderString(conn, key, value);
}


PUBLIC void espSetIntParam(HttpConn *conn, cchar *var, int value)
{
    httpSetIntParam(conn, var, value);
}


PUBLIC void espSetParam(HttpConn *conn, cchar *var, cchar *value)
{
    httpSetParam(conn, var, value);
}


PUBLIC EdiRec *espSetRec(HttpConn *conn, EdiRec *rec)
{
    return conn->record = rec;
}


PUBLIC int espSetSessionVar(HttpConn *conn, cchar *var, cchar *value)
{
    return httpSetSessionVar(conn, var, value);
}


PUBLIC void espSetStatus(HttpConn *conn, int status)
{
    httpSetStatus(conn, status);
}


PUBLIC void espShowRequest(HttpConn *conn)
{
    MprHash     *env;
    MprJson     *params, *param;
    MprKey      *kp;
    MprJson     *jkey;
    HttpRx      *rx;
    int         i;

    rx = conn->rx;
    httpAddHeaderString(conn, "Cache-Control", "no-cache");
    httpCreateCGIParams(conn);
    espRender(conn, "\r\n");

    /*
        Query
     */
    for (ITERATE_JSON(rx->params, jkey, i)) {
        espRender(conn, "PARAMS %s=%s\r\n", jkey->name, jkey->value ? jkey->value : "null");
    }
    espRender(conn, "\r\n");

    /*
        Http Headers
     */
    env = espGetHeaderHash(conn);
    for (ITERATE_KEYS(env, kp)) {
        espRender(conn, "HEADER %s=%s\r\n", kp->key, kp->data ? kp->data: "null");
    }
    espRender(conn, "\r\n");

    /*
        Server vars
     */
    for (ITERATE_KEYS(conn->rx->svars, kp)) {
        espRender(conn, "SERVER %s=%s\r\n", kp->key, kp->data ? kp->data: "null");
    }
    espRender(conn, "\r\n");

    /*
        Form vars
     */
    if ((params = espGetParams(conn)) != 0) {
        for (ITERATE_JSON(params, param, i)) {
            espRender(conn, "FORM %s=%s\r\n", param->name, param->value);
        }
        espRender(conn, "\r\n");
    }

#if KEEP
    /*
        Body
     */
    q = conn->readq;
    if (q->first && rx->bytesRead > 0 && scmp(rx->mimeType, "application/x-www-form-urlencoded") == 0) {
        buf = q->first->content;
        mprAddNullToBuf(buf);
        if ((numKeys = getParams(&keys, mprGetBufStart(buf), (int) mprGetBufLength(buf))) > 0) {
            for (i = 0; i < (numKeys * 2); i += 2) {
                value = keys[i+1];
                espRender(conn, "BODY %s=%s\r\n", keys[i], value ? value: "null");
            }
        }
        espRender(conn, "\r\n");
    }
#endif
}


PUBLIC bool espTestConfig(HttpRoute *route, cchar *key, cchar *desired)
{
    cchar       *value;

    if ((value = mprGetJson(route->config, key)) != 0) {
        return smatch(value, desired);
    }
    return 0;
}


PUBLIC void espUpdateCache(HttpConn *conn, cchar *uri, cchar *data, int lifesecs)
{
    httpUpdateCache(conn, uri, data, lifesecs * TPS);
}


PUBLIC cchar *espUri(HttpConn *conn, cchar *target)
{
    return httpLink(conn, target);
}


PUBLIC int espEmail(HttpConn *conn, cchar *to, cchar *from, cchar *subject, MprTime date, cchar *mime, 
    cchar *message, MprList *files)
{
    MprList         *lines;
    MprCmd          *cmd;
    cchar           *body, *boundary, *contents, *encoded, *file;
    char            *out, *err;
    ssize           length;
    int             i, next, status;

    if (!from || !*from) {
        from = "anonymous";
    }
    if (!subject || !*subject) {
        subject = "Mail message";
    }
    if (!mime || !*mime) {
        mime = "text/plain";
    }
    if (!date) {
        date = mprGetTime();
    }
    boundary = sjoin("esp.mail=", mprGetMD5("BOUNDARY"), NULL);
    lines = mprCreateList(0, 0);

    mprAddItem(lines, sfmt("To: %s", to));
    mprAddItem(lines, sfmt("From: %s", from));
    mprAddItem(lines, sfmt("Date: %s", mprFormatLocalTime(0, date)));
    mprAddItem(lines, sfmt("Subject: %s", subject));
    mprAddItem(lines, "MIME-Version: 1.0");
    mprAddItem(lines, sfmt("Content-Type: multipart/mixed; boundary=%s", boundary));
    mprAddItem(lines, "");

    boundary = sjoin("--", boundary, NULL);

    mprAddItem(lines, boundary);
    mprAddItem(lines, sfmt("Content-Type: %s", mime));
    mprAddItem(lines, "");
    mprAddItem(lines, "");
    mprAddItem(lines, message);

    for (ITERATE_ITEMS(files, file, next)) {
        mprAddItem(lines, boundary);
        if ((mime = mprLookupMime(NULL, file)) == 0) {
            mime = "application/octet-stream";
        }
        mprAddItem(lines, "Content-Transfer-Encoding: base64");
        mprAddItem(lines, sfmt("Content-Disposition: inline; filename=\"%s\"", mprGetPathBase(file)));
        mprAddItem(lines, sfmt("Content-Type: %s; name=\"%s\"", mime, mprGetPathBase(file)));
        mprAddItem(lines, "");
        contents = mprReadPathContents(file, &length);
        encoded = mprEncode64Block(contents, length);
        for (i = 0; i < length; i += 76) {
            mprAddItem(lines, snclone(&encoded[i], i + 76));
        }
    }
    mprAddItem(lines, sfmt("%s--", boundary));

    body = mprListToString(lines, "\n");
    httpTraceContent(conn, "esp.email", "context", body, slen(body), 0);

    cmd = mprCreateCmd(conn->dispatcher);
    if (mprRunCmd(cmd, "sendmail -t", NULL, body, &out, &err, -1, 0) < 0) {
        mprDestroyCmd(cmd);
        return MPR_ERR_CANT_OPEN;
    }
    if (mprWaitForCmd(cmd, ME_ESP_EMAIL_TIMEOUT) < 0) {
        httpTrace(conn, "esp.email.error", "error", 
            "msg=\"Timeout waiting for command to complete\", timeout=%d, command=\"%s\"",
            ME_ESP_EMAIL_TIMEOUT, cmd->argv[0]);
        mprDestroyCmd(cmd);
        return MPR_ERR_CANT_COMPLETE;
    }
    if ((status = mprGetCmdExitStatus(cmd)) != 0) {
        httpTrace(conn, "esp.email.error", "error", "msg=\"Sendmail failed\", status=%d, error=\"%s\"", status, err);
        mprDestroyCmd(cmd);
        return MPR_ERR_CANT_WRITE;
    }
    mprDestroyCmd(cmd);
    return 0;
}


PUBLIC void espClearCurrentSession(HttpConn *conn)
{
    EspRoute    *eroute;

    eroute = conn->rx->route->eroute;
    if (eroute->currentSession) {
        httpTrace(conn, "esp.singular.clear", "context", "session=%s", eroute->currentSession);
    }
    eroute->currentSession = 0;
}


/*
    Remember this connections session as the current session. Use for single login tracking.
 */
PUBLIC void espSetCurrentSession(HttpConn *conn)
{
    EspRoute    *eroute;

    eroute = conn->rx->route->eroute;
    eroute->currentSession = httpGetSessionID(conn);
    httpTrace(conn, "esp.singular.set", "context", "msg=\"Set singluar user\", session=%s", eroute->currentSession);
}


/*
    Test if this connection is the current session. Use for single login tracking.
 */
PUBLIC bool espIsCurrentSession(HttpConn *conn)
{
    EspRoute    *eroute;

    eroute = conn->rx->route->eroute;
    if (eroute->currentSession) {
        if (smatch(httpGetSessionID(conn), eroute->currentSession)) {
            return 1;
        }
        if (httpLookupSessionID(eroute->currentSession)) {
            /* Session is still current */
            return 0;
        }
        /* Session has expired */
        eroute->currentSession = 0;
    }
    return 1;
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

/*
    espHtml.c -- ESP HTML controls 

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/




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

/*
    espRequest.c -- ESP Request handler

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************* Local **********************************/
/*
    Singleton ESP control structure
 */
static Esp *esp;

/*
    UNUSED. espRenderView flags are reserved
 */
#define ESP_DONT_RENDER 0x1

/************************************ Forward *********************************/

static int cloneDatabase(HttpConn *conn);
static void closeEsp(HttpQueue *q);
static void ifConfigModified(HttpRoute *route, cchar *path, bool *modified);
static void manageEsp(Esp *esp, int flags);
static void manageReq(EspReq *req, int flags);
static int openEsp(HttpQueue *q);
static int runAction(HttpConn *conn);
static void startEsp(HttpQueue *q);
static int unloadEsp(MprModule *mp);

#if !ME_STATIC
static cchar *getCacheName(HttpRoute *route, cchar *kind, cchar *source);
static cchar *getModuleName(HttpRoute *route, cchar *kind, cchar *target);
static char *getModuleEntry(EspRoute *eroute, cchar *kind, cchar *source, cchar *cacheName);
static bool layoutIsStale(EspRoute *eroute, cchar *source, cchar *module);
#endif

/************************************* Code ***********************************/
/*
    Load and initialize ESP module. Manually loaded when used inside esp.c.
 */
PUBLIC int espOpen(MprModule *module)
{
    HttpStage   *handler;

    if ((handler = httpCreateHandler("espHandler", module)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    HTTP->espHandler = handler;
    handler->open = openEsp;
    handler->close = closeEsp;
    handler->start = startEsp;

    /* 
        Using the standard 'incoming' callback that simply transfers input to the queue head 
        Applications should read by defining a notifier for READABLE events and then calling httpGetPacket
        on the read queue.
     */
    if ((esp = mprAllocObj(Esp, manageEsp)) == 0) {
        return MPR_ERR_MEMORY;
    }
    MPR->espService = esp;
    handler->stageData = esp;
    esp->mutex = mprCreateLock();
    esp->local = mprCreateThreadLocal();
    if (espInitParser() < 0) {
        return 0;
    }
    if ((esp->ediService = ediCreateService()) == 0) {
        return 0;
    }
#if ME_COM_MDB
    mdbInit();
#endif
#if ME_COM_SQLITE
    sdbInit();
#endif
    if (module) {
        mprSetModuleFinalizer(module, unloadEsp);
    }
    return 0;
}


static int unloadEsp(MprModule *mp)
{
    HttpStage   *stage;

    if (esp->inUse) {
       return MPR_ERR_BUSY;
    }
    if (mprIsStopping()) {
        return 0;
    }
    if ((stage = httpLookupStage(mp->name)) != 0) {
        stage->flags |= HTTP_STAGE_UNLOADED;
    }
    return 0;
}


/*
    Open an instance of the ESP for a new request
 */
static int openEsp(HttpQueue *q)
{
    HttpConn    *conn;
    HttpRx      *rx;
    HttpRoute   *rp, *route;
    EspRoute    *eroute;
    EspReq      *req;

    conn = q->conn;
    rx = conn->rx;
    route = rx->route;

    if ((req = mprAllocObj(EspReq, manageReq)) == 0) {
        httpMemoryError(conn);
        return MPR_ERR_MEMORY;
    }
    conn->reqData = req;

    /*
        If unloading a module, this lock will cause a wait here while ESP applications are reloaded.
     */
    lock(esp);
    esp->inUse++;
    unlock(esp);

    /*
        Find the ESP route configuration. Search up the route parent chain.
     */
    for (eroute = 0, rp = route; rp; rp = rp->parent) {
        if (rp->eroute) {
            eroute = rp->eroute;
            break;
        }
    }
    if (!eroute) {
        /* WARNING: may yield */
        if (espInit(route, 0, "esp.json") < 0) {
            return MPR_ERR_CANT_INITIALIZE;
        }
        eroute = route->eroute;
    } else {
        route->eroute = eroute;
    }
    req->esp = esp;
    req->route = route;
    req->autoFinalize = 1;

    /*
        If a cookie is not explicitly set, use the application name for the session cookie so that
        cookies are unique per esp application.
     */
    if (!route->cookie) {
        httpSetRouteCookie(route, sfmt("esp-%s", eroute->appName));
    }
    return 0;
}


static void closeEsp(HttpQueue *q)
{
    lock(esp);
    esp->inUse--;
    assert(esp->inUse >= 0);
    unlock(esp);
}


#if !ME_STATIC
/*
    This is called when unloading a view or controller module
    All of ESP must be quiesced.
 */
static bool espUnloadModule(cchar *module, MprTicks timeout)
{
    MprModule   *mp;
    MprTicks    mark;

    if ((mp = mprLookupModule(module)) != 0) {
        mark = mprGetTicks();
        esp->reloading = 1;
        do {
            lock(esp);
            /* Own request will count as 1 */
            if (esp->inUse <= 1) {
                if (mprUnloadModule(mp) < 0) {
                    mprLog("error esp", 0, "Cannot unload module %s", mp->name);
                    unlock(esp);
                }
                esp->reloading = 0;
                unlock(esp);
                return 1;
            }
            unlock(esp);
            mprSleep(10);

        } while (mprGetRemainingTicks(mark, timeout) > 0);
        esp->reloading = 0;
    }
    return 0;
}
#endif


/*
    Not used
 */
PUBLIC void espClearFeedback(HttpConn *conn)
{
    EspReq      *req;

    req = conn->reqData;
    req->feedback = 0;
}


static void setupFeedback(HttpConn *conn)
{
    EspReq      *req;

    req = conn->reqData;
    req->lastFeedback = 0;
    if (req->route->json) {
        req->feedback = mprCreateHash(0, MPR_HASH_STABLE);
    } else {
        if (httpGetSession(conn, 0)) {
            req->feedback = httpGetSessionObj(conn, ESP_FEEDBACK_VAR);
            if (req->feedback) {
                httpRemoveSessionVar(conn, ESP_FEEDBACK_VAR);
                req->lastFeedback = mprCloneHash(req->feedback);
            }
        }
    }
}


static void finalizeFeedback(HttpConn *conn)
{
    EspReq  *req;
    MprKey  *kp, *lp;

    req = conn->reqData;
    if (req->feedback) {
        if (req->route->json) {
            if (req->lastFeedback) {
                for (ITERATE_KEYS(req->feedback, kp)) {
                    if ((lp = mprLookupKeyEntry(req->lastFeedback, kp->key)) != 0 && kp->data == lp->data) {
                        mprRemoveKey(req->feedback, kp->key);
                    }
                }
            }
            if (mprGetHashLength(req->feedback) > 0) {
                /*
                    If the session does not exist, this will create one. However, must not have
                    emitted the headers, otherwise cannot inform the client of the session cookie.
                */
                httpSetSessionObj(conn, ESP_FEEDBACK_VAR, req->feedback);
            }
        }
    }
}


/*
    Start the request. At this stage, body data may not have been fully received unless
    the request is a form (POST method and content type is application/x-www-form-urlencoded).
    Forms are a special case and delay invoking the start callback until all body data is received.
    WARNING: GC yield
 */
static void startEsp(HttpQueue *q)
{
    HttpConn    *conn;
    HttpRx      *rx;
    EspReq      *req;

    conn = q->conn;
    rx = conn->rx;
    req = conn->reqData;

#if ME_WIN_LIKE
    rx->target = mprGetPortablePath(rx->target);
#endif

    if (req) {
        mprSetThreadData(req->esp->local, conn);
        /* WARNING: GC yield */
        if (runAction(conn)) {
            if (!conn->error && req->autoFinalize) {
                if (!conn->tx->responded) {
                    /* WARNING: GC yield */
                    espRenderDocument(conn, rx->target);
                }
                if (req->autoFinalize) {
                    espFinalize(conn);
                }
            }
        }
        finalizeFeedback(conn);
        mprSetThreadData(req->esp->local, NULL);
    }
}


static bool loadController(HttpConn *conn)
{
#if !ME_STATIC
    HttpRx      *rx;
    HttpRoute   *route;
    EspRoute    *eroute;
    cchar       *errMsg, *controllers, *controller;

    rx = conn->rx;
    route = rx->route;
    eroute = route->eroute;
    if (!eroute->combine && (eroute->update || !mprLookupKey(eroute->actions, rx->target))) {
        if ((controllers = httpGetDir(route, "CONTROLLERS")) == 0) {
            controllers = ".";
        }
        controllers = mprJoinPath(route->home, controllers);
        controller = schr(route->sourceName, '$') ? stemplateJson(route->sourceName, rx->params) : route->sourceName;
        controller = controllers ? mprJoinPath(controllers, controller) : mprJoinPath(route->home, controller);

        if (espLoadModule(route, conn->dispatcher, "controller", controller, &errMsg) < 0) {
            if (mprPathExists(controller, R_OK)) {
                httpError(conn, HTTP_CODE_NOT_FOUND, "%s", errMsg);
                return 0;
            }
        } else {
            httpTrace(conn, "esp.handler", "context", "msg: 'Load module %s'", controller);
        }
    }
#endif /* !ME_STATIC */
    return 1;
}


static bool setToken(HttpConn *conn)
{
    HttpRx      *rx;
    HttpRoute   *route;

    rx = conn->rx;
    route = rx->route;

    if (route->flags & HTTP_ROUTE_XSRF && !(rx->flags & HTTP_GET)) {
        if (!httpCheckSecurityToken(conn)) {
            httpSetStatus(conn, HTTP_CODE_UNAUTHORIZED);
            if (route->json) {
                httpTrace(conn, "esp.xsrf.error", "error", 0);
                espRenderString(conn,
                    "{\"retry\": true, \"success\": 0, \"feedback\": {\"error\": \"Security token is stale. Please retry.\"}}");
                espFinalize(conn);
            } else {
                httpError(conn, HTTP_CODE_UNAUTHORIZED, "Security token is stale. Please reload page.");
            }
            return 0;
        }
    }
    return 1;
}


/*
    Run an action (may yield)
 */
static int runAction(HttpConn *conn)
{
    HttpRx      *rx;
    HttpRoute   *route;
    EspRoute    *eroute;
    EspReq      *req;
    EspAction   action;

    rx = conn->rx;
    req = conn->reqData;
    route = rx->route;
    eroute = route->eroute;
    assert(eroute);

    if (eroute->edi && eroute->edi->flags & EDI_PRIVATE) {
        cloneDatabase(conn);
    } else {
        req->edi = eroute->edi;
    }
    if (route->sourceName == 0 || *route->sourceName == '\0') {
        if (eroute->commonController) {
            (eroute->commonController)(conn);
        }
        return 1;
    }
    if (!loadController(conn)) {
        return 0;
    }
    if (!setToken(conn)) {
        return 0;
    }
    httpAuthenticate(conn);
    if (eroute->commonController) {
        (eroute->commonController)(conn);
    }
    assert(eroute->top);
    action = mprLookupKey(eroute->top->actions, rx->target);
    if (action) {
        httpTrace(conn, "esp.handler", "context", "msg: 'Invoke controller action %s'", rx->target);
        setupFeedback(conn);
        if (!httpIsFinalized(conn)) {
            (action)(conn);
        }
    }
    return 1;
}


static cchar *loadView(HttpConn *conn, cchar *target)
{
#if !ME_STATIC
    HttpRx      *rx;
    HttpRoute   *route;
    EspRoute    *eroute;
    cchar       *errMsg, *path;

    rx = conn->rx;
    route = rx->route;
    eroute = route->eroute;
    assert(eroute);

    if (!eroute->combine && (eroute->update || !mprLookupKey(eroute->top->views, target))) {
        /* WARNING: GC yield - need target below */
        target = sclone(target);
        mprHold(target);
        path = mprJoinPath(route->documents, target);
        httpTrace(conn, "esp.handler", "context", "msg: 'Load module %s'", path);

        if (espLoadModule(route, conn->dispatcher, "view", path, &errMsg) < 0) {
            httpError(conn, HTTP_CODE_NOT_FOUND, "%s", errMsg);
            mprRelease(target);
            return 0;
        }
        mprRelease(target);
    }
#endif
    return target;
}

PUBLIC bool espRenderView(HttpConn *conn, cchar *target, int flags)
{
    HttpRx      *rx;
    HttpRoute   *route;
    EspRoute    *eroute;
    EspViewProc viewProc;

    rx = conn->rx;
    route = rx->route;
    eroute = route->eroute;

    if ((target = loadView(conn, target)) == 0) {
        return 0;
    }
    if ((viewProc = mprLookupKey(eroute->views, target)) == 0) {
        httpError(conn, HTTP_CODE_NOT_FOUND, "Cannot find function %s for %s",
            getCacheName(route, "view", mprJoinPath(route->documents, target)), target);
        return 0;
    }
    if (!(flags & ESP_DONT_RENDER)) {
        if (route->flags & HTTP_ROUTE_XSRF) {
            /* Add a new unique security token */
            httpAddSecurityToken(conn, 1);
        }
        httpSetContentType(conn, "text/html");
        httpSetFilename(conn, mprJoinPath(route->documents, target), 0);
        /* WARNING: may GC yield */
        (viewProc)(conn);
    }
    return 1;
}


/*
    Check if the target/filename.ext is registered as an esp view
 */
static cchar *checkView(HttpConn *conn, cchar *target, cchar *filename, cchar *ext)
{
    HttpRx      *rx;
    HttpRoute   *route;
    EspRoute    *eroute;

    assert(target);

    rx = conn->rx;
    route = rx->route;
    eroute = route->eroute;

    if (filename) {
        target = mprJoinPath(target, filename);
    }
    assert(target && *target);

    if (ext && *ext) {
        if (!smatch(mprGetPathExt(target), ext)) {
            target = sjoin(target, ".", ext, NULL);
        }
    }
    /*
        See if module already loaded for this view
     */
    if (mprLookupKey(eroute->views, target)) {
        return target;
    }

#if !ME_STATIC
{
    MprPath info;
    cchar   *module, *path;

    path = mprJoinPath(route->documents, target);

    if (!eroute->combine) {
        /*
            If target exists as a cached module that has not yet been loaded
            Note: source may not be present.
         */
        module = getModuleName(route, "view", path);
        if (mprGetPathInfo(module, &info) == 0 && !info.isDir) {
            return target;
        }
    }

    /*
        If target exists as a view (extension appended above)
     */
    if (mprGetPathInfo(path, &info) == 0 && !info.isDir) {
        return target;
    }

#if UNUSED
    /*
        If target exists as a mapped / compressed view
     */
    if (route->map && !(conn->tx->flags & HTTP_TX_NO_MAP)) {
        path = httpMapContent(conn, path);
        if (mprGetPathInfo(path, &info) == 0 && !info.isDir) {
            return target;
        }
    }
#endif

#if DEPRECATED || 1
    /*
        See if views are under client/app. Remove in version 6.
     */
    path = mprJoinPaths(route->documents, "app", target, NULL);
    if (mprGetPathInfo(path, &info) == 0 && !info.isDir) {
        return mprJoinPath("app", target);
    }
#endif
}
#endif
    return 0;
}


/*
    Render a document by mapping a URL target to a document. The target is interpreted relative to route->documents.
    If target + defined extension exists, serve that.
    If target is a directory with an index.esp, return the index.esp without a redirect.
    If target is a directory without a trailing "/" but with an index.esp, do an external redirect to "URI/".
    Otherwise relay to the fileHandler.
 */
PUBLIC void espRenderDocument(HttpConn *conn, cchar *target)
{
    HttpUri     *up;
    MprKey      *kp;
    cchar       *dest;

    if (!target) {
        httpError(conn, HTTP_CODE_NOT_FOUND, "Cannot find document");
        return;
    }
    if (*target) {
        for (ITERATE_KEYS(conn->rx->route->extensions, kp)) {
            if (kp->data == HTTP->espHandler && kp->key && kp->key[0]) {
                if ((dest = checkView(conn, target, 0, kp->key)) != 0) {
                    httpTrace(conn, "esp.handler", "context", "msg: 'Render view %s'", dest);
                    espRenderView(conn, dest, 0);
                    return;
                }
            }
        }
    }
    /*
        Check for index
     */
    if ((dest = checkView(conn, target, "index", "esp")) != 0) {
        /*
            Must do external redirect first if URL does not end with "/"
         */
        if (!sends(conn->rx->parsedUri->path, "/")) {
            up = conn->rx->parsedUri;
            httpRedirect(conn, HTTP_CODE_MOVED_PERMANENTLY, httpFormatUri(up->scheme, up->host,
                up->port, sjoin(up->path, "/", NULL), up->reference, up->query, 0));
            return;
        }
        httpTrace(conn, "esp.handler", "context", "msg: 'Render index %s'", dest);
        espRenderView(conn, dest, 0);
        return;
    }

#if UNUSED
    /*
        Last chance, forward to the file handler ... not an ESP request. This enables file requests within ESP routes.
     */
    path = mprJoinPath(route->documents, target);
    if (mprGetPathInfo(path, &info) == 0 && !info.isDir) {
        target = path;
    }
    /*
        If target exists as a mapped / compressed document
     */
    if (route->map && !(conn->tx->flags & HTTP_TX_NO_MAP)) {
        path = httpMapContent(conn, path);
        if (mprGetPathInfo(path, &info) == 0 && !info.isDir) {
            target = path;
        }
    }
#endif

    httpTrace(conn, "esp.handler", "context", "msg: 'Relay to the fileHandler'");
    conn->rx->target = &conn->rx->pathInfo[1];
    httpMapFile(conn);
    if (conn->tx->fileInfo.isDir) {
        httpHandleDirectory(conn);
    }
    if (!conn->tx->finalized) {
        httpSetFileHandler(conn, 0);
    }
}


/************************************ Support *********************************/
/*
    Create a per user session database clone. 
    Used for demos so one users updates to not change anothers view of the database.
 */
static void pruneDatabases(Esp *esp)
{
    MprKey      *kp;

    lock(esp);
    for (ITERATE_KEYS(esp->databases, kp)) {
        if (!httpLookupSessionID(kp->key)) {
            mprRemoveKey(esp->databases, kp->key);
            /* Restart scan */
            kp = 0;
        }
    }
    unlock(esp);
}


/*
    This clones a database to give a private view per user.
 */
static int cloneDatabase(HttpConn *conn)
{
    Esp         *esp;
    EspRoute    *eroute;
    EspReq      *req;
    cchar       *id;

    req = conn->reqData;
    eroute = conn->rx->route->eroute;
    assert(eroute->edi);
    assert(eroute->edi->flags & EDI_PRIVATE);

    esp = req->esp;
    if (!esp->databases) {
        lock(esp);
        if (!esp->databases) {
            esp->databases = mprCreateHash(0, 0);
            esp->databasesTimer = mprCreateTimerEvent(NULL, "esp-databases", 60 * 1000, pruneDatabases, esp, 0);
        }
        unlock(esp);
    }
    /*
        If the user is logging in or out, this will create a redundant session here.
     */
    httpGetSession(conn, 1);
    id = httpGetSessionID(conn);
    if ((req->edi = mprLookupKey(esp->databases, id)) == 0) {
        if ((req->edi = ediClone(eroute->edi)) == 0) {
            mprLog("error esp", 0, "Cannot clone database: %s", eroute->edi->path);
            return MPR_ERR_CANT_OPEN;
        }
        mprAddKey(esp->databases, id, req->edi);
    }
    return 0;
}


#if !ME_STATIC
static char *getModuleEntry(EspRoute *eroute, cchar *kind, cchar *source, cchar *cacheName)
{
    char    *cp, *entry;

    if (smatch(kind, "view")) {
        entry = sfmt("esp_%s", cacheName);

    } else if (smatch(kind, "app")) {
        if (eroute->combine) {
            entry = sfmt("esp_%s_%s_combine", kind, eroute->appName);
        } else {
            entry = sfmt("esp_%s_%s", kind, eroute->appName);
        }
    } else {
        /* Controller */
        if (eroute->appName) {
            entry = sfmt("esp_%s_%s_%s", kind, eroute->appName, mprTrimPathExt(mprGetPathBase(source)));
        } else {
            entry = sfmt("esp_%s_%s", kind, mprTrimPathExt(mprGetPathBase(source)));
        }
    }
    for (cp = entry; *cp; cp++) {
        if (!isalnum((uchar) *cp) && *cp != '_') {
            *cp = '_';
        }
    }
    return entry;
}


static cchar *getCacheName(HttpRoute *route, cchar *kind, cchar *target)
{
    EspRoute    *eroute;
    cchar       *appName, *canonical;

    eroute = route->eroute;
#if VXWORKS
    /*
        Trim the drive for VxWorks where simulated host drives only exist on the target
     */
    target = mprTrimPathDrive(target);
#endif
    canonical = mprGetPortablePath(mprGetRelPath(target, route->home));

    appName = eroute->appName;
    return eroute->combine ? appName : mprGetMD5WithPrefix(sfmt("%s:%s", appName, canonical), -1, sjoin(kind, "_", NULL));
}


static cchar *getModuleName(HttpRoute *route, cchar *kind, cchar *target)
{
    cchar   *cache, *cacheName;

    cacheName = getCacheName(route, "view", target);
    if ((cache = httpGetDir(route, "CACHE")) == 0) {
        /* May not be set for non esp apps */
        cache = "cache";
    }
    return mprJoinPathExt(mprJoinPaths(route->home, cache, cacheName, NULL), ME_SHOBJ);
}


/*
    WARNING: GC yield
 */
PUBLIC int espLoadModule(HttpRoute *route, MprDispatcher *dispatcher, cchar *kind, cchar *source, cchar **errMsg)
{
    EspRoute    *eroute;
    MprModule   *mp;
    cchar       *cache, *cacheName, *entry, *module;
    int         isView, recompile;

    eroute = route->eroute;
    *errMsg = "";

    cacheName = getCacheName(route, kind, source);
    if ((cache = httpGetDir(route, "CACHE")) == 0) {
        cache = "cache";
    }
    module = mprJoinPathExt(mprJoinPaths(route->home, cache, cacheName, NULL), ME_SHOBJ);

    lock(esp);
    if (mprLookupModule(source) == 0 || eroute->update) {
        if (eroute->compile && mprPathExists(source, R_OK)) {
            isView = smatch(kind, "view");
            if (espModuleIsStale(source, module, &recompile) || (isView && layoutIsStale(eroute, source, module))) {
                if (recompile) {
                    mprHoldBlocks(source, module, cacheName, NULL);
                    if (!espCompile(route, dispatcher, source, module, cacheName, isView, (char**) errMsg)) {
                        mprReleaseBlocks(source, module, cacheName, NULL);
                        unlock(esp);
                        return MPR_ERR_CANT_WRITE;
                    }
                    mprReleaseBlocks(source, module, cacheName, NULL);
                }
            }
        }
    }
    if (mprLookupModule(source) == 0) {
        if (!mprPathExists(module, R_OK)) {
            *errMsg = "Module does not exist";
            unlock(esp);
            return MPR_ERR_CANT_FIND;
        }
        entry = getModuleEntry(eroute, kind, source, cacheName);
        if ((mp = mprCreateModule(source, module, entry, route)) == 0) {
            *errMsg = "Memory allocation error loading module";
            unlock(esp);
            return MPR_ERR_MEMORY;
        }
        if (mprLoadModule(mp) < 0) {
            *errMsg = "Cannot load compiled esp module";
            unlock(esp);
            return MPR_ERR_CANT_READ;
        }
    }
    unlock(esp);
    return 0;
}


/*
    Test if a module has been updated (is stale).
    This will unload the module if it loaded but stale.
    Set recompile to true if the source is absent or more recent.
    Will return false if the source does not exist (important for testing layouts).
 */
PUBLIC bool espModuleIsStale(cchar *source, cchar *module, int *recompile)
{
    MprModule   *mp;
    MprPath     sinfo, minfo;

    *recompile = 0;
    mprGetPathInfo(module, &minfo);
    if (!minfo.valid) {
        if ((mp = mprLookupModule(source)) != 0) {
            if (!espUnloadModule(source, ME_ESP_RELOAD_TIMEOUT)) {
                mprLog("error esp", 0, "Cannot unload module %s. Connections still open. Continue using old version.",
                    source);
                return 0;
            }
        }
        *recompile = 1;
        mprLog("info esp", 4, "Source %s is newer than module %s, recompiling ...", source, module);
        return 1;
    }
    mprGetPathInfo(source, &sinfo);
    if (sinfo.valid && sinfo.mtime > minfo.mtime) {
        if ((mp = mprLookupModule(source)) != 0) {
            if (!espUnloadModule(source, ME_ESP_RELOAD_TIMEOUT)) {
                mprLog("warn esp", 4, "Cannot unload module %s. Connections still open. Continue using old version.",
                    source);
                return 0;
            }
        }
        *recompile = 1;
        mprLog("info esp", 4, "Source %s is newer than module %s, recompiling ...", source, module);
        return 1;
    }
    if ((mp = mprLookupModule(source)) != 0) {
        if (minfo.mtime > mp->modified) {
            /* Module file has been updated */
            if (!espUnloadModule(source, ME_ESP_RELOAD_TIMEOUT)) {
                mprLog("warn esp", 4, "Cannot unload module %s. Connections still open. Continue using old version.",
                    source);
                return 0;
            }
            mprLog("info esp", 4, "Module %s has been externally updated, reloading ...", module);
            return 1;
        }
    }
    /* Loaded module is current */
    return 0;
}


/*
    Check if the layout has changed. Returns false if the layout does not exist.
 */
static bool layoutIsStale(EspRoute *eroute, cchar *source, cchar *module)
{
    char    *data, *lpath, *quote;
    cchar   *layout, *layoutsDir;
    ssize   len;
    bool    stale;
    int     recompile;

    stale = 0;
    layoutsDir = httpGetDir(eroute->route, "LAYOUTS");
    if ((data = mprReadPathContents(source, &len)) != 0) {
        if ((lpath = scontains(data, "@ layout \"")) != 0) {
            lpath = strim(&lpath[10], " ", MPR_TRIM_BOTH);
            if ((quote = schr(lpath, '"')) != 0) {
                *quote = '\0';
            }
            layout = (layoutsDir && *lpath) ? mprJoinPath(layoutsDir, lpath) : 0;
        } else {
            layout = (layoutsDir) ? mprJoinPath(layoutsDir, "default.esp") : 0;
        }
        if (layout) {
            stale = espModuleIsStale(layout, module, &recompile);
            if (stale) {
                mprLog("info esp", 4, "esp layout %s is newer than module %s", layout, module);
            }
        }
    }
    return stale;
}
#else

PUBLIC bool espModuleIsStale(cchar *source, cchar *module, int *recompile)
{
    return 0;
}
#endif /* ME_STATIC */


/************************************ Esp Route *******************************/
/*
    Public so that esp.c can also call
 */
PUBLIC void espManageEspRoute(EspRoute *eroute, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(eroute->actions);
        mprMark(eroute->appName);
        mprMark(eroute->compileCmd);
        mprMark(eroute->configFile);
        mprMark(eroute->currentSession);
        mprMark(eroute->edi);
        mprMark(eroute->env);
        mprMark(eroute->linkCmd);
        mprMark(eroute->searchPath);
        mprMark(eroute->top);
        mprMark(eroute->views);
        mprMark(eroute->winsdk);
#if DEPRECATED || 1
        mprMark(eroute->combineScript);
        mprMark(eroute->combineSheet);
#endif
    }
}


PUBLIC EspRoute *espCreateRoute(HttpRoute *route)
{
    EspRoute    *eroute;

    if ((eroute = mprAllocObj(EspRoute, espManageEspRoute)) == 0) {
        return 0;
    }
    route->eroute = eroute;
    eroute->route = route;
    eroute->compile = 1;
    eroute->keep = 0;
    eroute->update = 1;
    eroute->compileMode = ESP_COMPILE_SYMBOLS;

    if (route->parent && route->parent->eroute) {
        eroute->top = ((EspRoute*) route->parent->eroute)->top;
    } else {
        eroute->top = eroute;
    }
    eroute->appName = sclone("app");
    return eroute;
}


static EspRoute *cloneEspRoute(HttpRoute *route, EspRoute *parent)
{
    EspRoute      *eroute;

    assert(parent);
    assert(route);

    if ((eroute = mprAllocObj(EspRoute, espManageEspRoute)) == 0) {
        return 0;
    }
    eroute->route = route;
    eroute->top = parent->top;
    eroute->searchPath = parent->searchPath;
    eroute->configFile = parent->configFile;
    eroute->edi = parent->edi;
    eroute->commonController = parent->commonController;
    if (parent->compileCmd) {
        eroute->compileCmd = sclone(parent->compileCmd);
    }
    if (parent->linkCmd) {
        eroute->linkCmd = sclone(parent->linkCmd);
    }
    if (parent->env) {
        eroute->env = mprCloneHash(parent->env);
    }
    eroute->appName = parent->appName;
    eroute->combine = parent->combine;
    eroute->compile = parent->compile;
    eroute->keep = parent->keep;
    eroute->update = parent->update;
#if DEPRECATED || 1
    eroute->combineScript = parent->combineScript;
    eroute->combineSheet = parent->combineSheet;
#endif
    route->eroute = eroute;
    return eroute;
}


/*
    Get an EspRoute. Allocate if required.
    It is expected that the caller will modify the EspRoute.
 */
PUBLIC EspRoute *espRoute(HttpRoute *route)
{
    HttpRoute   *rp;

    if (route->eroute && (!route->parent || route->parent->eroute != route->eroute)) {
        return route->eroute;
    }
    /*
        Lookup up the route chain for any configured EspRoutes to clone
     */
    for (rp = route; rp; rp = rp->parent) {
        if (rp->eroute) {
            return cloneEspRoute(route, rp->eroute);
        }
        if (rp->parent == 0) {
            /*
                Create an ESP configuration on the top level parent so others can inherit
                Load the compiler rules once for all
             */
            espCreateRoute(rp);
            if (rp != route) {
                espInit(rp, 0, "esp.json");
            }
            break;
        }
    }
    return cloneEspRoute(route, rp->eroute);
}


/*
    Manage all links for EspReq for the garbage collector
 */
static void manageReq(EspReq *req, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(req->commandLine);
        mprMark(req->data);
        mprMark(req->edi);
        mprMark(req->feedback);
        mprMark(req->lastFeedback);
        mprMark(req->route);
    }
}


/*
    Manage all links for Esp for the garbage collector
 */
static void manageEsp(Esp *esp, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(esp->databases);
        mprMark(esp->databasesTimer);
        mprMark(esp->ediService);
        mprMark(esp->internalOptions);
        mprMark(esp->local);
        mprMark(esp->mutex);
        mprMark(esp->vstudioEnv);
        mprMark(esp->hostedDocuments);
    }
}


/*********************************** Directives *******************************/
/*
    Load the ESP configuration file esp.json (eroute->configFile) and an optional package.json file
    WARNING: may yield
 */
PUBLIC int espLoadConfig(HttpRoute *route)
{
    EspRoute    *eroute;
    cchar       *home, *name, *package;
    bool        modified;

    eroute = route->eroute;
    if (!eroute) {
        mprLog("esp error", 0, "Cannot find esp configuration when loading config");
        return MPR_ERR_CANT_LOAD;
    }
    if (eroute->loaded && !eroute->update) {
        return 0;
    }
    home = eroute->configFile ? mprGetPathDir(eroute->configFile) : route->home;
    package = mprJoinPath(home, "package.json");
    modified = 0;
    ifConfigModified(route, eroute->configFile, &modified);
    ifConfigModified(route, package, &modified);

    if (modified) {
        lock(esp);
        httpInitConfig(route);
        if (mprPathExists(package, R_OK) && !mprSamePath(package, eroute->configFile)) {
            if (httpLoadConfig(route, package) < 0) {
                unlock(esp);
                return MPR_ERR_CANT_LOAD;
            }
        }
        if (httpLoadConfig(route, eroute->configFile) < 0) {
            unlock(esp);
            return MPR_ERR_CANT_LOAD;
        }
        if ((name = espGetConfig(route, "name", 0)) != 0) {
            eroute->appName = name;
        }
        unlock(esp);
    }
    if (!httpGetDir(route, "CACHE")) {
        espSetDefaultDirs(route, 0);
    }
    return 0;
}


/*
    Preload application module
    WARNING: may yield when compiling modules
 */
static bool preload(HttpRoute *route)
{
#if !ME_STATIC
    EspRoute    *eroute;
    MprJson     *preload, *item;
    cchar       *errMsg, *source;
    char        *kind;
    int         i;

    eroute = route->eroute;
    if (eroute->app && !(route->flags & HTTP_ROUTE_NO_LISTEN)) {
        if (eroute->combine) {
            source = mprJoinPaths(route->home, httpGetDir(route, "CACHE"), sfmt("%s.c", eroute->appName), NULL);
        } else {
            source = mprJoinPaths(route->home, httpGetDir(route, "SRC"), "app.c", NULL);
        }
        if (espLoadModule(route, NULL, "app", source, &errMsg) < 0) {
            if (eroute->combine) {
                mprLog("error esp", 0, "%s", errMsg);
                return 0;
            }
        }
        if (!eroute->combine && (preload = mprGetJsonObj(route->config, "esp.preload")) != 0) {
            for (ITERATE_JSON(preload, item, i)) {
                source = ssplit(sclone(item->value), ":", &kind);
                if (*kind == '\0') {
                    kind = "controller";
                }
                source = mprJoinPaths(route->home, httpGetDir(route, "CONTROLLERS"), source, NULL);
                if (espLoadModule(route, NULL, kind, source, &errMsg) < 0) {
                    mprLog("error esp", 0, "Cannot preload esp module %s. %s", source, errMsg);
                    return 0;
                }
            }
        }
    }
#endif
    return 1;
}


/*
    Initialize ESP.
    Prefix is the URI prefix for the application
    Path is the path to the esp.json
 */
PUBLIC int espInit(HttpRoute *route, cchar *prefix, cchar *path)
{
    EspRoute    *eroute;
    cchar       *hostname;

    if (!route) {
        return MPR_ERR_BAD_ARGS;
    }
    lock(esp);
    if ((eroute = espRoute(route)) == 0) {
        unlock(esp);
        return MPR_ERR_MEMORY;
    }
    if (prefix) {
        if (*prefix != '/') {
            prefix = sjoin("/", prefix, NULL);
        }
        prefix = stemplate(prefix, route->vars);
        httpSetRoutePrefix(route, prefix);
        httpSetRoutePattern(route, sfmt("^%s", prefix), 0);
        hostname = route->host->name ? route->host->name : "default";
        mprLog("info esp", 3, "Load ESP app: %s%s from %s", hostname, prefix, path);
    }
    eroute->top = eroute;
    if (path && mprPathExists(path, R_OK)) {
        httpSetRouteHome(route, mprGetPathDir(path));
        eroute->configFile = sclone(path);
    }
    httpAddRouteHandler(route, "espHandler", "esp");

    if (espLoadConfig(route) < 0) {
        unlock(esp);
        return MPR_ERR_CANT_LOAD;
    }
    if (eroute->compile && espLoadCompilerRules(route) < 0) {
        unlock(esp);
        return MPR_ERR_CANT_OPEN;
    }
    if (route->database && !eroute->edi && espOpenDatabase(route, route->database) < 0) {
        unlock(esp);
        mprLog("error esp", 0, "Cannot open database %s", route->database);
        return MPR_ERR_CANT_LOAD;
    }
    if (!preload(route)) {
        unlock(esp);
        return MPR_ERR_CANT_LOAD;
    }
    unlock(esp);
    return 0;
}


PUBLIC int espOpenDatabase(HttpRoute *route, cchar *spec)
{
    EspRoute    *eroute;
    char        *provider, *path, *dir;
    int         flags;

    eroute = route->eroute;
    if (eroute->edi) {
        return 0;
    }
    flags = EDI_CREATE | EDI_AUTO_SAVE;
    if (smatch(spec, "default")) {
#if ME_COM_SQLITE
        spec = sfmt("sdb://%s.sdb", eroute->appName);
#elif ME_COM_MDB
        spec = sfmt("mdb://%s.mdb", eroute->appName);
#endif
    }
    provider = ssplit(sclone(spec), "://", &path);
    if (*provider == '\0' || *path == '\0') {
        return MPR_ERR_BAD_ARGS;
    }
    path = mprJoinPaths(route->home, httpGetDir(route, "DB"), path, NULL);
    dir = mprGetPathDir(path);
    if (!mprPathExists(dir, X_OK)) {
        mprMakeDir(dir, 0755, -1, -1, 1);
    }
    if ((eroute->edi = ediOpen(mprGetRelPath(path, NULL), provider, flags)) == 0) {
        return MPR_ERR_CANT_OPEN;
    }
    route->database = sclone(spec);
    return 0;
}


static void setDir(HttpRoute *route, cchar *key, cchar *value, bool force)
{
    if (force) {
        httpSetDir(route, key, value);
    } else if (!httpGetDir(route, key)) {
        httpSetDir(route, key, value);
    }
}


PUBLIC void espSetDefaultDirs(HttpRoute *route, bool app)
{
    cchar   *controllers, *documents, *path, *migrations;
    char    *output;

    documents = mprJoinPath(route->home, "dist");
#if DEPRECATED || 1
    /*
        Consider keeping documents, web and public 
     */
    if (!mprPathExists(documents, X_OK)) {
        documents = mprJoinPath(route->home, "documents");
        if (!mprPathExists(documents, X_OK)) {
            documents = mprJoinPath(route->home, "web");
            if (!mprPathExists(documents, X_OK)) {
                documents = mprJoinPath(route->home, "client");
                if (!mprPathExists(documents, X_OK)) {
                    documents = mprJoinPath(route->home, "public");
                    if (!mprPathExists(documents, X_OK)) {
                        if (!esp->hostedDocuments && mprPathExists("install.conf", R_OK)) {
                            /* 
                                This returns the documents directory of the default route of the default host 
                                When Appweb switches to appweb.json, then just it should be loaded with package.json
                             */
                            if (mprRun(NULL, "appweb --show-documents", NULL, (char**) &output, NULL, 5000) == 0) {
                                documents = esp->hostedDocuments = strim(output, "\n", MPR_TRIM_END);
                            } else {
                                documents = route->home;
                            }
                        } else {
                            documents = route->home;
                        }
                    }
                }
            }
        }
    }
#endif
    
    /*
        Detect if a controllers directory exists. Set controllers to "." if absent.
     */
    controllers = "controllers";
    path = mprJoinPath(route->home, controllers);
    if (!mprPathExists(path, X_OK)) {
        controllers = ".";
    }

#if DEPRECATED || 1
    migrations = "db/migrations";
    path = mprJoinPath(route->home, migrations);
    if (!mprPathExists(path, X_OK)) {
        migrations = "migrations";
    }
#else
    migrations = "migrations";
#endif

    setDir(route, "CACHE", 0, app);
    setDir(route, "CONTROLLERS", controllers, app);
    setDir(route, "CONTENTS", 0, app);
    setDir(route, "DB", 0, app);
    setDir(route, "DOCUMENTS", documents, app);
    setDir(route, "HOME", route->home, app);
    setDir(route, "LAYOUTS", 0, app);
    setDir(route, "LIB", 0, app);
    setDir(route, "MIGRATIONS", migrations, app);
    setDir(route, "PAKS", 0, app);
    setDir(route, "PARTIALS", 0, app);
    setDir(route, "SRC", 0, app);
    setDir(route, "UPLOAD", "/tmp", app);
}


/*
    Initialize and load a statically linked ESP module
 */
PUBLIC int espStaticInitialize(EspModuleEntry entry, cchar *appName, cchar *routeName)
{
    HttpRoute   *route;

    if ((route = httpLookupRoute(NULL, routeName)) == 0) {
        mprLog("error esp", 0, "Cannot find route %s", routeName);
        return MPR_ERR_CANT_ACCESS;
    }
    return (entry)(route, NULL);
}


PUBLIC int espBindProc(HttpRoute *parent, cchar *pattern, void *proc)
{
    HttpRoute   *route;
    EspRoute    *eroute;

    if ((route = httpDefineRoute(parent, "ALL", pattern, "$&", "unused")) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    httpSetRouteHandler(route, "espHandler");
    espDefineAction(route, pattern, proc);
    eroute = route->eroute;
    eroute->update = 0;
    return 0;
}


static void ifConfigModified(HttpRoute *route, cchar *path, bool *modified)
{
    EspRoute    *eroute;
    MprPath     info;

    if (path) {
        eroute = route->eroute;
        mprGetPathInfo(path, &info);
        if (info.mtime > eroute->loaded) {
            *modified = 1;
            eroute->loaded = info.mtime;
        }
    }
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

/*
    espTemplate.c -- ESP templated web pages with embedded C code.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/



/************************************ Defines *********************************/
/*
      ESP lexical analyser tokens
 */
#define ESP_TOK_ERR            -1            /* Any input error */
#define ESP_TOK_EOF             0            /* End of file */
#define ESP_TOK_CODE            1            /* <% text %> */
#define ESP_TOK_EXPR            2            /* <%= expression %> */
#define ESP_TOK_CONTROL         3            /* <%^ control */
#define ESP_TOK_PARAM           4            /* %$param */
#define ESP_TOK_FIELD           5            /* %#field */
#define ESP_TOK_VAR             6            /* %!var */
#define ESP_TOK_HOME            7            /* %~ Home URL */
#define ESP_TOK_LITERAL         8            /* literal HTML */
#if DEPRECATED || 1
#define ESP_TOK_SERVER          9            /* %| Server URL  */
#endif

/*
    ESP page parser structure
 */
typedef struct EspParse {
    int     lineNumber;                     /**< Line number for error reporting */
    char    *data;                          /**< Input data to parse */
    char    *next;                          /**< Next character in input */
    cchar   *path;                          /**< Filename being parsed */
    MprBuf  *token;                         /**< Input token */
} EspParse;

/************************************ Forwards ********************************/

static int getEspToken(EspParse *parse);
static cchar *getDebug(EspRoute *eroute);
static cchar *getEnvString(HttpRoute *route, cchar *key, cchar *defaultValue);
static cchar *getArExt(cchar *os);
static cchar *getShlibExt(cchar *os);
static cchar *getShobjExt(cchar *os);
static cchar *getArPath(cchar *os, cchar *arch);
static cchar *getCompilerName(cchar *os, cchar *arch);
static cchar *getCompilerPath(cchar *os, cchar *arch);
static cchar *getLibs(cchar *os);
static cchar *getMappedArch(cchar *arch);
static cchar *getObjExt(cchar *os);
static cchar *getVxCPU(cchar *arch);
static bool matchToken(cchar **str, cchar *token);

#if ME_WIN_LIKE
static cchar *getWinSDK(HttpRoute *route);
static cchar *getWinVer(HttpRoute *route);
#endif

/************************************* Code ***********************************/
/*
    Tokens:
    AR          Library archiver (ar)   
    ARLIB       Archive library extension (.a, .lib)
    ARCH        Build architecture (64)
    CC          Compiler (cc)
    DEBUG       Debug compilation options (-g, -Zi -Od)
    GCC_ARCH    ARCH mapped to gcc -arch switches (x86_64)
    INC         Include directory out/inc
    LIBPATH     Library search path
    LIBS        Libraries required to link with ESP
    OBJ         Name of compiled source (out/lib/view-MD5.o)
    MOD         Output module (view_MD5)
    SHLIB       Host Shared library (.lib, .so)
    SHOBJ       Host Shared Object (.dll, .so)
    SRC         Source code for view or controller (already templated)
    TMP         Temp directory
    VS          Visual Studio directory
    WINSDK      Windows SDK directory
 */
PUBLIC char *espExpandCommand(HttpRoute *route, cchar *command, cchar *source, cchar *module)
{
    MprBuf      *buf;
    Http        *http;
    EspRoute    *eroute;
    cchar       *cp, *outputModule, *os, *arch, *profile, *srcDir;
    char        *tmp;
    
    if (command == 0) {
        return 0;
    }
    http = MPR->httpService;
    eroute = route->eroute;
    outputModule = mprTrimPathExt(module);
    httpParsePlatform(http->platform, &os, &arch, &profile);
    buf = mprCreateBuf(-1, -1);

    for (cp = command; *cp; ) {
        if (*cp == '$') {
            if (matchToken(&cp, "${ARCH}")) {
                /* Target architecture (x86|mips|arm|x64) */
                mprPutStringToBuf(buf, arch);

            } else if (matchToken(&cp, "${ARLIB}")) {
                /* .a, .lib */
                mprPutStringToBuf(buf, getArExt(os));

            } else if (matchToken(&cp, "${GCC_ARCH}")) {
                /* Target architecture mapped to GCC mtune|mcpu values */
                mprPutStringToBuf(buf, getMappedArch(arch));

            } else if (matchToken(&cp, "${APPINC}")) {
                /* Application src include directory */
                if ((srcDir = httpGetDir(route, "SRC")) == 0) {
                    srcDir = ".";
                }
                mprPutStringToBuf(buf, srcDir);

            } else if (matchToken(&cp, "${INC}")) {
                /* Include directory for the configuration */
                mprPutStringToBuf(buf, mprJoinPath(http->platformDir, "inc")); 

            } else if (matchToken(&cp, "${LIBPATH}")) {
                /* Library directory for Appweb libraries for the target */
                mprPutStringToBuf(buf, mprJoinPath(http->platformDir, "bin")); 

            } else if (matchToken(&cp, "${LIBS}")) {
                /* Required libraries to link. These may have nested ${TOKENS} */
                mprPutStringToBuf(buf, espExpandCommand(route, getLibs(os), source, module));

            } else if (matchToken(&cp, "${MOD}")) {
                /* Output module path in the cache without extension */
                mprPutStringToBuf(buf, outputModule);

            } else if (matchToken(&cp, "${OBJ}")) {
                /* Output object with extension (.o) in the cache directory */
                mprPutStringToBuf(buf, mprJoinPathExt(outputModule, getObjExt(os)));

            } else if (matchToken(&cp, "${OS}")) {
                /* Target architecture (freebsd|linux|macosx|windows|vxworks) */
                mprPutStringToBuf(buf, os);

            } else if (matchToken(&cp, "${SHLIB}")) {
                /* .dll, .so, .dylib */
                mprPutStringToBuf(buf, getShlibExt(os));

            } else if (matchToken(&cp, "${SHOBJ}")) {
                /* .dll, .so, .dylib */
                mprPutStringToBuf(buf, getShobjExt(os));

            } else if (matchToken(&cp, "${SRC}")) {
                /* View (already parsed into C code) or controller source */
                mprPutStringToBuf(buf, source);

            } else if (matchToken(&cp, "${TMP}")) {
                if ((tmp = getenv("TMPDIR")) == 0) {
                    if ((tmp = getenv("TMP")) == 0) {
                        tmp = getenv("TEMP");
                    }
                }
                mprPutStringToBuf(buf, tmp ? tmp : ".");

#if ME_WIN_LIKE
            } else if (matchToken(&cp, "${VS}")) {
                mprPutStringToBuf(buf, espGetVisualStudio());
            } else if (matchToken(&cp, "${WINSDK}")) {
                mprPutStringToBuf(buf, getWinSDK(route));
            } else if (matchToken(&cp, "${WINVER}")) {
                mprPutStringToBuf(buf, getWinVer(route));
#endif

            } else if (matchToken(&cp, "${VXCPU}")) {
                mprPutStringToBuf(buf, getVxCPU(arch));

            /*
                These vars can be also be configured from environment variables.
                NOTE: the default esp.conf includes "esp->vxworks.conf" which has EspEnv definitions for the 
                configured VxWorks toolchain.
             */
            } else if (matchToken(&cp, "${AR}")) {
                mprPutStringToBuf(buf, getEnvString(route, "AR", getArPath(os, arch)));

            } else if (matchToken(&cp, "${CC}")) {
                mprPutStringToBuf(buf, getEnvString(route, "CC", getCompilerPath(os, arch)));

            } else if (matchToken(&cp, "${CFLAGS}")) {
                mprPutStringToBuf(buf, getEnvString(route, "CFLAGS", ""));

            } else if (matchToken(&cp, "${DEBUG}")) {
                mprPutStringToBuf(buf, getEnvString(route, "DEBUG", getDebug(eroute)));

            } else if (matchToken(&cp, "${LDFLAGS}")) {
                mprPutStringToBuf(buf, getEnvString(route, "LDFLAGS", ""));

            } else if (matchToken(&cp, "${LIB}")) {
                mprPutStringToBuf(buf, getEnvString(route, "LIB", ""));

            } else if (matchToken(&cp, "${LINK}")) {
                mprPutStringToBuf(buf, getEnvString(route, "LINK", ""));

            } else if (matchToken(&cp, "${WIND_BASE}")) {
                mprPutStringToBuf(buf, getEnvString(route, "WIND_BASE", WIND_BASE));

            } else if (matchToken(&cp, "${WIND_HOME}")) {
                mprPutStringToBuf(buf, getEnvString(route, "WIND_HOME", WIND_HOME));

            } else if (matchToken(&cp, "${WIND_HOST_TYPE}")) {
                mprPutStringToBuf(buf, getEnvString(route, "WIND_HOST_TYPE", WIND_HOST_TYPE));

            } else if (matchToken(&cp, "${WIND_PLATFORM}")) {
                mprPutStringToBuf(buf, getEnvString(route, "WIND_PLATFORM", WIND_PLATFORM));

            } else if (matchToken(&cp, "${WIND_GNU_PATH}")) {
                mprPutStringToBuf(buf, getEnvString(route, "WIND_GNU_PATH", WIND_GNU_PATH));

            } else if (matchToken(&cp, "${WIND_CCNAME}")) {
                mprPutStringToBuf(buf, getEnvString(route, "WIND_CCNAME", getCompilerName(os, arch)));

            } else {
                mprPutCharToBuf(buf, *cp++);
            }
        } else {
            mprPutCharToBuf(buf, *cp++);
        }
    }
    mprAddNullToBuf(buf);
    return sclone(mprGetBufStart(buf));
}


static int runCommand(HttpRoute *route, MprDispatcher *dispatcher, cchar *command, cchar *csource, cchar *module, 
    char **errMsg)
{
    MprCmd      *cmd;
    MprKey      *var;
    MprList     *elist;
    EspRoute    *eroute;
    cchar       **env, *commandLine;
    char        *err, *out;
    int         rc;

    *errMsg = 0;
    eroute = route->eroute;
    if ((commandLine = espExpandCommand(route, command, csource, module)) == 0) {
        *errMsg = sfmt("Missing EspCompile directive for %s", csource);
        return MPR_ERR_CANT_READ;
    }
    mprLog("info esp run", 4, "%s", commandLine);
    if (eroute->env) {
        elist = mprCreateList(0, MPR_LIST_STABLE);
        for (ITERATE_KEYS(eroute->env, var)) {
            mprAddItem(elist, sfmt("%s=%s", var->key, (char*) var->data));
        }
        mprAddNullItem(elist);
        env = (cchar**) &elist->items[0];
    } else {
        env = 0;
    }
    cmd = mprCreateCmd(dispatcher);
    if (eroute->searchPath) {
        mprSetCmdSearchPath(cmd, eroute->searchPath);
    }
    /*
        WARNING: GC will run here
     */
    mprHold((void*) commandLine);
    rc = mprRunCmd(cmd, commandLine, env, NULL, &out, &err, -1, 0);
    mprRelease((void*) commandLine);

    if (rc != 0) {
        if (err == 0 || *err == '\0') {
            /* Windows puts errors to stdout Ugh! */
            err = out;
        }
        mprLog("error esp", 0, "Cannot run command: %s, error %s", commandLine, err);
        if (route->flags & HTTP_ROUTE_SHOW_ERRORS) {
            *errMsg = sfmt("Cannot run command: %s, error %s", commandLine, err);
        } else {
            *errMsg = "Cannot compile view";
        }
        mprDestroyCmd(cmd);
        return MPR_ERR_CANT_COMPLETE;
    }
    mprDestroyCmd(cmd);
    return 0;
}


PUBLIC int espLoadCompilerRules(HttpRoute *route)
{
    EspRoute    *eroute;
    cchar       *compile, *rules;

    eroute = route->eroute;
    if (eroute->compileCmd) {
        return 0;
    }
    if ((compile = mprGetJson(route->config, "esp.rules")) == 0) {
        compile = ESP_COMPILE_JSON;
    }
    rules = mprJoinPath(mprGetAppDir(), compile);
    if (mprPathExists(rules, R_OK) && httpLoadConfig(route, rules) < 0) {
        mprLog("error esp", 0, "Cannot parse %s", rules);
        return MPR_ERR_CANT_OPEN;
    }
    return 0;
}


/*
    Compile a view or controller

    cacheName   MD5 cache name (not a full path)
    source      ESP source file name
    module      Module file name

    WARNING: this routine blocks and runs GC. All parameters must be retained.
 */
PUBLIC bool espCompile(HttpRoute *route, MprDispatcher *dispatcher, cchar *source, cchar *module, cchar *cacheName, 
    int isView, char **errMsg)
{
    MprFile     *fp;
    EspRoute    *eroute;
    cchar       *csource, *layoutsDir;
    char        *layout, *script, *page, *err;
    ssize       len;

    eroute = route->eroute;
    assert(eroute->compile);

    layout = 0;
    *errMsg = 0;

    mprLog("info esp", 2, "Compile %s", source);
    if (isView) {
        if ((page = mprReadPathContents(source, &len)) == 0) {
            *errMsg = sfmt("Cannot read %s", source);
            return 0;
        }
#if DEPRECATED || 1
        if ((layoutsDir = httpGetDir(route, "LAYOUTS")) != 0) {
            layout = mprJoinPath(layoutsDir, "default.esp");
        }
#endif
        if ((script = espBuildScript(route, page, source, cacheName, layout, NULL, &err)) == 0) {
            *errMsg = sfmt("Cannot build: %s, error: %s", source, err);
            return 0;
        }
        csource = mprJoinPathExt(mprTrimPathExt(module), ".c");
        mprMakeDir(mprGetPathDir(csource), 0775, -1, -1, 1);
        if ((fp = mprOpenFile(csource, O_WRONLY | O_TRUNC | O_CREAT | O_BINARY, 0664)) == 0) {
            *errMsg = sfmt("Cannot open compiled script file %s", csource);
            return 0;
        }
        len = slen(script);
        if (mprWriteFile(fp, script, len) != len) {
            *errMsg = sfmt("Cannot write compiled script file %s", csource);
            mprCloseFile(fp);
            return 0;
        }
        mprCloseFile(fp);
    } else {
        csource = source;
    }
    mprMakeDir(mprGetPathDir(module), 0775, -1, -1, 1);

#if ME_WIN_LIKE
    {
        /*
            Force a clean windows compile by removing the obj, pdb and ilk files
         */
        cchar   *path;
        path = mprReplacePathExt(module, "obj");
        if (mprPathExists(path, F_OK)) {
            mprDeletePath(path);
        }
        path = mprReplacePathExt(module, "pdb");
        if (mprPathExists(path, F_OK)) {
            mprDeletePath(path);
        }
        path = mprReplacePathExt(module, "ilk");
        if (mprPathExists(path, F_OK)) {
            mprDeletePath(path);
        }
    }
#endif

    if (!eroute->compileCmd && espLoadCompilerRules(route) < 0) {
        return 0;
    }

    /* 
        Run compiler: WARNING: GC yield here 
     */
    if (runCommand(route, dispatcher, eroute->compileCmd, csource, module, errMsg) != 0) {
        return 0;
    }
    if (eroute->linkCmd) {
        /* WARNING: GC yield here */
        if (runCommand(route, dispatcher, eroute->linkCmd, csource, module, errMsg) != 0) {
            return 0;
        }
#if !MACOSX
        /*
            MAC needs the object for debug information
         */
        mprDeletePath(mprJoinPathExt(mprTrimPathExt(module), &ME_OBJ[1]));
#endif
    }
#if ME_WIN_LIKE
    {
        /*
            Windows leaves intermediate object in the current directory
         */
        cchar   *path;
        path = mprReplacePathExt(mprGetPathBase(csource), "obj");
        if (mprPathExists(path, F_OK)) {
            mprDeletePath(path);
        }
    }
#endif
    if (!eroute->keep && isView) {
        mprDeletePath(csource);
    }
    return 1;
}


static char *fixMultiStrings(cchar *str)
{
    cchar   *cp;
    char    *buf, *bp;
    ssize   len;
    int     count, bquote, quoted;

    for (count = 0, cp = str; *cp; cp++) {
        if (*cp == '\n' || *cp == '"') {
            count++;
        }
    }
    len = slen(str);
    if ((buf = mprAlloc(len + (count * 3) + 1)) == 0) {
        return 0;
    }
    bquote = quoted = 0;
    for (cp = str, bp = buf; *cp; cp++) {
        if (*cp == '`') {
            *bp++ = '"';
            quoted = !quoted;
        } else if (quoted) {
            if (*cp == '\n') {
                *bp++ = '\\';
            } else if (*cp == '"') {
                *bp++ = '\\';
            } else if (*cp == '\\' && cp[1] != '\\') {
                bquote++;
            }
            *bp++ = *cp;
        } else {
            *bp++ = *cp;
        }
    }
    *bp = '\0';
    return buf;
}


static char *joinLine(cchar *str, ssize *lenp)
{
    cchar   *cp;
    char    *buf, *bp;
    ssize   len;
    int     count, bquote;

    for (count = 0, cp = str; *cp; cp++) {
        if (*cp == '\n' || *cp == '\r') {
            count++;
        }
    }
    /*
        Allocate room to backquote newlines (requires 3)
     */
    len = slen(str);
    if ((buf = mprAlloc(len + (count * 3) + 1)) == 0) {
        return 0;
    }
    bquote = 0;
    for (cp = str, bp = buf; *cp; cp++) {
        if (*cp == '\n') {
            *bp++ = '\\';
            *bp++ = 'n';
            *bp++ = '\\';
        } else if (*cp == '\r') {
            *bp++ = '\\';
            *bp++ = 'r';
            continue;
        } else if (*cp == '\\') {
            if (cp[1]) {
                *bp++ = *cp++;
                bquote++;
            }
        }
        *bp++ = *cp;
    }
    *bp = '\0';
    *lenp = len - bquote;
    return buf;
}


/*
    Convert an ESP web page into C code
    Directives:
        <%                  Begin esp section containing C code
        <%=                 Begin esp section containing an expression to evaluate and substitute
        <%= [%fmt]          Begin a formatted expression to evaluate and substitute. Format is normal printf format.
                            Use %S for safe HTML escaped output.
        %>                  End esp section
        -%>                 End esp section and trim trailing newline

        <%^ content         Mark the location to substitute content in a layout page
        <%^ global          Put esp code at the global level
        <%^ start           Put esp code at the start of the function
        <%^ end             Put esp code at the end of the function

        %!var               Substitue the value of a parameter. 
        %$param             Substitue the value of a request parameter.
        %#field             Lookup the current record for the value of the field.
        %~                  Home URL for the application

    Deprecated:
        <%^ layout "file"   Specify a layout page to use. Use layout "" to disable layout management
        <%^ include "file"  Include an esp file
 */

//  DEPRECATED layout
PUBLIC char *espBuildScript(HttpRoute *route, cchar *page, cchar *path, cchar *cacheName, cchar *layout, 
        EspState *state, char **err)
{
    EspState    top;
    EspParse    parse;
    MprBuf      *body;
    cchar       *layoutsDir;
    char        *control, *incText, *where, *layoutCode, *bodyCode;
    char        *rest, *include, *line, *fmt, *layoutPage, *incCode, *token;
    ssize       len;
    int         tid;

    assert(page);

    *err = 0;
    if (!state) {
        assert(cacheName);
        state = &top;
        memset(state, 0, sizeof(EspParse));
        state->global = mprCreateBuf(0, 0);
        state->start = mprCreateBuf(0, 0);
        state->end = mprCreateBuf(0, 0);
    }
    body = mprCreateBuf(0, 0);
    parse.data = (char*) page;
    parse.next = parse.data;
    parse.lineNumber = 0;
    parse.token = mprCreateBuf(0, 0);
    parse.path = path;
    tid = getEspToken(&parse);

    while (tid != ESP_TOK_EOF) {
        token = mprGetBufStart(parse.token);
#if KEEP
        if (state->lineNumber != lastLine) {
            mprPutToBuf(script, "\n# %d \"%s\"\n", state->lineNumber, path);
        }
#endif
        switch (tid) {
        case ESP_TOK_CODE:
#if DEPRECATED || 1
            if (*token == '^') {
                for (token++; *token && isspace((uchar) *token); token++) ;
                where = ssplit(token, " \t\r\n", &rest);
                if (*rest) {
                    if (smatch(where, "global")) {
                        mprPutStringToBuf(state->global, rest);

                    } else if (smatch(where, "start")) {
                        mprPutToBuf(state->start, "%s  ", rest);

                    } else if (smatch(where, "end")) {
                        mprPutToBuf(state->end, "%s  ", rest);
                    }
                }
            } else
#endif
            mprPutStringToBuf(body, fixMultiStrings(token));
            break;

        case ESP_TOK_CONTROL:
            control = ssplit(token, " \t\r\n", &token);
            if (smatch(control, "content")) {
                mprPutStringToBuf(body, ESP_CONTENT_MARKER);

#if DEPRECATED || 1
            } else if (smatch(control, "include")) {
                token = strim(token, " \t\r\n\"", MPR_TRIM_BOTH);
                token = mprNormalizePath(token);
                if (token[0] == '/') {
                    include = sclone(token);
                } else {
                    include = mprJoinPath(mprGetPathDir(path), token);
                }
                if ((incText = mprReadPathContents(include, &len)) == 0) {
                    *err = sfmt("Cannot read include file: %s", include);
                    return 0;
                }
                /* Recurse and process the include script */
                if ((incCode = espBuildScript(route, incText, include, NULL, NULL, state, err)) == 0) {
                    return 0;
                }
                mprPutStringToBuf(body, incCode);
#endif

#if DEPRECATED || 1
            } else if (smatch(control, "layout")) {
                mprLog("esp warn", 0, "Using deprecated \"layout\" control directive in esp page: %s", path);
                token = strim(token, " \t\r\n\"", MPR_TRIM_BOTH);
                if (*token == '\0') {
                    layout = 0;
                } else {
                    token = mprNormalizePath(token);
                    if (token[0] == '/') {
                        layout = sclone(token);
                    } else {
                        if ((layoutsDir = httpGetDir(route, "LAYOUTS")) != 0) {
                            layout = mprJoinPath(layoutsDir, token);
                        } else {
                            layout = mprJoinPath(mprGetPathDir(path), token);
                        }
                    }
                    if (!mprPathExists(layout, F_OK)) {
                        *err = sfmt("Cannot access layout page %s", layout);
                        return 0;
                    }
                }
#endif

            } else if (smatch(control, "global")) {
                mprPutStringToBuf(state->global, token);

            } else if (smatch(control, "start")) {
                mprPutToBuf(state->start, "%s  ", token);

            } else if (smatch(control, "end")) {
                mprPutToBuf(state->end, "%s  ", token);

            } else {
                *err = sfmt("Unknown control %s at line %d", control, state->lineNumber);
                return 0;
            }
            break;

        case ESP_TOK_ERR:
            return 0;

        case ESP_TOK_EXPR:
            /* <%= expr %> */
            if (*token == '%') {
                fmt = ssplit(token, ": \t\r\n", &token);
                /* Default without format is safe. If users want a format and safe, use %S or renderSafe() */
                token = strim(token, " \t\r\n;", MPR_TRIM_BOTH);
                mprPutToBuf(body, "  espRender(conn, \"%s\", %s);\n", fmt, token);
            } else {
                token = strim(token, " \t\r\n;", MPR_TRIM_BOTH);
                mprPutToBuf(body, "  espRenderSafeString(conn, %s);\n", token);
            }
            break;

        case ESP_TOK_VAR:
            /* %!var -- string variable */
            token = strim(token, " \t\r\n;", MPR_TRIM_BOTH);
            mprPutToBuf(body, "  espRenderString(conn, %s);\n", token);
            break;

        case ESP_TOK_FIELD:
            /* %#field -- field in the current record */
            token = strim(token, " \t\r\n;", MPR_TRIM_BOTH);
            mprPutToBuf(body, "  espRenderSafeString(conn, getField(getRec(), \"%s\"));\n", token);
            break;

        case ESP_TOK_PARAM:
            /* %$param -- variable in (param || session) - Safe render */
            token = strim(token, " \t\r\n;", MPR_TRIM_BOTH);
            mprPutToBuf(body, "  espRenderVar(conn, \"%s\");\n", token);
            break;

        case ESP_TOK_HOME:
            /* %~ Home URL */
            if (parse.next[0] && parse.next[0] != '/' && parse.next[0] != '\'' && parse.next[0] != '"') {
                mprLog("esp warn", 0, "Using %%~ without following / in %s\n", path);
            }
            mprPutToBuf(body, "  espRenderString(conn, httpGetRouteTop(conn));");
            break;

#if DEPRECATED || 1
        //  DEPRECATED serverPrefix in version 6
        case ESP_TOK_SERVER:
            /* @| Server URL */
            mprLog("esp warn", 0, "Using deprecated \"|\" server URL directive in esp page: %s", path);
            mprPutToBuf(body, "  espRenderString(conn, sjoin(conn->rx->route->prefix ? conn->rx->route->prefix : \"\", conn->rx->route->serverPrefix, NULL));");
            break;
#endif

        case ESP_TOK_LITERAL:
            line = joinLine(token, &len);
            mprPutToBuf(body, "  espRenderBlock(conn, \"%s\", %zd);\n", line, len);
            break;

        default:
            return 0;
        }
        tid = getEspToken(&parse);
    }
    mprAddNullToBuf(body);

#if DEPRECATED || 1
    if (layout && mprPathExists(layout, R_OK)) {
        if ((layoutPage = mprReadPathContents(layout, &len)) == 0) {
            *err = sfmt("Cannot read layout page: %s", layout);
            return 0;
        }
        if ((layoutCode = espBuildScript(route, layoutPage, layout, NULL, NULL, state, err)) == 0) {
            return 0;
        }
#if ME_DEBUG
        if (!scontains(layoutCode, ESP_CONTENT_MARKER)) {
            *err = sfmt("Layout page is missing content marker: %s", layout);
            return 0;
        }
#endif
        bodyCode = sreplace(layoutCode, ESP_CONTENT_MARKER, mprGetBufStart(body));
        mprLog("esp warn", 0, "Using deprecated layouts in esp page: %s, use Expansive instead", path);
    } else
#endif
    bodyCode = mprGetBufStart(body);

    if (state == &top) {
        if (mprGetBufLength(state->start) > 0) {
            mprPutCharToBuf(state->start, '\n');
        }
        if (mprGetBufLength(state->end) > 0) {
            mprPutCharToBuf(state->end, '\n');
        }
        mprAddNullToBuf(state->global);
        mprAddNullToBuf(state->start);
        mprAddNullToBuf(state->end);
        bodyCode = sfmt(\
            "/*\n   Generated from %s\n */\n"\
            "#include \"esp.h\"\n"\
            "%s\n"\
            "static void %s(HttpConn *conn) {\n"\
            "%s%s%s"\
            "}\n\n"\
            "%s int esp_%s(HttpRoute *route, MprModule *module) {\n"\
            "   espDefineView(route, \"%s\", %s);\n"\
            "   return 0;\n"\
            "}\n",
            mprGetRelPath(path, route->home), mprGetBufStart(state->global), cacheName, 
                mprGetBufStart(state->start), bodyCode, mprGetBufStart(state->end),
            ESP_EXPORT_STRING, cacheName, mprGetPortablePath(mprGetRelPath(path, route->documents)), cacheName);
        mprDebug("esp", 5, "Create ESP script: \n%s\n", bodyCode);
    }
    return bodyCode;
}


static bool addChar(EspParse *parse, int c)
{
    if (mprPutCharToBuf(parse->token, c) < 0) {
        return 0;
    }
    mprAddNullToBuf(parse->token);
    return 1;
}


static char *eatSpace(EspParse *parse, char *next)
{
    for (; *next && isspace((uchar) *next); next++) {
        if (*next == '\n') {
            parse->lineNumber++;
        }
    }
    return next;
}


static char *eatNewLine(EspParse *parse, char *next)
{
    for (; *next && isspace((uchar) *next); next++) {
        if (*next == '\n') {
            parse->lineNumber++;
            next++;
            break;
        }
    }
    return next;
}


/*
    Get the next ESP input token. input points to the next input token.
    parse->token will hold the parsed token. The function returns the token id.
 */
static int getEspToken(EspParse *parse)
{
    char    *start, *end, *next;
    int     tid, done, c, t;

    start = next = parse->next;
    end = &start[slen(start)];
    mprFlushBuf(parse->token);
    tid = ESP_TOK_LITERAL;

    for (done = 0; !done && next < end; next++) {
        c = *next;
        switch (c) {
        case '<':
            if (next[1] == '%' && ((next == start) || next[-1] != '\\') && next[2] != '%') {
                next += 2;
                if (mprGetBufLength(parse->token) > 0) {
                    next -= 3;
                } else {
                    next = eatSpace(parse, next);
                    if (*next == '=') {
                        /*
                            <%= directive
                         */
                        tid = ESP_TOK_EXPR;
                        next = eatSpace(parse, ++next);
                        while (next < end && !(*next == '%' && next[1] == '>' && (next[-1] != '\\' && next[-1] != '%'))) {
                            if (*next == '\n') parse->lineNumber++;
                            if (!addChar(parse, *next++)) {
                                return ESP_TOK_ERR;
                            }
                        }

                    //  DEPRECATED '@'
                    } else if (*next == '@' || *next == '^') {
                        /*
                            <%^ control
                         */
                        if (*next == '@') {
                            mprLog("esp warn", 0, "Using deprecated \"%%%c\" control directive in esp page: %s", 
                                *next, parse->path);
                        }
                        tid = ESP_TOK_CONTROL;
                        next = eatSpace(parse, ++next);
                        while (next < end && !(*next == '%' && next[1] == '>' && (next[-1] != '\\' && next[-1] != '%'))) {
                            if (*next == '\n') parse->lineNumber++;
                            if (!addChar(parse, *next++)) {
                                return ESP_TOK_ERR;
                            }
                        }
                        
                    } else {
                        tid = ESP_TOK_CODE;
                        while (next < end && !(*next == '%' && next[1] == '>' && (next[-1] != '\\' && next[-1] != '%'))) {
                            if (*next == '\n') parse->lineNumber++;
                            if (!addChar(parse, *next++)) {
                                return ESP_TOK_ERR;
                            }
                        }
                    }
                    if (*next && next > start && next[-1] == '-') {
                        /* Remove "-" */
                        mprAdjustBufEnd(parse->token, -1);
                        mprAddNullToBuf(parse->token);
                        next = eatNewLine(parse, next + 2) - 1;
                    } else {
                        next++;
                    }
                }
                done++;
            } else {
                if (!addChar(parse, c)) {
                    return ESP_TOK_ERR;
                }                
            }
            break;

        case '%':
            if (next > start && (next[-1] == '\\' || next[-1] == '%')) {
                break;
            }
            if ((next == start) || next[-1] != '\\') {
                t = next[1];
                if (t == '~') {
                    next += 2;
                    if (mprGetBufLength(parse->token) > 0) {
                        next -= 3;
                    } else {
                        tid = ESP_TOK_HOME;
                        if (!addChar(parse, c) || !addChar(parse, t)) {
                            return ESP_TOK_ERR;
                        }
                        next--;
                    }
                    done++;

#if DEPRECATED || 1
                } else if (t == '|') {
                    mprLog("esp warn", 0, "CC Using deprecated \"|\" control directive in esp page: %s", parse->path);
                    next += 2;
                    if (mprGetBufLength(parse->token) > 0) {
                        next -= 3;
                    } else {
                        tid = ESP_TOK_SERVER;
                        if (!addChar(parse, c)) {
                            return ESP_TOK_ERR;
                        }
                        next--;
                    }
                    done++;
#endif

                //  DEPRECATED '@'
                } else if (t == '!' || t == '@' || t == '#' || t == '$') {
                    next += 2;
                    if (mprGetBufLength(parse->token) > 0) {
                        next -= 3;
                    } else {
                        if (t == '!') {
                           tid = ESP_TOK_VAR;
#if DEPRECATED || 1
                        } else if (t == '@') {
                            tid = ESP_TOK_PARAM;
#endif
                        } else if (t == '#') {
                            tid = ESP_TOK_FIELD;
                        } else {
                            tid = ESP_TOK_PARAM;
                        }
                        next = eatSpace(parse, next);
                        while (isalnum((uchar) *next) || *next == '_') {
                            if (*next == '\n') parse->lineNumber++;
                            if (!addChar(parse, *next++)) {
                                return ESP_TOK_ERR;
                            }
                        }
                        next--;
                    }
                    done++;
                } else {
                    if (!addChar(parse, c)) {
                        return ESP_TOK_ERR;
                    }
                    done++;
                }
            }
            break;

        case '\n':
            parse->lineNumber++;
            /* Fall through */

        case '\r':
        default:
            if (c == '\"' || c == '\\') {
                if (!addChar(parse, '\\')) {
                    return ESP_TOK_ERR;
                }
            }
            if (!addChar(parse, c)) {
                return ESP_TOK_ERR;
            }
            break;
        }
    }
    if (mprGetBufLength(parse->token) == 0) {
        tid = ESP_TOK_EOF;
    }
    parse->next = next;
    return tid;
}


static cchar *getEnvString(HttpRoute *route, cchar *key, cchar *defaultValue)
{
    EspRoute    *eroute;
    cchar       *value;

    eroute = route->eroute;
    if (!eroute || !eroute->env || (value = mprLookupKey(eroute->env, key)) == 0) {
        if ((value = getenv(key)) == 0) {
            if (defaultValue) {
                value = defaultValue;
            } else {
                value = sfmt("${%s}", key);
            }
        }
    }
    return value;
}


static cchar *getShobjExt(cchar *os)
{
    if (smatch(os, "macosx")) {
        return ".dylib";
    } else if (smatch(os, "windows")) {
        return ".dll";
    } else if (smatch(os, "vxworks")) {
        return ".out";
    } else {
        return ".so";
    }
}


static cchar *getShlibExt(cchar *os)
{
    if (smatch(os, "macosx")) {
        return ".dylib";
    } else if (smatch(os, "windows")) {
        return ".lib";
    } else if (smatch(os, "vxworks")) {
        return ".a";
    } else {
        return ".so";
    }
}


static cchar *getObjExt(cchar *os)
{
    if (smatch(os, "windows")) {
        return ".obj";
    }
    return ".o";
}


static cchar *getArExt(cchar *os)
{
    if (smatch(os, "windows")) {
        return ".lib";
    }
    return ".a";
}


static cchar *getCompilerName(cchar *os, cchar *arch)
{
    cchar       *name;

    name = "gcc";
    if (smatch(os, "vxworks")) {
        if (smatch(arch, "x86") || smatch(arch, "i586") || smatch(arch, "i686") || smatch(arch, "pentium")) {
            name = "ccpentium";
        } else if (scontains(arch, "86")) {
            name = "cc386";
        } else if (scontains(arch, "ppc")) {
            name = "ccppc";
        } else if (scontains(arch, "xscale") || scontains(arch, "arm")) {
            name = "ccarm";
        } else if (scontains(arch, "68")) {
            name = "cc68k";
        } else if (scontains(arch, "sh")) {
            name = "ccsh";
        } else if (scontains(arch, "mips")) {
            name = "ccmips";
        }
    } else if (smatch(os, "macosx")) {
        name = "clang";
    }
    return name;
}


static cchar *getVxCPU(cchar *arch)
{
    char   *cpu, *family;

    family = ssplit(sclone(arch), ":", &cpu);
    if (*cpu == '\0') {
        if (smatch(family, "i386")) {
            cpu = "I80386";
        } else if (smatch(family, "i486")) {
            cpu = "I80486";
        } else if (smatch(family, "x86") || sends(family, "86")) {
            cpu = "PENTIUM";
        } else if (scaselessmatch(family, "mips")) {
            cpu = "MIPS32";
        } else if (scaselessmatch(family, "arm")) {
            cpu = "ARM7TDMI";
        } else if (scaselessmatch(family, "ppc")) {
            cpu = "PPC";
        } else {
            cpu = (char*) arch;
        }
    }
    return supper(cpu);
}


static cchar *getDebug(EspRoute *eroute)
{
    Http        *http;
    Esp         *esp;
    int         symbols;

    http = MPR->httpService;
    esp = MPR->espService;
    symbols = 0;
    if (esp->compileMode == ESP_COMPILE_SYMBOLS) {
        symbols = 1;
    } else if (esp->compileMode == ESP_COMPILE_OPTIMIZED) {
        symbols = 0;
    } else if (eroute->compileMode == ESP_COMPILE_SYMBOLS) {
        symbols = 1;
    } else if (eroute->compileMode == ESP_COMPILE_OPTIMIZED) {
        symbols = 0;
    } else {
        symbols = sends(http->platform, "-debug") || sends(http->platform, "-xcode") || 
            sends(http->platform, "-mine") || sends(http->platform, "-vsdebug");
    }
    if (scontains(http->platform, "windows-")) {
        return (symbols) ? "-Zi -Od" : "-Os";
    }
    return (symbols) ? "-g" : "-O2";
}


static cchar *getLibs(cchar *os)
{
    cchar       *libs;

    if (smatch(os, "windows")) {
        libs = "\"${LIBPATH}\\libesp${SHLIB}\" \"${LIBPATH}\\libhttp.lib\" \"${LIBPATH}\\libmpr.lib\"";
    } else {
#if LINUX
        /* 
            Fedora interprets $ORIGN relative to the shared library and not the application executable
            So loading compiled apps fails to locate libesp.so. 
            Since building a shared library, can omit libs and resolve at load time.
         */
        libs = "";
#else
        libs = "-lesp -lpcre -lhttp -lmpr -lpthread -lm";
#endif
    }
    return libs;
}


static bool matchToken(cchar **str, cchar *token)
{
    ssize   len;

    len = slen(token);
    if (sncmp(*str, token, len) == 0) {
        *str += len;
        return 1;
    }
    return 0;
}


static cchar *getMappedArch(cchar *arch)
{
    if (smatch(arch, "x64")) {
        arch = "x86_64";
    } else if (smatch(arch, "x86")) {
        arch = "i686";
    }
    return arch;
}


#if WINDOWS
static int reverseSortVersions(char **s1, char **s2)
{
    return -scmp(*s1, *s2);
}
#endif


#if ME_WIN_LIKE
static cchar *getWinSDK(HttpRoute *route)
{
    EspRoute *eroute;

    /*
        MS has made a huge mess of where and how the windows SDKs are installed. The registry key at 
        HKLM/Software/Microsoft/Microsoft SDKs/Windows/CurrentInstallFolder cannot be trusted and often
        points to the old 7.X SDKs even when 8.X is installed and active. MS have also moved the 8.X
        SDK to Windows Kits, while still using the old folder for some bits. So the old-reliable
        CurrentInstallFolder registry key is now unusable. So we must scan for explicit SDK versions 
        listed above. Ugh!
     */
    cchar   *path, *key, *version;
    MprList *versions;
    int     i;

    eroute = route->eroute;
    if (eroute->winsdk) {
        return eroute->winsdk;
    }
    /* 
        General strategy is to find an "include" directory in the highest version Windows SDK.
        First search the registry key: Windows Kits/InstalledRoots/KitsRoot*
     */
    key = sfmt("HKLM\\SOFTWARE%s\\Microsoft\\Windows Kits\\Installed Roots", (ME_64) ? "\\Wow6432Node" : "");
    versions = mprListRegistry(key);
    mprSortList(versions, (MprSortProc) reverseSortVersions, 0);
    path = 0;
    for (ITERATE_ITEMS(versions, version, i)) {
        if (scontains(version, "KitsRoot")) {
            path = mprReadRegistry(key, version);
            if (mprPathExists(mprJoinPath(path, "Include"), X_OK)) {
                break;
            }
            path = 0;
        }
    }
    if (!path) {
        /* 
            Next search the registry keys at Windows SDKs/Windows/ * /InstallationFolder
         */
        key = sfmt("HKLM\\SOFTWARE%s\\Microsoft\\Microsoft SDKs\\Windows", (ME_64) ? "\\Wow6432Node" : "");
        versions = mprListRegistry(key);
        mprSortList(versions, (MprSortProc) reverseSortVersions, 0);
        for (ITERATE_ITEMS(versions, version, i)) {
            if ((path = mprReadRegistry(sfmt("%s\\%s", key, version), "InstallationFolder")) != 0) {
                if (mprPathExists(mprJoinPath(path, "Include"), X_OK)) {
                    break;
                }
                path = 0;
            }
        }
    }
    if (!path) {
        /* Last chance: Old Windows SDK 7 registry location */
        path = mprReadRegistry("HKLM\\SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows", "CurrentInstallFolder");
    }
    if (!path) {
        path = "${WINSDK}";
    }
    mprLog("info esp", 5, "Using Windows SDK at %s", path);
    eroute->winsdk = strim(path, "\\", MPR_TRIM_END);
    return eroute->winsdk;
}


static cchar *getWinVer(HttpRoute *route)
{
    MprList     *versions;
    cchar       *winver, *winsdk;

    winsdk = getWinSDK(route);
    versions = mprGlobPathFiles(mprJoinPath(winsdk, "Lib"), "*", MPR_PATH_RELATIVE);
    mprSortList(versions, 0, 0);
    if ((winver = mprGetLastItem(versions)) == 0) {
        winver = sclone("win8");
    }
    return winver;
}
#endif


static cchar *getArPath(cchar *os, cchar *arch)
{
#if WINDOWS
    /* 
        Get the real system architecture (32 or 64 bit)
     */
    Http *http = MPR->httpService;
    cchar *path = espGetVisualStudio();
    if (scontains(http->platform, "-x64-")) {
        int is64BitSystem = smatch(getenv("PROCESSOR_ARCHITECTURE"), "AMD64") || getenv("PROCESSOR_ARCHITEW6432");
        if (is64BitSystem) {
            path = mprJoinPath(path, "VC/bin/amd64/lib.exe");
        } else {
            /* Cross building on a 32-bit system */
            path = mprJoinPath(path, "VC/bin/x86_amd64/lib.exe");
        }
    } else {
        path = mprJoinPath(path, "VC/bin/lib.exe");
    }
    return path;
#else
    return "ar";
#endif
}


static cchar *getCompilerPath(cchar *os, cchar *arch)
{
#if WINDOWS
    /* 
        Get the real system architecture (32 or 64 bit)
     */
    Http *http = MPR->httpService;
    cchar *path = espGetVisualStudio();
    if (scontains(http->platform, "-x64-")) {
        int is64BitSystem = smatch(getenv("PROCESSOR_ARCHITECTURE"), "AMD64") || getenv("PROCESSOR_ARCHITEW6432");
        if (is64BitSystem) {
            path = mprJoinPath(path, "VC/bin/amd64/cl.exe");
        } else {
            /* Cross building on a 32-bit system */
            path = mprJoinPath(path, "VC/bin/x86_amd64/cl.exe");
        }
    } else {
        path = mprJoinPath(path, "VC/bin/cl.exe");
    }
    return path;
#else
    return getCompilerName(os, arch);
#endif
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

/*
    mdb.c -- ESP In-Memory Embedded Database (MDB)

    WARNING: This is prototype code

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "http.h"


#include    "pcre.h"

#if ME_COM_MDB
/************************************* Local **********************************/

#define MDB_LOAD_BEGIN   1      /* Initial state */
#define MDB_LOAD_TABLE   2      /* Parsing a table */
#define MDB_LOAD_HINTS   3      /* Parsing hints */
#define MDB_LOAD_SCHEMA  4      /* Parsing schema */
#define MDB_LOAD_COL     5      /* Parsing column schema */ 
#define MDB_LOAD_DATA    6      /* Parsing data */
#define MDB_LOAD_FIELD   7      /* Parsing fields */

/*
    Operations for mdbReadWhere
 */
#define OP_ERR  -1              /* Illegal operation */
#define OP_EQ   0               /* "==" Equal operation */
#define OP_NEQ  0x2             /* "!=" Not equal operation */
#define OP_LT   0x4             /* "<" Less than operation */
#define OP_GT   0x8             /* ">" Greater than operation */
#define OP_LTE  0x10            /* ">=" Less than or equal operation */
#define OP_GTE  0x20            /* "<=" Greater than or equal operation */

/************************************ Forwards ********************************/

static void autoSave(Mdb *mdb, MdbTable *table);
static MdbCol *createCol(MdbTable *table, cchar *columnName);
static EdiRec *createRecFromRow(Edi *edi, MdbRow *row);
static MdbRow *createRow(Mdb *mdb, MdbTable *table);
static MdbCol *getCol(MdbTable *table, int col);
static MdbRow *getRow(MdbTable *table, int rid);
static MdbTable *getTable(Mdb *mdb, int tid);
static MdbSchema *growSchema(MdbTable *table);
static MdbCol *lookupField(MdbTable *table, cchar *columnName);
static int lookupRow(MdbTable *table, cchar *key);
static MdbTable *lookupTable(Mdb *mdb, cchar *tableName);
static void manageCol(MdbCol *col, int flags);
static void manageMdb(Mdb *mdb, int flags);
static void manageRow(MdbRow *row, int flags);
static void manageSchema(MdbSchema *schema, int flags);
static void manageTable(MdbTable *table, int flags);
static int parseOperation(cchar *operation);
static int updateFieldValue(MdbRow *row, MdbCol *col, cchar *value);
static int mdbAddColumn(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags);
static int mdbAddIndex(Edi *edi, cchar *tableName, cchar *columnName, cchar *indexName);
static int mdbAddTable(Edi *edi, cchar *tableName);
static int mdbChangeColumn(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags);
static void mdbClose(Edi *edi);
static EdiRec *mdbCreateRec(Edi *edi, cchar *tableName);
static int mdbDelete(cchar *path);
static MprList *mdbGetColumns(Edi *edi, cchar *tableName);
static int mdbGetColumnSchema(Edi *edi, cchar *tableName, cchar *columnName, int *type, int *flags, int *cid);
static MprList *mdbGetTables(Edi *edi);
static int mdbGetTableDimensions(Edi *edi, cchar *tableName, int *numRows, int *numCols);
static int mdbLoad(Edi *edi, cchar *path);
static int mdbLoadFromString(Edi *edi, cchar *string);
static int mdbLookupField(Edi *edi, cchar *tableName, cchar *fieldName);
static Edi *mdbOpen(cchar *path, int flags);
static EdiGrid *mdbQuery(Edi *edi, cchar *cmd, int argc, cchar **argv, va_list vargs);
static EdiField mdbReadField(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName);
static EdiRec *mdbReadRec(Edi *edi, cchar *tableName, cchar *key);
static EdiGrid *mdbReadWhere(Edi *edi, cchar *tableName, cchar *columnName, cchar *operation, cchar *value);
static int mdbRemoveColumn(Edi *edi, cchar *tableName, cchar *columnName);
static int mdbRemoveIndex(Edi *edi, cchar *tableName, cchar *indexName);
static int mdbRemoveRec(Edi *edi, cchar *tableName, cchar *key);
static int mdbRemoveTable(Edi *edi, cchar *tableName);
static int mdbRenameTable(Edi *edi, cchar *tableName, cchar *newTableName);
static int mdbRenameColumn(Edi *edi, cchar *tableName, cchar *columnName, cchar *newColumnName);
static int mdbSave(Edi *edi);
static int mdbUpdateField(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName, cchar *value);
static int mdbUpdateRec(Edi *edi, EdiRec *rec);

static EdiProvider MdbProvider = {
    "mdb",
    mdbAddColumn, mdbAddIndex, mdbAddTable, mdbChangeColumn, mdbClose, mdbCreateRec, mdbDelete, 
    mdbGetColumns, mdbGetColumnSchema, mdbGetTables, mdbGetTableDimensions, mdbLoad, mdbLookupField, mdbOpen, mdbQuery, 
    mdbReadField, mdbReadRec, mdbReadWhere, mdbRemoveColumn, mdbRemoveIndex, mdbRemoveRec, mdbRemoveTable, 
    mdbRenameTable, mdbRenameColumn, mdbSave, mdbUpdateField, mdbUpdateRec,
};

/************************************* Code ***********************************/

PUBLIC void mdbInit()
{
    ediAddProvider(&MdbProvider);
}


static Mdb *mdbAlloc(cchar *path, int flags)
{
    Mdb      *mdb;

    if ((mdb = mprAllocObj(Mdb, manageMdb)) == 0) {
        return 0;
    }
    mdb->edi.provider = &MdbProvider;
    mdb->edi.flags = flags;
    mdb->edi.path = sclone(path);
    mdb->edi.schemaCache = mprCreateHash(0, 0);
    mdb->edi.mutex = mprCreateLock();
    mdb->edi.validations = mprCreateHash(0, 0);
    return mdb;
}


static void manageMdb(Mdb *mdb, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(mdb->edi.path);
        mprMark(mdb->edi.schemaCache);
        mprMark(mdb->edi.errMsg);
        mprMark(mdb->edi.validations);
        mprMark(mdb->edi.mutex);
        mprMark(mdb->tables);
        /* Don't mark load fields */
    } else {
        mdbClose((Edi*) mdb);
    }
}


static void mdbClose(Edi *edi)
{
    Mdb     *mdb;
   
    mdb = (Mdb*) edi;
    autoSave(mdb, 0);
    mdb->tables = 0;
}


static EdiRec *mdbCreateRec(Edi *edi, cchar *tableName)
{
    Mdb         *mdb;
    MdbTable    *table;
    MdbCol      *col;
    EdiRec      *rec;
    EdiField    *fp;
    int         f, nfields;

    mdb = (Mdb*) edi;
    if ((table = lookupTable(mdb, tableName)) == 0) {
        mprLog("error esp mdb", 0, "Cannot find table %s", tableName);
        return 0;
    }
    nfields = max(table->schema->ncols, 1);
    if ((rec = mprAllocBlock(sizeof(EdiRec) + sizeof(EdiField) * nfields, MPR_ALLOC_MANAGER | MPR_ALLOC_ZERO)) == 0) {
        return 0;
    }
    mprSetManager(rec, (MprManager) ediManageEdiRec);

    rec->edi = edi;
    rec->tableName = table->name;
    rec->nfields = nfields;

    for (f = 0; f < nfields; f++) {
        col = getCol(table, f);
        fp = &rec->fields[f];
        fp->type = col->type;
        fp->name = col->name;
        fp->flags = col->flags;
    }
    return rec;
}


static int mdbDelete(cchar *path)
{
    return mprDeletePath(path);
}


static Edi *mdbOpen(cchar *source, int flags)
{
    Mdb     *mdb;

    if (flags & EDI_LITERAL) {
        flags |= EDI_NO_SAVE;
        if ((mdb = mdbAlloc("literal", flags)) == 0) {
            return 0;
        }
        if (mdbLoadFromString((Edi*) mdb, source) < 0) {
            return 0;
        }
    } else {
        if ((mdb = mdbAlloc(source, flags)) == 0) {
            return 0;
        }
        if (!mprPathExists(source, R_OK)) {
            if (flags & EDI_CREATE) {
                mdbSave((Edi*) mdb);
            } else {
                return 0;
            }
        }
        if (mdbLoad((Edi*) mdb, source) < 0) {
            return 0;
        }
    }
    return (Edi*) mdb;
}


static int mdbAddColumn(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags)
{
    Mdb         *mdb;
    MdbTable    *table;
    MdbCol      *col;

    assert(edi);
    assert(tableName && *tableName);
    assert(columnName && *columnName);
    assert(type);

    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if ((col = lookupField(table, columnName)) != 0) {
        unlock(edi);
        return MPR_ERR_ALREADY_EXISTS;
    }
    if ((col = createCol(table, columnName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    col->type = type;
    col->flags = flags;
    if (flags & EDI_INDEX) {
        if (table->index) {
            mprLog("warn esp mdb", 0, "Index already specified in table %s, replacing.", tableName);
        }
        if ((table->index = mprCreateHash(0, MPR_HASH_STATIC_VALUES | MPR_HASH_STABLE)) != 0) {
            table->indexCol = col;
        }
    }
    autoSave(mdb, table);
    unlock(edi);
    return 0;

}


/*
    IndexName is not implemented yet
 */
static int mdbAddIndex(Edi *edi, cchar *tableName, cchar *columnName, cchar *indexName)
{
    Mdb         *mdb;
    MdbTable    *table;
    MdbCol      *col;

    assert(edi);
    assert(tableName && *tableName);
    assert(columnName && *columnName);

    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if ((col = lookupField(table, columnName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if ((table->index = mprCreateHash(0, MPR_HASH_STATIC_VALUES | MPR_HASH_STABLE)) == 0) {
        unlock(edi);
        return MPR_ERR_MEMORY;
    }
    table->indexCol = col;
    col->flags |= EDI_INDEX;
    autoSave(mdb, table);
    unlock(edi);
    return 0;
}


static int mdbAddTable(Edi *edi, cchar *tableName)
{
    Mdb         *mdb;
    MdbTable    *table;

    assert(edi);
    assert(tableName && *tableName);

    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) != 0) {
        unlock(edi);
        return MPR_ERR_ALREADY_EXISTS;
    }
    if ((table = mprAllocObj(MdbTable, manageTable)) == 0) {
        unlock(edi);
        return MPR_ERR_MEMORY;
    }
    if ((table->rows = mprCreateList(0, MPR_LIST_STABLE)) == 0) {
        unlock(edi);
        return MPR_ERR_MEMORY;
    }
    table->name = sclone(tableName);
    if (mdb->tables == 0) {
        mdb->tables = mprCreateList(0, MPR_LIST_STABLE);
    }
    if (!growSchema(table)) {
        unlock(edi);
        return MPR_ERR_MEMORY;
    }
    mprAddItem(mdb->tables, table);
    autoSave(mdb, lookupTable(mdb, tableName));
    unlock(edi);
    return 0;
}


static int mdbChangeColumn(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags)
{
    Mdb         *mdb;
    MdbTable    *table;
    MdbCol      *col;

    assert(edi);
    assert(tableName && *tableName);
    assert(columnName && *columnName);
    assert(type);

    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if ((col = lookupField(table, columnName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    col->name = sclone(columnName);
    col->type = type;
    autoSave(mdb, table);
    unlock(edi);
    return 0;
}


/*
    Return a list of column names
 */
static MprList *mdbGetColumns(Edi *edi, cchar *tableName)
{
    Mdb         *mdb;
    MdbTable    *table;
    MdbSchema   *schema;
    MprList     *list;
    int         i;

    assert(edi);
    assert(tableName && *tableName);

    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return 0;
    }
    schema = table->schema;
    assert(schema);
    list = mprCreateList(schema->ncols, MPR_LIST_STABLE);
    for (i = 0; i < schema->ncols; i++) {
        /* No need to clone */
        mprAddItem(list, schema->cols[i].name);
    }
    unlock(edi);
    return list;
}


/*
    Make a field. WARNING: the value is not cloned
 */
static EdiField makeFieldFromRow(MdbRow *row, MdbCol *col)
{
    EdiField    f;

    /* Note: the value is not cloned */
    f.value = row->fields[col->cid];
    f.type = col->type;
    f.name = col->name;
    f.flags = col->flags;
    f.valid = 1;
    return f;
}


static int mdbGetColumnSchema(Edi *edi, cchar *tableName, cchar *columnName, int *type, int *flags, int *cid)
{
    Mdb         *mdb;
    MdbTable    *table;
    MdbCol      *col;

    mdb = (Mdb*) edi;
    if (type) {
        *type = -1;
    }
    if (cid) {
        *cid = -1;
    }
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if ((col = lookupField(table, columnName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if (type) {
        *type = col->type;
    }
    if (flags) {
        *flags = col->flags;
    }
    if (cid) {
        *cid = col->cid;
    }
    unlock(edi);
    return 0;
}


static MprList *mdbGetTables(Edi *edi)
{
    Mdb         *mdb;
    MprList     *list;
    MdbTable     *table;
    int         tid, ntables;

    mdb = (Mdb*) edi;
    lock(edi);
    list = mprCreateList(-1, MPR_LIST_STABLE);
    ntables = mprGetListLength(mdb->tables);
    for (tid = 0; tid < ntables; tid++) {
        table = mprGetItem(mdb->tables, tid);
        mprAddItem(list, table->name);
    }
    unlock(edi);
    return list;
}


static int mdbGetTableDimensions(Edi *edi, cchar *tableName, int *numRows, int *numCols)
{
    Mdb         *mdb;
    MdbTable    *table;

    mdb = (Mdb*) edi;
    lock(edi);
    if (numRows) {
        *numRows = 0;
    }
    if (numCols) {
        *numCols = 0;
    }
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if (numRows) {
        *numRows = mprGetListLength(table->rows);
    }
    if (numCols) {
        *numCols = table->schema->ncols;
    }
    unlock(edi);
    return 0;
}


static int mdbLoad(Edi *edi, cchar *path)
{
    cchar       *data;
    ssize       len;

    if ((data = mprReadPathContents(path, &len)) == 0) {
        return MPR_ERR_CANT_READ;
    }
    return mdbLoadFromString(edi, data);
}


static int mdbLookupField(Edi *edi, cchar *tableName, cchar *fieldName)
{
    Mdb         *mdb;
    MdbTable    *table;
    MdbCol      *col;

    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if ((col = lookupField(table, fieldName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    unlock(edi);
    return col->cid;
}


static EdiGrid *mdbQuery(Edi *edi, cchar *cmd, int argc, cchar **argv, va_list vargs)
{
    mprLog("error esp mdb", 0, "MDB does not implement ediQuery");
    return 0;
}


static EdiField mdbReadField(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName)
{
    Mdb         *mdb;
    MdbTable    *table;
    MdbCol      *col;
    MdbRow      *row;
    EdiField    field, err;
    int         r;

    mdb = (Mdb*) edi;
    lock(edi);
    err.valid = 0;
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return err;
    }
    if ((col = lookupField(table, fieldName)) == 0) {
        unlock(edi);
        return err;
    }
    if ((r = lookupRow(table, key)) < 0) {
        unlock(edi);
        return err;
    }
    row = mprGetItem(table->rows, r);
    field = makeFieldFromRow(row, col);
    unlock(edi);
    return field;
}


static EdiRec *mdbReadRec(Edi *edi, cchar *tableName, cchar *key)
{
    Mdb         *mdb;
    MdbTable    *table;
    MdbRow      *row;
    EdiRec      *rec;
    int         r;

    mdb = (Mdb*) edi;
    rec = 0;

    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return 0;
    }
    if ((r = lookupRow(table, key)) < 0) {
        unlock(edi);
        return 0;
    }
    if ((row = mprGetItem(table->rows, r)) != 0) {
        rec = createRecFromRow(edi, row);
    }
    unlock(edi);
    return rec;
}



static bool matchRow(MdbCol *col, cchar *existing, int op, cchar *value)
{
    if (value == 0 || *value == '\0') {
        return 0;
    }
    switch (op) {
    case OP_EQ:
        if (smatch(existing, value)) {
            return 1;
        }
        break;
    case OP_NEQ:
        if (!smatch(existing, value)) {
            return 1;
        }
        break;
#if FUTURE
    case OP_LT:
    case OP_GT:
    case OP_LTE:
    case OP_GTE:
#endif
    default:
        assert(0);
    }
    return 0;
}


static EdiGrid *mdbReadWhere(Edi *edi, cchar *tableName, cchar *columnName, cchar *operation, cchar *value)
{
    Mdb         *mdb;
    EdiGrid     *grid;
    MdbTable    *table;
    MdbCol      *col;
    MdbRow      *row;
    int         nrows, next, op, r, count;

    assert(edi);
    assert(tableName && *tableName);

    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return 0;
    }
    nrows = mprGetListLength(table->rows);
    if ((grid = ediCreateBareGrid(edi, tableName, nrows)) == 0) {
        unlock(edi);
        return 0;
    }
    grid->flags = EDI_GRID_READ_ONLY;
    if (columnName) {
        if ((col = lookupField(table, columnName)) == 0) {
            unlock(edi);
            return 0;
        }
        if ((op = parseOperation(operation)) < 0) {
            unlock(edi);
            return 0;
        }
        if (col->flags & EDI_INDEX && (op == OP_EQ)) {
            if ((r = lookupRow(table, value)) >= 0) {
                row = getRow(table, r);
                grid->records[0] = createRecFromRow(edi, row);
                grid->nrecords = 1;
            }
        } else {
            grid->nrecords = count = 0;
            for (ITERATE_ITEMS(table->rows, row, next)) {
                if (!matchRow(col, row->fields[col->cid], op, value)) {
                    continue;
                }
                grid->records[count++] = createRecFromRow(edi, row);
                grid->nrecords = count;
            }
        }
    } else {
        for (ITERATE_ITEMS(table->rows, row, next)) {
            grid->records[next - 1] = createRecFromRow(edi, row);
        }
        grid->nrecords = next;
    }
    unlock(edi);
    return grid;
}


static int mdbRemoveColumn(Edi *edi, cchar *tableName, cchar *columnName)
{
    Mdb         *mdb;
    MdbTable    *table;
    MdbSchema   *schema;
    MdbCol      *col;
    int         c;

    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if ((col = lookupField(table, columnName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if (table->indexCol == col) {
        table->index = 0;
        table->indexCol = 0;
    }
    if (table->keyCol == col) {
        table->keyCol = 0;
    }
    schema = table->schema;
    assert(schema);
    for (c = col->cid; c < schema->ncols; c++) {
        schema->cols[c] = schema->cols[c + 1];
    }
    schema->ncols--;
    schema->cols[schema->ncols].name = 0;
    assert(schema->ncols >= 0);
    autoSave(mdb, table);
    unlock(edi);
    return 0;
}


static int mdbRemoveIndex(Edi *edi, cchar *tableName, cchar *indexName)
{
    Mdb         *mdb;
    MdbTable    *table;

    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    table->index = 0;
    if (table->indexCol) {
        table->indexCol->flags &= ~EDI_INDEX;
        table->indexCol = 0;
        autoSave(mdb, table);
    }
    unlock(edi);
    return 0;
}


static int mdbRemoveRec(Edi *edi, cchar *tableName, cchar *key)
{
    Mdb         *mdb;
    MdbTable    *table;
    MprKey      *kp;
    int         r, rc, value;

    assert(edi);
    assert(tableName && *tableName);
    assert(key && *key);

    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if ((r = lookupRow(table, key)) < 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    rc = mprRemoveItemAtPos(table->rows, r);
    if (table->index) {
        mprRemoveKey(table->index, key);
        for (ITERATE_KEYS(table->index, kp)) {
            value = (int) PTOL(kp->data);
            if (value >= r) {
                mprAddKey(table->index, kp->key, LTOP(value - 1));
            }
        }
    }
    autoSave(mdb, table);
    unlock(edi);
    return rc;
}


static int mdbRemoveTable(Edi *edi, cchar *tableName)
{
    Mdb         *mdb;
    MdbTable    *table;
    int         next;

    mdb = (Mdb*) edi;
    lock(edi);
    for (ITERATE_ITEMS(mdb->tables, table, next)) {
        if (smatch(table->name, tableName)) {
            mprRemoveItem(mdb->tables, table);
            autoSave(mdb, table);
            unlock(edi);
            return 0;
        }
    }
    unlock(edi);
    return MPR_ERR_CANT_FIND;
}


static int mdbRenameTable(Edi *edi, cchar *tableName, cchar *newTableName)
{
    Mdb         *mdb;
    MdbTable    *table;

    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    table->name = sclone(newTableName);
    autoSave(mdb, table);
    unlock(edi);
    return 0;
}


static int mdbRenameColumn(Edi *edi, cchar *tableName, cchar *columnName, cchar *newColumnName)
{
    Mdb         *mdb;
    MdbTable    *table;
    MdbCol      *col;

    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if ((col = lookupField(table, columnName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    col->name = sclone(newColumnName);
    autoSave(mdb, table);
    unlock(edi);
    return 0;
}


static int mdbUpdateField(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName, cchar *value)
{
    Mdb         *mdb;
    MdbTable    *table;
    MdbRow      *row;
    MdbCol      *col;
    int         r;

    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, tableName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if ((col = lookupField(table, fieldName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if ((r = lookupRow(table, key)) < 0) {
        row = createRow(mdb, table);
    }
    if ((row = getRow(table, r)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    updateFieldValue(row, col, value);
    autoSave(mdb, table);
    unlock(edi);
    return 0;
}


static int mdbUpdateRec(Edi *edi, EdiRec *rec)
{
    Mdb         *mdb;
    MdbTable    *table;
    MdbRow      *row;
    MdbCol      *col;
    int         f, r;

    if (!ediValidateRec(rec)) {
        return MPR_ERR_CANT_WRITE;
    }
    mdb = (Mdb*) edi;
    lock(edi);
    if ((table = lookupTable(mdb, rec->tableName)) == 0) {
        unlock(edi);
        return MPR_ERR_CANT_FIND;
    }
    if (rec->id == 0 || (r = lookupRow(table, rec->id)) < 0) {
        row = createRow(mdb, table);
    } else {
        if ((row = getRow(table, r)) == 0) {
            unlock(edi);
            return MPR_ERR_CANT_FIND;
        }
    }
    for (f = 0; f < rec->nfields; f++) {
        if ((col = getCol(table, f)) != 0) {
            updateFieldValue(row, col, rec->fields[f].value);
        }
    }
    autoSave(mdb, table);
    unlock(edi);
    return 0;
}


/******************************** Database Loading ***************************/

static void clearLoadState(Mdb *mdb)
{
    mdb->loadNcols = 0;
    mdb->loadCol = 0;
    mdb->loadRow = 0;
}


static void pushState(Mdb *mdb, int state)
{
    mprPushItem(mdb->loadStack, LTOP(state));
    mdb->loadState = state;
}


static void popState(Mdb *mdb)
{
    mprPopItem(mdb->loadStack);
    mdb->loadState = (int) PTOL(mprGetLastItem(mdb->loadStack));
    assert(mdb->loadState > 0);
}


static int checkMdbState(MprJsonParser *jp, cchar *name, bool leave)
{
    Mdb     *mdb;

    mdb = jp->data;
    if (leave) {
        popState(mdb);
        return 0;
    }
    switch (mdb->loadState) {
    case MDB_LOAD_BEGIN:
        if (mdbAddTable((Edi*) mdb, name) < 0) {
            return MPR_ERR_MEMORY;
        }
        mdb->loadTable = lookupTable(mdb, name);
        clearLoadState(mdb);
        pushState(mdb, MDB_LOAD_TABLE);
        break;
        
    case MDB_LOAD_TABLE:
        if (smatch(name, "hints")) {
            pushState(mdb, MDB_LOAD_HINTS);
        } else if (smatch(name, "schema")) {
            pushState(mdb, MDB_LOAD_SCHEMA);
        } else if (smatch(name, "data")) {
            pushState(mdb, MDB_LOAD_DATA);
        } else {
            mprSetJsonError(jp, "Bad property '%s'", name);
            return MPR_ERR_BAD_FORMAT;
        }
        break;
    
    case MDB_LOAD_SCHEMA:
        if ((mdb->loadCol = createCol(mdb->loadTable, name)) == 0) {
            mprSetJsonError(jp, "Cannot create '%s' column", name);
            return MPR_ERR_MEMORY;
        }
        pushState(mdb, MDB_LOAD_COL);
        break;

    case MDB_LOAD_DATA:
        if ((mdb->loadRow = createRow(mdb, mdb->loadTable)) == 0) {
            return MPR_ERR_MEMORY;
        }
        mdb->loadCid = 0;
        pushState(mdb, MDB_LOAD_FIELD);
        break;

    case MDB_LOAD_HINTS:
    case MDB_LOAD_COL:
    case MDB_LOAD_FIELD:
        pushState(mdb, mdb->loadState);
        break;

    default:
        mprSetJsonError(jp, "Potential corrupt data. Bad state");
        return MPR_ERR_BAD_FORMAT;
    }
    return 0;
}


static int setMdbValue(MprJsonParser *parser, MprJson *obj, cchar *name, MprJson *child)
{
    Mdb         *mdb;
    MdbCol      *col;
    cchar       *value;

    mdb = parser->data;
    value = child->value;
    
    switch (mdb->loadState) {
    case MDB_LOAD_BEGIN:
    case MDB_LOAD_TABLE:
    case MDB_LOAD_SCHEMA:
    case MDB_LOAD_DATA:
        break;

    case MDB_LOAD_HINTS:
        if (smatch(name, "ncols")) {
            mdb->loadNcols = atoi(value);
        } else {
            mprSetJsonError(parser, "Unknown hint '%s'", name);
            return MPR_ERR_BAD_FORMAT;
        }
        break;

    case MDB_LOAD_COL:
        if (smatch(name, "index")) {
            mdbAddIndex((Edi*) mdb, mdb->loadTable->name, mdb->loadCol->name, NULL);
        } else if (smatch(name, "type")) {
            if ((mdb->loadCol->type = ediParseTypeString(value)) <= 0) {
                mprSetJsonError(parser, "Bad column type %s", value);
                return MPR_ERR_BAD_FORMAT;
            }
        } else if (smatch(name, "key")) {
            mdb->loadCol->flags |= EDI_KEY;
            mdb->loadTable->keyCol = mdb->loadCol;
        } else if (smatch(name, "autoinc")) {
            mdb->loadCol->flags |= EDI_AUTO_INC;
        } else if (smatch(name, "foreign")) {
            mdb->loadCol->flags |= EDI_FOREIGN;
        } else if (smatch(name, "notnull")) {
            mdb->loadCol->flags |= EDI_NOT_NULL;
        } else {
            mprSetJsonError(parser, "Bad property '%s' in column definition", name);
            return MPR_ERR_BAD_FORMAT;
        }
        break;

    case MDB_LOAD_FIELD:
        if ((col = getCol(mdb->loadTable, mdb->loadCid++)) == 0) {
            mprSetJsonError(parser, "Bad state '%d' in setMdbValue, column %s,  potential corrupt data", mdb->loadState, name);
            return MPR_ERR_BAD_FORMAT;
        }
        updateFieldValue(mdb->loadRow, col, value);
        break;

    default:
        mprSetJsonError(parser, "Bad state '%d' in setMdbValue potential corrupt data", mdb->loadState);
        return MPR_ERR_BAD_FORMAT;
    }
    return 0;
}


static int mdbLoadFromString(Edi *edi, cchar *str)
{
    Mdb             *mdb;
    MprJson         *obj;
    MprJsonCallback cb;

    mdb = (Mdb*) edi;
    mdb->edi.flags |= EDI_SUPPRESS_SAVE;
    mdb->edi.flags |= MDB_LOADING;
    mdb->loadStack = mprCreateList(0, MPR_LIST_STABLE);
    pushState(mdb, MDB_LOAD_BEGIN);

    memset(&cb, 0, sizeof(cb));
    cb.checkBlock = checkMdbState;
    cb.setValue = setMdbValue;

    obj = mprParseJsonEx(str, &cb, mdb, 0, 0);
    mdb->edi.flags &= ~MDB_LOADING;
    mdb->loadStack = 0;
    if (obj == 0) {
        return MPR_ERR_CANT_LOAD;
    }
    mdb->edi.flags &= ~EDI_SUPPRESS_SAVE;
    return 0;
}


/******************************** Database Saving ****************************/

static void autoSave(Mdb *mdb, MdbTable *table)
{
    assert(mdb);

    if (mdb->edi.flags & EDI_NO_SAVE) {
        return;
    }
    if (mdb->edi.flags & EDI_AUTO_SAVE && !(mdb->edi.flags & EDI_SUPPRESS_SAVE)) {
        if (mdbSave((Edi*) mdb) < 0) {
            mprLog("error esp mdb", 0, "Cannot save database %s", mdb->edi.path);
        }
    }
}


/*
    Must be called locked
 */
static int mdbSave(Edi *edi)
{
    Mdb         *mdb;
    MdbSchema   *schema;
    MdbTable    *table;
    MdbRow      *row;
    MdbCol      *col;
    cchar       *value, *path, *cp;
    char        *npath, *bak, *type;
    MprFile     *out;
    int         cid, rid, tid, ntables, nrows;

    mdb = (Mdb*) edi;
    if (mdb->edi.flags & EDI_NO_SAVE) {
        return MPR_ERR_BAD_STATE;
    }
    path = mdb->edi.path;
    if (path == 0) {
        mprLog("error esp mdb", 0, "No database path specified");
        return MPR_ERR_BAD_ARGS;
    }
    npath = mprReplacePathExt(path, "new");
    if ((out = mprOpenFile(npath, O_WRONLY | O_TRUNC | O_CREAT | O_BINARY, 0664)) == 0) {
        mprLog("error esp mdb", 0, "Cannot open database %s", npath);
        return 0;
    }
    mprEnableFileBuffering(out, 0, 0);
    mprWriteFileFmt(out, "{\n");

    ntables = mprGetListLength(mdb->tables);
    for (tid = 0; tid < ntables; tid++) {
        table = getTable(mdb, tid);
        schema = table->schema;
        assert(schema);
        mprWriteFileFmt(out, "    '%s': {\n", table->name);
        mprWriteFileFmt(out, "        hints: {\n            ncols: %d\n        },\n", schema->ncols);
        mprWriteFileString(out, "        schema: {\n");
        /* Skip the id which is always the first column */
        for (cid = 0; cid < schema->ncols; cid++) {
            col = getCol(table, cid);
            type = ediGetTypeString(col->type);
            mprWriteFileFmt(out, "            '%s': { type: '%s'", col->name, type);
            if (col->flags & EDI_AUTO_INC) {
                mprWriteFileString(out, ", autoinc: true");
            }
            if (col->flags & EDI_INDEX) {
                mprWriteFileString(out, ", index: true");
            }
            if (col->flags & EDI_KEY) {
                mprWriteFileString(out, ", key: true");
            }
            if (col->flags & EDI_FOREIGN) {
                mprWriteFileString(out, ", foreign: true");
            }
            if (col->flags & EDI_NOT_NULL) {
                mprWriteFileString(out, ", notnull: true");
            }
            mprWriteFileString(out, " },\n");
        }
        mprWriteFileString(out, "        },\n");
        mprWriteFileString(out, "        data: [\n");

        nrows = mprGetListLength(table->rows);
        for (rid = 0; rid < nrows; rid++) {
            mprWriteFileString(out, "            [ ");
            row = getRow(table, rid);
            for (cid = 0; cid < schema->ncols; cid++) {
                col = getCol(table, cid);
                value = row->fields[col->cid];
                if (value == 0 && col->flags & EDI_AUTO_INC) {
                    row->fields[col->cid] = itos(++col->lastValue);
                }
                if (value == 0) {
                    mprWriteFileFmt(out, "null, ");
                } else if (col->type == EDI_TYPE_STRING || col->type == EDI_TYPE_TEXT) {
                    mprWriteFile(out, "'", 1);
                    /*
                        The MPR JSON parser is tolerant of embedded, unquoted control characters. So only need
                        to worry about embedded single quotes and back quote.
                     */
                    for (cp = value; *cp; cp++) {
                        if (*cp == '\'' || *cp == '\\') {
                            mprWriteFile(out, "\\", 1);
                        }
                        mprWriteFile(out, cp, 1);
                    }
                    mprWriteFile(out, "',", 2);
                } else {
                    for (cp = value; *cp; cp++) {
                        if (*cp == '\'' || *cp == '\\') {
                            mprWriteFile(out, "\\", 1);
                        }
                        mprWriteFile(out, cp, 1);
                    }
                    mprWriteFile(out, ",", 1);
                }
            }
            mprWriteFileString(out, "],\n");
        }
        mprWriteFileString(out, "        ],\n    },\n");
    }
    mprWriteFileString(out, "}\n");
    mprCloseFile(out);

    bak = mprReplacePathExt(path, "bak");
    mprDeletePath(bak);
    if (mprPathExists(path, R_OK) && rename(path, bak) < 0) {
        mprLog("error esp mdb", 0, "Cannot rename %s to %s", path, bak);
        return MPR_ERR_CANT_WRITE;
    }
    if (rename(npath, path) < 0) {
        mprLog("error esp mdb", 0, "Cannot rename %s to %s", npath, path);
        /* Restore backup */
        rename(bak, path);
        return MPR_ERR_CANT_WRITE;
    }
    return 0;
}


/********************************* Table Operations **************************/

static MdbTable *getTable(Mdb *mdb, int tid)
{
    int         ntables;

    ntables = mprGetListLength(mdb->tables);
    if (tid < 0 || tid >= ntables) {
        return 0;
    }
    return mprGetItem(mdb->tables, tid);
}


static MdbTable *lookupTable(Mdb *mdb, cchar *tableName)
{
    MdbTable     *table;
    int         tid, ntables;

    ntables = mprGetListLength(mdb->tables);
    for (tid = 0; tid < ntables; tid++) {
        table = mprGetItem(mdb->tables, tid);
        if (smatch(table->name, tableName)) {
            return table;
        }
    }
    return 0;
}


static void manageTable(MdbTable *table, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(table->name);
        mprMark(table->schema);
        mprMark(table->index);
        mprMark(table->rows);
        /*
            Do not mark keyCol or indexCol - they are unmanaged
         */
    }
}


static int lookupRow(MdbTable *table, cchar *key)
{
    MprKey      *kp;
    MdbRow      *row;
    int         nrows, r, keycol;

    if (table->index) {
        if ((kp = mprLookupKeyEntry(table->index, key)) != 0) {
            return (int) PTOL(kp->data);
        } 
    } else {
        nrows = mprGetListLength(table->rows);
        keycol = table->keyCol ? table->keyCol->cid : 0;
        for (r = 0; r < nrows; r++) {
            row = mprGetItem(table->rows, r);
            if (smatch(row->fields[keycol], key)) {
                return r;
            }
        }
    }
    return -1;
}


/********************************* Schema / Col ****************************/

static MdbSchema *growSchema(MdbTable *table)
{
    if (table->schema == 0) {
        if ((table->schema = mprAllocBlock(sizeof(MdbSchema) + 
                sizeof(MdbCol) * MDB_INCR, MPR_ALLOC_MANAGER | MPR_ALLOC_ZERO)) == 0) {
            return 0;
        }
        mprSetManager(table->schema, (MprManager) manageSchema);
        table->schema->capacity = MDB_INCR;

    } else if (table->schema->ncols >= table->schema->capacity) {
        if ((table->schema = mprRealloc(table->schema, sizeof(MdbSchema) + 
                (sizeof(MdbCol) * (table->schema->capacity + MDB_INCR)))) == 0) {
            return 0;
        }
        memset(&table->schema->cols[table->schema->capacity], 0, MDB_INCR * sizeof(MdbCol));
        table->schema->capacity += MDB_INCR;
    }
    return table->schema;
}


static MdbCol *createCol(MdbTable *table, cchar *columnName)
{
    MdbSchema    *schema;
    MdbCol       *col;

    if ((col = lookupField(table, columnName)) != 0) {
        return 0;
    }
    if ((schema = growSchema(table)) == 0) {
        return 0;
    }
    col = &schema->cols[schema->ncols];
    col->cid = schema->ncols++;
    col->name = sclone(columnName);
    return col;
}


static void manageSchema(MdbSchema *schema, int flags) 
{
    int     i;

    if (flags & MPR_MANAGE_MARK) {
        for (i = 0; i < schema->ncols; i++) {
            manageCol(&schema->cols[i], flags);
        }
    }
}


static void manageCol(MdbCol *col, int flags) 
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(col->name);
    }
}


static MdbCol *getCol(MdbTable *table, int col)
{
    if (col < 0 || col >= table->schema->ncols) {
        return 0;
    }
    return &table->schema->cols[col];
}


static MdbCol *lookupField(MdbTable *table, cchar *columnName)
{
    MdbSchema    *schema;
    MdbCol       *col;

    if ((schema = growSchema(table)) == 0) {
        return 0;
    }
    for (col = schema->cols; col < &schema->cols[schema->ncols]; col++) {
        if (smatch(col->name, columnName)) {
            return col;
        }
    }
    return 0;
}


/********************************* Row Operations **************************/

static MdbRow *createRow(Mdb *mdb, MdbTable *table)
{
    MdbRow      *row;
    int         ncols;

    ncols = max(table->schema->ncols, 1);
    if ((row = mprAllocBlock(sizeof(MdbRow) + sizeof(EdiField) * ncols, MPR_ALLOC_MANAGER | MPR_ALLOC_ZERO)) == 0) {
        return 0;
    }
    mprSetManager(row, (MprManager) manageRow);
    row->table = table;
    row->nfields = ncols;
    row->rid = mprAddItem(table->rows, row);
    return row;
}


static void manageRow(MdbRow *row, int flags)
{
    int     fid;

    if (flags & MPR_MANAGE_MARK) {
        mprMark(row->table);
        for (fid = 0; fid < row->nfields; fid++) {
            mprMark(row->fields[fid]);
        }
    }
}


static MdbRow *getRow(MdbTable *table, int rid)
{
    int     nrows;

    nrows = mprGetListLength(table->rows);
    if (rid < 0 || rid > nrows) {
        return 0;
    }
    return mprGetItem(table->rows, rid);
}

/********************************* Field Operations ************************/

static cchar *mapMdbValue(cchar *value, int type)
{
    MprTime     time;

    if (value == 0) {
        return value;
    }
    switch (type) {
    case EDI_TYPE_DATE:
        if (!snumber(value)) {
            mprParseTime(&time, value, MPR_UTC_TIMEZONE, 0);
            value = itos(time);
        }
        break;

    case EDI_TYPE_BINARY:
    case EDI_TYPE_BOOL:
    case EDI_TYPE_FLOAT:
    case EDI_TYPE_INT:
    case EDI_TYPE_STRING:
    case EDI_TYPE_TEXT:
    default:
        break;
    }
    return sclone(value);
}

static int updateFieldValue(MdbRow *row, MdbCol *col, cchar *value)
{
    MdbTable    *table;
    cchar       *key;
    
    assert(row);
    assert(col);

    table = row->table;
    if (col->flags & EDI_INDEX) {
        if ((key = row->fields[col->cid]) != 0) {
            mprRemoveKey(table->index, key);
        }
    } else {
        key = 0;
    }
    if (col->flags & EDI_AUTO_INC) {
        if (value == 0) {
            row->fields[col->cid] = value = itos(++col->lastValue);
        } else {
            row->fields[col->cid] = sclone(value);
            col->lastValue = max(col->lastValue, (int64) stoi(value));
        }
    } else {
        row->fields[col->cid] = mapMdbValue(value, col->type);
    }
    if (col->flags & EDI_INDEX && value) {
        mprAddKey(table->index, value, LTOP(row->rid));
    }
    return 0;
}

/*********************************** Support *******************************/
/*
    Optimized record creation
 */
static EdiRec *createRecFromRow(Edi *edi, MdbRow *row)
{
    EdiRec  *rec;
    MdbCol  *col;
    int     c;

    if ((rec = mprAllocBlock(sizeof(EdiRec) + sizeof(EdiField) * row->nfields, MPR_ALLOC_MANAGER | MPR_ALLOC_ZERO)) == 0) {
        return 0;
    }
    mprSetManager(rec, (MprManager) ediManageEdiRec);
    rec->edi = edi;
    rec->tableName = row->table->name;
    rec->id = row->fields[0];
    rec->nfields = row->nfields;
    for (c = 0; c < row->nfields; c++) {
        col = getCol(row->table, c);
        rec->fields[c] = makeFieldFromRow(row, col);
    }
    return rec;
}


static int parseOperation(cchar *operation)
{
    switch (*operation) {
    case '=':
        if (smatch(operation, "==")) {
            return OP_EQ;
        }
        break;
    case '!':
        if (smatch(operation, "=!")) {
            return OP_EQ;
        }
        break;
    case '<':
        if (smatch(operation, "<")) {
            return OP_LT;
        } else if (smatch(operation, "<=")) {
            return OP_LTE;
        }
        break;
    case '>':
        if (smatch(operation, ">")) {
            return OP_GT;
        } else if (smatch(operation, ">=")) {
            return OP_GTE;
        }
    }
    mprLog("error esp mdb", 0, "Unknown read operation '%s'", operation);
    return OP_ERR;
}

#else
/* To prevent ar/ranlib warnings */
PUBLIC void mdbDummy() {}

#endif /* ME_COM_MDB */
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

/*
    sdb.c -- ESP SQLite Database (SDB)

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "http.h"


#if ME_COM_SQLITE
 #include    "sqlite3.h"

#ifndef ME_MAX_SQLITE_MEM
    #define ME_MAX_SQLITE_MEM      (2*1024*1024)   /**< Maximum buffering for Sqlite */
#endif
#ifndef ME_MAX_SQLITE_DURATION
    #define ME_MAX_SQLITE_DURATION 30000           /**< Database busy timeout */
#endif

/************************************* Local **********************************/

typedef struct Sdb {
    Edi             edi;            /**< EDI database interface structure */
    sqlite3         *db;            /**< SQLite database handle */
    MprHash         *schemas;       /**< Table schemas */
} Sdb;

static int sqliteInitialized;
static void initSqlite();

#if KEEP
static char *DataTypeToSqlType[] = {
    "binary":       "blob",
    "boolean":      "tinyint",
    "date":         "date",
    "datetime":     "datetime",
    "decimal":      "decimal",
    "float":        "float",
    "integer":      "int",
    "number":       "decimal",
    "string":       "varchar",
    "text":         "text",
    "time":         "time",
    "timestamp":    "datetime",
    0, 0, 
};
#endif

static char *dataTypeToSqlType[] = {
                            0,
    /* EDI_TYPE_BINARY */   "BLOB",
    /* EDI_TYPE_BOOL */     "TINYINT",      //  "INTEGER",
    /* EDI_TYPE_DATE */     "DATE",         //  "TEXT",
    /* EDI_TYPE_FLOAT */    "FLOAT",        //  "REAL",
    /* EDI_TYPE_INT */      "INTEGER",
    /* EDI_TYPE_STRING */   "STRING",       //  "TEXT",
    /* EDI_TYPE_TEXT */     "TEXT",
                            0,
};

/************************************ Forwards ********************************/

static EdiRec *createBareRec(Edi *edi, cchar *tableName, int nfields);
static EdiField makeRecField(cchar *value, cchar *name, int type);
static void manageSdb(Sdb *sdb, int flags);
static int mapSqliteTypeToEdiType(int type);
static cchar *mapToSqlType(int type);
static int mapToEdiType(cchar *type);
static EdiGrid *query(Edi *edi, cchar *cmd, ...);
static EdiGrid *queryArgv(Edi *edi, cchar *cmd, int argc, cchar **argv, ...);
static EdiGrid *queryv(Edi *edi, cchar *cmd, int argc, cchar **argv, va_list args);
static int sdbAddColumn(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags);
static int sdbAddIndex(Edi *edi, cchar *tableName, cchar *columnName, cchar *indexName);
static int sdbAddTable(Edi *edi, cchar *tableName);
static int sdbChangeColumn(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags);
static void sdbClose(Edi *edi);
static EdiRec *sdbCreateRec(Edi *edi, cchar *tableName);
static int sdbDelete(cchar *path);
static void sdbError(Edi *edi, cchar *fmt, ...);
static int sdbRemoveRec(Edi *edi, cchar *tableName, cchar *key);
static MprList *sdbGetColumns(Edi *edi, cchar *tableName);
static int sdbGetColumnSchema(Edi *edi, cchar *tableName, cchar *columnName, int *type, int *flags, int *cid);
static MprList *sdbGetTables(Edi *edi);
static int sdbGetTableDimensions(Edi *edi, cchar *tableName, int *numRows, int *numCols);
static int sdbLookupField(Edi *edi, cchar *tableName, cchar *fieldName);
static Edi *sdbOpen(cchar *path, int flags);
PUBLIC EdiGrid *sdbQuery(Edi *edi, cchar *cmd, int argc, cchar **argv, va_list vargs);
static EdiField sdbReadField(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName);
static EdiRec *sdbReadRec(Edi *edi, cchar *tableName, cchar *key);
static EdiGrid *sdbReadWhere(Edi *edi, cchar *tableName, cchar *columnName, cchar *operation, cchar *value);
static int sdbRemoveColumn(Edi *edi, cchar *tableName, cchar *columnName);
static int sdbRemoveIndex(Edi *edi, cchar *tableName, cchar *indexName);
static int sdbRemoveTable(Edi *edi, cchar *tableName);
static int sdbRenameTable(Edi *edi, cchar *tableName, cchar *newTableName);
static int sdbRenameColumn(Edi *edi, cchar *tableName, cchar *columnName, cchar *newColumnName);
static int sdbSave(Edi *edi);
static void sdbDebug(Edi *edi, int level, cchar *fmt, ...);
static int sdbUpdateField(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName, cchar *value);
static int sdbUpdateRec(Edi *edi, EdiRec *rec);
static bool validName(cchar *str);

static EdiProvider SdbProvider = {
    "sdb",
    sdbAddColumn, sdbAddIndex, sdbAddTable, sdbChangeColumn, sdbClose, sdbCreateRec, sdbDelete, 
    sdbGetColumns, sdbGetColumnSchema, sdbGetTables, sdbGetTableDimensions, NULL, sdbLookupField, sdbOpen, sdbQuery, 
    sdbReadField, sdbReadRec, sdbReadWhere, sdbRemoveColumn, sdbRemoveIndex, sdbRemoveRec, sdbRemoveTable, 
    sdbRenameTable, sdbRenameColumn, sdbSave, sdbUpdateField, sdbUpdateRec,
};

/************************************* Code ***********************************/

PUBLIC void sdbInit()
{
    ediAddProvider(&SdbProvider);
}


static Sdb *sdbCreate(cchar *path, int flags)
{
    Sdb      *sdb;

    assert(path && *path);

    initSqlite();
    if ((sdb = mprAllocObj(Sdb, manageSdb)) == 0) {
        return 0;
    }
    sdb->edi.flags = flags;
    sdb->edi.provider = &SdbProvider;
    sdb->edi.path = sclone(path);
    sdb->edi.schemaCache = mprCreateHash(0, 0);
    sdb->edi.validations = mprCreateHash(0, 0);
    sdb->edi.mutex = mprCreateLock();
    sdb->schemas = mprCreateHash(0, MPR_HASH_STABLE);
    return sdb;
}


static void manageSdb(Sdb *sdb, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(sdb->edi.path);
        mprMark(sdb->edi.schemaCache);
        mprMark(sdb->edi.errMsg);
        mprMark(sdb->edi.mutex);
        mprMark(sdb->edi.validations);
        mprMark(sdb->schemas);

    } else if (flags & MPR_MANAGE_FREE) {
        sdbClose((Edi*) sdb);
    }
}


static void sdbClose(Edi *edi)
{
    Sdb     *sdb;

    assert(edi);

    sdb = (Sdb*) edi;
    if (sdb->db) {
        sqlite3_close(sdb->db);
        sdb->db = 0;
    }
}


static void removeSchema(Edi *edi, cchar *tableName)
{
    mprRemoveKey(((Sdb*) edi)->schemas, tableName);
}


static EdiRec *getSchema(Edi *edi, cchar *tableName)
{
    Sdb         *sdb;
    EdiRec      *rec, *schema;
    EdiField    *fp;
    EdiGrid     *grid;
    int         r;

    sdb = (Sdb*) edi;
    if (!validName(tableName)) {
        return 0;
    }
    if ((schema = mprLookupKey(sdb->schemas, tableName)) != 0) {
        return schema;
    }
    /*
        Each result row represents an EDI column: CID, Name, Type, NotNull, Dflt_value, PK
     */
    if ((grid = query(edi, sfmt("PRAGMA table_info(%s);", tableName), NULL)) == 0) {
        return 0;
    }
    schema = createBareRec(edi, tableName, grid->nrecords);
    for (r = 0; r < grid->nrecords; r++) {
        rec = grid->records[r];
        fp = &schema->fields[r];
        fp->name = rec->fields[1].value;
        fp->type = mapToEdiType(rec->fields[2].value);
        if (rec->fields[5].value && rec->fields[5].value[0] == '1') {
            fp->flags = EDI_KEY;
        }
    }
    mprAddKey(sdb->schemas, tableName, schema);
    return schema;
}


static EdiRec *sdbCreateRec(Edi *edi, cchar *tableName)
{
    EdiRec  *rec, *schema;
    int     i;

    schema = getSchema(edi, tableName);
    if ((rec = mprAllocBlock(sizeof(EdiRec) + sizeof(EdiField) * schema->nfields, MPR_ALLOC_MANAGER | MPR_ALLOC_ZERO)) == 0) {
        return 0;
    }
    mprSetManager(rec, (MprManager) ediManageEdiRec);
    rec->edi = edi;
    rec->tableName = sclone(tableName);
    rec->nfields = schema->nfields;
    for (i = 0; i < schema->nfields; i++) {
        rec->fields[i].name = schema->fields[i].name;
        rec->fields[i].type = schema->fields[i].type;
        rec->fields[i].flags = schema->fields[i].flags;
    }
    return rec;
}


static int sdbDelete(cchar *path)
{
    assert(path && *path);
    return mprDeletePath(path);
}


static Edi *sdbOpen(cchar *path, int flags)
{
    Sdb     *sdb;

    assert(path && *path);
    if ((sdb = sdbCreate(path, flags)) == 0) {
        return 0;
    }
    if (mprPathExists(path, R_OK) || (flags & EDI_CREATE)) {
        if (sqlite3_open(path, &sdb->db) != SQLITE_OK) {
            mprLog("error esp sdb", 0, "Cannot open database %s", path);
            return 0;
        }
        sqlite3_soft_heap_limit(ME_MAX_SQLITE_MEM);
        sqlite3_busy_timeout(sdb->db, ME_MAX_SQLITE_DURATION);
    } else {
        return 0;
    }
    return (Edi*) sdb;
}


static int sdbAddColumn(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags)
{
    assert(edi);
    assert(tableName && *tableName);
    assert(columnName && *columnName);
    assert(type > 0);

    if (!validName(tableName) || !validName(columnName)) {
        return MPR_ERR_BAD_ARGS;
    }
    if (sdbLookupField(edi, tableName, columnName) >= 0) {
        /* Already exists */
        return 0;
    }
    removeSchema(edi, tableName);
    /*
        The field types are used for the SQLite column affinity settings
     */
    if (query(edi, sfmt("ALTER TABLE %s ADD %s %s", tableName, columnName, mapToSqlType(type)), NULL) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    return 0;
}


static int sdbAddIndex(Edi *edi, cchar *tableName, cchar *columnName, cchar *indexName)
{
    assert(edi);
    assert(tableName && *tableName);
    assert(columnName && *columnName);
    assert(indexName && *indexName);

    if (!validName(tableName) || !validName(columnName) || !validName(indexName)) {
        return MPR_ERR_BAD_ARGS;
    }
    return query(edi, sfmt("CREATE INDEX %s ON %s (%s);", indexName, tableName, columnName), NULL) != 0;
}


static int sdbAddTable(Edi *edi, cchar *tableName)
{
    assert(edi);
    assert(tableName && *tableName);

    if (!validName(tableName)) {
        return MPR_ERR_BAD_ARGS;
    }
    if (query(edi, sfmt("DROP TABLE IF EXISTS %s;", tableName), NULL) == 0) {
        return MPR_ERR_CANT_DELETE;
    }
    /*
        SQLite cannot add a primary key after the table is created
     */
    removeSchema(edi, tableName);
    return query(edi, sfmt("CREATE TABLE %s (id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL);", tableName), NULL) != 0;
}


static int sdbChangeColumn(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags)
{
    mprLog("error esp sdb", 0, "SDB does not support changing columns");
    return MPR_ERR_BAD_STATE;
}


static MprList *sdbGetColumns(Edi *edi, cchar *tableName)
{
    MprList     *result;
    EdiRec      *schema;
    int         c;

    assert(edi);
    assert(tableName && *tableName);
    
    if ((schema = getSchema(edi, tableName)) == 0) {
        return 0;
    }
    if ((result = mprCreateList(0, MPR_LIST_STABLE)) == 0) {
        return 0;
    }
    for (c = 0; c < schema->nfields; c++) {
        mprAddItem(result, schema->fields[c].name);
    }
    return result;
}



static int sdbGetColumnSchema(Edi *edi, cchar *tableName, cchar *columnName, int *type, int *flags, int *cid)
{
    EdiRec      *schema;
    EdiField    *fp;
    int         c;

    assert(edi);
    assert(tableName && *tableName);
    assert(columnName && *columnName);

    schema = getSchema(edi, tableName);
    for (c = 0; c < schema->nfields; c++) {
        fp = &schema->fields[c];
        if (smatch(columnName, fp->name)) {
            if (type) {
                *type = fp->type;
            }
            if (flags) {
                *flags = fp->flags;
            }
            if (cid) {
                *cid = c;
            }
        }
    }
    return 0;
}


static MprList *sdbGetTables(Edi *edi)
{
    EdiGrid     *grid;
    EdiRec      *rec;
    MprList     *result;
    int         r;

    assert(edi);

    if ((grid = query(edi, "SELECT name from sqlite_master WHERE type = 'table' order by NAME;", NULL)) == 0) {
        return 0;
    }
    if ((result = mprCreateList(0, MPR_LIST_STABLE)) == 0) {
        return 0;
    }
    for (r = 0; r < grid->nrecords; r++) {
        rec = grid->records[r];
        if (sstarts(rec->tableName, "sqlite_")) {
            continue;
        }
        mprAddItem(result, rec->tableName);
    }
    return result;
}


static int sdbGetTableDimensions(Edi *edi, cchar *tableName, int *numRows, int *numCols)
{
    EdiGrid     *grid;
    EdiRec      *schema;

    assert(edi);
    assert(tableName && *tableName);

    if (numRows) {
        *numRows = 0;
    }
    if (numCols) {
        *numCols = 0;
    }
    if (!validName(tableName)) {
        return MPR_ERR_BAD_ARGS;
    }
    if (numRows) {
        if ((grid = query(edi, sfmt("SELECT COUNT(*) FROM %s;", tableName), NULL)) == 0) { 
            return MPR_ERR_BAD_STATE;
        }
        *numRows = grid->nrecords;
    }
    if (numCols) {
        if ((schema = getSchema(edi, tableName)) == 0) {
            return MPR_ERR_CANT_FIND;
        }
        *numCols = schema->nfields;
    }
    return 0;
}


static int sdbLookupField(Edi *edi, cchar *tableName, cchar *fieldName)
{
    EdiRec      *schema;
    int         c;

    assert(edi);
    assert(tableName && *tableName);
    assert(fieldName && *fieldName);
    
    if ((schema = getSchema(edi, tableName)) == 0) {
        return 0;
    }
    for (c = 0; c < schema->nfields; c++) {
        /* Name is second field */
        if (smatch(fieldName, schema->fields[c].name)) {
            return c;
        }
    }
    return MPR_ERR_CANT_FIND;
}


PUBLIC EdiGrid *sdbQuery(Edi *edi, cchar *cmd, int argc, cchar **argv, va_list vargs)
{
    return queryv(edi, cmd, argc, argv, vargs);
}


static EdiField sdbReadField(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName)
{
    EdiField    err;
    EdiGrid     *grid;

    if (!validName(tableName) || !validName(fieldName)) {
        err.valid = 0;
        return err;
    }
    if ((grid = query(edi, sfmt("SELECT %s FROM %s WHERE 'id' = ?;", fieldName, tableName), key, NULL)) == 0) {
        err.valid = 0;
        return err;
    }
    return grid->records[0]->fields[0];
}


static EdiRec *sdbReadRec(Edi *edi, cchar *tableName, cchar *key)
{
    EdiGrid     *grid;

    if (!validName(tableName)) {
        return 0;
    }
    if ((grid = query(edi, sfmt("SELECT * FROM %s WHERE id = ?;", tableName), key, NULL)) == 0) {
        return 0;
    }
    if (grid->nrecords == 0) {
        return 0;
    }
    return grid->records[0];
}


static EdiGrid *setTableName(EdiGrid *grid, cchar *tableName)
{
    if (grid && !grid->tableName) {
        grid->tableName = sclone(tableName);
    }
    return grid;
}


static EdiGrid *sdbReadWhere(Edi *edi, cchar *tableName, cchar *columnName, cchar *operation, cchar *value)
{
    EdiGrid     *grid;
    
    assert(tableName && *tableName);

    if (!validName(tableName)) {
        return 0;
    }
    if (columnName) {
        if (!validName(columnName)) {
            return 0;
        }
        assert(columnName && *columnName);
        assert(operation && *operation);
        assert(value);
        grid = query(edi, sfmt("SELECT * FROM %s WHERE %s %s ?;", tableName, columnName, operation), value, NULL);
    } else {
        grid = query(edi, sfmt("SELECT * FROM %s;", tableName), NULL);
    }
    return setTableName(grid, tableName);
}


static int sdbRemoveColumn(Edi *edi, cchar *tableName, cchar *columnName)
{
    mprLog("error esp sdb", 0, "SDB does not support removing columns");
    return MPR_ERR_BAD_STATE;
}


static int sdbRemoveIndex(Edi *edi, cchar *tableName, cchar *indexName)
{
    if (!validName(tableName) || !validName(indexName)) {
        return 0;
    }
    return query(edi, sfmt("DROP INDEX %s;", indexName), NULL) != 0;
}


static int sdbRemoveRec(Edi *edi, cchar *tableName, cchar *key)
{
    assert(edi);
    assert(tableName && *tableName);
    assert(key && *key);

    if (!validName(tableName)) {
        return 0;
    }
    return query(edi, sfmt("DELETE FROM %s WHERE id = ?;", tableName), key, NULL) != 0;
}


static int sdbRemoveTable(Edi *edi, cchar *tableName)
{
    if (!validName(tableName)) {
        return 0;
    }
    return query(edi, sfmt("DROP TABLE IF EXISTS %s;", tableName), NULL) != 0;
}


static int sdbRenameTable(Edi *edi, cchar *tableName, cchar *newTableName)
{
    if (!validName(tableName) || !validName(newTableName)) {
        return 0;
    }
    removeSchema(edi, tableName);
    removeSchema(edi, newTableName);
    return query(edi, sfmt("ALTER TABLE %s RENAME TO %s;", tableName, newTableName), NULL) != 0;
}


static int sdbRenameColumn(Edi *edi, cchar *tableName, cchar *columnName, cchar *newColumnName)
{
    mprLog("error esp sdb", 0, "SQLite does not support renaming columns");
    return MPR_ERR_BAD_STATE;
}


static int sdbSave(Edi *edi)
{
    return 0;
}


/*
    Map values before storing in the database
    While not required, it is prefereable to normalize the storage of some values.
    For example: dates are stored as numbers
 */
static cchar *mapSdbValue(cchar *value, int type)
{
    MprTime     time;

    if (value == 0) {
        return value;
    }
    switch (type) {
    case EDI_TYPE_DATE:
        if (!snumber(value)) {
            mprParseTime(&time, value, MPR_UTC_TIMEZONE, 0);
            value = itos(time);
        }
        break;

    case EDI_TYPE_BINARY:
    case EDI_TYPE_BOOL:
    case EDI_TYPE_FLOAT:
    case EDI_TYPE_INT:
    case EDI_TYPE_STRING:
    case EDI_TYPE_TEXT:
    default:
        break;
    }
    return value;
}


static int sdbUpdateField(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName, cchar *value)
{
    int     type;

    if (!validName(tableName) || !validName(fieldName)) {
        return 0;
    }
    sdbGetColumnSchema(edi, tableName, fieldName, &type, 0, 0);
    value = mapSdbValue(value, type);
    return query(edi, sfmt("UPDATE %s SET %s TO ? WHERE 'id' = ?;", tableName, fieldName), value, key, NULL) != 0;
}


/*
    Use parameterized queries to reduce the risk of SQL injection
 */
static int sdbUpdateRec(Edi *edi, EdiRec *rec)
{
    MprBuf      *buf;
    EdiField    *fp;
    cchar       **argv;
    int         argc, f;

    if (!ediValidateRec(rec)) {
        return MPR_ERR_CANT_WRITE;
    }
    if ((argv = mprAlloc(((rec->nfields * 2) + 2) * sizeof(cchar*))) == 0) {
        return MPR_ERR_MEMORY;
    }
    argc = 0;

    buf = mprCreateBuf(0, 0);
    if (rec->id) {
        mprPutToBuf(buf, "UPDATE %s SET ", rec->tableName);
        for (f = 0; f < rec->nfields; f++) {
            fp = &rec->fields[f];
            mprPutToBuf(buf, "%s = ?, ", fp->name);
            argv[argc++] = mapSdbValue(fp->value, fp->type);
        }
        mprAdjustBufEnd(buf, -2);
        mprPutStringToBuf(buf, " WHERE id = ?;");
        argv[argc++] = rec->id;

    } else {
        mprPutToBuf(buf, "INSERT INTO %s (", rec->tableName);
        for (f = 1; f < rec->nfields; f++) {
            fp = &rec->fields[f];
            mprPutToBuf(buf, "%s,", fp->name);
        }
        mprAdjustBufEnd(buf, -1);
        mprPutStringToBuf(buf, ") VALUES (");
        for (f = 1; f < rec->nfields; f++) {
            mprPutStringToBuf(buf, "?,");
            fp = &rec->fields[f];
            argv[argc++] = mapSdbValue(fp->value, fp->type);
        }
        mprAdjustBufEnd(buf, -1);
        mprPutCharToBuf(buf, ')');
    }
    argv[argc] = NULL;

    if (queryArgv(edi, mprGetBufStart(buf), argc, argv) == 0) {
        return MPR_ERR_CANT_WRITE;
    }
    return 0;
}


PUBLIC cchar *sdbGetLastError(Edi *edi)
{
    return ((Sdb*) edi)->edi.errMsg;
}


/*********************************** Support *******************************/

static EdiGrid *query(Edi *edi, cchar *cmd, ...)
{
    EdiGrid     *grid;
    va_list     args;

    va_start(args, cmd);
    grid = queryv(edi, cmd, 0, NULL, args);
    va_end(args);
    return grid;
}


/*
    Vars are ignored. Just to satisify old compilers
 */
static EdiGrid *queryArgv(Edi *edi, cchar *cmd, int argc, cchar **argv, ...)
{
    va_list     vargs;
    EdiGrid     *grid;

    va_start(vargs, argv);
    grid = queryv(edi, cmd, argc, argv, vargs);
    va_end(vargs);
    return grid;
}


/*
    This function supports parameterized queries. Provide parameters for query via either argc+argv or vargs.
    Strongly recommended to use parameterized queries to lessen SQL injection risk.
 */
static EdiGrid *queryv(Edi *edi, cchar *cmd, int argc, cchar **argv, va_list vargs)
{
    Sdb             *sdb;
    sqlite3         *db;
    sqlite3_stmt    *stmt;
    EdiGrid         *grid;
    EdiRec          *rec, *schema;
    MprList         *result;
    char            *tableName;
    cchar           *tail, *colName, *value, *defaultTableName, *arg;
    ssize           len;
    int             r, nrows, i, ncol, rc, retries, index, type;

    assert(edi);
    assert(cmd && *cmd);

    sdb = (Sdb*) edi;
    retries = 0;
    sdb->edi.errMsg = 0;

    if ((db = sdb->db) == 0) {
        sdbError(edi, "Database '%s' is closed", sdb->edi.path);
        return 0;
    }
    if ((result = mprCreateList(0, MPR_LIST_STABLE)) == 0) {
        return 0;
    }
    defaultTableName = 0;
    rc = SQLITE_OK;
    nrows = 0;

    while (cmd && *cmd && (rc == SQLITE_OK || (rc == SQLITE_SCHEMA && ++retries < 2))) {
        stmt = 0;
        mprLog("info esp sdb", 4, "SQL: %s", cmd);
        rc = sqlite3_prepare_v2(db, cmd, -1, &stmt, &tail);
        if (rc != SQLITE_OK) {
            sdbDebug(edi, 2, "SDB: cannot prepare command: %s, error: %s", cmd, sqlite3_errmsg(db));
            continue;
        }
        if (stmt == 0) {
            /* Comment or white space */
            cmd = tail;
            continue;
        }
        if (argc == 0) {
            for (index = 0; (arg = va_arg(vargs, cchar*)) != 0; index++) {
                if (sqlite3_bind_text(stmt, index + 1, arg, -1, 0) != SQLITE_OK) {
                    sdbError(edi, "SDB: cannot bind to arg: %d, %s, error: %s", index + 1, arg, sqlite3_errmsg(db));
                    return 0;
                }
            }
        } else if (argv) {
            for (index = 0; index < argc; index++) {
                if (sqlite3_bind_text(stmt, index + 1, argv[index], -1, 0) != SQLITE_OK) {
                    sdbError(edi, "SDB: cannot bind to arg: %d, %s, error: %s", index + 1, argv[index], sqlite3_errmsg(db));
                    return 0;
                }
            } 
        }
        ncol = sqlite3_column_count(stmt);
        for (nrows = 0; ; nrows++) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                tableName = (char*) sqlite3_column_table_name(stmt, 0);
                if ((rec = createBareRec(edi, tableName, ncol)) == 0) {
                    sqlite3_finalize(stmt);
                    return 0;
                }
                if (defaultTableName == 0) {
                    defaultTableName = rec->tableName;
                }
                mprAddItem(result, rec);
                for (i = 0; i < ncol; i++) {
                    colName = sqlite3_column_name(stmt, i);
                    value = (cchar*) sqlite3_column_text(stmt, i);
                    if (tableName && strcmp(tableName, defaultTableName) != 0) {
                        len = strlen(tableName) + 1;
                        tableName = sjoin("_", tableName, colName, NULL);
                        tableName[len] = toupper((uchar) tableName[len]);
                    }
                    if (tableName && ((schema = getSchema(edi, tableName)) != 0)) {
                        rec->fields[i] = makeRecField(value, colName, schema->fields[i].type);
                    } else {
                        type = sqlite3_column_type(stmt, i);
                        rec->fields[i] = makeRecField(value, colName, mapSqliteTypeToEdiType(type));
                    }
                    if (smatch(colName, "id")) {
                        rec->fields[i].flags |= EDI_KEY;
                        rec->id = rec->fields[i].value;
                    }
                }
            } else {
                rc = sqlite3_finalize(stmt);
                stmt = 0;
                if (rc != SQLITE_SCHEMA) {
                    retries = 0;
                    for (cmd = tail; isspace((uchar) *cmd); cmd++) {}
                }
                break;
            }
        }
    }
    if (stmt) {
        rc = sqlite3_finalize(stmt);
    }
    if (rc != SQLITE_OK) {
        if (rc == sqlite3_errcode(db)) {
            sdbDebug(edi, 2, "SDB: cannot run query: %s, error: %s", cmd, sqlite3_errmsg(db));
        } else {
            sdbDebug(edi, 2, "SDB: unspecified SQL error for: %s", cmd);
        }
        return 0;
    }
    if ((grid = ediCreateBareGrid(edi, defaultTableName, nrows)) == 0) {
        return 0;
    }
    for (r = 0; r < nrows; r++) {
        grid->records[r] = mprGetItem(result, r);
    }
    return grid;
}


static EdiRec *createBareRec(Edi *edi, cchar *tableName, int nfields)
{
    EdiRec  *rec;

    if ((rec = mprAllocBlock(sizeof(EdiRec) + sizeof(EdiField) * nfields, MPR_ALLOC_MANAGER | MPR_ALLOC_ZERO)) == 0) {
        return 0;
    }
    mprSetManager(rec, (MprManager) ediManageEdiRec);
    rec->edi = edi;
    rec->tableName = sclone(tableName);
    rec->nfields = nfields;
    return rec;
}


static EdiField makeRecField(cchar *value, cchar *name, int type)
{
    EdiField    f;

    f.valid = 1;
    f.value = sclone(value);
    f.name = sclone(name);
    f.type = type;
    f.flags = 0;
    return f;
}


static void *allocBlock(int size)
{
    void    *ptr;

    if ((ptr = mprAlloc(size)) != 0) {
        mprHold(ptr);
    }
    return ptr;
}


static void freeBlock(void *ptr)
{
    mprRelease(ptr);
}


static void *reallocBlock(void *ptr, int size)
{
    mprRelease(ptr);
    if ((ptr =  mprRealloc(ptr, size)) != 0) {
        mprHold(ptr);
    }
    return ptr;
}


static int blockSize(void *ptr)
{
    return (int) mprGetBlockSize(ptr);
}


static int roundBlockSize(int size)
{
    return MPR_ALLOC_ALIGN(size);
}


static int initAllocator(void *data)
{
    return 0;
}


static void termAllocator(void *data)
{
}


struct sqlite3_mem_methods mem = {
    allocBlock, freeBlock, reallocBlock, blockSize, roundBlockSize, initAllocator, termAllocator, NULL 
};


static cchar *mapToSqlType(int type)
{
    assert(0 < type && type < EDI_TYPE_MAX);
    return dataTypeToSqlType[type];
}


static int mapToEdiType(cchar *type)
{
    int     i;

    for (i = 0; i < EDI_TYPE_MAX; i++) {
        if (smatch(dataTypeToSqlType[i], type)) {
            return i;
        }
    }
    mprLog("error esp sdb", 0, "Cannot find type %s", type);
    return 0;
}


static int mapSqliteTypeToEdiType(int type) 
{
    if (type == SQLITE_INTEGER) {
        return EDI_TYPE_INT;
    } else if (type == SQLITE_FLOAT) {
        return EDI_TYPE_FLOAT;
    } else if (type == SQLITE_TEXT) {
        return EDI_TYPE_TEXT;
    } else if (type == SQLITE_BLOB) {
        return EDI_TYPE_BINARY;
    } else if (type == SQLITE_NULL) {
        return EDI_TYPE_TEXT;
    }
    mprLog("error esp sdb", 0, "Cannot find query type %d", type);
    return 0;
}


static bool validName(cchar *str)
{
    cchar   *cp;

    if (!str) {
        return 0;
    }
    if (!isalpha(*str) && *str != '_') {
        return 0;
    }
    for (cp = &str[1]; *cp && (isalnum((uchar) *cp) || *cp == '_' || *cp == '$'); cp++) {}
    return (*cp == '\0');
}


static void sdbError(Edi *edi, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    edi->errMsg = sfmtv(fmt, args);
    va_end(args);
    mprLog("error esp sdb", 0, "%s", edi->errMsg);
}


static void sdbDebug(Edi *edi, int level, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    edi->errMsg = sfmtv(fmt, args);
    va_end(args);
    mprDebug("debug esp sdb", level, "%s", edi->errMsg);
}


/*********************************** Factory *******************************/

static void initSqlite()
{
    mprGlobalLock();
    if (!sqliteInitialized) {
        sqlite3_config(SQLITE_CONFIG_MALLOC, &mem);
        sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
        if (sqlite3_initialize() != SQLITE_OK) {
            mprLog("error esp sdb", 0, "Cannot initialize SQLite");
            return;
        }
        sqliteInitialized = 1;
    }
    mprGlobalUnlock();
}

#else
/* To prevent ar/ranlib warnings */
PUBLIC void sdbDummy() {}
#endif /* ME_COM_SQLITE */

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

#endif /* ME_COM_ESP */
