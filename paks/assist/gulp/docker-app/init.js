/*
    init.js - Initialization
 */

global.print = (...args) => console.log(...args)

process.env.NODE_PATH = [ global.top + '/../paks', global.top + '/node_modules'].join(':')
require('module').Module._initPaths()
