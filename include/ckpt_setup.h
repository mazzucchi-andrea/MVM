#ifndef _CKPT_SETUP_
#define _CKPT_SETUP_

#include <stdint.h>

int tls_setup();

void restore_area(int8_t *);

void set_ckpt(int8_t *);

#endif