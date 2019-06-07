#include "remote_util.h"

UDP mgschwan_Udp;
int mgschwan_broadcastPort = 4888;
bool mgschwan_udp_begin = false;
#define MESSAGE_MAX_LEN 512

#define SEND_TIMEOUT 5000

#define RECV_STATE_NEW 0
#define RECV_STATE_ONGOING 1
#define RECV_STATE_ERROR 2
#define RECV_STATE_FINISHED 3

int recv_buffer_idx = 0;
int recv_state = RECV_STATE_ERROR;
char recv_buffer[MESSAGE_MAX_LEN+1];


int tcp_recv_buffer_idx = 0;
int tcp_recv_state = RECV_STATE_NEW;
char tcp_recv_buffer[MESSAGE_MAX_LEN+1];

TCPServer server = TCPServer(mgschwan_broadcastPort+1);
TCPClient client;

void mgschwan_setupNetwork()
{
  server.begin();
}
    
void mgschwan_sendStringUDP(String message, IPAddress &remote) {
    if (!mgschwan_udp_begin) {
        //We need to listen at so&me port because begin() requires us to specify a listening port
        mgschwan_Udp.begin(mgschwan_broadcastPort+1);
        mgschwan_udp_begin = true;
    }
    mgschwan_Udp.sendPacket(message, message.length(), remote, mgschwan_broadcastPort);
}

bool mgschwan_sendStringTCP(String message) {
    if (client.connected()) {
        const uint8_t *msg_str = (const uint8_t *) message.c_str();
        client.write(msg_str, message.length(), SEND_TIMEOUT );
        return true;
    }
    return false;
}

bool mgschwan_recvStringTCP(String &message)
{
    int c;
    if (client.connected()) {
        while (client.available())
        {
          c = client.read();
          if (tcp_recv_state == RECV_STATE_NEW || tcp_recv_state == RECV_STATE_ONGOING )
          {
            if (tcp_recv_state == RECV_STATE_NEW && c == '@') 
            {
                Log.info("Start marker found");
                tcp_recv_state = RECV_STATE_ONGOING;        
                tcp_recv_buffer_idx = 0;
            }
                
            if (c >= 0 && tcp_recv_state == RECV_STATE_ONGOING)
            {
                    if (tcp_recv_buffer_idx > MESSAGE_MAX_LEN)
                    {
                        tcp_recv_state == RECV_STATE_ERROR;
                    }
                    else {
                        tcp_recv_buffer[tcp_recv_buffer_idx] = (char)c;
                        tcp_recv_buffer_idx ++;
                        if (c == ';') {
                            Log.info("End marker found");
                            //Message end marker found
                            tcp_recv_buffer[tcp_recv_buffer_idx] = 0;
                            tcp_recv_state = RECV_STATE_FINISHED;
                        }
                    }
                }
            }
            
            if (tcp_recv_state == RECV_STATE_FINISHED) {
               tcp_recv_state = RECV_STATE_NEW;
               message.remove(0);
               message += String(tcp_recv_buffer);
               return true;
            }
            if (tcp_recv_state == RECV_STATE_ERROR)
            {
                tcp_recv_state = RECV_STATE_NEW;
            }
        }
    } else {
        client = server.available();
        if (client.connected())
        {
            Log.info ("Client connected");
        }
    }
    return false;
}



bool mgschwan_recvStringUDP(String &message)
{
    int data_size = mgschwan_Udp.parsePacket();
    int idx = 0;
    int c;
    if (data_size >= 0)
    {
        while (idx < data_size)
        {
            c = mgschwan_Udp.read();
            
            if (recv_state == RECV_STATE_NEW || recv_state == RECV_STATE_ONGOING )
            {
                
                if (recv_state == RECV_STATE_NEW && c == 64) //Search for @ character
                {
                    recv_state = RECV_STATE_ONGOING;        
                    recv_buffer_idx = 0;
                    Log.info(String::format("Start marker found (message: %d)",data_size));
                }
                
                if (c >= 0 && recv_state == RECV_STATE_ONGOING)
                {
                    if (recv_buffer_idx > MESSAGE_MAX_LEN)
                    {
                        recv_state == RECV_STATE_ERROR;
                    }
                    else {
                        recv_buffer[recv_buffer_idx] = (char)c;
                        recv_buffer_idx ++;
                        if (c == 59) { //Search for ; character
                            Log.info("End marker found");
                            //Message end marker found
                            recv_buffer[recv_buffer_idx] = 0;
                            recv_state = RECV_STATE_FINISHED;
                        }
                    }
                }
            }
            idx++;
        }
    } else {
        Log.info("Error receiving");
    }
    
    if (recv_state == RECV_STATE_FINISHED) {
        recv_state = RECV_STATE_NEW;
        message.remove(0);
        message += String(recv_buffer);
        return true;
    }
    if (recv_state == RECV_STATE_ERROR)
    {
        recv_state = RECV_STATE_NEW;
    }
    
    return false;
}




IPAddress mgschwan_getBroadcastAddress() {
 
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

unsigned int mgschwan_message_repeater = 3; //Repeat the message to avoid dropped messages

void mgschwan_playRemoteSound (String sound, IPAddress &remote) {
    long timestamp = millis();
    String packet = String::format ("@[%d][play]<%s>",timestamp,sound.c_str());
    for (int idx = 0; idx < mgschwan_message_repeater; idx++) {
        mgschwan_sendStringUDP(packet, remote);
    }
}



