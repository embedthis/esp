Embedthis Multithreaded Portable Runtime (MPR) 4.X
===

Licensing
---
See LICENSE.md for details.

### To Read Documentation:

  See doc/index.html

### Prerequisites:
    MakeMe (https://embedthis.com/makeme/download.html) for MakeMe to configure and build.
    Ejscript (https://embedthis.com/ejscript/download.html) for utest to test.

### To Build:

    ./configure
    me

Alternatively to build without MakeMe:

    make

Images are built into */bin. The build configuration is saved in */inc/me.h.

### To Test:

    me test

### To Run:

    me run

This will run appweb in the src/server directory using the src/server/appweb.conf configuration file.

### To Install:

    me install

### To Create Packages:

    me package

Resources
---
  - [Embedthis web site](https://embedthis.com/)
  - [MPR GitHub repository](http://github.com/embedthis/mpr-4)
  - [MakeMe GitHub repository](http://github.com/embedthis/makeme)
