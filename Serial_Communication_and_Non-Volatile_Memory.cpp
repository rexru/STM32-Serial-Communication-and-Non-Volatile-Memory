#include "LCD_DISCO_F429ZI.h"
#include "DebouncedInterrupt.h"
#include "mbed.h"
#include <cstdint>
#include <time.h>

// -----------------------------
// I2C & EEPROM Configuration
// -----------------------------
#define SDA_PIN PC_9
#define SCL_PIN PA_8
#define EEPROM_ADDR 0xA0          // 7-bit device address shifted left by 1 (0x50 << 1)

// EEPROM Memory Addresses
#define EEPROM_ADDR_1 0           // Address for most recent time
#define EEPROM_ADDR_2 20          // Address for previous time

// -----------------------------
// Hardware Peripherals
// -----------------------------
LCD_DISCO_F429ZI LCD;             // LCD display object
I2C i2c(SDA_PIN, SCL_PIN);        // I2C interface

// Input buttons (interrupt-driven)
InterruptIn userButton(BUTTON1);  // Onboard user button
DebouncedInterrupt displayButton(PA_6);   // External button: Toggle time/log display
DebouncedInterrupt cycleButton(PC_2);     // External button: Select field (hours/mins/sec)
DebouncedInterrupt incrementButton(PC_3); // External button: Increment selected field

// -----------------------------
// Global Variables
// -----------------------------
int selectedField = 0;            // Tracks which time field is selected (0 = hours, 1 = mins, 2 = secs)
char prevTime1[20], prevTime2[20]; // Buffers for EEPROM-stored times
bool timeIsDirty = false;         // Flag to indicate RTC update required
time_t rawTime;                   // Current system time
time_t selectedTime;              // Used when adjusting RTC time

// -----------------------------
// Finite State Machine States
// -----------------------------
enum SystemState {
    DISPLAY_TIME,   // Default: show current time on LCD
    SAVE_TIME,      // Save timestamp to EEPROM
    PREV_TIMES,     // Show last two saved times
    SET_TIME        // User adjusting RTC via buttons
};
SystemState state = DISPLAY_TIME;

// -----------------------------
// EEPROM Helper Class
// -----------------------------
class EEPROM {
public:
    // Write data to EEPROM at given address
    static void Write(int address, unsigned int eeaddress, const char* data, int size) {
        char buffer[size + 2];
        buffer[0] = (unsigned char)(eeaddress >> 8); // High byte
        buffer[1] = (unsigned char)(eeaddress & 0xFF); // Low byte
        memcpy(&buffer[2], data, size);

        i2c.write(address, buffer, size + 2, false);
        thread_sleep_for(6); // Write cycle delay
    }

    // Read data from EEPROM at given address
    static void Read(int address, unsigned int eeaddress, char* data, int size) {
        char buffer[2] = {
            (unsigned char)(eeaddress >> 8),
            (unsigned char)(eeaddress & 0xFF)
        };

        i2c.write(address, buffer, 2, false);
        thread_sleep_for(6);
        i2c.read(address, data, size);
        thread_sleep_for(6);
    }
};

// -----------------------------
// Function Prototypes
// -----------------------------
void GetTime();           // ISR: Logs time on user button press
void DisplayTimes();      // ISR: Toggles between display/log view
void ValueCycle();        // ISR: Cycles through fields in SET_TIME mode
void ValueIncrement();    // ISR: Increments selected field in SET_TIME mode
void ShowTime();          // Displays current RTC time
void ShowPreviousTimes(); // Displays last two saved times
void SaveTime();          // Saves current RTC time into EEPROM
void SetTime();           // Displays editable RTC time

// -----------------------------
// Interrupt Service Routines
// -----------------------------

// Onboard button pressed → save current RTC time
void GetTime() {
    if (state == SET_TIME) timeIsDirty = true; // Commit user edits if needed
    state = SAVE_TIME;
}

// External button pressed → toggle between Idle (current time) and Log display
void DisplayTimes() {
    if (state == SET_TIME) timeIsDirty = true;
    state = (state != PREV_TIMES) ? PREV_TIMES : DISPLAY_TIME;
}

// External button pressed → cycle through hour/min/sec fields
void ValueCycle() {
    if (state != SET_TIME) {
        state = SET_TIME;
        selectedTime = rawTime;   // Start editing from current RTC time
        selectedField = 0;
    } else {
        selectedField = (selectedField + 1) % 3; // Rotate through fields
    }
}

// External button pressed → increment currently selected field
void ValueIncrement() {
    if (state != SET_TIME) {
        state = SET_TIME;
        selectedTime = rawTime;
        selectedField = 0;
    } else {
        struct tm* timeInfo = localtime(&selectedTime);
        if (selectedField == 0)      timeInfo->tm_hour = (timeInfo->tm_hour + 1) % 24;
        else if (selectedField == 1) timeInfo->tm_min = (timeInfo->tm_min + 1) % 60;
        else                         timeInfo->tm_sec = (timeInfo->tm_sec + 1) % 60;

        selectedTime = mktime(timeInfo);
    }
}

