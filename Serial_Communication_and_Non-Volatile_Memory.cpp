#include "LCD_DISCO_F429ZI.h"
#include "DebouncedInterrupt.h"
#include "mbed.h"
#include <cstdint>
#include <time.h>

// I2C Configuration
#define SDA_PIN PC_9
#define SCL_PIN PA_8
#define EEPROM_ADDR 0xA0

// EEPROM Memory Addresses for storing times
#define EEPROM_ADDR_1 0
#define EEPROM_ADDR_2 20 // ADDED SECOND

// Initialize LCD and I2C communication
LCD_DISCO_F429ZI LCD;
I2C i2c(SDA_PIN, SCL_PIN);

// BUTTONS: Interrupt-based input handling
InterruptIn userButton(BUTTON1);
DebouncedInterrupt displayButton(PA_6);
DebouncedInterrupt cycleButton(PC_2);
DebouncedInterrupt incrementButton(PC_3);

// VARIABLES
int selectedField = 0; // Tracks which part of the time is being modified (hours/mins/sec)
char prevTime1[20], prevTime2[20]; // Stores previous saved time values
char curTime[20];  // Stores current retrieved time
bool timeIsDirty = false; // Flag to track if time needs to be updated
time_t rawTime;  // Stores current system time
time_t selectedTime;  // Stores time being modified by user

// ENUMERATIONS: Define system states
enum SystemState {
    DISPLAY_TIME,  // Shows current time
    SAVE_TIME,     // Saves time to EEPROM
    PREV_TIMES,    // Displays previous saved times
    SET_TIME       // Allows user to modify time
};
SystemState state = DISPLAY_TIME;

// EEPROM Class for Read/Write Operations
class EEPROM {
public:
    // Write data to EEPROM at specified address
    static void Write(int address, unsigned int eeaddress, const char* data, int size) {
        char buffer[size + 2];
        buffer[0] = (unsigned char)(eeaddress >> 8);
        buffer[1] = (unsigned char)(eeaddress & 0xFF);
        memcpy(&buffer[2], data, size);

        i2c.write(address, buffer, size + 2, false);
        thread_sleep_for(6);
    }

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

// FUNCTION PROTOTYPES
void GetTime();
void DisplayTimes();
void ValueCycle();
void ValueIncrement();
void SetTime();
void ShowTime();
void ShowPreviousTimes();
void SaveTime();

// FUNCTION DEFINITIONS

void GetTime() {
    if (state == SET_TIME) timeIsDirty = true;
    state = SAVE_TIME;
}

void DisplayTimes() {
    if (state == SET_TIME) timeIsDirty = true;
    state = (state != PREV_TIMES) ? PREV_TIMES : DISPLAY_TIME;
}

void ValueCycle() {
    if (state != SET_TIME) {
        state = SET_TIME;
        selectedTime = rawTime;
        selectedField = 0;
    } else {
        selectedField = (selectedField + 1) % 3;
    }
}

void ValueIncrement() {
    if (state != SET_TIME) {
        state = SET_TIME;
        selectedTime = rawTime;
        selectedField = 0;
    } else {
        struct tm* timeInfo = localtime(&selectedTime);
        if (selectedField == 0) timeInfo->tm_hour = (timeInfo->tm_hour + 1) % 24;
        else if (selectedField == 1) timeInfo->tm_min = (timeInfo->tm_min + 1) % 60;
        else timeInfo->tm_sec = (timeInfo->tm_sec + 1) % 60;

        selectedTime = mktime(timeInfo);
    }
}

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

void ShowPreviousTimes() {
    EEPROM::Read(EEPROM_ADDR, EEPROM_ADDR_1, prevTime1, 20);
    EEPROM::Read(EEPROM_ADDR, EEPROM_ADDR_2, prevTime2, 20);

    LCD.Clear(LCD_COLOR_WHITE);
    LCD.DisplayStringAt(0, 60, (uint8_t*)"Previous Times:", LEFT_MODE);
    LCD.DisplayStringAt(0, 80, (uint8_t*)"(HH:MM:SS)", LEFT_MODE);
    LCD.DisplayStringAt(0, 120, (uint8_t*)prevTime1, LEFT_MODE);
    LCD.DisplayStringAt(0, 140, (uint8_t*)prevTime2, LEFT_MODE);
}

void SaveTime() {
    // Read last saved times
    EEPROM::Read(EEPROM_ADDR, EEPROM_ADDR_1, prevTime1, 20);

    // Get the current time
    time(&rawTime);
    struct tm* timeinfo = localtime(&rawTime);
    char timebuff[20];
    sprintf(timebuff, "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    // Shift previous time down (newest -> EEPROM_ADDR_1, old EEPROM_ADDR_1 -> EEPROM_ADDR_2)
    EEPROM::Write(EEPROM_ADDR, EEPROM_ADDR_2, prevTime1, 20);
    EEPROM::Write(EEPROM_ADDR, EEPROM_ADDR_1, timebuff, 20);
    
    printf("Saved time to EEPROM: %s\n", timebuff);

    state = DISPLAY_TIME;
}

void SetTime() {
    struct tm* timeInfo = localtime(&selectedTime);
    char timebuff[20];

    if (selectedField == 0) sprintf(timebuff, "|%02d|:%02d:%02d", timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
    else if (selectedField == 1) sprintf(timebuff, "%02d:|%02d|:%02d", timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
    else sprintf(timebuff, "%02d:%02d:|%02d|", timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);

    LCD.Clear(LCD_COLOR_WHITE);
    LCD.DisplayStringAt(0, 60, (uint8_t*)"Set Time", CENTER_MODE);
    LCD.DisplayStringAt(0, 100, (uint8_t*)timebuff, CENTER_MODE);
    LCD.DisplayStringAt(0, 140, (uint8_t*)"(HH:MM:SS)", CENTER_MODE);
}

int main() {
    userButton.fall(&GetTime);
    displayButton.attach(&DisplayTimes, IRQ_FALL, 100, false);
    cycleButton.attach(&ValueCycle, IRQ_FALL, 100, false);
    incrementButton.attach(&ValueIncrement, IRQ_FALL, 100, false);

    __enable_irq();

    // Set RTC Date & Time (January 1, 2025, 00:00:00)
    tm t = {0};
    t.tm_year = 125;  // Years since 1900
    set_time(mktime(&t));

    LCD.SetFont(&Font20);
    LCD.SetTextColor(LCD_COLOR_BLACK);

    while (1) {
        if (timeIsDirty) {
            set_time(selectedTime);
            timeIsDirty = false;
        }

        switch (state) {
            case DISPLAY_TIME: ShowTime(); break;
            case SAVE_TIME: SaveTime(); break;
            case PREV_TIMES: ShowPreviousTimes(); break;
            case SET_TIME: SetTime(); break;
        }

        thread_sleep_for(100);
    }
}
