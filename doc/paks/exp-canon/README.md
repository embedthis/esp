exp-canon
===

Expansive plugin to canonicalize index pages.

Provides the 'canon' service which creates canonical links for index pages to assist search engines by defining the "canonical" preferred link for an index page.

    &lt;link href="//" rel="canonical"&gt;

### To install:

    pak install embedthis/exp-canon

### To configure in expansive.json:

* canon.enable &mdash; Enable the html service to post-process HTML files.

```
{
    services: {
        'canon': {
            enable: true,
        }
    }
}
```

### Get Pak from

[https://embedthis.com/pak/](https://embedthis.com/pak/)
