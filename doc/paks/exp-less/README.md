exp-less
===

Expansive plugin for Less files.

## Overview

The exp-less plugin provides build tooling for stylesheets. It provides the **compile-less-css** service to compile less stylesheets into CSS stylesheets, and the **clean-css** service to remove unwanted intermediate stylesheets.

A map of file dependencies may be defined so that a master Less stylesheet will be rebuilt
by Expansive whenever an include stylesheet is updated. 

## Installation

    pak install exp-less

## Services

Provides the following services:

* compile-less-css
* clean-css

## compile-less-css

The compile-less-css service processes LESS stylesheets using the **lessc** utility into CSS stylesheets.

By convention, exp-less expects master Less stylesheets to be named with a '.css.less' extension and included Less 
files to use a simple '.less' extension. 

### Configuration

* enable &mdash; Enable the compile-less-css service to process less files.
* stylesheets &mdash; Stylesheets to update if any less file changes.
    If specified, the "dependencies" map will be automatically created.
* dependencies &mdash; Explicit map of dependencies if not using "stylesheet". They property name is the master LESS
    stylesheet and the value contains the ingredient less stylesheets that are included by the master. 

## clean-css

The clean-css service may be used to remove unwanted CSS files from the **dist** directory. By default, the clean-css service is disabled. 

### Configuration

* enable &mdash; Enable the compile-less-css service to process less files. Defaults to false.
* files &mdash; Array of glob file patterns for CSS files to remove.

```
{
    services: {
        'compile-less-css': {
            enable: true,
            stylesheets: [ 'css/all.css' ],
            dependencies: { 
                'css/all.css.less' : '**.less' 
            },
        },
        'clean-css': {
            files: [ 'css/unwanted.css', '!**important.css] ]
        }
    }
}
```

### Get Pak from

[https://embedthis.com/pak/](https://embedthis.com/pak/download.html)
