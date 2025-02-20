#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <regex.h>
#include "uthash.h"
#include <stdbool.h>

#define MEM_SIZE 524288 // 512 KB

//// here we define the CPU
typedef struct cpu {
    uint8_t memory[MEM_SIZE];
    int64_t registers[32];
    uint64_t programCounter;
    int userMode; // 0 = false, 1 = true;
} CPU;

CPU* createCPU() {
    CPU* cpu = malloc(sizeof(CPU));
    if (cpu == NULL) {
        perror("malloc failed!");
        exit(1);
    }
    return cpu;
}

// handling integer arithmetic instructions
void overflowErrorMessage() {
    printf("Signed integer overflow!!!");
}

// Performs signed addition of two 64-bit signed values in registers rs and rt and stores the result in register rd.
void handleAdd(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = (int64_t)cpu->registers[rs];
    int64_t val2 = (int64_t)cpu->registers[rt];

    int64_t result = val1 + val2;

    // Overflow detection for signed addition
    bool overflow = ((val1 > 0 && val2 > 0 && result < 0) || (val1 < 0 && val2 < 0 && result > 0));
    if (overflow) {
        overflowErrorMessage();
        return;  // Stop execution if overflow occurs
    }

    cpu->registers[rd] = (uint64_t)result;
    cpu->programCounter += 4;
}

// Adds the unsigned value L to the value in register rd.
void handleAddI(CPU* cpu, uint8_t rd, uint64_t L) {
    cpu->registers[rd] = cpu->registers[rd] + L;
    cpu->programCounter += 4;
}

// Performs signed subtraction of two 64-bit signed values in registers rs and rt and stores the result in register rd.
void handleSub(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = (int64_t)cpu->registers[rs];
    int64_t val2 = (int64_t)cpu->registers[rt];

    int64_t result = val1 - val2;

    // Overflow detection for signed subtraction
    bool overflow = ((val1 > 0 && val2 < 0 && result < 0) || (val1 < 0 && val2 > 0 && result > 0));
    if (overflow) {
        overflowErrorMessage();
        return;
    }

    cpu->registers[rd] = (uint64_t)result;
    cpu->programCounter += 4;
}

// Subtracts the unsigned value L from the value in register rd.
void handleSubI(CPU* cpu, uint8_t rd, uint64_t L) {
    cpu->registers[rd] = cpu->registers[rd] - L;  
    cpu->programCounter += 4;
}

// Performs signed multiplication of two 64-bit signed values in registers rs and rt and stores the result in register rd.
void handleMul(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = (int64_t)cpu->registers[rs];
    int64_t val2 = (int64_t)cpu->registers[rt];

    // Check for overflow
    if (val1 != 0 && (val2 > INT64_MAX / val1 || val2 < INT64_MIN / val1)) {
        overflowErrorMessage();
        return;
    }

    cpu->registers[rd] = (uint64_t)(val1 * val2);
    cpu->programCounter += 4;
}

// Performs signed division of two 64-bit signed values in registers rs and rt and stores the result in register rd.
void handleDiv(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = (int64_t)cpu->registers[rs];
    int64_t val2 = (int64_t)cpu->registers[rt];

    // Division-by-zero check
    if (val2 == 0) {
        printf("ERROR: Division by zero!\n");
        return;
    }

    // Handle INT64_MIN / -1 overflow case
    if (val1 == INT64_MIN && val2 == -1) {
        overflowErrorMessage();
        return;
    }

    cpu->registers[rd] = (uint64_t)(val1 / val2);
    cpu->programCounter += 4;
}

//// handling logic instructions ////////////////////
// Performs bitwise “and” of two 64-bit values in registers rs and rt and stores the result in register rd 
void handleAnd(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] & cpu->registers[rt]; 
    cpu->programCounter += 4;
}

// Performs bitwise “or” of two 64-bit values in registers rs and rt and stores the result in register rd
void handleOr(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] | cpu->registers[rt]; 
    cpu->programCounter += 4;
}

// Performs bitwise “xor” of two 64-bit values in registers rs and rt and stores the result in register rd
void handleXor(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] ^ cpu->registers[rt]; 
    cpu->programCounter += 4;
}

// Performs bitwise “not” (one’s complement) of a 64-bit value in register rs and stores the result in register rd 
void handleNot(CPU* cpu, uint8_t rd, uint8_t rs) {
    cpu->registers[rd] = ~cpu->registers[rs]; 
    cpu->programCounter += 4;
}

// Shifts the value in register rs to the right by the number of bits specified in the value in register rt and stores the result in register rd
void handleShftR(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] >> cpu->registers[rt]; 
    cpu->programCounter += 4;
}

// Shifts the value in register rd to the right by the number of bits specified by L
void handleShftRI(CPU* cpu, uint8_t rd, uint64_t L) {
    cpu->registers[rd] = cpu->registers[rd] >> L; 
    cpu->programCounter += 4;
}

