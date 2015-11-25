# Embedthis ESP

<img align="right" src="https://embedthis.com/images/pak.png">

ESP is a light-weight web framework that makes it easy to create blazing fast, dynamic web applications. 
ESP uses the "C" language for server-side web programming which allows easy access to low-level data for
management user interfaces.

However, ESP is not a traditional low-level environment. If web pages or controllers are modified during development, the
code is transparently recompiled and reloaded. ESP uses a garbage-collected environment memory management and for safe
programming. This enables unparalleled performance with "script-like" flexibility for web applications. environment and
blazing runtime speed.

The ESP web framework provides a complete set of components including: an application generator, web request handler,
templating engine, Model-View-Controller framework, Web Sockets, database migrations and an extensive programming API.
This document describes the ESP web framework and how to use ESP. Note that ESP is integrated into Appweb and is not a
separate product.

Licensing
---
See [LICENSE.md](LICENSE.md) for details.

### To read documentation:

  See https://embedthis.com/esp/doc/

### Building
    You can build with make or with MakeMe. MakeMe is quicker and more flexible.
    To install MakeMe, download it from https://embedthis.com/makeme/

### To build with make:

    make

You can pass make variables to tailor the build. For a list of variables:

	make help

### To build with MakeMe:

    ./configure
    me

For a list of configure options:

	./configure --help

### To run

	make run

or

    me run

### To install:

    sudo make install

or 

    sudo me install

### To uninstall

    sudo make uninstall

or 

    sudo me uninstall

### To test:

    me test

Resources
---
  - [ESP web site](https://embedthis.com/esp/)
  - [ESP GitHub repository](https://github.com/embedthis/esp)
  - [Embedthis web site](https://embedthis.com/)
