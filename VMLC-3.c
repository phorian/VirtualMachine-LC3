#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <Windows.h>
#include <conio.h>  // _kbhit

#include "Registers.h"
#include "Condition_flag.h"
#include "Opcodes.h"
#include "trapCodes.h"


/* Memory mapped Registers */
enum
{
    MR_KBSR = 0xFE00,   /* Keyboard status */
    MR_KBDR = 0xFE02    /* Keyboard data */
};

/* Input Buffering --> Windows */
HANDLE hStdin = INVALID_HANDLE_VALUE;
DWORD fdwMode, fdwOLdMode;

void disable_input_buffering()
{
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &fdwOLdMode); /* save old mode */
    fdwMode = fdwOLdMode
            ^ ENABLE_ECHO_INPUT  /* no input echo */
            ^ ENABLE_LINE_INPUT; /* return when one or more characters are available */
    SetConsoleMode(hStdin, fdwMode); /* set new mode */
    FlushConsoleInputBuffer(hStdin); /* clear buffer */
}

void restore_input_buffering()
{
    SetConsoleMode(hStdin, fdwOLdMode);
}

uint16_t check_key()
{
    return WaitForSingleObject(hStdin, 1000) == WAIT_OBJECT_0 && _kbhit();
}

/* Handle Interrupe */
void handle_interrupt(int signal)
{
    restore_input_buffering();
    printf("\n");
    exit(-2);
}

/* Memory Access */
void mem_write(uint16_t address, uint16_t val)
{
    memory[address] = val;
};

