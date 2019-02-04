/*
    clean.js - Clean
 */

import * as spawn from 'child_process'
import * as gulp from 'gulp'
import * as log from 'fancy-log'
import config from 'assist'

function clean(cb) {
    let cmd = 'expansive'
    print(`Cleaning: ${cmd} clean`)
    let pkg = spawn.spawn(cmd, ['-q', 'clean'], {cwd: '.'})
    pkg.stdout.on('data', data => process.stdout.write(data.toString()))
    pkg.stderr.on('data', data => process.stdout.write(data.toString()))
    pkg.on('exit', err => {
        if (err) {
            throw new Error('Cannot package web')
        }
        cb()
    })
}

export default gulp.series(clean)
