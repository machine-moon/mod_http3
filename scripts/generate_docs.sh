#!/bin/bash

set -euo pipefail

cd "$(dirname "$0")/.."

echo "==> [1/3] Doxygen: C sources -> XML"
mkdir -p build/doxygen
doxygen docs/site/Doxyfile

echo "==> [2/3] doxybook2: XML -> Markdown (docs/site/pages/api)"
DOXYBOOK2="${DOXYBOOK2:-doxybook2}"
if ! command -v "$DOXYBOOK2" >/dev/null 2>&1 && [ -x "$HOME/.local/bin/doxybook2" ]; then
    DOXYBOOK2="$HOME/.local/bin/doxybook2"
fi
rm -rf docs/site/pages/api
mkdir -p docs/site/pages/api
"$DOXYBOOK2" --input build/doxygen/xml --output docs/site/pages/api --config docs/site/doxybook_config.json \
    --templates docs/site/doxybook-templates

find docs/site/pages/api -name '*.md' -exec sed -i 's/^```cpp$/```c/' {} +

find docs/site/pages/api -name '*.md' -exec sed -i 's/^## Classes$/## Structs/' {} +

find docs/site/pages/api -name '*.md' -exec sed -i -E 's/#(file|dir)-[^)]*\)/)/g' {} +

echo "==> [3/3] static site: Markdown -> docs/site/build/"
if command -v zensical >/dev/null 2>&1; then
    (cd docs/site && zensical build -f zensical.toml --clean --strict)
else
    ZENSICAL_SPEC="$(grep -m1 -E '^zensical' docs/site/requirements.txt || echo zensical)"
    (cd docs/site && uvx --from "$ZENSICAL_SPEC" zensical build -f zensical.toml --clean --strict)
fi

echo "Done. Preview with 'cd docs/site && zensical serve -f zensical.toml' or open docs/site/build/index.html."
