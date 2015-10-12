/*
    expansive.es - Configuration for exp-js

    Transform by minifying.
 */
Expansive.load({
    transforms: [ {
        name:     'js',
        mappings: {
            'js',
            'min.js',
            'min.map',
            'min.js.map'
        },
        compress:   true,
        dotmin:     true,
        extract:    false,
        files:      [ 'lib/**.js*', '!lib**.map' ],
        force:      false,
        mangle:     true,
        minify:     false,
        usemap:     true,
        usemin:     true,

        script: `
            let service = expansive.services.js
            //  DEPRECATE
            for each (svc in [ 'render-js', 'minify-js' ]) {
                let sp
                if ((sp = expansive.services[svc]) != null) {
                    for each (name in [ 'compress', 'dotmin', 'minify', 'mangle', 'usemap', 'usemin' ]) {
                        if (sp[name]) {
                            service[name] = sp[name] | service[name]
                            trace('Warn', 'Legacy configuration for ' + svc + '.' + name + 
                                          ' in expansive.json. Migrate to js.' + name + '.')
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
                expansive.services['extract-js'].enable = false
            }

            function resolve(path: Path, service): Path? {
                let vfile = directories.contents.join(path)
                if (vfile.endsWith('.min.js')) {
                    let base = vfile.trimEnd('.min.js')
                    if (service.usemin && !(service.minify && service.force && base.joinExt('js', true).exists)) {
                        if (service.usemap) {
                            if (base.joinExt('min.map', true).exists || base.joinExt('min.js.map', true).exists) {
                                return path
                            }
                        } else {
                            return path
                        }
                    }
                } else if (vfile.endsWith('.min.map') || vfile.endsWith('.min.js.map')) {
                    if (service.usemin) {
                        let base = vfile.trimEnd('.min.map').trimEnd('.min.js.map')
                        if (service.usemap && !(service.minify && service.force && base.joinExt('js', true).exists)) {
                            if (base.joinExt('min.js', true).exists) {
                                return path
                            }
                        }
                    }
                } else {
                    let minified = vfile.replaceExt('min.js')
                    /*
                        Minify if forced, or a suitable minfied version does not exist or !usemin
                     */                           
                    if ((service.minify && service.force) ||
                        !(minified.exists && service.usemin && (!service.usemap ||
                            (vfile.replaceExt('min.map').exists || vfile.replaceExt('min.js.map').exists)))) {
                        if (service.minify && service.dotmin) {
                            return path.trimExt().joinExt('min.js', true)
                        }
                        return path
                    }
                }
                return null
            }
        `

    }, {
        name:     'minify-js',
        mappings: 'js',

        script: `
            let directories = expansive.directories
            let service = expansive.services.js

            let cmd = Cmd.locate('uglifyjs')
            if (!cmd) {
                trace('Warn', 'Cannot find uglifyjs')
                service.enable = false
            }
            if (service.compress) {
                cmd += ' --compress'
            }
            if (service.mangle) {
                cmd += ' --mangle'
            }
            expansive.services['minify-js'].cmd = cmd

            if (!expansive.services.js.minify) {
                expansive.services['minify-js'].enable = false
            }

            function transform(contents, meta, service) {
                trace('Minify', meta.source)
                let cmd = service.cmd
                if (expansive.services.js.usemap) {
                    let mapFile = meta.dest.replaceExt('map')
                    mapFile.dirname.makeDir()
                    cmd += ' --source-map ' + mapFile
                    contents = runFile(cmd, contents, meta)
                    let map = mapFile.readJSON()
                    map.sources = [ meta.dest ]
                    mapFile.write(serialize(map))
                } else {
                    contents = run(cmd, contents)
                }
                return contents
            }
        `

    }, {
        name:  'render-js',
        /*
            Default to only scripts from packages under contents/lib
            Required scripts may vary on a page by page basis.
         */
        mappings: {
            'js',
            'min.js'
        },
        script: `
            let service = expansive.services.js
            if (service.files) {
                if (!(service.files is Array)) {
                    service.files = [ service.files ]
                }
                expansive.control.collections.scripts += service.files
            }
            expansive.services['render-js'].hash = {}

            function pre(meta, service) {
                if (expansive.modified.everything) {
                    service.hash = {}
                }
            }

            function buildScriptList(files: Array): Array {
                let directories = expansive.directories
                let service = expansive.services.js
                let scripts = []
                for each (script in files) {
                    let before = script
                    if ((script = expansive.getDestPath(script)) == null) {
                        continue
                    }
                    let vfile = directories.contents.join(script)
                    let base = vfile.trimEnd('.min.js').trimEnd('.js')
                    let map = base.joinExt('min.map', true).exists || base.joinExt('js.map', true).exists ||
                        base.joinExt('.min.js.map', true).exists
                    if (vfile.endsWith('min.js')) {
                        if (service.usemin && (!service.usemap || map)) {
                            scripts.push(script)
                        }
                    } else {
                        let minified = vfile.replaceExt('min.js').exists
                        let map = vfile.replaceExt('min.map').exists || vfile.replaceExt('js.map').exists ||
                            vfile.replaceExt('.min.js.map').exists
                        if ((service.minify && service.force) || !minified || !(service.usemap && map)) {
                            if (service.minify && service.dotmin) {
                                scripts.push(script.replaceExt('min.js'))
                            } else {
                                scripts.push(script)
                            }
                        }
                    }
                }
                return scripts
            }

            /*
                Render Scripts is based on 'collections.scripts' which defaults to 'lib/**.js' and is modified
                via expansive.json and addItems.
             */
            public function renderScripts(filter = null, extras = []) {
                let collections = expansive.collections
                if (!collections.scripts) {
                    return
                }
                let directories = expansive.directories
                let service = expansive.services['render-js']

                /*
                    Pages have different scripts and so must compute script list per page.
                    This is hased and saved.
                 */
                if (!service.hash[collections.scripts]) {
                    let files = directories.contents.files(collections.scripts, 
                        { contents: true, directories: false, relative: true})
                    files = expansive.orderFiles(files, "js")
                    service.hash[collections.scripts] = buildScriptList(files).unique()
                }
                for each (script in service.hash[collections.scripts]) {
                    if (filter && !Path(script).glob(filter)) {
                        continue
                    }
                    let uri = meta.top.join(script).trimStart('./')
                    write('<script src="' + uri + '"></script>\n    ')
                }
                if (extras && extras is String) {
                    extras = [extras]
                }
                if (expansive.services['extract-js'].states) {
                    let extracted = expansive.services['extract-js'].states[meta.destPath]
                    if (extracted && extracted.href) {
                        extras.push(expansive.services.js.resolve(extracted.href, expansive.services.js))
                    }
                }
                for each (script in extras) {
                    let uri = meta.top.join(script).trimStart('./')
                    write('<script src="' + uri + '"></script>\n    ')
                }
            }
        `

    }, {
        name:       'extract-js',
        mappings:   'html',

        script: `
            let service = expansive.services['extract-js']
            service.nextId = 0
            service.states = {}

            /*
                Local function to extract inline script elements
             */
            function handleScriptElements(contents, meta, state): String {
                let service = expansive.services['extract-js']
                let re = /<script[^>]*>(.*)<\\/script>/smg
                let start = 0, end = 0
                let output = ''
                let matches
                while (matches = re.exec(contents, start)) {
                    let elt = matches[0]
                    end = re.lastIndex - elt.length
                    output += contents.slice(start, end)
                    if (elt.match(/src=['"]/)) {
                        output += matches[0]
                    } else {
                        state.scripts ||= []
                        state.scripts.push(matches[1].trimEnd(';'))
                    }
                    start = re.lastIndex
                }
                output += contents.slice(start)
                return output
            }

            /*
                Local function to extract onclick attributes 
            */
            function handleScriptAttributes(contents, meta, state): String {
                let result = ''
                let re = /<([\\w_\\-:.]+)([^>]*>)/g
                let start = 0, end = 0
                let matches

                while (matches = re.exec(contents, start)) {
                    let elt = matches[0]
                    end = re.lastIndex - matches[0].length
                    /* Emit prior contents */
                    result += contents.slice(start, end)

                    let clickre = /(.*) +(onclick=)(["'])(.*?)(\\3)(.*)/m
                    if ((cmatches = clickre.exec(elt)) != null) {
                        elt = cmatches[1] + cmatches[6]
                        let id
                        if ((ematches = elt.match(/id=['"]([^'"]+)['"]/)) != null) {
                            id = ematches[1]
                        } else {
                            let service = expansive.services['extract-js']
                            // id = 'exp-' + md5(++service.nextId).slice(0, 8)
                            id = 'exp-' + ++service.nextId
                            elt = cmatches[1] + ' id="' + id + '"' + cmatches[6]
                        }
                        state.onclick ||= {}
                        state.onclick[id] = cmatches[4].trimEnd(';')
                    }
                    result += elt
                    start = re.lastIndex
                }
                return result + contents.slice(start)
            }

            function transform(contents, meta, service) {
                if (!expansive.services.js.extract) {
                    return contents
                }
                let state = {}
                contents = handleScriptElements(contents, meta, state)
                contents = handleScriptAttributes(contents, meta, state)
                if (state.scripts || state.onclick) {
                    let ss = service.states[meta.destPath] ||= {}
                    if (expansive.services.js.extract === true) {
                        ss.href = meta.destPath.replaceExt('js')
                    } else {
                        ss.href = expansive.services.js.extract
                    }
                    if (state.scripts) {
                        ss.scripts ||= []
                        ss.scripts += state.scripts
                        vtrace('Extract', 'Scripts from ' + meta.document)
                    }
                    if (state.onclick) {
                        ss.onclick ||= {}
                        blend(ss.onclick, state.onclick)
                        vtrace('Extract', 'Onclick from ' + meta.document)
                    }
                }
                return contents
            }

            /*
                Post process and create external script files for inline scripts
             */
            function post(topMeta, service) {
                let perdoc = (expansive.services.js.extract === true)
                let scripts = '/*\n    Inline scripts for \n */\n'

                for (let [file, state] in service.states) {
                    if (perdoc) {
                        scripts = '/*\n    Inline scripts for ' + file + '\n */\n'
                    }
                    if (state.scripts) {
                        scripts += state.scripts.unique().join(';\n\n') + ';\n\n'
                    }
                    for (let [id, code] in state.onclick) {
                        scripts += 'document.getElementById("' + id + '").addEventListener("click", function() { ' +
                            code + '});\n\n'
                    }
                    if (perdoc) {
                        let destPath = Path(file).replaceExt('js')
                        let dest = directories.dist.join(destPath)
                        let meta = blend(topMeta.clone(), { document: destPath, layout: null })
                        scripts = renderContents(scripts, meta)
                        writeDest(scripts, meta)
                    }
                }
                if (!perdoc) {
                    let destPath = expansive.services.js.extract
                    let dest = directories.dist.join(destPath)
                    let meta = blend(topMeta.clone(), { document: destPath, layout: null })
                    scripts = renderContents(scripts, meta)
                    writeDest(scripts, meta)
                }
            }
        `
    } ]
})
