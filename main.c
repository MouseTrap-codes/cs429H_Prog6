#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <endian.h>  // Optional, only if you need le32toh on big-endian systems.

#define MEM_SIZE 524288 // 512 KB

// Single unified error function
static void simulationError(void) {
    printf("Simulation error\n");
    exit(1);
}

// The CPU struct
typedef struct cpu {
    uint8_t  memory[MEM_SIZE];  // 512KB memory
    int64_t  registers[32];     // 32 general-purpose registers
    uint64_t programCounter;    // PC
    int      userMode;          // 0 = false, 1 = true
} CPU;

// Create and zero-initialize the CPU
CPU* createCPU(void) {
    CPU* cpu = malloc(sizeof(CPU));
    if (!cpu) {
        // If we can't allocate, also treat as simulation error:
        simulationError();
    }
    memset(cpu, 0, sizeof(*cpu));
    // Optionally set userMode = 1 if that's standard:
    cpu->userMode = 1;
    return cpu;
}

// We can unify all overflow or error messages into simulationError().

////////////////////////////////////////
// Integer Arithmetic
////////////////////////////////////////
static void handleAdd(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = cpu->registers[rs];
    int64_t val2 = cpu->registers[rt];
    int64_t result = val1 + val2;
    bool overflow = ((val1 > 0 && val2 > 0 && result < 0) ||
                     (val1 < 0 && val2 < 0 && result > 0));
    if (overflow) {
        simulationError();
    }
    cpu->registers[rd] = result;
    cpu->programCounter += 4;
}

static void handleAddI(CPU* cpu, uint8_t rd, uint64_t L) {
    // L is treated as unsigned
    cpu->registers[rd] += (int64_t)L;
    cpu->programCounter += 4;
}

static void handleSub(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = cpu->registers[rs];
    int64_t val2 = cpu->registers[rt];
    int64_t result = val1 - val2;
    bool overflow = ((val1 > 0 && val2 < 0 && result < 0) ||
                     (val1 < 0 && val2 > 0 && result > 0));
    if (overflow) {
        simulationError();
    }
    cpu->registers[rd] = result;
    cpu->programCounter += 4;
}

static void handleSubI(CPU* cpu, uint8_t rd, uint64_t L) {
    cpu->registers[rd] -= (int64_t)L;
    cpu->programCounter += 4;
}

static void handleMul(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = cpu->registers[rs];
    int64_t val2 = cpu->registers[rt];
    if (val1 != 0 &&
       (val2 > INT64_MAX / val1 || val2 < INT64_MIN / val1)) {
        simulationError();
    }
    cpu->registers[rd] = val1 * val2;
    cpu->programCounter += 4;
}

static void handleDiv(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = cpu->registers[rs];
    int64_t val2 = cpu->registers[rt];
    if (val2 == 0) {
        simulationError();
    }
    if (val1 == INT64_MIN && val2 == -1) {
        simulationError();
    }
    cpu->registers[rd] = val1 / val2;
    cpu->programCounter += 4;
}

////////////////////////////////////////
// Floating-Point Arithmetic
////////////////////////////////////////
static void handleAddf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    double val1 = *(double *)&cpu->registers[rs];
    double val2 = *(double *)&cpu->registers[rt];
    double res  = val1 + val2;
    cpu->registers[rd] = *(int64_t *)&res;
    cpu->programCounter += 4;
}

static void handleSubf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    double val1 = *(double *)&cpu->registers[rs];
    double val2 = *(double *)&cpu->registers[rt];
    double res  = val1 - val2;
    cpu->registers[rd] = *(int64_t *)&res;
    cpu->programCounter += 4;
}

static void handleMulf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    double val1 = *(double *)&cpu->registers[rs];
    double val2 = *(double *)&cpu->registers[rt];
    double res  = val1 * val2;
    cpu->registers[rd] = *(int64_t *)&res;
    cpu->programCounter += 4;
}

static void handleDivf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    double val1 = *(double *)&cpu->registers[rs];
    double val2 = *(double *)&cpu->registers[rt];
    if (val2 == 0.0) {
        simulationError();
    }
    double res  = val1 / val2;
    cpu->registers[rd] = *(int64_t *)&res;
    cpu->programCounter += 4;
}

