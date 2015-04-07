/*
    upload.tst - Test http upload
 */

require support

let upfile = 'support.es.com'
let updir = Path('uploaded')

cleanDir(updir)
ttrue(updir.files().length == 0)

//  Upload to html
data = http("--upload support.es.com /upload/upload.html")
ttrue(data.contains('Upload Complete'))
ttrue(updir.files().length > 0)
ttrue(updir.join(upfile).exists)
ttrue(updir.join(upfile).readString().contains('support.es.com --'))
cleanDir(updir)
ttrue(updir.files().length == 0)

//  Upload to esp
data = http("--upload support.es.com /upload/upload.esp")
ttrue(data.contains('Upload Complete'))
ttrue(updir.files().length > 0)
ttrue(updir.join(upfile).exists)
ttrue(updir.join(upfile).readString().contains('support.es.com --'))

cleanDir(updir)
ttrue(updir.files().length == 0)
