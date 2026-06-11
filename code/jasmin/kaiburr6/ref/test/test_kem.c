
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../include/api.h"

#define SK_BYTES   JADE_KEM_mlkem_mlkem1024_amd64_ref_SECRETKEYBYTES
#define PK_BYTES   JADE_KEM_mlkem_mlkem1024_amd64_ref_PUBLICKEYBYTES
#define CT_BYTES   JADE_KEM_mlkem_mlkem1024_amd64_ref_CIPHERTEXTBYTES
#define SS_BYTES   JADE_KEM_mlkem_mlkem1024_amd64_ref_BYTES
#define KP_COINS   JADE_KEM_mlkem_mlkem1024_amd64_ref_KEYPAIRCOINBYTES
#define ENC_COINS  JADE_KEM_mlkem_mlkem1024_amd64_ref_ENCCOINBYTES

#define NTESTS 10

int main(void)
{
  unsigned char sk[SK_BYTES];
  unsigned char pk[PK_BYTES];
  unsigned char ct[CT_BYTES];
  unsigned char ss_enc[SS_BYTES];
  unsigned char ss_dec[SS_BYTES];
  unsigned char kp_coins[KP_COINS];
  unsigned char enc_coins[ENC_COINS];
  int ret;

  FILE *urandom = fopen("/dev/urandom", "r");
  if (!urandom) { printf("error: could not open /dev/urandom\n"); return -1; }

  printf("kaiburr6 ref roundtrip test (k=18, NOISE_N=6)\n");
  printf("PK=%d SK=%d CT=%d SS=%d bytes\n\n", PK_BYTES, SK_BYTES, CT_BYTES, SS_BYTES);

  for (int t = 0; t < NTESTS; t++)
  {
    fread(kp_coins,  KP_COINS,  1, urandom);
    fread(enc_coins, ENC_COINS, 1, urandom);

    /* --- keygen --- */
    ret = jade_kem_mlkem_mlkem1024_amd64_ref_keypair_derand(pk, sk, kp_coins);
    if (ret != 0) { printf("test %d: keypair failed (ret=%d)\n", t, ret); fclose(urandom); return -1; }

    /* --- encapsulate --- */
    ret = jade_kem_mlkem_mlkem1024_amd64_ref_enc_derand(ct, ss_enc, pk, enc_coins);
    if (ret != 0) { printf("test %d: enc failed (ret=%d)\n", t, ret); fclose(urandom); return -1; }

    /* --- decapsulate (should succeed) --- */
    ret = jade_kem_mlkem_mlkem1024_amd64_ref_dec(ss_dec, ct, sk);
    if (ret != 0) { printf("test %d: dec failed (ret=%d)\n", t, ret); fclose(urandom); return -1; }

    if (memcmp(ss_enc, ss_dec, SS_BYTES) != 0) {
      printf("test %d: FAIL — shared secrets do not match after successful dec\n", t);
      fclose(urandom); return -1;
    }

    /* --- decapsulate corrupted ciphertext (implicit rejection) --- */
    ct[0] ^= 1;
    ret = jade_kem_mlkem_mlkem1024_amd64_ref_dec(ss_dec, ct, sk);
    if (ret != 0) { printf("test %d: dec (corrupt) failed (ret=%d)\n", t, ret); fclose(urandom); return -1; }

    if (memcmp(ss_enc, ss_dec, SS_BYTES) == 0) {
      printf("test %d: FAIL — shared secrets match after corrupted ct (implicit rejection broken)\n", t);
      fclose(urandom); return -1;
    }

    printf("test %d: OK\n", t);
  }

  fclose(urandom);
  printf("\nAll %d tests passed.\n", NTESTS);
  return 0;
}
