/**
 * @file   screens.h
 * @author Tobias Kallevik
 * @author Alexander Ruud
*/

#ifndef EXAMPROJECT_SCREENS_H
#define EXAMPROJECT_SCREENS_H

#pragma once 

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
#include "utilities.h"

struct ChangeLocationData {
    // Wether manu variables
    bool changeLocation = false;
    bool confirmLetter = false;
    bool removeLetter = false;
    bool confirmChange = false;
};

// Menu/screen functions
void bootUp(SharedData *sharedData, DFRobot_RGBLCD *lcd);
void mainMenu(AlarmData *alarmData, SystemTimeData *systemTimeData, DFRobot_RGBLCD *lcd);
void alarmMenu(AlarmData *alarmData, DFRobot_RGBLCD *lcd, AnalogIn *pot);
void sensorMenu(DFRobot_RGBLCD *lcd, HTS221Sensor *hts221);
void weatherMenu(SharedData *sharedData, DFRobot_RGBLCD *lcd);
void changeLocationMenu(SharedData *sharedData, DFRobot_RGBLCD *lcd, AnalogIn *pot, Thread *weatherThread, ChangeLocationData *changeLocationData);
string rssMenu(SharedData *sharedData, DFRobot_RGBLCD *lcd, bool *menuSwitched);



#endif // EXAMPROJECT_SCREENS_H