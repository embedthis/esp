ESP Many Sample
===

This sample shows how to load many ESP applications in one ESP instance.

Requirements
---
* [ESP](https://embedthis.com/esp/download.html)

To run:
---
    esp -s

This will display the routing table to the console and then listen on port 4000.
Browse to: 
 
     http://localhost:4000/test/hello

This then returns "Hello World" to the client.

If you modify the service.c it will be automatically recompiled and reloaded when 
next accessed.

Code:
---
* [service.c](service.c) - ESP controller source

Documentation:
---
* [ESP Documentation](https://embedthis.com/esp/doc/index.html)
* [ESP Tour](https://embedthis.com/esp/doc/start/tour.html)
* [ESP Controllers](https://embedthis.com/esp/doc/users/controllers.html)
* [ESP APIs](https://embedthis.com/esp/doc/ref/api/esp.html)
* [ESP Guide](https://embedthis.com/esp/doc/users/index.html)
* [ESP Overview](https://embedthis.com/esp/doc/users/using.html)

See Also:
---
* [html-mvc - ESP HTML MMVC](../html-mvc/README.md)
* [layout - ESP Layouts](../layout/README.md)
* [page - Serving ESP pages](../page/README.md)
