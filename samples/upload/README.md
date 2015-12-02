ESP Upload Sample
===

This sample shows how to configure ESP for file upload.

The sample includes an upload web form: web/upload/upload-form.html. This form will
post the uploaded file to the web/upload/upload.esp page.

For security, file upload is restricted to URIs starting with /upload.

Requirements
---
* [ESP](https://embedthis.com/esp/download.html)

To run:
---
    esp

The server listens on port 4000. Browse to: 
 
     http://localhost:4000/upload/upload-form.html

Code:
---
* [upload-form.html](upload-form.html) - File upload form
* [upload.esp](upload.esp) - ESP page to receive the uploaded file
* [cache](cache) - Compiled ESP modules

Documentation:
---
* [ESP Documentation](https://embedthis.com/esp/doc/index.html)
* [File Upload)(https://embedthis.com/esp/doc/users/uploading.html)
* [ESP Configuration](https://embedthis.com/esp/doc/users/config.html)
* [ESP APIs](https://embedthis.com/esp/doc/ref/api/esp.html)
* [ESP Guide](https://embedthis.com/esp/doc/users/index.html)
* [ESP Overview](https://embedthis.com/esp/doc/users/using.html)

See Also:
---
* [html-mvc - ESP HTML MVC Application](../html-mvc/README.md)
* [controller - Creating ESP controllers](../controller/README.md)
* [page - Serving ESP pages](../page/README.md)
