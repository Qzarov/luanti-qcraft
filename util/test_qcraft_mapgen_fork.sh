#!/bin/sh
# Static contract for the independent QCraft mapgen fork.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

test -f "$root/src/mapgen/mapgen_qcraft.cpp"
test -f "$root/src/mapgen/mapgen_qcraft.h"
grep -Fq 'class MapgenQCraft' "$root/src/mapgen/mapgen_qcraft.h"
grep -Fq 'struct MapgenQCraftParams' "$root/src/mapgen/mapgen_qcraft.h"
grep -Fq 'MAPGEN_QCRAFT' "$root/src/mapgen/mapgen.h"
grep -Fq '{"qcraft",' "$root/src/mapgen/mapgen.cpp"
grep -Fq '#include "mapgen_qcraft.h"' "$root/src/mapgen/mapgen.cpp"
grep -Fq 'new MapgenQCraft((MapgenQCraftParams *)params, emerge)' "$root/src/mapgen/mapgen.cpp"
grep -Fq 'new MapgenQCraftParams' "$root/src/mapgen/mapgen.cpp"
grep -Fq 'mapgen_qcraft.cpp' "$root/src/mapgen/CMakeLists.txt"
grep -Fq 'qcraft' "$root/builtin/settingtypes.txt"
grep -Fq '[*Mapgen QCraft]' "$root/builtin/settingtypes.txt"
grep -Fq 'mgqcraft_spflags' "$root/builtin/settingtypes.txt"

echo 'QCraft mapgen fork registration is complete.'
