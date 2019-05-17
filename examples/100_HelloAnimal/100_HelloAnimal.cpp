/*
 *  HelloAnimal
 *  ===========
 *
 *  An introduction to the basics of building HackerPet interactions. It will
 *  wait until the Hub's Device Layer is in a good state, then turn on the main
 *  lights, and then wait for one of the Hub's touchpads to be touched.
 *
 *  Author: CleverPet
 *
 *  Copyright 2018
 *  Licensed under the AGPL 3.0
 */

#include <hackerpet.h>

// enables simultaneous execution of application and system thread, per
// https://docs.particle.io/reference/device-os/firmware/photon/#system-thread
SYSTEM_THREAD(ENABLED);

#define PLAYER_NAME "Pet, Clever"             // player

// access to hub functionality (lights, foodtreats, etc.)
HubInterface hub;

bool HelloAnimal()
{
    yield_begin();
    static unsigned long timer_ms;
    static unsigned long playstart;


    // before starting interaction, wait until:
    //  0. Device is on.
    //  1. device layer is ready (in a good state)
    //  2. foodmachine is "idle", meaning it is not spinning or dispensing
    //          and tray is retracted (see FOODMACHINE_... constants)
    //  3. no touchpad is currently pressed
    yield_wait_for(hub.IsReady()
                        && hub.FoodmachineState() == hub.FOODMACHINE_IDLE
                        && not hub.AnyButtonPressed(),
                    false);

    // mark the start time of the interaction
    playstart = Time.now();

    // start the timer
    timer_ms = millis();


    // DI reset occurs if, for example, DL detects that touchpads
    // need re- calibration, such as when a hub is moved to a
    // different surface. Locking prevents DI board from resetting
    // during an interaction (would cause lights to go blank etc.)
    hub.SetDIResetLock(true);

    // turn on lights
    hub.SetLights(hub.LIGHT_BTNS, 60, 60, 0);

    // Wait until a touchpad has been pressed on the Hub
    yield_wait_for(hub.AnyButtonPressed(), false);

    // record the moment the touchpad was pressed
    timer_ms = millis() - timer_ms;

    yield_sleep_ms(400, false); // delay 400 ms in case a touchpad sound is still playing

    // turn off lights
    hub.SetLights(hub.LIGHT_BTNS, 0, 0, 0);

    // keep them off for 1000 ms
    yield_sleep_ms(1000, false);


    // allow DI board to reset if needed between interactions -- should
    // probably be last thing done before hub.Run() and
    // waiting until hub.IsReady(), etc.
    hub.SetDIResetLock(false);

    hub.Report( Time.format(playstart, TIME_FORMAT_ISO8601_FULL),    // play_start_time
                PLAYER_NAME,        // player
                0,                  // level -- 0 because there are no levels here
                "PAD_TOUCHED",      // result
                timer_ms,           // how long it took for the pad to get pressed
                0,                  // foodtreat_presented
                0                   // foodtreat_eaten
            );

    yield_finish();
    return true;
}

void setup()
{
  // Initializes the hub and passes the current filename as ID for reporting
  hub.Initialize(__FILE__);
}


void loop()
{
    // advance the device layer state machine, but with 20 millisecond max time
    // spent per loop cycle
    hub.Run(20);

    HelloAnimal();
}
