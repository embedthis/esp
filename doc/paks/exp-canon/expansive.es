Expansive.load({
    transforms: [ {
        name: 'canon',
        script: `
            public function renderCanonical() {
                if (meta.dest.basename.trimExt() == 'index') {
                    let ref: String = meta.abs.dirname
                    if (!ref.endsWith('/')) {
                        ref += '/'
                    }
                    write('<link href="' + ref + '" rel="canonical" />')
                }
            }
        `
    } ]
})
