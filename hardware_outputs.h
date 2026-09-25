#ifndef HARDWARE_OUTPUTS_H
#define HARDWARE_OUTPUTS_H

#include <stdint.h>

void HardwareOutputs_Initialize(void);
void HardwareOutputs_Process(uint32_t now_ms);

#endif
