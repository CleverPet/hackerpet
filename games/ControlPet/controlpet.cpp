/**
    controlpet.cpp
    Description: ControlPet Server for remote control of the CleverPet Hub

    Author: Michael Gschwandtner
    Copyright:  Copyright 2019, Michael Gschwandtner
    License: MIT
    Email: mgschwan@gmail.com
    Version: 1.0 5/22/2019
*/

// This #include statement was automatically added by the Particle IDE.
#include <papertrail.h>

// This #include statement was automatically added by the Particle IDE.
#include "controlpet_util.h"

// This #include statement was automatically added by the Particle IDE.
#include <hackerpet.h>

using namespace std;

IPAddress broadcastAddress;
bool system_ready = false;

String deviceID;

unsigned char hubButtonPressed = 0;
int hubFoodtreatDuration = 5000;

unsigned long last_timestamp;
unsigned long last_button_sent[3];



// Use the IP address and port where the receiver is listening
PapertrailLogHandler papertailHandler("192.168.0.255", 4888, "ControlPet");

SYSTEM_THREAD(ENABLED); 

HubInterface *dli = NULL;

// to avoid quirks define setup() nearly last, and right before loop()
void setup()
{

    Serial.begin(9600);  // for debug text output
    Serial1.begin(38400);  // needed for dli

    last_timestamp = millis();
    last_button_sent[0] = millis();
    last_button_sent[1] = millis();
    last_button_sent[2] = millis();

    deviceID = System.deviceID();
}


void reinitializeHub()
{
    // before starting trial, wait until:
    //  1. device layer is ready (in a good state)
    //  2. foodmachine is "idle", meaning it is not spinning or dispensing food and tray is retracted (see FOODMACHINE_... constants)
    //  3. no button is currently pressed
    
    Log.info("reinitialize hub");
    dli->SetDIResetLock(false);  // allow DI board to reset if needed between trials

    while(true)
    {
        dli->Run(20);
        hubButtonPressed = dli->AnyButtonPressed();    
        if (dli->IsReady() && dli->FoodmachineState() == dli->FOODMACHINE_IDLE && hubButtonPressed == 0)
        {
            // DI reset occurs if, for example, DL detects that touchpads need re-calibration, like if hub was moved to a different surface
            dli->SetDIResetLock(true);  // prevent DI board from resetting during a trial (would cause lights to go blank etc.)
            break;
        }    
    }
    dli->SetLights(dli->LIGHT_BTNS, 0, 0, 0);   
    Log.info("reinitialization complete");
}


const String cmd_foodtreat_dispense = String("dispense");
const String cmd_check_buttons = String("buttons");
const String cmd_set_light = String("light");
const String cmd_play_audio = String("playaudio");
const String cmd_reinitialize = String("reinitialize");


const String audio_entice = String("entice");
const String audio_positive = String("positive");
const String audio_do = String("do");
const String audio_click = String("click");
const String audio_squeak = String("squeak");
const String audio_negative = String("negative");
const String audio_left = String("left");
const String audio_middle = String("middle");
const String audio_right = String("right");

unsigned char indexToLight (unsigned char index) {
    if (index == 0) return dli->LIGHT_LEFT;
    if (index == 1) return dli->LIGHT_MIDDLE;
    if (index == 2) return dli->LIGHT_RIGHT;
    if (index == 3) return dli->LIGHT_CUE;
    return 0;
}
        
unsigned char lightToIndex(unsigned char light) {
    if (light == dli->LIGHT_LEFT)    return 0;
    if (light == dli->LIGHT_MIDDLE)  return 1;
    if (light == dli->LIGHT_RIGHT)   return 2;
    return 0;
}


String findNthSubstring(const String &message, const String &delimiter, int n)
{
    int i = 0;
    int npos = 0;

    for (i = 0; i < n+1; i++)
    {
        npos = message.indexOf(delimiter, npos);
        if (npos < 0) break;
        npos++;
    }
    if (npos >= 0)
    {
        int endOfSubstring = message.indexOf(delimiter, npos);
        if (endOfSubstring >= 0)
        {
            return message.substring(npos, endOfSubstring);
        }
        return message.substring(npos);
    }
    return String("");
}

