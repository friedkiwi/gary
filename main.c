#include <stdio.h>
#include <stdlib.h>

#include "i8080.h"

int main() {
    unsigned char RAM[0x10000] = {0};
    struct i8080 cpu;
    struct i8080* cpu_p = &cpu;

    printf("IBM i i8080 Emulator\n");
//        printf("Written by friedkiwi - friedkiwi@qseco.fr\n");

    RAM[0] = 0x3c;
    RAM[1] = 0xd3;
    RAM[2] = 0x01;
    RAM[4] = 0x3c;
    RAM[5] = 0xd3;
    RAM[6] = 0x02;
    RAM[7] = 0x3c;
    RAM[8] = 0xd3;
    RAM[9] = 0x04;
    RAM[10] = 0x3c;
    RAM[11] = 0xd3;
    RAM[12] = 0x05;
    RAM[13] = 0xdb;
    RAM[14] = 0x05;
    RAM[15] = 0xd3;
    RAM[16] = 0x06;
    RAM[17] = 0x76;

    printf("starting execution...\n");
    run(0, cpu_p, RAM);
    printf("Done!\n");

    return 0;
}