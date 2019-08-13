/*
 * Embedthis ESP Library Source
 */
#include "me.h"

#if ME_COM_ESP

#include "osdep.h"

#ifndef ESP_VERSION
    #define ESP_VERSION "7.2.0"
#endif

/*
    edi.h -- Embedded Database Interface (EDI).

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

#ifndef _h_EDI
#define _h_EDI 1

/********************************* Includes ***********************************/

#include    "http.h"

#ifdef __cplusplus
extern "C" {
#endif

/****************************** Forward Declarations **************************/

#if !DOXYGEN
#endif

/********************************** Defines ***********************************/

struct Edi;
struct EdiGrid;
struct EdiProvider;
struct EdiRec;
struct EdiValidation;

/**
    Edi service control structure
    @defgroup EdiService EdiService
 */
typedef struct EdiService {
    MprHash    *providers;
    MprHash    *validations;
} EdiService;

/**
    Create the EDI service
    @return EdiService object
    @ingroup EdiService
    @stability Evolving
    @internal
 */
PUBLIC EdiService *ediCreateService();

/**
    Add a database provider. 
    @description This should only be called by database providers. 
    @ingroup EdiService
    @stability Evolving
 */
PUBLIC void ediAddProvider(struct EdiProvider *provider);

/**
    Field validation callback procedure
    @param vp Validation structure reference
    @param rec Record to validate
    @param fieldName Field name to validate
    @param value Field value to validate 
    @ingroup EdiService
    @stability Evolving
 */
typedef cchar *(*EdiValidationProc)(struct EdiValidation *vp, struct EdiRec *rec, cchar *fieldName, cchar *value);

/**
    Validation structure
    @ingroup EdiService
    @stability Evolving
 */
typedef struct EdiValidation {
    cchar               *name;          /**< Validation name */
    EdiValidationProc   vfn;            /**< Validation callback procedure */
    cvoid               *data;          /**< Custom data (managed) */
    cvoid               *mdata;         /**< Custom data (unmanaged) */
} EdiValidation;

/**
    Define a field validation procedure
    @param name Validation name
    @param vfn Validation callback to invoke when validating field data.
    @ingroup EdiService
    @stability Evolving
 */
PUBLIC void ediDefineValidation(cchar *name, EdiValidationProc vfn);

/**
    Add a field error message
    @param rec Record to update
    @param field Field name for the error message
    @param fmt Message format string
    @ingroup EdiService
    @stability Prototype
 */
PUBLIC void ediAddFieldError(struct EdiRec *rec, cchar *field, cchar *fmt, ...);

/*
   Field data type hints
 */
#define EDI_TYPE_BINARY     1           /**< Arbitrary binary data */
#define EDI_TYPE_BOOL       2           /**< Boolean true|false value */
#define EDI_TYPE_DATE       3           /**< Date type */
#define EDI_TYPE_FLOAT      4           /**< Floating point number */
#define EDI_TYPE_INT        5           /**< Integer number */
#define EDI_TYPE_STRING     6           /**< String */
#define EDI_TYPE_TEXT       7           /**< Multi-line text */
#define EDI_TYPE_MAX        8           /**< Max type + 1 */

/*
    Field flags
 */
#define EDI_AUTO_INC        0x1         /**< Field flag -- Automatic increments on new row */
#define EDI_KEY             0x2         /**< Field flag -- Column is the ID key */
#define EDI_INDEX           0x4         /**< Field flag -- Column is indexed */
#define EDI_FOREIGN         0x8         /**< Field flag -- Column is a foreign key */
#define EDI_NOT_NULL        0x10        /**< Field flag -- Column must not be null (not implemented) */
 
/**
    EDI Record field structure
    @description The EdiField stores record field data and minimal schema information such as the data type and
        source column name.
    @defgroup EdiField EdiField
  */
typedef struct EdiField {
    cchar           *value;             /**< Field data value */
    cchar           *name;              /**< Field name. Sourced from the database column name */
    int             type:  8;           /**< Field data type. Set to one of EDI_TYPE_BINARY, EDI_TYPE_BOOL, EDI_TYPE_DATE
                                             EDI_TYPE_FLOAT, EDI_TYPE_INT, EDI_TYPE_STRING, EDI_TYPE_TEXT  */
    int             valid: 8;           /**< Field validity. Set to true if valid */
    int             flags: 8;           /**< Field flags. Flag mask set to EDI_AUTO_INC, EDI_KEY and/or EDI_INDEX */
} EdiField;

/**
    Database record structure
    @description Records may capture database row data, or may be free-standing without a backing database.
    @defgroup EdiRec EdiRec
 */
typedef struct EdiRec {
    struct Edi      *edi;               /**< Database handle */
    MprHash         *errors;            /**< Hash of record errors */
    cchar           *tableName;         /**< Base table name for record */
    cchar           *id;                /**< Record key ID */
    int             nfields;            /**< Number of fields in record */
    int             index;              /**< Grid index for iteration */
    EdiField        fields[ARRAY_FLEX]; /**< Field records */
} EdiRec;

#define EDI_GRID_READ_ONLY  0x1         /**< Grid contains pure database records, must not be modified */

/**
    Grid structure
    @description A grid is a tabular (grid) of rows and records.
        Grids may capture database table data, or may be free-standing without a backing database.
    @defgroup EdiGrid EdiGrid
 */
typedef struct EdiGrid {
    struct Edi      *edi;               /**< Database handle */
    cchar           *tableName;         /**< Base table name for grid */
    int             flags;              /**< Grid flags */
    int             nrecords;           /**< Number of records in grid */
    EdiRec          *records[ARRAY_FLEX];/**< Grid records */
} EdiGrid;

/*
    Database flags
 */
#define EDI_CREATE          0x1         /**< Create database if not present */
#define EDI_AUTO_SAVE       0x2         /**< Auto-save database if modified in memory */
#define EDI_NO_SAVE         0x4         /**< Prevent saving to disk */
#define EDI_LITERAL         0x8         /**< Literal schema in ediOpen source parameter */
#define EDI_SUPPRESS_SAVE   0x10        /**< Temporarily suppress auto-save */
#define EDI_PRIVATE         0x20        /**< Create private clone of the database */

typedef int (*EdiMigration)(struct Edi *db);

/**
    Define migration callbacks
    @param edi Database handle
    @param forw Forward migration callback. Of the form:
        int forw(Edi *edit);
        A successful return should be zero.
    @param back Backward migration callback. Of the form:
        int back(Edi *edit);
        A successful return should be zero.
    @ingroup EdiService
    @stability Evolving
 */
PUBLIC void ediDefineMigration(struct Edi *edi, EdiMigration forw, EdiMigration back);

/**
    Database structure
    @description The Embedded Database Interface (EDI) defines an abstract interface atop various relational 
    database providers. Providers are supplied for SQLite and for the ESP Memory Database (MDB).
    @defgroup Edi Edi
  */
typedef struct Edi {
    struct EdiProvider *provider;       /**< Database provider */
    MprHash         *schemaCache;       /**< Cache of table schema in JSON */
    MprHash         *validations;       /**< Validations */
    MprMutex        *mutex;             /**< Multithread lock */
    cchar           *path;              /**< Database path */
    int             flags;              /**< Database flags */
    EdiMigration    forw;               /**< Forward migration callback */
    EdiMigration    back;               /**< Backward migration callback */
    char            *errMsg;            /**< Last error message */
} Edi;

/**
    Database provider interface
    @internal
 */
typedef struct EdiProvider {
    cchar     *name;
    int       (*addColumn)(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags);
    int       (*addIndex)(Edi *edi, cchar *tableName, cchar *columnName, cchar *indexName);
    int       (*addTable)(Edi *edi, cchar *tableName);
    int       (*changeColumn)(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags);
    void      (*close)(Edi *edi);
    EdiRec    *(*createRec)(Edi *edi, cchar *tableName);
    int       (*deleteDatabase)(cchar *path);
    MprList   *(*getColumns)(Edi *edi, cchar *tableName);
    int       (*getColumnSchema)(Edi *edi, cchar *tableName, cchar *columnName, int *type, int *flags, int *cid);
    MprList   *(*getTables)(Edi *edi);
    int       (*getTableDimensions)(Edi *edi, cchar *tableName, int *numRows, int *numCols);
    int       (*load)(Edi *edi, cchar *path);
    int       (*lookupField)(Edi *edi, cchar *tableName, cchar *fieldName);
    Edi       *(*open)(cchar *path, int flags);
    EdiGrid   *(*query)(Edi *edi, cchar *cmd, int argc, cchar **argv, va_list vargs);
    EdiField  (*readField)(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName);
    EdiRec    *(*readRec)(Edi *edi, cchar *tableName, cchar *key);
    EdiGrid   *(*readWhere)(Edi *edi, cchar *tableName, cchar *fieldName, cchar *operation, cchar *value);
    int       (*removeColumn)(Edi *edi, cchar *tableName, cchar *columnName);
    int       (*removeIndex)(Edi *edi, cchar *tableName, cchar *indexName);
    int       (*removeRec)(Edi *edi, cchar *tableName, cchar *key);
    int       (*removeTable)(Edi *edi, cchar *tableName);
    int       (*renameTable)(Edi *edi, cchar *tableName, cchar *newTableName);
    int       (*renameColumn)(Edi *edi, cchar *tableName, cchar *columnName, cchar *newColumnName);
    int       (*save)(Edi *edi);
    int       (*updateField)(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName, cchar *value);
    int       (*updateRec)(Edi *edi, EdiRec *rec);
} EdiProvider;

/*************************** EDI Interface Wrappers **************************/
/**
    Add a column to a table
    @param edi Database handle
    @param tableName Database table name
    @param columnName Database column name
    @param type Column data type. Set to one of EDI_TYPE_BINARY, EDI_TYPE_BOOL, EDI_TYPE_DATE
        EDI_TYPE_FLOAT, EDI_TYPE_INT, EDI_TYPE_STRING, EDI_TYPE_TEXT 
    @param flags Control column attributes. Set to a set of: EDI_AUTO_INC for auto incrementing columns, 
        EDI_KEY if the column is the key column and/or EDI_INDEX to create an index on the column.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediAddColumn(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags);

/**
    Add an index to a table
    @param edi Database handle
    @param tableName Database table name
    @param columnName Database column name
    @param indexName Ignored. Set to null.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediAddIndex(Edi *edi, cchar *tableName, cchar *columnName, cchar *indexName);

/**
    Add a table to a database
    @param edi Database handle
    @param tableName Database table name. Table names should be singular. Certain routines like ediJoin rely on being
    able to map foreign key fields of the form NameId by converting the Name to a database table.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediAddTable(Edi *edi, cchar *tableName);

/**
    Add a validation
    @description Validations are run when calling ediUpdateRec. A validation is used to validate field data
        using builtin validators.
    @param edi Database handle
    @param name Validation name. Select from: 
        @arg banned -- to validate field data against a regular express for banned content.
        @arg boolean -- to validate field data as "true" or "false"
        @arg date -- to validate field data as a date or time.
        @arg format -- to validate field data against a regular expression supplied in the "data" argument
        @arg integer -- to validate field data as an integral value
        @arg number -- to validate field data as a number. It may be an integer or floating point number.
        @arg present -- to validate field data as not null.
        @arg unique -- to validate field data as being unique in the database table.
    @param tableName Database table name
    @param columnName Database column name
    @param data Argument data for the validator. For example: the "format" validator requires a regular expression.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediAddValidation(Edi *edi, cchar *name, cchar *tableName, cchar *columnName, cvoid *data);

/**
    Change a column schema definition
    @param edi Database handle
    @param tableName Database table name
    @param columnName Database column name
    @param type Column data type. Set to one of EDI_TYPE_BINARY, EDI_TYPE_BOOL, EDI_TYPE_DATE
        EDI_TYPE_FLOAT, EDI_TYPE_INT, EDI_TYPE_STRING, EDI_TYPE_TEXT 
    @param flags Control column attributes. Set to a set of: EDI_AUTO_INC for auto incrementing columns, 
        EDI_KEY if the column is the key column and/or EDI_INDEX to create an index on the column.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediChangeColumn(Edi *edi, cchar *tableName, cchar *columnName, int type, int flags);

/**
    Close a database
    @param edi Database handle
    @ingroup Edi
    @stability Evolving
 */
PUBLIC void ediClose(Edi *edi);

/**
    Clone a grid
    @param grid to clone
    @return A complete copy of a grid
    @ingroup Edi
    @stability Prototype
 */
PUBLIC EdiGrid *ediCloneGrid(EdiGrid *grid);

/**
    Create a record
    @description This will create a record using the given database tableName to supply the record schema. Use
        #ediCreateBareRec to create a free-standing record without requiring a database.
        The record is allocated and room is reserved to store record values. No record field values are stored.
    @param edi Database handle
    @param tableName Database table name
    @return Record instance.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiRec *ediCreateRec(Edi *edi, cchar *tableName);

/**
    Delete the database at the given path.
    @param edi Database handle. This is required to identify the database provider. The database should be closed before
        deleting.
    @param path Database path name.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediDelete(Edi *edi, cchar *path);

/**
    Display the grid to the debug log
    @param grid EDI grid
    @ingroup Edi
    @stability Prototype
 */
PUBLIC void espDumpGrid(EdiGrid *grid);

/**
    Get a list of column names.
    @param edi Database handle
    @param tableName Database table name
    @return An MprList of column names in the given table.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC MprList *ediGetColumns(Edi *edi, cchar *tableName);

/**
    Get the column schema
    @param edi Database handle
    @param tableName Database table name
    @param columnName Database column name
    @param type Output parameter to receive the column data type. Will be set to one of:
        EDI_TYPE_BINARY, EDI_TYPE_BOOL, EDI_TYPE_DATE, EDI_TYPE_FLOAT, EDI_TYPE_INT, EDI_TYPE_STRING, EDI_TYPE_TEXT.
        Set to null if this data is not required.
    @param flags Output parameter to receive the column control flags. Will be set to one or more of:
            EDI_AUTO_INC, EDI_KEY and/or EDI_INDEX 
        Set to null if this data is not required.
    @param cid Output parameter to receive the ordinal column index in the database table.
        Set to null if this data is not required.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediGetColumnSchema(Edi *edi, cchar *tableName, cchar *columnName, int *type, int *flags, int *cid);

/**
    Get the schema for a record and format as JSON
    @param rec
    @ingroup EdiRec
    @stability Prototype
 */
PUBLIC cchar *ediGetRecSchemaAsJson(EdiRec *rec);

/**
    Get the next field in a record
    This is used as an iterator. For the first call, set fp to NULL.
    @param rec Record whose fields are iterated
    @param fp Field pointer
    @param offset Initial offset. Set to 1 to step over the ID field.
    @return The next field object. Returns NULL after the last field.
    @ingroup EdiRec
    @stability Prototype
 */
PUBLIC EdiField *ediGetNextField(EdiRec *rec, EdiField *fp, int offset);

/**
    Get the next record in a grid
    This is used as an iterator. For the first call, set rec to NULL.
    @param grid Grid whose records are iterated
    @param rec Record pointer
    @return The next record object. Returns NULL after the last record.
    @ingroup EdiGrid
    @stability Prototype
 */
PUBLIC EdiRec *ediGetNextRec(EdiGrid *grid, EdiRec *rec);

/**
    Get table dimensions information.
    @param edi Database handle
    @param tableName Database table name
    @param numRows Output parameter to receive the number of rows in the table
        Set to null if this data is not required.
    @param numCols Output parameter to receive the number of columns in the table
        Set to null if this data is not required.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediGetTableDimensions(Edi *edi, cchar *tableName, int *numRows, int *numCols);

/**
    Get a table schema and format as JSON
    @param edi Database handle
    @param tableName Name of table to examine
    @ingroup Edi
    @stability Prototype
 */
PUBLIC cchar *ediGetTableSchemaAsJson(Edi *edi, cchar *tableName);

/**
    Get a list of database tables.
    @param edi Database handle
    @return An MprList of table names in the database.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC MprList *ediGetTables(Edi *edi);

/**
    Convert an EDI database grid into a JSON string.
    @param grid EDI grid
    @param flags Reserved. Set to zero.
    @return JSON string 
    @ingroup Edi
    @stability Prototype
  */
PUBLIC cchar *ediGridAsJson(EdiGrid *grid, int flags);

/**
    Join grids
    @param edi Database handle
    @param ... Null terminated list of data grids. These are instances of EdiGrid.
    @return A joined grid.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiGrid *ediJoin(Edi *edi, ...);

/**
    Load the database file.
    @param edi Database handle
    @param path Database path name
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediLoad(Edi *edi, cchar *path);

/**
    Lookup a column field by name.
    @param edi Database handle
    @param tableName Database table name
    @param fieldName Database column field name
    @return The ordinal column index in the table if the column field is found. Otherwise returns a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediLookupField(Edi *edi, cchar *tableName, cchar *fieldName);

/**
    Lookup an EDI provider name
    @param providerName Name of the EDI provider
    @return The EDI provider object. Returns null if the provider cannot be found.
    @ingroup Edi
    @stability Evolving
    @internal
 */
PUBLIC EdiProvider *ediLookupProvider(cchar *providerName);

/**
    Open a database.
    @description This opens a database using the specified database provider.
    @param source Database path name. If using the "mdb" provider with the EDI_LITERAL flag, then the source argument can
        be set to a literal JSON database content string.
    @param provider Database provider. Set to "mdb" for the Memory Database or "sqlite" for the SQLite provider.
    @param flags Set to:
        @arg EDI_CREATE  -- Create database if not present.
        @arg EDI_AUTO_SAVE -- Auto-save database if modified in memory. This option is only supported by the "mdb" provider.
        @arg EDI_NO_SAVE  -- Prevent saving to disk. This option is only supported by the "mdb" provider.
        @arg EDI_LITERAL -- Literal schema in ediOpen source parameter. This option is only supported by the "mdb" provider.
    @return If successful, returns an EDI database instance object. Otherwise returns zero.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC Edi *ediOpen(cchar *source, cchar *provider, int flags);

/**
    Clone a database
    @param edi Database to clone
    @return A copy of the database
    @ingroup Edi
    @stability Internal
 */
PUBLIC Edi *ediClone(Edi *edi);

/**
    Run a query.
    @description This runs a provider dependant query. For the SDB SQLite provider, this runs an SQL statement.
    The "mdb" provider does not implement this API. To do queries using the "mdb" provider, use:
        #ediReadRec, #ediReadRecWhere, #ediReadWhere, #ediReadField and #ediReadTable.
    The query may contain positional parameters via argc/argv or via a va_list. These are recommended to mitigate
    SQL injection risk.
    @param edi Database handle
    @param cmd Query command to execute.
    @param argc Number of query parameters in argv
    @param argv Query parameter arguments
    @param vargs Query parameters supplied in a NULL terminated va_list.
    @return If succesful, returns tabular data in the form of an EgiGrid structure. Returns NULL on errors.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiGrid *ediQuery(Edi *edi, cchar *cmd, int argc, cchar **argv, va_list vargs);

/**
    Read a formatted field from the database
    @description This reads a field from the database and formats the result using an optional format string.
        If the field has a null or empty value, the supplied defaultValue will be returned.
    @param edi Database handle
    @param fmt Reserved. Set to NULL.
    @param tableName Database table name
    @param key Row key column value to read.
    @param fieldName Column name to read
    @param defaultValue Default value to return if the field is null or empty.
    @return Field value or default value if field is null or empty. Returns null if no matching record is found.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC cchar *ediReadFieldValue(Edi *edi, cchar *fmt, cchar *tableName, cchar *key, cchar *fieldName, cchar *defaultValue);

/**
    Read a field from the database.
    @description This reads a field from the database.
    @param edi Database handle
    @param tableName Database table name
    @param key Row key column value to read.
    @param fieldName Column name to read
    @return Field value or null if the no record is found. May return null or empty if the field is null or empty.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiField ediReadField(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName);

/**
    Read one record.
    @description This runs a simple query on the database and selects the first matching record. The query selects
        a row that has a "field" that matches the given "value".
    @param edi Database handle
    @param tableName Database table name
    @param fieldName Database field name to evaluate
    @param operation Comparision operation. Set to "==", "!=", "<", ">", "<=" or ">=".
    @param value Data value to compare with the field values.
    @return First matching record. Returns NULL if no matching records.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiRec *ediReadRecWhere(Edi *edi, cchar *tableName, cchar *fieldName, cchar *operation, cchar *value);

/**
    Read a record.
    @description Read a record from the given table as identified by the key value.
    @param edi Database handle
    @param tableName Database table name
    @param key Key value of the record to read 
    @return Record instance of EdiRec.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiRec *ediReadRec(Edi *edi, cchar *tableName, cchar *key);

/**
    Read matching records.
    @description This runs a simple query on the database and returns matching records in a grid. The query selects
        all rows that have a "field" that matches the given "value".
    @param edi Database handle
    @param tableName Database table name
    @param fieldName Database field name to evaluate
    @param operation Comparision operation. Set to "==", "!=", "<", ">", "<=" or ">=".
    @param value Data value to compare with the field values.
    @return A grid containing all matching records. Returns NULL if no matching records.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiGrid *ediReadWhere(Edi *edi, cchar *tableName, cchar *fieldName, cchar *operation, cchar *value);

/**
    Read a table.
    @description This reads all the records in a table and returns a grid containing the results.
    @param edi Database handle
    @param tableName Database table name
    @return A grid containing all records in the table. Returns NULL if no matching records.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiGrid *ediReadTable(Edi *edi, cchar *tableName);

/**
    Convert an EDI database record into a JSON string.
    @param rec EDI record
    @param flags Reserved. Set to zero.
    @return JSON string 
    @ingroup Edi
    @stability Prototype
  */
PUBLIC cchar *ediRecAsJson(EdiRec *rec, int flags);

/**
    Remove a column from a table.
    @param edi Database handle
    @param tableName Database table name
    @param columnName Database column name
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int edRemoveColumn(Edi *edi, cchar *tableName, cchar *columnName);

/**
    Remove a table index.
    @param edi Database handle
    @param tableName Database table name
    @param indexName Ignored. Set to null. This call will remove the table index.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediRemoveIndex(Edi *edi, cchar *tableName, cchar *indexName);

/**
    Delete a row in a database table
    @param edi Database handle
    @param tableName Database table name
    @param key Row key column value to delete.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediRemoveRec(Edi *edi, cchar *tableName, cchar *key);

/**
    Remove a table from the database.
    @param edi Database handle
    @param tableName Database table name
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediRemoveTable(Edi *edi, cchar *tableName);

/**
    Rename a table.
    @param edi Database handle
    @param tableName Database table name
    @param newTableName New database table name
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediRenameTable(Edi *edi, cchar *tableName, cchar *newTableName);

/**
    Rename a column. 
    @param edi Database handle
    @param tableName Database table name
    @param columnName Database column name
    @param newColumnName New column name
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediRenameColumn(Edi *edi, cchar *tableName, cchar *columnName, cchar *newColumnName);

/**
    Save in-memory database contents to disk.
    @description How this call behaves is provider dependant. If the provider is "mdb" and the database is not opened
        with AutoSave, then this call will save the in-memory contents. If the "mdb" database is opened with AutoSave,
        then this call will do nothing. For the "sdb" SQLite provider, this call does nothing.
    @param edi Database handle
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediSave(Edi *edi);

/**
    Set a record field without writing to the database.
    @description This routine updates the record object with the given value. The record will not be written
        to the database. To write to the database, use #ediUpdateRec.
    @param rec Record to update
    @param fieldName Record field name to update
    @param value Value to update
    @return The record instance if successful, otherwise NULL.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiRec *ediSetField(EdiRec *rec, cchar *fieldName, cchar *value);

/**
    Set record fields without writing to the database.
    @description This routine updates the record object with the given values. The "data' argument supplies 
        a hash of fieldNames and values. The data hash may come from the request params() or it can be manually
        created via #ediMakeHash to convert a JSON string into an options hash.
        For example: ediSetFields(rec, mprParseJson("{ name: '%s', address: '%s' }", name, address))
        The record will not be written
        to the database. To write to the database, use #ediUpdateRec.
    @param rec Record to update
    @param data Json object of field to use for the update
    @return The record instance if successful, otherwise NULL.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiRec *ediSetFields(EdiRec *rec, MprJson *data);

/**
    Control whether the database accepts updates.
    @param edi Database handle
    @param on Set to true to make the database readonly, i.e. to disable all updates.
    @ingroup Edi
    @stability Prototype
 */
PUBLIC void ediSetReadonly(Edi *edi, bool on);

/**
    Create a private database for each client.
    @param edi Database handle
    @param on Set to true to clone the database for each connected client.
    @ingroup Edi
    @stability Internal
 */
PUBLIC void ediSetPrivate(Edi *edi, bool on);

/**
    Write a value to a database table field
    @description Update the value of a table field in the selected table row. Note: field validations are not run.
    @param edi Database handle
    @param tableName Database table name
    @param key Key value for the table row to update.
    @param fieldName Column name to update
    @param value Value to write to the database field
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediUpdateField(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName, cchar *value);

/**
    Write a record to the database.
    @description If the record is a new record and the "id" column is EDI_AUTO_INC, then the "id" will be assigned
        prior to saving the record.
    @param edi Database handle
    @param rec Record to write to the database.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediUpdateRec(Edi *edi, EdiRec *rec);

/**
    Validate a record.
    @description Run defined field validations and return true if the record validates. Field validations are defined
        via #ediAddValidation calls. If any validations fail, error messages will be added to the record and can be 
        retrieved via #ediGetRecErrors.
    @param rec Record to validate
    @return True if all field valiations pass.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC bool ediValidateRec(EdiRec *rec);

/**************************** Convenience Routines ****************************/
/**
    Create a bare grid.
    @description This creates an empty grid based on the given table's schema.
    @param edi Database handle
    @param tableName Database table name
    @param nrows Number of rows to reserve in the grid
    @return EdiGrid instance
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiGrid *ediCreateBareGrid(Edi *edi, cchar *tableName, int nrows);

/**
    Create a bare record.
    @description This creates an empty record based on the given table's schema.
    @param edi Database handle
    @param tableName Database table name
    @param nfields Number of fields to reserve in the record
    @return EdiGrid instance
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiRec *ediCreateBareRec(Edi *edi, cchar *tableName, int nfields);

/**
    Filter the fields of a grid
    @param grid Grid to modify and filter
    @param fields Space separated list of record field names 
    @param include Set to true to interpret the names as fields to include. If false, interpret the names
        as fields to reject.
    @return The filtered grid. Same reference as the input grid.
    @ingroup EdiGrid
    @stability Internal
 */
PUBLIC EdiGrid *ediFilterGridFields(EdiGrid *grid, cchar *fields, int include);

/**
    Filter the fields of a record
    @param rec Record to modify and filter
    @param fields Space separated list of record field names 
    @param include Set to true to interpret the names as fields to include. If false, interpret the names
        as fields to reject.
    @return The filtered record. Same reference as the input record.
    @ingroup EdiRec
    @stability Internal
 */
PUBLIC EdiRec *ediFilterRecFields(EdiRec *rec, cchar *fields, int include);

/**
    Format a field value.
    @param fmt Printf style format string
    @param fp Field whoes value will be formatted
    @return Formatted value string
    @ingroup Edi
    @stability Evolving
 */
PUBLIC cchar *ediFormatField(cchar *fmt, EdiField *fp);

/**
    Get a record field
    @param rec Database record
    @param fieldName Field in the record to extract
    @return An EdiField structure containing the record field value and details.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiField *ediGetField(EdiRec *rec, cchar *fieldName);

/**
    Get a field value 
    @param rec Database record
    @param fieldName Field in the record to extract
    @return A field value as a string. Returns ZZ
    @ingroup Edi
    @stability Evolving
 */
PUBLIC cchar *ediGetFieldValue(EdiRec *rec, cchar *fieldName);

/**
    Get the data type of a record field.
    @param rec Record to examine
    @param fieldName Field to examine
    @return The field type. Returns one of: EDI_TYPE_BINARY, EDI_TYPE_BOOL, EDI_TYPE_DATE, EDI_TYPE_FLOAT, 
        EDI_TYPE_INT, EDI_TYPE_STRING, EDI_TYPE_TEXT. 
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediGetFieldType(EdiRec *rec, cchar *fieldName);

/**
    Get a list of grid column names.
    @param grid Database grid
    @return An MprList of column names in the given grid.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC MprList *ediGetGridColumns(EdiGrid *grid);

/**
    Get the schema for a grid and format as JSON
    @param grid Grid to examine
    @ingroup EdiGrid
    @stability Prototype
 */
PUBLIC cchar *ediGetGridSchemaAsJson(EdiGrid *grid);

/**
    Get record validation errors.
    @param rec Database record
    @return A hash of validation errors. If validation passed, then this call returns NULL.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC MprHash *ediGetRecErrors(EdiRec *rec);

/**
    Convert an EDI type to a string.
    @param type Column data type. Set to one of EDI_TYPE_BINARY, EDI_TYPE_BOOL, EDI_TYPE_DATE
        EDI_TYPE_FLOAT, EDI_TYPE_INT, EDI_TYPE_STRING, EDI_TYPE_TEXT 
    @return Type string. This will be set to one of: "binary", "bool", "date", "float", "int", "string" or "text".
    @ingroup Edi
    @stability Evolving
 */
PUBLIC char *ediGetTypeString(int type);

/**
    Make a hash container of property values.
    @description This routine formats the given arguments, parses the result as a JSON string and returns an 
        equivalent hash of property values. 
    @param fmt Printf style format string
    @param ... arguments
    @return MprHash instance
    @ingroup Edi
    @stability Evolving
 */
PUBLIC MprHash *ediMakeHash(cchar *fmt, ...);

/**
    Make a JSON container of property values.
    @description This routine formats the given arguments, parses the result into a JSON object.
    @param fmt Printf style format string
    @param ... arguments
    @return MprJson instance
    @ingroup Edi
    @stability Evolving
 */
PUBLIC MprJson *ediMakeJson(cchar *fmt, ...);

/**
    Make a grid.
    @description This call makes a free-standing data grid based on the JSON format content string.
    @param content JSON format content string. The content should be an array of objects where each object is a
        set of property names and values.
    @return An EdiGrid instance
    @example:
grid = ediMakeGrid("[ \\ \n
    { id: '1', country: 'Australia' }, \ \n
    { id: '2', country: 'China' }, \ \n
    ]");
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiGrid *ediMakeGrid(cchar *content);

/**
    Make a record.
    @description This call makes a free-standing data record based on the JSON format content string.
    @param content JSON format content string. The content should be a set of property names and values.
    @return An EdiRec instance
    @example: rec = ediMakeRec("{ id: 1, title: 'Message One', body: 'Line one' }");
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiRec *ediMakeRec(cchar *content);

/**
    Manage an EdiRec instance for garbage collection.
    @param rec Record instance
    @param flags GC management flag
    @ingroup Edi
    @stability Evolving
    @internal
 */
PUBLIC void ediManageEdiRec(EdiRec *rec, int flags);

/**
    Parse an EDI type string.
    @param type Type string set to one of: "binary", "bool", "date", "float", "int", "string" or "text".
    @return Type code. Set to one of EDI_TYPE_BINARY, EDI_TYPE_BOOL, EDI_TYPE_DATE, EDI_TYPE_FLOAT, EDI_TYPE_INT, 
        EDI_TYPE_STRING, EDI_TYPE_TEXT.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediParseTypeString(cchar *type);

/**
    Pivot a grid swapping rows for columns
    @param grid Source grid
    @param flags Control flags. Set to EDI_PIVOT_FIELD_NAMES to use field names as the first column of data.
    @result New pivoted grid
    @ingroup EdiGrid
    @stability Evolving
 */
PUBLIC EdiGrid *ediPivotGrid(EdiGrid *grid, int flags);

/**
    @internal
  */
PUBLIC EdiGrid *ediSortGrid(EdiGrid *grid, cchar *sortColumn, int sortOrder);

#if ME_COM_MDB
PUBLIC void mdbInit();
#endif

#if ME_COM_SQLITE
PUBLIC void sdbInit();
#endif

#ifdef __cplusplus
} /* extern C */
#endif

