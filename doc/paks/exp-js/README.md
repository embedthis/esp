exp-js
===

Expansive plugin to manage Javascript files.

## Overview

The exp-js plugin provides build tooling for script files. It provides the **render-js** service to
manage the generation HTML for script files and provides the **minify-js** service to minify script files for release distributions.

## Installation

    pak install exp-js

### Services

Provides the following services:

* js
* minify-js
* render-js

## js

The **js** service provides configuration control for the other js services.

### Configuration

* compress &mdash; Enable compression of script files. Default to true.
* dotmin &mdash; Use '.min.js' as the output file extension after minification. Otherwise will be '.js'.  Default to true.
* enable &mdash; Enable the service. Default to true.
* force &mdash; Always minify even if a minified version exists in contents. Defaults to false.
* mappings &mdash; Set of extensions to transform. Defaults to:
```
mappings: {
    'js',
    'min.js',
    'min.map',
    'min.js.map'
}
```
* mangle &mdash; Enable mangling of Javascript variable and function names. Default to true.
* minify &mdash; Enable minifying of Javascript files. Will also generate map files if the render-js service defines 'usemap' to be true. Default to false.
* usemap &mdash; Use minified Javascript if corresponding source maps is present. Default to true.
* usemin &mdash; Use minified Javascript if present. Default to true.

## render-js

The render-js service smartly selects supplied Javascript files. By default, it selects minified scripts if a corresponding source map file with a 'min.map' extension is present. Otherwise, non-minified Javascript files with a plain 'js' extension will be selected.

The render-js service also provides the `renderScript` API which generates &lt;script&gt; elements for selected script files. The order of generated script elements will match the required order as specified by Pak dependencies.

```
    renderScripts()
```

## minify-js

The minify-js service optimizes script files by minifying to remove white-space, managle names and otherwise compress the scripts. By default, the script files use a '.js' extension, but will use a '.min.js' extension if the 'dotmin' option is enabled.

## Example

The `debug` collection will be selected if the package.json `pak.mode` is set to debug. Similarly for the `release` collection.

```
debug: {
    services: {
        "js": {
            usemap: true
        }
    }
}
release: {
    services: {
        "js": {
            minify: true
        }
    }
}
```

## Get Pak

[https://embedthis.com/pak/](https://embedthis.com/pak/)
