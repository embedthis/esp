layout -- ESP Layout Sample
===

This sample shows how to use Expansive layout pages with ESP pages.

The page to be served, source/index.esp provides the content while the
layout page: layouts/default.esp, provides the common look and feel.

Requirements
---
* [ESP](https://www.embedthis.com/esp/download.html)

To build:
---
    expansive render

To run:
---
    esp -v

or
    expansive

The server listens on port 4000. Browse to:

     http://localhost:4000/index.esp

Code:
---
* [index.esp](index.esp) - ESP page to serve. Uses layout.esp as a template.
* [layouts/layout.esp](index.esp) - ESP layout template

Documentation:
---
* [ESP Documentation](https://www.embedthis.com/esp/doc/index.html)
* [ESP APIs](https://www.embedthis.com/esp/doc/ref/api/esp.html)
* [ESP Guide](https://www.embedthis.com/esp/doc/users/index.html)
* [ESP Overview](https://www.embedthis.com/esp/doc/users/using.html)
* [Expansive](https://www.embedthis.com/expansive/)

See Also:
---
* [html-mvc - ESP MVC Application](../html-mvc/README.md)
* [controller - ESP Page](../controller/README.md)
* [page - ESP Page](../page/README.md)
