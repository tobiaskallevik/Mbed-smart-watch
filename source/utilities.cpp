/**
 * @file   utilities.cpp
 * @author Tobias Kallevik
 * @author Thomas Markussen
*/

#include "screens.h"
#include "utilities.h"
#include <cstdio>

// Function used to connect the device to the network
void connectToNetwork(SharedData *sharedData) {

    nsapi_size_or_error_t result;
    // Locks the mutex to protect the shared data
    sharedData->mutex.lock();
    // Gets the default network interface
    sharedData->network = NetworkInterface::get_default_instance();
    // Check if inteface was obtaind 
    if (!sharedData->network) {
        printf("Failed to get default network interface\n");
    }

    // Connect to the network
    do {
        printf("\nConnecting to the network");
        result = sharedData->network->connect();

        if (result != NSAPI_ERROR_OK) {
            printf("\nFailed to connect to network: %d", result);
        }
        
    } while (result != NSAPI_ERROR_OK);

    // Unlcoks the mutex
    sharedData->mutex.unlock();

    // Check if connection was successfull
    if (result != 0) {
        printf("\nFailed to connect to network: %d", result);

    } else {
        printf("\nConnected to network!");
    }
}

// Function used by the systemTimeThread to get system time as up to data variables
void systemTimeThreadFunc(void *arg) {

    SystemTimeData* systemTimeData = static_cast<SystemTimeData*>(arg);

    while (true) {
        // Locks the mutex to protect the shared data
        systemTimeData->mutex.lock();

        // Get the current time as epoch time, convert the time to lt annd finds the amount of second that has passed this day
        systemTimeData->currentEpochTime = time(NULL);
        tm *ltm = localtime(&systemTimeData->currentEpochTime);
        systemTimeData->clockInSec = (ltm->tm_hour * 3600) + (ltm-> tm_min*60) + ltm->tm_sec;

        // Unlocks the mutex and put the thread to sleep for 500ms
        systemTimeData->mutex.unlock();
        thread_sleep_for(500);
    }
}

