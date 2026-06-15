
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/random.h>

#include "../include/api.h"

#define SK_BYTES   JADE_KEM_mlkem_kaiburr6_amd64_avx2_SECRETKEYBYTES
#define PK_BYTES   JADE_KEM_mlkem_kaiburr6_amd64_avx2_PUBLICKEYBYTES
#define CT_BYTES   JADE_KEM_mlkem_kaiburr6_amd64_avx2_CIPHERTEXTBYTES
#define SS_BYTES   JADE_KEM_mlkem_kaiburr6_amd64_avx2_BYTES
#define KP_COINS   JADE_KEM_mlkem_kaiburr6_amd64_avx2_KEYPAIRCOINBYTES
#define ENC_COINS  JADE_KEM_mlkem_kaiburr6_amd64_avx2_ENCCOINBYTES

#ifndef NTESTS
#define NTESTS 10000
#endif

int main(int argc, char **argv)
{
  long ntests = NTESTS;
  if (argc > 1) ntests = atol(argv[1]);

  printf("kaiburr6 avx2 roundtrip test (k=18, NOISE_N=6)\n");
  printf("PK=%d SK=%d CT=%d SS=%d bytes\n", PK_BYTES, SK_BYTES, CT_BYTES, SS_BYTES);
  printf("running %ld iterations...\n\n", ntests);

  long roundtrip_fail = 0;  /* ss mismatch after a valid decapsulation */
  long reject_fail    = 0;  /* corrupted ct still produced the genuine ss */
  long error_fail     = 0;  /* a primitive returned nonzero, or rng failed */

  #pragma omp parallel for schedule(dynamic, 64) \
          reduction(+:roundtrip_fail,reject_fail,error_fail)
  for (long t = 0; t < ntests; t++)
  {
    unsigned char sk[SK_BYTES];
    unsigned char pk[PK_BYTES];
    unsigned char ct[CT_BYTES];
    unsigned char ss_enc[SS_BYTES];
    unsigned char ss_dec[SS_BYTES];
    unsigned char kp_coins[KP_COINS];
    unsigned char enc_coins[ENC_COINS];

    if (getrandom(kp_coins,  KP_COINS,  0) != (ssize_t)KP_COINS ||
        getrandom(enc_coins, ENC_COINS, 0) != (ssize_t)ENC_COINS) { error_fail++; continue; }

    if (jade_kem_mlkem_kaiburr6_amd64_avx2_keypair_derand(pk, sk, kp_coins) != 0) { error_fail++; continue; }
    if (jade_kem_mlkem_kaiburr6_amd64_avx2_enc_derand(ct, ss_enc, pk, enc_coins) != 0) { error_fail++; continue; }

    if (jade_kem_mlkem_kaiburr6_amd64_avx2_dec(ss_dec, ct, sk) != 0) { error_fail++; continue; }
    if (memcmp(ss_enc, ss_dec, SS_BYTES) != 0) roundtrip_fail++;

    ct[0] ^= 1;
    if (jade_kem_mlkem_kaiburr6_amd64_avx2_dec(ss_dec, ct, sk) != 0) { error_fail++; continue; }
    if (memcmp(ss_enc, ss_dec, SS_BYTES) == 0) reject_fail++;
  }

  printf("roundtrip failures (ss mismatch after valid dec): %ld / %ld\n", roundtrip_fail, ntests);
  printf("implicit-rejection failures (match after corrupt): %ld / %ld\n", reject_fail, ntests);
  printf("primitive/rng errors:                              %ld / %ld\n", error_fail, ntests);

  if (roundtrip_fail || reject_fail || error_fail) {
    printf("\nFAILED\n");
    return 1;
  }
  printf("\nAll %ld iterations passed.\n", ntests);
  return 0;
}
