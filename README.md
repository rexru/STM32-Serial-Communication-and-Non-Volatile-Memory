# STM32 Serial Communication and Non-Volatile Memory

## Overview
This project explores **I2C communication** and the use of **non-volatile memory** with the STM32F429ZI Discovery board. An external **24FC64F EEPROM** chip is interfaced via I2C to log button press timestamps provided by the MCU’s **real-time clock (RTC)**. The project also integrates **pushbutton input handling**, **LCD display output**, and a **finite state machine (FSM)** to manage system behavior.  

---

## Features
- **RTC Integration**
  - Uses the STM32F429’s built-in real-time clock (HH:MM:SS).  
  - Supports user-controlled time and date setting with external pushbuttons.  

- **EEPROM Logging (24FC64F over I2C)**
  - Logs the last two timestamps when the onboard user button is pressed.  
  - Data persists even after power cycles (non-volatile).  

- **LCD Display**
  - Idle mode: continuously shows the current RTC time.  
  - Log mode: displays the last two stored button press times.  
  - All values labeled clearly for usability.  

- **External Buttons**
  - **Button 1 (toggle display mode):** switch between current time and log display.  
  - **Button 2 (unit select):** choose hours, minutes, or seconds to adjust.  
  - **Button 3 (increment):** increase the selected time unit.  

- **FSM-Based Control**
  - State-driven design for clarity and robustness.  
  - Modes: Idle → Log Display → Time-Set.  

- **Interrupt-Driven Timing**
  - RTC and timers use hardware interrupts.  
  - No busy-wait loops.  

---

## Hardware
- **STM32F429ZI Discovery Board**  
  - Onboard LCD (2.41", 240x320 QVGA).  
  - Onboard user pushbutton (blue).  
  - Onboard RTC.  
  - Onboard LEDs.  

- **External Components**  
  - 24FC64F EEPROM chip (I2C protocol).  
  - 4.7 kΩ resistors ×2 (I2C SDA & SCL pull-ups).  
  - 100 Ω resistors ×2 (for power stability).  
  - 10 µF capacitor (decoupling).  
  - External pushbuttons ×3 (mode toggle, select, increment).  
  - Breadboard + jumper wires.  

---

## Circuit
- Connect EEPROM **SDA** and **SCL** lines to MCU pins with 4.7 kΩ pull-up resistors.  
- Vcc powered by **3.3V output** from STM32 Discovery board.  
- GND common between MCU and EEPROM.  
- External pushbuttons connected to GPIO pins with **internal pull-ups enabled**.  
- (Refer to lab schematic for exact pin mapping.)  

---

## FSM (System Behavior)
1. **Idle State**  
   - LCD shows current RTC time.  
   - Waits for inputs.  

2. **Button Press Logging**  
   - Onboard button pressed → read current RTC time.  
   - Store timestamp in EEPROM (keep last 2 entries).  

3. **Log Display Mode**  
   - External button pressed → show last two logged times on LCD.  
   - Press again → return to Idle.  

4. **Time-Set Mode**  
   - Entered using dedicated external buttons.  
   - One button selects unit (hours, minutes, seconds).  
   - Another increments value.  
   - Exits automatically after inactivity timeout.  

---

## Requirements
- **Software:**  
  - Keil Studio Cloud with Mbed OS.  
  - BSP and LCD libraries imported.  

- **Hardware:**  
  - STM32F429ZI Discovery board.  
  - External EEPROM circuit (as above).  
  - Pushbuttons with pull-ups.  

---

## How to Run
1. Import this project into **Keil Studio Cloud**.  
2. Set target to `DISCO-F429ZI`.  
3. Flash program to board.  
4. Build the circuit on a breadboard as per schematic.  
5. Interact with the system:  
   - Onboard button → log timestamp.  
   - External button (1) → toggle log display.  
   - External button (2 + 3) → set time/date.  

---

## Example LCD Outputs
**Idle Mode:**  
