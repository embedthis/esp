/*
    support.es.com -- Support functions for the Http unit tests
 */

module support {
    require ejs.testme

    function http(args): String {
        let HOST = tget('TM_HTTP') || "127.0.0.1:5100"
        let httpcmd = Cmd.locate('http') + ' --host ' + HOST + " "
        // tinfo('CMD', httpcmd + args)
        let result = Cmd.run(httpcmd + args, {exceptions: false})
        // tinfo('Result', result)
        return result.trim()
    }
}
