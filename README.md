Embedthis ESP
===

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

Branches
---
The repository has several branches:

* master - Most recent release of the software.
* dev - Current ongoing development.
* X.X - Archived prior release branches for maintenance.

Licensing
---
See [LICENSE.md](LICENSE.md) for details.

### Documentation

  See https://www.embedthis.com/esp/doc/index.html.

### Building from Source

You can build ESP with make, Visual Studio, Xcode or [MakeMe](https://www.embedthis.com/makeme/).

The IDE projects and Makefiles will build with [ESP](https://www.embedthis.com/esp/) and SSL using the [MbedTLS](https://github.com/ARMmbed/mbedtls) TLS stack. To build with CGI, OpenSSL or other modules, read the [projects/README.md](projects/README.md) for details.

### To Build with Make:

#### Linux or MacOS

    make

or to see the commands as they are invoked:

    make SHOW=1

You can pass make variables to tailor the build. For a list of variables:

	make help

To run

	make run

#### Windows

First open a Windows cmd prompt window and then set your Visual Studio environment variables by running vcvarsall.bat from your Visual Studio installation folder.

Then run a Windows cmd prompt window and type:

    make

### To Build with Visual Studio:

Open the solution file at:

    projects/esp-windows-default.sln

Then select Build -> Solution.

To run the debugger, right-click on the "esp" project and set it as the startup project. Then modify the project properties and set the Debugging configuration properties. Set the working directory to be:

    $(ProjectDir)\..\..\test

Set the arguments to be
    -v

Then start debugging.

### To Build with Xcode.

Open the solution file:

    projects/esp-macosx-default.sln

Choose Product -> Scheme -> Edit Scheme, and select "Build" on the left of the dialog. Click the "+" symbol at the bottom in the center and then select all targets to be built. Before leaving this dialog, set the debugger options by selecting "Run/Debug" on the left hand side. Under "Info" set the Executable to be "esp", set the launch arguments to be "-v" and set the working directory to be an absolute path to the "./test" directory in the esp source. The click "Close" to save.

Click Project -> Build to build.

Click Project -> Run to run.

### To build with MakeMe:

To install MakeMe, download it from https://www.embedthis.com/makeme/.

    ./configure
    me

For a list of configure options:

	./configure --help

### To install:

If you have built from source using Make or MakeMe, you can install the software using:

    sudo make install

or

    sudo me install

### To uninstall

    sudo make uninstall

or

    sudo me uninstall

### To Test:

Build with MakeMe and then:

    me test

Resources
---
  - [ESP web site](https://www.embedthis.com/esp/)
  - [ESP GitHub repository](http://github.com/embedthis/esp)
  - [Embedthis web site](https://www.embedthis.com/)
