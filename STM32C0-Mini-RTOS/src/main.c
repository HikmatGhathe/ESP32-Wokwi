#include <stdint.h>

// ---------------------------------------------------------
// 1. HARDWARE REGISTER ADDRESSES (STM32C0 & Cortex-M)
// ---------------------------------------------------------
volatile uint32_t *RCC_IOPENR   = (volatile uint32_t *)0x40021034;
volatile uint32_t *PortA_MODER  = (volatile uint32_t *)0x50000000;
volatile uint32_t *PortA_ODR    = (volatile uint32_t *)0x50000014;

volatile uint32_t *SysTick_CTRL = (volatile uint32_t *)0xE000E010;
volatile uint32_t *SysTick_LOAD = (volatile uint32_t *)0xE000E014;
volatile uint32_t *SysTick_VAL  = (volatile uint32_t *)0xE000E018;

// ---------------------------------------------------------
// 2. RTOS MEMORY SETUP (Stacks and TCBs)
// ---------------------------------------------------------
#define STACK_SIZE 100

typedef struct {
    uint32_t *stack_pointer;
} TCB_t;

uint32_t task1_stack[STACK_SIZE];
uint32_t task2_stack[STACK_SIZE];

TCB_t task1_tcb;
TCB_t task2_tcb;
TCB_t *current_task;

// ---------------------------------------------------------
// 3. THE TASKS
// ---------------------------------------------------------
void task1_blink(void) {
    while (1) {
        *PortA_ODR ^= (1U << 5); // Toggle LED on PA5

        // Simple software delay so the LED blink is visible.
        for (volatile uint32_t i = 0; i < 50000U; i++) {
        }
    }
}

void task2_counter(void) {
    volatile uint32_t my_counter = 0;

    while (1) {
        my_counter++;
    }
}

// ---------------------------------------------------------
// 4. STACK FRAME INITIALIZATION
// ---------------------------------------------------------
void init_task(TCB_t *tcb, uint32_t *stack, void (*task_code)(void)) {
    uint32_t *top_of_stack = &stack[STACK_SIZE];

    // Hardware exception frame restored automatically by Cortex-M.
    top_of_stack[-1] = 0x01000000U;          // xPSR: Thumb state
    top_of_stack[-2] = (uint32_t)task_code;  // PC: task entry point
    top_of_stack[-3] = 0xFFFFFFF9U;          // LR: EXC_RETURN
    top_of_stack[-4] = 0U;                   // R12
    top_of_stack[-5] = 0U;                   // R3
    top_of_stack[-6] = 0U;                   // R2
    top_of_stack[-7] = 0U;                   // R1
    top_of_stack[-8] = 0U;                   // R0

    // Software-saved registers restored by SysTick_Handler.
    top_of_stack[-9]  = 0U; // R11
    top_of_stack[-10] = 0U; // R10
    top_of_stack[-11] = 0U; // R9
    top_of_stack[-12] = 0U; // R8
    top_of_stack[-13] = 0U; // R7
    top_of_stack[-14] = 0U; // R6
    top_of_stack[-15] = 0U; // R5
    top_of_stack[-16] = 0U; // R4

    // Point to the beginning of the software-saved context.
    tcb->stack_pointer = &top_of_stack[-16];
}

// ---------------------------------------------------------
// 5. TASK SELECTION
// ---------------------------------------------------------
void update_next_task(void) {
    if (current_task == &task1_tcb) {
        current_task = &task2_tcb;
    } else {
        current_task = &task1_tcb;
    }
}

// ---------------------------------------------------------
// 6. START FIRST TASK
// ---------------------------------------------------------
// Load the first task's fabricated exception frame into MSP and
// use EXC_RETURN so the Cortex-M hardware restores the frame and
// starts execution at the task's PC.
__attribute__((naked)) void start_first_task(void) {
    __asm volatile (
        "LDR r0, =current_task     \n"
        "LDR r0, [r0]              \n"
        "LDR r0, [r0]              \n"
        "MSR MSP, r0               \n"
        "LDR r0, =0xFFFFFFF9       \n"
        "MOV lr, r0                \n"
        "BX lr                     \n"
    );
}

// ---------------------------------------------------------
// 7. CONTEXT SWITCH (Cortex-M0+)
// ---------------------------------------------------------
__attribute__((naked)) void SysTick_Handler(void) {
    __asm volatile (
        // Save low registers R4-R7.
        "PUSH {r4-r7}             \n"

        // Move high registers R8-R11 into low registers so they
        // can be saved using the Cortex-M0+ PUSH instruction.
        "MOV r4, r8               \n"
        "MOV r5, r9               \n"
        "MOV r6, r10              \n"
        "MOV r7, r11              \n"
        "PUSH {r4-r7}             \n"

        // Save the current task's MSP in its TCB.
        "LDR r0, =current_task    \n"
        "LDR r1, [r0]             \n"
        "MRS r2, MSP              \n"
        "STR r2, [r1]             \n"

        // Select the next task.
        "MOV r4, lr               \n"
        "PUSH {r4, r5}            \n"
        "BL update_next_task      \n"
        "POP {r4, r5}             \n"
        "MOV lr, r4               \n"

        // Load the next task's saved MSP.
        "LDR r0, =current_task    \n"
        "LDR r1, [r0]             \n"
        "LDR r2, [r1]             \n"
        "MSR MSP, r2              \n"

        // Restore R8-R11.
        "POP {r4-r7}              \n"
        "MOV r8, r4               \n"
        "MOV r9, r5               \n"
        "MOV r10, r6              \n"
        "MOV r11, r7              \n"

        // Restore R4-R7.
        "POP {r4-r7}              \n"

        // EXC_RETURN restores the hardware exception frame and
        // resumes the selected task.
        "BX lr                    \n"
    );
}

// ---------------------------------------------------------
// 8. MAIN
// ---------------------------------------------------------
int main(void) {
    // Enable GPIOA and configure PA5 as output.
    *RCC_IOPENR |= (1U << 0);
    *PortA_MODER &= ~(3U << 10);
    *PortA_MODER |= (1U << 10);

    // Build the initial stack frame for both tasks.
    init_task(&task1_tcb, task1_stack, task1_blink);
    init_task(&task2_tcb, task2_stack, task2_counter);
    current_task = &task1_tcb;

    // Configure SysTick for a nominal 1 ms period when the CPU
    // clock is 16 MHz. Reload values are hardware/clock dependent.
    *SysTick_LOAD = 16000U - 1U;
    *SysTick_VAL = 0U;
    *SysTick_CTRL = 7U; // ENABLE | TICKINT | CLKSOURCE

    // Transfer control to the first task.
    start_first_task();

    while (1) {
        // Scheduler/tasks own execution after start_first_task().
    }
}
