#
#   patchVersion -- Patch pak.json and package.json version from ../pak.json
#

if [ -f package.json ] ; then
    npm --allow-same-version version $(cd .. ; pak version) >/dev/null
fi
pak edit version=$(cd .. ; pak version)
