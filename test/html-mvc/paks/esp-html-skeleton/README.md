esp-html-skeleton
===

HTML5 skeleton for ESP

#### Description

Provides a skeleton for a HTML5 application using ESP, Expansive and Less stylesheets.

#### Provides

* documents/css &mdash; Application less stylesheets
* documents/index.esp &mdash; Default home page
* esp.json &mdash; ESP configuration file
* expansive.json &mdash; Expansive configuration file
* generate/* &mdash; Generation templates
* layouts/default.html.exp &mdash; Master web page layout 
* partials/* &mdash; Web page partial content

### Installation

    pak install esp-html-skeleton

### Building

    expansive render

In debug mode, 

### Running

    expansive

or

    expansive render
    esp

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
