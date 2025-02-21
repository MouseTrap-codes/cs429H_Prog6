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

// Define the CPU structure
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
    memset(cpu, 0, sizeof(CPU));  // Initialize registers and memory to 0.
    return cpu;
}

// Error message for integer overflow
void overflowErrorMessage() {
    printf("Signed integer overflow!!!");
}

// Integer arithmetic instructions
void handleAdd(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = cpu->registers[rs];
    int64_t val2 = cpu->registers[rt];
    int64_t result = val1 + val2;
    bool overflow = ((val1 > 0 && val2 > 0 && result < 0) || (val1 < 0 && val2 < 0 && result > 0));
    if (overflow) {
        overflowErrorMessage();
        return;
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
    bool overflow = ((val1 > 0 && val2 < 0 && result < 0) || (val1 < 0 && val2 > 0 && result > 0));
    if (overflow) {
        overflowErrorMessage();
        return;
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
        return;
    }
    cpu->registers[rd] = val1 * val2;
    cpu->programCounter += 4;
}

void handleDiv(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = cpu->registers[rs];
    int64_t val2 = cpu->registers[rt];
    if (val2 == 0) {
        printf("ERROR: Division by zero!\n");
        return;
    }
    if (val1 == INT64_MIN && val2 == -1) {
        overflowErrorMessage();
        return;
    }
    cpu->registers[rd] = val1 / val2;
    cpu->programCounter += 4;
}

// Floating point instructions
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
        printf("error: cannot divide by 0");
        exit(1);
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
        printf("error: L is invalid");
        exit(1);
    }
    exit(0);
}

void handlePrivTrap(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    if (L != 0) {
        printf("error: L is invalid");
        exit(1);
    }
    cpu->userMode = 0;
    cpu->programCounter += 4;
}

void handlePrivRTE(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    if (L != 0) {
        printf("error: L is invalid");
        exit(1);
    }
    cpu->userMode = 1;
    cpu->programCounter += 4;
}

void handlePrivInput(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
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
    cpu->registers[rd] = input;
    cpu->programCounter += 4;
}

