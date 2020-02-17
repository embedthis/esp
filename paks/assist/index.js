/*
    config - Read configuration for gulp builds and define polyfills
 */

import * as fs from 'fs'
import * as json5 from 'json5'
import * as gulp from 'gulp'
import * as log from 'fancy-log'
import blend from './blend'

/*
    Polyfills
 */
global.print = (...args) => log(...args)
global.dump = (...args) => { for (let item of args) print(JSON.stringify(item, null, 4)) }

/*
    Build up config environment which is
        ../pak.json + package.json + pak.json + product.json + command line
 */
var config = {}
let argv = process.argv
let files = [
    '../CONFIG/keys.json',
    'CONFIG/keys.json',
    '../pak.json',
    'pak.json',
    'product.json',
]
for (let file of files) {
    file = process.cwd() + '/' + file
    if (fs.existsSync(file)) {
        let json
        try {
            json = json5.parse(fs.readFileSync(file))
        } catch (e) {
            print(`Cannot parse ${file}`)
            throw e
        }
        blend(config, json)
    }
}
if (fs.existsSync('../CONFIG/terraform.json')) {
    config.terraform = {}
    blend(config.terraform, json5.parse(fs.readFileSync('../CONFIG/terraform.json')))
}

/*
    Support --profile and other '-' switches. Convert --args into config.NAME = value.
 */
let args = {}
for (let i = 2; i < argv.length; i++) {
    let arg = argv[i]
    if (arg.indexOf('--') == 0) {
        arg = arg.slice(2)
        args[arg] = argv[++i] || true
    } else if (arg.indexOf('-') == 0) {
        arg = arg.slice(1)
        args[arg] = true
    }
}

/*
    Set sensible defaults: Default to profile == dev, debug == true if dev.
 */
config.profile = args.profile || process.env.PROFILE || 'dev'

if (config.profiles && config.profile && config.profiles[config.profile]) {
    blend(config, config.profiles[config.profile])
}
config.debug = args.debug || config.debug
if (config.debug == 'true') {
    config.debug = true
}


/*
    Get version from ../pak.json or ./pak.json
 */
try {
    var parent = json5.parse(fs.readFileSync(process.cwd() + '/../pak.json'))
    config.version = parent.version
} catch (err) {
    try {
        var parent = json5.parse(fs.readFileSync(process.cwd() + '/pak.json'))
        config.version = parent.version
    } catch (err) {
        print('Cannot read ../pak.json or ./pak.json')
    }
}

global.expand = function(v, ...contexts) {
    let json = JSON.stringify(v)
    for (let context of contexts) {
        json = template(json, context)
    }
    return JSON.parse(json)
}

function template(v, context) {
    let text = v.toString()
    let matches = text.match(/\$\{[^}]*}/gm)
    if (matches) {
        for (let name of matches) {
            let word = name.slice(2).slice(0, -1)
            if (context[word] == undefined) {
                context[word] = '${' + word + '}'
            }
        }
        let fn = Function('_context_', 'with (_context_) { return `' + text + '`}')
        return fn(context)
    }
    return text
}

/*
    Map configuration hash into env var format. Uppercase with dots and "-" converted to "_".
 */
function mapEnv(obj, prefix = '', vars = {}) {
    for (let name of Object.keys(obj)) {
        let value = obj[name]
        if (name == 'profiles') {
            continue
        }
        if (typeof value == 'object') {
            mapEnv(value, prefix + name.toUpperCase() + '_', vars)
        } else {
            name = (prefix + name).toUpperCase().replace(/\./g, '_').replace(/-/g, '_')
            vars[name] = value
        }
    }
    return vars
}

global.getEnv = function(obj, context) {
    let vars = mapEnv(obj)
    for (let [index, v] of Object.entries(vars)) {
        vars[index] = expand(v, context)
    }
    let env = blend({}, process.env)
    return blend(env, vars)
}


export default config
