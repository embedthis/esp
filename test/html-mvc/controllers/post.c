/*
    post Controller (esp-html-skeleton)
 */
#include "esp.h"

/*
    Create a new resource in the database
 */
static void createPost() {
    if (updateRec(createRec("post", params(NULL)))) {
        feedback("info", "New post Created");
        renderView("post/list");
    } else {
        feedback("error", "Cannot Create Post");
        renderView("post/edit");
    }
}

/*
    Prepare an edit template with the resource
 */
static void editPost() {
    readRec("post", param("id"));
}

/*
    Get a resource
 */
static void getPost() {
    readRec("post", param("id"));
    renderView("post/edit");
}

/*
    Initialize a new resource for the client to complete
 */
static void initPost() {
    createRec("post", 0);
    renderView("post/edit");
}

/*
    List the resources in this group
 */
static void listPost() {
    renderView("post/list");
}

/*
    Remove a resource identified by the "id" parameter
 */
static void removePost() {
    if (removeRec("post", param("id"))) {
        feedback("info", "Post Removed");
    }
    redirect(".");
}

/*
    Update an existing resource in the database
    If "id" is not defined, this is the same as a create
    Also we tunnel delete here if the user clicked delete
 */
static void updatePost() {
    if (smatch(param("submit"), "Delete")) {
        removePost();
    } else {
        if (updateFields("post", params(NULL))) {
            feedback("info", "Post Updated Successfully");
            redirect(".");
        } else {
            feedback("error", "Cannot Update Post");
            renderView("post/edit");
        }
    }
}

/*
    Redirect to get a properly terminated URL. Essential for relative URL references.
 */
static void redirectPost() {
    redirect(sjoin(getUri(), "/", NULL));
}

static void commonPost(HttpConn *conn, EspAction *action) {
}

/*
    Dynamic module initialization
 */
ESP_EXPORT int esp_controller_html_mvc_post(HttpRoute *route) {
    cchar   *role = "user";
    espController(route, commonPost);
    espAction(route, "post/create", role, createPost);
    espAction(route, "post/remove", role, removePost);
    espAction(route, "post/edit", role, editPost);
    espAction(route, "post/get", role, getPost);
    espAction(route, "post/init", role, initPost);
    espAction(route, "post/list", role, listPost);
    espAction(route, "post/update", role, updatePost);
    espAction(route, "post/", role, listPost);
    espAction(route, "post", role, redirectPost);

#if SAMPLE_VALIDATIONS
    Edi *edi = espGetRouteDatabase(route);
    ediAddValidation(edi, "present", "post", "title", 0);
    ediAddValidation(edi, "unique", "post", "title", 0);
    ediAddValidation(edi, "banned", "post", "body", "(swear|curse)");
    ediAddValidation(edi, "format", "post", "phone", "/^\\(?([0-9]{3})\\)?[-. ]?([0-9]{3})[-. ]?([0-9]{4})$/");
#endif
    return 0;
}
