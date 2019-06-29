#include <stdio.h>

#include "i8080.h"
#include "io.h"

char par_tab[256] =
        {
                1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
                0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
                0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
                1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
                0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,
                1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
                1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1,0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,
                0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0,1,0,0,1,0,1,1,0,0,1,1,0,1,0,0,1
        };

//Update zero, sign, parity flags based on argument byte
void zsp_flags(unsigned char byte, struct i8080* p) {
    if (byte == 0)
    {
        p->iszero = 1;
        p->sign = 0;
    }
    else if (byte >= 0x80)
    {
        p->iszero = 0;
        p->sign = 0;
    }
    else
    {
        p->iszero = 0;
        p->sign = 1;
    }
    p->parity = par_tab[byte];
}

void increment(unsigned char * regm, struct i8080* cpu) {
    *regm += 1;
    zsp_flags(*regm, cpu);
}

void decrement(unsigned char * regm, struct i8080* cpu) {
    *regm -= 1;
    zsp_flags(*regm, cpu);
}

void out(unsigned char port, struct i8080* cpu) {
    io_out(port, cpu->reg[A]);
}
void in(unsigned char port, struct i8080* cpu) {
    cpu->reg[A] = io_in(port);
}

void rotate(char left, char throughcarry, struct i8080* cpu) {
    char tempcarry = cpu->carry;
    if (left)
    {
        cpu->carry = (cpu->reg)[A] / 0x80;
        (cpu->reg)[A] <<= 1;
        (cpu->reg)[A] %= 0x100;
        if (throughcarry)
            (cpu->reg)[A] += tempcarry;
        else
            (cpu->reg)[A] += cpu->carry;
    }
    else
    {
        cpu->carry = (cpu->reg)[A] % 2;
        (cpu->reg)[A] >>= 1;
        if (throughcarry)
            (cpu->reg)[A] += 0x80 * tempcarry;
        else
            (cpu->reg)[A] += 0x80 * cpu->carry;
    }
}

void add(unsigned char regm, struct i8080* cpu, char shouldicarry) {
    unsigned char res = (cpu->reg)[A] + regm;
    if (shouldicarry)
        res += cpu->carry;
    res %= 0x100;
    if (res < (cpu->reg)[A] || res < regm)
        cpu->carry = 1;
    else
        cpu->carry = 0;
    zsp_flags(res, cpu);
    (cpu->reg)[A] = res;
}

void sub(unsigned char regm, struct i8080* cpu, char shouldiborrow) {
    unsigned char res;
    if (shouldiborrow)
    {
        regm +=1;
        regm %= 0x100;
    }
    if ((cpu->reg)[A] < regm)
        cpu->carry = 0;
    else
        cpu->carry = 1;
    res = 0x100 - regm;
    res += (cpu->reg)[A];
    res %= 0x100;
    zsp_flags(res, cpu);
    (cpu->reg)[A] = res;
}

enum log_op {bw_and, bw_xor, bw_or};
void logic(enum log_op sw, unsigned char regm, struct i8080* cpu) {
    switch (sw) {
        case bw_and: (cpu->reg)[A] &= regm;
        case bw_xor: (cpu->reg)[A] ^= regm;
        case bw_or: (cpu->reg)[A] |= regm;
    }
    cpu->carry = 0;
    zsp_flags((cpu->reg)[A], cpu);
}

void cmp(unsigned char regm, struct i8080* cpu) {
    unsigned char res;
    if ((cpu->reg)[A] < regm)
        cpu->carry = 1;
    else
        cpu->carry = 0;
    res = 0x100 - regm;
    res += (cpu->reg)[A];
    res %= 0x100;
    zsp_flags(res, cpu);
}

void doubleinr(enum regs R, struct i8080* cpu) {
    if (R == SP) {
        if (cpu->stack_ptr == 0xFFFF) {cpu->stack_ptr = 0; return;}
        ++cpu->stack_ptr; return;
    }
    else if (R == B || R == D || R == H) {
        if ((cpu->reg)[R+1] == 0xFF) {
            cpu->reg[R+1] = 0;
            if ((cpu->reg)[R] == 0xFF) {cpu->reg[R] = 0; return;}
            ++cpu->reg[R]; return;
        }
        ++(cpu->reg)[R+1]; return;
    }
}

