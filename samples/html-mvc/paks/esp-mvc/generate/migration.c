/*
    ${COMMENT}
 */
#include "esp.h"

static int forward(Edi *db) {
${FORWARD}    return 0;
}

static int backward(Edi *db) {
${BACKWARD}    return 0;
}

ESP_EXPORT int esp_migration_${MIGRATION}(Edi *db)
{
    ediDefineMigration(db, forward, backward);
    return 0;
}
