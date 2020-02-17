/*
    js-blend - Blend objects
 */

function clone(src) {
    var result

    if (Array.isArray(src)) {
        result = src.slice(0)
    } else if (typeof src == 'object' && !(src instanceof Date || src instanceof RegExp || src == null)) {
        result = Object.create(Object.getPrototypeOf(src))
        var i, descriptor, keys = Object.getOwnPropertyNames(src)
        for (i = 0; i < keys.length; i ++) {
            descriptor = Object.getOwnPropertyDescriptor(src, keys[ i ])
            if (descriptor.value && typeof descriptor.value === 'object') {
                descriptor.value = clone(descriptor.value)
            }
            Object.defineProperty(result, keys[i], descriptor)
        }
    } else {
        result = src
    }
    return result
}

export default function blend(dest, src, combine = '') {
    if (!src) {
        return dest
    }
    if (!dest || typeof dest != 'object' || Array.isArray(dest)) {
        return dest
    }
    for (let key of Object.getOwnPropertyNames(src)) {
        let property = key
        let op = key[0]
        if (op == '+') {
            property = key.slice(1)
        } else if (op == '-') {
            property = key.slice(1)
        } else if (op == '?') {
            property = key.slice(1)
        } else if (op == '=') {
            property = key.slice(1)
        } else if (combine) {
            op = combine
        } else {
            /* Default is to blend */
            op = ''
        }
        let s = src[key]
        if (!dest.hasOwnProperty(property)) {
            if (op == '-') {
                continue
            }
            dest[property] = clone(s)
            continue
        } else if (op == '?') {
            continue
        }
        let d = dest[property]
        if (Array.isArray(d)) {
            if (op == '+') {
                if (Array.isArray(s)) {
                    for (let item of s) {
                        if (d.indexOf(s) < 0) d.push(item)
                    }
                } else {
                    d.push(s)
                }
            } else if (op == '-') {
                if (Array.isArray(s)) {
                    for (let item of s) {
                        let index = d.indexOf(item)
                        if (index >= 0) d.slice(index, 1)
                    }
                } else {
                    let index = d.indexOf(s)
                    if (index >= 0) d.slice(index, 1)
                }
            } else {
                /* op == '=' */
                dest[property] = clone(s)
            }
        } else if (typeof d == 'object' && d !== null && d !== undefined) {
            if (op == '=') {
                dest[property] = clone(s)
            } else if (op == '-') {
                delete dest[property]
            } else if (typeof s == 'object') {
                blend(d, s, op)
            }
        } else if (typeof d == 'string') {
            if (op == '+') {
                dest[property] += ' ' + s
            } else if (op == '-') {
                if (d == s) {
                    delete dest[property]
                } else {
                    dest[property] = d.replace(s, '')
                }
            } else {
                /* op == '=' */
                dest[property] = s
            }
        } else if (typeof d == 'number') {
            if (op == '+') {
                dest[property] += s
            } else if (op == '-') {
                dest[property] -= s
            } else {
                /* op == '=' */
                dest[property] = s
            }
        } else {
            if (op == '=') {
                dest[property] = s
            } else if (op == '-') {
                delete dest[property]
            } else {
                dest[property] = s
            }
        }
    }
    return dest
}
