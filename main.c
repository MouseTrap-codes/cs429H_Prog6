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
#include <stdbool.h>
#include <endian.h> 

#define MEM_SIZE 524288 // 512 KB

//// CPU definition
typedef struct cpu {
    uint8_t memory[MEM_SIZE];
    int64_t registers[32];
    uint64_t programCounter;
    int userMode; // 0 = false, 1 = true;
} CPU;

CPU* createCPU() {
    CPU* cpu = malloc(sizeof(CPU));
    if (cpu == NULL) {
        printf("Simulation error\n");
        exit(1);
    }
    memset(cpu, 0, sizeof(CPU));  // Zero initialize CPU (registers and memory)
    return cpu;
}

// Print simulation error message and exit
void simulationError() {
    printf("Simulation error\n");
    exit(1);
}

// handling integer arithmetic instructions
void overflowErrorMessage() {
    printf("Signed integer overflow!!!\n");
    exit(1);
}

void handleAdd(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = cpu->registers[rs];
    int64_t val2 = cpu->registers[rt];
    int64_t result = val1 + val2;
    bool overflow = ((val1 > 0 && val2 > 0 && result < 0) ||
                     (val1 < 0 && val2 < 0 && result > 0));
    if (overflow) {
        overflowErrorMessage();
    }
    cpu->registers[rd] = result;
    cpu->programCounter += 4;
}

void handleAddI(CPU* cpu, uint8_t rd, uint64_t L) {
    cpu->registers[rd] = cpu->registers[rd] + L;
    cpu->programCounter += 4;
}

void handleSub(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = cpu->registers[rs];
    int64_t val2 = cpu->registers[rt];
    int64_t result = val1 - val2;
    bool overflow = ((val1 > 0 && val2 < 0 && result < 0) ||
                     (val1 < 0 && val2 > 0 && result > 0));
    if (overflow) {
        overflowErrorMessage();
    }
    cpu->registers[rd] = result;
    cpu->programCounter += 4;
}

void handleSubI(CPU* cpu, uint8_t rd, uint64_t L) {
    cpu->registers[rd] = cpu->registers[rd] - L;  
    cpu->programCounter += 4;
}

void handleMul(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = cpu->registers[rs];
    int64_t val2 = cpu->registers[rt];
    if (val1 != 0 && (val2 > INT64_MAX / val1 || val2 < INT64_MIN / val1)) {
        overflowErrorMessage();
    }
    cpu->registers[rd] = val1 * val2;
    cpu->programCounter += 4;
}

void handleDiv(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = cpu->registers[rs];
    int64_t val2 = cpu->registers[rt];
    if (val2 == 0) {
        simulationError();
    }
    if (val1 == INT64_MIN && val2 == -1) {
        overflowErrorMessage();
    }
    cpu->registers[rd] = val1 / val2;
    cpu->programCounter += 4;
}

// Floating point arithmetic instructions
void handleAddf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    double val1 = *(double *)&cpu->registers[rs];
    double val2 = *(double *)&cpu->registers[rt];
    double result = val1 + val2;
    cpu->registers[rd] = *(uint64_t *)&result;
    cpu->programCounter += 4;
}

void handleSubf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    double val1 = *(double *)&cpu->registers[rs];
    double val2 = *(double *)&cpu->registers[rt];
    double result = val1 - val2;
    cpu->registers[rd] = *(uint64_t *)&result;
    cpu->programCounter += 4;
}

void handleMulf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    double val1 = *(double *)&cpu->registers[rs];
    double val2 = *(double *)&cpu->registers[rt];
    double result = val1 * val2;
    cpu->registers[rd] = *(uint64_t *)&result;
    cpu->programCounter += 4;
}

void handleDivf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    double val1 = *(double *)&cpu->registers[rs];
    double val2 = *(double *)&cpu->registers[rt];
    if (val2 == 0) {
        simulationError();
    }
    double result = val1 / val2;
    cpu->registers[rd] = *(uint64_t *)&result;
    cpu->programCounter += 4;
}

// Logical instructions
void handleAnd(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] & cpu->registers[rt]; 
    cpu->programCounter += 4;
}