#endif /* _h_EDI */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */

/*
    mdb.h -- Memory Database (MDB).

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

#ifndef _h_MDB
#define _h_MDB 1

/********************************* Includes ***********************************/

#include    "http.h"


#if ME_COM_MDB

#ifdef __cplusplus
extern "C" {
#endif

/****************************** Forward Declarations **************************/

#if !DOXYGEN
#endif

/********************************** Tunables **********************************/

#define MDB_INCR    8                   /**< Default memory allocation increment for MDB */

/*
    Per column structure
 */
typedef struct MdbCol {
    char            *name;              /* Column name */
    int             type;               /* Column type */
    int             flags;              /* Column flags */
    int             cid;                /* Column index in MdbSchema.cols */
    int64           lastValue;          /* Last value if auto-inc */
} MdbCol;

/*
    Table schema
 */
typedef struct MdbSchema {
    int             ncols;              /* Number of columns in table */
    int             capacity;           /* Capacity of cols */
    MdbCol          cols[ARRAY_FLEX];   /* Array of columns */
} MdbSchema;

/*
    Per row structure
 */
typedef struct MdbRow {
    struct MdbTable *table;             /* Reference to MdbTable */
    int             rid;                /* Table index in MdbTable.row */
    int             nfields;            /* Number of fields in fields */
    cchar           *fields[ARRAY_FLEX];/* All data stored as strings */
} MdbRow;

/*
    Per table structure
 */
typedef struct MdbTable {
    char            *name;              /* Table name */
    MdbSchema       *schema;            /* Table columns schema */
    MprHash         *index;             /* Table index */
    MdbCol          *keyCol;            /* Reference to the key column (unmanaged) */
    MdbCol          *indexCol;          /* Reference to the index column (unmanaged) */
    MprList         *rows;              /* Table row */
} MdbTable;

/*
    Mdb flags
 */
#define MDB_LOADING     0x1

/*
    Per database structure
 */
typedef struct Mdb {
    Edi             edi;                /**< EDI database interface structure */
    MprList         *tables;            /**< List of tables */

    /*
        When loading from file only (do not mark)
     */
    MdbTable        *loadTable;         /* Current table */
    MdbCol          *loadCol;           /* Current column */
    MdbRow          *loadRow;           /* Current row */
    MprList         *loadStack;         /* State stack */
    MprHash         *validations;       /**< Validations */
    int             loadCid;            /* Current column index to load */
    int             loadState;          /* Current state */
    int             loadNcols;          /* Expected number of cols */
    int             lineNumber;         /* Current line number in path */
} Mdb;


#ifdef __cplusplus
} /* extern C */
#endif

