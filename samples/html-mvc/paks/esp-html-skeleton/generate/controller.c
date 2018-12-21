/*
    ${CONTROLLER} Controller (esp-html-skeleton)
 */
#include "esp.h"

/*
    Create a new resource in the database
 */
static void create${UCONTROLLER}() {
    if (updateRec(createRec("${CONTROLLER}", params()))) {
        feedback("info", "New ${CONTROLLER} Created");
        renderView("${CONTROLLER}/list");
    } else {
        feedback("error", "Cannot Create ${UCONTROLLER}");
        renderView("${CONTROLLER}/edit");
    }
}

/*
    Prepare an edit template with the resource
 */
static void edit${UCONTROLLER}() {
    readRec("${CONTROLLER}", param("id"));
}

/*
    Get a resource
 */
static void get${UCONTROLLER}() {
    readRec("${CONTROLLER}", param("id"));
    renderView("${CONTROLLER}/edit");
}

/*
    Initialize a new resource for the client to complete
 */
static void init${UCONTROLLER}() {
    createRec("${CONTROLLER}", 0);
    renderView("${CONTROLLER}/edit");
}

/*
    List the resources in this group
 */
static void list${UCONTROLLER}() {
    renderView("${CONTROLLER}/list");
}

/*
    Remove a resource identified by the "id" parameter
 */
static void remove${UCONTROLLER}() {
    if (removeRec("${CONTROLLER}", param("id"))) {
        feedback("info", "${UCONTROLLER} Removed");
    }
    redirect(".");
}

/*
    Update an existing resource in the database
    If "id" is not defined, this is the same as a create
    Also we tunnel delete here if the user clicked delete
 */
static void update${UCONTROLLER}() {
    if (smatch(param("submit"), "Delete")) {
        remove${UCONTROLLER}();
    } else {
        if (updateFields("${CONTROLLER}", params())) {
            feedback("info", "${UCONTROLLER} Updated Successfully");
            redirect(".");
        } else {
            feedback("error", "Cannot Update ${UCONTROLLER}");
            renderView("${CONTROLLER}/edit");
        }
    }
}

/*
    Redirect to get a properly terminated URL. Essential for relative URL references.
 */
static void redirect${UCONTROLLER}() {
    redirect(sjoin(getUri(), "/", NULL));
}

static void common${UCONTROLLER}(HttpStream *stream) {
}

/*
    Dynamic module initialization
 */
ESP_EXPORT int esp_controller_${NAME}_${CONTROLLER}(HttpRoute *route) {
    espDefineBase(route, common${UCONTROLLER});
    espDefineAction(route, "${CONTROLLER}/create", create${UCONTROLLER});
    espDefineAction(route, "${CONTROLLER}/remove", remove${UCONTROLLER});
    espDefineAction(route, "${CONTROLLER}/edit", edit${UCONTROLLER});
    espDefineAction(route, "${CONTROLLER}/get", get${UCONTROLLER});
    espDefineAction(route, "${CONTROLLER}/init", init${UCONTROLLER});
    espDefineAction(route, "${CONTROLLER}/list", list${UCONTROLLER});
    espDefineAction(route, "${CONTROLLER}/update", update${UCONTROLLER});
    espDefineAction(route, "${CONTROLLER}/", list${UCONTROLLER});
    espDefineAction(route, "${CONTROLLER}", redirect${UCONTROLLER});
${DEFINE_ACTIONS}
#if SAMPLE_VALIDATIONS
    Edi *edi = espGetRouteDatabase(route);
    ediAddValidation(edi, "present", "${CONTROLLER}", "title", 0);
    ediAddValidation(edi, "unique", "${CONTROLLER}", "title", 0);
    ediAddValidation(edi, "banned", "${CONTROLLER}", "body", "(swear|curse)");
    ediAddValidation(edi, "format", "${CONTROLLER}", "phone", "/^\\(?([0-9]{3})\\)?[-. ]?([0-9]{3})[-. ]?([0-9]{4})$/");
#endif
    return 0;
}
