/**
 * @file   apiThreads.cpp
 * @author Tobias Kallevik
 * @author Alexander Ruud
 * @author Thomas Markussen
*/

#include "apiThreads.h"
#include <cstdio>
#include "TLSSocket.h"
#include "ipgeolocation_ca_certificate.h"

// Thread to run the API from ipgeolocation.io
void timeThreadFunc(void *arg) {
   
    SharedData* sharedData = static_cast<SharedData*>(arg);

    while (true) {
        // Wait for signal, locks mutex and takes timestamp
        sharedData->timeThreadFlag.wait_all(timeFlagBtn);
        sharedData->mutex.lock();
        sharedData->lastTimeApiRunTime = time(NULL);

        // Sets up the TLS socket
        SocketAddress address;
        sharedData->network->get_ip_address(&address);
        TLSSocket *socket = new TLSSocket;
        socket->set_timeout(500);
        socket->open(sharedData->network);

        // Connects the TLS socket to the API server
        const char host[] = "api.ipgeolocation.io";
        sharedData->network->gethostbyname(host, &address);
        address.set_port(443);
        socket->set_root_ca_cert(ipgeolocation_ca_certificate);
        nsapi_size_or_error_t result = socket->connect(address);

        // If test to ensure connection before trying to send/recive data
        if (result != 0) {
            // Closes and deletes the socket
            socket->close();
            delete socket;
            printf("\nFailed to connect to API server");
            // Clears the thread flag, ensuring only one run per "call"
            sharedData->timeThreadFlag.clear(timeFlagBtn); 
            // Unlocks the mutex
             sharedData->mutex.unlock(); 
            // Starts at the top of the loop again
            continue;
        }

        // Creates the request to be sent
        const char request[] = "GET /timezone?apiKey=3a3e3d923a45438581920fee5e4b26d1 HTTP/1.1\r\n"
                               "Host: api.ipgeolocation.io\r\n"
                               "Connection: close\r\n""\r\n";

        // Gets the length of the message and inizialises bytes_sent to 0
        nsapi_size_t bytes_to_send = strlen(request);
        nsapi_size_or_error_t bytes_sent = 0; 

        // Sends the GET request to the API server
        while (bytes_to_send) {
            bytes_sent = socket->send(request + bytes_sent, bytes_to_send); // Sends a chunk of the message staring from the last byte sent
            bytes_to_send -= bytes_sent; // Subtracts the number of sent bytes from the remaing bytes
        }

        // Variables used for reciving the response
        static char buffer[1200];
        int buffer_length = sizeof(buffer);
        memset(buffer, 0, buffer_length);
        int remaining_bytes = buffer_length;
        int received_bytes = 0;

        // Recives the response
        while (remaining_bytes > 0) {
            nsapi_size_or_error_t result = socket->recv(buffer + received_bytes, remaining_bytes);
            if (result <= 0) break;

            received_bytes += result;
            remaining_bytes -= result;
        }

        // Closes and deletes the socket
        socket->close();
        delete socket;

        // Parses the data to a JSON object
        char *json_begin = strchr(buffer, '{');
        char *json_end = strrchr(buffer, '}');
        json_end[1] = 0;
        printf("\n%s\n", json_begin);
        json jComplete = json::parse(json_begin);

        // Extracts the JSON
        size_t epochtime = jComplete["date_time_unix"];
        sharedData->timezoneOffsetWithDst = jComplete["timezone_offset_with_dst"];
        sharedData->latitude = jComplete["geo"]["latitude"];
        sharedData->longitude = jComplete["geo"]["longitude"];
        
        // Sets the RTC
        set_time(epochtime + (sharedData->timezoneOffsetWithDst * 3600));

        // Clears the thread flag. This ensures the thread only runs one time when called
        sharedData->timeThreadFlag.clear(timeFlagBtn); 
        
        // Sets an event flag to unlock the main thread after first run, only extracts the city once to avoid overwriting user set city
        if (sharedData->firstTimeApiRun == true) {
            sharedData->mainThreadFlag.set(mainFlagBtn); 
            sharedData->city = jComplete["geo"]["state_prov"];
            sharedData->firstTimeApiRun = false;
        }
        
        // Unlocks the mutex
        sharedData->mutex.unlock(); 
    }
}

