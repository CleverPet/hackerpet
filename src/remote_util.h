//Please make sure that Wifi is ready before calling the remote_util functions

#include <Particle.h>

using namespace std;

//Calculate a broadcast address from the local IP and netmask
IPAddress getBroadcastAddress();

//Send the string via UDP to the address <remote>
void sendStringUDP(String message, IPAddress &remote);

//Construct a message that will instruct the receiver to play the specified sound file
void playRemoteSound (String sound, IPAddress &remote);
