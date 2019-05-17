/*
 *  WhackAMole
 *  ==========
 *
 *  Once your dog or cat can see the lights easily, this is a more "active" game
 *  for the Hub that challenges your dog or cat's "paw-eye" coordination
 *
 *  By adjusting how the values for move_wait are set, you can make the game
 *  harder or easier.
 *
 *
 *  Author: CleverPet
 *
 *  Copyright 2018
 *  Licensed under the AGPL 3.0
 *
 */

#include <hackerpet.h>

using namespace std;

SYSTEM_THREAD(ENABLED);

// Use primary serial over USB interface for logging output (9600)
// Choose logging level here (ERROR, WARN, INFO)
SerialLogHandler logHandler(LOG_LEVEL_ERROR, { // Logging level for all messages
    { "app.hackerpet", LOG_LEVEL_ERROR }, // Logging level for library messages
    { "app", LOG_LEVEL_INFO } // Logging level for application messages
});

// initialize hub interface (from CleverpetLibrary)
HubInterface hub;

// Set this to the name of your player (dog, cat, etc.)
const char PLAYER_NAME[] = "Pet, Clever";

const char YELLOW = 60;
const char BLUE = 60;
const char FLASHING = 0;  // 0: no FLASHING
const char FLASHING_DUTY_CYCLE = 99;  // ignored since no FLASHING
const int FOODTREAT_DURATION = 5000;  // (ms)
const unsigned long TIMEOUT_MS = 60000;

// Two settings below adjust the speed of the game
const int MIN_LIGHT_ON_MS = 1000;
const int MAX_LIGHT_ON_MS = 1200;

static char target;
static bool accurate;
static char pressed;
static bool timeout;
static char foodtreat_state;
String result_str;

// anything that deals with millis need to be able to be big enough, hence we
// need the data type that can express the biggest integers (unsigned long)
static unsigned long time_start_wait;
static unsigned long move_wait;
static unsigned long playstart;
static unsigned long timer_ms;
static unsigned long light_timer_ms;

const String PAD_NAMES[3] = {"LEFT", "MIDDLE", "RIGHT"};

// loop is at the bottom of the file after OneTwoThreeButtonGame has been introduced into the name space

bool WhackAMoleGame()
{
    yield_begin();

    timeout = false;
    pressed = 0;
    result_str = String("");

    // before starting interaction, wait until:
    //  1. device layer is ready (in a good state)
    //  2. foodmachine is "idle", meaning it is not spinning or dispensing and tray is retracted (see FOODMACHINE_... constants)
    //  3. no button is currently pressed
    yield_wait_for(hub.IsReady()
                    && hub.FoodmachineState() == hub.FOODMACHINE_IDLE
                    && not hub.AnyButtonPressed(),
                false);

    // DI reset occurs if, for example, DL detects that touchpads need re-calibration, like if hub was moved to a different surface
    hub.SetDIResetLock(true);  // prevent DI board from resetting during an interaction (would cause lights to go blank etc.)

    time_start_wait = millis();

    playstart = Time.now(); // seconds since Unix epoch (1 January 1970)
    timer_ms = millis();

    light_timer_ms = 0;

    do
    {
        if (millis() > time_start_wait + light_timer_ms)
        {
            // choose some target lights, and store which targets were randomly chosen
            target = hub.SetRandomButtonLights(1, YELLOW, BLUE, FLASHING, FLASHING_DUTY_CYCLE);
            hub.SetLights((target^0b00000111), 0, 0, 0);   // turn off all lights except the target
            light_timer_ms += random(MIN_LIGHT_ON_MS, MAX_LIGHT_ON_MS);
        }

        // detect any touchpads currently pressed
        pressed = hub.AnyButtonPressed();

        yield(false); // use yields statements any time the machine is pausing or waiting
    }
    while ((pressed != hub.BUTTON_LEFT // we want it to just be a single touchpad
                && pressed != hub.BUTTON_MIDDLE
                && pressed != hub.BUTTON_RIGHT)
            && millis()  < time_start_wait + TIMEOUT_MS
        );

    timer_ms = millis() - timer_ms;

    hub.SetLights(hub.LIGHT_BTNS, 0, 0, 0);   // turn off lights

    if (pressed == 0)
    {
        // if we are in this state and no button was pressed, it is a timeout
        timeout = true;
        accurate = false;
    }
    else
    {
        accurate = !((pressed & target) == 0); // will be zero if and only if the wrong touchpad was touched
    }

    // delay 600 ms in case a button sound still playing
    yield_sleep_ms(300, false);  // wait for audio to finish playing

    if (accurate)
    {
        hub.PlayAudio(hub.AUDIO_POSITIVE, 20);
        yield_sleep_ms(1200, false); // give the Hub a moment to finish playing the reward sound

        // if successful interaction, present foodtreat using PresentAndCheckFoodtreat state machine
        do {
            foodtreat_state=hub.PresentAndCheckFoodtreat(FOODTREAT_DURATION); // time pres (ms)
            yield(false);
        } while (foodtreat_state!=hub.PACT_RESPONSE_FOODTREAT_NOT_TAKEN &&
             foodtreat_state!=hub.PACT_RESPONSE_FOODTREAT_TAKEN);
    }
    else
    {
        if (!timeout) // don't play any audio if there's a timeout (no response -> no consequence)
        {
            // if unsuccessful interaction: play negative feedback sound at low volume
            hub.PlayAudio(hub.AUDIO_NEGATIVE, 5);
        }
        yield_sleep_ms(5000, false);
    }

    if (!timeout)
    {
        if (accurate) {
            result_str = PAD_NAMES[pressed >> 1];
        }
        else
        {
            result_str = String("MISS");
        }

         String extras = String::format(
            "{\"touched\":%d,\"target\":%d,\"min_target_wait\":%d,\"max_target_wait\":%d}",
             pressed >> 1, target >> 1, MIN_LIGHT_ON_MS, MAX_LIGHT_ON_MS);

         if (!timeout) {
             // Only post a record when there was an interaction
             hub.Report(
                 Time.format(playstart,
                             TIME_FORMAT_ISO8601_FULL),  // play_start_time
                 PLAYER_NAME,                            // player
                 0,  // no natural level here. Makes sense to just use 'extra'
                 result_str,  // result
                 timer_ms,    // reaction time
                 accurate,    // foodtreat_presented
                 accurate ? foodtreat_state == hub.PACT_RESPONSE_FOODTREAT_TAKEN
                          : 0,  // foodtreat_eaten
                 extras);
        }
     }


    hub.SetDIResetLock(false);  // allow DI board to reset if needed between interactions

    yield_finish();
    return true;
}

// to avoid quirks define setup() nearly last, and right before loop()
void setup()
{
  // Initializes the hub and passes the current filename as ID for reporting
  hub.Initialize(__FILE__);
}

// run-loop function required for Particle
void loop()
{
    // advance the device layer state machine, but with 20 millisecond max time spent per loop cycle
    hub.Run(20);
    WhackAMoleGame();
}