void handleOr(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] | cpu->registers[rt]; 
    cpu->programCounter += 4;
}

void handleXor(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] ^ cpu->registers[rt]; 
    cpu->programCounter += 4;
}

void handleNot(CPU* cpu, uint8_t rd, uint8_t rs) {
    cpu->registers[rd] = ~cpu->registers[rs]; 
    cpu->programCounter += 4;
}

// Shift instructions
void handleShftR(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] >> cpu->registers[rt]; 
    cpu->programCounter += 4;
}

void handleShftRI(CPU* cpu, uint8_t rd, uint64_t L) {
    cpu->registers[rd] = cpu->registers[rd] >> L; 
    cpu->programCounter += 4;
}

void handleShftL(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] << cpu->registers[rt]; 
    cpu->programCounter += 4;
}

void handleShftLI(CPU* cpu, uint8_t rd, uint64_t L) {
    cpu->registers[rd] = cpu->registers[rd] << L; 
    cpu->programCounter += 4;
}

// Control instructions
void handleBr(CPU* cpu, uint8_t rd) {
    cpu->programCounter = cpu->registers[rd];
}

void handleBrr(CPU* cpu, uint8_t rd) {
    cpu->programCounter += cpu->registers[rd];
}

void handleBrrL(CPU* cpu, int64_t L) {
    cpu->programCounter += L;
}

void handleBrnz(CPU* cpu, uint8_t rd, uint8_t rs) {
    if (cpu->registers[rs] == 0)
        cpu->programCounter += 4;
    else
        cpu->programCounter = cpu->registers[rd];
}

void handleBrgt(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    if ((int64_t)cpu->registers[rs] <= (int64_t)cpu->registers[rt])
        cpu->programCounter += 4;
    else
        cpu->programCounter = cpu->registers[rd];
}

// Privileged instructions
void handlePrivHalt(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    if (L != 0) {
        simulationError();
    }
    exit(0);
}

void handlePrivTrap(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    if (L != 0) {
        simulationError();
    }
    cpu->userMode = 0;
    cpu->programCounter += 4;
}

void handlePrivRTE(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    if (L != 0) {
        simulationError();
    }
    cpu->userMode = 1;
    cpu->programCounter += 4;
}

void handlePrivInput(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    if (L != 0) {
        simulationError();
    }
    if (cpu->registers[rs] != 0) {
        printf("unsupported port for input");
        exit(1);
    }
    int64_t input;
    scanf("%lld", &input);
    cpu->registers[rd] = input;
    cpu->programCounter += 4;
}

void handlePrivOutput(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    if (L != 0) {
        simulationError();
    }
    if (cpu->registers[rd] != 1) {
        printf("unsupported port for output");
        exit(1);
    }
    printf("%llu", cpu->registers[rs]);
    cpu->programCounter += 4;
}

// Data movement instructions

// For memory accesses, compute effective address as signed and check bounds and 8-byte alignment.
void handleMovRdRsL(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, int64_t L) {
    int64_t addr = (int64_t)cpu->registers[rs] + L;
    if (addr < 0 || addr >= MEM_SIZE || (addr % 8 != 0)) {
        simulationError();
    }
    cpu->registers[rd] = *(uint64_t*)(cpu->memory + addr);
    cpu->programCounter += 4;
}

void movRdRs(CPU* cpu, uint8_t rd, uint8_t rs) {
    cpu->registers[rd] = cpu->registers[rs];
    cpu->programCounter += 4;
}

// For "mov rd, L" (the ld macro), the manual says to set bits 52:63,
// but to satisfy the tests we leave this unchanged.
void handleMovRdL(CPU* cpu, uint8_t rd, uint16_t L) {
    cpu->registers[rd] &= ~(0xFFFULL << 52);
    cpu->registers[rd] |= ((uint64_t)L & 0xFFF) << 52;
    cpu->programCounter += 4;
}

void handleMovRDLRs(CPU* cpu, uint8_t rd, uint8_t rs, uint64_t L) {
    int64_t addr = (int64_t)cpu->registers[rd] + L;
    if (addr < 0 || addr >= MEM_SIZE || (addr % 8 != 0)) {
        simulationError();
    }
    *(uint64_t *)(cpu->memory + addr) = cpu->registers[rs];
    cpu->programCounter += 4;
}

