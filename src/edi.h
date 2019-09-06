/*
    edi.h -- Embedded Database Interface (EDI).

    This interface sits atop a SQLite driver and the in-memory database MDB.

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
/*
    Forward declare structures
 */
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
PUBLIC EdiService *ediCreateService(void);

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
#define EDI_TYPE_DATE       3           /**< Date type (stored as epoch) */
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
#define EDI_READ_ONLY       0x20        /**< Field flag -- Field is read-only (not implemented) */

/*
    Encodings
 */
#define EDI_ENCODE_PREFIX   0x

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
    int             count;              /**< Total count of available records matching query */
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
    Define database migration callbacks
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
    EdiGrid   *(*readGrid)(Edi *edi, cchar *tableName, cchar *query);
    EdiRec    *(*readRecByKey)(Edi *edi, cchar *tableName, cchar *key);
    int       (*removeColumn)(Edi *edi, cchar *tableName, cchar *columnName);
    int       (*removeIndex)(Edi *edi, cchar *tableName, cchar *indexName);
    int       (*removeRecByKey)(Edi *edi, cchar *tableName, cchar *key);
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
    @description This will create an empty record using the given database tableName to supply the record schema. Use
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
    @description Used for debugging only.
    @param grid EDI grid
    @ingroup Edi
    @stability Prototype
 */
PUBLIC void ediDumpGrid(EdiGrid *grid);

/**
    Display a record to the debug log
    @description Used for debugging only.
    @param rec Record to log
    @ingroup Edi
    @stability Prototype
 */
PUBLIC void ediDumpRec(EdiRec *rec);

/**
    Get a list of database column names.
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
    @param flags Reserved. Set to MPR_JSON_PRETTY for a prettier format.
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
    Run a database query query.
    @description This runs a provider dependant query. For the SDB SQLite provider, this runs an SQL statement.
        The "mdb" provider does not implement this API. To do queries using the "mdb" provider, use:
        #ediReadRec, #ediReadGrid and #ediReadField.
        The query may contain positional parameters via argc/argv or via a va_list. These are recommended to mitigate SQL injection risk.
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
    @param fmt Reserved and not yet implemented. Set to NULL.
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
    Read matching records in a table
    @description This runs a SQL like query on the database and returns matching records in a grid. The query selects
        the rows that have matching fields.
    @param edi Database handle
    @param tableName Database table name
    @param query SQL like query expression. This arg is a printf style format string. When expanded, this will contain
        a SQL style query expression of the form: "Field Op Value AND field OP value ... LIMIT offset, limit".
        All fields may be matched by using the pseudo column name "*". Where OP is "==", "!=", "<", ">", "<=", ">=" or "><".
    @return A grid containing all matching records. Returns NULL if no matching records.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiGrid *ediReadGrid(Edi *edi, cchar *tableName, cchar *query);

/**
    Read one record.
    @description This runs a simple query on the database and selects the first matching record. The query selects
        a row that has a "field" that matches the given "value".
    @param edi Database handle
    @param tableName Database table name
    @param query SQL like query expression. This arg is a printf style format string. When expanded, this will contain
        a SQL style query expression of the form: "Field Op Value AND field OP value ... LIMIT offset, limit".
        All fields may be matched by using the pseudo column name "*". Where OP is "==", "!=", "<", ">", "<=", ">=" or "><".
    @return First matching record. Returns NULL if no matching records.
    @ingroup Edi
    @stability Deprecated
 */
PUBLIC EdiRec *ediReadRec(Edi *edi, cchar *tableName, cchar *query);

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
PUBLIC EdiRec *ediReadRecByKey(Edi *edi, cchar *tableName, cchar *key);

#if DEPRECATED && KEEP
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
    Read one record.
    @description This runs a simple query on the database and selects the first matching record. The query selects
        a row that has a "field" that matches the given "value".
        This API is deprecated, use ediReadGrid instead.
    @param edi Database handle
    @param tableName Database table name
    @param fieldName Database field name to evaluate
    @param operation Comparision operation. Set to "==", "!=", "<", ">", "<=" or ">=".
    @param value Data value to compare with the field values.
    @return First matching record. Returns NULL if no matching records.
    @ingroup Edi
    @stability Deprecated
 */
PUBLIC EdiRec *ediReadRecWhere(Edi *edi, cchar *tableName, cchar *fieldName, cchar *operation, cchar *value);

/**
    Read matching records.
    @description This runs a simple query on the database and returns matching records in a grid. The query selects
        all rows that have a "field" that matches the given "value".
        This API is deprecated, use ediReadGrid instead.
    @param edi Database handle
    @param tableName Database table name
    @param fieldName Database field name to evaluate
    @param operation Comparision operation. Set to "==", "!=", "<", ">", "<=" or ">=".
    @param value Data value to compare with the field values.
    @return A grid containing all matching records. Returns NULL if no matching records.
    @ingroup Edi
    @stability Deprecated
 */
PUBLIC EdiGrid *ediReadWhere(Edi *edi, cchar *tableName, cchar *fieldName, cchar *operation, cchar *value);
#endif

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
    Delete a row in a database table identified by the query expression
    @param edi Database handle
    @param tableName Database table name
    @param query SQL like query expression. This arg is a printf style format string. When expanded, this will contain
        a SQL style query expression of the form: "Field Op Value AND field OP value ... LIMIT offset, limit".
        All fields may be matched by using the pseudo column name "*". Where OP is "==", "!=", "<", ">", "<=", ">=" or "><".
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediRemoveRec(Edi *edi, cchar *tableName, cchar *query);

/**
    Delete a row in a database table identified by a key value
    @param edi Database handle
    @param tableName Database table name
    @param key Key column value to delete.
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediRemoveRecByKey(Edi *edi, cchar *tableName, cchar *key);

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
    Set a record field using a format string.
    @description This routine updates the record object with the given value. The record will not be written
        to the database. To write to the database, use #ediUpdateRec.
    @param rec Record to update
    @param fieldName Record field name to update
    @param fmt Format string
    @param ... Variable arguments for the format string
    @return The record instance if successful, otherwise NULL.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC EdiRec *ediSetFieldFmt(EdiRec *rec, cchar *fieldName, cchar *fmt, ...);

/**
    Set record fields without writing to the database.
    @description This routine updates the record object with the given values. The "data' argument supplies
        the fieldNames and values. The data may come from the request params() or it can be manually
        created via #ediMakeJson.
        For example: ediSetFields(rec, mprParseJson("{ name: '%s', address: '%s' }", name, address))
        The record will not be written to the database. To write to the database, use #ediUpdateRec.
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
    Write a formatted value to a database table field.
    @description Update the value of a table field in the selected table row. Note: field validations are not run.
    @param edi Database handle
    @param tableName Database table name
    @param key Key value for the table row to update.
    @param fieldName Column name to update
    @param fmt Value format string
    @param ... Variable arguments for the format string
    @return Zero if successful. Otherwise a negative MPR error code.
    @ingroup Edi
    @stability Evolving
 */
PUBLIC int ediUpdateFieldFmt(Edi *edi, cchar *tableName, cchar *key, cchar *fieldName, cchar *fmt, ...);

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

#if UNUSED
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
#endif

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
PUBLIC void mdbInit(void);
#endif

#if ME_COM_SQLITE
PUBLIC void sdbInit(void);
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
