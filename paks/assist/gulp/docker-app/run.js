/*
    run - Run and watch
 */

import * as spawn from 'child_process'
import * as gulp from 'gulp'
import * as log from 'fancy-log'
import config from 'assist'

function run(cb) {
    let name = config.name
    let cmd = '../paks/assist/gulp/docker-app/run.sh'
    spawn.execSync(`docker stop -t 0 ${config.name} >/dev/null 2>&1; docker rm ${config.name} >/dev/null 2>&1 ; true`)
    print(`Running: ${cmd}`)
    let pkg = spawn.spawn(cmd, ['debug', 'background'])
    pkg.stdout.on('data', data => process.stdout.write(data.toString()))
    pkg.stderr.on('data', data => process.stdout.write(data.toString()))
    pkg.on('exit', err => {
        if (err) {
            throw new Error('Cannot run server')
        }
        cb()
    })
    process.on('SIGINT', function () {
        log('Cleanup', `docker stop -t 0 ${config.name}; docker rm ${config.name}`)
        spawn.execSync(`docker stop -t 0 ${config.name} >/dev/null 2>&1; docker rm ${config.name} >/dev/null 2>&1 ; true`)
        process.exit(0)
    })
}


export default gulp.series(run)
