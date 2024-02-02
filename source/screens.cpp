/**
 * @file   screens.cpp
 * @author Tobias Kallevik
 * @author Alexander Ruud
*/

#include "screens.h"
#include "apiThreads.h"
#include <cstdio>

// Boot up menu
void bootUp(SharedData *sharedData, DFRobot_RGBLCD *lcd) {
    // Shows bootup screens at startup. Mutex isn't needed since we know this is the only thread to access these variables during this time

    // Shows epoch time
    int i = 0;
    while (i < 2) {
        lcd->printf("                ");
        lcd->setCursor(0, 0);
        lcd->printf("Unix epoch time:");
        lcd->setCursor(0, 1);
        lcd->printf("%i", (time(NULL) - (sharedData->timezoneOffsetWithDst * 3600)));
        i++;
        thread_sleep_for(1000);
    }

    lcd->clear();

    // Shows coordinates
    lcd->printf("                ");
    lcd->setCursor(0, 0);
    lcd->printf("Lat: %s", sharedData->latitude.c_str());
    lcd->setCursor(0, 1);
    lcd->printf("Lon: %s", sharedData->longitude.c_str());
    thread_sleep_for(2000);
    lcd->clear();

    // Shows city gotten from IP
    lcd->printf("                ");
    lcd->setCursor(0, 0);
    lcd->printf("City:");
    lcd->setCursor(0, 1);
    lcd->printf("%s", sharedData->city.c_str());
    thread_sleep_for(2000);
}

// Main clock menu
void mainMenu(AlarmData *alarmData, SystemTimeData *systemTimeData, DFRobot_RGBLCD *lcd) {

    char timeBuffer[64];
    char alarmTimeBuffer[64];
    time_t alarmTimeToDisplay = alarmData->alarmTimeSec + alarmData->alarmSnoozForSec;

    // Formats the time variables from epoch time
    systemTimeData->mutex.lock();
    strftime(timeBuffer, sizeof(timeBuffer), "%a %b %d %H:%M", localtime(&systemTimeData->currentEpochTime)); //Formats time from epoch time 
    systemTimeData->mutex.unlock();
    strftime(alarmTimeBuffer, sizeof(alarmTimeBuffer), "%H:%M", localtime(&alarmTimeToDisplay)); //Formats time from epoch time 

    lcd->printf("                ");
    lcd->setCursor(0, 0);
    lcd->printf("%s", timeBuffer);

    // Decides what to show of the alarm on the main menu. Hitting the alarm menu button when an alarm is active will toggle the alarm on and off
    switch (alarmData->alarmState) {
        case 0:
            // Prints nothing indicating alarm is not active
            lcd->setCursor(0, 1);
            lcd->printf("                ");
            break;

        case 1:
            
            // Case 1 is handled by the alarmMenu
            break;

        case 2: 
            // Prints the time of the upcoming alarm
            alarmData->alarmHasBeenSet = true;
            lcd->setCursor(0, 1);
            lcd->printf("Alarm: %s", alarmTimeBuffer);
            break;

        case 3: 
            // Prints the time of the upcoming alarm with an indication that the alarm is muted 
            lcd->setCursor(0, 1);
            lcd->printf("Alarm OFF: %s", alarmTimeBuffer);
            break;

        case 4: 
            // Reverts back to case 2. This is done so that the user can mute and unmute an active alarm using one button
            alarmData->alarmState = 2;
            break;

        default:
            break;
    }


}

// Menu for setting alarm
void alarmMenu(AlarmData *alarmData, DFRobot_RGBLCD *lcd, AnalogIn *pot){

    // Reads the potensiometer to change the minute or hour of an alarm
    if (alarmData->switchAlarmInputs == false) {
        alarmData->alarmHour = pot->read() * 23; 
    } else if (alarmData->switchAlarmInputs == true) {
        alarmData->alarmMin = pot->read() * 59; 
    }

    // Prints and updates the alarm time
    lcd->printf("                ");
    lcd->setCursor(0, 0);
    lcd->printf("Alarm For: %02d:%02d", alarmData->alarmHour, alarmData->alarmMin);
    alarmData->alarmTimeSec = (alarmData->alarmHour*3600) + (alarmData->alarmMin*60);
}

// menu for mesuring and showing sensor data
void sensorMenu(DFRobot_RGBLCD *lcd, HTS221Sensor *hts221) {
    // Float values for sansor data
    float temp = 0;
    float humidity = 0;

    // Gets data from sensors
    hts221->get_humidity(&humidity);
    hts221->get_temperature(&temp);

    // Prints sensor data to screen
    lcd->printf("                ");
    lcd->setCursor(0, 0);
    lcd->printf("Temp: %.1f C", temp);
    lcd->setCursor(0, 1);
    lcd->printf("Humidity: %.0f%%", humidity);
}

