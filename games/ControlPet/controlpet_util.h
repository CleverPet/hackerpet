//Please make sure that Wifi is ready before calling the remote_util functions

#include <Particle.h>

using namespace std;

//Calculate a broadcast address from the local IP and netmask
IPAddress mgschwan_getBroadcastAddress(); 

//Send the string via UDP to the address <remote>
void mgschwan_sendStringUDP(String message, IPAddress &remote);

//Receive a string via UDP enclosed in a @......;
bool mgschwan_recvStringUDP(String &message);


void mgschwan_setupNetwork();
bool mgschwan_recvStringTCP(String &message);
bool mgschwan_sendStringTCP(String message);


//Construct a message that will instruct the receiver to play the specified sound file
void mgschwan_playRemoteSound (String sound, IPAddress &remote);



