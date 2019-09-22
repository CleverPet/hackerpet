/*
 * TCPDebug
 * ========
 *
 * This is an example how to send debug messages over a TCP connection
 *
 * For the receiver run a program that prints the output of incoming tcp
 * connections. For exmample on linux you can use nc  (netcat) netcat usage:
 * nc -ln 4888
 *
 * NOTE: The receiver must be started before the hub is started
 */

#include <hackerpet.h>

using namespace std;

unsigned long last_timestamp;

SYSTEM_THREAD(ENABLED);

TCPClient client;
byte server[] = { 192, 168, 0, 227 };  //Change to the IP that listens for debug connection
int port = 4888;                       //Port where the receiver listens
String logString;

//Write the message over the TCP connection if it is connected
void remotePrintLn(char *message)
{
    if (client.connected())
    {
        client.println(message);
    }
}

// initialize hub interface (from CleverpetLibrary)
HubInterface hub;

// to avoid quirks define setup() nearly last, and right before loop()
void setup()
{
    Particle.variable("logString", logString);
    Particle.publish("start-game");

    Serial.begin(9600);  // for debug text output

    // Initializes the hub and passes the current filename as ID for reporting
    hub.Initialize(__FILE__);

    if (client.connect(server, port))
    {
        logString = "connected ";
        logString += millis();
        client.println("Connected ...");
    } else
    {
        logString = "did not connect ";
        logString += millis();
    }

    last_timestamp = millis();
}

// run-loop function required for Particle
void loop()
{
  // advance the device layer state machine, but with 20 millisecond max time
  // spent per loop cycle
  hub.Run(20);

  if (millis() - last_timestamp > 1000) {
    remotePrintLn("Next message");
    last_timestamp = millis();
    }

}
