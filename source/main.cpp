/**
 * @file   main.cpp
 * @author Tobias Kallevik
 * @author Alexander Ruud
 * @author Thomas Markussen
*/

#include "mbed.h"
#include "time.h"
#include "rtos.h"
#include "DFRobot_RGBLCD.h"
#include "HTS221Sensor.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include "ISM43362Interface.h"
#include <cstring>
#include <utility>
#include <string>
#include <iostream>
#include <vector>
#include "apiThreads.h"
#include "screens.h"
#include "utilities.h"

// Gets pointer to default network instance
NetworkInterface *network = NetworkInterface::get_default_instance(); 

// GPIO *Note that generic names are used since the interrupts serve different purposes depending on where in the program they are used
InterruptIn interrupt1(D11); // D11
InterruptIn interrupt2(D10); // D10
InterruptIn interrupt3(D9);
InterruptIn interrupt4(D7);
InterruptIn interrupt5(D4);

PwmOut buzzer(D12);
AnalogIn pot(A0);

// Creates instances for connected devices
DFRobot_RGBLCD lcd(16, 2, D14, D15);
DevI2C *i2c = new DevI2C(PB_11, PB_10);
HTS221Sensor hts221(i2c, HTS221_I2C_ADDRESS, PA_0);

// Creates instances of the different structs
SharedData sharedData;
AlarmData alarmData;
SystemTimeData systemTimeData;
ChangeLocationData changeLocationData;

// Threads
Thread timeThread;
Thread weatherThread;
Thread rssThread;
Thread systemTimeThread;

// Interrupt Flags and interrupt vaiables
bool menuSwitched = false; 
int menuState = 0;

// Interrut functions. If test is used to protect against the bad button hardware that can cause a dubble trigger of the interrupt and to do different tings based on where in the program function is called
// Changes the main menu screens
void interrupt1Func() {
    wait_us(50000);

    if (menuState < 4) {
        menuState++;
        menuSwitched = true;
    } 
}

// Changes the alarmState used to determine what alarm screen to show. Only changes when in main menu. Also dubbles as btn to press for entering change location menu when on the weather screen
void interrupt2Func() {
    wait_us(50000);
 
    if (alarmData.alarmState < 4 && menuState == 0) {
        alarmData.alarmState++;
    } else if (menuState == 2) {
        changeLocationData.changeLocation = !changeLocationData.changeLocation;
    }
}

// Turns snooze on and confirms location change
void interrupt3Func() {
    wait_us(50000);
 
    if (alarmData.alarmRinging == true && menuState == 0) {
        alarmData.alarmSnoozed = true;
    } else if (changeLocationData.changeLocation == true && menuState == 2) {
    changeLocationData.confirmChange = true;
    }
}

// Turns on and off the alarm functionality
void interrupt4Func() {
    wait_us(50000);

    if ((alarmData.alarmState == 2 || alarmData.alarmState == 3) && menuState == 0) {
    alarmData.alarmState = 0;
    } else if (alarmData.alarmState == 0 && alarmData.alarmHasBeenSet == true && menuState == 0) {
        alarmData.alarmState = 2;
    } else if (changeLocationData.changeLocation == true && menuState == 2) {
        changeLocationData.removeLetter = true;
    }
}

// Switches the min/hour input of the pot meater used to set alarm time and confirms letters in change location menu
void interrupt5Func() {
 wait_us(50000);

    alarmData.switchAlarmInputs = !alarmData.switchAlarmInputs;

    if (changeLocationData.changeLocation == true) {
        changeLocationData.confirmLetter = true;
    }
}

