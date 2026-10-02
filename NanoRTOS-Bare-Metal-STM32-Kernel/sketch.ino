#include <stdint.h>

// ---------------------------------------------------------
// 1. HARDWARE REGISTER ADDRESSES (STM32C0 & Cortex-M)
// ---------------------------------------------------------
volatile uint32_t *RCC_IOPENR  = (volatile uint32_t *) 0x40021034;
volatile uint32_t *PortA_MODER = (volatile uint32_t *) 0x50000000;
volatile uint32_t *PortA_ODR   = (volatile uint32_t *) 0x50000014;

volatile uint32_t *SysTick_CTRL = (volatile uint32_t *) 0xE000E010;
volatile uint32_t *SysTick_LOAD = (volatile uint32_t *) 0xE000E014;
volatile uint32_t *SysTick_VAL  = (volatile uint32_t *) 0xE000E018;


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
    while(1) {
        *PortA_ODR ^= (1 << 5); // Toggle LED on PA5
        // A short software delay to make the blink visible to human eyes
        for(volatile int i = 0; i < 50000; i++); 
    }
}

void task2_counter(void) {
    volatile int my_counter = 0;
    while(1) {
        my_counter++; // Juggled in the background
    }
}


// ---------------------------------------------------------
// 4. STACK FORGERY (Initialization)
// ---------------------------------------------------------
void init_task(TCB_t *tcb, uint32_t *stack, void (*task_code)(void)) {
    uint32_t *top_of_stack = &stack[STACK_SIZE];

    // Hardware Auto-Saved Registers
    top_of_stack[-1] = 0x01000000;          // xPSR (Thumb mode)
    top_of_stack[-2] = (uint32_t)task_code; // PC (Starting function address)
    top_of_stack[-3] = 0xFFFFFFF9;          // LR (Return to Thread mode using Main Stack)
    top_of_stack[-4] = 0;                   // R12
    top_of_stack[-5] = 0;                   // R3
    top_of_stack[-6] = 0;                   // R2
    top_of_stack[-7] = 0;                   // R1
    top_of_stack[-8] = 0;                   // R0

    // Software Manually-Saved Registers
    top_of_stack[-9]  = 0; // R11
    top_of_stack[-10] = 0; // R10
    top_of_stack[-11] = 0; // R9
    top_of_stack[-12] = 0; // R8
    top_of_stack[-13] = 0; // R7
    top_of_stack[-14] = 0; // R6
    top_of_stack[-15] = 0; // R5
    top_of_stack[-16] = 0; // R4

    // Save initial paused location
    tcb->stack_pointer = &top_of_stack[-16];
}


// ---------------------------------------------------------
// 5. THE CONTEXT SWITCH (Cortex-M0+ Compatible)
// ---------------------------------------------------------
void update_next_task(void) {
    if (current_task == &task1_tcb) {
        current_task = &task2_tcb;
    } else {
        current_task = &task1_tcb;
    }
}

__attribute__((naked)) void SysTick_Handler(void) {
    __asm volatile (
        // 1. SAVE CURRENT TASK (M0+ Compatible)
        "PUSH {r4-r7}                   \n" 
        "MOV r4, r8                     \n"
        "MOV r5, r9                     \n"
        "MOV r6, r10                    \n"
        "MOV r7, r11                    \n"
        "PUSH {r4-r7}                   \n" 
        
        "LDR r0, =current_task          \n" 
        "LDR r1, [r0]                   \n" 
        "MRS r2, MSP                    \n" 
        "STR r2, [r1]                   \n" 
        
        // 2. SWAP THE TASKS
        "MOV r4, lr                     \n"
        "PUSH {r4, r5}                  \n" 
        "BL update_next_task            \n" 
        "POP {r4, r5}                   \n"
        "MOV lr, r4                     \n"
        
        // 3. RESTORE NEXT TASK
        "LDR r0, =current_task          \n" 
        "LDR r1, [r0]                   \n" 
        "LDR r2, [r1]                   \n" 
        "MSR MSP, r2                    \n" 
        
        "POP {r4-r7}                    \n"
        "MOV r8, r4                     \n"
        "MOV r9, r5                     \n"
        "MOV r10, r6                    \n"
        "MOV r11, r7                    \n"
        "POP {r4-r7}                    \n"
        
        // 4. RETURN
        "BX lr                          \n" 
    );
}


// ---------------------------------------------------------
// 6. MAIN FUNCTION
// ---------------------------------------------------------
int main(void) {
    // 1. Power on Port A and set Pin 5 as Output
    *RCC_IOPENR |= (1 << 0);
    *PortA_MODER &= ~(3 << 10);
    *PortA_MODER |= (1 << 10);

    // 2. Initialize Task Stacks
    init_task(&task1_tcb, task1_stack, task1_blink);
    init_task(&task2_tcb, task2_stack, task2_counter);
    current_task = &task1_tcb;

    // 3. Start the Heartbeat (Switch tasks every 1 millisecond)
    *SysTick_LOAD = 16000;
    *SysTick_VAL = 0;          
    *SysTick_CTRL = 7;         

    // 4. The Infinite Loop (Main does nothing, RTOS takes over)
    while(1) { }
}