void doubledcr(enum regs R, struct i8080* cpu) {
    if (R == SP) {
        if (cpu->stack_ptr == 0) {cpu->stack_ptr = 0xFFFF; return;}
        --cpu->stack_ptr; return;
    }
    else if (R == B || R == D || R == H) {
        if ((cpu->reg)[R+1] == 0) {
            cpu->reg[R+1] = 0xFF;
            if ((cpu->reg)[R] == 0) {cpu->reg[R] = 0xFF; return;}
            --cpu->reg[R]; return;
        }
        --(cpu->reg)[R+1]; return;
    }
}

void xthl(struct i8080* cpu, unsigned char* mem) {
    unsigned char temp = (cpu->reg)[H];
    (cpu->reg)[H] = mem[(cpu->stack_ptr)+1];
    mem[(cpu->stack_ptr)+1] = temp;
    temp = (cpu->reg)[L];
    (cpu->reg)[L] = mem[cpu->stack_ptr];
    mem[cpu->stack_ptr] = temp;
    return;
}

void xchg(struct i8080* cpu) {
    unsigned char temp = (cpu->reg)[H];
    (cpu->reg)[H] = (cpu->reg)[D];
    (cpu->reg)[D] = temp;
    temp = (cpu->reg)[L];
    (cpu->reg)[L] = (cpu->reg)[E];
    (cpu->reg)[E] = temp;
    return;
}

void doubleadd(enum regs R, struct i8080* cpu) {
    unsigned int targ = 0x100*(cpu->reg)[H] + (cpu->reg)[L];
    unsigned int summand;
    if (R == SP)
        summand = cpu->stack_ptr;
    else if (R == B || R == D || R == H)
        summand = 0x100*(cpu->reg)[R] + (cpu->reg)[R+1];
    targ += summand;
    cpu->carry = targ / 0x10000; targ %= 0x10000;
    (cpu->reg)[H] = targ / 0x100; (cpu->reg)[L] = targ % 0x100;
    return;
}

unsigned int call(unsigned int ret, unsigned int jmp,
                  struct i8080* cpu, unsigned char* mem) {
    mem[(cpu->stack_ptr) - 1] = ret/0x100;
    mem[(cpu->stack_ptr) - 2] = ret%0x100;
    cpu->stack_ptr -= 2;
    return jmp;
}

unsigned int ret(struct i8080* cpu, unsigned char* mem) {
    cpu->stack_ptr += 2;
    return 0x100*mem[(cpu->stack_ptr)-1] + mem[cpu->stack_ptr-2];
}

void push(enum regs R, struct i8080* cpu, unsigned char* mem) {
    mem[(cpu->stack_ptr) - 1] = (cpu->reg)[R];
    if (R == A) {
        mem[(cpu->stack_ptr) - 2] =
                (cpu->carry)//Least signif. bit of PSW is carry
                + 0x02//2nd bit is always on
                + 0x04 * (cpu->parity)//3rd bit parity, 4th always off
                + 0x10 * (cpu->aux_carry)//5th aux_carry, 6th always off
                + 0x40 * (cpu->iszero)//7th bit is zero bit
                + 0x80 * (cpu->sign);//8th (most signif.) bit is sign
    }
    else if (R == B || R == D || R == H)
        mem[(cpu->stack_ptr) - 2] = (cpu->reg)[R+1];
    cpu->stack_ptr -= 2;
    return;
}

void pop(enum regs R, struct i8080* cpu, unsigned char* mem) {
    (cpu->reg)[R] = mem[(cpu->stack_ptr)+1];
    if (R == A) {
        unsigned char psw = mem[cpu->stack_ptr];
        cpu->carry = psw%2;
        psw >>= 2; cpu->parity    = psw%2;
        psw >>= 2; cpu->aux_carry = psw%2;
        psw >>= 2; cpu->iszero    = psw%2;
        psw >>= 1; cpu->sign      = psw%2;
    }
    else if (R == B || R == D || R == H)
        (cpu->reg)[R+1] = mem[cpu->stack_ptr];
    cpu->stack_ptr += 2;
    return;
}

