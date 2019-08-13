mpr-version 
===

SemVer version management

## Installation

    pak install mpr-mbedtls

## Notes

Version compatibity rules:

    VER                             Allows prereleases
    1.2.[*xX]                       Wild card version portion. (allows prereleases).
    ~VER                            Compatible with VER at the least significant level.
                                    ~1.2.3 == (>=1.2.3 <1.3.0) Compatible at the patch level
                                    ~1.2 == 1.2.x   Compatible at the minor level
                                    ~1 == 1.x   Compatible at the major level
    ^VER                            Compatible with VER at the most significant level.
                                    ^0.2.3 == 0.2.3 <= VER < 0.3.0
                                    ^1.2.3 == 1.2.3 <= VER < 2.0.0
    [>, >=, <, <=, ==, !=]VER       Create range relative to given version
    EXPR1 - EXPR2                   <=EXPR1 <=EXPR2
    EXPR1 || EXPR2 ...              EXPR1 OR EXPR2
    EXPR1 && EXPR2 ...              EXPR1 AND EXPR2
    EXPR1 EXPR2 ...                 EXPR1 AND EXPR2

    Pre-release versions will only match if the criteria contains a "-.*" prerelease suffix


## Get Pak

[https://www.embedthis.com/pak/](https://www.embedthis.com/pak/)