// Main function
int main()
{
    // Interrupts
    interrupt1.rise(&interrupt1Func);
    interrupt2.rise(&interrupt2Func);
    interrupt3.rise(&interrupt3Func);
    interrupt4.rise(&interrupt4Func);
    interrupt5.rise(&interrupt5Func);

    // Sets up LCD, sensor and timer
    lcd.init();
    lcd.clear();
    hts221.init(NULL);
    hts221.enable();

    string rssFeed;

    // Starts and runs different bootup function and threads. The threads needs to be started in a specific oreder to ensure that data is available when needed
    // Connect to newtork
    lcd.printf("STARTING DEVICE"); 
    connectToNetwork(&sharedData); 
    // Starts the time api thread, singals the thread to unblock and waits until the time API thread unblocks the main thread. This is done since other threads rely on the data in this gotten from this thread
    timeThread.start(callback(timeThreadFunc, (void *)&sharedData)); 
    sharedData.timeThreadFlag.set(timeFlagBtn);
    sharedData.mainThreadFlag.wait_all(mainFlagBtn);
    // Starts the thread for setting system time
    systemTimeThread.start(callback(systemTimeThreadFunc, (void *)&systemTimeData));
    // Starts and unblocks the other threads
    weatherThread.start(callback(weatherThreadFunc, (void *)&sharedData)); 
    sharedData.weatherThreadFlag.set(weatherFlagBtn); 
    rssThread.start(callback(rssThreadFunc, (void *)&sharedData)); 
    sharedData.rssThreadFlag.set(rssFlagBtn); 
    
    // Calls the bootup function to display the bootup screens
    bootUp(&sharedData, &lcd);
    lcd.clear();


    while(true) {

        switch (menuState) {
            // Displays the main menu screen
            case 0:
                // Clears display and unblocks the time api thread when changing screens to ensure that the data is up to date
                if (menuSwitched == true) {
                    lcd.clear();
                    menuSwitched = false;
                    sharedData.timeThreadFlag.set(timeFlagBtn); 
                }

                // Depending on user action this will either display main menu or show alarm menu
                if (alarmData.alarmState == 1) {
                    // Shows the alarmMenu used to set an alarm
                    alarmMenu(&alarmData, &lcd, &pot);
                }  else {
                    // Uses the timeApiCheck function to regularly update the time data and the mainMenu function to displays the main menu screen
                    timeApiCheck(&sharedData, &lcd);
                    mainMenu(&alarmData, &systemTimeData, &lcd);
                }
                
                break;

            // Displays the sensor menu screen (indoor temp / humidity)
            case 1:
                // Clears display when changing screens
                if (menuSwitched == true) {
                    lcd.clear();
                    menuSwitched = false;
                }
                
                // Runs the sensor menu function to get and display sensor data
                sensorMenu(&lcd, &hts221);
                
                break;

            // Displays the weather menu screen
            case 2: 
                
                // Clears display and unblocks the weather api thread when changing screens to ensure that the data is up to date
                if (menuSwitched == true) {
                    lcd.clear();
                    menuSwitched = false;
                    sharedData.weatherThreadFlag.set(weatherFlagBtn); 
                }

                // Checks if the user wants to go to the change locations menu
                if (changeLocationData.changeLocation == true) {
                    lcd.clear();
                    changeLocationMenu(&sharedData, &lcd, &pot, &weatherThread, &changeLocationData);
                }

                // Uses the weatherApiCheck function to regularly update the time data and the weatherMenu function to displays the weather menu screen
                weatherMenu(&sharedData, &lcd);
                weatherApiCheck(&sharedData, &lcd);
                
                break;

            // Displays the RSS menu screen
            case 3: 
                // Clears display and unblocks the rss thread when changing screens to ensure that the data is up to date
                if (menuSwitched == true) {
                    lcd.clear();
                    sharedData.rssThreadFlag.set(rssFlagBtn); 
                    menuSwitched = false;
                } 

                // Gets the rss feed string from the rssMenu function. This function also prints the title of the rss feed
                rssFeed = rssMenu(&sharedData, &lcd, &menuSwitched);
                // Uses the scrollFeed function to scroll the rss feed on line 2 
                scrollFeed(&menuSwitched, rssFeed, &alarmData, &systemTimeData, &buzzer, &lcd);

                if (menuSwitched == false) {
                    // Unblocks the rss thread to recive the updated rss feed when it has finished scrolling passed once. We use an if statment to avoid fetching the RSS feed again when the user exits the RSS menu by changing screens
                    sharedData.rssThreadFlag.set(rssFlagBtn); 
                }

                break;

            // Completes the menu loop by setting the menu back to default screen
            case 4:
                
                menuState = 0;
                break;


            default:
                break;

        }

        // Runs the alarm check. This is done regardless of the screen the user is currently viewing to ensure that the alarm always goes off
        // Acitions on the alarm like snoozing, turning off and muting can only be done while in the main menu. This is done so that the buttons used for these actions can be used for other things in other menus
        // This funciton is also called from inside loops other places in the code where the loop stops this function from beeing run for a longer period of time
        alarmCheck(&alarmData, &systemTimeData, &buzzer);

    }
}

