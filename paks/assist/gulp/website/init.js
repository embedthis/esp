/*
    init.js - Initialization
 */

global.print = (...args) => console.log(...args)

let path = process.env.NODE_PATH
if (!path || path.indexOf('paks') < 0) {
    process.env.NODE_PATH = [ 
        global.top + '/../paks', 
        global.top + '/node_modules'
    ].join(':')
}

require('module').Module._initPaths()
