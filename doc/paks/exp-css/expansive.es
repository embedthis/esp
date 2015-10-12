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
        files:      [ '**.css*', '!**.map', '!*.less*' ],
        force:      false,
        extract:    false,
        minify:     false,
        usemap:     true,
        usemin:     true,

        script: `
            let service = expansive.services.css
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
            if (!service.extract) {
                expansive.services['extract-css'].enable = false
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
                        Use this source file if forced+miify, or a suitable minfied version does not exist or !usemin
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
        mappings: {
            'css',
            'min.css'
        },
        script: `
            let service = expansive.services.css
            if (service.files) {
                if (!(service.files is Array)) {
                    service.files = [ service.files ]
                }
                expansive.control.collections.styles += service.files
            }
            expansive.services['render-css'].hash = {}

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
                if (expansive.services['extract-css'].states) {
                    let extracted = expansive.services['extract-css'].states[meta.destPath]
                    if (extracted && extracted.link) {
                        extras.push(expansive.services.css.resolve(extracted.link, expansive.services.css))
                    }
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
    }, {
        name:       'extract-css',
        mappings:   'html',

        script: `
            let service = expansive.services['extract-css']
            service.nextId = 0
            service.states = {}

            /*
                Local function to handle inline style elements
             */
            function handleStyleElements(contents, meta, state): String {
                let service = expansive.services['extract-css']
                let re = /<style[^>]*>(.*)<\\/style>/smg
                let start = 0, end = 0
                let output = ''
                let matches
                while (matches = re.exec(contents, start)) {
                    let elt = matches[0]
                    end = re.lastIndex - elt.length
                    output += contents.slice(start, end)
                    state.elements ||= []
                    state.elements.push(matches[1].trimEnd(';'))
                    start = re.lastIndex
                }
                output += contents.slice(start)
                return output
            }

            /*
                Local function to handle style attributes 
            */
            function handleStyleAttributes(contents, meta, state): String {
                let result = ''
                let re = /<([\\w_\\-:.]+)([^>]*>)/g
                let start = 0, end = 0
                let matches

                while (matches = re.exec(contents, start)) {
                    let elt = matches[0]
                    end = re.lastIndex - matches[0].length
                    /* Emit prior contents */
                    result += contents.slice(start, end)

                    let attre = /(.*) +(style=)(["'])(.*?)(\\3)(.*)/m
                    if ((cmatches = attre.exec(elt)) != null) {
                        elt = cmatches[1] + cmatches[6]
                        let id
                        if ((ematches = elt.match(/id=['"]([^'"]+)['"]/)) != null) {
                            id = '#' + ematches[1]
                        } else {
                            let service = expansive.services['extract-css']
                            id = 'exp-' + ++service.nextId
                        }
                        state.attributes ||= {}
                        state.attributes[id] = cmatches[4].trimEnd(';')
                        if (!id.startsWith('#')) {
                            let classre = /(.*) +(class=)(["'])([^'"]+)(\\3)(.*)/m
                            if ((classes = classre.exec(elt)) != null) {
                                elt = classes[1] + ' ' + classes[2] + '"' + classes[4] + ' ' + id + '"' + classes[6]
                            } else {
                                elt = cmatches[1] + ' class="' + id + '" ' + cmatches[6]
                            }
                        } else {
                            elt = cmatches[1] + cmatches[6]
                        }
                    }
                    result += elt
                    start = re.lastIndex
                }
                return result + contents.slice(start)
            }

            function transform(contents, meta, service) {
                if (!expansive.services.css.extract) {
                    return contents
                }
                let state = {}
                contents = handleStyleElements(contents, meta, state)
                contents = handleStyleAttributes(contents, meta, state)
                if (state.elements || state.attributes) {
                    let ss = service.states[meta.destPath] ||= {}
                    if (expansive.services.css.extract === true) {
                        ss.link = meta.destPath.replaceExt('css')
                    } else {
                        ss.link = expansive.services.css.extract
                    }
                    if (state.elements) {
                        ss.elements ||= []
                        ss.elements += state.elements
                        vtrace('Extract', 'Scripts from ' + meta.document)
                    }
                    if (state.attributes) {
                        ss.attributes ||= {}
                        blend(ss.attributes, state.attributes)
                        vtrace('Extract', 'Onclick from ' + meta.document)
                    }
                }
                return contents
            }

            /*
                Post process and create external stylesheets for inline styles
             */
            function post(topMeta, service) {
                let perdoc = (expansive.services.css.extract === true)
                let styles = '/*\n    Inline styles\n */\n'

                for (let [file, state] in service.states) {
                    if (perdoc) {
                        styles = '/*\n    Inline styles for ' + file + '\n */\n'
                    }
                    if (state.elements) {
                        for (let [key,value] in state.elements) {
                            value = value.trim().split('\n').transform(function (e) e.trim()).join('\n    ')
                            //  Match {
                            value = value.replace('    }', '}') 
                            state.elements[key] = value
                        }
                        styles += state.elements.join('\n\n') + '\n\n'
                    }
                    for (let [id, code] in state.attributes) {
                        styles += id + ' {\n    ' + code + '\n}\n\n'
                    }
                    if (perdoc) {
                        let destPath = Path(file).replaceExt('css')
                        let dest = directories.dist.join(destPath)
                        let meta = blend(topMeta.clone(), { document: destPath, layout: null })
                        styles = renderContents(styles, meta)
                        writeDest(styles, meta)
                    }
                }
                if (!perdoc) {
                    let destPath = expansive.services.css.extract
                    let dest = directories.dist.join(destPath)
                    let meta = blend(topMeta.clone(), { document: destPath, layout: null })
                    styles = renderContents(styles, meta)
                    writeDest(styles, meta)
                }
            }
        `
    } ]
})
