#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <Windows.h>
#include <conio.h>  // _kbhit

#include "Registers.h"
#include "Condition_flag.h"
#include "Opcodes.h"
#include "trapCodes.h"


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

#define MEM_MAX(1 << 16)

uint16_t memory[MEM_MAX];  /* 65536 locations */

uint16_t reg[R_COUNT];

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
}

