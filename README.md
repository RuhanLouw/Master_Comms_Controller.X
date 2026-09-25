# Master Controller Firmware

Firmware version 2.06 runs on the AVR128DA48 master card. It communicates with the Raspberry Pi as Modbus RTU unit 1 on USART0 and polls the sensor card as unit 2 on USART3. Both links operate at 9600 baud, 8 data bits, no parity and one stop bit.

## Active application

- The master polls and mirrors all 33 sensor-card registers once per second.
- Manual mode is selected after reset. Dashboard coils control the discrete outputs, while registers 39 and 46 provide the manual EEV and heater duties.
- Automatic mode applies the ramp-limited trajectory, chamber PID, heat/cool mode selection, cold-box free cooling, compressor minimum on/off times, fan sequencing, heater duty and EEV control.
- The heater output on PD0 uses a one-second time-proportioning period with a 0-1000 duty command.
- The EEV output on PC6 uses a six-second time-proportioning period with a 0-100 percent duty command.
- The EEV controller uses the filtered NTC6-minus-NTC5 evaporator temperature difference as its feedback proxy. It is not pressure-derived refrigerant superheat.
- Safety faults are latched. Register 34 may be written to zero only after the underlying condition has cleared.
- Automatic defrost is not implemented because no validated duration or termination condition is available. The defrost output remains available in manual mode.

## Startup behavior

The controller starts in manual mode with every process output off. Sensor communication and the required temperature channels must become valid before outputs are permitted. Changing register 23 to zero selects automatic mode. The default compressor minimum on and off times are 900 seconds and 600 seconds respectively.

Before automatic EEV operation, confirm that NTC5 is on the evaporator inlet and NTC6 is on the evaporator outlet. During operation, register 14 must represent a positive outlet-minus-inlet temperature difference. A negative value is rejected by the EEV controller and produces a latched EEV fault.

## Build and test

Run the host tests from PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\run_host_tests.ps1
```

The production image is generated at:

`dist/default/production/Master_Comms_Controller.X.production.hex`
