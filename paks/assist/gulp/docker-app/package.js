/*
    package.js - Build the package
 */

import * as spawn from 'child_process'
import * as path from 'path'
import * as gulp from 'gulp'
import * as gulpif from 'gulp-if'
import * as log from 'fancy-log'
import * as gzip from 'gulp-gzip'
import config from 'assist'

function buildPackage(cb) {
    let cmd = 'bin/package'
    print(`Running: ${cmd} ${config.profile} ...`)

    let part = path.basename(global.top)
    let settings = config[part]
    let pkg = spawn.spawn(cmd, [settings.image, config.owner, config.profile])
    pkg.stdout.on('data', data => process.stdout.write(data.toString() + '\n'))
    pkg.stderr.on('data', data => process.stdout.write(data.toString() + '\n'))
    pkg.on('exit', err => {
        if (err) {
            throw new Error('Cannot package')
        }
        cb()
    })
}

/*
function compress(cb) {
    if (config.profile == 'prod') {
        gulp.src(['build/*.js'], {buffer: false})
            .pipe(gzip({}))
            .pipe(gulp.dest('build'))
    }
    cb()
} */

export default gulp.parallel(buildPackage)
