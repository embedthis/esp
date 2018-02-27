ESP MVC Sample
===

This sample demonstrates an ESP MVC application using server-side HTML page views. 
The app is a trivial blogging application. Posts with a title and body can be created, listed and deleted.

An alternative approach is to use a client-side Javascript framework to generate views on the client.
Te [esp-angular-skeleton](../esp-angular-skeleton/README.md) sample demonstrates such an approach.

The app contains:

* blog database with post table in the db directory
* post controller to manage posts in the controllers directory
* post views to create, list and display posts in the documents/post directory
* master view layout under the layouts directory

This app was generated with these commands:

    pak --name blog install esp-html-skeleton
    esp generate scaffold post title:string body:text

Requirements
---
* [Expansive](https://embedthis.com/expansive/download.html)
* [Pak Package Manager](https://embedthis.com/pak/download.html)

To build:
---

    expansive render

To run:
---
    esp 

or
    expansive

The server listens on port 4000. Browse to: 
 
     http://localhost:4000/post

Notes:
---
If you modify the controller or web pages they will be automatically recompiled and reloaded when next accessed.
The expansive tool is used to apply page layouts and render content under "source" into the "documents" directory.

Code:
---
* [controllers](controllers/post.c) - Post controller
* [cache](cache) - Directory of compiled ESP modules
* [documents](documents) - Client-side public web content
* [documents/post](documents/post) - Blogging post scaffold and view pages
* [documents/assets](documents/assets) - Client-side media assets
* [documents/css](documents/css) - Client-side CSS and Less stylesheets
* [documents/index.esp](documents/index.esp) - Application home page
* [db](db) - Database directory for the blog application
* [db/blog.mdb](db/blog.mdb) - Blog database 
* [db/migrations](db/migrations) - Database base migrations to create / destroy the database schema
* [esp.json](esp.json) - ESP configuration file
* [layouts](layouts) - Master view layout templates 
* [source](source) - Input source documents for the Expansive tool to render into "documents"

Documentation:
---
* [ESP Documentation](https://embedthis.com/esp/doc/index.html)
* [ESP Directives](https://embedthis.com/esp/doc/users/dir/esp.html)
* [ESP Tour](https://embedthis.com/esp/doc/users/tour.html)
* [ESP Controllers](https://embedthis.com/esp/doc/users/controllers.html)
* [ESP APIs](https://embedthis.com/esp/doc/ref/api/esp.html)
* [ESP Guide](https://embedthis.com/esp/doc/users/index.html)
* [ESP Overview](https://embedthis.com/esp/doc/users/using.html)

See Also:
---
* [controller - Creating ESP controllers](../controller/README.md)
* [page - Serving ESP pages](../page/README.md)
* [secure-server - Secure server](../secure-server/README.md)
* [simple-server - Simple server and embedding API](../simple-server/README.md)
* [typical-server - Fully featured server and embedding API](../typical-server/README.md)