// Shifts the value in register rs to the left by the number of bits specified in the value in register rt and stores the result in register rd 
void handleShftL(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] << cpu->registers[rt]; 
    cpu->programCounter += 4;
}

// Shifts the value in register rd to the left by the number of bits specified by L
void handleShftLI(CPU* cpu, uint8_t rd, uint64_t L) {
    cpu->registers[rd] = cpu->registers[rd] << L; 
    cpu->programCounter += 4;
}


// handling control instructions //////////
// Jumps to the instruction address specified by the value in register rd 
void handleBr(CPU* cpu, uint8_t rd) {
    cpu->programCounter = cpu->registers[rd] ;
}

// Jumps to the instruction address specified by adding the value in register rd to the program counter
void handleBrr(CPU* cpu, uint8_t rd) {
    cpu->programCounter += cpu->registers[rd];
}

// Jumps to the instruction address specified by adding L to the program counter (L can be negative)
void handleBrrL(CPU* cpu, int64_t L) {
    cpu->programCounter += (int64_t)L;
}

// Jumps to the instruction address specified by the value in register rd if rs is nonzero, otherwise continue to the next instruction
void handleBrnz(CPU* cpu, uint8_t rd, uint8_t rs) {
    if (cpu->registers[rs] == 0) {
        cpu->programCounter += 4;
    } else {
        cpu->programCounter = cpu->registers[rd];
    }
}

// Calls the function that starts at the address specified by rd and stores the return address on the stack.
void handleCall(CPU *cpu, uint8_t rd) {
    // Save return address (pc + 4) on the stack
    //cpu->registers[31] -= 8;  // Move stack pointer down // you apparently are not supposed to do this
    *(uint64_t *)(cpu->memory + (int64_t)cpu->registers[31]) = cpu->programCounter + 4;

    // Jump to the function address stored in register rd
    cpu->programCounter = cpu->registers[rd];
}

// Restores the program counter from the stack and returns to the caller
void handleReturn(CPU* cpu) {
    // Load return address
    uint64_t returnAddress = *(uint64_t *)(cpu->memory + (int64_t)cpu->registers[31]);

    // Undo call (move the stack pointer back up)
    //cpu->registers[31] += 8; you apparently are not supposed to do this

    // Now set programCounter to returnAddress
    cpu->programCounter = returnAddress; 
}

// Jumps to the instruction address specified by the value in register rd if rs is greater than rt , rs and rt being signed integers; otherwise continue to the next instruction
void handleBrgt(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    if (((int64_t) cpu->registers[rs]) <= ((int64_t)cpu->registers[rt])) {
        cpu->programCounter += 4;
    } else {
        cpu->programCounter = cpu->registers[rd];
    }
}

// handling priveledged instructions
// 0x0: Halt instruction. This stops the simulation. It is the last instruction run by a program
void handlePrivHalt(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    // the value of L can cause a simulation error
    if (L != 0) {
        printf("error: L is invalid");
        exit(1);
    }
    exit(0);
}

// 0x1: Trap instruction. This allows a user program to call an operating system, with the values of the register specifying the system call and the input parameters. When this instruction is called, the processor switches from user mode to supervisor mode
void handlePrivTrap(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    // the value of L can cause a simulation error
    if (L != 0) {
        printf("error: L is invalid");
        exit(1);
    }
    cpu->userMode = 0; // false because we are now in supervisor mode
    cpu->programCounter++;
}

// 0x2: RTE instruction. This switches the processor from supervisor mode back to user mode
void handlePrivRTE(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    // the value of L can cause a simulation error
    if (L != 0) {
        printf("error: L is invalid");
        exit(1);
    }
    cpu->userMode = 1; // true because we are back in user mode
    cpu->programCounter++;
}

/* 0x3: Input instruction.
Function:
rd ← Input[rs ]
Reads from the input port pointed to by the value in register rs and stores
it in register rd .
By convention, port 0 is always connected to the keyboard, while port 1 is connected to
the console output
*/ 
void handlePrivInput(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    // the value of L can cause a simulation error
    if (L != 0) {
        printf("error: L is invalid");
        exit(1);
    }
    if (cpu->registers[rs] != 0) {
        printf("unsupported port for input");
        return;
    }
    int64_t input;
    scanf("%lld", &input);

    cpu->registers[rd] = (uint64_t)input;
    cpu->programCounter++;
}

/*
0x4: Output instruction.
Function:
Output[rd ] ← rs
Reads the value in register rs and writes it to the output port pointed to by the
value in register rd .
By convention, port 0 is always connected to the keyboard, while port 1 is connected to
the console output.
*/
void handlePrivOutput(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    // the value of L can cause a simulation error
    if (L != 0) {
        printf("error: L is invalid");
        exit(1);
    }
    if (cpu->registers[rd] != 1) {
        printf("unsupported port for output");
        return;
    }
    printf("%llu", cpu->registers[rs]);
    cpu->programCounter += 4;
}

