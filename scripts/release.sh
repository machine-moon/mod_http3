#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: scripts/release.sh"
    echo ""
    echo "Build the release artifacts into build-release/dist/. CPack writes a .sha256"
    echo "next to each one, and everything left in that directory is a release asset."
    echo ""
    echo "Tags vX.Y.Z from CMakeLists.txt once the artifacts are built, so a failed"
    echo "build leaves no tag behind. An existing tag is kept unless you say so. The"
    echo "tag is not pushed: \`git push origin vX.Y.Z\` is what triggers gh release."
    echo ""
    echo "  -h, --help          Show this help and exit."
    echo ""
    echo "See docs/release-process.md for the full workflow."
}

die() {
    echo "error: $1" >&2
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h | --help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown option '$1'" >&2
            usage >&2
            exit 2
            ;;
    esac
done

cd "$(git rev-parse --show-toplevel)"

version=$(sed -n 's/^project(mod_http3 VERSION \(.*\))$/\1/p' CMakeLists.txt)

if [[ -z $version ]]; then
    die "no version found in CMakeLists.txt"
fi

if [[ -n $(git status --porcelain) ]]; then
    die "working tree is dirty, commit before tagging v$version"
fi

force=()

if git rev-parse -q --verify "refs/tags/v$version" >/dev/null; then
    if [[ ! -t 0 ]]; then
        die "tag v$version already exists"
    fi

    read -r -p "tag v$version already exists. overwrite it? [y/N] " reply

    if [[ $reply != [Yy]* ]]; then
        echo "aborted, v$version left as it was" >&2
        exit 1
    fi

    force=(-f)
fi

cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -G Ninja

rm -rf build-release/dist

cmake --build build-release --target release -- -j"$(nproc)"

rm -rf build-release/dist/_CPack_Packages

git tag "${force[@]}" -a "v$version" -m "mod_http3 $version"

echo ""
echo "mod_http3 $version"

ls -1sh build-release/dist
