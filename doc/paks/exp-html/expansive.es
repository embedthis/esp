Expansive.load({
    transforms: [ {
        name:       'html',
        mappings:   'html',
        options:    '--remove-comments --collapse-whitespace --prevent-attributes-escaping --remove-empty-attributes --remove-optional-tags'
        script: `
            function transform(contents, meta, service) {
                if (meta.isLayout && !meta.layout) {
                    let htmlmin = Cmd.locate('html-minifier')
                    if (!htmlmin) {
                        trace('Warn', 'Cannot find html-minifier')
                        return contents
                    }
                    try {
                        contents = run(htmlmin + ' ' + service.options, contents)
                    } catch (e) {
                        if (expansive.options.debug) {
                            print('Cannot minify', meta.source, '\n', e)
                            print('Contents', contents)
                        }
                        /* Keep going with unminified contents */
                    }
                }
                return contents
            }
        `
    }, {
        name: 'canonicalize-html',
        script: `
            public function renderCanonical() {
                if (meta.dest.basename.trimExt() == 'index') {
                    write('<link href="' + meta.abs.dirname + '/" rel="canonical" />')
                }
            }
        `
    } ]
})
