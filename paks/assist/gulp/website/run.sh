#!/bin/bash
#
#   Run the web site
#
#   bin/run [PROFILE]
#
PROFILE=$1
: ${PROFILE:=dev}

pak profile ${PROFILE}
expansive