uint16_t mem_read(uint16_t address)
{
    if (address == MR_KBSR)
    {
        if(check_key())
        {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBSR] = getchar();
        }
        else 
        {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
};


#define MEM_MAX (1 << 16)
uint16_t memory[MEM_MAX];  /* 65536 locations */

uint16_t reg[R_COUNT];


/* Sign Extend: ---> This is useful when using immediate mode to extend negative numbers */

uint16_t sign_extend(uint16_t x, int bit_count)
{
    if((x >> (bit_count - 1)) & 1){
        x |= (0xFFFF << bit_count);
    }
    return x;
}

/* Update Flags */
void update_flags(uint16_t r)
{
    if(reg[r] == 0)
    {
        reg[R_COND] == FL_ZRO;
    }
    else if (reg[r] >> 15) /* a 1 in the leftmost bit indicates negative*/
    {
        reg[R_COND] == FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}

/* Swap */
uint16_t swap16(uint16_t x)
{
    return(x << 8) | (x >> 8);
}

/* Read Image FIle */
void read_image_file(FILE* file)
{
    /* origin is where the image is placed in memory */
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    uint16_t max_read = MEM_MAX - origin;
    uint16_t* p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    /* swap to little endian */
    while (read-- > 0)
    {
        *p = swap16(*p);
        ++p;
    }
}

/* Read Image */
int read_image(const char* image_path)
{
    FILE* file = fopen(image_path, "rb");
    if(!file) { 
        return 0; 
        };
        read_image_file(file);
        fclose(file);
        return 1;
}

int main(int argc, const char* argv[])
{
    /* ---Load Arguments--- */
    if (argc < 2)
    {
        printf("lc3 [image-file]...\n");
        exit(2);
    }

    for (int j =1; j < argc; ++j)
    {
        if(!read_image(argv[j]))
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }
    //Setup
    signal(SIGINT, handle_interrupt);
    disable_input_buffering;

    //One condition flag should be set at any given time, we will use the Z flag
    reg[R_COND] = FL_ZRO;

    /* Set PC to start position: 0x3000 is the default */
    enum 
    { 
        PC_START = 0x3000
    };
    reg[R_PC] = PC_START;

    int running = 1;
    while (running)
    {
        /* FETCH */
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;

        switch (op)
        {
        case OP_ADD:
            /* ADD */
            {
                /* Set Destination Register (DR) */
                uint16_t r0 = (instr >> 9) & 0x7;

                /* first operand/input (SR1) */
                uint16_t r1 = (instr >> 6) & 0x7;

                /* Check mode if it is immediate */
                uint16_t imm_flag = (instr >> 5) & 0x1;

                if(imm_flag)
                {
                    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                    reg[r0] = reg[r1] + imm5;
                }
                else
                {
                    uint16_t r2 = instr & 0x7;
                    reg[r0] = reg[r1] + reg[r2];
                }

                update_flags(r0);
            }
            break;
        
        case OP_AND:
            //AND
            {
                /* Set Destination Register (DR) */
                uint16_t r0 = (instr >> 9) & 0x7;

                /* first operand/input (SR1) */
                uint16_t r1 = (instr >> 6) & 0x7;

                /* Check mode if it is immediate */
                uint16_t imm_flag = (instr >> 5) & 0x1;

                if(imm_flag)
                {
                    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                    reg[r0] = reg[r1] & imm5;
                }
                else
                {
                    uint16_t r2 = instr & 0x7;
                    reg[r0] = reg[r1] & reg[r2];
                }

                update_flags(r0);
            }
            break;
        
        case OP_NOT:
            //NOT
            {
                /* Set Destination Register (DR) */
                uint16_t r0 = (instr >> 9) & 0x7;

                /* first operand/input (SR1) */
                uint16_t r1 = (instr >> 6) & 0x7;

                reg[r0] = ~reg[r1];
                update_flags(r0);
            }
            break;
        
        case OP_BR:
            //BR
            {
                /* PCoffset9: address that tells where to load from */
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                uint16_t cond_flag = (instr >> 9) & 0x7;
                if(cond_flag & reg[R_COND])
                {
                    reg[R_PC] += pc_offset;
                }
            }
            break;
        
        case OP_JMP:
            //JMP: --> also handles RET when R1 = 7
            {
                uint16_t r1 = (instr >> 6) & 0x7;
                reg[R_PC] = reg[r1];
            }
            break;
        
        case OP_JSR:
            //JSR
            {
                uint16_t long_flag = ( instr >> 11) & 1;
                reg[R_R7] = reg[R_PC];
                if(long_flag)
                {
                    uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
                    reg[R_PC] += long_pc_offset; //JSR
                }
                else
                {
                    uint16_t r1 = (instr >> 6) & 0x7;
                    reg[R_PC] = reg[r1]; //JSRR
                }
            }
            break;
        
        case OP_LD:
            //LD
            {
                uint16_t r0 = (instr >> 9 ) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = mem_read(reg[R_PC] + pc_offset);
                update_flags(r0);
            }
            break;
        
        case OP_LDI:
            /* LDI: This instruction is used to load a value from a location in memory into a register */
            {
                /* Set destination register (DR) */
                uint16_t r0 = (instr >> 9) & 0x7;

                /* PCoffset9: address that tells where to load from */
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

                /* add pc_offset to current PC, look at mem location to get final addr*/
                reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
                update_flags(r0);
            }
            break;
        
        case OP_LDR:
            //LDR
            {
            uint16_t r0 = (instr >> 9) & 0x7;
            uint16_t r1 = (instr >> 6) & 0x7;
            uint16_t offset = sign_extend(instr & 0x3F, 6);
            reg[r0] = mem_read(reg[r1] + offset);
            update_flags(r0);
            }
            break;
        
        case OP_LEA:
            //LEA
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                reg[r0] = reg[R_PC] + pc_offset;
                update_flags(r0);
            }
            break;
        
        case OP_ST:
            //ST
            {
            uint16_t r0 = (instr >> 9) & 0x7;
            uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
            mem_write(reg[R_PC] + pc_offset, reg[r0]);
            }
            break;
        
        case OP_STI:
            //STI
            {
                uint16_t r0 = (instr >> 9) & 0x7;
                uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
                mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
            }
            break;
        
        case OP_STR:
            //STR
            {
            uint16_t r0 = (instr >> 9) & 0x7;
            uint16_t r1 = (instr >> 6) & 0x7;
            uint16_t offset = sign_extend(instr & 0x3F, 6);
            mem_write(reg[r1] + offset, reg[r0]);
            }
            break;
        
        case OP_TRAP:
            //TRAP: --> Used for interacting with I/O devices 

            reg[R_R7] = reg[R_PC];

            switch (instr & 0xFF)
            {
                case TRAP_GETC:
                    /* read a single ASCII char */
                    {
                    reg[R_R0] = (uint16_t)getchar();
                    update_flags(R_R0);
                    }
                    break;
            
                case TRAP_OUT:
                    {
                    putc((char)reg[R_R0], stdout);
                    fflush(stdout);
                    }
                    break;
            
                case TRAP_PUTS:
                    {
                        uint16_t* c = memory + reg[R_R0];
                        while (*c)
                        {
                            putc((char)*c, stdout);
                            ++c;
                        };
                        fflush(stdout);    
                    }
                    break;
            
                case TRAP_IN:
                    {
                    printf("Enter a character: ");
                    char c = getchar();
                    putc(c, stdout);
                    fflush(stdout);
                    reg[R_R0] = (uint16_t)c;
                    update_flags(R_R0);
                    }
                    break;
            
                case TRAP_PUTSP:
                    {
                        uint16_t* c = memory + reg[R_R0];
                        while(*c)
                        {
                            char char1 = (*c) & 0xFF;
                            putc(char1, stdout);
                            char char2 = (*c) >> 8;
                            if(char2) putc(char2, stdout);
                            ++c;
                        }
                        fflush(stdout);
                    }
                    break;
            
                case TRAP_HALT:
                    {
                        puts("HALT");
                        fflush(stdout);
                        running = 0;
                    }
                    break;
            
                default:
                    break;
            };
            break;
        
        case OP_RES:
        case OP_RTI:
        
        default:
            //BAD OPCODE
            abort();
            break;
        }
    }
    //Shutdown
    restore_input_buffering();
}