////////////////////////////////////////
// Logical
////////////////////////////////////////
static void handleAnd(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] & cpu->registers[rt];
    cpu->programCounter += 4;
}

static void handleOr(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] | cpu->registers[rt];
    cpu->programCounter += 4;
}

static void handleXor(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] ^ cpu->registers[rt];
    cpu->programCounter += 4;
}

static void handleNot(CPU* cpu, uint8_t rd, uint8_t rs) {
    cpu->registers[rd] = ~cpu->registers[rs];
    cpu->programCounter += 4;
}

////////////////////////////////////////
// Shifts
////////////////////////////////////////
static void handleShftR(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    // no range check on shift value
    cpu->registers[rd] = cpu->registers[rs] >> cpu->registers[rt];
    cpu->programCounter += 4;
}

static void handleShftRI(CPU* cpu, uint8_t rd, uint64_t L) {
    cpu->registers[rd] = cpu->registers[rd] >> (L & 63); 
    cpu->programCounter += 4;
}

static void handleShftL(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    cpu->registers[rd] = cpu->registers[rs] << cpu->registers[rt];
    cpu->programCounter += 4;
}

static void handleShftLI(CPU* cpu, uint8_t rd, uint64_t L) {
    cpu->registers[rd] = cpu->registers[rd] << (L & 63);
    cpu->programCounter += 4;
}

////////////////////////////////////////
// Control
////////////////////////////////////////
static void handleBr(CPU* cpu, uint8_t rd) {
    cpu->programCounter = cpu->registers[rd];
}

static void handleBrr(CPU* cpu, uint8_t rd) {
    cpu->programCounter += cpu->registers[rd];
}

static void handleBrrL(CPU* cpu, int64_t L) {
    cpu->programCounter += L;
}

static void handleBrnz(CPU* cpu, uint8_t rd, uint8_t rs) {
    if (cpu->registers[rs] == 0) {
        cpu->programCounter += 4;
    } else {
        cpu->programCounter = cpu->registers[rd];
    }
}

static void handleBrgt(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    if ((int64_t)cpu->registers[rs] <= (int64_t)cpu->registers[rt]) {
        cpu->programCounter += 4;
    } else {
        cpu->programCounter = cpu->registers[rd];
    }
}

// Official Tinker call/return convention
static void handleCall(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    // Tinker manual says:
    // Mem[r31 - 8] = pc + 4
    // r31 = r31 - 8
    // pc = rd
    cpu->registers[31] -= 8;
    // check if r31 is in range
    if (cpu->registers[31] < 0 || cpu->registers[31] + 7 >= MEM_SIZE) {
        simulationError();
    }
    *(uint64_t *)(cpu->memory + cpu->registers[31]) = cpu->programCounter + 4;
    cpu->programCounter = cpu->registers[rd];
}

static void handleReturn(CPU* cpu) {
    // pc = Mem[r31]
    // r31 = r31 + 8
    if (cpu->registers[31] < 0 || cpu->registers[31] + 7 >= MEM_SIZE) {
        simulationError();
    }
    uint64_t retAddr = *(uint64_t *)(cpu->memory + cpu->registers[31]);
    cpu->registers[31] += 8;
    cpu->programCounter = retAddr;
}

////////////////////////////////////////
// Privileged
////////////////////////////////////////
static void handlePrivHalt(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    if (L != 0) {
        simulationError();
    }
    exit(0);
}

static void handlePrivTrap(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    if (L != 0) {
        simulationError();
    }
    cpu->userMode = 0;
    cpu->programCounter += 4;
}

static void handlePrivRTE(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    if (L != 0) {
        simulationError();
    }
    cpu->userMode = 1;
    cpu->programCounter += 4;
}

static void handlePrivInput(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    if (L != 0) {
        simulationError();
    }
    // By convention, port 0 -> keyboard
    if (cpu->registers[rs] != 0) {
        // either ignore or treat as error
        simulationError();
    }
    int64_t val;
    if (scanf("%lld", &val) != 1) {
        // If input fails
        simulationError();
    }
    cpu->registers[rd] = val;
    cpu->programCounter += 4;
}

