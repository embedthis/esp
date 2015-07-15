/*
    expansive.es - Configuration for exp-css

    Transform by prefixing and minifying. Uses autoprefixer and less|recess.
 */
Expansive.load({
    transforms: [{
        name:     'prefix-css',
        mappings: 'css',
        script: `
            function transform(contents, meta, service) {
                let autoprefixer = Cmd.locate('autoprefixer')
                if (autoprefixer) {
                    contents = run(autoprefixer, contents)
                } else {
                    trace('Warn', 'Cannot find autoprefixer')
                }
                return contents
            }
        `
    }, {
        name:     'render-css',
        files:    null,
        mappings: {
            'css',
            'min.css',
            'css.map'
        },
        usemap: true,
        usemin: null,

        script: `
            function initRenderStyles() {
                let directories = expansive.directories
                let service = expansive.services['render-css']
                let collections = expansive.control.collections
                /*
                    Build list of stylesheets to render. Must be in pak dependency order.
                    Permit explicit stylesheet override list in pak package.json pak.render.css
                 */
                let files
                if (service.files) {
                    files = expansive.directories.top.files(service.files, 
                        {contents: true, directories: false, relative: true})
                } else {
                    let list = expansive.directories.top.files(expansive.control.documents, 
                        {contents: true, directories: false, relative: true})
                    list = list.filter(function(path) path.glob(['**/*.css', '**.css.map']))
                    files = expansive.orderFiles(list, "css")
                }
                files = files.unique()

                /*
                    Save instructions for minify-css in the service.hash
                    Update the styles collection for use by renderStyles()
                 */
                let styles = []
                let minify = expansive.services['minify-css']
                service.hash = {}
                for each (file in files) {
                    let style = null
                    let ext = file.extension
                    if (ext == 'map') {
                        if (service.usemap) {
                            service.hash[file.name] = true
                        } else {
                            service.hash[file.name] = 'not required because "usemap" is false.'
                            expansive.skip(file.name)
                        }
                    } else if (file.endsWith('min.css')) {
                        if (service.usemin ||
                           (service.usemin !== false && service.usemap && 
                                file.trimEnd('.min.css').joinExt('css.map').exists)) {
                            service.hash[file.name] = true
                            style = file
                        } else {
                            service.hash[file.name] = 'not required because "usemin" is false.'
                            expansive.skip(file.name)
                        }
                    } else if (ext == 'css') {
                        let minified = file.replaceExt('min.css')
                        if (service.usemin && minified.exists) {
                            service.hash[file.name] = 'not required because ' + minified + ' exists.'
                            expansive.skip(file.name)
                        } else {
                            let mapped = file.replaceExt('min.map')
                            if (service.usemap && minified.exists &&
                                (file.replaceExt('min.map').exists || file.replaceExt('css.map').exists)) {
                                service.hash[file.name] = 'not required because ' + minified + ' exists.'
                                expansive.skip(file.name)
                            } else if (minify.minify) {
                                service.hash[file.name] = { minify: true }
                                style = file.trimExt().joinExt('min.css', true)
                            } else {
                                service.hash[file.name] = true
                                style = file
                            }
                        }
                    }
                    if (style) {
                        styles.push(style.trimStart(directories.lib + '/').trimStart(directories.contents + '/'))
                    }
                }
                collections.styles = styles + (collections.styles || [])
                if (expansive.options.debug) {
                    print("EXP_CSS", "usemap", service.usemap, "usemin", service.usemin, "minify", service.minify)
                    dump("EXP-CSS HASH", service.hash)
                    dump("EXP-CSS COLLECTIONS", collections)
                }
            }
            initRenderStyles()

            public function renderStyles(filter, extras = []) {
                let styles = (expansive.collections.styles || []) + extras
                for each (style in styles.unique()) {
                    if (filter && !Path(style).glob(filter)) {
                        continue
                    }
                    write('<link href="' + meta.top + '/' + style + '" rel="stylesheet" type="text/css" />\n    ')
                }
                if (expansive.collections['inline-styles']) {
                    write('<style>')
                    for each (style in expansive.collections['inline-styles']) {
                        write(style)
                    }
                    write('\n    </style>')
                }
            }
        `
    }, {
        name:     'minify-css',
        mappings: {
            'css',
            'min.css'
            'css.map'
        },
        minify: false,
        dotmin: true,

        script: `
            function transform(contents, meta, service) {
                let instructions = expansive.services['render-css'].hash[meta.source]
                if (!instructions) {
                    return contents
                }
                if (instructions is String) {
                    vtrace('Info', meta.path + ' ' + instructions)
                    return null
                }
                if (instructions.minify) {
                    let less = Cmd.locate('lessc')
                    if (less) {
                        contents = run(less + ' --compress - ', contents, meta)
                    } else {
                        /*
                            Can also use recess if lessc is not installed
                         */
                        let recess = Cmd.locate('recess')
                        if (recess) {
                            let results = runFile(recess + ' -compile -compress', contents, meta)
                            if (results == '') {
                                /* Failed, run again to get diagnostics - Ugh! */
                                results = runFile(recess, contents, meta)
                                throw 'Failed to parse less file with recess ' + meta.source + '\n' + results + '\n'
                            }
                            contents = results
                        } else {
                            trace('Warn', 'Cannot find lessc or recess')
                        }
                    }
                    if (service.dotmin && !meta.dest.contains('min.css')) {
                        meta.dest = meta.dest.trimExt().joinExt('min.css', true)
                    }
                }
                return contents
            }
        `
    }]
})
