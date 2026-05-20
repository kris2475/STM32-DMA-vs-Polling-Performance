#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>

/* --- GLOBAL STATE VARIABLES --- */
volatile uint32_t msTicks = 0;         // 1ms timebase for debouncing
volatile uint32_t overhead = 0;      // Stores the performance measurement
volatile uint8_t  buttonFlag = 0;    // Signal from Interrupt to Main loop
volatile uint8_t  useDMA = 0;        // Toggle state: 0 = CPU Polling, 1 = DMA
char msg_buffer[128];                // Shared buffer for UART data

/* --- SYSTICK: 1ms HEARTBEAT --- */
void SysTick_Handler(void) {
    msTicks++; 
}

/* --- PERFORMANCE MONITOR: DWT CYCLE COUNTER --- */
// Measures exact CPU clock cycles to show the efficiency of DMA.
void TIMING_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; 
    DWT->CYCCNT = 0;                                
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            
}

/* --- MODE 0: CPU POLLING (BLOCKING) --- */
void CPU_UART2_Send(char* str) {
    uint32_t start = DWT->CYCCNT; // Start timing
    
    while (*str) {
        // CPU is TRAPPED here until the UART hardware clears the TX register
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = (*str++ & 0xFF);
    }
    
    overhead = DWT->CYCCNT - start; // Capture blocking time
}

/* --- MODE 1: DMA CONFIGURATION (NON-BLOCKING) --- */
void DMA_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN; // Enable DMA1 clock
    
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;   // Disable stream for configuration
    while (DMA1_Stream6->CR & DMA_SxCR_EN); 

    // Destination: UART2 Data Register
    DMA1_Stream6->PAR = (uint32_t)&(USART2->DR);    
    // Source: Our RAM buffer
    DMA1_Stream6->M0AR = (uint32_t)msg_buffer;      
    
    // Configuration:
    // CHSEL(4): UART2 TX is on Channel 4
    // MINC: Increment memory pointer after each byte
    // DIR(01): Direction is Memory-to-Peripheral
    DMA1_Stream6->CR = (4UL << 25) | DMA_SxCR_MINC | DMA_SxCR_DIR_0;
}

/* --- DMA EXECUTION TRIGGER --- */
void DMA_UART2_Send(uint16_t size) {
    uint32_t start = DWT->CYCCNT; // Start timing

    USART2->CR3 |= USART_CR3_DMAT;          // Enable UART-DMA hardware link
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;       // Reset stream
    while (DMA1_Stream6->CR & DMA_SxCR_EN); 
    DMA1_Stream6->NDTR = size;              // Set number of bytes to move
    DMA1->HIFCR |= (0x3FUL << 16);          // Clear all Stream 6 status flags
    DMA1_Stream6->CR |= DMA_SxCR_EN;        // START DMA: Hardware takes over!

    overhead = DWT->CYCCNT - start; // Measure the tiny "Setup" time only
}

/* --- EXTERNAL INTERRUPT: BLUE BUTTON (PC13) --- */
void EXTI15_10_IRQHandler(void) {
    if (EXTI->PR & EXTI_PR_PR13) {
        static uint32_t lastPress = 0;
        // Debounce: 200ms lock-out
        if ((msTicks - lastPress) > 200) {
            buttonFlag = 1;         // Signal main loop to act
            useDMA = !useDMA;       // Flip the mode
            GPIOA->ODR ^= (1UL << 5); // Toggle LED LD2
            lastPress = msTicks;
        }
        EXTI->PR = EXTI_PR_PR13; // Clear the interrupt flag
    }
}

/* --- PERIPHERAL INITIALIZATION --- */
void Hardware_Init(void) {
    // 1. Clocks
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // 2. LED PA5
    GPIOA->MODER &= ~(3UL << (2 * 5)); GPIOA->MODER |= (1UL << (2 * 5));

    // 3. UART2 Setup (9600 Baud)
    GPIOA->MODER &= ~(3UL << (2 * 2)); GPIOA->MODER |= (2UL << (2 * 2));
    GPIOA->AFR[0] |= (7UL << (4 * 2)); // AF7 for Pin 2
    USART2->BRR = 0x0683; 
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;

    // 4. Button PC13 + EXTI Setup
    GPIOC->MODER &= ~(3UL << (2 * 13));
    SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI13_PC; 
    EXTI->IMR |= EXTI_IMR_IM13;
    EXTI->FTSR |= EXTI_FTSR_TR13;
    NVIC_EnableIRQ(EXTI15_10_IRQn);
    
    TIMING_Init(); // Start cycle counter
    DMA_Init();    // Prepare DMA1
}

/* --- REPORTING FUNCTION --- */
// Prints the performance data back to the user via manual polling.
void Report_Results(char* mode) {
    char res[80];
    sprintf(res, "[%s MODE] CPU cycles spent: %u\r\n", mode, (unsigned int)overhead);
    
    for(int i=0; res[i]; i++) {
        while(!(USART2->SR & USART_SR_TXE)); 
        USART2->DR = res[i];
    }
}

/* --- MAIN APPLICATION LOOP --- */
int main(void) {
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000); // 1ms timer
    Hardware_Init();

    while(1) {
        if (buttonFlag) {
            buttonFlag = 0; // Clear the button event
            
            if (useDMA) {
                // START DMA TEST
                sprintf(msg_buffer, "DMA-TRANSFER-IN-PROGRESS... ");
                DMA_UART2_Send(strlen(msg_buffer));
                
                /* --- SYNCHRONIZATION POINT --- 
                   Because DMA is non-blocking, the CPU would normally keep running 
                   and try to print the report immediately, causing garbled text.
                   We check the HISR (High Interrupt Status Register) bit 21 (TCIF6)
                   to wait specifically for the DMA to finish its work. */
                while (!(DMA1->HISR & DMA_HISR_TCIF6)); 
                
                Report_Results("DMA");
            } else {
                // START CPU TEST
                USART2->CR3 &= ~USART_CR3_DMAT; // Ensure UART isn't linked to DMA
                sprintf(msg_buffer, "CPU-POLLING-IN-PROGRESS... ");
                CPU_UART2_Send(msg_buffer);
                
                // No sync needed here because CPU polling naturally blocks execution.
                Report_Results("CPU");
            }
        }
    }
}