short int exec_inst(struct i8080* cpu, unsigned char* mem) {
    unsigned int p = cpu->prog_ctr;
    unsigned char opcode = mem[p];
    unsigned int dest = 0x100 * (cpu->reg)[H] + (cpu->reg)[L];
    unsigned int d8 = mem[p+1];//8-bit data or least sig. part of 16-bit data
    unsigned int d16 = mem[p+2];//Most sig. part of 16-bit data
    unsigned int da = 0x100 * d16 + d8;//Use if 16 bit data refers to an address
    switch (opcode) {
        //LXI(dest, d16)
        case 0x01: (cpu->reg)[B] = d16; (cpu->reg)[C] = d8; return p+3;
        case 0x11: (cpu->reg)[D] = d16; (cpu->reg)[E] = d8; return p+3;
        case 0x21: (cpu->reg)[H] = d16; (cpu->reg)[L] = d8; return p+3;
        case 0x31: cpu->stack_ptr = 0x100*d16 + d8; return p+3;
            //Direct addressing: STA, LDA, SHLD, LHLD
        case 0x32: mem[da] = (cpu->reg)[A]; return p+3;
        case 0x3a: (cpu->reg)[A] = mem[da]; return p+3;
        case 0x22: mem[da] = (cpu->reg)[L]; mem[da+1] = (cpu->reg)[H]; return p+3;
        case 0x2a: (cpu->reg)[L] = mem[da]; (cpu->reg)[H] = mem[da+1]; return p+3;
            //STAX, LDAX
        case 0x02: mem[0x100*(cpu->reg)[B]+(cpu->reg)[C]]=(cpu->reg)[A]; return p+1;
        case 0x12: mem[0x100*(cpu->reg)[D]+(cpu->reg)[E]]=(cpu->reg)[A]; return p+1;
        case 0x0a: (cpu->reg)[A]=mem[0x100*(cpu->reg)[B]+(cpu->reg)[C]]; return p+1;
        case 0x1a: (cpu->reg)[A]=mem[0x100*(cpu->reg)[D]+(cpu->reg)[E]]; return p+1;
            //MVI(dest, d8)
        case 0x06: (cpu->reg)[B] = d8; return p+2;
        case 0x16: (cpu->reg)[D] = d8; return p+2;
        case 0x26: (cpu->reg)[H] = d8; return p+2;
        case 0x36: mem[dest] = d8; return p+2;
        case 0x0e: (cpu->reg)[C] = d8; return p+2;
        case 0x1e: (cpu->reg)[E] = d8; return p+2;
        case 0x2e: (cpu->reg)[L] = d8; return p+2;
        case 0x3e: (cpu->reg)[A] = d8; return p+2;
            //MOV(dest, src)
        case 0x40: return p+1;
        case 0x41: (cpu->reg)[B] = (cpu->reg)[C]; return p+1;
        case 0x42: (cpu->reg)[B] = (cpu->reg)[D]; return p+1;
        case 0x43: (cpu->reg)[B] = (cpu->reg)[E]; return p+1;
        case 0x44: (cpu->reg)[B] = (cpu->reg)[H]; return p+1;
        case 0x45: (cpu->reg)[B] = (cpu->reg)[L]; return p+1;
        case 0x46: (cpu->reg)[B] = mem[dest]; return p+1;
        case 0x47: (cpu->reg)[B] = (cpu->reg)[A]; return p+1;
        case 0x48: (cpu->reg)[C] = (cpu->reg)[B]; return p+1;
        case 0x49: return p+1;
        case 0x4a: (cpu->reg)[C] = (cpu->reg)[D]; return p+1;
        case 0x4b: (cpu->reg)[C] = (cpu->reg)[E]; return p+1;
        case 0x4c: (cpu->reg)[C] = (cpu->reg)[H]; return p+1;
        case 0x4d: (cpu->reg)[C] = (cpu->reg)[L]; return p+1;
        case 0x4e: (cpu->reg)[C] = mem[dest]; return p+1;
        case 0x4f: (cpu->reg)[C] = (cpu->reg)[A]; return p+1;
        case 0x50: (cpu->reg)[D] = (cpu->reg)[B]; return p+1;
        case 0x51: (cpu->reg)[D] = (cpu->reg)[C]; return p+1;
        case 0x52: return p+1;
        case 0x53: (cpu->reg)[D] = (cpu->reg)[E]; return p+1;
        case 0x54: (cpu->reg)[D] = (cpu->reg)[H]; return p+1;
        case 0x55: (cpu->reg)[D] = (cpu->reg)[L]; return p+1;
        case 0x56: (cpu->reg)[D] = mem[dest]; return p+1;
        case 0x57: (cpu->reg)[D] = (cpu->reg)[A]; return p+1;
        case 0x58: (cpu->reg)[E] = (cpu->reg)[B]; return p+1;
        case 0x59: (cpu->reg)[E] = (cpu->reg)[C]; return p+1;
        case 0x5a: (cpu->reg)[E] = (cpu->reg)[D]; return p+1;
        case 0x5b: return p+1;
        case 0x5c: (cpu->reg)[E] = (cpu->reg)[H]; return p+1;
        case 0x5d: (cpu->reg)[E] = (cpu->reg)[L]; return p+1;
        case 0x5e: (cpu->reg)[E] = mem[dest]; return p+1;
        case 0x5f: (cpu->reg)[E] = (cpu->reg)[A]; return p+1;
        case 0x60: (cpu->reg)[H] = (cpu->reg)[B]; return p+1;
        case 0x61: (cpu->reg)[H] = (cpu->reg)[C]; return p+1;
        case 0x62: (cpu->reg)[H] = (cpu->reg)[D]; return p+1;
        case 0x63: (cpu->reg)[H] = (cpu->reg)[E]; return p+1;
        case 0x64: return p+1;
        case 0x65: (cpu->reg)[H] = (cpu->reg)[L]; return p+1;
        case 0x66: (cpu->reg)[H] = mem[dest]; return p+1;
        case 0x67: (cpu->reg)[H] = (cpu->reg)[A]; return p+1;
        case 0x68: (cpu->reg)[L] = (cpu->reg)[B]; return p+1;
        case 0x69: (cpu->reg)[L] = (cpu->reg)[C]; return p+1;
        case 0x6a: (cpu->reg)[L] = (cpu->reg)[D]; return p+1;
        case 0x6b: (cpu->reg)[L] = (cpu->reg)[E]; return p+1;
        case 0x6c: (cpu->reg)[L] = (cpu->reg)[H]; return p+1;
        case 0x6d: return p+1;
        case 0x6e: (cpu->reg)[L] = mem[dest]; return p+1;
        case 0x6f: (cpu->reg)[L] = (cpu->reg)[A]; return p+1;
        case 0x70: mem[dest] = (cpu->reg)[B]; return p+1;
        case 0x71: mem[dest] = (cpu->reg)[C]; return p+1;
        case 0x72: mem[dest] = (cpu->reg)[D]; return p+1;
        case 0x73: mem[dest] = (cpu->reg)[E]; return p+1;
        case 0x74: mem[dest] = (cpu->reg)[H]; return p+1;
        case 0x75: mem[dest] = (cpu->reg)[L]; return p+1;
        case 0x76: return p;//halt
        case 0x77: mem[dest] = (cpu->reg)[A]; return p+1;
        case 0x78: (cpu->reg)[A] = (cpu->reg)[B]; return p+1;
        case 0x79: (cpu->reg)[A] = (cpu->reg)[C]; return p+1;
        case 0x7a: (cpu->reg)[A] = (cpu->reg)[D]; return p+1;
        case 0x7b: (cpu->reg)[A] = (cpu->reg)[E]; return p+1;
        case 0x7c: (cpu->reg)[A] = (cpu->reg)[H]; return p+1;
        case 0x7d: (cpu->reg)[A] = (cpu->reg)[L]; return p+1;
        case 0x7e: (cpu->reg)[A] = mem[dest]; return p+1;
        case 0x7f: return p+1;
            //Increment/decrement
        case 0x04: increment(&(cpu->reg)[B], cpu); return p+1;
        case 0x0c: increment(&(cpu->reg)[C], cpu); return p+1;
        case 0x14: increment(&(cpu->reg)[D], cpu); return p+1;
        case 0x1c: increment(&(cpu->reg)[E], cpu); return p+1;
        case 0x24: increment(&(cpu->reg)[H], cpu); return p+1;
        case 0x2c: increment(&(cpu->reg)[L], cpu); return p+1;
        case 0x34: increment(&mem[dest], cpu); return p+1;
        case 0x3c: increment(&(cpu->reg)[A], cpu); return p+1;
        case 0x05: decrement(&(cpu->reg)[B], cpu); return p+1;
        case 0x0d: decrement(&(cpu->reg)[C], cpu); return p+1;
        case 0x15: decrement(&(cpu->reg)[D], cpu); return p+1;
        case 0x1d: decrement(&(cpu->reg)[E], cpu); return p+1;
        case 0x25: decrement(&(cpu->reg)[H], cpu); return p+1;
        case 0x2d: decrement(&(cpu->reg)[L], cpu); return p+1;
        case 0x35: decrement(&mem[dest], cpu); return p+1;
        case 0x3d: decrement(&(cpu->reg)[A], cpu); return p+1;
            //Rotate
        case 0x07: rotate(1, 0, cpu); return p+1;
        case 0x17: rotate(1, 1, cpu); return p+1;
        case 0x0f: rotate(0, 0, cpu); return p+1;
        case 0x1f: rotate(0, 1, cpu); return p+1;
        case 0x27: return p+1;//Here be dragons
        case 0x2f: (cpu->reg)[A] = ~(cpu->reg)[A]; return p+1;
        case 0x37: cpu->carry = 1; return p+1;
        case 0x3f: cpu->carry = !(cpu->carry); return p+1;
            //16-bit inc/dec
        case 0x03: doubleinr(B, cpu); return p+1;
        case 0x13: doubleinr(D, cpu); return p+1;
        case 0x23: doubleinr(H, cpu); return p+1;
        case 0x33: doubleinr(SP, cpu); return p+1;
        case 0x0b: doubledcr(B, cpu); return p+1;
        case 0x1b: doubledcr(D, cpu); return p+1;
        case 0x2b: doubledcr(H, cpu); return p+1;
        case 0x3b: doubledcr(SP, cpu); return p+1;
            //XTHL, XCHG, SPHL
        case 0xe3: xthl(cpu, mem); return p+1;
        case 0xeb: xchg(cpu); return p+1;
        case 0xf9: cpu->stack_ptr = 0x100*(cpu->reg)[H]+(cpu->reg)[L]; return p+1;
            //Arith/logic
        case 0x80: add((cpu->reg)[B], cpu, 0); return p+1;
        case 0x81: add((cpu->reg)[C], cpu, 0); return p+1;
        case 0x82: add((cpu->reg)[D], cpu, 0); return p+1;
        case 0x83: add((cpu->reg)[E], cpu, 0); return p+1;
        case 0x84: add((cpu->reg)[H], cpu, 0); return p+1;
        case 0x85: add((cpu->reg)[L], cpu, 0); return p+1;
        case 0x86: add(mem[dest], cpu, 0); return p+1;
        case 0x87: add((cpu->reg)[A], cpu, 0); return p+1;
        case 0x88: add((cpu->reg)[B], cpu, 1); return p+1;
        case 0x89: add((cpu->reg)[C], cpu, 1); return p+1;
        case 0x8a: add((cpu->reg)[D], cpu, 1); return p+1;
        case 0x8b: add((cpu->reg)[E], cpu, 1); return p+1;
        case 0x8c: add((cpu->reg)[H], cpu, 1); return p+1;
        case 0x8d: add((cpu->reg)[L], cpu, 1); return p+1;
        case 0x8e: add(mem[dest], cpu, 1); return p+1;
        case 0x8f: add((cpu->reg)[A], cpu, 1); return p+1;
        case 0x90: sub((cpu->reg)[B], cpu, 0); return p+1;
        case 0x91: sub((cpu->reg)[C], cpu, 0); return p+1;
        case 0x92: sub((cpu->reg)[D], cpu, 0); return p+1;
        case 0x93: sub((cpu->reg)[E], cpu, 0); return p+1;
        case 0x94: sub((cpu->reg)[H], cpu, 0); return p+1;
        case 0x95: sub((cpu->reg)[L], cpu, 0); return p+1;
        case 0x96: sub(mem[dest], cpu, 0); return p+1;
        case 0x97: sub((cpu->reg)[A], cpu, 0); return p+1;
        case 0x98: sub((cpu->reg)[B], cpu, 1); return p+1;
        case 0x99: sub((cpu->reg)[C], cpu, 1); return p+1;
        case 0x9a: sub((cpu->reg)[D], cpu, 1); return p+1;
        case 0x9b: sub((cpu->reg)[E], cpu, 1); return p+1;
        case 0x9c: sub((cpu->reg)[H], cpu, 1); return p+1;
        case 0x9d: sub((cpu->reg)[L], cpu, 1); return p+1;
        case 0x9e: sub(mem[dest], cpu, 1); return p+1;
        case 0x9f: sub((cpu->reg)[A], cpu, 1); return p+1;
        case 0xa0: logic(bw_and, (cpu->reg)[B], cpu); return p+1;
        case 0xa1: logic(bw_and, (cpu->reg)[C], cpu); return p+1;
        case 0xa2: logic(bw_and, (cpu->reg)[D], cpu); return p+1;
        case 0xa3: logic(bw_and, (cpu->reg)[E], cpu); return p+1;
        case 0xa4: logic(bw_and, (cpu->reg)[H], cpu); return p+1;
        case 0xa5: logic(bw_and, (cpu->reg)[L], cpu); return p+1;
        case 0xa6: logic(bw_and, mem[dest], cpu); return p+1;
        case 0xa7: logic(bw_and, (cpu->reg)[A], cpu); return p+1;
        case 0xa8: logic(bw_xor, (cpu->reg)[B], cpu); return p+1;
        case 0xa9: logic(bw_xor, (cpu->reg)[C], cpu); return p+1;
        case 0xaa: logic(bw_xor, (cpu->reg)[D], cpu); return p+1;
        case 0xab: logic(bw_xor, (cpu->reg)[E], cpu); return p+1;
        case 0xac: logic(bw_xor, (cpu->reg)[H], cpu); return p+1;
        case 0xad: logic(bw_xor, (cpu->reg)[L], cpu); return p+1;
        case 0xae: logic(bw_xor, mem[dest], cpu); return p+1;
        case 0xaf: logic(bw_xor, (cpu->reg)[A], cpu); return p+1;
        case 0xb0: logic(bw_or, (cpu->reg)[B], cpu); return p+1;
        case 0xb1: logic(bw_or, (cpu->reg)[C], cpu); return p+1;
        case 0xb2: logic(bw_or, (cpu->reg)[D], cpu); return p+1;
        case 0xb3: logic(bw_or, (cpu->reg)[E], cpu); return p+1;
        case 0xb4: logic(bw_or, (cpu->reg)[H], cpu); return p+1;
        case 0xb5: logic(bw_or, (cpu->reg)[L], cpu); return p+1;
        case 0xb6: logic(bw_or, mem[dest], cpu); return p+1;
        case 0xb7: logic(bw_or, (cpu->reg)[A], cpu); return p+1;
        case 0xb8: cmp((cpu->reg)[B], cpu); return p+1;
        case 0xb9: cmp((cpu->reg)[C], cpu); return p+1;
        case 0xba: cmp((cpu->reg)[D], cpu); return p+1;
        case 0xbb: cmp((cpu->reg)[E], cpu); return p+1;
        case 0xbc: cmp((cpu->reg)[H], cpu); return p+1;
        case 0xbd: cmp((cpu->reg)[L], cpu); return p+1;
        case 0xbe: cmp(mem[dest], cpu); return p+1;
        case 0xbf: cmp((cpu->reg)[A], cpu); return p+1;
            //ADI, ADC, SUI, SBI, ANI, XRI, ORI, CPI
        case 0xc6: add(d8, cpu, 0); return p+2;
        case 0xce: add(d8, cpu, 1); return p+2;
        case 0xd6: sub(d8, cpu, 0); return p+2;
        case 0xde: sub(d8, cpu, 1); return p+2;
        case 0xe6: logic(bw_and, d8, cpu); return p+2;
        case 0xee: logic(bw_xor, d8, cpu); return p+2;
        case 0xf6: logic(bw_or, d8, cpu); return p+2;
        case 0xfe: cmp(d8, cpu); return p+2;
            //16bit A/L
        case 0x09: doubleadd(B, cpu); return p+1;
        case 0x19: doubleadd(D, cpu); return p+1;
        case 0x29: doubleadd(H, cpu); return p+1;
        case 0x39: doubleadd(SP, cpu); return p+1;
            //Push/pop using stack pointer
        case 0xc1: pop(B, cpu, mem); return p+1;
        case 0xd1: pop(D, cpu, mem); return p+1;
        case 0xe1: pop(H, cpu, mem); return p+1;
        case 0xf1: pop(A, cpu, mem); return p+1;
        case 0xc5: push(B, cpu, mem); return p+1;
        case 0xd5: push(D, cpu, mem); return p+1;
        case 0xe5: push(H, cpu, mem); return p+1;
        case 0xf5: push(A, cpu, mem); return p+1;
            //Jumps
        case 0xcb:
        case 0xc3: return da;//JMP
        case 0xc2: return !(cpu->iszero) ? da : p+3;//JNZ
        case 0xd2: return !(cpu->carry)  ? da : p+3;//JNC
        case 0xe2: return !(cpu->parity) ? da : p+3;//JPO
        case 0xf2: return !(cpu->sign)   ? da : p+3;//JP
        case 0xca: return  (cpu->iszero) ? da : p+3;//JZ
        case 0xda: return  (cpu->carry)  ? da : p+3;//JC
        case 0xea: return  (cpu->parity) ? da : p+3;//JPE
        case 0xfa: return  (cpu->sign)   ? da : p+3;//JM
        case 0xe9: return 0x100*(cpu->reg)[H]+(cpu->reg)[L];//PCHL
            //Calls
        case 0xcd:
        case 0xdd:
        case 0xed:
        case 0xfd: return call(p+3, da, cpu, mem);//CALL
        case 0xc4: return !(cpu->iszero) ? call(p+3, da, cpu, mem) : p+3;//CNZ
        case 0xd4: return !(cpu->carry)  ? call(p+3, da, cpu, mem) : p+3;//CNC
        case 0xe4: return !(cpu->parity) ? call(p+3, da, cpu, mem) : p+3;//CPO
        case 0xf4: return !(cpu->sign)   ? call(p+3, da, cpu, mem) : p+3;//CP
        case 0xcc: return  (cpu->iszero) ? call(p+3, da, cpu, mem) : p+3;//CZ
        case 0xdc: return  (cpu->carry)  ? call(p+3, da, cpu, mem) : p+3;//CC
        case 0xec: return  (cpu->parity) ? call(p+3, da, cpu, mem) : p+3;//CPE
        case 0xfc: return  (cpu->sign)   ? call(p+3, da, cpu, mem) : p+3;//CM
            //Returns
        case 0xc9:
        case 0xd9: return ret(cpu, mem);//RET
        case 0xc0: return !(cpu->iszero) ? ret(cpu, mem) : p+1;//RNZ
        case 0xd0: return !(cpu->carry)  ? ret(cpu, mem) : p+1;//RNC
        case 0xe0: return !(cpu->parity) ? ret(cpu, mem) : p+1;//RPO
        case 0xf0: return !(cpu->sign)   ? ret(cpu, mem) : p+1;//RP
        case 0xc8: return  (cpu->iszero) ? ret(cpu, mem) : p+1;//RZ
        case 0xd8: return  (cpu->carry)  ? ret(cpu, mem) : p+1;//RC
        case 0xe8: return  (cpu->parity) ? ret(cpu, mem) : p+1;//RPE
        case 0xf8: return  (cpu->sign)   ? ret(cpu, mem) : p+1;//RM
            //Restarts
        case 0xc7: return call(p+1, 0x00, cpu, mem);//RST 0
        case 0xcf: return call(p+1, 0x08, cpu, mem);//RST 1
        case 0xd7: return call(p+1, 0x10, cpu, mem);//RST 2
        case 0xdf: return call(p+1, 0x18, cpu, mem);//RST 3
        case 0xe7: return call(p+1, 0x20, cpu, mem);//RST 4
        case 0xef: return call(p+1, 0x28, cpu, mem);//RST 5
        case 0xf7: return call(p+1, 0x30, cpu, mem);//RST 6
        case 0xff: return call(p+1, 0x38, cpu, mem);//RST 7
            //IO
        case 0xDB: in (d8, cpu); return p + 2;
        case 0xD3: out(d8, cpu); return p + 2;
            //NOP
        case 0x00:
        case 0x10:
        case 0x20:
        case 0x30:
        case 0x08:
        case 0x18:
        case 0x28:
        case 0x38: return p+1;
        default: perror("Unrecognized instruction");
    }
}

void run(unsigned int address, struct i8080* cpu, unsigned char* mem) {
    unsigned int temp;
    cpu->prog_ctr = address;

    do
    {
        temp = cpu->prog_ctr;
        cpu->prog_ctr = exec_inst(cpu, mem);
    }
    while (temp != cpu->prog_ctr);
}