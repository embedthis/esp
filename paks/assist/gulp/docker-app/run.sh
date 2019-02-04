#!/bin/bash
#
#   Run the web container
#
#   bin/web [debug]
#
#   The debug container does not contain the web pages. Rather they are mounted from the local directory.
#
DEBUG=$1
GULP=$2
IP=`ip`
PORT=9000
OWNER=embedthis
NAME=kickstart
unset CDPATH

function finish {
    echo "Cleanup, ..."
    if [ "${ID}" != "" ] ; then
        docker stop ${ID}
        docker rmi -f ${ID}
    else
        docker stop ${NAME}
        docker rmi -f ${NAME}
    fi
}
trap finish EXIT

if [ ! -f pak.json -a ! -f package.json ] ; then
    echo "Must run from top directory"
    exit 1
fi

echo "${NAME} exposed on port ${PORT}"
echo docker run -d --name ${NAME} -p ${PORT}:80 ${OWNER}/${NAME}
ID=$(docker run -d --name ${NAME} -p ${PORT}:80 ${OWNER}/${NAME})
echo Started ${ID}
docker wait ${ID}
