MusyX v1/v2 audio microcode in C, from mupen64plus-rsp-hle (https://github.com/mupen64plus/mupen64plus-rsp-hle, commit dd09c7dc, GPL-2.0-or-later, see LICENSES).
`musyx.c`, `audio.[ch]`, `memory.[ch]`, `arithmetics.h`, `common.h` and `hle_external.h` are unmodified; `hle_internal.h` is cut down to what `musyx.c` uses.
The S30 build runs it instead of the RSPRecomp'd synth, which is too slow on the Cortex-A7 (s30_musyx.cpp).