void handlePrivOutput(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
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

// Data movement instructions
void handleMovRdRsL(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, int64_t L) {
    uint64_t address = cpu->registers[rs] + L;
    if (address >= MEM_SIZE) {
        printf("error: invalid memory address at %lld\n", address);
        exit(1);
    }
    cpu->registers[rd] = *(uint64_t*)(cpu->memory + address);
    cpu->programCounter += 4;
}

void movRdRs(CPU* cpu, uint8_t rd, uint8_t rs) {
    cpu->registers[rd] = cpu->registers[rs];
    cpu->programCounter += 4;
}

// Changed to fully load the literal (ld)
void handleMovRdL(CPU* cpu, uint8_t rd, uint16_t L) {
    cpu->registers[rd] = (uint64_t)L;
    cpu->programCounter += 4;
}

void handleMovRDLRs(CPU* cpu, uint8_t rd, uint8_t rs, uint64_t L) {
    uint64_t address = cpu->registers[rd] + L;
    if (address >= MEM_SIZE) {
        printf("error: invalid memory address at %lld\n", address);
        exit(1);
    }
    *(uint64_t *)(cpu->memory + address) = cpu->registers[rs];
    cpu->programCounter += 4;
}

// Call/Return instructions with proper stack pointer update
void handleCall(CPU *cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    cpu->registers[31] -= 8;
    *(uint64_t *)(cpu->memory + cpu->registers[31]) = cpu->programCounter + 4;
    cpu->programCounter = cpu->registers[rd];
}

void handleReturn(CPU* cpu) {
    uint64_t returnAddress = *(uint64_t *)(cpu->memory + cpu->registers[31]);
    cpu->registers[31] += 8;
    cpu->programCounter = returnAddress; 
}

// Uniform Instruction Handler and wrappers
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
void wrapperHandleCall(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleCall(cpu, rd, rs, rt, L); }
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

// Global opcode handler array
InstructionHandler opHandlers[256] = {0};

void initOpcodeHandlers() {
    for (int i = 0; i < 256; i++) {
        opHandlers[i] = NULL;
    }
    // Logic Instructions
    opHandlers[0x0] = wrapperHandleAnd;
    opHandlers[0x1] = wrapperHandleOr;
    opHandlers[0x2] = wrapperHandleXor;
    opHandlers[0x3] = wrapperHandleNot;
    // Shift Instructions
    opHandlers[0x4] = wrapperHandleShftR;
    opHandlers[0x5] = wrapperHandleShftRI;
    opHandlers[0x6] = wrapperHandleShftL;
    opHandlers[0x7] = wrapperHandleShftLI;
    // Control Instructions
    opHandlers[0x8] = wrapperHandleBr;
    opHandlers[0x9] = wrapperHandleBrr;
    opHandlers[0xA] = wrapperHandleBrrL;
    opHandlers[0xB] = wrapperHandleBrnz;
    opHandlers[0xC] = wrapperHandleCall;
    opHandlers[0xD] = wrapperHandleReturn;
    opHandlers[0xE] = wrapperHandleBrgt;
    // Privileged Instructions
    opHandlers[0xF] = wrapperHandlePriv;
    // Data Movement Instructions
    opHandlers[0x10] = wrapperHandleMovRdRsL;
    opHandlers[0x11] = wrapperMovRdRs;
    opHandlers[0x12] = wrapperHandleMovRdL;
    opHandlers[0x13] = wrapperHandleMovRDLRs;
    // Floating Point Instructions
    opHandlers[0x14] = wrapperHandleAddf;
    opHandlers[0x15] = wrapperHandleSubf;
    opHandlers[0x16] = wrapperHandleMulf;
    opHandlers[0x17] = wrapperHandleDivf;
    // Integer Arithmetic Instructions
    opHandlers[0x18] = wrapperHandleAdd;
    opHandlers[0x19] = wrapperHandleAddI;
    opHandlers[0x1A] = wrapperHandleSub;
    opHandlers[0x1B] = wrapperHandleSubI;
    opHandlers[0x1C] = wrapperHandleMul;
    opHandlers[0x1D] = wrapperHandleDiv;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program.tko>\n", argv[0]);
        exit(1);
    }
    
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Error opening file");
        exit(1);
    }
    
    CPU* cpu = createCPU();
    cpu->registers[31] = MEM_SIZE;  // Initialize stack pointer.
    cpu->programCounter = 0x1000;
    
    // Load the object code into memory starting at 0x1000.
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_size > MEM_SIZE - 0x1000) {
        fprintf(stderr, "File too large for memory\n");
        exit(1);
    }
    if (fread(cpu->memory + 0x1000, 1, file_size, fp) != file_size) {
        fprintf(stderr, "Error reading file\n");
        exit(1);
    }
    fclose(fp);
    
    initOpcodeHandlers();
    
    // Fetch-decode-execute loop.
    while (cpu->programCounter < 0x1000 + file_size) {
        uint32_t instruction = *(uint32_t*)(cpu->memory + cpu->programCounter);
        instruction = le32toh(instruction);
        
        // Decode the fields: 
        uint8_t opcode = (instruction >> 27) & 0x1F;
        uint8_t rd     = (instruction >> 22) & 0x1F;
        uint8_t rs     = (instruction >> 17) & 0x1F;
        uint8_t rt     = (instruction >> 12) & 0x1F;
        uint16_t imm = instruction & 0xFFF;
        uint64_t L = 0;
        
        // Immediate decoding based on opcode.
        switch(opcode) {
            case 0xA: {
                 int16_t signedImm = (imm & 0x800) ? (int16_t)(imm | 0xF000) : imm;
                 L = (uint64_t)((int64_t)signedImm);
                 break;
            }
            case 0x10:
            case 0x13: {
                 int16_t signedImm = (imm & 0x800) ? (int16_t)(imm | 0xF000) : imm;
                 L = (uint64_t)((int64_t)signedImm);
                 break;
            }
            case 0x19:
            case 0x1B:
            case 0x12:
            case 0x5:
            case 0x7:
            case 0xF:
                 L = imm;
                 break;
            default:
                 L = 0;
                 break;
        }
        
        if (opHandlers[opcode]) {
            opHandlers[opcode](cpu, rd, rs, rt, L);
        } else {
            fprintf(stderr, "Unhandled opcode: 0x%X\n", opcode);
        }
    }
    
    return 0;
}
