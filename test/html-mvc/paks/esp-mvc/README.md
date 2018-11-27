esp-mvc
===

Pak for MVC support for ESP applications.

#### Description

The esp-mvc pak provides the MVC application support for ESP applications. It provides templates so that the ````esp````
command can generate controllers, migrations, scaffolds and database tables. It also includes a stub ESP application 
main module source file (app.c).

#### Provides

* esp.json &mdash; ESP MVC application configuration
* appweb.conf &mdash; Appweb hosting configuration file
* generate/* &mdash; Generation templates

### Installation

    pak install esp-mvc

#### Generate Targets

To generate an appweb.conf Appweb configuration file for hosting the ESP application in Appweb.

    esp generate appweb

To generate a controller

    esp generate controller name [action [, action] ...

To generate a database table

    esp generate table name [field:type [, field:type] ...]

To generate a migration

    esp generate migration description model [field:type [, field:type] ...]

To generate a scaffold

    esp generate scaffold model [field:type [, field:type] ...]

### Get Pak from

[https://www.embedthis.com/pak](https://www.embedthis.com/pak)
