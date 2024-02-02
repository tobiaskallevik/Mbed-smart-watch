/**
 * @file   apiThreads.h
 * @author Tobias Kallevik
 * @author Alexander Ruud
 * @author Thomas Markussen
*/

#ifndef SMARTWATCH_API_THREADS_H
#define SMARTWATCH_API_THREADS_H

// Includes
#include "mbed.h"
#include <string>
#include <vector>
#include <sstream>
#include "json.hpp"
#include "ISM43362Interface.h"

// Namespaces
using json = nlohmann::json;

// Defenitons for the event flags
#define mainFlagBtn (1 << 0) 
#define timeFlagBtn (1 << 1) 
#define weatherFlagBtn (1 << 2)
#define rssFlagBtn (1 << 3)


struct SharedData {
    // Shared variables from time API
    size_t epochTime = 0;
    int timezoneOffsetWithDst = 0;
    string latitude = 0;
    string longitude = 0;
    string city;

    // Shared variables from weather API
    bool firstTimeApiRun = true;
    string weatherCondition;
    size_t outdoorTemp = 0;

    // Shared variables from RSS 
    string rssFeedTitle;
    string newsTitle1;
    string newsTitle2;
    string newsTitle3;

    // Time stamps for thread runs
    time_t lastTimeApiRunTime = 0;
    time_t lastWeatherApiRunTime = 0;
    time_t lastRssRunTime = 0;

    // Shared network data
    NetworkInterface* network;
    nsapi_connection_status_t status;

    // Event flags
    EventFlags mainThreadFlag; 
    EventFlags timeThreadFlag; 
    EventFlags weatherThreadFlag; 
    EventFlags rssThreadFlag; 

    // Mutex used to protect the struct
    Mutex mutex;
};


// Thread function declarations
void timeThreadFunc(void* arg);
void weatherThreadFunc(void* arg);
void rssThreadFunc(void* arg);

#endif // SMARTWATCH_API_THREADS_H