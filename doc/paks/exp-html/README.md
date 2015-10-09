exp-html
===

Expansive plugin for the HTML minifier.

Provides the 'html' service to minify HTML files.

### To install:

    pak install exp-html

### To configure in expansive.json:

* html.enable &mdash; Enable the html service to post-process HTML files.
* html.options &mdash; Command line options to html-minifier. Default options are:
    --remove-comments --collapse-whitespace --prevent-attributes-escaping --remove-empty-attributes --remove-optional-tags

```
{
    services: {
        'html': {
            enable: true,
            options: '--remove-comments \
                      --conservative-collapse \
                      --prevent-attributes-escaping \
                      --remove-empty-attributes \
                      --remove-optional-tags'
        }
    }
}
```

### Get Pak from

[https://embedthis.com/pak/](https://embedthis.com/pak/)
