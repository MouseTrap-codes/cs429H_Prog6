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

    memset(cpu, 0, sizeof(*cpu));
    return cpu;
}

// handling integer arithmetic instructions
void overflowErrorMessage() {
    printf("Signed integer overflow!!!");
}

// Performs signed addition of two 64-bit signed values in registers rs and rt and stores the result in register rd.
void handleAdd(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    //printf("Called! Add\n");

    int64_t val1 = (int64_t)cpu->registers[rs];
    int64_t val2 = (int64_t)cpu->registers[rt];

    int64_t result = val1 + val2;


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



    cpu->registers[rd] = (uint64_t)(val1 * val2);
    cpu->programCounter += 4;
}

// Performs signed division of two 64-bit signed values in registers rs and rt and stores the result in register rd.
void handleDiv(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    int64_t val1 = (int64_t)cpu->registers[rs];
    int64_t val2 = (int64_t)cpu->registers[rt];

    // Division-by-zero check
    if (val2 == 0) {
        fprintf(stderr, "Simulation error");
        exit(1);
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
void priv(CPU* cpu, int rd, int rs, int rt, uint64_t L) {
    //printf("Called! Priv\n");
    switch (L) {
        case 0x0: // Halt instruction: stop simulation
            //printf("RAVI TEJ 1\n");
            exit(0);
            break;
        case 0x1: // Trap instruction: switch to supervisor mode and handle system call
            //printf("RAVI TEJ 2\n");
            cpu->userMode = 0; // false because we are now in supervisor mode
            cpu->programCounter += 4;
            break;
        case 0x2: // RTE instruction: return from exception, switch back to user mode
            cpu->userMode = 1; // true because we are back in user mode
            cpu->programCounter += 4;
            break;
        case 0x3: // Input instruction: rd <- Input[rs]
            //printf("INPUT");
            if (cpu->registers[rs] != 0) {
                printf("unsupported port for input");
                return;
            }
            int64_t input;
            scanf("%lld", &input);
        
            cpu->registers[rd] = (uint64_t)input;
            cpu->programCounter += 4;
            break;
        case 0x4: // Output instruction: Output[rd] <- rs
            //printf("OUTPUT");
            if (cpu->registers[rd] != 1) {
                printf("unsupported port for output");
                return;
            }
            printf("%llu", cpu->registers[rs]);
            cpu->programCounter += 4;
            break;
        default: // Illegal L value: undefined priv operation
            fprintf(stderr, "Simulation error");
            exit(1);
    }
    // Note: The program counter (PC) is incremented by 4 after every instruction
    // in the main simulation loop (except for control instructions), so it is not
    // updated here.
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
    double val1 = 0;
    double val2 = 0;

    memcpy(&val1,&registers[rs],sizeof(uint64_t));
    memcpy(&val2,&registers[rt],sizeof(uint64_t));

    double result = val1 + val2;

    memcpy(&registers[rd],&result,sizeof(uint64_t));

    cpu->programCounter += 4;
}

// Performs signed subtraction of two double precision values in registers rs and rt , and stores the result in register rd
void handleSubf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    /double val1 = 0;
    double val2 = 0;

    memcpy(&val1,&registers[rs],sizeof(uint64_t));
    memcpy(&val2,&registers[rt],sizeof(uint64_t));

    double result = val1 - val2;

    memcpy(&registers[rd],&result,sizeof(uint64_t));

    // Move to the next instruction
    cpu->programCounter += 4;
}

// Performs signed multiplication of two double precision values in registers rs and rt , and stores the result in register rd
void handleMulf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    double val1 = 0;
    double val2 = 0;

    memcpy(&val1,&registers[rs],sizeof(uint64_t));
    memcpy(&val2,&registers[rt],sizeof(uint64_t));

    double result = val1 * val2;

    memcpy(&registers[rd],&result,sizeof(uint64_t));

    // Move to the next instruction
    cpu->programCounter += 4;
}

