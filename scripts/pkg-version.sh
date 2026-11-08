#!/bin/sh
# Prints the version used to name build artifacts.
#   tag build (refs/tags/vX.Y.Z)  ->  X.Y.Z
#   everything else               ->  <VERSION in src/longhorn.h>.<YYYYMMDD>
# The Git commit is recorded in the RPM Release field, not the Version.
set -eu
cd "$(dirname "$0")/.."
base="$(sed -n 's/^#define LH4051_VERSION "\(.*\)"/\1/p' src/longhorn.h)"
if [ "${GITHUB_REF_TYPE:-}" = tag ]; then
    printf '%s\n' "${GITHUB_REF_NAME#v}"
    exit 0
fi
day="${LH4051_GIT_DATE:-$(git show -s --format=%cd --date=format:%Y%m%d HEAD 2>/dev/null || date -u +%Y%m%d)}"
printf '%s.%s\n' "$base" "$day"
