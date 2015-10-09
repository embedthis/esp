/*
    expansive.es - Configuration for exp-less

    Transform by less into css. Uses lessc|recess.
 */
Expansive.load({
    transforms: [{
        name:   'less',
        mappings: {
            less: [ 'css', 'less' ],
        },
        files: [ '**.less', '!**.css.less' ],
        stylesheets: [ 'css/all.css' ],
        dependencies: null,
        script: `
            let service = expansive.services.less
            //  DEPRECATE
            for each (svc in [ 'compile-less-css' ]) {
                let sp
                if ((sp = expansive.services[svc]) != null) {
                    for each (name in [ 'dependencies', 'files', 'mappings', 'stylesheets' ]) {
                        if (sp[name]) {
                            service[name] = sp[name] | service[name]
                            trace('Warn', 'Legacy configuration for ' + svc + '.' + name +
                                          ' in expansive.json. Migrate to less.' + name + '.')
                        }
                    }
                }
            }
            if (service.enable) {
                let control = expansive.control
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
                    blend(control.dependencies, service.dependencies)
                    let collections = expansive.control.collections
                    collections.styles ||= []
                    collections.styles.push(stylesheet)
                }
            }

            function resolve(path: Path, service): Path? {
                if (path.glob(service.files)) {
                    return null
                }
                return path
            }

            function transform(contents, meta, service) {
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
        `
    }]
})
