/*
    expansive.es - Configuration for exp-esp

    Post process by compiling esp pages, controllers and apps
 */
Expansive.load({

    services: {
        name:     'esp',
        clean:    'esp clean',
        compile:  'esp compile',
        esp:      Cmd.locate('esp'),
        keep:     true,
        serve:    'esp --trace stdout:4 LISTEN',

        transforms: {
            mappings: 'esp'
            init: function(transform) {
                /*
                    Create command to run esp to serve requests
                 */
                let service = transform.service
                let endpoint: Uri = Uri(expansive.options.listen || expansive.control.listen || '127.0.0.1:4000').complete()
                let listen = endpoint.host + (endpoint.port ? (':' + endpoint.port) : '')
                if (service.serve) {
                    service.serve = service.serve.replace('LISTEN', listen)
                }
                expansive.control.server ||= service.serve

                let esp = Path('esp.json')
                if (esp.exists) {
                    let ecfg = esp.readJSON()
                    let package = expansive.package
                    if (package.pak) {
                        let profile = package.profile
                        if (profile && ecfg.profiles) {
                            blend(ecfg, ecfg.profiles[profile], {combine: true})
                        }
                    }
                    if (ecfg.esp) {
                        service.combine = ecfg.esp.combine
                    }
                }
            },

            pre: function(transform) {
                let service = transform.service
                if (expansive.modify.everything) {
                    trace('Clean', service.clean)
                    run(service.clean)
                    trace('Compile', service.compile)
                }
                transform.files = []
            },

            render: function(contents, meta, transform) {
                if (meta.isDocument) {
                    transform.files.push(meta.dest)
                }
                return contents
            }

            post: function(transform) {
                let service = transform.service
                if (service.combine) {
                    trace('Run', service.compile)
                    Cmd.run(service.compile)
                } else {
                    for each (file in transform.files) {
                        trace('Run', service.compile + ' ' + file)
                        Cmd.run(service.compile + ' ' + file)
                    }
                }
                if (!service.keep) {
                    for each (file in transform.files) {
                        file.remove()
                    }
                }
            }
        }
    }
})
