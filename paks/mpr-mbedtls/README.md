mpr-mbedtls
===

MbedTLS SSL stack interface. 

## Installation

Install into a product that supports the MPR such as Appweb.

    pak install mpr-mbedtls

## Building

Optionally add the following to main.me to modify the MbedTLS compile time configuration.

    settings: {
        mbedtls: {
            compact: true,      /* Compact edition - disables all the ciphers and features below */
            blowfish: false,    /* Disable Blowfish cipher */
            camellia: false,    /* Disable Camellia cipher */
            rc4: false,         /* Disable RC4 */
            des: false,         /* Disable DES */
            padlock: false,     /* Disable Via Padlock */
            psk: false,         /* Disable Pre-shared Keys */
            romTables: false,   /* Disable Rom tables */
            xtea: false         /* Disable XTEA */
        }
    }

    ./configure --with mbedtls build

## Configuration

Compile time SSL controls:

    settings: {
        ssl: {
            cache: 512,         /* Set the session cache size (items) */
            logLevel: 5,        /* Starting logging level for SSL messages */
            renegotiate: true,  /* Enable/Disable SSL renegotiation (defaults to true) */
            ticket: true,       /* Enable session resumption via ticketing - client side session caching */
            timeout: 86400      /* Session and ticketing duration in seconds */
        }
    }

## Get Pak

[https://embedthis.com/pak/](https://embedthis.com/pak/download.html)
