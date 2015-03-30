/*
    put.tst - Test the put command
 */

require support

cleanDir('dist/tmp')

//  PUT file
http('test.dat /tmp/day.tmp')
ttrue(Path('dist/tmp/day.tmp').exists)

//  PUT files
http(Path('.').files('*.tst').join(' ') + ' /tmp/')
ttrue(Path('dist/tmp/basic.es.tst').exists)

cleanDir('dist/tmp')
