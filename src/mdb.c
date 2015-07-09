/*
    mdb.c -- ESP In-Memory Embedded Database (MDB)

    WARNING: This is prototype code

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "http.h"
#include    "edi.h"
#include    "mdb.h"
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
            if ((r = lookupRow(table, value)) != 0) {
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
