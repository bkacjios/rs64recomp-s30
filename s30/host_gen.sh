#!/bin/sh
# Host-side code generation for the S30 cross build (the target can't run these tools).
# Needs the decomp ELF (../rogue_squadron64/build/roguesquadron.elf) and rogue_squadron.z64 in the repo root.
# Writes RecompiledFuncs/, RecompiledPatches/, build/factor5_ucode/*.c (fixed up), src/game/state_table.inl and s30/gen/.
set -e
cd "$(dirname "$0")/.."
HOST=build-tools
GEN=s30/gen
mkdir -p "$GEN" build/factor5_ucode build-s30-host/patches

# S30 runtime changes (memory size, retrace timeline, preemption points), kept
# here as a patch on the N64ModernRuntime submodule; applied once.
if ! git -C lib/N64ModernRuntime apply --reverse --check ../../s30/N64ModernRuntime.patch 2>/dev/null; then
	git -C lib/N64ModernRuntime apply ../../s30/N64ModernRuntime.patch
fi

if [ ! -x "$HOST/N64Recomp" ] || [ ! -x "$HOST/RSPRecomp" ]; then
	cmake -S lib/N64ModernRuntime/N64Recomp -B "$HOST" -G Ninja -DCMAKE_BUILD_TYPE=Release
	cmake --build "$HOST" --target N64RecompCLI RSPRecomp
fi

"$HOST/N64Recomp" rogue_squadron.toml
"$HOST/RSPRecomp" rsp/factor5_boot_rsp.toml
"$HOST/RSPRecomp" rsp/musyx_rsp.toml

c++ -std=c++17 -O1 -o build-s30-host/fixup_factor5_ucode tools/fixup_factor5_ucode.cpp
build-s30-host/fixup_factor5_ucode build/factor5_ucode/factor5_boot_recompiled.c build/factor5_ucode/musyx_audio_recompiled.c

c++ -std=c++17 -O1 -Ilib/N64ModernRuntime/N64Recomp/lib/tomlplusplus/include -o build-s30-host/gen_state_table tools/state/gen_state_table.cpp
build-s30-host/gen_state_table tools/state/state_model.toml src/game/state_table.inl

# Base-game patches: MIPS clang + ld.lld instead of a MIPS GCC.
MIPS_CFLAGS="--target=mips-linux-gnu -mips2 -mabi=32 -O2 -G0 -mno-abicalls -fno-pic -mno-odd-spreg -fomit-frame-pointer -fno-builtin -fno-optimize-sibling-calls -fno-stack-protector -nostdinc -I syms/include -Wall -Wextra -Wno-unused-parameter"
OBJS=""
for src in $(find patches/src -name '*.c' | sort); do
	obj="build-s30-host/patches/$(echo "${src#patches/src/}" | tr / _).o"
	clang $MIPS_CFLAGS -c "$src" -o "$obj"
	OBJS="$OBJS $obj"
done
ld.lld -nostdlib -T patches/patches.ld -T patches/syms.ld -Map build-s30-host/patches/patches.map \
	--unresolved-symbols=ignore-all --emit-relocs --noinhibit-exec -e 0 $OBJS -o patches/patches.elf
"$HOST/N64Recomp" patches.toml

c++ -std=c++17 -O1 -o build-s30-host/file_to_c lib/rt64/src/tools/file_to_c/file_to_c.cpp
build-s30-host/file_to_c patches/patches.bin rs64_patches_bin "$GEN/patches_bin.c" "$GEN/patches_bin.h"
echo "host_gen: done"
