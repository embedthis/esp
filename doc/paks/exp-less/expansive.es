/*
    expansive.es - Configuration for exp-less

    Transform by less into css. Uses lessc|recess.
 */
Expansive.load({
    transforms: [{
        name:   'compile-less-css',
        mappings: {
            less: [ 'css', 'less' ],
        }
        stylesheets: ['css/all.css'],
        dependencies: null,
        script: `
            let service = expansive.services['compile-less-css']
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
                for each (file in expansive.directories.contents.files(['**.less', '!**.css.less'])) {
                    expansive.skip(file)
                }
            }

            function transform(contents, meta, service) {
                if (!meta.source.glob('**.css.less')) {
                    vtrace('Info', 'Skip included css file', meta.source)
                    expansive.skip(meta.source)
                    return null
                }
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
    }, {
        /*
            Remove unwanted css files
            Uses configuration from compile-less-css (stylesheet)
            Disabled by default
         */
        files:    [ '**.css' ],
        name:     'clean-css',
        mappings: 'css',
        enable:   false,
        script: `
            function transform(contents, meta, service) {
                let lservice = expansive.services['compile-less-css']
                for each (stylesheet in lservice.stylesheets) {
                    if (stylesheet != meta.path && meta.dest.glob(service.files)) {
                        trace('Clean', meta.dest)
                        contents = null
                        break
                    }
                }
                return contents
            }
        `
    }]
})
