## 🧑‍💻 Bare-Metal UART CLI with Real-Time Interrupt-Driven I/O

This project implements a high-performance, **zero-abstraction** Command Line Interface (CLI) for the STM32F446RE (ARM Cortex-M4). While my previous project (image_624f13.png) utilized the ST HAL library for rapid prototyping, this kernel was built entirely using **Direct Register Manipulation** to achieve deterministic interrupt latency and a minimal binary footprint.

---

## ✨ Key Technical Highlights (No-Library Approach)

* **Direct Register Orchestration**: Manual bit-mapping of **RCC, GPIO, USART, and Multi-Channel Timers** without any vendor-provided abstraction layers.
* **Deterministic ISR Architecture**: Custom interrupt handlers for background UART and timer management, bypassing standard HAL callbacks for single-cycle execution speed.
* **Atomic Hardware Access**: Utilizes the **BSRR (Bit Set/Reset Register)** for thread-safe, single-cycle I/O operations, effectively eliminating the race conditions common in Read-Modify-Write (ODR) operations.
* **Robust Input Validation**: Developed a custom parser using `strtol` for strict data validation, protecting hardware from malformed user input.

---

## 🧑‍💻 Supported Commands

| Command | Description | Implementation |
| :--- | :--- | :--- |
| `HELP` | Show interactive command list | UART Character Echo |
| `SET PWM <1..100>` | Adjust LED brightness via 16-bit PWM | TIM2 CCR Manipulation |
| `SET BLINK <ms>` | Configure asynchronous heartbeat period | TIM6 ARR Calculation |
| `SET BREATHE` | Toggle the non-linear pulse effect | PWM State Machine Logic |
| `STATUS` | Real-time dump of current system registers | Register-to-String Formatting |
| `IDLE` | Force-kill all active hardware outputs | Global Clock/Timer Disable |

---

## 🛠️ Peripheral Architecture (Register Level)


* **USART2**: Manual **BRR** calculation and interrupt-driven data handling via pure register access.
* **TIM2**: Configured for PA5 (LED pin using **AF1**) for brightness control.
* **System Heartbeat**: `TIM6` periodic interrupt mapping for deterministic system timing.
* **Zero-Library Dependencies**: 
    * Manual clock gating via `RCC->AHB1ENR` / `RCC->APB1ENR`.
    * Direct pin-muxing via `GPIO->MODER` and `GPIO->AFR`.
    * Direct NVIC interrupt enablement via `NVIC->ISER`.

---

## 🔬 Hardware Challenge: Silicon vs. Physical Package

A key differentiator in this project was the deep dive into the **STM32F446xx Datasheet** (Table 11) and Nucleo-64 schematics to navigate physical hardware constraints:
* **The PF4 Discovery**: Verified through Alternate Function tables (image_1ed891.png) that while Port F logic exists in silicon, pins like **PF4** are not physically bonded in the LQFP64 package.
* **Solder Bridge Constraints**: Identified "Open" solder bridges (e.g., **SB25**) in the board schematics that required strategic GPIO mapping to avoid physical signal gaps.

---

## 📦 Memory Footprint & Efficiency

| Metric | Previous CLI (HAL Implementation) | **This Register-Level Kernel** |
| :--- | :--- | :--- |
| **Flash Usage** | ~20–25 KB (Library Overhead) | **~8 KB (C-Stdlib included)** |
| **Logic Speed** | Higher overhead (HAL callbacks) | **Deterministic / Minimal Jitter** |
| **Abstraction** | Heavy ST HAL / LL Drivers | **Pure Register-to-Silicon Manipulation** |

**Link to HAL-Level CLI**
https://github.com/Shaurya-singh21/stm32-uart-cli-controller