// Menu for showing the weather forcast
void weatherMenu(SharedData *sharedData, DFRobot_RGBLCD *lcd) {
    // Uses a mutex to safely retrive weather data
    sharedData->mutex.lock();
    string weatherCondition = sharedData->weatherCondition;
    int outdoorTemp = sharedData->outdoorTemp;
    sharedData->mutex.unlock();

    // Prints the weather data to display 
    lcd->printf("                ");
    lcd->setCursor(0, 0);
    lcd->printf("%s", weatherCondition.c_str());
    lcd->setCursor(0, 1);
    lcd->printf("%i degrees", outdoorTemp);
}

// Menu for changing the location used to retrive weather data
void changeLocationMenu(SharedData *sharedData, DFRobot_RGBLCD *lcd, AnalogIn *pot, Thread *weatherThread, ChangeLocationData *changeLocationData) {

    // Get the old city
    sharedData->mutex.lock();
    string oldCity = sharedData->city;
    sharedData->mutex.unlock();

    string confirmedLetters = ""; 
    char currentLetter = 'A'; 
    int amountOfConfirmedLetters = 0;
    lcd->setCursor(0, 0);
    lcd->printf("New City:");
    lcd->setCursor(0, 1);

    while (changeLocationData->changeLocation == true) {

        float potValue = pot->read(); 
        int letterIndex = potValue * 26; // Maps pot value to letter index (0-25)
        currentLetter = 'A' + letterIndex; // Updates letter based on pot value using ascii values starting from A(65)

        // Changes the last letter to be a space insted of "["
        if (currentLetter == '[') {
            currentLetter = ' ';
        }

        lcd->setCursor(amountOfConfirmedLetters, 1);
        lcd->printf("%c", currentLetter);
        
        // Add current letter to confirmed letters string and print the confirmed letters
        if (changeLocationData->confirmLetter == true && amountOfConfirmedLetters < 16) {

            confirmedLetters += currentLetter; 
            amountOfConfirmedLetters++;
            lcd->setCursor(0, 1);
            lcd->printf("%s", confirmedLetters.c_str());
            changeLocationData->confirmLetter = false;

        } else if (changeLocationData->removeLetter == true && amountOfConfirmedLetters > 0) { // Removes the confirmed letter from the letter string
            
            confirmedLetters.pop_back(); 
            amountOfConfirmedLetters--;
            lcd->clear();
            lcd->printf("New City:");
            lcd->setCursor(0, 1);
            lcd->printf("%s", confirmedLetters.c_str());
            changeLocationData->removeLetter = false;
            
        } 

        // If the confirm button is pressed we check if the new city is valid by running the api and looking at the return
        if (changeLocationData->confirmChange == true) {

            changeLocationData->confirmChange = false;

            // Replaces any spaces in the string with the HTML code for space (%20)
            string newCity;
            bool spaceFound = false;
            for (int i = 0; i < confirmedLetters.length(); i++) {
                printf("\nTransforming spaces");
                if (confirmedLetters[i] == ' ') {
                    if (!spaceFound) {
                        newCity += "%20";
                        spaceFound = true;
                    }
                } else {
                    newCity += confirmedLetters[i];
                    spaceFound = false;
                }
            }

            // Uses a mutex to safely set the new city
            sharedData->mutex.lock();
            sharedData->city = newCity;
            sharedData->mutex.unlock();

            // Ensures that the threads run in proper order
            sharedData->mainThreadFlag.clear(mainFlagBtn);
            sharedData->weatherThreadFlag.set(weatherFlagBtn);
            sharedData->mainThreadFlag.wait_all(mainFlagBtn);
            sharedData->mainThreadFlag.clear(mainFlagBtn);

            // Uses a mutex to safely revert the change if the city isnt valid
            sharedData->mutex.lock();

            if (sharedData->city == "error") {
                lcd->clear();
                lcd->printf("Non valid city");
                sharedData->city = oldCity;
                thread_sleep_for(1000);
            } 

            sharedData->mutex.unlock();
            lcd->clear();    
            return;
        }  
    }

    lcd->clear();  
}

// Menu used to show the 3 lates top news stories from the rss feed
string rssMenu(SharedData *sharedData, DFRobot_RGBLCD *lcd, bool *menuSwitched) {

    // Locks the mutex for safe retrival of the RSS feed strings
    sharedData->mutex.lock();
    string rssFeedTitle = sharedData->rssFeedTitle;
    string newsTitle1 = sharedData->newsTitle1;
    string newsTitle2 = sharedData->newsTitle2;
    string newsTitle3 = sharedData->newsTitle3;
    sharedData->mutex.unlock();

    // Creates a new string containg all the RSS headling titles and adds spaces between them for better viewing when shown on LCD
    string fullRssFeed = "                " + newsTitle1 + 
                         "                " + newsTitle2 + 
                         "                " + newsTitle3 + 
                         "                ";
    
    // Prints the title for the RSS news feed
    lcd->setCursor(0, 0);
    lcd->printf("%s", rssFeedTitle.c_str());

    return fullRssFeed;
}