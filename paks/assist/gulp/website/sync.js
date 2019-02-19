/*
    sync.js - Sync doc
 */

import * as Fs from 'fs'
import * as gulp from 'gulp'
import * as log from 'fancy-log'
import * as del from 'del'
import config from 'assist'

function sync(cb) {
    cb()
}

export default gulp.series(sync)