#endif /* ME_COM_MDB */
#endif /* _h_MDB */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */

/*
    esp.h -- Embedded Server Pages (ESP) Module handler.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

#ifndef _h_ESP
#define _h_ESP 1

/********************************* Includes ***********************************/



#ifdef __cplusplus
extern "C" {
#endif

/********************************** Tunables **********************************/

#ifndef ME_ESP_EMAIL_TIMEOUT
    #define ME_ESP_EMAIL_TIMEOUT (60 * 1000)           /**< Timeout for sending email */
#endif
#ifndef ME_ESP_RELOAD_TIMEOUT
    #define ME_ESP_RELOAD_TIMEOUT (5 * 1000)           /**< Timeout for reloading esp modules */
#endif
#define ESP_TOK_INCR        1024                        /**< Growth increment for ESP tokens */
#define ESP_LISTEN          "4000"                      /**< Default listening endpoint for the esp program */
#define ESP_UNLOAD_TIMEOUT  (10)                        /**< Very short timeout for reloading */
#define ESP_LIFESPAN        (3600 * TPS)                /**< Default generated content cache lifespan */
#define ESP_COMPILE_JSON    "esp-compile.json"          /**< Compile rules filename */

#if ME_64
    #define ESP_VSKEY "HKLM\\SOFTWARE\\Wow6432Node\\Microsoft\\VisualStudio\\SxS\\VS7"
#else
    #define ESP_VSKEY "HKLM\\SOFTWARE\\Microsoft\\VisualStudio\\SxS\\VS7"
#endif

#ifndef ESP_VERSION
    #define ESP_VERSION ME_VERSION
#endif
#ifndef ESP_MAJOR_VERSION
    #define ESP_MAJOR_VERSION ME_MAJOR_VERSION
#ifndef ESP_MINOR_VERSION
    #define ESP_MINOR_VERSION ME_MINOR_VERSION
#endif
#endif

/********************************** Defines ***********************************/
/**
    Procedure callback
    @ingroup Esp
    @stability Stable
 */
typedef void (*EspProc)(HttpConn *conn);

#define ESP_CONTENT_MARKER  "${_ESP_CONTENT_MARKER_}"       /* Layout content marker */

#if ME_WIN_LIKE
    #define ESP_EXPORT __declspec(dllexport)
#else
    #define ESP_EXPORT
#endif
#define ESP_EXPORT_STRING MPR_STRINGIFY(ESP_EXPORT)

#define ESP_FEEDBACK_VAR        "__feedback__"

/*
    Default VxWorks environment
 */
#ifndef WIND_BASE
    #define WIND_BASE "WIND_BASE-Not-Configured"
#endif
#ifndef WIND_HOME
    #define WIND_HOME "WIND_HOME-Not-Configured"
#endif
#ifndef WIND_HOST_TYPE
    #define WIND_HOST_TYPE "WIND_HOST_TYPE-Not-Configured"
#endif
#ifndef WIND_PLATFORM
    #define WIND_PLATFORM "WIND_PLATFORM-Not-Configured"
#endif
#ifndef WIND_GNU_PATH
    #define WIND_GNU_PATH "WIND_GNU_PATH-Not-Configured"
#endif

/********************************** Parsing ***********************************/
/**
    ESP page parser structure
    @defgroup EspParse EspParse
    @see Esp
    @internal
 */
typedef struct EspState {
    char    *data;                          /**< Input data to parse */
    char    *next;                          /**< Next character in input */
    int     lineNumber;                     /**< Line number for error reporting */
    MprBuf  *token;                         /**< Current token */
    MprBuf  *global;                        /**< Accumulated compiled esp global code */
    MprBuf  *start;                         /**< Accumulated compiled esp start of function code */
    MprBuf  *end;                           /**< Accumulated compiled esp end of function code */
} EspState;

#define ESP_COMPILE_SYMBOLS     0           /**< Override to compile in debug mode. Defaults to same as Appweb */
#define ESP_COMPILE_OPTIMIZED   1           /**< Override to compile in release mode */

/**
    Top level ESP structure. This is a singleton.
 */
typedef struct Esp {
    MprHash         *databases;             /**< Cloned databases */
    MprEvent        *databasesTimer;        /**< Database prune timer */
    MprHash         *internalOptions;       /**< Table of internal HTML control options  */
    MprThreadLocal  *local;                 /**< Thread local data */
    MprMutex        *mutex;                 /**< Multithread lock */
    EdiService      *ediService;            /**< Database service */
    cchar           *hostedDocuments;       /**< Documents directory if hosted */
    int             compileMode;            /**< Force a debug compile */
    int             inUse;                  /**< Active ESP request counter */
    int             reloading;              /**< Reloading ESP and modules */
    MprHash         *vstudioEnv;            /**< Visual Studio environment */
} Esp;

/**
    Entry point for a loadable ESP module
    @param route HttpRoute object
    @param module Mpr module object
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup EspRoute
    @stability Stable
  */
typedef int (*EspModuleEntry)(struct HttpRoute *route, MprModule *module);

/**
    ESP initialization entry point
    @param module Module object if loaded as an MPR module.
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup Esp
    @stability Evolving
 */
PUBLIC int espOpen(MprModule *module);

/**
    Initialize a static library ESP module
    @description This invokes the ESP initializers for the required pre-compiled ESP shared library.
    @param entry ESP initialization function.
    @param appName Name of the ESP application
    @param routeName Name of the route in the appweb.conf file for this ESP application or page
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup Esp
    @stability Evolving
  */
PUBLIC int espStaticInitialize(EspModuleEntry entry, cchar *appName, cchar *routeName);

/**
    Add HTLM internal options to the Esp.options hash
    @internal
 */
PUBLIC void espInitHtmlOptions(Esp *esp);

/**
    Initialize the ESP configuration file parser
    @internal
 */
PUBLIC int espInitParser();

/********************************** EspRoutes *********************************/
/**
    EspRoute extended route configuration.
    Note that HttpRoutes may share an EspRoute.
    @defgroup EspRoute EspRoute
    @see Esp
 */
typedef struct EspRoute {
    cchar           *appName;               /**< App module name */
    struct EspRoute *top;                   /**< Top-level route for this application */
    HttpRoute       *route;                 /**< Back link to route */
    EspProc         commonController;       /**< Common code for all controllers */
    MprTime         loaded;                 /**< When configuration was last loaded */

    MprHash         *actions;               /**< Table of actions */
    MprHash         *env;                   /**< Environment variables for route */
    MprHash         *views;                 /**< Table of views */
    cchar           *currentSession;        /**< Current login session when enforcing a single login */
    cchar           *configFile;            /**< Path to config file */

    cchar           *compileCmd;            /**< Compile command template */
    cchar           *linkCmd;               /**< Link command template */
    cchar           *searchPath;            /**< Search path to use when locating compiler/linker */
    cchar           *winsdk;                /**< Windows SDK */

    uint            app: 1;                 /**< Is an esp mvc application */
    uint            combine: 1;             /**< Combine C source into a single file */
    uint            compileMode: 1;         /**< Compile the application debug or release mode */
    uint            compile: 1;             /**< Enable recompiling the application or esp page */
    uint            update: 1;              /**< Enable dynamically updating the application */
    uint            keep: 1;                /**< Keep intermediate source code after compiling */

    Edi             *edi;                   /**< Default database for this route */

#if DEPRECATED || 1
    cchar           *combineScript;         /**< Combine mode script filename */
    cchar           *combineSheet;          /**< Combine mode stylesheet filename */
#endif
} EspRoute;

#if DEPRECATED || 1
/**
    Add the specified pak to the pak.json packs list.
    @param route HttpRoute defining the ESP application
    @param name Desired pak name. For example: "angular-mvc"
    @param version Pack version string.
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup EspRoute
    @stability Prototype
 */
PUBLIC void espAddPak(HttpRoute *route, cchar *name, cchar *version);
#endif

/**
    Add a route for the home page.
    @description This will add a home page to route ESP applications. This will add the following route:
    <table>
        <tr><td>Name</td><td>Method</td><td>Pattern</td><td>Target</td></tr>
        <tr><td>home</td><td>GET,POST,PUT</td><td>^/$</td><td>index.esp</td></tr>
    </table>
    @param route Parent route from which to inherit configuration.
    @ingroup EspRoute
    @stability Evolving
 */
PUBLIC void espAddHomeRoute(HttpRoute *route);

/**
    Add a route set
    @description This will add a set of routes. It will add a home route and optional routes depending on the route set.
    <table>
        <tr><td>Name</td><td>Method</td><td>Pattern</td><td>Target</td></tr>
        <tr><td>home</td><td>GET,POST,PUT</td><td>^/$</td><td>index.esp</td></tr>
    </table>
    @param route Parent route from which to inherit configuration.
    @param set Route set to select. Use "angular-mvc", or "html-mvc".
    @ingroup EspRoute
    @stability Stable
 */
PUBLIC void espAddRouteSet(HttpRoute *route, cchar *set);

/**
    Initialize ESP
    @description This initializes a route for ESP. This may be called multiple times for different routes.
    @param route Parent route from which to inherit configuration.
    @param prefix Optional URI prefix for all application URIs.
    @param path Pathname to the esp.json file.
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup EspRoute
    @stability Prototype
 */
PUBLIC int espInit(HttpRoute *route, cchar *prefix, cchar *path);

/**
    Configure an ESP application
    @description Load the esp.json and pak.json configuration files.
    @param route Parent route from which to inherit configuration.
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup EspRoute
    @stability Prototype
 */
PUBLIC int espLoadConfig(HttpRoute *route);

/**
    Return the corresponding EspRoute for the given Route.
    @description Returns the defined EspRoute for the given Route. Creates a new EspRoute if required.
    @param route Parent route from which to inherit configuration.
    @returns The EspRoute object.
    @ingroup EspRoute
    @stability Prototype
    @param route
 */
PUBLIC EspRoute *espRoute(HttpRoute *route);

/**
    Add caching for response content.
    @description This call configures caching for request responses. Caching may be used for any HTTP method,
    though typically it is most useful for state-less GET requests. Output data may be uniquely cached for requests
    with different request parameters (query, post and route parameters).
    \n\n
    When server-side caching is requested and manual-mode is not enabled, the request response will be automatically
    cached. Subsequent client requests will revalidate the cached content with the server. If the server-side cached
    content has not expired, a HTTP Not-Modified (304) response will be sent and the client will use its client-side
    cached content.  This results in a very fast transaction with the client as no response data is sent.
    Server-side caching will cache both the response headers and content.
    \n\n
    If manual server-side caching is requested, the response will be automatically cached, but subsequent requests will
    require the handler to explicitly send cached content by calling #httpWriteCached.
    \n\n
    If client-side caching is requested, a "Cache-Control" Http header will be sent to the client with the caching
    "max-age" set to the lifesecs argument value. This causes the client to serve client-cached
    content and to not contact the server at all until the max-age expires.
    Alternatively, you can use #httpSetHeader to explicitly set a "Cache-Control header. For your reference, here are
    some keywords that can be used in the Cache-Control Http header.
    \n\n
        "max-age" Max time in seconds the resource is considered fresh.
        "s-maxage" Max time in seconds the resource is considered fresh from a shared cache.
        "public" marks authenticated responses as cacheable.
        "private" shared caches may not store the response.
        "no-cache" cache must re-submit request for validation before using cached copy.
        "no-store" response may not be stored in a cache.
        "must-revalidate" forces clients to revalidate the request with the server.
        "proxy-revalidate" similar to must-revalidate except only for proxy caches.
    \n\n
    Use client-side caching for static content that will rarely change or for content for which using "reload" in
    the browser is an adequate solution to force a refresh. Use manual server-side caching for situations where you need to
    explicitly control when and how cached data is returned to the client. For most other situations, use server-side
    caching.
    @param route HttpRoute object
    @param uri URI to cache.
        If the URI is set to "*" all URIs for that action are uniquely cached. If the request has POST data,
        the URI may include such post data in a sorted query format. E.g. {uri: /buy?item=scarf&quantity=1}.
    @param lifesecs Lifespan of cache items in seconds. If not set to positive integer, the lifesecs will
        default to the route lifespan.
    @param flags Cache control flags. Select ESP_CACHE_MANUAL to enable manual mode. In manual mode, cached content
        will not be automatically sent. Use #httpWriteCached in the request handler to write previously cached content.
        \n\n
        Select ESP_CACHE_CLIENT to enable client-side caching. In this mode a "Cache-Control" Http header will be
        sent to the client with the caching "max-age". WARNING: the client will not send any request for this URI
        until the max-age timeout has expired.
        \n\n
        Select HTTP_CACHE_RESET to first reset existing caching configuration for this route.
        \n\n
        Select HTTP_CACHE_COMBINED, HTTP_CACHE_ONLY or HTTP_CACHE_UNIQUE to define the server-side caching mode. Only
        one of these three mode flags should be specified.
        \n\n
        If the HTTP_CACHE_COMBINED flag is set, the request params (query, post data and route parameters) will be
        ignored and all request for a given URI path will cache to the same cache record.
        \n\n
        Select HTTP_CACHE_UNIQUE to uniquely cache requests with different request parameters. The URIs specified in
        uris should not contain any request parameters.
        \n\n
        Select HTTP_CACHE_ONLY to cache only the exact URI with parameters specified in uris. The parameters must be
        in sorted www-urlencoded format. For example: /example.esp?hobby=sailing&name=john.
    @return A count of the bytes actually written
    @ingroup EspRoute
    @stability Evolving
    @internal
 */
PUBLIC int espCache(HttpRoute *route, cchar *uri, int lifesecs, int flags);

/**
    Compile an ESP page, controller or view
    @description This compiles ESP resources into loadable, cached modules
    @param route HttpRoute object
    @param dispatcher Optional dispatcher to use when waiting for the compilation command.
    @param source ESP source file name
    @param module Output module file name
    @param cacheName MD5 cache name. Not a full path
    @param isView Set to "true" if the source is a view
    @param errMsg Reference to receive an error message if the routine fails.
    @return "True" if the compilation is successful. Errors are logged and sent back to the client if ShowErrors is true.
    @ingroup EspRoute
    @stability Evolving
    @internal
 */
PUBLIC bool espCompile(HttpRoute *route, MprDispatcher *dispatcher, cchar *source, cchar *module, cchar *cacheName,
    int isView, char **errMsg);

/**
    Convert an ESP web page into C code
    @description This parses an ESP web page into an equivalent C source view.
    @param route HttpRoute object
    @param page ESP web page script.
    @param path Pathname for the ESP web page. This is used to process include directives which are resolved relative
        to this path.
    @param cacheName MD5 cache name. Not a full path.
    @param layout Default layout page. Deprecated.
    @param state Reserved. Must set to NULL.
    @param err Output parameter to hold any relevant error message.
    @return Compiled script. Return NULL on errors.
    @ingroup EspRoute
    @stability Evolving
    @internal
 */
PUBLIC char *espBuildScript(HttpRoute *route, cchar *page, cchar *path, cchar *cacheName, cchar *layout,
    EspState *state, char **err);

/**
    Define an action
    @description Actions are C procedures that are invoked when specific URIs are routed to the controller/action pair.
    @param route HttpRoute object
    @param targetKey Target key used to select the action in a HttpRoute target. This is typically a URI prefix.
    @param actionProc EspProc callback procedure to invoke when the action is requested.
    @ingroup EspRoute
    @stability Stable
 */
PUBLIC void espDefineAction(HttpRoute *route, cchar *targetKey, void *actionProc);

/**
    Define an action for a URI pattern.
    @description This creates a new route and binds the action function to a URI pattern.
    @param route Parent route object from which to inherit settings when creating the new route.
    @param pattern URI pattern to use to find the releavant route.
    @param actionProc EspProc callback procedure to invoke when the action is requested.
    @ingroup EspRoute
    @stability Stable
 */
PUBLIC int espBindProc(HttpRoute *route, cchar *pattern, void *actionProc);

/**
    Create an EspRoute object
    @param route HttpRoute to associate with
    @return EspRoute object
    @internal
    @stability Stable
 */
PUBLIC EspRoute *espCreateRoute(HttpRoute *route);

/**
    Define a base function to invoke for all controller actions.
    @description A base function can be defined that will be called before calling any controller action. This
        emulates a super class constructor.
    @param route HttpRoute object
    @param baseProc Function to call just prior to invoking a controller action.
    @ingroup EspRoute
    @stability Stable
 */
PUBLIC void espDefineBase(HttpRoute *route, EspProc baseProc);

/**
    Define a view
    @description Views are ESP web pages that are executed to return presentation data back to the client.
    @param route Http route object
    @param path Path to the ESP view source code.
    @param viewProc EspViewPrococ callback procedure to invoke when the view is requested.
    @ingroup EspRoute
    @stability Stable
 */
PUBLIC void espDefineView(HttpRoute *route, cchar *path, void *viewProc);

/**
    Expand a compile or link command template
    @description This expands a command template and replaces "${tokens}" with their equivalent value. The supported
        tokens are:
        <ul>
            <li>ARCH - Build architecture (i386, x86_64)</li>
            <li>CC - Compiler pathname</li>
            <li>DEBUG - Compiler debug options (-g, -Zi, -Od)</li>
            <li>INC - Include directory (out/inc)</li>
            <li>LIB - Library directory (out/lib, out/bin)</li>
            <li>LIBS - Required libraries directory (esp, mpr)</li>
            <li>OBJ - Name of compiled source (out/lib/view-MD5.o)</li>
            <li>OUT - Output module (view_MD5.dylib)</li>
            <li>SHLIB - Shared library extension (.lib, .so)</li>
            <li>SHOBJ - Shared object extension (.dll, .so)</li>
            <li>SRC - Path to source code for view or controller (already templated)</li>
            <li>TMP - System temporary directory</li>
            <li>WINSDK - Path to the Windows SDK</li>
            <li>VS - Path to Visual Studio</li>
        </ul>
    @param route HttpRoute object
    @param command Http connection object
    @param source ESP web page source pathname
    @param module Output module pathname
    @return An expanded command line
    @ingroup EspRoute
    @stability Evolving
    @internal
 */
PUBLIC char *espExpandCommand(HttpRoute *route, cchar *command, cchar *source, cchar *module);

/**
    Get a configuration value from the ESP pak.json
    @param route HttpRoute defining the ESP application
    @param key Configuration property path. May contain dots.
    @param defaultValue Default value to use if the configuration is not defined. May be null
    @returns the Configuration string value
    @ingroup EspRoute
    @stability Stable
 */
PUBLIC cchar *espGetConfig(HttpRoute *route, cchar *key, cchar *defaultValue);

#if DEPRECATED || 1
/**
    Test if the ESP application includes the specified pak
    @description This tests the dependencies property specified pak.
    @param route HttpRoute defining the ESP application
    @param name Desired pak name. For example: "angular-mvc"
    @returns True if the specified pak is supported
    @ingroup EspRoute
    @stability Prototype
 */
PUBLIC bool espHasPak(HttpRoute *route, cchar *name);
#endif

/**
    Load the compiler rules from esp-compile.json
    @param route HttpRoute object
    @ingroup EspRoute
    @stability Prototype
    @internal
 */
PUBLIC int espLoadCompilerRules(HttpRoute *route);

#if DEPRECATED || 1
/**
    Save the in-memory ESP pak.json configuration to the default location for the ESP application
    defined by the specified route.
    @param route HttpRoute defining the ESP application
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup EspRoute
    @stability Evolving
 */
PUBLIC int espSaveConfig(HttpRoute *route);
#endif

/**
    Set a configuration value to the ESP pak.json.
    @description This updates the in-memory copy of the pak.json only.
    @param route HttpRoute defining the ESP application
    @param key Configuration property path. May contain dots.
    @param value Value to set the property to.
    @returns Zero if successful, otherwise a negative MPR error code.
    @ingroup EspRoute
    @stability Evolving
 */
PUBLIC int espSetConfig(HttpRoute *route, cchar *key, cchar *value);

/**
    Set a private data reference for the current request
    @param conn HttpConn object
    @param data Data object to associate with the current request. This must be a managed reference.
    @return Reference to private data
    @ingroup Esp
    @stability prototype
 */
PUBLIC void espSetData(HttpConn *conn, void *data);

/**
    Test if a configuration property from the ESP pak.json has a desired value.
    @param route HttpRoute defining the ESP application
    @param key Configuration property path. May contain dots.
    @param desired Desired value to compare with.
    @returns True if the configuration property has the desired value.
    @ingroup EspRoute
    @stability Evolving
 */
PUBLIC bool espTestConfig(HttpRoute *route, cchar *key, cchar *desired);

/*
    Internal
 */
PUBLIC cchar *espGetVisualStudio();
PUBLIC void espManageEspRoute(EspRoute *eroute, int flags);
PUBLIC bool espModuleIsStale(HttpRoute *route, cchar *source, cchar *module, int *recompile);
PUBLIC int espOpenDatabase(HttpRoute *route, cchar *spec);
PUBLIC void espSetDefaultDirs(HttpRoute *route, bool app);

/********************************** Requests **********************************/
/**
    View procedure callback.
    @param conn Http connection object
    @ingroup EspReq
    @stability Stable
 */
typedef void (*EspViewProc)(HttpConn *conn);

/**
    ESP Action
    @description Actions are run after a request URI is routed to a controller.
    @ingroup EspReq
    @stability Stable
 */
typedef EspProc EspAction;

PUBLIC void espManageAction(EspAction *ap, int flags);

/**
    ESP request structure
    @defgroup EspReq EspReq
    @stability Internal
    @see Esp
 */
typedef struct EspReq {
    HttpRoute       *route;                 /**< Route reference */
    Esp             *esp;                   /**< Convenient esp reference */
    MprHash         *feedback;              /**< Feedback messages */
    MprHash         *lastFeedback;          /**< Feedback messages from the last request */
    HttpNotifier    notifier;               /**< Connection Http state change notification callback */
    void            *data;                  /**< Custom data for request (managed) */
    void            *staticData;            /**< Custom data for request (unmanaged) */
    cchar           *commandLine;           /**< Command line for compile/link */
    int             autoFinalize;           /**< Request is or will be auto-finalized */
    int             sessionProbed;          /**< Already probed for session store */
    int             lastDomID;              /**< Last generated DOM ID */
    Edi             *edi;                   /**< Database for this request */
} EspReq;

/**
    Add a header to the transmission using a format string.
    @description Add a header if it does not already exist.
    @param conn HttpConn connection object
    @param key Http response header key
    @param fmt Printf style formatted string to use as the header key value
    @param ... Arguments for fmt
    @return Zero if successful, otherwise a negative MPR error code. Returns MPR_ERR_ALREADY_EXISTS if the header already
        exists.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espAddHeader(HttpConn *conn, cchar *key, cchar *fmt, ...);

/**
    Add a header to the transmission.
    @description Add a header if it does not already exist.
    @param conn HttpConn connection object
    @param key Http response header key
    @param value Value to set for the header
    @return Zero if successful, otherwise a negative MPR error code. Returns MPR_ERR_ALREADY_EXISTS if the header already
        exists.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espAddHeaderString(HttpConn *conn, cchar *key, cchar *value);

/**
    Add a request parameter value if it is not already defined.
    @param conn HttpConn connection object
    @param var Name of the request parameter to set
    @param value Value to set.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espAddParam(HttpConn *conn, cchar *var, cchar *value);

/**
    Append a transmission header.
    @description Set the header if it does not already exist. Append with a ", " separator if the header already exists.
    @param conn HttpConn connection object
    @param key Http response header key
    @param fmt Printf style formatted string to use as the header key value
    @param ... Arguments for fmt
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espAppendHeader(HttpConn *conn, cchar *key, cchar *fmt, ...);

/**
    Append a transmission header string.
    @description Set the header if it does not already exist. Append with a ", " separator if the header already exists.
    @param conn HttpConn connection object
    @param key Http response header key
    @param value Value to set for the header
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espAppendHeaderString(HttpConn *conn, cchar *key, cchar *value);

/**
    Auto-finalize transmission of the http request.
    @description If auto-finalization is enabled via #espSetAutoFinalizing, this call will finalize writing Http response
    data by writing the final chunk trailer if required. If using chunked transfers, a null chunk trailer is required
    to signify the end of write data.  If the request is already finalized, this call does nothing.
    @param conn HttpConn connection object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espAutoFinalize(HttpConn *conn);

/**
    Create a session state object.
    @description The session state object can be used to share state between requests.
    If a session has not already been created, this call will create a new session.
    It will create a response cookie containing a session ID that will be sent to the client
    with the response. Note: Objects are stored in the session state using JSON serialization.
    @param conn HttpConn connection object
    @return Session ID string
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espCreateSession(HttpConn *conn);

/**
    Destroy a session state object.
    @description This will destroy the server-side session state and
        emit an expired cookie to the client to force it to erase the session cookie.
    @param conn HttpConn connection object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espDestroySession(HttpConn *conn);

/**
    Send mail using sendmail
    @param conn HttpConn connection object
    @param to Message recipient
    @param from Message sender
    @param subject Message subject
    @param date Message creation date. Set to null to use the current date/time.
    @param mime Message mime type. Set to null for text/plain.
    @param message Message body
    @param files MprList of files to send with the message.
    @return Zero if the email is successfully sent.
    @stability Evolving
 */
PUBLIC int espEmail(HttpConn *conn, cchar *to, cchar *from, cchar *subject, MprTime date, cchar *mime,
    cchar *message, MprList *files);

/**
    Indicate the request is finalized.
    @description Calling this routine indicates that the handler has fully finished processing the request including
        processing all input, generating a full response and any other required processing. This call will invoke
        #httpFinalizeOutput and then set the request finalized flag. If the request is already finalized, this call
        does nothing. A handler MUST call httpFinalize when it has completed processing a request.
        As background: there are three finalize concepts: HttpTx.finalizedOutput means the handler has generated all
        the response output but it may not yet be fully transmited through the pipeline and to the network by the
        connector. HttpTx.finalizedConnector means the connector has sent all the output to the network.  HttpTx.finalized
        means the application has fully processed the request including reading all the input data it wishes to read
        and has generated all the output that will be generated. A fully finalized request has both HttpTx.finalized
        and HttpTx.finalizedConnector true.
    @param conn HttpConn connection object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espFinalize(HttpConn *conn);

/**
    Flush transmit data.
    @description This writes any buffered data.
    @param conn HttpConn connection object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espFlush(HttpConn *conn);

/**
    Get the current route HttpAuth object.
    @return The HttpAuth object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC HttpAuth *espGetAuth();

/**
    Get the current request connection.
    @return The HttpConn connection object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC HttpConn *espGetConn();

/**
    Get the receive body content length.
    @description Get the length of the receive body content (if any). This is used in servers to get the length of posted
        data and, in clients, to get the response body length.
    @param conn HttpConn connection object
    @return A count of the response content data in bytes.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC MprOff espGetContentLength(HttpConn *conn);

/**
    Get the receive body content type.
    @description Get the content mime type of the receive body content (if any).
    @param conn HttpConn connection object
    @return Mime type of any receive content. Set to NULL if not posted data.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetContentType(HttpConn *conn);

/**
    Get a request cookie.
    @description Get the cookie for the given name.
    @param conn HttpConn connection object
    @param name Cookie name to retrieve
    @return Return the cookie value
        Return null if the cookie is not defined.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetCookie(HttpConn *conn, cchar *name);

/**
    Get the request cookies.
    @description Get the cookies defined in the current request. This returns the HTTP cookies header with all
        cookies in one string.
    @param conn HttpConn connection object
    @return Return a string containing the cookies sent in the Http header of the last request
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetCookies(HttpConn *conn);

/**
    Get the private data reference for the current request set via #setData
    @param conn HttpConn object
    @return Reference to private data
    @ingroup EspReq
    @stability prototype
 */
PUBLIC void *espGetData(HttpConn *conn);

/**
    Get the current database instance.
    @description A route may have a default database configured via the EspDb Appweb.conf configuration directive.
    The database will be opened when the web server initializes and will be shared between all requests using the route.
    @return Edi EDI database handle
    @ingroup EspReq
    @stability Stable
 */
PUBLIC Edi *espGetDatabase(HttpConn *conn);

/**
    Get the current extended route information.
    @return EspRoute instance
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC EspRoute *espGetEspRoute(HttpConn *conn);

/**
    Get the default documents directory for the request route.
    @param conn HttpConn connection object
    @return A directory path name
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetDocuments(HttpConn *conn);

/**
    Get a feedback message defined via #feedback
    @param conn HttpConn object
    @param type type of feedback message to retrieve. This may be set to any word, but the following feedback types
        are typically supported as per RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @return Reference to the feedback message
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *espGetFeedback(HttpConn *conn, cchar *type);

/**
    Get the current database grid.
    @description The current grid is defined via #setGrid
    @return EdiGrid instance
    @ingroup EspReq
    @stability Deprecated
    @internal
 */
PUBLIC EdiGrid *espGetGrid(HttpConn *conn);

/**
    Get an rx http header.
    @description Get a http response header for a given header key.
    @param conn HttpConn connection object
    @param key Name of the header to retrieve. This should be a lower case header name. For example: "Connection"
    @return Value associated with the header key or null if the key did not exist in the response.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetHeader(HttpConn *conn, cchar *key);

/**
    Get the hash table of rx Http headers.
    @description Get the internal hash table of rx headers
    @param conn HttpConn connection object
    @return Hash table. See MprHash for how to access the hash table.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC MprHash *espGetHeaderHash(HttpConn *conn);

/**
    Get all the request http headers.
    @description Get all the rx headers. The returned string formats all the headers in the form:
        key: value\\nkey2: value2\\n...
    @param conn HttpConn connection object
    @return String containing all the headers. The caller must free this returned string.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC char *espGetHeaders(HttpConn *conn);

/**
    Get a request pararmeter as an integer.
    @description Get the value of a named request parameter as an integer. Form variables are defined via
        www-urlencoded query or post data contained in the request.
    @param conn HttpConn connection object
    @param var Name of the request parameter to retrieve
    @param defaultValue Default value to return if the variable is not defined. Can be null.
    @return Integer containing the request parameter's value
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC int espGetIntParam(HttpConn *conn, cchar *var, int defaultValue);

/**
    Get the HTTP method.
    @description This is a convenience API to return the Http method
    @return The HttpConn.rx.method property
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetMethod(HttpConn *conn);

/**
    Get a request parameter.
    @description Get the value of a named request parameter. Form variables are defined via www-urlencoded query or post
        data contained in the request.
    @param conn HttpConn connection object
    @param var Name of the request parameter to retrieve
    @param defaultValue Default value to return if the variable is not defined. Can be null.
    @return String containing the request parameter's value. Caller should not free.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetParam(HttpConn *conn, cchar *var, cchar *defaultValue);

/**
    Get the request parameter hash table.
    @description This call gets the params hash table for the current request.
        Route tokens, request query data, and www-url encoded form data are all entered into the params table after decoding.
        Use #mprLookupKey to retrieve data from the table.
    @param conn HttpConn connection object
    @return MprJson instance containing the request parameters
    @ingroup EspReq
    @stability Stable
 */
PUBLIC MprJson *espGetParams(HttpConn *conn);

/**
    Get the request URI path string.
    @description This is a convenience API to return the request URI path. This is the request URI path after removing
        query parameters. It does not include the application route prefix.
    @return The espGetConn()->rx->pathInfo
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *espGetPath(HttpConn *conn);

/**
    Get the request query string.
    @description Get query string sent with the current request.
    @param conn HttpConn connection object
    @return String containing the request query string. Caller should not free.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetQueryString(HttpConn *conn);

/**
    Get the referring URI.
    @description This returns the referring URI as described in the HTTP "referer" (yes the HTTP specification does
        spell it incorrectly) header. If this header is not defined, this routine will return the home URI as returned
        by uri("~").
    @param conn HttpConn connection object
    @return String URI back to the referring URI. If no referrer is defined, refers to the home URI.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC char *espGetReferrer(HttpConn *conn);

/**
    Get the current route HttpRoute object.
    @return The HttpRoute object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC HttpRoute *espGetRoute();

/**
    Get the default database defined on a route.
    @param route HttpRoute object
    @return Database instance object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC Edi *espGetRouteDatabase(HttpRoute *route);

/**
    Get a route variable
    @description Get the value of a request route variable.
    @param conn HttpConn connection object
    @param var Name of the request parameter to retrieve
    @return String containing the route variable value. Caller should not free.
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *espGetRouteVar(HttpConn *conn, cchar *var);

/**
    Get the session state ID.
    @description This will get the session and return the session ID. This will create a new session state storage area if
        create is true and one does not already exist. This can be used to test if the session state exists for this
        connection.
    @param conn HttpConn connection object
    @param create Set to true to create a new session if one does not already exist.
    @return The session state identifier string.
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *espGetSessionID(HttpConn *conn, int create);

/**
    Get the response status.
    @param conn HttpConn connection object
    @return An integer Http response code. Typically 200 is success.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC int espGetStatus(HttpConn *conn);

/**
    Get the Http response status message.
    @description The HTTP status message is supplied on the first line of the HTTP response.
    @param conn HttpConn connection object
    @returns A Http status message.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC char *espGetStatusMessage(HttpConn *conn);

/**
    Get the uploaded files.
    @description Get the list of uploaded files.
        This list entries are HttpUploadFile objects.
    @param conn HttpConn connection object
    @return A list of HttpUploadFile objects.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC MprList *espGetUploads(HttpConn *conn);

/**
    Get the request URI string.
    @description This is a convenience API to return the request URI. This is the request URI after removing
        query parameters. It includes any application route prefix.
    @return The espGetConn()->rx->uri
    @ingroup EspReq
    @stability Stable
 */
PUBLIC cchar *espGetUri(HttpConn *conn);

/**
    Test if a current grid has been defined.
    @return "True" if a current grid has been defined
    @ingroup EspReq
    @stability Deprecated
    @internal
 */
PUBLIC bool espHasGrid(HttpConn *conn);

/**
    Test if a current record has been defined and save to the database.
    @description This call returns "true" if a current record is defined and has been saved to the database with a
        valid "id" field.
    @return "True" if a current record with a valid "id" is defined.
    @ingroup EspReq
    @stability Deprecated
    @internal
 */
PUBLIC bool espHasRec(HttpConn *conn);

/**
    Test if the connection is being made on behalf of the current, single authenticated user.
    @description Set esp.login.single to true to enable current session tracking.
    @return true if the
    @stability Evolving
    @ingroup EspReq
 */
PUBLIC bool espIsCurrentSession(HttpConn *conn);

/**
    Test if the receive input stream is at end-of-file.
    @param conn HttpConn connection object
    @return "True" if there is no more receive data to read
    @ingroup EspReq
    @stability Stable
 */
PUBLIC bool espIsEof(HttpConn *conn);

/**
    Test if the connection is using SSL and is secure.
    @param conn HttpConn connection object
    @return "True" if the connection is using SSL.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC bool espIsSecure(HttpConn *conn);

/**
    Test if the request has been finalized.
    @description This tests if #espFinalize or #httpFinalize has been called for a request.
    @param conn HttpConn connection object
    @return "True" if the request has been finalized.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC bool espIsFinalized(HttpConn *conn);

/**
    Match a request parameter with an expected value.
    @description Compare a request parameter and return "true" if it exists and its value matches.
    @param conn HttpConn connection object
    @param var Name of the request parameter
    @param value Expected value to match
    @return "True" if the value matches
    @ingroup EspReq
    @stability Stable
 */
PUBLIC bool espMatchParam(HttpConn *conn, cchar *var, cchar *value);

/**
    Read receive body content.
        Use httpReadBlock for more options to read data.
    @description Read body content from the client. This call does not block.
    @param conn HttpConn connection object
    @param buf Buffer to accept content data
    @param size Size of the buffer
    @return A count of bytes read into the buffer
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espReceive(HttpConn *conn, char *buf, ssize size);

/**
    Redirect the client.
    @description Redirect the client to a new uri.
    @param conn HttpConn connection object
    @param status Http status code to send with the response
    @param target New target uri for the client
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espRedirect(HttpConn *conn, int status, cchar *target);

/**
    Redirect the client back to the referrer
    @description Redirect the client to the referring URI.
    @param conn HttpConn connection object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espRedirectBack(HttpConn *conn);

/**
    Remove a cookie
    @param conn HttpConn connection object
    @param name Cookie name
    @ingroup EspReq
    @stability Stable
*/
PUBLIC void espRemoveCookie(HttpConn *conn, cchar *name);

/**
    Remove a header from the transmission
    @description Remove a header if present.
    @param conn HttpConn connection object
    @param key Http response header key
    @return Zero if successful, otherwise a negative MPR error code.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC int espRemoveHeader(HttpConn *conn, cchar *key);

/**
    Remove a session state variable
    @param conn HttpConn connection object
    @param name Variable name to set
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espRemoveSessionVar(HttpConn *conn, cchar *name);

/**
    Render a formatted string.
    @description Render a formatted string of data into packets to the client. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client.
    @param conn HttpConn connection object
    @param fmt Printf style formatted string
    @param ... Arguments for fmt
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRender(HttpConn *conn, cchar *fmt, ...);

/**
    Render the client configuration string in JSON
    @param conn HttpConn connection object
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability PRototype
 */
PUBLIC ssize espRenderConfig(HttpConn *conn);

/**
    Render a block of data to the client.
    @description Render a block of data to the client. Data packets will be created as required to store the write data.
    @param conn HttpConn connection object
    @param buf Buffer containing the write data
    @param size Size of the data in buf
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderBlock(HttpConn *conn, cchar *buf, ssize size);

/**
    Render cached content.
    @description Render the saved, cached response from a prior request to this URI. This is useful if the caching
        mode has been set to "manual".
    @param conn HttpConn connection object
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderCached(HttpConn *conn);

/**
    Render an ESP document
    @description If the document is an ESP page, it will be rendered as a view via #espRenderDocument.
        Otherwise, it will be rendered using the fileHandler as a static document.
    @param conn Http connection object
    @param path Relative pathname from route->documents to the document to render.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espRenderDocument(HttpConn *conn, cchar *path);

/**
    Render an error message back to the client and finalize the request. The output is Html escaped for security.
    @param conn HttpConn connection object
    @param status Http status code
    @param fmt Printf style message format
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderError(HttpConn *conn, int status, cchar *fmt, ...);

/**
    Render feedback messages.
    @description Feedback messages for one-time messages that are sent to the client. For HTML clients, feedback
    messages use the session state store and persist for only one request. For smart/thick clients, feedback messages
    are sent as JSON responses via the espSendFeedback API. See #espSetFeedback for how to define feedback messages.
    @param conn Http connection object
    @param types Types of feedback message to retrieve. Set to "*" to retrieve all types of feedback.
        This may be set to any word, but the following feedback types are typically supported as per
        RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @return Number of bytes written
    @ingroup EspControl
    @stability Deprecated
    @internal
 */
PUBLIC ssize espRenderFeedback(HttpConn *conn, cchar *types);

/**
    Render the contents of a file back to the client.
    @param conn HttpConn connection object
    @param path File path name
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderFile(HttpConn *conn, cchar *path);

/**
    Read a table from the current database
    @param conn HttpConn connection object
    @param tableName Database table name
    @return An EDI grid containing data for the table.
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC EdiGrid *espReadTable(HttpConn *conn, cchar *tableName);

/**
    Render a formatted string after HTML escaping
    @description Render a formatted string of data and then HTML escape. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client.
    @param conn HttpConn connection object
    @param fmt Printf style formatted string
    @param ... Arguments for fmt
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
*/
PUBLIC ssize espRenderSafe(HttpConn *conn, cchar *fmt, ...);

/**
    Render a safe string of data to the client.
    @description HTML escape a string and then write the string of data to the client.
        Data packets will be created as required to store the write data. This call may block waiting for the data to
        the client to drain.
    @param conn HttpConn connection object
    @param s String containing the data to write
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderSafeString(HttpConn *conn, cchar *s);

/**
    Render a string of data to the client
    @description Render a string of data to the client. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client.
    @param conn HttpConn connection object
    @param s String containing the data to write
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderString(HttpConn *conn, cchar *s);

/**
    Render the value of a request variable to the client.
    If a request parameter is not found by the given name, consult the session store for a variable the same name.
    @description This writes the value of a request variable after HTML escaping its value.
    @param conn HttpConn connection object
    @param name Form variable name
    @return A count of the bytes actually written
    @ingroup EspReq
    @stability Stable
 */
PUBLIC ssize espRenderVar(HttpConn *conn, cchar *name);

/**
    Render an ESP view page to the client
    @param conn Http connection object
    @param view View name. The view name is interpreted relative to the matching route documents directory and may omit
        an ESP extension.
    @param flags Reserved. Set to zero.
    @return true if a vew can be rendered.
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC bool espRenderView(HttpConn *conn, cchar *view, int flags);

/**
    Send a database grid as a JSON string
    @description The JSON string is rendered as part of an enclosing "{ data: JSON }" wrapper.
    @param conn HttpConn connection object
    @param grid EDI grid
    @param flags Reserved. Set to zero.
    @return Number of bytes rendered
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC ssize espSendGrid(HttpConn *conn, EdiGrid *grid, int flags);

/**
    Send a database record as a JSON string
    @description The JSON string is rendered as part of an enclosing "{ data: JSON }" wrapper.
    @param conn HttpConn connection object
    @param rec EDI record
    @param flags Reserved. Set to zero.
    @return Number of bytes rendered
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC ssize espSendRec(HttpConn *conn, EdiRec *rec, int flags);

/**
    Send a JSON response result
    @description This renders a JSON response including the request success status, feedback message and field errors.
    The field errors apply to the current EDI record.
    The format of the response is:
        "{ success: STATUS, feedback: {messages}, fieldErrors: {messages}}" wrapper.
    The feedback messages are created via the espSetFeedback API. Field errors are created by ESP validations.
    @param conn HttpConn connection object
    @param success True if the operation was a success.
    @return Number of bytes sent.
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC ssize espSendResult(HttpConn *conn, bool success);

/**
    Enable auto-finalizing for this request
    @param conn HttpConn connection object
    @param on Set to "true" to enable auto-finalizing.
    @return "True" if auto-finalizing was enabled prior to this call
    @ingroup EspReq
    @stability Stable
 */
PUBLIC bool espSetAutoFinalizing(HttpConn *conn, bool on);

/**
    Set the current request connection.
    @param conn The HttpConn connection object to define
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetConn(HttpConn *conn);

/**
    Define a content length header in the transmission.
    @description This will define a "Content-Length: NNN" request header.
    @param conn HttpConn connection object
    @param length Numeric value for the content length header.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetContentLength(HttpConn *conn, MprOff length);

/**
    Set a cookie in the transmission
    @description Define a cookie to send in the transmission Http header
    @param conn HttpConn connection object
    @param name Cookie name
    @param value Cookie value
    @param path URI path to which the cookie applies
    @param domain String Domain in which the cookie applies. Must have 2-3 "." and begin with a leading ".".
        For example: domain: .example.com. Set to NULL to use the current connection's client domain.
    Some browsers will accept cookies without the initial ".", but the spec: (RFC 2109) requires it.
    @param lifespan Duration for the cookie to persist in msec. Set to a negative number to delete a cookie. Set to
        zero for a "session" cookie that lives only for the user's session.
    @param isSecure Set to "true" if the cookie only applies for SSL based connections
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetCookie(HttpConn *conn, cchar *name, cchar *value, cchar *path, cchar *domain, MprTicks lifespan,
    bool isSecure);

/**
    Set the transmission (response) content mime type
    @description Set the mime type Http header in the transmission
    @param conn HttpConn connection object
    @param mimeType Mime type string
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetContentType(HttpConn *conn, cchar *mimeType);

/**
    Set this authenticated session as the current session.
    @description Set esp.login.single to true to enable current session tracking.
    @return true if the
    @stability Evolving
    @ingroup EspReq
 */
PUBLIC void espSetCurrentSession(HttpConn *conn);

/**
    Clear the current authenticated session
    @stability Evolving
    @ingroup EspReq
 */
PUBLIC void espClearCurrentSession(HttpConn *conn);

/**
    Set a feedback message
    @description Feedback messages are a convenient way to aggregate messages state information in the response.
        Feedback messages are removed at the completion of the request.
    @param conn Http connection object
    @param type type of feedback message. This may be set to any word, but the following feedback types
        are typically supported as per RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @param fmt Printf style formatted string to use as the message
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetFeedback(HttpConn *conn, cchar *type, cchar *fmt, ...);

/**
    Send a feedback message
    @param conn Http connection object
    @param type type of feedback message. This may be set to any word, but the following feedback types
        are typically supported as per RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @param fmt Printf style formatted string to use as the message
    @param args Varargs style list
    @ingroup EspReq
    @stability Internal
    @internal
 */
PUBLIC void espSetFeedbackv(HttpConn *conn, cchar *type, cchar *fmt, va_list args);

/**
    Set the current database grid
    @return The grid instance. This permits chaining.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC EdiGrid *espSetGrid(HttpConn *conn, EdiGrid *grid);

/**
    Set a transmission header
    @description Set a Http header to send with the request. If the header already exists, its value is overwritten.
    @param conn HttpConn connection object
    @param key Http response header key
    @param fmt Printf style formatted string to use as the header key value
    @param ... Arguments for fmt
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetHeader(HttpConn *conn, cchar *key, cchar *fmt, ...);

/**
    Set a simple key/value transmission header
    @description Set a Http header to send with the request. If the header already exists, its value is overwritten.
    @param conn HttpConn connection object
    @param key Http response header key
    @param value String value for the key
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetHeaderString(HttpConn *conn, cchar *key, cchar *value);

/**
    Set an integer request parameter value
    @description Set the value of a named request parameter to an integer value. Form variables are defined via
        www-urlencoded query or post data contained in the request.
    @param conn HttpConn connection object
    @param var Name of the request parameter to set
    @param value Value to set.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetIntParam(HttpConn *conn, cchar *var, int value);

/**
    Define a notifier callback for this connection.
    @description The notifier callback will be invoked for state changes and I/O events as requests are processed.
    The supported events are:
    <ul>
    <li>HTTP_EVENT_STATE &mdash; The request is changing state. Valid states are:
        HTTP_STATE_BEGIN, HTTP_STATE_CONNECTED, HTTP_STATE_FIRST, HTTP_STATE_CONTENT, HTTP_STATE_READY,
        HTTP_STATE_RUNNING, HTTP_STATE_FINALIZED and HTTP_STATE_COMPLETE. A request will always visit all states and the
        notifier will be invoked for each and every state. This is true even if the request has no content, the
        HTTP_STATE_CONTENT will still be visited.</li>
    <li>HTTP_EVENT_READABLE &mdash; There is data available to read</li>
    <li>HTTP_EVENT_WRITABLE &mdash; The outgoing pipeline can absorb more data</li>
    <li>HTTP_EVENT_ERROR &mdash; The request has encountered an error</li>
    <li>HTTP_EVENT_DESTROY &mdash; The connection structure is about to be destoyed</li>
    <li>HTTP_EVENT_OPEN &mdash; The application layer is now open</li>
    <li>HTTP_EVENT_CLOSE &mdash; The application layer is now closed</li>
    </ul>
    Before the notifier is invoked, espSetConn is called to set the connection object in the thread local storage.
    This enables the ESP Abbreviated API.
    @param conn HttpConn connection object created via #httpCreateConn
    @param notifier Notifier function.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetNotifier(HttpConn *conn, HttpNotifier notifier);

/**
    Set the current database record
    @description The current record is used to supply data to various abbreviated controls, such as: text(), input(),
        checkbox and dropdown()
    @param conn HttpConn connection object
    @param rec Record object to define as the current record.
    @return The grid instance. This permits chaining.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC EdiRec *espSetRec(HttpConn *conn, EdiRec *rec);

/**
    Set a request parameter value
    @description Set the value of a named request parameter to a string value. Parameters are defined via
        requeset POST data or request URI queries. This API permits these initial request parameters to be set or
        modified.
    @param conn HttpConn connection object
    @param var Name of the request parameter to set
    @param value Value to set.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetParam(HttpConn *conn, cchar *var, cchar *value);

/**
    Set a Http response status.
    @description Set the Http response status for the request. This defaults to 200 (OK).
    @param conn HttpConn connection object
    @param status Http status code.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espSetStatus(HttpConn *conn, int status);

/**
    Set a session variable.
    @description
    @param conn Http connection object
    @param name Variable name to set
    @param value Variable value to use
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup HttpSession
    @stability Stable
 */
PUBLIC int espSetSessionVar(HttpConn *conn, cchar *name, cchar *value);

/**
    Show request details
    @description This e request details back to the client. This is useful as a debugging tool.
    @param conn HttpConn connection object
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espShowRequest(HttpConn *conn);

/**
    Update the cached content for a request
    @description Save the given content for future requests. This is useful if the caching mode has been set to "manual".
    @param conn HttpConn connection object
    @param uri Request URI to cache for
    @param data Data to cache
    @param lifesecs Time in seconds to cache the data
    @ingroup EspReq
    @stability Stable
 */
PUBLIC void espUpdateCache(HttpConn *conn, cchar *uri, cchar *data, int lifesecs);

/**
    Write a record to the database
    @description The record will be saved to the database after running any field validations. If any field validations
        fail to pass, the record will not be written and error details can be retrieved via #ediGetRecErrors.
        If the record is a new record and the "id" column is EDI_AUTO_INC, then the "id" will be assigned
        prior to saving the record.
    @param conn HttpConn connection object
    @param rec Record to write to the database.
    @return "true" if the record can be successfully written.
    @ingroup EspReq
    @stability Stable
 */
PUBLIC bool espUpdateRec(HttpConn *conn, EdiRec *rec);

/**
    Create a URI.
    @description Create a URI link by expansions tokens based on the current request and route state.
    The target parameter may contain partial or complete URI information. The missing parts
    are supplied using the current request and route tables. The resulting URI is a normalized, server-local
    URI (that begins with "/"). The URI will include any defined route prefix, but will not include scheme, host or
    port components.
    @param conn HttpConn connection object
    @param target The URI target. The target parameter can be a URI string or JSON style set of options.
        The target will have any embedded "{tokens}" expanded by using token values from the request parameters.
        If the target has an absolute URI path, that path is used directly after tokenization. If the target begins with
        "~", that character will be replaced with the route prefix. This is a very convenient way to create application
        top-level relative links.
        \n\n
        If the target is a string that begins with "{AT}" it will be interpreted as a controller/action pair of the
        form "{AT}controller/action". If the "controller/" portion is absent, the current controller is used. If
        the action component is missing, the "list" action is used. A bare "{AT}" refers to the "list" action
        of the current controller.
        \n\n
        If the target starts with "{" it is interpreted as being a JSON style set of options that describe the link.
        If the target is a relative URI path, it is appended to the current request URI path.
        \n\n
        If the target is a JSON style of options, it can specify the URI components: scheme, host, port, path, reference and
        query. If these component properties are supplied, these will be combined to create a URI.
        \n\n
        If the target specifies either a controller/action or a JSON set of options, The URI will be created according
        to the route URI template. The template may be explicitly specified
        via a "route" target property. Otherwise, if an "action" property is specified, the route of the same
        name will be used. If these don't result in a usable route, the "default" route will be used.
        \n\n
        These are the properties supported in a JSON style "{ ... }" target:
        <ul>
            <li>scheme String URI scheme portion</li>
            <li>host String URI host portion</li>
            <li>port Number URI port number</li>
            <li>path String URI path portion</li>
            <li>reference String URI path reference. Does not include "#"</li>
            <li>query String URI query parameters. Does not include "?"</li>
            <li>controller String controller name if using a controller-based route. This can also be specified via
                the action option.</li>
            <li>action String Action to invoke. This can be a URI string or a controller action of the form
                {AT}controller/action.</li>
            <li>route String Route name to use for the URI template</li>
        </ul>
    @return A normalized, server-local Uri string.
    @example espUri(conn, "http://example.com/index.html", 0); \n
    espUri(conn, "/path/to/index.html", 0); \n
    espUri(conn, "../images/splash.png", 0); \n
    espUri(conn, "~/client/images/splash.png", 0); \n
    espUri(conn, "${app}/client/images/splash.png", 0); \n
    espUri(conn, "@controller/checkout", 0); \n
    espUri(conn, "@controller/") \n
    espUri(conn, "@init") \n
    espUri(conn, "@") \n
    espUri(conn, "{ action: '@post/create' }", 0); \n
    espUri(conn, "{ action: 'checkout' }", 0); \n
    espUri(conn, "{ action: 'logout', controller: 'admin' }", 0); \n
    espUri(conn, "{ action: 'admin/logout'", 0); \n
    espUri(conn, "{ product: 'candy', quantity: '10', template: '/cart/${product}/${quantity}' }", 0); \n
    espUri(conn, "{ route: '~/STAR/edit', action: 'checkout', id: '99' }", 0); \n
    espUri(conn, "{ template: '~/client/images/${theme}/background.jpg', theme: 'blue' }", 0);
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *espUri(HttpConn *conn, cchar *target);

/***************************** Abbreviated Controls ***************************/
/**
    Abbreviated ESP API.
    @description This is a short-form API that uses the current HttpConn connection object.
        These APIs are designed to be terse and highly readable. Consequently, they are not prefixed with "esp".
    @see espAlert
    @defgroup EspAbbrev EspAbbrev
    @stability Stable
  */
typedef struct EspAbbrev { int dummy; } EspAbbrev;

/******************************* Abbreviated API ******************************/

/**
    Add a header to the transmission using a format string.
    @description Add a header if it does not already exist.
    @param key Http response header key
    @param fmt Printf style formatted string to use as the header key value
    @param ... Arguments for fmt
    @return Zero if successful, otherwise a negative MPR error code. Returns MPR_ERR_ALREADY_EXISTS if the header already
        exists.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void addHeader(cchar *key, cchar *fmt, ...);

/**
    Add a request parameter value if not already defined.
    @param name Name of the request parameter to set
    @param value Value to set.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void addParam(cchar *name, cchar *value);

/**
    Test if a user has the required abilities
    @param abilities Comma separated list of abilities to test for. If null, then use the required abilities defined
        for the current request route.
    @param warn If true, warn the user via #sendResult.
    @return True if the user has all the required abilities
    @ingroup EspAbbrev
    @stability prototype
 */
PUBLIC bool canUser(cchar *abilities, bool warn);

/**
    Create a record and initialize field values
    @description This will call #ediCreateRec to create a record based on the given table's schema. It will then
        call #ediSetFields to update the record with the given data.
    The record is remembered for this request as the "current" record and can be retrieved via: getRec().
    @param tableName Database table name
    @param data Json object with field values
    @return EdRec instance
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *createRec(cchar *tableName, MprJson *data);

/**
    Create a record from the request parameters
    @description A new record is created with the request parameters in the specified table.
    The record is remembered for this request as the "current" record and can be retrieved via: getRec().
    @param table Database table to update
    @return True if the update is successful.
    @ingroup EspAbbrev
    @stability Prototype
*/
PUBLIC bool createRecFromParams(cchar *table);

/**
    Create a session state object.
    @description The session state object can be used to share state between requests.
    If a session has not already been created, this call will create a new session.
    It will create a response cookie containing a session ID that will be sent to the client
    with the response. Note: Objects are stored in the session state using JSON serialization.
    @return Session ID string
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *createSession();

/**
    Destroy a session state object.
    @description This will emit an expired cookie to the client to force it to erase the session cookie.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void destroySession();

/**
    Don't auto-finalize this request
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void dontAutoFinalize();

/**
    Finalize the response.
    @description Signals the end of any and all response data and flushes any buffered write data to the client.
    If the request has already been finalized, this call has no additional effect.
    This routine calls #espFinalize.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void finalize();

/**
    Set a feedback message
    @description Feedback messages are a convenient way to aggregate messages state information in the response.
        The #getFeedback API can be used to retrieve feedback messages.
        Feedback messages are removed at the completion of the request.
    @param type type of feedback message. This may be set to any word, but the following feedback types
        are typically supported as per RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @param fmt Printf style formatted string to use as the message
    @return True if the request has been successful so far, i.e. there is not an error feedback message defined.
        Return false if there is an error feedback defined.
        This permits feedback to be chained as: sendResult(feedback("error", ...));
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool feedback(cchar *type, cchar *fmt, ...);

/**
    Flush transmit data.
    @description This writes any buffered data.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void flush();

/**
    Get the auth object for the current route
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC HttpAuth *getAuth();

/**
    Get a list of column names.
    @param rec Database record.
    @return An MprList of column names in the given table. If there is no record defined, an empty list is returned.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprList *getColumns(EdiRec *rec);

/**
    Get the request cookies
    @description Get the cookies defined in the current request.
    @return Return a string containing the cookies sent in the Http header of the last request.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getCookies();

/**
    Get the connection object
    @description Before a view or controller is run, the current connection object for the request is saved in thread
    local data. Most EspAbbrev APIs take an HttpConn object as an argument.
    @return HttpConn connection instance object.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC HttpConn *getConn();

/**
    Get the receive body content length
    @description Get the length of the receive body content (if any). This is used in servers to get the length of posted
        data and in clients to get the response body length.
    @return A count of the response content data in bytes.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprOff getContentLength();

/**
    Get the receive body content type
    @description Get the content mime type of the receive body content (if any).
    @return Mime type of any receive content. Set to NULL if not posted data.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getContentType();

/**
    Get the private data reference for the current request set via #setData
    @return Reference to private data
    @ingroup EspAbbrev
    @stability prototype
 */
PUBLIC void *getData();

/**
    Get the connection dispatcher object
    @return MprDispatcher connection dispatcher instance object.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprDispatcher *getDispatcher();

/**
    Get a feedback message defined via #feedback
    @param type type of feedback message to retrieve. This may be set to any word, but the following feedback types
        are typically supported as per RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @return Reference to private data
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getFeedback(cchar *type);

/**
    Get the current database instance
    @description A route may have a default database configured via the EspDb Appweb.conf configuration directive.
    The database will be opened when the web server initializes and will be shared between all requests using the route.
    @return Edi EDI database handle
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC Edi *getDatabase();

/**
    Get the extended route EspRoute structure
    @return EspRoute instance
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EspRoute *getEspRoute();

/**
    Get the default document root directory for the request route.
    @return A directory path name
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getDocuments();

/**
    Get a field from the current database record
    @param rec Database record.
    @param field Field name to return
    @return String value for "field" in the current record.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getField(EdiRec *rec, cchar *field);

/**
    Get the current database grid
    @description The current grid is defined via #setGrid
    @return EdiGrid instance
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiGrid *getGrid();

/**
    Get an rx http header.
    @description Get a http response header for a given header key.
    @param key Name of the header to retrieve. This should be a lower case header name. For example: "Connection"
    @return Value associated with the header key or null if the key did not exist in the response.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getHeader(cchar *key);

/**
    Get the HTTP method
    @description This is a convenience API to return the Http method
    @return The HttpConn.rx.method property
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *getMethod();

/**
    Get the HTTP URI query string
    @description This is a convenience API to return the query string for the current request.
    @return The espGetConn()->rx->parsedUri->query property
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getQuery();

/**
    Get the referring URI
    @description This returns the referring URI as described in the HTTP "referer" (yes the HTTP specification does
        spell it incorrectly) header. If this header is not defined, this routine will return the home URI as returned
        by uri("~").
    @return String URI back to the referring URI. If no referrer is defined, refers to the home URI.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getReferrer();

/**
    Get the ESP request object
    @return EspReq connection instance object.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EspReq *getReq();

/**
    Get the HttpRoute object for the current route
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC HttpRoute *getRoute();

/**
    Get the security token.
    @description To minimize form replay attacks, a security token may be required for POST requests on a route.
    Client-side Javascript must then send this token as a request header in subsquent POST requests.
    To configure a route to require security tokens, call #httpSetRouteXsrf.
    @return the security token.
    @ingroup EspAbbrev
    @stability Evolving
*/
PUBLIC cchar *getSecurityToken();

/**
    Get a session state variable
    @description The #session API is an alias for this routine.
    @param name Variable name to get
    @return The session variable value. Returns NULL if not set.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getSessionVar(cchar *name);

/**
    Get the session state ID.
    @description This will get a session and return the session ID. This will create a new session state storage area if
        one does not already exist.
    @return The session state identifier string.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getSessionID();

/**
    Test if a field in the current record has input validation errors
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC cchar *getFieldError(cchar *field);

/**
    Get the request URI path string
    @description This is a convenience API to return the request URI path. This is the portion after the application/route
        prefix.
    @return The espGetConn()->rx->pathInfo
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getPath();

/**
    Get the current database record
    @return EdiRec instance
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *getRec();

/**
    Get a field from the application pak.json configuration
    @param field Property field name in pak.json. May contain dots.
    @return The field value. Returns "" if the field is not found.
    @ingroup EspAbbrev
    @stability deprecated
 */
PUBLIC cchar *getConfig(cchar *field);

/**
    Get the uploaded files
    @description Get the list of uploaded files.
    @return A list of HttpUploadFile objects.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprList *getUploads();

/**
    Get the request URI string
    @description This is a convenience API to return the request URI.
    @return The espGetConn()->rx->uri
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *getUri();

/**
    Test if a current grid has been defined
    @return "true" if a current grid has been defined
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool hasGrid();

/**
    Test if a current record has been defined and save to the database
    @description This call returns "true" if a current record is defined and has been saved to the database with a
        valid "id" field.
    @return "true" if a current record with a valid "id" is defined.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool hasRec();

/**
    Render an input field as part of a form. This is a smart input control that will call the appropriate
        input control based on the database record field data type. This control should not be used
        if using the esp-angular-mvc or other similar client-side Javascript framework.
    @param field Name for the input field. This defines the HTML element name and provides the source
        of the initial value to display. The field should be a property of the form current record.
        If this call is used without a form control record, the actual data value should be supplied via the
        options.value property.
    @param options These are in JSON string form and are converted to attributes to pass to the input element
    @arg noescape Boolean Do not HTML escape the text before rendering.
    @arg ... Other options are converted and rendered as HTML attributes.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void input(cchar *field, cchar *options);

/**
    Test if the receive input stream is at end-of-file
    @return "true" if there is no more receive data to read
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool isEof();

/**
    Test if a http request is finalized.
    @description This tests if #espFinalize or #httpFinalize has been called for a request.
    @return "true" if the request has been finalized.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool isFinalized();

/**
    Test if the connection is using SSL and is secure
    @return "true" if the connection is using SSL.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool isSecure();

/**
    Make a hash table container of property values
    @description This routine formats the given arguments, parses the result as a JSON string and returns an
        equivalent hash of property values. The result after formatting should be of the form:
        hash("{ key: 'value', key2: 'value', key3: 'value' }");
    @param fmt Printf style format string
    @param ... arguments
    @return MprHash instance
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprHash *makeHash(cchar *fmt, ...);

/**
    Make a JSON object container of property values
    @description This routine formats the given arguments, parses the result into a JSON object.
    @param fmt Printf style format string
    @param ... arguments
    @return MprJson instance
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprJson *makeJson(cchar *fmt, ...);

/**
    Make a record
    @description This call makes a free-standing data record based on the JSON format content string.
        The record is not saved to the database.
    @param content JSON format content string. The content should be a set of property names and values.
    @return An EdiRec instance
    @example: rec = ediMakeRec("{ id: 1, title: 'Message One', body: 'Line one' }");
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *makeRec(cchar *content);

/**
    Create a URI.
    @description Create a URI link by expansions tokens based on the current request and route state.
    The target parameter may contain partial or complete URI information. The missing parts
    are supplied using the current request and route tables. The resulting URI is a normalized, server-local
    URI (that begins with "/"). The URI will include any defined route prefix, but will not include scheme, host or
    port components.
    @param target The URI target. The target parameter can be a URI string or JSON style set of options.
        The target will have any embedded "{tokens}" expanded by using token values from the request parameters.
        If the target has an absolute URI path, that path is used directly after tokenization. If the target begins with
        "~", that character will be replaced with the route prefix. This is a very convenient way to create application
        top-level relative links.
        \n\n
        If the target is a string that begins with "{AT}" it will be interpreted as a controller/action pair of the
        form "{AT}controller/action". If the "controller/" portion is absent, the current controller is used. If
        the action component is missing, the "list" action is used. A bare "{AT}" refers to the "list" action
        of the current controller.
        \n\n
        If the target starts with "{" it is interpreted as being a JSON style set of options that describe the link.
        If the target is a relative URI path, it is appended to the current request URI path.
        \n\n
        If the target is a JSON style of options, it can specify the URI components: scheme, host, port, path, reference and
        query. If these component properties are supplied, these will be combined to create a URI.
        \n\n
        If the target specifies either a controller/action or a JSON set of options, The URI will be created according
        to the route URI template. The template may be explicitly specified
        via a "route" target property. Otherwise, if an "action" property is specified, the route of the same
        name will be used. If these don't result in a usable route, the "default" route will be used.
        \n\n
        These are the properties supported in a JSON style "{ ... }" target:
        <ul>
            <li>scheme String URI scheme portion</li>
            <li>host String URI host portion</li>
            <li>port Number URI port number</li>
            <li>path String URI path portion</li>
            <li>reference String URI path reference. Does not include "#"</li>
            <li>query String URI query parameters. Does not include "?"</li>
            <li>controller String controller name if using a controller-based route. This can also be specified via
                the action option.</li>
            <li>action String Action to invoke. This can be a URI string or a controller action of the form
                {AT}controller/action.</li>
            <li>route String Route name to use for the URI template</li>
        </ul>
    @return A normalized, server-local Uri string.
    @example makeUri("http://example.com/index.html", 0); \n
    makeUri("/path/to/index.html", 0); \n
    makeUri("../images/splash.png", 0); \n
    makeUri("~/client/images/splash.png", 0); \n
    makeUri("${app}/client/images/splash.png", 0); \n
    makeUri("@controller/checkout", 0); \n
    makeUri("@controller/") \n
    makeUri("@init") \n
    makeUri("@") \n
    makeUri("{ action: '@post/create' }", 0); \n
    makeUri("{ action: 'checkout' }", 0); \n
    makeUri("{ action: 'logout', controller: 'admin' }", 0); \n
    makeUri("{ action: 'admin/logout'", 0); \n
    makeUri("{ product: 'candy', quantity: '10', template: '/cart/${product}/${quantity}' }", 0); \n
    makeUri("{ route: '~/STAR/edit', action: 'checkout', id: '99' }", 0); \n
    makeUri("{ template: '~/client/images/${theme}/background.jpg', theme: 'blue' }", 0);
    @ingroup EspReq
    @stability Evolving
 */
PUBLIC cchar *makeUri(cchar *target);

/**
    Get an MD5 checksum
    @param str String to hash
    @returns An allocated MD5 checksum string.
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC cchar *md5(cchar *str);

/**
    Generate a onetime random string
    @returns An MD5 encoded random string
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC cchar *nonce();

/**
    Test the the application mode
    @description This is typically set to "debug" or "release". The mode is defined by the "profile" property in the pak.json.
    @param check Mode to compare with the current application mode.
    @return True if the current app mode matches the check mode
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC bool modeIs(cchar *check);

/**
    Get a request parameter
    @description Get the value of a named request parameter. Form variables are defined via www-urlencoded query or post
        data contained in the request.
        This routine calls #espGetParam
    @param name Name of the request parameter to retrieve
    @return String containing the request parameter's value. Caller should not free.
        Returns NULL if the parameter is not defined.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *param(cchar *name);

/**
    Get the request parameter hash table
    @description This call gets the params hash table for the current request.
        Route tokens, request query data, and www-url encoded form data are all entered into the params table after decoding.
        Use #mprLookupKey to retrieve data from the table.
        This routine calls #espGetParams
    @return MprJson instance containing the request parameters
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC MprJson *params();

/**
    Read the identified record
    @description Read the record identified by the request param("id") from the nominated table.
    The record is remembered for this request as the "current" record and can be retrieved via: getRec().
    @param tableName Database table name
    @param key Key value of the record to read
    @return The identified record. Returns NULL if the table or record cannot be found.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *readRec(cchar *tableName, cchar *key);

/**
    Read matching records
    @description This runs a simple query on the database and returns matching records in a grid. The query selects
        all rows that have a "field" that matches the given "value".
    The grid of records is remembered for this request as the "current" grid and can be retrieved via: getGrid().
    @param tableName Database table name
    @param fieldName Database field name to evaluate
    @param operation Comparison operation. Set to "==", "!=", "<", ">", "<=" or ">=".
    @param value Data value to compare with the field values.
    @return A grid containing all matching records. Returns NULL if no matching records.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiGrid *readWhere(cchar *tableName, cchar *fieldName, cchar *operation, cchar *value);

/**
    Read one record
    @description This runs a simple query on the database and selects the first matching record. The query selects
        a row that has a "field" that matches the given "value".
    The record is remembered for this request as the "current" record and can be retrieved via: getRec().
    @param tableName Database table name
    @param fieldName Database field name to evaluate
    @param operation Comparison operation. Set to "==", "!=", "<", ">", "<=" or ">=".
    @param value Data value to compare with the field values.
    @return First matching record. Returns NULL if no matching records.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *readRecWhere(cchar *tableName, cchar *fieldName, cchar *operation, cchar *value);

/**
    Read a record identified by key value
    @description Read a record from the given table as identified by the key value.
    The record is remembered for this request as the "current" record and can be retrieved via: getRec().
    @param tableName Database table name
    @param key Key value of the record to read
    @return Record instance of EdiRec.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *readRecByKey(cchar *tableName, cchar *key);

/**
    Read all the records in table from the database
    @description This reads a table and returns a grid containing the table data.
    The grid of records is remembered for this request as the "current" grid and can be retrieved via: getGrid().
    @param tableName Database table name
    @return A grid containing all table rows. Returns NULL if the table cannot be found.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiGrid *readTable(cchar *tableName);

/**
    Read receive body content
    @description Read body content from the client. This will not block by default.
        Use httpReadBlock for more options to read data.
    @param buf Buffer to accept content data
    @param size Size of the buffer
    @return A count of bytes read into the buffer
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize receive(char *buf, ssize size);

/**
    Redirect the client
    @description Redirect the client to a new uri. This will redirect with an HTTP 302 status. If a different HTTP status
    code is required, use #espRedirect.
    @param target New target uri for the client
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void redirect(cchar *target);

/**
    Redirect the client back to the referrer
    @description Redirect the client to the referring URI.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void redirectBack();

/**
    Remove a cookie
    @param name Cookie name
    @ingroup EspAbbrev
    @stability Evolving
*/
PUBLIC void removeCookie(cchar *name);

/**
    Remove a record from a database table
    @description Remove the record identified by the key value from the given table.
        If the removal succeeds, the feedback message {inform: "Deleted Record"} will be created. If the removal fails,
        a feedback message {error: "Cannot delete Record"} will be created.
    @param tableName Database table name
    @param key Key value of the record to remove
    @return Record instance of EdiRec.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool removeRec(cchar *tableName, cchar *key);

/**
    Remove a session state variable
    @param name Variable name to set
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC void removeSessionVar(cchar *name);

/**
    Render a formatted string
    @description Render a formatted string of data into packets to the client. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client.
    @param fmt Printf style formatted string
    @param ... Arguments for fmt
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize render(cchar *fmt, ...);

/**
    Render cached content
    @description Render the saved, cached response from a prior request to this URI. This is useful if the caching
        mode has been set to "manual".
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize renderCached();

/**
    Render the pak.json
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC ssize renderConfig();

/**
    Render an error message back to the client and finalize the request. The output is Html escaped for security.
    @param status Http status code
    @param fmt Printf style message format
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void renderError(int status, cchar *fmt, ...);

/**
    Render feedback messages.
    @description Feedback notices are one-time messages that are passed to the next request (only).
        See #espSetFeedback and #feedback for how to define feedback messages.
        This API will render feedback messages as HTML in place of the renderFeedback call in ESP page.
    @param types Types of feedback message to retrieve. Set to "*" to retrieve all types of feedback.
        This may be set to any word, but the following feedback types are typically supported as per
        RFC 5424: "debug", "info", "notice", "warn", "error", "critical".
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void renderFeedback(cchar *types);

/**
    Render a file back to the client
    @description Render a formatted string of data and then HTML escape. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client.
    @param path Filename of the file to send to the client.
    @param ... Arguments for fmt
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize renderFile(cchar *path);

/**
    Render a formatted string after HTML escaping
    @description Render a formatted string of data and then HTML escape. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client.
    @param fmt Printf style formatted string
    @param ... Arguments for fmt
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize renderSafe(cchar *fmt, ...);

/**
    Render an input field with a hidden XSRF security token.
    @description Security tokens are used to help guard against CSRF threats.
    This call will generate a hidden input field that includes the CSRF security token for the form.
    This call should not be included in Angular client applications as the Angular framework will automatically
    handle the security token.
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC void inputSecurityToken();

/**
    Render a string of data to the client
    @description Render a string of data to the client. Data packets will be created
        as required to store the write data. This call may block waiting for data to drain to the client.
    @param s String containing the data to write
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize renderString(cchar *s);

/**
    Render the value of a request variable to the client.
    If a request parameter is not found by the given name, consult the session store for a variable the same name.
    @description This writes the value of a request variable after HTML escaping its value.
    @param name Form variable name
    @return A count of the bytes actually written
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC ssize renderVar(cchar *name);

/**
    Render an ESP page to the client
    @param view View name. The view name is interpreted relative to the matching route documents directory and may omit
        an ESP extension.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void renderView(cchar *view);

/**
    Run a command
    @description Run a command and return output.
    @param command Command line and arguments to run.
    @param input Input data to pass to the command. Set to null if not required.
    @param output Pointer to accept command standard output response. Set to null if not required.
    @param error Pointer to accept command standard error response. Set to null if not required.
    @param flags MprCmd flags. Use MPR_CMD_DETACH to run in the background.
    @param timeout Time in milliseconds to wait for the command to complete and exit.
    @ingroup EspAbbrev
    @stability Prototype
 */
PUBLIC int runCmd(cchar *command, char *input, char **output, char **error, MprTicks timeout, int flags);

#if DEPRECATED || 1
/**
    Render scripts
    @description This renders script elements for all matching filenames on the server.
    @param patterns An enhanced glob-style expression pattern. The format is is a comma separated string of filename
    expressions. Each expression may contain the wildcard tokens: "*" which matches any filename portion, "**" which matches
    any filename portion in any subdirectory. An expression may be prefixed with "!" to exclude files of that expression.
    @ingroup EspAbbrev
    @stability Deprecated
 */
PUBLIC void scripts(cchar *patterns);
#endif

/**
    Send a Edatabase grid as a JSON string
    @description The JSON string is rendered as part of an enclosing "{ data: JSON }" wrapper.
    This API is used to send database data to client user interfaces such as Angular or Aurelia clients.
    @param grid EDI grid
    @return Number of bytes sent
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC ssize sendGrid(EdiGrid *grid);

/**
    Send a database record as a JSON string
    @description The JSON string is rendered as part of an enclosing "{ data: JSON }" wrapper.
    This API is used to send database data to client user interfaces such as Angular or Aurelia clients.
    @param rec EDI record
    @return Number of bytes sent
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC ssize sendRec(EdiRec *rec);

/**
    Send a JSON response result
    @description This sends a JSON response including the request success status, feedback message and field errors.
    This API is used to send controller action responses to client user interfaces such as Angular or Aurelia clients.
    The field errors apply to the current EDI record.
    The format of the response is:
        "{ success: STATUS, feedback: {messages}, fieldErrors: {messages}}" wrapper.
    The feedback messages are created via the espSetFeedback API. Field errors are created by ESP validations.
    @param status Request success status. Note: this is not the HTTP response status code.
    @ingroup EspReq
    @stability Evolving
  */
PUBLIC void sendResult(bool status);

#if DEPRECATED || 1
/**
    Render stylesheets
    @description This renders stylesheet elements for all matching filenames on the server.
    @param patterns An enhanced glob-style expression pattern. The format is is a comma separated string of filename
    expressions. Each expression may contain the wildcard tokens: "*" which matches any filename portion, "**" which matches
    any filename portion in any subdirectory. An expression may be prefixed with "!" to exclude files of that expression.
    @ingroup EspAbbrev
    @stability Deprecated
 */
PUBLIC void stylesheets(cchar *patterns);
#endif

/**
    Add the security token to the response.
    @description To minimize form replay attacks, a security token may be required for POST requests on a route.
    This call will set a security token in the response as a response header and as a response cookie.
    Client-side Javascript must then send this token as a request header in subsquent POST requests.
    To configure a route to require security tokens, call #httpSetRouteXsrf.
    @ingroup EspAbbrev
    @stability Evolving
*/
PUBLIC void securityToken();

/**
    Get a session state variable
    @description This is a convenient alias for #getSessionVar.
    @param name Variable name to get
    @return The session variable value. Returns NULL if not set.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *session(cchar *name);

/**
    Define a cookie header to send with the response. The Path, Domain, and Expires properties can be set to null for
    default values.
    @param name Cookie name
    @param value Cookie value
    @param path Uri path to which the cookie applies
    @param domain String Domain in which the cookie applies. Must have 2-3 "." and begin with a leading ".".
        For example: domain: .example.com
        Some browsers will accept cookies without the initial ".", but the spec: (RFC 2109) requires it.
    @param lifespan Lifespan of the cookie in seconds.
    @param isSecure Boolean Set to "true" if the cookie only applies for SSL based connections
    @ingroup EspAbbrev
    @stability Evolving
*/
PUBLIC void setCookie(cchar *name, cchar *value, cchar *path, cchar *domain, MprTicks lifespan, bool isSecure);

/**
    Set the current request connection.
    @param conn The HttpConn connection object to define
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setConn(HttpConn *conn);

/**
    Set the transmission (response) content mime type
    @description Set the mime type Http header in the transmission
    @param mimeType Mime type string
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setContentType(cchar *mimeType);

/**
    Set a private data reference for the current request
    @return Reference to private data
    @ingroup EspAbbrev
    @stability prototype
 */
PUBLIC void setData(void *data);

/**
    Update a record field without writing to the database
    @description This routine updates the record object with the given value. The record will not be written
        to the database. To write to the database, use #updateRec.
    @param rec Record to update
    @param fieldName Record field name to update
    @param value Value to update
    @return The record instance if successful, otherwise NULL.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *setField(EdiRec *rec, cchar *fieldName, cchar *value);

/**
    Update record fields without writing to the database
    @description This routine updates the record object with the given values. The "data' argument supplies
        a hash of fieldNames and values. The data hash may come from the request #params or it can be manually
        created via #ediMakeHash to convert a JSON string into an options hash.
        For example: updateFields(rec, hash("{ name: '%s', address: '%s' }", name, address))
        The record will not be written to the database. To write to the database, use #ediUpdateRec.
    @param rec Record to update
    @param data Json object of field data.
    @return The record instance if successful, otherwise NULL.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *setFields(EdiRec *rec, MprJson *data);

/**
    Set the current database grid
    @return The grid instance. This permits chaining.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiGrid *setGrid(EdiGrid *grid);

/**
    Set a transmission header
    @description Set a Http header to send with the request. If the header already exists, its value is overwritten.
    @param key Http response header key
    @param fmt Printf style formatted string to use as the header key value
    @param ... Arguments for fmt
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setHeader(cchar *key, cchar *fmt, ...);

/**
    Set an integer request parameter value
    @description Set the value of a named request parameter to an integer value. Form variables are defined via
        www-urlencoded query or post data contained in the request.
    @param name Name of the request parameter to set
    @param value Integer value to set.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setIntParam(cchar *name, int value);

/**
    Set a notifier callback for the connection.
    This wraps httpSetConnNotifier and calls espSetConn before invoking the notifier for
    connection events.
    @param notifier Callback function
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setNotifier(HttpNotifier notifier);

/**
    Set a request parameter value
    @description Set the value of a named request parameter to a string value. Parameters are defined via
        requeset POST data or request URI queries. This API permits these initial request parameters to be set or
        modified.
    @param name Name of the request parameter to set
    @param value Value to set.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setParam(cchar *name, cchar *value);

/**
    Set the current database record
    @description The current record is used to supply data to various abbreviated controls, such as: text(), input(),
        checkbox and dropdown()
    @return The grid instance. This permits chaining.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC EdiRec *setRec(EdiRec *rec);

/**
    Set a session state variable
    @param name Variable name to set
    @param value Value to set
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setSessionVar(cchar *name, cchar *value);

/**
    Set a Http response status.
    @description Set the Http response status for the request. This defaults to 200 (OK).
    @param status Http status code.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setStatus(int status);

/**
    Create a timeout event
    @description invoke the given procedure after the timeout
    @param proc Function to invoke
    @param timeout Time in milliseconds to elapse before invoking the timeout
    @param data Argument to pass to proc
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void setTimeout(void *proc, MprTicks timeout, void *data);

/**
    Show request details
    @description This echoes request details back to the client. This is useful as a debugging tool.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void showRequest();

//  FUTURE - document
PUBLIC EdiGrid *sortGrid(EdiGrid *grid, cchar *sortColumn, int sortOrder);

/**
    Update the cached content for a request
    @description Save the given content for future requests. This is useful if the caching mode has been set to "manual".
    @param uri Request URI to cache for
    @param data Data to cache
    @param lifesecs Time in seconds to cache the data
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC void updateCache(cchar *uri, cchar *data, int lifesecs);

/**
    Write a value to a database table field
    @description Update the value of a table field in the selected table row. Note: validations are not run.
    @param tableName Database table name
    @param key Key value for the table row to update.
    @param fieldName Column name to update
    @param value Value to write to the database field
    @return "true" if the field  can be successfully written.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool updateField(cchar *tableName, cchar *key, cchar *fieldName, cchar *value);

/**
    Write field values to a database row
    @description This routine updates the current record with the given data and then saves the record to
        the database. The "data' argument supplies
        a hash of fieldNames and values. The data hash may come from the request #params or it can be manually
        created via #ediMakeHash to convert a JSON string into an options hash.
        For example: ediWriteFields(rec, params());
        The record runs field validations before saving to the database.
    @param tableName Database table name
    @param data Json object of fields to update
    @return "true" if the field  can be successfully written. Returns false if field validations fail.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool updateFields(cchar *tableName, MprJson *data);

/**
    Write a record to the database
    @description The record will be saved to the database after running any field validations. If any field validations
        fail to pass, the record will not be written and error details can be retrieved via #ediGetRecErrors.
        If the record is a new record and the "id" column is EDI_AUTO_INC, then the "id" will be assigned
        prior to saving the record.
        If the update succeeds, the feedback message {inform: "Saved Record"} will be created. If the update fails,
        a feedback message {error: "Cannot save Record"} will be created.
    @param rec Record to write to the database.
    @return "true" if the record can be successfully written.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC bool updateRec(EdiRec *rec);

/**
    Update a record from the request parameters
    @description The record identified by the params(id) is read and updated with the request parameters.
    @param table Database table to update
    @return True if the update is successful.
    @ingroup EspAbbrev
    @stability Prototype
*/
PUBLIC bool updateRecFromParams(cchar *table);

/**
    Create a URI link.
    @description Create a URI link based on a given target an expanding embedded tokens based on the current request and
        route state. The target URI parameter may contain partial or complete URI information. The missing parts
    are supplied using the current request and route tables.
    @param target The URI target. The target parameter can be a URI string or JSON style set of options.
        The target will have any embedded "{tokens}" expanded by using token values from the request parameters.
        If the target has an absolute URI path, that path is used directly after tokenization. If the target begins with
        "~", that character will be replaced with the route prefix. This is a very convenient way to create application
        top-level relative links.
        \n\n
        If the target is a string that begins with "{AT}" it will be interpreted as a service/action pair of the
        form "{AT}Service/action". If the "service/" portion is absent, the current service is used. If
        the action component is missing, the "list" action is used. A bare "{AT}" refers to the "list" action
        of the current service.
        \n\n
        If the target starts with "{" it is interpreted as being a JSON style set of options that describe the link.
        If the target is a relative URI path, it is appended to the current request URI path.
        \n\n
        If the is a JSON style of options, it can specify the URI components: scheme, host, port, path, reference and
        query. If these component properties are supplied, these will be combined to create a URI.
        \n\n
        If the target specifies either a service/action or a JSON set of options, The URI will be created according
        to the route URI template. The template may be explicitly specified
        via a "route" target property. Otherwise, if an "action" property is specified, the route of the same
        name will be used. If these don't result in a usable route, the "default" route will be used.
        \n\n
        These are the properties supported in a JSON style "{ ... }" target:
        <ul>
            <li>scheme String URI scheme portion</li>
            <li>host String URI host portion</li>
            <li>port Number URI port number</li>
            <li>path String URI path portion</li>
            <li>reference String URI path reference. Does not include "#"</li>
            <li>query String URI query parameters. Does not include "?"</li>
            <li>service String Service name if using a Service-based route. This can also be specified via
                the action option.</li>
            <li>action String Action to invoke. This can be a URI string or a Service action of the form
                {AT}Service/action.</li>
            <li>route String Route name to use for the URI template</li>
        </ul>
    @param ... arguments to the formatted target string
    @return A normalized Uri string.
    @ingroup EspAbbrev
    @stability Evolving
    @remarks Examples:<pre>
    uri("http://example.com/index.html");
    uri("/path/to/index.html");
    uri("../images/splash.png");
    uri("~/static/images/splash.png");
    uri("${app}/static/images/splash.png");
    uri("@service/checkout");
    uri("@service/")               //  Service = Service, action = index
    uri("@init")                   //  Current service, action = init
    uri("@")                       //  Current service, action = index
    uri("{ action: '@post/create' }");
    uri("{ action: 'checkout' }");
    uri("{ action: 'logout', service: 'admin' }");
    uri("{ action: 'admin/logout'");
    uri("{ product: 'candy', quantity: '10', template: '/cart/${product}/${quantity}' }");
    uri("{ route: '~/STAR/edit', action: 'checkout', id: '99' }");
    uri("{ template: '~/static/images/${theme}/background.jpg', theme: 'blue' }");
</pre>
 */
PUBLIC cchar *uri(cchar *target, ...);

/**
    Create an absolute URI with a scheme and host
    @param target The URI target. See httpLink for details
    @param ... arguments to the formatted target string
    @return A normalized, absolute Uri string containing scheme and host.
    @ingroup EspAbbrev
    @stability Evolving
 */
PUBLIC cchar *absuri(cchar *target, ...);

#if DEPRECATED || 1

#define espGetFlash(conn, type) espGetFeedback(conn, type)
#define espRenderFlash(conn, types) espRenderFeedback(conn, types)
#define espSetFlashv(conn, type, fmt, args) espSetFeedbackv(conn, type, fmt, args)
#define getFlash(type) getFeedback(type)
#define renderFlash(types) renderFeedback(types)

PUBLIC void espSetFlash(HttpConn *conn, cchar *type, cchar *fmt, ...);
PUBLIC void flash(cchar *type, cchar *fmt, ...);
#endif /* DEPRECATED */

#ifdef __cplusplus
} /* extern C */
#endif
#endif /* _h_ESP */

/*
    Copyright (c) Embedthis Software. All Rights Reserved.
    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.
 */

#endif /* ME_COM_ESP */