// handling data movement instructions /////////
/*
    Reads the value in the memory location pointed to by the value composed of the value in
    register rs as a base register and the literal value L as an index, and stores it in register rd
*/
void handleMovRdRsL(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, int64_t L) {
    uint64_t address = cpu->registers[rs] + L;

    // Check for out-of-bounds memory access
    if (address < 0 || address >= sizeof(cpu->memory)) {
        printf("error: invalid memory address at %lld\n", address);
        exit(1);
    }

    cpu->registers[rd] = *(uint64_t*)(cpu->memory + address);
    cpu->programCounter += 4;
}

// Reads the value in register rs and stores it in register rd 
void movRdRs(CPU* cpu, uint8_t rd, uint8_t rs) {
    cpu->registers[rd] = cpu->registers[rs];
    cpu->programCounter += 4;
}

// Sets bits 52:63 (inclusive) of register rd to the value of L
void handleMovRdL(CPU* cpu, uint8_t rd, uint16_t L) {
    // Clear bits 52-63 of rd and insert L
    cpu->registers[rd] &= ~(0xFFFULL << 52); // Clear bits 52-63
    cpu->registers[rd] |= ((uint64_t)L & 0xFFF) << 52; // Insert L in bits 52-63

    // Move to the next instruction
    cpu->programCounter += 4;
}

/*
    Reads the value in register rs and stores it in the memory location pointed to by the value
    composed of the value in register rd . as a base register and the literal L as an index.
*/
void handleMovRDLRs(CPU* cpu, uint8_t rd, uint8_t rs, uint64_t L) {
    // Calculate the memory address using rd as the base and L as the offset
    uint64_t address = cpu->registers[rd] + L;

    // Check for out-of-bounds memory access
    if (address >= sizeof(cpu->memory)) {
        printf("error: invalid memory address at %lld\n", address);
        exit(1);
    }

    // Store the value from register rs into memory at the computed address
    *(uint64_t *)(cpu->memory + address) = cpu->registers[rs];

    // Move to the next instruction
    cpu->programCounter += 4;
}

// handling floating point instructions
// Performs signed addition of two double precision values in registers rs and rt , and stores the result in register rd 
void handleAddf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    // Interpret the values in registers as double-precision floating points
    double val1 = *(double *)&cpu->registers[rs];
    double val2 = *(double *)&cpu->registers[rt];

    // Perform floating-point addition
    double result = val1 + val2;

    // Store the result back into the destination register
    cpu->registers[rd] = *(uint64_t *)&result;

    // Move to the next instruction
    cpu->programCounter += 4;
}

// Performs signed subtraction of two double precision values in registers rs and rt , and stores the result in register rd
void handleSubf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    // Interpret the values in registers as double-precision floating points
    double val1 = *(double *)&cpu->registers[rs];
    double val2 = *(double *)&cpu->registers[rt];

    // Perform floating-point subtraction
    double result = val1 - val2;

    // Store the result back into the destination register
    cpu->registers[rd] = *(uint64_t *)&result;

    // Move to the next instruction
    cpu->programCounter += 4;
}

// Performs signed multiplication of two double precision values in registers rs and rt , and stores the result in register rd
void handleMulf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    // Interpret the values in registers as double-precision floating points
    double val1 = *(double *)&cpu->registers[rs];
    double val2 = *(double *)&cpu->registers[rt];

    // Perform floating-point multiplication
    double result = val1 * val2;

    // Store the result back into the destination register
    cpu->registers[rd] = *(uint64_t *)&result;

    // Move to the next instruction
    cpu->programCounter += 4;
}

// Performs signed division of two double precision values in registers rs and rt , and stores the result in register rd 
void handleDivf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    // Interpret the values in registers as double-precision floating points
    double val1 = *(double *)&cpu->registers[rs];
    double val2 = *(double *)&cpu->registers[rt];

    if (val2 == 0) {
        printf("error: cannot divide by 0");
        exit(1);
    }

    // Perform floating-point division
    double result = val1 / val2;

    // Store the result back into the destination register
    cpu->registers[rd] = *(uint64_t *)&result;

    // Move to the next instruction
    cpu->programCounter += 4;
}

/// now we need to make a function array
typedef void (*generic_function)(void*);



/// we also need to make a method to parse the lines of binary
/// to get the necessary information ie opcode rd rs rt L etc











int main(int argc, char *argv[]) {
    CPU* cpu = createCPU();
    cpu->registers[31] = MEM_SIZE;
    cpu->programCounter = 0x1000;

    // line by line we will look at the tko file
    // and call corresponding function from function array
    // depending on the opcode of that line

    return 0;
}


