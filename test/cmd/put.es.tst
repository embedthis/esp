/*
    put.tst - Test the put command
 */

require ejs.testme
require support

cleanDir('dist/tmp')


//  PUT file
http('test.dat /tmp/day.tmp')
ttrue(Path('dist/tmp/day.tmp').exists)

//  PUT files
http(Path('.').files('*.dat').join(' ') + ' /tmp/')
ttrue(Path('dist/tmp/test.dat').exists)

// cleanDir('dist/tmp')
