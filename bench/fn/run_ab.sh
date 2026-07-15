set -u
ROOT="$HOME/kaiburr-jasmin"
BENCH="$ROOT/bench"; FN="$BENCH/fn"; KC="$ROOT/code/kaiburr-c/avx2"
CC="${CC:-cc}"; DRIVER="$BENCH/src/crypto_kem.c"; RB="$BENCH/randombytes/librandombytes1.a"
JC="${JASMINC:-$HOME/.opam/default/bin/jasminc}"; KECCAK="$ROOT/formosa-keccak/src/amd64"
VARIANTS="${VARIANTS:-kaiburr6:18:13920:6944:7296}"
TIMINGS="${TIMINGS:-1000}"
CFLAGS="-O3 -march=native -mtune=native -fomit-frame-pointer -fno-lto -DTIMINGS=$TIMINGS -Wno-implicit-function-declaration"
AVX="-mavx2 -mbmi2 -mpopcnt"
KCSRC=(kem.c indcpa.c polyvec.c poly.c consts.c rejsample.c fn.c verify.c fips202.c fips202x4.c symmetric-shake.c keccak4x/KeccakP-1600-times4-SIMD256.c fq.S shuffle.S ntt.S invntt.S basemul.S)

make -C "$BENCH/randombytes" >/dev/null 2>&1
echo "host: $(hostname)  cpu: $(grep -m1 'model name' /proc/cpuinfo|cut -d: -f2|sed 's/^ //')  TIMINGS=$TIMINGS"

hdr=1
for row in $VARIANTS; do
  IFS=: read -r v k SK PK CT <<< "$row"
  w="$FN/ab_$v"; mkdir -p "$w/cbind"
  U="$(echo "$v" | tr a-z A-Z)_AVX2"        # KAIBURR6_AVX2

  # C
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
#define ${U}_IMPL    "avx2c"
int ${v}_avx2_keypair(uint8_t*,uint8_t*);
int ${v}_avx2_keypair_derand(uint8_t*,uint8_t*,const uint8_t*);
int ${v}_avx2_enc(uint8_t*,uint8_t*,const uint8_t*);
int ${v}_avx2_enc_derand(uint8_t*,uint8_t*,const uint8_t*,const uint8_t*);
int ${v}_avx2_dec(uint8_t*,const uint8_t*,const uint8_t*);
#endif
EOF

  # C binary
  csrc=(); for f in "${KCSRC[@]}"; do csrc+=("$KC/$f"); done
  $CC $CFLAGS $AVX -DKYBER_K=$k -DJADE_NAMESPACE=$U -DJADE_NAMESPACE_LC=${v}_avx2 \
      -I"$w/cbind" -I"$BENCH/randombytes" -I"$KC" \
      "$DRIVER" "${csrc[@]}" "$RB" -o "$w/bench_c" 2>"$w/c.log" || { echo "!! $v C build failed:"; tail -4 "$w/c.log"; }

  ( cd "$ROOT/code/jasmin/$v/avx2" && "$JC" -I Keccak="$KECCAK" -o "$w/jkem.s" jkem.jazz ) 2>"$w/jc.log" \
     || { echo "!! $v jasminc failed:"; tail -4 "$w/jc.log"; }

  # Jasmin binary
  [ -f "$w/jkem.s" ] && $CC $CFLAGS -DKYBER_K=$k -DJADE_NAMESPACE=JADE_KEM_mlkem_${v}_amd64_avx2 -DJADE_NAMESPACE_LC=jade_kem_mlkem_${v}_amd64_avx2 \
      -I"$ROOT/code/jasmin/$v/avx2/include" -I"$BENCH/randombytes" \
      "$DRIVER" "$w/jkem.s" "$RB" -o "$w/bench_j" 2>"$w/j.log" || { echo "!! $v Jasmin build failed:"; tail -4 "$w/j.log"; }

  [ -x "$w/bench_c" ] && { "$w/bench_c" "8700K" "$v  C      avx2" $hdr; hdr=0; }
  [ -x "$w/bench_j" ] &&   "$w/bench_j" "8700K" "$v  Jasmin avx2" 0
done
