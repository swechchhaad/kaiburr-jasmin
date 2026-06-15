
#include <stdint.h>
#include <sys/random.h>

void __jasmin_syscall_randombytes__(uint8_t *out, uint64_t len)
{
  getrandom(out, len, 0);
}
