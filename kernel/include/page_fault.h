#ifndef PAGE_FAULT_H
#define PAGE_FAULT_H

#include "isr.h"

void page_fault_init(void);
void page_fault_handler(interrupt_frame_t *frame);

#endif
