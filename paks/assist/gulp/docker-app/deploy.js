/*
    deploy.js - Deploy published package
 */

import * as spawn from 'child_process'
import * as path from 'path'
import * as gulp from 'gulp'
import * as log from 'fancy-log'
import * as fs from 'fs'
import * as json5 from 'json5'
import config from 'assist'

function deploy(cb) {
    let aws = config.aws
    let cmd, args
    let part = path.basename(global.top)
    let settings = config[part]

    if (settings.proxy) {
        args = ['--aws-profile', settings.aws_profile, '--deploy', settings.deploy, '--group', settings.group,
                '--proxy', settings.proxy, '--region', settings.region, '--validate', settings.validate, settings.image]
        for (let [index, value] of Object.entries(args)) {
            args[index] = expand(value, config)
        }
        cmd = '../paks/assist/docker-deploy-elb'
    } else {
        cmd = '../paks/assist/docker-deploy-instance'
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

export default gulp.series(deploy)
