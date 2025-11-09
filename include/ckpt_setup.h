#ifndef _CKPT_SETUP_
#define _CKPT_SETUP_

#include <stdint.h>

void *tls_setup();

void restore_area(int8_t *);

#endif