/**
 * @file   utilities.h
 * @author Tobias Kallevik
 * @author Thomas Markussen
*/

#ifndef EXAMPROJECT_UTILITES_H
#define EXAMPROJECT_UTILITES_H

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

using namespace std::chrono;

// Struct to keep alarm data
struct AlarmData {

    // Alarm trackers
    int alarmState = 0;
    int ringingAlarmSeconds = 0;
    int alarmMin = 0;
    int alarmHour = 0;

    bool turnOffAlarm = false;
    bool alarmRinging = false;
    bool alarmSnoozed = false;
    bool alarmHasBeenSet = false;
    bool switchAlarmInputs = false;

    time_t alarmTimeSec = 0;
    time_t alarmSnoozForSec = 0;

    Timer alarmRingingTimer;
};

// Struct to keep system time data
struct SystemTimeData {
    time_t currentEpochTime = 0;
    time_t clockInSec = 0;   

    Mutex mutex;
};


// Utility functiuon
void systemTimeThreadFunc(void *arg);
void alarmCheck(AlarmData *alarmData, SystemTimeData *systemTimeData, PwmOut *buzzer);
void weatherApiCheck(SharedData *sharedData, DFRobot_RGBLCD *lcd);
void timeApiCheck(SharedData *sharedData, DFRobot_RGBLCD *lcd);
void scrollFeed(bool *menuSwitched, string rssFeed, AlarmData *alarmData, SystemTimeData *systemTimeData, PwmOut *buzzer, DFRobot_RGBLCD *lcd);
void connectToNetwork(SharedData *sharedData);

#endif // EXAMPROJECT_UTILITES_H