// Performs signed division of two double precision values in registers rs and rt , and stores the result in register rd 
void handleDivf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt) {
    double val1 = 0;
    double val2 = 0;

    if (val2 == 0) {
        fprintf(stderr, "Simulation error");
        exit(1);
    }

    memcpy(&val1,&registers[rs],sizeof(uint64_t));
    memcpy(&val2,&registers[rt],sizeof(uint64_t));

    double result = val1 / val2;

    memcpy(&registers[rd],&result,sizeof(uint64_t));

    // Move to the next instruction
    cpu->programCounter += 4;
}

// here we implement the function array for O(1) access based on opcode
// Uniform Instruction Handler Type and Wrappers
typedef void (*InstructionHandler)(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L);

// For instructions that ignore the immediate value L:
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

// Data Movement wrappers
void wrapperHandleMovRdRsL(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleMovRdRsL(cpu, rd, rs, rt, (int64_t)L); }
void wrapperMovRdRs(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { movRdRs(cpu, rd, rs); }
void wrapperHandleMovRdL(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleMovRdL(cpu, rd, (uint16_t)L); }
void wrapperHandleMovRDLRs(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleMovRDLRs(cpu, rd, rs, L); }

// Floating Point wrappers
void wrapperHandleAddf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleAddf(cpu, rd, rs, rt); }
void wrapperHandleSubf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleSubf(cpu, rd, rs, rt); }
void wrapperHandleMulf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleMulf(cpu, rd, rs, rt); }
void wrapperHandleDivf(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) { handleDivf(cpu, rd, rs, rt); }

// Privileged instructions wrapper
// The opcode for privileged instructions is 0xF. We dispatch based on L.
void wrapperHandlePriv(CPU* cpu, uint8_t rd, uint8_t rs, uint8_t rt, uint64_t L) {
    //printf("I was called\n");
   // printf("%lu\n", L);
    priv(cpu, rd, rs, rt, L);
}


// Global Function Pointer Array (256 entries)
InstructionHandler opHandlers[256] = {0};

void initOpcodeHandlers() {
    // Clear array (if not already zeroed)
    for (int i = 0; i < 256; i++) {
        opHandlers[i] = NULL;
    }
    // Use opcodes exactly as defined in the manual:

    // Logic Instructions (0x0 - 0x3)
    opHandlers[0x0] = wrapperHandleAnd;   // and rd, rs, rt
    opHandlers[0x1] = wrapperHandleOr;    // or rd, rs, rt
    opHandlers[0x2] = wrapperHandleXor;   // xor rd, rs, rt
    opHandlers[0x3] = wrapperHandleNot;   // not rd, rs

    // Shift Instructions (0x4 - 0x7)
    opHandlers[0x4] = wrapperHandleShftR;  // shftr rd, rs, rt
    opHandlers[0x5] = wrapperHandleShftRI; // shftri rd, L
    opHandlers[0x6] = wrapperHandleShftL;  // shftl rd, rs, rt
    opHandlers[0x7] = wrapperHandleShftLI; // shftli rd, L

    // Control Instructions (0x8 - 0xE)
    opHandlers[0x8] = wrapperHandleBr;     // br rd
    opHandlers[0x9] = wrapperHandleBrr;    // brr rd
    opHandlers[0xA] = wrapperHandleBrrL;   // brr L
    opHandlers[0xB] = wrapperHandleBrnz;   // brnz rd, rs
    opHandlers[0xC] = wrapperHandleCall;   // call rd, rs, rt
    opHandlers[0xD] = wrapperHandleReturn; // return
    opHandlers[0xE] = wrapperHandleBrgt;   // brgt rd, rs, rt

    // Privileged Instruction (opcode 0xF)
    opHandlers[0xF] = wrapperHandlePriv;   // priv rd, rs, rt, L

    // Data Movement Instructions (0x10 - 0x13)
    opHandlers[0x10] = wrapperHandleMovRdRsL; // mov rd, (rs)(L)
    opHandlers[0x11] = wrapperMovRdRs;        // mov rd, rs
    opHandlers[0x12] = wrapperHandleMovRdL;     // mov rd, L
    opHandlers[0x13] = wrapperHandleMovRDLRs;   // mov (rd)(L), rs

    // Floating Point Instructions (0x14 - 0x17)
    opHandlers[0x14] = wrapperHandleAddf;   // addf rd, rs, rt
    opHandlers[0x15] = wrapperHandleSubf;   // subf rd, rs, rt
    opHandlers[0x16] = wrapperHandleMulf;   // mulf rd, rs, rt
    opHandlers[0x17] = wrapperHandleDivf;   // divf rd, rs, rt

    // Integer Arithmetic Instructions (0x18 - 0x1D)
    opHandlers[0x18] = wrapperHandleAdd;    // add rd, rs, rt
    opHandlers[0x19] = wrapperHandleAddI;   // addi rd, L
    opHandlers[0x1A] = wrapperHandleSub;    // sub rd, rs, rt
    opHandlers[0x1B] = wrapperHandleSubI;   // subi rd, L
    opHandlers[0x1C] = wrapperHandleMul;    // mul rd, rs, rt
    opHandlers[0x1D] = wrapperHandleDiv;    // div rd, rs, rt
}



