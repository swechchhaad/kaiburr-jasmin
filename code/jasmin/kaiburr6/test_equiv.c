//  *   gcc -fopenmp -O3 -Wall -I ref/include -I avx2/include 
//  *       -o test_equiv test_equiv.c ref/jkem.s avx2/jkem.s ref/randombytes.c
//  *   ./test_equiv          # default 10000 iters, or ./test_equiv 100000
//  */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/random.h>

#include "ref/include/api.h"
#include "avx2/include/api.h"

#define PK   JADE_KEM_mlkem_kaiburr6_amd64_ref_PUBLICKEYBYTES
#define SK   JADE_KEM_mlkem_kaiburr6_amd64_ref_SECRETKEYBYTES
#define CT   JADE_KEM_mlkem_kaiburr6_amd64_ref_CIPHERTEXTBYTES
#define SS   JADE_KEM_mlkem_kaiburr6_amd64_ref_BYTES
#define KPC  JADE_KEM_mlkem_kaiburr6_amd64_ref_KEYPAIRCOINBYTES
#define ENCC JADE_KEM_mlkem_kaiburr6_amd64_ref_ENCCOINBYTES

#ifndef NTESTS
#define NTESTS 10000
#endif

int main(int argc, char **argv)
{
  long ntests = (argc > 1) ? atol(argv[1]) : NTESTS;

  printf("kaiburr6 ref<->avx2 equivalence test\n");
  printf("PK=%d SK=%d CT=%d SS=%d  (KPC=%d ENCC=%d)\n", PK, SK, CT, SS, KPC, ENCC);
  printf("running %ld iterations...\n\n", ntests);

  long keygen_mismatch = 0;  /* pk or sk differ between ref and avx2  */
  long enc_mismatch    = 0;  /* ct or ss differ between ref and avx2  */
  long interop_fail    = 0;  /* cross-decaps produced the wrong ss    */
  long error_fail      = 0;  /* a primitive returned nonzero / rng    */

  #pragma omp parallel for schedule(dynamic, 64) \
          reduction(+:keygen_mismatch,enc_mismatch,interop_fail,error_fail)
  for (long t = 0; t < ntests; t++)
  {
    unsigned char kp_coins[KPC], enc_coins[ENCC];
    unsigned char pk_r[PK], sk_r[SK], ct_r[CT], ss_r[SS];
    unsigned char pk_a[PK], sk_a[SK], ct_a[CT], ss_a[SS];
    unsigned char ss_x[SS];

    if (getrandom(kp_coins, KPC, 0) != (ssize_t)KPC ||
        getrandom(enc_coins, ENCC, 0) != (ssize_t)ENCC) { error_fail++; continue; }

    /* keygen: same coins must give identical keypairs */
    if (jade_kem_mlkem_kaiburr6_amd64_ref_keypair_derand(pk_r, sk_r, kp_coins)  != 0) { error_fail++; continue; }
    if (jade_kem_mlkem_kaiburr6_amd64_avx2_keypair_derand(pk_a, sk_a, kp_coins) != 0) { error_fail++; continue; }
    if (memcmp(pk_r, pk_a, PK) != 0 || memcmp(sk_r, sk_a, SK) != 0) keygen_mismatch++;

    /* enc: same pk + coins must give identical ct and ss */
    if (jade_kem_mlkem_kaiburr6_amd64_ref_enc_derand(ct_r, ss_r, pk_r, enc_coins)  != 0) { error_fail++; continue; }
    if (jade_kem_mlkem_kaiburr6_amd64_avx2_enc_derand(ct_a, ss_a, pk_a, enc_coins) != 0) { error_fail++; continue; }
    if (memcmp(ct_r, ct_a, CT) != 0 || memcmp(ss_r, ss_a, SS) != 0) enc_mismatch++;

    /* cross-decaps interop: ref decaps avx2's ct, and vice versa */
    if (jade_kem_mlkem_kaiburr6_amd64_ref_dec(ss_x, ct_a, sk_r)  != 0) { error_fail++; continue; }
    if (memcmp(ss_x, ss_a, SS) != 0) interop_fail++;
    if (jade_kem_mlkem_kaiburr6_amd64_avx2_dec(ss_x, ct_r, sk_a) != 0) { error_fail++; continue; }
    if (memcmp(ss_x, ss_r, SS) != 0) interop_fail++;
  }

  printf("keygen mismatches (pk/sk differ):       %ld / %ld\n", keygen_mismatch, ntests);
  printf("enc mismatches (ct/ss differ):          %ld / %ld\n", enc_mismatch, ntests);
  printf("cross-decaps interop failures:          %ld / %ld\n", interop_fail, ntests);
  printf("primitive/rng errors:                   %ld / %ld\n", error_fail, ntests);

  if (keygen_mismatch || enc_mismatch || interop_fail || error_fail) {
    printf("\nFAILED: ref and avx2 are NOT equivalent.\n");
    return 1;
  }
  printf("\nAll %ld iterations passed: ref and avx2 agree byte-for-byte.\n", ntests);
  return 0;
}
