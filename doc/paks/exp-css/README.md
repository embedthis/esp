exp-css
===

Expansive plugin for CSS files.

## Overview

The exp-css plugin provides build tooling for stylesheets. It provides the **prefix-css** service to automatically insert browser specific CSS prefixes, the **render-css** service to manage the generation HTML for stylesheets, and the **minify-js** service to minify script files for release distributions.

## Installation

    pak install exp-css

## Services

Provides the following services

* css
* prefix-css
* minify-css
* render-css
* extract-css

## css

The **css** service provides configuration control for the other css services.

### Configuration

* dotmin &mdash; Use '.min.css' as the output file extension after minification. Otherwise will be
    '.css'.  Default to true.
* enable &mdash; Enable the service. Defaults to true.
* extract &mdash; Extract inline syltes into external stylesheet. Defaults to to false. Set to true to enable or set
    to a filename to contain all extracted styles.
* force &mdash; Force minification even if a minified source file exists.
* minify &mdash; Enable minifying of Javascript files. Default to false.
* usemap &mdash; Use minified stylesheet if corresponding source maps is present. Defaults to true.
* usemin &mdash; Use minified stylesheet if present. Defaults to null. Set explicitly to false
    to disable the use of minified resources.

## prefix-css

The prefix-css service processes CSS files to automatically add browser specific prefixes to CSS rules. It requires the `autoprefixer` utility.

## render-css

The render-css service smartly selects supplied stylesheets. By default, it selects minified stylesheets if a corresponding source map file with a 'css.map' extension is present. Otherwise, non-minified stylesheets files with a plain 'css' extension  will be selected.

### Configuration

* files &mdash; List of stylesheets to render. Defaults to [ '**.css*', '!**.map', '!*.less*' ]

The renderStyles API will generate the HTML for the page to include the specified stylesheets. The styles are taken from the current 'styles' collection for the page. Use 'addItems' and 'removeItems' to modify the styles collection.

The renderStyles API may be invoked with an argument can specify a set of patterns to select a subset of stylesheets for which to create link elements. This can be used to select or reject specific stylesheets. A second argument can supply an array of additional stylesheets to render.

```
    renderStyles(['!unwanted.css'], ['extra.css'])
```

## minify-css

The minify-css service optimizes stylesheets by minifying to remove white-space, managle names and otherwise compress the stylesheets. By default, the stylesheets use a '.css' extension, but will use a '.min.css' extension if the 'dotmin' option is enabled.

## extract-css

To support Content Security Policy headers, the extract-css service extracts inline styles into external Stylesheets. It will extract incline \<style> elements and style attributes into a per-page external stylesheet file. If the **extract** attribute is set to a filename, then all the styles will be placed in that file.

## Example

The `debug` collection will be selected if the package.json `pak.mode` is set to debug. Similarly for the `release` collection.

```
debug: {
    services: {
        "css": {
            usemap: true
        }
    }
}
release: {
    services: {
        "css": {
            minify: true
        }
    }
}
```

## Get Pak

[https://embedthis.com/pak/](https://embedthis.com/pak/)