static void handlePrivOutput(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    if (L != 0) {
        simulationError();
    }
    // By convention, port 1 -> console
    if (cpu->registers[rd] != 1) {
        simulationError();
    }
    // Print as unsigned (the manual says so)
    printf("%llu", (unsigned long long)cpu->registers[rs]);
    cpu->programCounter += 4;
}

////////////////////////////////////////
// Data Movement
////////////////////////////////////////

// mov rd, (rs)(L)
void handleMovRdRsL(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, int64_t L) {
    int64_t addr = (int64_t)cpu->registers[rs] + L;
    // Check only that 0 <= addr <= MEM_SIZE-8
    if (addr < 0 || addr + 7 >= MEM_SIZE) {
        simulationError();
    }
    cpu->registers[rd] = *(int64_t *)(cpu->memory + addr);
    cpu->programCounter += 4;
}

// mov rd, rs
static void movRdRs(CPU* cpu, uint8_t rd, uint8_t rs) {
    cpu->registers[rd] = cpu->registers[rs];
    cpu->programCounter += 4;
}

// mov rd, L => official Tinker sets bits [52..63] of rd
static void handleMovRdL(CPU* cpu, uint8_t rd, uint16_t L) {
    // If your tests want "ld" semantics, do: cpu->registers[rd] = (uint64_t)L;
    // But official Tinker = set bits [52..63]
    cpu->registers[rd] &= ~(0xFFFULL << 52);
    cpu->registers[rd] |= ((uint64_t)L & 0xFFF) << 52;
    cpu->programCounter += 4;
}

void handleMovRDLRs(CPU* cpu, uint8_t rd, uint8_t rs, uint64_t L) {
    int64_t addr = (int64_t)cpu->registers[rd] + L;
    if (addr < 0 || addr + 7 >= MEM_SIZE) {
        simulationError();
    }
    *(int64_t *)(cpu->memory + addr) = cpu->registers[rs];
    cpu->programCounter += 4;
}

////////////////////////////////////////
// Wrapper for Priv
////////////////////////////////////////
static void wrapperHandlePriv(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    switch(L) {
        case 0: handlePrivHalt(cpu, rd, rs, rt, L);   break;
        case 1: handlePrivTrap(cpu, rd, rs, rt, L);   break;
        case 2: handlePrivRTE(cpu, rd, rs, rt, L);    break;
        case 3: handlePrivInput(cpu, rd, rs, rt, L);  break;
        case 4: handlePrivOutput(cpu, rd, rs, rt, L); break;
        default:
            simulationError();
    }
}

////////////////////////////////////////
// Build a Function Pointer Array
////////////////////////////////////////
typedef void (*InstructionHandler)(CPU*, uint8_t, uint8_t, uint8_t, uint64_t);

static InstructionHandler opHandlers[256];

static void initOpcodeHandlers(void) {
    memset(opHandlers, 0, sizeof(opHandlers));

    // 0x0 - 0x3: Logic
    opHandlers[0x0] = (InstructionHandler)handleAnd;
    opHandlers[0x1] = (InstructionHandler)handleOr;
    opHandlers[0x2] = (InstructionHandler)handleXor;
    opHandlers[0x3] = (InstructionHandler)handleNot;

    // 0x4 - 0x7: Shift
    opHandlers[0x4] = (InstructionHandler)handleShftR; 
    opHandlers[0x5] = (InstructionHandler)handleShftRI; 
    opHandlers[0x6] = (InstructionHandler)handleShftL; 
    opHandlers[0x7] = (InstructionHandler)handleShftLI; 

    // 0x8 - 0xE: Control
    // note 0xC (call) and 0xD (return) need full params
    opHandlers[0x8] = (InstructionHandler)handleBr; 
    opHandlers[0x9] = (InstructionHandler)handleBrr; 
    opHandlers[0xA] = (InstructionHandler)handleBrrL; 
    opHandlers[0xB] = (InstructionHandler)handleBrnz; 
    // call and return get the full signature:
    // We'll wrap them to match the function pointer type:
    opHandlers[0xC] = (InstructionHandler)handleCall; 
    opHandlers[0xD] = (InstructionHandler)handleReturn;
    opHandlers[0xE] = (InstructionHandler)handleBrgt;

    // 0xF: Priv
    opHandlers[0xF] = wrapperHandlePriv;

    // 0x10 - 0x13: Data movement
    opHandlers[0x10] = (InstructionHandler)handleMovRdRsL; 
    opHandlers[0x11] = (InstructionHandler)movRdRs;
    opHandlers[0x12] = (InstructionHandler)handleMovRdL;
    opHandlers[0x13] = (InstructionHandler)handleMovRDLRs;

    // 0x14 - 0x17: Floating point
    opHandlers[0x14] = (InstructionHandler)handleAddf;
    opHandlers[0x15] = (InstructionHandler)handleSubf;
    opHandlers[0x16] = (InstructionHandler)handleMulf;
    opHandlers[0x17] = (InstructionHandler)handleDivf;

    // 0x18 - 0x1D: Integer arithmetic
    opHandlers[0x18] = (InstructionHandler)handleAdd;
    opHandlers[0x19] = (InstructionHandler)handleAddI;
    opHandlers[0x1A] = (InstructionHandler)handleSub;
    opHandlers[0x1B] = (InstructionHandler)handleSubI;
    opHandlers[0x1C] = (InstructionHandler)handleMul;
    opHandlers[0x1D] = (InstructionHandler)handleDiv;
}

