esp-server
===

Pak for basic ESP applications.

#### Provides

* esp.json &mdash; ESP application configuration
* appweb.conf &mdash; Appweb hosting configuration file
* public/index.esp &mdash; Demonstration home page

#### Description

The esp-server pak provides the lowest level of ESP application support. It provides a basic esp.json ESP configuration 
file and the ability to generate Appweb hosting configuration files.

The esp.json configuration file defines the esp-server route set that creates a simple controller/action route. 
Run ````esp -s```` to display the route table. 


#### Generate Targets

To generate an appweb.conf Appweb configuration file for hosting the ESP application in Appweb.

    esp generate appweb

### Install

    pak install esp-server

### Get Pak from

[https://www.embedthis.com/pak](https://www.embedthis.com/pak)