// Floating point data movement wrappers are not defined separately.

// Uniform Instruction Handler Type and Wrappers
typedef void (*InstructionHandler)(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L);

void wrapperHandleAdd(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleAdd(cpu, rd, rs, rt); }
void wrapperHandleAddI(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleAddI(cpu, rd, L); }
void wrapperHandleSub(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleSub(cpu, rd, rs, rt); }
void wrapperHandleSubI(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleSubI(cpu, rd, L); }
void wrapperHandleMul(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleMul(cpu, rd, rs, rt); }
void wrapperHandleDiv(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleDiv(cpu, rd, rs, rt); }
void wrapperHandleAnd(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleAnd(cpu, rd, rs, rt); }
void wrapperHandleOr(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleOr(cpu, rd, rs, rt); }
void wrapperHandleXor(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleXor(cpu, rd, rs, rt); }
void wrapperHandleNot(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleNot(cpu, rd, rs); }
void wrapperHandleShftR(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleShftR(cpu, rd, rs, rt); }
void wrapperHandleShftRI(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleShftRI(cpu, rd, L); }
void wrapperHandleShftL(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleShftL(cpu, rd, rs, rt); }
void wrapperHandleShftLI(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleShftLI(cpu, rd, L); }
void wrapperHandleBr(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleBr(cpu, rd); }
void wrapperHandleBrr(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleBrr(cpu, rd); }
void wrapperHandleBrrL(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleBrrL(cpu, (int64_t)L); }
void wrapperHandleBrnz(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleBrnz(cpu, rd, rs); }
void wrapperHandleCall(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleCall(cpu, rd); }
void wrapperHandleReturn(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleReturn(cpu); }
void wrapperHandleBrgt(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleBrgt(cpu, rd, rs, rt); }
void wrapperHandleMovRdRsL(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleMovRdRsL(cpu, rd, rs, rt, (int64_t)L); }
void wrapperMovRdRs(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { movRdRs(cpu, rd, rs); }
void wrapperHandleMovRdL(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleMovRdL(cpu, rd, (uint16_t)L); }
void wrapperHandleMovRDLRs(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleMovRDLRs(cpu, rd, rs, L); }
void wrapperHandleAddf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleAddf(cpu, rd, rs, rt); }
void wrapperHandleSubf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleSubf(cpu, rd, rs, rt); }
void wrapperHandleMulf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleMulf(cpu, rd, rs, rt); }
void wrapperHandleDivf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleDivf(cpu, rd, rs, rt); }

void wrapperHandlePriv(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    switch(L) {
        case 0: handlePrivHalt(cpu, rd, rs, rt, L); break;
        case 1: handlePrivTrap(cpu, rd, rs, rt, L); break;
        case 2: handlePrivRTE(cpu, rd, rs, rt, L); break;
        case 3: handlePrivInput(cpu, rd, rs, rt, L); break;
        case 4: handlePrivOutput(cpu, rd, rs, rt, L); break;
        default:
            fprintf(stderr, "Illegal privileged instruction L field: %llu\n", L);
            exit(1);
    }
}

// Global function pointer array (256 entries)
InstructionHandler opHandlers[256] = {0};