////////////////////////////////////////
// Main
////////////////////////////////////////
int main(int argc, char *argv[]) {
    // 1) Check arguments
    if (argc < 2) {
        // Some tests want exactly "Invalid tinker filepath"
        printf("Invalid tinker filepath\n");
        return 1;
    }

    // 2) Open file
    FILE* fp = fopen(argv[1], "rb");
    if (!fp) {
        printf("Invalid tinker filepath\n");
        return 1;
    }

    // 3) Create CPU
    CPU* cpu = createCPU();
    cpu->programCounter = 0x1000;
    cpu->registers[31] = MEM_SIZE; // stack pointer at top of memory?

    // 4) Load file
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // If file doesn't fit
    if (file_size > MEM_SIZE - 0x1000) {
        fclose(fp);
        simulationError();
    }
    size_t readCount = fread(cpu->memory + 0x1000, 1, file_size, fp);
    fclose(fp);
    if (readCount != (size_t)file_size) {
        simulationError();
    }

    // 5) Init opcode handler table
    initOpcodeHandlers();

    // 6) Fetch-decode-execute
    while (cpu->programCounter < 0x1000 + file_size) {
        // read instruction as 32-bit
        uint32_t instruction = *(uint32_t*)(cpu->memory + cpu->programCounter);
        // If your system is big-endian, do: instruction = le32toh(instruction);

        // decode
        uint8_t opcode = (instruction >> 27) & 0x1F;
        uint8_t rd     = (instruction >> 22) & 0x1F;
        uint8_t rs     = (instruction >> 17) & 0x1F;
        uint8_t rt     = (instruction >> 12) & 0x1F;
        uint16_t imm   = (uint16_t)(instruction & 0xFFF);
        uint64_t L = 0;

        // Expand immediate decoding
        // Some instructions require sign extension, others treat it as unsigned
        switch(opcode) {
            // brr L => sign-extend
            case 0xA: {
                int16_t sImm = imm;
                if (imm & 0x800) {
                    sImm |= 0xF000;
                }
                L = (int64_t)sImm;
                break;
            }
            // mov rd, (rs)(L) => also sign-extend
            case 0x10:
            // mov (rd)(L), rs => sign-extend
            case 0x13: {
                int16_t sImm = imm;
                if (imm & 0x800) {
                    sImm |= 0xF000;
                }
                L = (int64_t)sImm;
                break;
            }
            // addi, subi, shftri, shftli, mov rd, L => treat L as unsigned
            case 0x19: // addi
            case 0x1B: // subi
            case 0x5:  // shftri
            case 0x7:  // shftli
            case 0x12: // mov rd, L
                L = imm;
                break;
            default:
                L = 0;
                break;
        }

        // 7) Dispatch
        if (!opHandlers[opcode]) {
            // unhandled opcode => simulation error
            simulationError();
        }
        // call the correct handler
        opHandlers[opcode](cpu, rd, rs, rt, L);
    }

    // If we exit the loop, no halt encountered => error
    simulationError();
    return 0; // never reached
}
