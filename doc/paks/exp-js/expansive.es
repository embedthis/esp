Expansive.load({
    transforms: [ {
        name:  'render-js',
        files: null,
        mappings: {
            'js',
            'min.js',
            'min.map',
            'min.js.map'
        },

        /*
            Smart defaults for debug. Will use minified if map present.
         */
        usemap: true,
        usemin: null,

        script: `
            function initRenderScripts() {
                let directories = expansive.directories
                let service = expansive.services['render-js']
                let collections = expansive.control.collections
                /*
                    Build list of scripts to render. Must be in pak dependency order.
                    Permit explicit script override list in pak.render.js
                 */
                let files
                if (service.files) {
                    files = directories.top.files(service.files, { directories: false, relative: true})
                } else {
                    let list = directories.top.files(expansive.control.documents, 
                        {contents: true, directories: false, relative: true })
                    list = list.filter(function(path) path.glob(['**.js', '**.min.map', '**.min.js.map']))
                    files = expansive.orderFiles(list, "js")
                }
                files = files.unique()

                /*
                    Save instructions for minify-js in the service.hash
                    Update the scripts collection for use by renderScripts()
                 */
                let scripts = []
                service.hash = {}
                let minify = expansive.services['minify-js']
                for each (file in files) {
                    let ext = file.extension
                    let script = null
                    if (ext == 'map') {
                        if (service.usemap) {
                            service.hash[file.name] = true
                        } else {
                            service.hash[file.name] = 'not required because "usemap" is false.'
                        }
                    } else if (file.endsWith('min.js')) {
                        if (service.usemin ||
                           (service.usemin !== false && service.usemap && 
                                /* angular.min.map or angular.min.js.map */
                                (file.replaceExt('map').exists || file.replaceExt('js.map').exists))) {
                            script = file
                            service.hash[file.name] = true
                        } else {
                            service.hash[file.name] = 'not required because "usemin" is false.'
                        }
                    } else if (ext == 'js') {
                        let minified = file.replaceExt('min.js')
                        if (service.usemin && minified.exists) {
                            service.hash[file.name] = 'not required because ' + file.replaceExt('min.js') + ' exists.'
                        } else {
                            if (service.usemap && minified.exists && 
                                (file.replaceExt('min.map').exists || file.replaceExt('js.map').exists || 
                                 file.replaceExt('.min.js.map').exists)) {
                                service.hash[file.name] = 'not required because ' + file.replaceExt('min.js') + ' exists.'
                            } else if (minify.minify) {
                                service.hash[file.name] = { minify: true }
                                script = file
                            } else {
                                service.hash[file.name] = true
                                script = file
                            }
                        }
                    }
                    if (script) {
                        scripts.push(script.trimStart(directories.lib + '/').trimStart(directories.contents + '/'))
                    }
                }
                collections.scripts = scripts + (collections.scripts || [])
                if (expansive.options.debug) {
                    print("EXP_JS", "usemap", service.usemap, "usemin", service.usemin, "minify", service.minify)
                    dump("EXP-JS HASH", service.hash)
                    dump("EXP-JS COLLECTIONS", collections)
                }
            }
            initRenderScripts()

            public function renderScripts(filter, extras = []) {
                let scripts = (expansive.collections.scripts || []) + extras
                for each (script in scripts.unique()) {
                    if (filter && !Path(script).glob(filter)) {
                        continue
                    }
                    write('<script src="' + meta.top + script + '"></script>\n    ')
                }
                if (expansive.collections['inline-scripts']) {
                    write('<script>')
                    for each (script in expansive.collections['inline-scripts']) {
                        write(script)
                    }
                    write('\n    </script>')
                }
            }
        `
    }, {
        name:       'minify-js',
        mappings: {
            'js',
            'min.js',
            'min.map',
            'min.js.map'
        },

        minify:     false,
        dotmin:     true,
        genmap:     true,
        compress:   true,
        mangle:     true,

        script: `
            let service = expansive.services['minify-js']
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
            service.cmd = cmd

            /*
                Transformation callback called when rendering
             */
            function transform(contents, meta, service) {
                let instructions = expansive.services['render-js'].hash[meta.source]
                if (!instructions) {
                    return contents
                }
                if (instructions is String) {
                    vtrace('Info', meta.path + ' ' + instructions)
                    return null
                }
                if (instructions.minify) {
                    vtrace('Info', 'Minify', meta.path)
                    let cmd = service.cmd
                    if (service.genmap) {
                        let mapFile = meta.dest.replaceExt('min.map')
                        mapFile.dirname.makeDir()
                        cmd += ' --source-map ' + mapFile
                        contents = runFile(cmd, contents, meta)
                        let map = mapFile.readJSON()
                        map.sources = [ meta.dest ]
                        mapFile.write(serialize(map))
                    } else {
                        contents = run(cmd, contents)
                    }
                    if (service.dotmin && !meta.dest.contains('min.js')) {
                        meta.dest = meta.dest.trimExt().joinExt('min.js', true)
                    }
                }
                return contents
            }
        `
    } ]
})
