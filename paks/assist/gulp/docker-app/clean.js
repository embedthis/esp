/*
    clean.js - Clean
 */

import * as spawn from 'child_process'
import * as gulp from 'gulp'
import * as log from 'fancy-log'
import config from 'assist'

function clean(cb) {
    cb()
}

export default gulp.series(clean)
