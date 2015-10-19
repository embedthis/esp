/*
    expansive.es - Configuration for exp-less

    Transform by less into css. Uses lessc|recess.
 */
Expansive.load({
    services: {
        name:   'less',
        files: [ '**.less', '!**.css.less' ],
        stylesheets: [ 'css/all.css' ],
        dependencies: null,

        transforms: {
            mappings: {
                less: [ 'css', 'less' ],
            },
            init: function(transform) { 
                let service = transform.service
                if (!(service.stylesheets is Array)) {
                    service.stylesheets = [service.stylesheets]
                }
                for each (stylesheet in service.stylesheets) {
                    if (expansive.directories.contents.join(stylesheet + '.less').exists) {
                        if (!service.dependencies) {
                            service.dependencies ||= {}
                            service.dependencies[stylesheet + '.less'] = '**.less'
                        }
                    }
                    blend(expansive.control.dependencies, service.dependencies)
                    let collections = expansive.control.collections
                    collections.styles ||= []
                    collections.styles.push(stylesheet)
                }
            },

            resolve: function(path: Path, transform): Path? {
                if (path.glob(transform.service.files)) {
                    return null
                }
                return path
            },

            render: function(contents, meta) {
                let less = Cmd.locate('lessc')
                if (less) {
                    contents = Cmd.run(less + ' - ', {dir: meta.source.dirname}, contents)
                } else {
                    /*
                        Can also use recess if lessc not installed
                     */
                    let recess = Cmd.locate('recess')
                    if (recess) {
                        let results = runFile(recess + ' -compile', contents, meta)
                        if (results == '') {
                            /* Failed, run again to get diagnostics - Ugh! */
                            let errors = runFile(recess, contents, meta)
                            throw 'Failed to parse less sheet ' + meta.source + '\n' + errors + '\n'
                        }
                        contents = results
                    } else {
                        trace('Warn', 'Cannot find lessc or recess')
                    }
                }
                return contents
            }
        }
    }
})
