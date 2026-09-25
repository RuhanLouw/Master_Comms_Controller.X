#ifndef COMMUNICATIONS_H
#define COMMUNICATIONS_H

#include <stdint.h>

void Communications_Initialize(void);
void Communications_Process(void);
uint32_t Communications_Millis(void);

#endif
