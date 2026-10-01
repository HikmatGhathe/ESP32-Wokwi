# STM32C0 Mini RTOS — SysTick Context Switching

A bare-metal educational RTOS prototype for STM32C0 / ARM Cortex-M0+ written in C with inline ARM assembly.

## What this project demonstrates

- Direct memory-mapped access to STM32 RCC and GPIO registers
- Manual task stacks and Task Control Blocks (TCBs)
- Construction of the Cortex-M exception stack frame
- SysTick-driven periodic preemptive scheduling
- Saving and restoring CPU context during a task switch
- Cortex-M0+ handling of R4–R11 registers
- Two independent tasks: LED blinking and a background counter
- No vendor HAL and no external RTOS used for the scheduler core

## Architecture

```text
                   SysTick interrupt
                          |
                          v
                +--------------------+
                | Save CPU context   |
                +---------+----------+
                          |
                          v
                +--------------------+
                | Select next TCB    |
                +---------+----------+
                          |
                          v
                +--------------------+
                | Restore CPU context|
                +---------+----------+
                          |
                          v
                     Resume task
```

Each task has its own stack. The TCB stores the stack pointer used by the scheduler when suspending and resuming a task.

## Low-level concepts

The project is intentionally close to the Cortex-M hardware. It combines C with Thumb assembly to demonstrate:

- Hardware-saved exception frames: R0–R3, R12, LR, PC and xPSR
- Software-saved registers: R4–R11
- SysTick exception handling
- MSP manipulation
- EXC_RETURN-based exception return
- Manual stack-frame construction
- Round-robin task selection

## Project structure

```text
STM32C0-Mini-RTOS/
├── README.md
└── src/
    └── main.c
```

## Important hardware note

The memory addresses and SysTick reload value are device- and clock-dependent. They must be verified against the exact STM32C0 device reference manual and system-clock configuration before flashing real hardware.

The repository contains the scheduler prototype rather than a complete production firmware project. A real build also needs MCU-specific startup code, vector table, linker script and toolchain configuration.

## Educational scope

This is **not intended to replace a production RTOS**. The purpose is to understand what an RTOS scheduler is doing underneath APIs such as task creation, scheduling and context switching.

The project is especially useful for learning:

- Embedded C
- ARM Cortex-M architecture
- Interrupts and SysTick
- Stack frames
- Context switching
- Register preservation
- Bare-metal programming
- RTOS fundamentals

## Author

Hikmat Ghathe

GitHub: https://github.com/HikmatGhathe