// -----------------------------
// Display Functions
// -----------------------------

// Show live current RTC time
void ShowTime() {
    LCD.Clear(LCD_COLOR_WHITE);
    LCD.DisplayStringAt(0, 60, (uint8_t*)"Current Time", CENTER_MODE);

    time(&rawTime);
    struct tm* timeinfo = localtime(&rawTime);
    char timebuff[20];
    sprintf(timebuff, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    LCD.DisplayStringAt(0, 100, (uint8_t*)timebuff, CENTER_MODE);
    LCD.DisplayStringAt(0, 140, (uint8_t*)"(HH:MM:SS)", CENTER_MODE);
}

// Show last two logged button press times
void ShowPreviousTimes() {
    EEPROM::Read(EEPROM_ADDR, EEPROM_ADDR_1, prevTime1, 20);
    EEPROM::Read(EEPROM_ADDR, EEPROM_ADDR_2, prevTime2, 20);

    LCD.Clear(LCD_COLOR_WHITE);
    LCD.DisplayStringAt(0, 60, (uint8_t*)"Previous Times:", LEFT_MODE);
    LCD.DisplayStringAt(0, 80, (uint8_t*)"(HH:MM:SS)", LEFT_MODE);
    LCD.DisplayStringAt(0, 120, (uint8_t*)prevTime1, LEFT_MODE);
    LCD.DisplayStringAt(0, 140, (uint8_t*)prevTime2, LEFT_MODE);
}

// -----------------------------
// EEPROM Storage Functions
// -----------------------------

// Save current RTC time into EEPROM (shift old log down)
void SaveTime() {
    // Get most recent saved time
    EEPROM::Read(EEPROM_ADDR, EEPROM_ADDR_1, prevTime1, 20);

    // Fetch current RTC time
    time(&rawTime);
    struct tm* timeinfo = localtime(&rawTime);
    char timebuff[20];
    sprintf(timebuff, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    // Shift logs: [latest → addr1], [addr1 → addr2]
    EEPROM::Write(EEPROM_ADDR, EEPROM_ADDR_2, prevTime1, 20);
    EEPROM::Write(EEPROM_ADDR, EEPROM_ADDR_1, timebuff, 20);

    printf("Saved time to EEPROM: %s\n", timebuff);

    state = DISPLAY_TIME; // Return to idle
}

// -----------------------------
// RTC Time-Set Mode
// -----------------------------

// Display editable RTC time (with field highlighting)
void SetTime() {
    struct tm* timeInfo = localtime(&selectedTime);
    char timebuff[20];

    if (selectedField == 0) sprintf(timebuff, "|%02d|:%02d:%02d", timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
    else if (selectedField == 1) sprintf(timebuff, "%02d:|%02d|:%02d", timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
    else                         sprintf(timebuff, "%02d:%02d:|%02d|", timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);

    LCD.Clear(LCD_COLOR_WHITE);
    LCD.DisplayStringAt(0, 60, (uint8_t*)"Set Time", CENTER_MODE);
    LCD.DisplayStringAt(0, 100, (uint8_t*)timebuff, CENTER_MODE);
    LCD.DisplayStringAt(0, 140, (uint8_t*)"(HH:MM:SS)", CENTER_MODE);
}

// -----------------------------
// Main Program
// -----------------------------
int main() {
    // Attach interrupts
    userButton.fall(&GetTime);
    displayButton.attach(&DisplayTimes, IRQ_FALL, 100, false);
    cycleButton.attach(&ValueCycle, IRQ_FALL, 100, false);
    incrementButton.attach(&ValueIncrement, IRQ_FALL, 100, false);

    __enable_irq();

    // Initialize RTC to Jan 1, 2025, 00:00:00
    tm t = {0};
    t.tm_year = 125; // Years since 1900 → 2025
    set_time(mktime(&t));

    // LCD configuration
    LCD.SetFont(&Font20);
    LCD.SetTextColor(LCD_COLOR_BLACK);

    // FSM Loop
    while (1) {
        // If user updated RTC, apply changes
        if (timeIsDirty) {
            set_time(selectedTime);
            timeIsDirty = false;
        }

        // Execute state-specific behavior
        switch (state) {
            case DISPLAY_TIME: ShowTime(); break;
            case SAVE_TIME:    SaveTime(); break;
            case PREV_TIMES:   ShowPreviousTimes(); break;
            case SET_TIME:     SetTime(); break;
        }

        thread_sleep_for(100); // Refresh interval
    }
}
