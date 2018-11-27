esp-html-skeleton
===

ESP Application skeleton for HTML MVC applications.

### Description

Provides a skeleton for HTML MVC applications using ESP, Expansive and Less stylesheets.

The **esp-html-skeleton** provides provides a environment for building HTML MVC applications and web sites. 
The skeleton includes the ESP default directory structure, templates for generating controllers, database migrations,
and scaffolds. The skeleton provides a default look and feel and includes a home page and Less-based stylesheet.

The skeleton provides configuration for a "debug" and "release" mode of operation via
the "pak.mode" property in package.json. By default, debug mode will use pre-minified
libraries if they have a symbol map file. Release mode will minify scripts as required.

### Provides

* contents/css &mdash; Application less stylesheets
* contents/index.esp &mdash; Default home page
* esp.json &mdash; ESP configuration file
* expansive.json &mdash; Expansive configuration file
* layouts/default.html.exp &mdash; Master web page layout 
* paks/ &mdash; Installed packages
* partials/ &mdash; Web page partial content

### Dependencies

The esp-html-skeleton package depends upon:

* [exp-css](https://github.com/embedthis/exp-css) to process CSS files
* [exp-less](https://github.com/embedthis/exp-less) to process Less files
* [exp-js](https://github.com/embedthis/exp-js) to process script files
* [exp-esp](https://github.com/embedthis/exp-esp) to compile ESP controllers and pages
* [esp-mvc](https://github.com/embedthis/esp-mvc) for ESP MVC application support.

### Installation

    pak install esp-html-skeleton

### Building

    expansive render

### Running

    expansive

or

    expansive render
    esp

### Generate Targets

To generate an appweb.conf configuration file for hosting the ESP application in Appweb.

    esp generate appweb

To generate a controller

    esp generate controller name [action [, action] ...

To generate a database table

    esp generate table name [field:type [, field:type] ...]

To generate a migration

    esp generate migration description model [field:type [, field:type] ...]

To generate a scaffold

    esp generate scaffold model [field:type [, field:type] ...]

### Configuration

#### esp.json

* esp.generate &mdash; Template files to use when using esp generate.
* http.auth.store &mdash; Store passwords in an application database.
* http.routes &mdash; Use a default package of RESTful routes.

```
{
    "esp": {
        "generate": {
            "appweb": "esp-html-skeleton/generate/appweb.conf",
            "clientList": "esp-html-skeleton/generate/list.esp",
            "clientEdit": "esp-html-skeleton/generate/edit.esp",
            "controller": "esp-html-skeleton/generate/controller.c",
            "controllerSingleton": "esp-html-skeleton/generate/controllerSingleton.c"
        }
    },
    "http": {
        "auth": {
            "store": "app"
        },
        "routes": "esp-restful"
    }
}
```

#### expansive.json

* less.enable &mdash; Enable the less service to process less files.
* less.stylesheet &mdash; Primary stylesheet to update if any less file changes.
    If specified, the "dependencies" map will be automatically created.
* less.dependencies &mdash; Explicit map of dependencies if not using "stylesheet".
* less.documents &mdash; Array of less files to compile.
* css.prefix &mdash; Enable running autoprefixer on CSS files to handle browser specific extensions.
* css.minify &mdash; Enable minifying CSS files.
* js.enable &mdash; Enable minifying script files.
* js.files &mdash; Array of files to minify. Files are relative to 'source'.
* js.compress &mdash; Enable compression of script files.
* js.mangle &mdash; Enable mangling of Javascript variable and function names.
* js.dotmin &mdash; Set '.min.js' as the output file extension after minification. Otherwise will be '.js'.

```
{
    services: {
        'less': {
            enable: true,
            stylesheet: 'css/all.css',
            dependencies: { 'css/all.css.less' : '**.less' },
            files: [ '!**.less', '**.css.less' ]
        },
        'css': {
            enable:     true,
        },
        'js': {
            enable:     true,
            files:      null,
            compress:   true,
            mangle:     true,
            dotmin:     false,
        }
    }
}
```

### Download

* [Pak](https://www.embedthis.com/pak/)
* [Expansive](https://www.embedthis.com/expansive/)
