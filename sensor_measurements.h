#ifndef SENSOR_MEASUREMENTS_H
#define SENSOR_MEASUREMENTS_H

#include <stdint.h>

void SensorMeasurements_Initialize(void);
void SensorMeasurements_OnValidFrame(uint32_t now_ms);
void SensorMeasurements_Process(uint32_t now_ms);

#endif
