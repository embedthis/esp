Expansive.load({

    services: {
        name: 'canon',
        init: function() {
            global.renderCanonical = function() {
                if (meta.dest.basename.trimExt() == 'index') {
                    let ref: String = meta.absurl.dirname
                    if (!ref.endsWith('/')) {
                        ref += '/'
                    }
                    write('<link href="' + ref + '" rel="canonical" />')
                }
            }
        }
    }
})
