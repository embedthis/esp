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
* minify-js
* render-js

## render-js

The render-js service smartly selects supplied Javascript files. By default, it selects minified scripts if a corresponding source map file with a 'min.map' extension is present. Otherwise, non-minified Javascript files with a plain 'js' extension will be selected.

The render-js service also provides the `renderScript` API which generates &lt;script&gt; elements for selected script files. The order of generated script elements will match the required order as specified by Pak dependencies.

The renderScripts API may be invoked with an optional array of Path patterns to select a subset of scripts for which
to create script elements. This can be used to select or reject specific script files. A second argument may provide
extra script files to render.
```
    renderScripts(['!unwanted.js'], ['extra.js'])
```

### Configuration

* enable &mdash; Enable the service. Default to true.
* files &mdash; Array of files to process. Default so [ '**.js', '**.min.map', '**.min.js.map'] in
    the control.documents directories.
* mappings &mdash; Set of extensions to transform. Defaults to:
```
mappings: {
    'js',
    'min.js',
    'min.map',
    'min.js.map'
}
```
* usemap &mdash; Use minified Javascript if corresponding source maps is present. Default to true.
* usemin &mdash; Use minified Javascript if present. Default to null. Set explicitly to false
    to disable the use of minified resources.

## minify-js

The minify-js service optimizes script files by minifying to remove white-space, managle names and otherwise compress the scripts. By default, the script files use a '.js' extension, but will use a '.min.js' extension if the 'dotmin' option is enabled.

### Configuration

* compress &mdash; Enable compression of script files. Default to true.
* dotmin &mdash; Use '.min.js' as the output file extension after minification. Otherwise will be '.js'.  Default to true.
* enable &mdash; Enable minifying script files. Default to true.
* files &mdash; Array of files to minify. Files are relative to 'source'.
* genmap &mdash; Generate source map for minified scripts if 'minified' is true. Default to true.
* mangle &mdash; Enable mangling of Javascript variable and function names. Default to true.
* minify &mdash; Enable minifying of Javascript files. Default to false.

## Example

The `debug` collection will be selected if the package.json `pak.mode` is set to debug. Similarly for the `release` collection.

```
debug: {
    services: {
        "minify-js": {
            usemap: true
        }
    }
}
release: {
    services: {
        "minify-js": {
            minify: true
        }
    }
}
```

## Get Pak

[https://embedthis.com/pak/](https://embedthis.com/pak/download.html)
