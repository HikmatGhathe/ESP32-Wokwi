# Embedded RTOS Projects

A collection of embedded systems projects developed and simulated with Wokwi.

## Projects

### FreeRTOS Industrial Gateway

ESP32/FreeRTOS project with three concurrent tasks for sensor acquisition, MQTT communication, and local alarm control. Shared data is protected with a FreeRTOS mutex.

- ESP32
- C++
- FreeRTOS tasks and mutex synchronization
- DHT22 sensor
- WiFi
- MQTT
- JSON status messages
- Wokwi simulation

Project folder: [FreeRTOS-Industrial-Gateway](./FreeRTOS-Industrial-Gateway)

Wokwi project: https://wokwi.com/projects/475772929646558209

### NanoRTOS - Bare-Metal STM32 Kernel

Educational bare-metal STM32C0 / ARM Cortex-M0+ scheduler prototype demonstrating manual task stacks, Task Control Blocks, SysTick-based context switching, register save/restore, and direct memory-mapped register access.

- STM32C0 / ARM Cortex-M0+
- C
- Bare-metal programming
- SysTick
- Task Control Blocks
- Manual stack frames
- Inline ARM assembly
- Wokwi simulation

Project folder: [NanoRTOS-Bare-Metal-STM32-Kernel](./NanoRTOS-Bare-Metal-STM32-Kernel)

Wokwi project: https://wokwi.com/projects/476763243665371137
