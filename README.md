# STM32 DMA vs. CPU Polling Performance Analysis

This project provides a practical comparison between traditional CPU-based UART polling and DMA (Direct Memory Access) transfers on STM32 microcontrollers. By utilizing the DWT (Data Watchpoint and Trace) cycle counter, this code quantifies the exact CPU overhead saved when using DMA to handle peripheral communication.

## Project Overview
On low-power embedded systems, blocking I/O functions trap the CPU, preventing it from performing other tasks. This project showcases how to:
* Implement standard **CPU Polling** for UART transmission.
* Configure **DMA1** to offload UART data transmission.
* Use the **DWT cycle counter** to measure performance in real-time.
* Synchronize peripheral tasks using status registers.

## Hardware Requirements
* **Nucleo Board**: Compatible with F411RE or G431RB series.
* **Toolchain**: Keil uVision 5 (Project workflow provided in the `docs` folder).

## Key Features
* **Performance Metrics**: Real-time cycle counting to visualize "CPU trapped" time vs. "DMA offloaded" time.
* **Modular Design**: Toggle between modes using hardware interrupts (button debounce logic included).
* **System Timing**: SysTick integration for precise timebase management.

## Project Structure
* `/src`: Contains the primary `main.c` implementation.
* `/docs`: Includes the `STM32_Project_Flow.pptx` for setting up your Keil project, including specific pack versioning requirements for seamless builds.

## Setup Instructions
1.  **Environment**: Ensure you are using the Keil "Classic" pack versions as detailed in `STM32_Project_Flow.pptx`.
2.  **Configuration**: The project is pre-configured for STM32F411RE. Adjust the target device in your project settings if using the G431RB.
3.  **Build**: Open the project in Keil uVision 5, compile, and load onto your Nucleo board.

---
*Created for educational purposes to demonstrate ARM Cortex-M architecture efficiency.*
