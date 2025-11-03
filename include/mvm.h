#ifndef _MVM_
#define _MVM_

#define INSTRUMENT                                                                                                     \
    {                                                                                                                  \
        asm volatile("jmp 1f; cli;nop;nop;nop;nop;cli;nop;nop;nop;nop;cli;nop;nop;nop;nop; 1:" ::);                    \
    };

#endif
