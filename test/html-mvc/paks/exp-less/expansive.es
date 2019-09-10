/*
    expansive.es - Configuration for exp-less

    Transform by less into css. Uses lessc
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
                    throw new Error('Cannot find lessc')
                }
                return contents
            }
        }
    }
})
