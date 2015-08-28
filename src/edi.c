/*
    edi.c -- Embedded Database Interface (EDI)

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "edi.h"
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
