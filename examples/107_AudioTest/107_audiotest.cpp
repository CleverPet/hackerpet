/*
 *  Audiotest
 *  ==========
 *
 *  This is an example that lets you test the speaker and all available audio
 *	samples stored on the hub.
 *	The middle touchpad will scroll through and play all the different samples,
 *	the left touchpad will decrease volume and the right touchpad will increase
 *	volume.
 *
 *  Author: jelmer@tiete.be
 *
 *  Copyright 2018
 *  Licensed under the AGPL 3.0
 *
 */

#include <hackerpet.h>

// enables simultaneous execution of application and system thread, per
// https://docs.particle.io/reference/device-os/firmware/photon/#system-thread
SYSTEM_THREAD(ENABLED);

// Use primary serial over USB interface for logging output (9600)
// Choose logging level here (ERROR, WARN, INFO)
SerialLogHandler logHandler(LOG_LEVEL_INFO, { // Logging level for all messages
    { "app.hackerpet", LOG_LEVEL_ERROR }, // Logging level for library messages
    { "app", LOG_LEVEL_INFO } // Logging level for application messages
});

// access to hub functionality (lights, foodtreats, etc.)
HubInterface hub;

unsigned char volume = 20;
unsigned char pressed = 0;
unsigned char sample_num = 1;
bool ready = false;

// available samples:
// AUDIO_ENTICE = 1;
// AUDIO_POSITIVE = 2;
// AUDIO_DO = 3;
// AUDIO_CLICK = 4;
// AUDIO_SQUEAK = 5;
// AUDIO_NEGATIVE = 6;
// AUDIO_L = 7;
// AUDIO_M = 8;
// AUDIO_R = 9;

void setup()
{
    Serial1.begin(38400);  // needed for device layer (hub) communication
    Log.info("Starting audio testing firmware");

    // Initializes the hub and passes the current filename as ID for reporting
    hub.Initialize(__FILE__);

    hub.SetButtonAudioEnabled(false); // disable the standard button sounds
}

void loop() {
    // advance the device layer state machine, but with 20 millisecond max time
    // spent per loop cycle
    hub.Run(20);

    // wait for unpressed state
    if (hub.IsReady() && (hub.AnyButtonPressed()==0))
    { ready = true;}

	// check if touchpad is being pressed
    if(ready && hub.AnyButtonPressed() != 0){
    	ready = false;
   		pressed = hub.AnyButtonPressed(); // check which touchpad is pressed
   		hub.SetLightsRGB(pressed, 99, 0, 0, 0); // turn on touchpad light
   		if(pressed == hub.BUTTON_LEFT){ // decrease volume
   			if(volume < 10){volume = 0;}
   			else{volume = volume - 10;}
   			Log.info("Volume: %u", volume);
   		}
   		if(pressed == hub.BUTTON_MIDDLE){ // select next sample
   			Log.info("Next audiosample");
   			sample_num++;
   			if(sample_num > 9){sample_num = 1;}
   		}
   		if(pressed == hub.BUTTON_RIGHT){ // increase volume
   			volume = volume + 10;
   			if(volume > 89){volume = 90;}
   			Log.info("Volume: %u", volume);
   		}
   		hub.PlayAudio(sample_num, volume);	// play the sample
   		hub.SetLights(hub.LIGHT_BTNS, 0, 0, 0);  // turn off lights
    }
}
