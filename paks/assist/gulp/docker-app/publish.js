/*
    publish.js - Publish package
 */

import * as spawn from 'child_process'
import * as path from 'path'
import * as gulp from 'gulp'
import * as log from 'fancy-log'
import * as fs from 'fs'
import * as json5 from 'json5'
import config from 'assist'

function publish(cb) {
    let cmd = '../paks/assist/docker-publish'
    let part = path.basename(global.top)
    let settings = config[part]
    let args = ['--primary', 'true', '--aws-profile', settings.aws_profile, '--region', settings.region, settings.image]
    for (let [index, value] of Object.entries(args)) {
        args[index] = expand(value, config)
    }
    print(`Running: ${cmd} ${args.join(' ')} ...`)
    let pkg = spawn.spawn(cmd, args)
    pkg.stdout.on('data', data => process.stdout.write(data.toString()))
    pkg.stderr.on('data', data => process.stdout.write(data.toString()))
    pkg.on('exit', err => {
        if (err) {
            throw new Error(`Cannot publish ${err.toString()}`)
        }
        cb()
    })
}

export default gulp.series(publish)