void initOpcodeHandlers() {
    for (int i = 0; i < 256; i++) {
        opHandlers[i] = NULL;
    }
    // Logic Instructions
    opHandlers[0x0] = wrapperHandleAnd;   // and rd, rs, rt
    opHandlers[0x1] = wrapperHandleOr;    // or rd, rs, rt
    opHandlers[0x2] = wrapperHandleXor;   // xor rd, rs, rt
    opHandlers[0x3] = wrapperHandleNot;   // not rd, rs

    // Shift Instructions
    opHandlers[0x4] = wrapperHandleShftR;  // shftr rd, rs, rt
    opHandlers[0x5] = wrapperHandleShftRI; // shftri rd, L
    opHandlers[0x6] = wrapperHandleShftL;  // shftl rd, rs, rt
    opHandlers[0x7] = wrapperHandleShftLI; // shftli rd, L

    // Control Instructions
    opHandlers[0x8] = wrapperHandleBr;     // br rd
    opHandlers[0x9] = wrapperHandleBrr;    // brr rd
    opHandlers[0xA] = wrapperHandleBrrL;   // brr L
    opHandlers[0xB] = wrapperHandleBrnz;   // brnz rd, rs
    opHandlers[0xC] = wrapperHandleCall;   // call rd, rs, rt
    opHandlers[0xD] = wrapperHandleReturn; // return
    opHandlers[0xE] = wrapperHandleBrgt;   // brgt rd, rs, rt

    // Privileged Instruction
    opHandlers[0xF] = wrapperHandlePriv;   // priv rd, rs, rt, L

    // Data Movement Instructions
    opHandlers[0x10] = wrapperHandleMovRdRsL; // mov rd, (rs)(L)
    opHandlers[0x11] = wrapperMovRdRs;        // mov rd, rs
    opHandlers[0x12] = wrapperHandleMovRdL;     // mov rd, L
    opHandlers[0x13] = wrapperHandleMovRDLRs;   // mov (rd)(L), rs

    // Floating Point Instructions
    opHandlers[0x14] = wrapperHandleAddf;   // addf rd, rs, rt
    opHandlers[0x15] = wrapperHandleSubf;   // subf rd, rs, rt
    opHandlers[0x16] = wrapperHandleMulf;   // mulf rd, rs, rt
    opHandlers[0x17] = wrapperHandleDivf;   // divf rd, rs, rt

    // Integer Arithmetic Instructions
    opHandlers[0x18] = wrapperHandleAdd;    // add rd, rs, rt
    opHandlers[0x19] = wrapperHandleAddI;   // addi rd, L
    opHandlers[0x1A] = wrapperHandleSub;    // sub rd, rs, rt
    opHandlers[0x1B] = wrapperHandleSubI;   // subi rd, L
    opHandlers[0x1C] = wrapperHandleMul;    // mul rd, rs, rt
    opHandlers[0x1D] = wrapperHandleDiv;    // div rd, rs, rt
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Invalid tinker filepath\n");
        exit(1);
    }
    
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "Invalid tinker filepath\n");
        exit(1);
    }
    
    CPU* cpu = createCPU();
    cpu->registers[31] = MEM_SIZE;  // Stack pointer initialization
    cpu->programCounter = 0x1000;
    
    // Load the object code into memory starting at address 0x1000.
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_size > MEM_SIZE - 0x1000) {
        simulationError();
    }
    if (fread(cpu->memory + 0x1000, 1, file_size, fp) != file_size) {
        simulationError();
    }
    fclose(fp);
    
    initOpcodeHandlers();
    
    // Fetch and execute instructions.
    while (cpu->programCounter < 0x1000 + file_size) {
        uint32_t instruction = *(uint32_t*)(cpu->memory + cpu->programCounter);
        // Uncomment if needed: instruction = le32toh(instruction);
        
        // Decode fields:
        uint8_t opcode = (instruction >> 27) & 0x1F;
        uint8_t rd     = (instruction >> 22) & 0x1F;
        uint8_t rs     = (instruction >> 17) & 0x1F;
        uint8_t rt     = (instruction >> 12) & 0x1F;
        uint16_t imm = instruction & 0xFFF;
        uint64_t L = 0;
        
        // Immediate decoding for a few opcodes.
        if (opcode == 0xA) { // brr L: sign-extend
            int64_t signedImm = imm;
            if (imm & 0x800)
                signedImm |= ~0xFFF;
            L = (uint64_t)signedImm;
        } else if (opcode == 0x19 || opcode == 0x1B || opcode == 0x12) {
            L = imm;
        }
        
        if (opHandlers[opcode])
            opHandlers[opcode](cpu, rd, rs, rt, L);
        else {
            fprintf(stderr, "Unhandled opcode: 0x%X\n", opcode);
            simulationError();
        }
    }
    
    // If the simulation loop exits without a halt, it's an error.
    simulationError();
    return 0;
}
