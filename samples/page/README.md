ESP Page Sample
===

This sample shows the features of ESP pages.

The ESP test page, index.esp, demonstrates the various ESP directives. These include:

* @@ to access request parameters/session variables
* @! to access local C variables
* <%= %> to emit the result of a C expression
* <%= %,d Number %> to use comma-separated number formatting
* <%= %S String %> to use safe-strings (HTML escape input)
* <%^start %> to emit code at the start of the generated page function
* <%^end %> to emit code at the end of the generated page function

Requirements
---
* [ESP](https://embedthis.com/esp/download.html)

To run:
---
    esp

The server listens on port 4000. Browse to: 
 
     http://localhost:4000/index.esp

Code:
---
* [index.esp](index.esp) - ESP page to serve

Documentation:
---
* [ESP Documentation](https://embedthis.com/esp/doc/index.html)
* [ESP APIs](https://embedthis.com/esp/doc/ref/api/esp.html)
* [ESP Guide](https://embedthis.com/esp/doc/users/index.html)
* [ESP Overview](https://embedthis.com/esp/doc/users/using.html)

See Also:
---
* [controller - ESP Controller](../controller/README.md)
* [html-mvc - ESP HTML MMVC](../html-mvc/README.md)
* [layout - ESP Layouts](../layout/README.md)
