/*
    Build everything
 */

import * as gulp from 'gulp'
import * as log from 'fancy-log'
import clean from './clean'
import config from 'assist'
import blend from 'assist/blend'
import render from './render'
import patch from './patch'
import publish from './publish'
import run from './run'
import sync from './sync'
import watch from './watch'

function setProd(done) {
    config.profile = 'prod'
    blend(config, config.profiles[config.profile])
    done()
}

function noDeploy(cb) {
    // print(`Skipping: no deploy action required`)
    cb()
}

function noPackage(cb) {
    // print(`Skipping: no package action required`)
    cb()
}

function trace(cb) {
    print(`Using: profile ${config.profile} ${config.debug ? 'debug' : ''}`)
    cb()
}

gulp.task('patch', gulp.series(patch))
gulp.task('render', render)

gulp.task('build', gulp.series('patch', 'render'))
gulp.task('clean', gulp.series(clean))
gulp.task('deploy', gulp.series(noDeploy))
gulp.task('package', gulp.series(noPackage))
gulp.task('publish', gulp.series(publish))
gulp.task('promote', gulp.series(setProd, trace, 'build', 'package', 'publish'))
gulp.task('run', gulp.series(run))
gulp.task('sync', gulp.series(sync))
gulp.task('watch', gulp.series(watch))

gulp.task('default', gulp.series(run))
