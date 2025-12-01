#ifndef _CKPT_SETUP_
#define _CKPT_SETUP_

#include <stdint.h>

void _tls_setup();

void _restore_area(int8_t *);

void _set_ckpt(int8_t *);

#endif