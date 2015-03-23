exp-reload
===

Expansive plugin for live reloading the browser on content changes.

Provides the 'reload' service. To use, add the 'reload' partial to 
your layout. This will generate a reload script in 'debug' mode.

    <@ partial('reload') @>


### To install:

    pak install exp-reload

### To configure in expansive.json:

* reload.enable &mdash; Enable compressing all files using reload.

```
{
    services: {
        reload: {
            enable: true,
        }
    }
}
```

### Get Pak from

[https://embedthis.com/pak/](https://embedthis.com/pak/download.html)
