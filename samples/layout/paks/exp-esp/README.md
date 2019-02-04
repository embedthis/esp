exp-esp
===

Expansive plugin for ESP files. This plugin will compile ESP pages into loadable libraries.

Provides the 'esp' service.

### To install:

    pak install exp-esp

### To configure in expansive.json:

* clean &mdash; Command to use to clean the cache. Defaults to 'esp clean'. If using Appweb, set to 'appweb-esp clean'.
* compile &mdash; Command to use to compile the files. Defaults to 'esp compile'. If using Appweb, set to 'appweb-esp compile'.
* enable &mdash; Set to true to enable the compilation of ESP files. Defaults to true.
* esp &mdash; Set to the path to the esp command. Appweb users may need to set this to 'appesp'.
* keep &mdash; Keep the dist/\**.esp files after compilation. Note: this means you cannot do stand-alone 
    'esp compile' as the files will not be present to compile. You must do 'expansive render'. Defaults to true.
* mappings &mdash; File extensions to process. Defaults to: [ 'esp' ].
* serve &mdash; ESP command line to invoke esp to serve client browser requests. Defaults to 'esp --trace stdout:4 LISTEN'
    where LISTEN is replaced with the listen port configured in the esp.json or expansive.json files.

```
{
    services: {
        'esp': {
            clean: 'appweb-esp clean',
            compile: 'appweb-esp compile',
            keep: true,
            serve: 'appweb --log stdout:4 --trace stdout:4 LISTEN',
        }
    }
}
```

### Get Pak from

[https://www.embedthis.com/pak/](https://www.embedthis.com/pak/)

