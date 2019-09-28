exp-less
===

Expansive plugin for Less files.

## Overview

The exp-less plugin provides build tooling for stylesheets. It provides the **less** service to compile less stylesheets into CSS stylesheets.

A map of file dependencies may be defined so that a master Less stylesheet will be rebuilt
by Expansive whenever an include stylesheet is updated.

## Installation

    pak install embedthis/exp-less

## Services

Provides the following services:

* less

## less

The less service processes LESS stylesheets using the **lessc** utility into CSS stylesheets.

By convention, exp-less expects master Less stylesheets to be named with a '.css.less' extension and included Less
files to use a simple '.less' extension.

### Configuration

* enable &mdash; Enable the less service to process less files. Defaults to true.
* files &mdash; Array of file patterns of stylesheets to compile. Defaults to [ '**.less', '!**.css.less' ] which
    selects all files with a '.less' extension and omits those with a '.css.less' extension.
* stylesheets &mdash; Stylesheets to update if any less file changes.
    If specified, the "dependencies" map will be automatically created. Defaults to 'css/all.css'.
* dependencies &mdash; Explicit map of dependencies if not using "stylesheet". They property name is the master LESS
    stylesheet and the value contains the ingredient less stylesheets that are included by the master.

```
{
    services: {
        'less': {
            enable: true,
            files: [ '**.less', '!**.css.less', '!css/unwanted.css' ],
            dependencies: {
                'css/all.css.less' : '**.less'
            },
            stylesheets: [ 'css/all.css' ],
        }
    }
}
```

### Get Pak from

[https://www.embedthis.com/pak/](https://www.embedthis.com/pak/)
