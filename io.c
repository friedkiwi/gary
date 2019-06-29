
#include <stdio.h>
#include "io.h"

void    io_out(unsigned char port, unsigned char value) {
    printf("OUT - port %0x, value %0x\n", port, value);
}

unsigned char io_in (unsigned char port) {
    printf("IN - port %0x\n", port);
    return 0;
}