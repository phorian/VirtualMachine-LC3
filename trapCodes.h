#ifndef TRAPCODES_H
#define TRAPCODES_H

enum
{
    TRAP_GETC = 0x20,   /* get character from keyboard, not echoed onto the terminal */
    TRAP_OUT =0x21,     /* Output a character */
    TRAP_PUTS = 0x22,   /* output a word string */
    TRAP_IN = 0x23,     /* get character from keyboard, echoed onto the terminal */
    TRAP_PUTSP = 0x24,  /* output a byte string */
    TRAP_HALT = 0x25    /* halt the program */
};

#endif