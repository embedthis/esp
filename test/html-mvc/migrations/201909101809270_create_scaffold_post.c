/*
    Create Scaffold Post
 */
#include "esp.h"

static int forward(Edi *db) {
    ediAddTable(db, "post");
    ediAddColumn(db, "post", "id", EDI_TYPE_INT, EDI_AUTO_INC | EDI_INDEX | EDI_KEY);
    ediAddColumn(db, "post", "title", EDI_TYPE_STRING, 0);
    ediAddColumn(db, "post", "body", EDI_TYPE_TEXT, 0);
    return 0;
}

static int backward(Edi *db) {
    ediRemoveTable(db, "post");
    return 0;
}

ESP_EXPORT int esp_migration_create_scaffold_post(Edi *db)
{
    ediDefineMigration(db, forward, backward);
    return 0;
}
