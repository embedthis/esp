/*
    delete.tst - Test http delete
 */

require support

//  PUT
http("test.dat /tmp/test.dat")
ttrue(Path("dist/tmp/test.dat").exists)

http("--method DELETE /tmp/test.dat")
ttrue(!Path("dist/tmp/test.dat").exists)

