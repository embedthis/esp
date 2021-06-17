exp-reload
===

Expansive plugin for live reloading the browser on content changes.

Provides the 'reload' service. To use, add the 'reload' partial to 
your layout. This will generate a reload script in 'debug' mode.
Ensure you include the reload partial before calling renderScripts().

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

[https://www.embedthis.com/pak/](https://www.embedthis.com/pak/)