// Thread to run the API from weatherapi.com provides weather data
void weatherThreadFunc(void *arg) {

    SharedData* sharedData = static_cast<SharedData*>(arg);
    
    while (true) {
        // Wait for signal, locks mutex and takes timestamp
        sharedData->weatherThreadFlag.wait_all(weatherFlagBtn);
        sharedData->mutex.lock(); 
        sharedData->lastWeatherApiRunTime = time(NULL); 

        // Creates TCP socket and address objects
        TCPSocket socket; 
        SocketAddress address; 

        // Finds the destination IP, sets the port number, opens a socket and connect the socket
        sharedData->network->gethostbyname("api.weatherapi.com", &address); //
        address.set_port(80);   
        socket.open(sharedData->network); 
        nsapi_size_or_error_t result = socket.connect(address); 

        // If test to ensure connection before trying to send/recive data
        if (result != 0) {
            // Closes the socket
            socket.close();
            // Sets the main thread flag, clears the weather thrad flag and clears the mutex
            // We need to clear the main thread flag since we need to block the main thread when a user changes a city, so that we can run this thread and check if the city is valid before proceeding
            sharedData->mainThreadFlag.set(mainFlagBtn);
            sharedData->weatherThreadFlag.clear(weatherFlagBtn); 
            // Unlocks the mutex
            sharedData->mutex.unlock(); 
            // Starts at the top of the loop again
            continue;
        }

        // Builds the weather request
        ostringstream oss;
        oss << "GET /v1/current.json?key=4d53a85a07d04f84a72210133232802&q=" << sharedData->city << " HTTP/1.1\r\n"
            << "Host: api.weatherapi.com\r\n"
            << "Connection: close\r\n"
            << "\r\n";

        string requestString = oss.str();
        const char *request = requestString.c_str();
    
        // Gets the length of the message to be sent and initializes the byte_sent var to 0
        nsapi_size_t bytes_to_send = strlen(request); 
        nsapi_size_or_error_t bytes_sent = 0; 

        // Sends the GET request
        while (bytes_to_send) {
            bytes_sent = socket.send(request + bytes_sent, bytes_to_send); 
            bytes_to_send -= bytes_sent; 
        }

        // Variables used to recive the message
        char buffer[400];
        int received_bytes = 0;
        char message[1500] = {0};
        int size = 0;

        // Recives the response in chunks, gradually buliding the response char. Breaks of when result is 0, meaning the entire message has been recived
        while (1) {
            nsapi_size_or_error_t result = socket.recv(buffer, sizeof(buffer)); 
            
            if (result <= 0) break; 
    
            memcpy(message + size, buffer, result); 
            size += result; 
            thread_sleep_for(100); 
        }  

        // Closes the TCP socket
        socket.close();

        string jsonMessage = message;  

        // If the response contain the word "erro", it means that the city tried to retrive weather data from wasn't recognized. This need to be done since user can change city
        if (jsonMessage.find("error") != std::string::npos) {
            printf("%s", jsonMessage.c_str());
            sharedData->city = "error";
        } else {
            // Parses the response to a JSON object
            int start_pos = jsonMessage.find("{"); 
            int end_pos = jsonMessage.rfind("}}"); 
            string output = jsonMessage.substr(start_pos, end_pos - start_pos + 2); 
            printf("\n%s\n", output.c_str()); 
            json jComplete = json::parse(output); 

            // Extracts the data from the JSON object
            sharedData->outdoorTemp = jComplete["current"]["temp_c"]; 
            sharedData->weatherCondition = jComplete["current"]["condition"]["text"];
        }

        // Sets the main thread flag, clears the weather thrad flag and clears the mutex
        // We need to clear the main thread flag since we need to block the main thread when a user changes a city, so that we can run this thread and check if the city is valid before proceeding
        sharedData->mainThreadFlag.set(mainFlagBtn);
        sharedData->weatherThreadFlag.clear(weatherFlagBtn); 
        sharedData->mutex.unlock(); 
       
    }
}

