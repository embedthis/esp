/*
    expansive.es - Configuration for exp-css

    Transform by prefixing and minifying. Uses autoprefixer and less|recess.
 */
Expansive.load({
    transforms: [{
        name:     'css',
        mappings: {
            'css',
            'min.css'
            'css.map'
        },
        dotmin:     true,
        force:      false,
        minify:     false,
        usemap:     true,
        usemin:     true,

        script: `
            let service = expansive.services['css']
            for each (svc in [ 'prefix-css', 'render-css', 'minify-css' ]) {
                let sp
                if ((sp = expansive.services[svc]) != null) {
                    for each (name in [ 'dotmin', 'minify', 'usemap', 'usemin' ]) {
                        if (sp[name]) {
                            service[name] = sp[name] | service[name]
                            trace('Warn', 'Legacy configuration for ' + svc + '.' + name + 
                                          ' in expansive.json. Migrate to css.' + name + '.')
                        }
                    }
                }
            }
            if (service.minify) {
                service.usemin ||= true
            }
            if (service.usemap) {
                service.usemin ||= true
            }

            function resolve(path: Path, service): Path? {
                let vfile = directories.contents.join(path)
                if (vfile.endsWith('.min.css')) {
                    let base = vfile.trimEnd('.min.css')
                    if (service.usemin && !(service.minify && service.force && base.joinExt('css', true).exists)) {
                        if (service.usemap) {
                            if (base.joinExt('css.map', true).exists) {
                                return path
                            }
                        } else {
                            return path
                        }
                    }
                } else if (vfile.endsWith('.css.map')) {
                    if (service.usemin) {
                        let base = vfile.trimEnd('.css.map')
                        if (service.usemap && !(service.minify && service.force && base.joinExt('css', true).exists)) {
                            if (base.joinExt('min.css', true).exists) {
                                return path
                            }
                        }
                    }
                } else {
                    let minified = vfile.replaceExt('min.css')
                    /*
                        Minify if forced, or a suitable minfied version does not exist or !usemin
                     */                           
                    if ((service.minify && service.force) ||
                        !(minified.exists && service.usemin && (!service.usemap || vfile.replaceExt('css.map').exists))) {
                        if (service.minify && service.dotmin) {
                            return path.trimExt().joinExt('min.css', true)
                        }
                        return path
                    }
                }
                return null
            }
        `

    }, {
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
        name:     'minify-css',
        mappings: 'css',
        script: `
            if (!expansive.services.css.minify) {
                expansive.services['minify-css'].enable = false
            }
            function transform(contents, meta, service) {
                trace('Minify', meta.source)
                let less = Cmd.locate('lessc')
                if (less) {
                    contents = expansive.run(less + ' --compress - ', contents, meta)
                } else {
                    /*
                        Can also use recess if lessc is not installed
                     */
                    let recess = Cmd.locate('recess')
                    if (recess) {
                        let results = expansive.runFile(recess + ' -compile -compress', contents, meta)
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
                return contents
            }
        `
   
    }, {
        name:     'render-css',
        /*
            Default to styles from packages under contents/lib
            Required styles may vary on a page by page basis.
         */
        files:    [ '**.css*', '!**.map', '!*.less*' ]
        mappings: {
            'css',
            'min.css'
        },
        script: `
            let service = expansive.services['render-css']
            if (service.files) {
                if (!(service.files is Array)) {
                    service.files = [ service.files ]
                }
                expansive.control.collections.styles += service.files
            }
            service.hash = {}

            function pre(meta, service) {
                if (expansive.modified.everything) {
                    service.hash = {}
                }
            }

            function buildStyleList(files: Array): Array {
                let directories = expansive.directories
                let service = expansive.services.css
                let styles = []
                for each (style in files) {
                    if ((style = expansive.getDestPath(style)) == null) {
                        continue
                    }
                    let vfile = directories.contents.join(style)
                    let base = vfile.trimEnd('.min.css').trimEnd('.css')

                    let map = base.joinExt('min.map', true).exists || base.joinExt('css.map', true).exists || 
                        base.joinExt('.min.css.map', true).exists
                    if (vfile.endsWith('min.css')) {
                        if (service.usemin && (!service.usemap || map)) {
                            styles.push(style)
                        }
                    } else {
                        let minified = vfile.replaceExt('min.css').exists
                        let map = vfile.replaceExt('min.map').exists || vfile.replaceExt('css.map').exists || 
                            vfile.replaceExt('.min.css.map').exists
                        if ((service.minify && service.force) || !minified || !(service.usemap && map)) {
                            if (service.minify && service.dotmin) {
                                styles.push(style.replaceExt('min.css'))
                            } else {
                                styles.push(style)
                            }
                        }
                    }
                }
                return styles
            }

            /*
                Render styles is based on 'collections.styles' which defaults to '**.css' and is modified
                via expansive.json and addItems.
             */
            public function renderStyles(filter = null, extras = []) {
                let collections = expansive.collections
                if (!collections.styles) {
                    return
                }
                let directories = expansive.directories
                let service = expansive.services['render-css']

                /*
                    Pages have different stylesheets and so must compute style list per page.
                    This is hased and saved.
                 */
                if (!service.hash[collections.styles]) {
                    let files = directories.contents.files(collections.styles, 
                        { contents: true, directories: false, relative: true})
                    files = expansive.orderFiles(files, "css")
                    service.hash[collections.styles] = buildStyleList(files).unique()
                }
                for each (style in service.hash[collections.styles]) {
                    if (filter && !Path(script).glob(filter)) {
                        continue
                    }
                    let uri = meta.top.join(style).trimStart('./')
                    write('<link href="' + uri + '" rel="stylesheet" type="text/css" />\n    ')
                }
                if (extras && extras is String) {
                    extras = [extras]
                }
                for each (style in extras) {
                    let uri = meta.top.join(style).trimStart('./')
                    write('<link href="' + uri + '" rel="stylesheet" type="text/css" />\n    ')
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
    } ]
})
