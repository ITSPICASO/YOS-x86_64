#ifndef DOUBLE_FAULT_H
#define DOUBLE_FAULT_H

#include "isr.h"

void double_fault_init(void);
void double_fault_handler(interrupt_frame_t *frame);

#endif
