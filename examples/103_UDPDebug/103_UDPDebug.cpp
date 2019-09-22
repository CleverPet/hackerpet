/*
 * UDPDebug
 * ========
 *
 * This is an example how to send debug messages over a UDP connection
 * using the Papertrail loghandler.
 *
 * For the receiver run a program that prints the output of incoming tcp
 * connections.
 * For exmample on linux you can use nc  (netcat)
 * netcat usage:   nc -lnu 4888
 *
 * NOTE: You must add the papertrail library to your project
 */

#include <papertrail.h>
#include <hackerpet.h>

using namespace std;

unsigned long last_timestamp;

SYSTEM_THREAD(ENABLED);

// initialize device layer interface (from CleverpetLibrary)
HubInterface hub;

// Use the IP address and port where the receiver is listening
PapertrailLogHandler papertrailHandler("192.168.1.191", 4888, "UDPDebug_Demo");


// to avoid quirks define setup() nearly last, and right before loop()
void setup()
{
    Serial.begin(9600);  // for debug text output

    // Initializes the hub and passes the current filename as ID for reporting
    hub.Initialize(__FILE__);

    last_timestamp = millis();
}

// run-loop function required for Particle
void loop()
{
  // advance the device layer state machine, but with 20 millisecond max time
  // spent per loop cycle
  hub.Run(20);

  if (millis() - last_timestamp > 1000) {
    Log.info("Elapsed:  %d\n", millis() - last_timestamp);
    last_timestamp = millis();
    }

}
