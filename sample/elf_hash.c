#include <stddef.h>
#include <stdint.h>

uint32_t elf_hash(const unsigned char *name) {
  uint32_t h = 0;
  uint32_t g;

  while (*name) {
    h = (h << 4) + *name++;
    g = h & 0xF0000000;
    if (g) {
      h ^= g >> 24;
    }
    h &= ~g;
  }

  return h;
}
