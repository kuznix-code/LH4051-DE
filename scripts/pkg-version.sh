#!/bin/sh
# Prints the version used to name build artifacts.
#   tag build (refs/tags/vX.Y.Z)  ->  X.Y.Z
#   everything else               ->  <VERSION in src/longhorn.h>.<YYYYMMDD>.<short commit>
set -eu
cd "$(dirname "$0")/.."
base="$(sed -n 's/^#define LH4051_VERSION "\(.*\)"/\1/p' src/longhorn.h)"
if [ "${GITHUB_REF_TYPE:-}" = tag ]; then
    printf '%s\n' "${GITHUB_REF_NAME#v}"
    exit 0
fi
sha="$(printf '%s' "${LH4051_GIT_COMMIT:-$(git rev-parse --short=7 HEAD 2>/dev/null || echo nogit)}" | cut -c1-7)"
day="${LH4051_GIT_DATE:-$(git show -s --format=%cd --date=format:%Y%m%d HEAD 2>/dev/null || date -u +%Y%m%d)}"
printf '%s.%s.%s\n' "$base" "$day" "$sha"
