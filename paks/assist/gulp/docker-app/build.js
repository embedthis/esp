/*
    Build everything
 */

import * as gulp from 'gulp'
import * as log from 'fancy-log'
import config from 'assist'
import {blend} from 'assist/blend'
import buildPackage from './package'
import deploy from './deploy'
import publish from './publish'
import run from './run'

function setProd(done) {
    config.profile = 'prod'
    blend(config, config.profiles[config.profile])
    done()
}

function trace(cb) {
    print(`Using: profile ${config.profile}  ${config.debug ? 'debug' : ''}`)
    cb()
}

function noBuild(cb) {
    print(`Skipping: no build action required`)
    cb()
}

function noClean(cb) {
    // print(`Skipping: no clean action taken`)
    cb()
}

gulp.task('build', gulp.series(noBuild))
gulp.task('clean', gulp.series(noClean))
gulp.task('deploy', gulp.series(deploy))
gulp.task('package', gulp.series(buildPackage))
gulp.task('publish', gulp.series(publish))
gulp.task('promote', gulp.series(setProd, trace, buildPackage, publish, deploy))
gulp.task('run', gulp.series(run))

gulp.task('default', gulp.series(run))