// Thread to get the RSS feed
void rssThreadFunc(void *arg) {

    SharedData* sharedData = static_cast<SharedData*>(arg);
    
    while (true) {
        // Wait for signal, locks mutex and takes timestamp
        sharedData->rssThreadFlag.wait_all(rssFlagBtn);
        sharedData->mutex.lock(); 
        sharedData->lastRssRunTime = time(NULL); 

        // Creates TCP socket and address objects
        TCPSocket socket; 
        SocketAddress address; 

        // Finds the destination IP, sets the port number, opens a socket and connect the socket
        sharedData->network->gethostbyname("feeds.feedburner.com", &address); //
        address.set_port(80);
        socket.open(sharedData->network); 
        nsapi_size_or_error_t result = socket.connect(address);

        // If test to ensure connection before trying to send/recive data
        if (result != 0) {
            // Closes the socket
            socket.close();
            // Clears the thread flag and unlocks the mutex
            sharedData->rssThreadFlag.clear(rssFlagBtn); 
            sharedData->mutex.unlock();
            // Starts at the top of the loop again
            continue;
        }
        

        // Creates the GET request
        const char request[] = "GET /TheHackersNews?format-xml HTTP/1.1\r\n"
                               "Host: feeds.feedburner.com\r\n"
                               "Connection: close\r\n""\r\n";
    
        // Gets the length of the message to be sent and initializes the byte_sent var to 0
        nsapi_size_t bytes_to_send = strlen(request); 
        nsapi_size_or_error_t bytes_sent = 0; 

        // Sends the GET request
        while (bytes_to_send) {
            bytes_sent = socket.send(request + bytes_sent, bytes_to_send); 
            bytes_to_send -= bytes_sent; 
        }

        // Variables used to recive the message
        string message;
        char buffer[1500];
        string search = "</item>";
        
        // Recives the response in chunks, gradually buliding the response char. We count the amount of </item> and break the loop when 3 or more items have been retrived. 
        // This is done to reduce the time needed to recive data. The RSS feed is big and takes a bit of time to recive. Since we only need 3, we can shorten the load time by breaking the loop here. 
        while (true) {
            // Receive data from the socket and store the number of bytes received
            nsapi_size_or_error_t result = socket.recv(buffer, sizeof(buffer)); 
            message.append(buffer, result);

            // Searches through the recived chunk to find occurrences of </item>.  
            size_t pos = message.find(search);
            int count = 0;
            while (pos != string::npos) { 
                count++; 
                pos = message.find(search, pos + search.size()); // Find the next occurrence of </item> starting from last occurrenc
            }

            // Breaks the loop when three </item> have been found
            if (count >= 3) break; 

            thread_sleep_for(100); 
        }

        // Closes the TCP socket
        socket.close();

        // Veriables used to extract the feed/title strings
        string rssTitle, title1, title2, title3;
        size_t start, end;
        int count = 0;
        
        // Find the first occurrence of "<title>"
        start = message.find("<title>");
        while (start != string::npos) {
            // Find the next occurrence of "</title>"
            end = message.find("</title>", start);
            if (end == string::npos) {
                break;
            }
            // Extract the text between the tags
            string title = message.substr(start + 7, end - start - 7);
            count++;
            if (count == 1) {
                rssTitle = title;
            } else if (count == 2) {
                title1 = title;
            } else if (count == 3) {
                title2 = title;
            } else if (count == 4) {
                title3 = title;
                break;
            }
            // Find the next occurrence of "<title>"
            start = message.find("<title>", end);
        }

    
        printf("\n%s\n%s\n%s\n%s\n", rssTitle.c_str(), title1.c_str(), title2.c_str(), title3.c_str());

        // Adds the titles to the shared data struct. We separate the tiles for easier manipulation later
        sharedData->rssFeedTitle = rssTitle;
        sharedData->newsTitle1 = title1;
        sharedData->newsTitle2 = title2;
        sharedData->newsTitle3 = title3;

        // Clears the thread flag and unlocks the mutex
        sharedData->rssThreadFlag.clear(rssFlagBtn); 
        sharedData->mutex.unlock();
    }
}