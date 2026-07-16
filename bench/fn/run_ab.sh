#!/bin/bash
# ONE driver (bench/src/crypto_kem.c), same librandombytes; builds C and Jasmin,
# ref and avx2, for each variant, all bound to that driver
set -u
ROOT="$HOME/kaiburr-jasmin"
BENCH="$ROOT/bench"; FN="$BENCH/fn"
CC="${CC:-cc}"; DRIVER="$BENCH/src/crypto_kem.c"; RB="$BENCH/randombytes/librandombytes1.a"
JC="${JASMINC:-$HOME/.opam/default/bin/jasminc}"; KECCAK="$ROOT/formosa-keccak/src/amd64"
VARIANTS="${VARIANTS:-kaiburr4:7:5472:2720:3072 kaiburr6:18:13920:6944:7296 kaiburr8:24:18528:9248:9600}"
IMPLS="${IMPLS:-ref avx2}"
TIMINGS="${TIMINGS:-10001}"
CFLAGS="-O3 -march=native -mtune=native -fomit-frame-pointer -fwrapv -DTIMINGS=$TIMINGS -Wno-implicit-function-declaration"
AVX="-mavx2 -mbmi2 -mpopcnt"
C_REF=(kem.c indcpa.c polyvec.c poly.c ntt.c fn.c reduce.c verify.c fips202.c symmetric-shake.c)
C_AVX2=(kem.c indcpa.c polyvec.c poly.c consts.c rejsample.c fn.c verify.c fips202.c fips202x4.c symmetric-shake.c keccak4x/KeccakP-1600-times4-SIMD256.c fq.S shuffle.S ntt.S invntt.S basemul.S)

make -C "$BENCH/randombytes" >/dev/null 2>&1
echo "host: $(hostname)  cpu: $(grep -m1 'model name' /proc/cpuinfo|cut -d: -f2|sed 's/^ //')  TIMINGS=$TIMINGS"

hdr=1
for row in $VARIANTS; do
  IFS=: read -r v k SK PK CT <<< "$row"
  for impl in $IMPLS; do
    w="$FN/ab_${v}_${impl}"; mkdir -p "$w/cbind"
    U="$(echo ${v}_${impl} | tr a-z A-Z)"; LC="${v}_${impl}"

    cat > "$w/cbind/api.h" <<EOF
#ifndef ${U}_API_H
#define ${U}_API_H
#include <stdint.h>
#define ${U}_SECRETKEYBYTES  $SK
#define ${U}_PUBLICKEYBYTES   $PK
#define ${U}_CIPHERTEXTBYTES  $CT
#define ${U}_KEYPAIRCOINBYTES 64
#define ${U}_ENCCOINBYTES     32
#define ${U}_BYTES            32
#define ${U}_ALGNAME "$v"
#define ${U}_ARCH    "amd64"
#define ${U}_IMPL    "${impl}"
int ${LC}_keypair(uint8_t*,uint8_t*);
int ${LC}_keypair_derand(uint8_t*,uint8_t*,const uint8_t*);
int ${LC}_enc(uint8_t*,uint8_t*,const uint8_t*);
int ${LC}_enc_derand(uint8_t*,uint8_t*,const uint8_t*,const uint8_t*);
int ${LC}_dec(uint8_t*,const uint8_t*,const uint8_t*);
#endif
EOF

    # C binary
    KCDIR="$ROOT/code/kaiburr-c/$impl"
    if [ "$impl" = avx2 ]; then names=("${C_AVX2[@]}"); xf="$AVX"; else names=("${C_REF[@]}"); xf=""; fi
    csrc=(); for f in "${names[@]}"; do csrc+=("$KCDIR/$f"); done
    $CC $CFLAGS $xf -DKYBER_K=$k -DJADE_NAMESPACE=$U -DJADE_NAMESPACE_LC=$LC \
        -I"$w/cbind" -I"$BENCH/randombytes" -I"$KCDIR" \
        "$DRIVER" "${csrc[@]}" "$RB" -o "$w/bench_c" 2>"$w/c.log" || { echo "!! $v $impl C build failed:"; tail -4 "$w/c.log"; }

    # Jasmin binary (rebuild jkem.s on this box for correct Linux symbols)
    JDIR="$ROOT/code/jasmin/$v/$impl"
    ( cd "$JDIR" && "$JC" -I Keccak="$KECCAK" -o "$w/jkem.s" jkem.jazz ) 2>"$w/jc.log" || { echo "!! $v $impl jasminc failed:"; tail -4 "$w/jc.log"; }
    [ -f "$w/jkem.s" ] && $CC $CFLAGS -DKYBER_K=$k -DJADE_NAMESPACE=JADE_KEM_mlkem_${v}_amd64_${impl} -DJADE_NAMESPACE_LC=jade_kem_mlkem_${v}_amd64_${impl} \
        -I"$JDIR/include" -I"$BENCH/randombytes" \
        "$DRIVER" "$w/jkem.s" "$RB" -o "$w/bench_j" 2>"$w/j.log" || { echo "!! $v $impl Jasmin build failed:"; tail -4 "$w/j.log"; }

    [ -x "$w/bench_c" ] && { "$w/bench_c" "8700K" "$v C      $impl" $hdr; hdr=0; }
    [ -x "$w/bench_j" ] &&   "$w/bench_j" "8700K" "$v Jasmin $impl" 0
  done
done
