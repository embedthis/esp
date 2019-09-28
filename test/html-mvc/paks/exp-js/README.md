exp-js
===

Expansive plugin to manage Javascript files.

## Overview

The exp-js plugin provides build tooling for script files. It provides the **js-render** service to manage the generation HTML for script files, the **js-minify** service to minify script files for release distributions, and the **js-extract** service to extract inline scripts.

## Installation

    pak install embedthis/exp-js

### Services

Provides the following transformations:

* js
* js-minify
* js-render
* js-extract

## js

The **js** transform provides configuration control for the other js services.

### Configuration

* compress &mdash; Enable compression of script files. Default to true.
* dotmin &mdash; Use '.min.js' as the output file extension after minification. Otherwise will be '.js'.  Default to true.
* enable &mdash; Enable the service. Default to true.
* extract &mdash; Extract inline scripts into external script files. Defaults to to false. Set to true to enable or set
    to a filename to contain all extracted scripts.
* files &mdash; List of scripts to render. Defaults to [ 'lib/\*\*.js*, '!lib/\*\*.map' ]
* mappings &mdash; Set of extensions to transform. Defaults to: mappings: [ 'js', 'min.js', 'min.map', 'min.js.map' ]
* minify &mdash; Enable minifying Javascript files. Will also generate map files if the js-render transform defines 'usemap' to be true. Default to false.
* options &mdash; Uglifyjs command options to use when minifying Javascript files. Defaults to:
--compress dead_code=true,conditionals=true,booleans=true,unused=true,if_return=true,join_vars=true,drop_console=true --mangle
* usemap &mdash; Use minified Javascript if corresponding source maps is present. Default to true.
* usemin &mdash; Use minified Javascript if present. Default to true.

## js-render

The js-render transform intelligently selects minified or non-minified Javascript files. By default, it selects minified scripts if a corresponding source map file with a 'min.map' extension is present. Otherwise, non-minified Javascript files with a plain .js extension will be selected.

The js-render transform also provides the `renderScript` API which generates &lt;script&gt; elements for selected script files. The order of generated script elements will match the required order as specified by Pak dependencies.

The renderScripts API may be invoked with an argument can specify a set of patterns to select a subset of scripts for which to create script elements. This can be used to select or reject specific scripts. A second argument may specify an array of additional scripts to render.

```
    renderScripts(['!unwanted.js'], ['extra.js'])
```

## js-minify

The js-minify transform optimizes script files by minifying to remove white-space, managle names and otherwise compress the scripts. By default, the script files use a '.js' extension, but will use a '.min.js' extension if the 'dotmin' option is enabled.


## js-extract

To support using Content Security Policy headers, the js-extract transform extracts inline scripts into external Javascript files. It will extract incline \<script> tags and onclick attributes into a per-page external script file. If the **extract** attribute is set to a filename, then all the scripts will be placed in that file.

## Example

The `debug` collection will be selected if the pak.json `profile` is set to debug. Similarly for the `release` collection.

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

[https://www.embedthis.com/pak/](https://www.embedthis.com/pak/)
