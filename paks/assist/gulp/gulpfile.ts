/*
    gulpfile.js - main gulp file
 */
var fs = require('fs');
var requireDir = require('require-dir');

eval(`global.top = "${__dirname}"`);

require('../paks/assist/gulp/website/init');
requireDir('../paks/assist/gulp/website');
if (fs.existsSync('bin/gulp')) {
    requireDir('bin/gulp')
}
