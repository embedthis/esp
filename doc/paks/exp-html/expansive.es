Expansive.load({

    services: {
        name:       'html',
        _options:   '--remove-comments',
        options:    '--collapse-whitespace --prevent-attributes-escaping --remove-empty-attributes --remove-optional-tags'

        transforms: {
            mappings:   ['html'],

            init: function(transform) {
                transform.htmlmin = Cmd.locate('html-minifier')
                if (!transform.htmlmin) {
                    throw new Error('Cannot find html-minifier')
                }
            },

            render: function(contents, meta, transform) {
                /*
                    Only minify the final aggregation of document, partials and layout
                 */
                if (meta.isLayout && !meta.layout && transform.htmlmin) {
                    try {
                        contents = run(transform.htmlmin + ' ' + transform.service.options, contents)
                        contents += '\n'
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
        }
    }
})
