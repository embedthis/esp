/*
    render.js - Render web
 */
import * as spawn from 'child_process'
import * as gulp from 'gulp'
import * as log from 'fancy-log'
import config from 'assist'

function render(cb) {
    let cmd = 'expansive'
    print(`Running: ${cmd} -r --profile ${config.profile} render`)

    let pkg = spawn.spawn(cmd, ['-r', '-q', '--profile', config.profile, 'render'], {cwd: '.'})
    pkg.stdout.on('data', data => process.stdout.write(data.toString()))
    pkg.stderr.on('data', data => process.stdout.write(data.toString()))
    pkg.on('exit', err => {
        if (err) {
            throw new Error('Cannot render with expansive')
        }
        cb()
    })
}

export default gulp.series(render)