// Function used to check the state of the alarm. This function is called one time in every itteration of the main loop
// To ensure that the alarm goes off at the set time, it is also called from places in the program that keep the main loop from itterating for a longer period of time (like the RSS feed scroll)
void alarmCheck(AlarmData *alarmData, SystemTimeData *systemTimeData, PwmOut *buzzer){

    // Locks the mutex to retrive the shared data and set a local variable for time of day in sec
    systemTimeData->mutex.lock();
    time_t clockInSec = systemTimeData->clockInSec;
    systemTimeData->mutex.unlock();

    // Starts ringing the alarm when the seconds matches current time of day + time snoozed (0 if not snoozed)
    // A buffer of 1 second is used to be sure that alarm time isn't missed
    // The alarmState also needs to be 2 for the alarm to ring, meaning that the alarm is active
    if (alarmData->alarmState == 2 && alarmData->ringingAlarmSeconds < 600
        && clockInSec >= alarmData->alarmTimeSec + alarmData->alarmSnoozForSec 
        && clockInSec <= alarmData->alarmTimeSec + alarmData->alarmSnoozForSec + 1) { 
        // Activates the alarm
        *buzzer = 0.8;
        alarmData->alarmRinging = true;
        // Start the alarm timer to track how long the alarm is ringing
        alarmData->alarmRingingTimer.start();
        printf("\nRinging");
    } 
    // Mutes the alarm if it has rung for 10 min
    else if (alarmData->ringingAlarmSeconds >= 600) {  
        // Stops the alarm
        *buzzer = 0;
        alarmData->alarmRinging = false;
        // Stops and resets the alarm timer
        alarmData->alarmRingingTimer.stop();
        alarmData->alarmRingingTimer.reset();
        // Resets the alarm state and snooz time tracker
        alarmData->alarmSnoozForSec = 0;
        alarmData->alarmState = 2;
        printf("\nAlarm Stopped");
    }

    // Turns the alarm off and adds snooze time if the snooz button is pressed
    if (alarmData->alarmSnoozed == true) {
        // Turns off the alarm and de activates the snooze button
        *buzzer = 0;
        alarmData->alarmSnoozed = false;
        alarmData->alarmRinging = false;
        // Adds 5 min / 300 sec to the snooz time and stops/resets the alarm timer
        alarmData->alarmSnoozForSec += 300; 
        alarmData->alarmRingingTimer.stop();
        alarmData->alarmRingingTimer.reset();
    } 

    // Stops the alarm from ringing if it is disabled
    if (alarmData->alarmState == 0) {
        *buzzer = 0;
        alarmData->alarmRinging = false;
        alarmData->alarmSnoozForSec = 0;
        alarmData->alarmRingingTimer.stop();
        alarmData->alarmRingingTimer.reset();
    }

    // If the alarm is muted when the alarm was suppose to go off, the alarm is re acivated so that it goes of at the set time the next day
    // A buffer of +2sec to +3sec is used to ensure that the reactivation of the alarm doesn't trigger the alarm immediately
    if (alarmData->alarmState == 3 
        && clockInSec >= alarmData->alarmTimeSec + alarmData->alarmSnoozForSec + 2
        && clockInSec <= alarmData->alarmTimeSec + alarmData->alarmSnoozForSec + 3) {

        *buzzer = 0;
        alarmData->alarmRinging = false;
        alarmData->alarmSnoozForSec = 0;
        alarmData->alarmRingingTimer.stop();
        alarmData->alarmRingingTimer.reset();
        // Changes alarm state to 2 aka active alarm
        alarmData->alarmState = 2;

    } 
    // If the alarm is muted while the alarm is rining, we assume that the user only wants to mute this alarm. The alarm is than reactivated for the next day
    else if (alarmData->alarmState == 3 && alarmData->alarmRinging == true) { 

        // Turns off alarm
        *buzzer = 0;
        alarmData->alarmRinging = false;
        alarmData->alarmSnoozForSec = 0;
        alarmData->alarmRingingTimer.stop();
        alarmData->alarmRingingTimer.reset();
        // Changes alarm state to 2 aka active alarm
        alarmData->alarmState = 2;
    } 


    // Checks how long the alarm has been ringing
    alarmData->ringingAlarmSeconds = duration_cast<seconds>(alarmData->alarmRingingTimer.elapsed_time()).count();
}

// Checks if it has been longer than 15 min since last time time was fetched
void timeApiCheck(SharedData *sharedData, DFRobot_RGBLCD *lcd) {
    
    // Fetches the time data if it has been more than 15 min since last fetch. This is done by setting the thread flag and unblocking the weather API thread
    if (sharedData->lastTimeApiRunTime + 900 <= time(NULL)) { 
        sharedData->timeThreadFlag.set(timeFlagBtn); 
        lcd->clear();
    }

}

// Checks if it has been longer than 15 min since last time weather was fetched
void weatherApiCheck(SharedData *sharedData, DFRobot_RGBLCD *lcd) {
    
    // Fetches the weather data if it has been more than 15 min since last fetch. This is done by setting the thread flag and unblocking the weather API thread
    if (sharedData->lastWeatherApiRunTime + 900 <= time(NULL)) { 
        sharedData->weatherThreadFlag.set(weatherFlagBtn);
        lcd->clear(); 
    }

}

// Scrolls the feed from right to left for a given char
void scrollFeed(bool *menuSwitched, string rssFeed, AlarmData *alarmData, SystemTimeData *systemTimeData, PwmOut *buzzer, DFRobot_RGBLCD *lcd) {

    // Creates a new char containing the RSS feed
    char rssChar[rssFeed.size()];
    strcpy(rssChar, rssFeed.c_str());

    // Prints each letter in the rss feed while moving the previous letters to the left
    for (int i = 0; i < sizeof(rssChar)-16; i++) {
        
        // Breaks the loop if the user changes screens/menus
        if (*menuSwitched == true) {return;} 

        lcd->setCursor(0, 1);

        // Prints 16 characters of the rss feed at a time
        for (int j = i; j < i + 16; j++) {
            lcd->printf("%c", rssChar[j]);
        }

        // Runs the alarm chack to avoid missing an alarm while inside the for loop
        alarmCheck(alarmData, systemTimeData, buzzer);

        thread_sleep_for(250);
        lcd->printf("                ");
    } 
}