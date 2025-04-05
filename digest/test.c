#include "digest/sha2.h"
#include <unistd.h>
#include <string.h>

int main()
{
    struct sha256_state state = sha256_init;
    union digest_state * u = (union digest_state *) &state;
    const uint8_t * input = (const uint8_t *) "Lorem ipsum dolor sit amet, consectetur adipiscing elit viverra fusce.";
    size_t remain = 70;
    while(remain >= 64) {
        sha256_block(u, input);
        input += 64;
        remain -= 64;
    }
    uint8_t final_block[64];
    memcpy(final_block, input, remain);
    uint8_t d[32];
    sha256_final(u, final_block, remain, d);
    write(1, d, 32);
}
