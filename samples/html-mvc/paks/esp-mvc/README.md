esp-mvc
===

ESP MVC Application package.

#### Description

Provides MVC support for ESP applications. The package includes the ESP default
directory structure, templates for generating controllers, and database migrations.
This package provides default configuration files for ESP and Expansive.

The package provides configuration for a "debug" and "release" mode of operation via
the "pak.mode" property in package.json. By default, debug mode will use pre-minified
libraries if they have a symbol map file. Release mode will minify scripts as required.


#### Provides

* esp.json &mdash; ESP configuration file
* expansive.json &mdash; Expansive configuration file
* contents/ &mdash; Directory for input web page contents
* layouts/ &mdash; Directory for Expansive master page layouts
* partials/ &mdash; Directory ofr partial pages

#### Dependencies

The esp-mvc package depends upon:

* [exp-css](https://github.com/embedthis/exp-css) to process CSS files
* [exp-less](https://github.com/embedthis/exp-less) to process Less files
* [exp-js](https://github.com/embedthis/exp-js) to process script files
* [exp-esp](https://github.com/embedthis/exp-esp) to compile ESP controllers and pages    

### Installation

    pak install embedthis/esp-mvc

### Building

    expansive render

### Running

    expansive

or

    expansive render
    esp

### Deploy

    expansive deploy

    This follows the instructions in control.deploy in expansive.json.

#### Generate Targets

To generate an appweb.conf Appweb configuration file for hosting the ESP application in Appweb.

    esp generate appweb

To generate a controller

    esp generate controller name [action [, action] ...

To generate a database table

    esp generate table name [field:type [, field:type] ...]

To generate a migration

    esp generate migration description model [field:type [, field:type] ...]

### Configuration

#### esp.json

* esp.generate &mdash; Template files to use when using esp generate.
* http.auth.store &mdash; Store passwords in an application database.
* http.routes &mdash; Use a default package of RESTful routes.

```
{
    "esp": {
        "generate": {
            "appweb": "esp-mvc/generate/appweb.conf",
            "controller": "esp-mvc/generate/controller.c",
            "controllerSingleton": "esp-mvc/generate/controller.c",
            "migration": "esp-mvc/generate/migration.c",
            "module": "esp-mvc/generate/src/app.c"
        }
    },
    "http": {
        "auth": {
            "store": "app"
        },
        "database": "default",
        "routes": "esp-restful"
    }
}
```

#### expansive.json

* less.enable &mdash; Enable the less service to process less files.
* less.dependencies &mdash; Explicit map of dependencies if not using "stylesheet".
* less.files &mdash; Array of less files to compile.
* less.stylesheet &mdash; Primary stylesheet to update if any less file changes.
    If specified, the "dependencies" map will be automatically created.
* css.enable &mdash; Enable minifying CSS files.
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
            enable: true,
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
