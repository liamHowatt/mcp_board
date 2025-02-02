#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

int main() {
    uint8_t x = 200;
    uint8_t y = 100;
    uint32_t res = x + y;
    uint8_t z = 255;
    int bigger = res > z;
    printf("%"PRId32" %d\n", res, bigger);
}
