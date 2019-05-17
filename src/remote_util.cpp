#include "remote_util.h"

UDP Udp;
int broadcastPort = 4888;
bool udp_begin = false;

void sendStringUDP(String message, IPAddress &remote) {
    if (!udp_begin) {
        //We need to listen at so&me port because begin() requires us to specify a listening port
        Udp.begin(broadcastPort+1);
        udp_begin = true;
    }
    Udp.sendPacket(message, message.length(), remote, broadcastPort);
}

IPAddress getBroadcastAddress() {

        IPAddress localIP;
        IPAddress broadcastIP;
        IPAddress netmask;

        localIP = WiFi.localIP();
        netmask = WiFi.subnetMask();

        for (int idx = 0; idx < 4; idx++) {
            broadcastIP[idx] = localIP[idx] | ~netmask[idx];
        }

        return broadcastIP;
}

unsigned int message_repeater = 1; //Repeat the message to avoid dropped messages

void playRemoteSound (String sound, IPAddress &remote) {
    long timestamp = millis();
    String packet = String::format ("@[%d][play]<%s>",timestamp,sound.c_str());
    for (int idx = 0; idx < message_repeater; idx++) {
        sendStringUDP(packet, remote);
    }
}