/// we also need to make a method to parse the lines of binary
/// to get the necessary information ie opcode rd rs rt L etc











int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program.tko>\n", argv[0]);
        exit(1);
    }
    
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) {
        fprintf(stderr, "Invalid tinker filepath");
        exit(1);
    }
    
    CPU* cpu = createCPU();
    cpu->registers[31] = MEM_SIZE;  // Stack pointer initialization (call/return not fixed)
    cpu->programCounter = 0x1000;
    
    // Load the object code into memory starting at address 0x1000.
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
    
    // Initialize the opcode function array.
    initOpcodeHandlers();
    
    // Fetch and execute instructions based on the program counter.
    while (cpu->programCounter < 0x1000 + file_size) {
        uint32_t instruction = *(uint32_t*)(cpu->memory + cpu->programCounter);
        // Convert from little-endian to host order.
        instruction = le32toh(instruction);
        
        // Decode fields based on the Tinker Instruction Manual:
        // Bits 31-27: opcode (5 bits)
        // Bits 26-22: rd (5 bits)
        // Bits 21-17: rs (5 bits)
        // Bits 16-12: rt (5 bits)
        // Bits 11-0 : immediate L (12 bits) for instructions that use it.
        uint8_t opcode = (instruction >> 27) & 0x1F;
        //printf("opcode: 0x%x ", opcode);
        uint8_t rd     = (instruction >> 22) & 0x1F;
        //printf("rd: %d ", rd);
        uint8_t rs     = (instruction >> 17) & 0x1F;
        //printf("rs: %d ", rs);
        uint8_t rt     = (instruction >> 12) & 0x1F;
        //printf("rt: %d ", rt);
        uint16_t imm = instruction & 0xFFF;
        //printf("L: %d\n", imm);
        uint64_t L = 0;
        
        // For immediate instructions:
        // For brr L (opcode 0xA) we sign-extend the immediate since it can be negative.
        if (opcode == 0xA) {
            int64_t signedImm = imm;
            if (imm & 0x800) // If bit 11 is set, sign-extend.
                signedImm |= ~0xFFF;
            L = (uint64_t)signedImm;
        } else if (opcode == 0x19 || opcode == 0x1B || opcode == 0x12 || opcode == 0xf) {
            L = imm;
        }
        
        // Dispatch the instruction.
        if (opHandlers[opcode]) {
            //printf("%d\n", opcode);
            opHandlers[opcode](cpu, rd, rs, rt, L);
            //printRegisters(cpu);
        } else {
            fprintf(stderr, "Unhandled opcode: 0x%X\n", opcode);
        }
    }

    fprintf(stderr, "Simulation error");
    exit(1);
    return 0;
}