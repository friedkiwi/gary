
#ifndef __i8080_h
#define __i8080_h



// Intel 8080 CPU emulation
// Loosely based on https://github.com/cbrooks90/i8080-emu



enum regs {
    B, C, D, E, H, L, PSW, A, SP
};

struct i8080 {
    //Registers. reg[6] not used by convention. IRL this is the PSW
    unsigned char reg[8];
    unsigned int stack_ptr;
    unsigned int prog_ctr;
    //Flags
    char carry;
    char aux_carry;
    char iszero;
    char parity;
    char sign;
};

void run(unsigned int address, struct i8080* cpu, unsigned char* mem);
#endif