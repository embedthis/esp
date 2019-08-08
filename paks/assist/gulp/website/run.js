/*
    run - Run and watch
 */

import * as spawn from 'child_process'
import * as gulp from 'gulp'
import * as log from 'fancy-log'
import config from 'assist'
import watch from './watch'

function run(cb) {
    let cmd = '../paks/assist/gulp/website/run.sh'
    print(`Running: ${cmd}`)

    let pkg = spawn.spawn(cmd, ['dev', 'background'])
    pkg.stdout.on('data', data => process.stdout.write(data.toString()))
    pkg.stderr.on('data', data => process.stdout.write(data.toString()))
    pkg.on('exit', err => {
        if (err) {
            throw new Error('Cannot run server')
        }
        cb()
    })
    process.on('SIGINT', function () {
        process.exit(0)
    })
}

export default gulp.parallel(run, watch)
