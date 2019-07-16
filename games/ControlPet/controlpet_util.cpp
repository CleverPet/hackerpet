#include <MDNS.h>
#include "controlpet_util.h"
#include "WebSockets.h"
#include "WebSocketsServer.h"
#include "client/websocket/index_html.h"

MDNS mgschwan_mdns;

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
TCPServer webserver = TCPServer(80);

TCPClient client;
TCPClient webclient;

WebSocketsServer webSocket = WebSocketsServer(mgschwan_broadcastPort+2);
String webSocket_message_in;
uint8_t webSocket_client_id = 0;


void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

    switch(type) {
        case WStype_DISCONNECTED:
            break;
        case WStype_CONNECTED:
            {
                webSocket_client_id = num;
                Log.info("Websocket client connected %d", num);
            }
            break;
        case WStype_TEXT:
            webSocket_message_in.remove(0);
            webSocket_message_in += String((char *)payload);
            break;
        case WStype_BIN:
            //Binary not supported
            break;
    }

}

void mgschwan_websocket_loop() {
    /* Only process messages if not plain TCP client is connected */
    if (!client.connected())
    {
        webSocket.loop();
    }
}

void mgschwan_MDNS_loop() {
    mgschwan_mdns.processQueries();
}

bool mgschwan_setupMDNS() {
    bool success = false;

    success = mgschwan_mdns.setHostname("cleverpet");
    Log.info("MDNS: Set hostname %d",success);

    if (success) {
      success = mgschwan_mdns.addService("tcp", "http", 80, "Web Interface");
    } 

    Log.info("MDNS: Add service %d",success);

    if (success) {
      success = mgschwan_mdns.addService("tcp", "controlpet", 4889, "Remote control");
    }

    Log.info("MDNS: Add service %d",success);


    if (success) {
      success = mgschwan_mdns.addService("tcp", "websocket", 4890, "WebSocket connector");
    }

    Log.info("MDNS: Add service %d",success);

    if (success) {
        success = mgschwan_mdns.begin(true);
    }

    Log.info("MDNS: Begin %d",success);

    return success;
}

void mgschwan_serve_webinterface() {
    int c = 0, last_c = 0, last_last_c = 0;
    webclient = webserver.available();
    bool request_finished = false;
    if (webclient.connected()) {
        while (webclient.available())
        {
            c = webclient.read();
            last_last_c = last_c;
            last_c = c;
            /* Request finished. We are assuming it is a GET request since 
             * we are only serving a single html file
             * The browser will probably try to access the favicon as well
             * but he has to deal with it
             */
            if (c == '\n' && (c == last_c || c == last_last_c) )
            {
                request_finished = true;
            }
        }

        if (request_finished)
        {
            webclient.println("HTTP/1.0 200 OK");
            webclient.println("Content-type: text/html");
            webclient.print("Content-length: ");
            webclient.println(sizeof(bin2c_index_html));
            webclient.println("");
            webclient.write(bin2c_index_html, sizeof(bin2c_index_html));

            webclient.println("</body>");
        }

        delay (1); //That is a hack to allow the browser to receive the data
        webclient.stop();
    }
}



//Receive on all connected channels
bool mgschwan_recvString(String &message)
{

    if (mgschwan_recvStringTCP(message))
    {
        //Received TCP Message
        return true;
    } 
    else {
        //Receive webSocket Message
        
        if (webSocket_message_in.length() > 0)
        {
            message.remove(0);
            message += webSocket_message_in;
            webSocket_message_in.remove(0);
            return true;
        }        
    }
    return false;
}

void mgschwan_setupNetwork()
{
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  mgschwan_setupMDNS();
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

bool mgschwan_sendString(String message) {
    bool retVal = false;

    retVal = mgschwan_sendStringTCP(message);
    retVal = retVal || webSocket.sendTXT(webSocket_client_id, message.c_str());
    
    return retVal;
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



