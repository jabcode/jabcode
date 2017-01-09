#include "pseudo_random.h"

static uint64_t lcg64_seed = 42;

uint32_t temper(uint32_t x)
{
    x ^= x>>11;
    x ^= x<<7 & 0x9D2C5680;
    x ^= x<<15 & 0xEFC60000;
    x ^= x>>18;
    return x;
}

uint32_t lcg64_temper()
{
    lcg64_seed = 6364136223846793005ULL * lcg64_seed + 1;
    return temper(lcg64_seed >> 32);
}

void setSeed(uint64_t seed)
{
	lcg64_seed = seed;
}
