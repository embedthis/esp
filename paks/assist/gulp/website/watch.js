/*
    watch - watch for changes
 */

import * as spawn from 'child_process'
import * as gulp from 'gulp'
import * as log from 'fancy-log'
import config from 'assist'
import render from './render'

function onChange(file) {
    log('File ' + file + ' was modified')
}

function watch(cb) {
    gulp.watch(['src/**/*', '!src/.expansive-lastgen'], {interval: 750}, gulp.series(render)).on('change', onChange)
    cb()
}

export default gulp.series(render, watch)
