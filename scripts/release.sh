#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: scripts/release.sh [-t]"
    echo ""
    echo "Build the release artifacts into build-release/dist/. CPack writes a .sha256"
    echo "next to each one, and everything left in that directory is a release asset."
    echo ""
    echo "  -t, --tag           Tag the current commit vX.Y.Z. The tag is not pushed:"
    echo "                      \`git push origin vX.Y.Z\` is what triggers gh release"
    echo "  -h, --help          Show this help and exit."
    echo ""
    echo "See docs/release-process.md for the full workflow."
}

tag=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        -t | --tag) tag=1; shift ;;
        -h | --help) usage; exit 0 ;;
        *) echo "error: unknown option '$1'" >&2; usage >&2; exit 2 ;;
    esac
done

cd "$(git rev-parse --show-toplevel)"

if [[ $tag -eq 1 ]]; then
    version="$(sed -n 's/^project(mod_http3 VERSION \(.*\))$/\1/p' CMakeLists.txt)"
    [[ -n "$version" ]] \
        || { echo "error: no version found in CMakeLists.txt" >&2; exit 1; }
    git tag -a "v$version" -m "mod_http3 $version"
fi

cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -G Ninja
rm -rf build-release/dist
cmake --build build-release --target release -- -j"$(nproc)"
rm -rf build-release/dist/_CPack_Packages

ls -1 build-release/dist