void commandCallback(String &msg)
{
    if (msg.indexOf(cmd_foodtreat_dispense) == 1)
    {
        Log.info("Show foodtreat");

        int max_iterations = 1 + hubFoodtreatDuration/100;   
        bool foodtreat_taken = false;
        bool tray_error = true;
        
        while (max_iterations > 0)
        {
            unsigned char foodtreat_state = dli->PresentAndCheckFoodtreat(hubFoodtreatDuration);   
            dli->Run(20);
            // if state machine in one of two end states (food eaten or food not eaten), go to next trial
            if (foodtreat_state == dli->PACT_RESPONSE_FOODTREAT_NOT_TAKEN || foodtreat_state == dli->PACT_RESPONSE_FOODTREAT_TAKEN) // state machine finished
            {
                // start a new trial
                reinitializeHub();
                foodtreat_taken = foodtreat_state == dli->PACT_RESPONSE_FOODTREAT_TAKEN;
                tray_error = false;
                break;
            }
        }
        
        if (tray_error)
        {
            mgschwan_sendString(String::format("@error;"));
        }
        else {
            if (foodtreat_taken) mgschwan_sendString(String::format("@ok:taken:;"));
            else mgschwan_sendString(String::format("@ok:not_taken:;"));
        }
    }
    if (msg.indexOf(cmd_check_buttons) == 1)
    {
        mgschwan_sendString(String::format("@buttons:%d:;",hubButtonPressed));
        hubButtonPressed = 0;
    }
    if (msg.indexOf(cmd_play_audio) == 1)
    {
        String sound = findNthSubstring(msg, String(":"),0);
        if (sound.equals(audio_negative)) dli->PlayAudio(dli->AUDIO_NEGATIVE, 20);
        if (sound.equals(audio_positive)) dli->PlayAudio(dli->AUDIO_POSITIVE, 20);
        if (sound.equals(audio_do)) dli->PlayAudio(dli->AUDIO_DO, 20);
        if (sound.equals(audio_click)) dli->PlayAudio(dli->AUDIO_CLICK, 20);
        if (sound.equals(audio_squeak)) dli->PlayAudio(dli->AUDIO_SQUEAK, 20);
        if (sound.equals(audio_left)) dli->PlayAudio(dli->AUDIO_L, 20);
        if (sound.equals(audio_middle)) dli->PlayAudio(dli->AUDIO_M, 20);
        if (sound.equals(audio_right)) dli->PlayAudio(dli->AUDIO_R, 20);
    
        mgschwan_sendString(String("@ok;"));
    }
    if (msg.indexOf(cmd_set_light) == 1)
    {
        String light_idx = findNthSubstring(msg, String(":"),0);
        String yellow = findNthSubstring(msg, String(":"),1);
        String blue = findNthSubstring(msg, String(":"),2);
        
        Log.info(String("Set light ") + light_idx +" "+yellow+" "+blue);
        
        int yellow_val = max(0,min(100,yellow.toInt()));
        int blue_val = max(0,min(100,blue.toInt()));
        
        dli->SetLights(indexToLight(light_idx.toInt()), yellow_val , blue_val , 0);
        
        mgschwan_sendString(String("@ok;"));
    }
    if (msg.indexOf(cmd_reinitialize) == 1)
    {
        reinitializeHub();
        mgschwan_sendString(String("@ok;"));
    }
}



// run-loop function required for Particle
void loop()
{
    
    static String message;
    
    // advance the device layer state machine, but with 20 millisecond max time spent per loop cycle
    if (dli)
    {
        dli->Run(20);
    }
    
    if (WiFi.ready() && system_ready == false)
    {
        broadcastAddress = mgschwan_getBroadcastAddress();
        system_ready = true;
        mgschwan_setupNetwork(); //Open TCP Port
        Log.info("Wifi Ready");
        
        // initialize device layer interface (from CleverpetLibrary)
        dli = new HubInterface();
        Log.info("Device Layer created");
        
        dli->SetDoPollDiagnostics(true); //start polling the diagnostics
        dli->SetDoPollButtons(true); //start polling the buttons
        dli->PlayTone(0, 5, 10); // turn off sound
        dli->SetLights(dli->LIGHT_BTNS, 0, 0, 0);   

        Log.info ("Hub setup complete");

        reinitializeHub();
    }
    else {
        //Waiting for the Wifi to become ready        
    }

    if (system_ready) 
    {
        mgschwan_serve_webinterface();

        unsigned char buttons = dli->AnyButtonPressed();
        if (buttons != 0)
        {
            hubButtonPressed |= buttons;
            
            if (millis()-last_button_sent[lightToIndex(buttons)] > 400) //Prevent double taps
            {
                mgschwan_sendString(String::format("@button_event:%d:;",buttons));
                last_button_sent[lightToIndex(buttons)]  = millis();
            }
        }
        
    
        if (millis() - last_timestamp > 5000)
        {
            last_timestamp = millis();
            //Announce presence
            mgschwan_sendStringUDP(String::format("@shout:")+deviceID+":;", broadcastAddress);
            
        }

        mgschwan_websocket_loop();

        if (mgschwan_recvStringUDP(message))
        {
            commandCallback(message);
            Log.info(String("UDP Received: ")+message);
        }

        if (mgschwan_recvString(message))
        {
            commandCallback(message);
            Log.info(String("Message Received: ")+message);
        }
    }
}

