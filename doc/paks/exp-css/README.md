exp-css
===

Expansive plugin for CSS files.

## Overview

The exp-css plugin provides build tooling for stylesheets. It provides the **prefix-css** service to automatically insert browser specific CSS prefixes, the **render-css** service to manage the generation HTML for stylesheets, and the **minify-js** service to minify script files for release distributions.

## Installation

    pak install exp-css

## Services

Provides the following services
* minify-css
* prefix-css
* render-css

## prefix-css

The prefix-css service processes CSS files to automatically add browser specific prefixes to CSS rules. It requires the `autoprefixer` utility.

### Configuration

* enable &mdash; Enable the service. Defaults to true.

## render-css
The render-css service smartly selects supplied stylesheets. By default, it selects minified stylesheets if a corresponding source map file with a 'css.map' extension is present. Otherwise, non-minified stylesheets files with a plain 'css' extension  will be selected.

The renderStyles API may be invoked with an optional array of Path patterns to select a subset of stylesheets for which to create link elements. This can be used to select or reject specific stylesheets. A second argument may provide extra stylesheets to render.
```
    renderStyles(['!unwanted.css'], ['extra.css'])
```

### Configuration

* enable &mdash; Enable the service. Default to true.
* files &mdash; Array of files to process. Default so [ '**.css', '**.css.map'] in
    the control.documents directories.
* mappings &mdash; Set of extensions to transform. Defaults to:
```
mappings: {
    'css',
    'min.css',
    'css.map',
}
```
* usemap &mdash; Use minified stylesheet if corresponding source maps is present. Defaults to true.
* usemin &mdash; Use minified stylesheet if present. Defaults to null. Set explicitly to false
    to disable the use of minified resources.

## minify-css

The minify-css service optimizes stylesheets by minifying to remove white-space, managle names and otherwise compress the stylesheets. By default, the stylesheets use a '.css' extension, but will use a '.min.css' extension if the 'dotmin' option is enabled.

### Configuration

* dotmin &mdash; Use '.min.css' as the output file extension after minification. Otherwise will be
    '.css'.  Default to true.
* enable &mdash; Enable minifying stylesheets. Default to true.
* mappings &mdash; Set of extensions to transform. Defaults to:
```
mappings: {
    'css',
    'min.css',
    'css.map',
}
```
* minify &mdash; Enable minifying of Javascript files. Default to false.

## Example

The `debug` collection will be selected if the package.json `pak.mode` is set to debug. Similarly for the `release` collection.

```
debug: {
    services: {
        "minify-css": {
            usemap: true
        }
    }
}
release: {
    services: {
        "minify-css": {
            minify: true
        }
    }
}
```

## Get Pak

[https://embedthis.com/pak/](https://embedthis.com/pak/download.html)
