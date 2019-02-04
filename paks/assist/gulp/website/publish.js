/*
    publish.js - Publish package
 */
 
import * as spawn from 'child_process'
import * as gulp from 'gulp'
import * as log from 'fancy-log'
import * as path from 'path'
import config from 'assist'

function publish(cb) {
    let cmd = '../paks/assist/s3-publish-files'
    let tf = config.terraform
    let part = path.basename(process.cwd())
    let args = [`build/${part}`, tf[`${part}_cdn_bucket`].value, tf[`${part}_cdn_id`].value, tf.aws_profile.value]

    print(`Publishing: profile "${config.profile}" ${config.version}`)
    print(`Starting: ${cmd} ${args.join(' ')} ...`)

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
