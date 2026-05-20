# STM32-DMA-vs-Polling-Performance
A comparative performance analysis project for STM32 microcontrollers. This repository demonstrates the efficiency gap between CPU-bound UART polling and DMA-driven data transfers. Includes raw cycle-counting measurements using the DWT unit to quantify the CPU overhead saved by offloading I/O operations on Nucleo hardware (F4/G4 series).
