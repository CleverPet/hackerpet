/*
 *  OneTwoThreeButtons
 *  ==================
 *
 *  This is game that can be useful during initial training of a dog or a cat
 *  to use the Hub. Given the animal onboarding ordering of:
 *
 *  1 Getting comfortable
 *  2 Getting curious
 *  3 Touching the touchpads
 *  4 Seeing the lights
 *
 *  .. this challenge is most useful for phases 2-4. In particular, setting all
 *  three touchpads to "on" during phases 2 and 3 can be useful, and setting 2
 *  of 3 touchpads to on can be useful for phases 3 and 4.
 *
 *  Author: CleverPet
 *
 *  Copyright 2018
 *  Licensed under the AGPL 3.0
 *
 */

#include <hackerpet.h>

// Set this to the name of your player (dog, cat, etc.)
const char PlayerName[] = "Pet, Clever";

using namespace std;

SYSTEM_THREAD(ENABLED);

// Use primary serial over USB interface for logging output (9600)
// Choose logging level here (ERROR, WARN, INFO)
SerialLogHandler logHandler(LOG_LEVEL_ERROR, { // Logging level for all messages
    { "app.hackerpet", LOG_LEVEL_ERROR }, // Logging level for library messages
    { "app", LOG_LEVEL_INFO } // Logging level for application messages
});

// access to hub functionality (lights, foodtreats, etc.)
HubInterface hub;

const int NUM_PADS = 1; // Choose number of lit-up pads at a time (1, 2 or 3)
const long FOODTREAT_DURATION = 5000; // (ms) how long to present foodtreat
const long SOUND_FOODTREAT_DELAY = 1200; // (ms) delay between reward sound and foodtreat
const char YELLOW = 60;
const char BLUE = 60;
const char FLASHING = 0;  // 0: no FLASHING
const char FLASHING_DUTY_CYCLE = 99;  // ignored since no FLASHING
const unsigned long TIMEOUT_MS = 60000; // (ms) how long to wait until restarting the interaction

static bool accurate;
static bool timeout;
static char foodtreat_state;
static unsigned char target;
static unsigned char pressed;
static unsigned long time_start_wait;
static unsigned long playstart;
static unsigned long timer_ms;

const unsigned char BUTTONS[] = {hub.LIGHT_LEFT, hub.LIGHT_MIDDLE, hub.LIGHT_RIGHT};

bool OneTwoThreeButtonGame(int num_pads)
{
    yield_begin();

    pressed = 0;     // static variables need to be re-initialized if we're using
    timeout = false; // yield macros to simplify our code structure


    // before starting interaction, wait until:
    //  1. device layer is ready (in a good state)
    //  2. foodmachine is "idle", meaning it is not spinning or dispensing and tray is retracted (see FOODMACHINE_... constants)
    //  3. no touchpad is currently pressed
    yield_wait_for(hub.IsReady()
                    && hub.FoodmachineState() == hub.FOODMACHINE_IDLE
                    && not hub.AnyButtonPressed(),
                false);


    // DI reset occurs if, for example, DL detects that touchpads need re-calibration, like if hub was moved to a different surface
    hub.SetDIResetLock(true);  // prevent DI board from resetting during an interaction (would cause lights to go blank etc.)

    // choose some target lights, and store which targets were randomly chosen
    target = hub.SetRandomButtonLights(num_pads, YELLOW, BLUE, FLASHING, FLASHING_DUTY_CYCLE);

    playstart = Time.now(); // seconds since Unix epoch (1 January 1970)
    timer_ms = millis();

    // progress to next state
    time_start_wait = millis();

    do
    {
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
        // if we are in this state and no touchpad was pressed, it is a timeout
        timeout = true;
        accurate = false;
    }
    else
    {
        accurate = !((pressed & target) == 0); // will be zero if and only if the wrong touchpad was touched
    }

    // delay 600 ms in case a touchpad sound still playing
    yield_sleep_ms(600, false);  // wait for audio to finish playing

    if (accurate)
    {
        hub.PlayAudio(hub.AUDIO_POSITIVE, 20);
        yield_sleep_ms(SOUND_FOODTREAT_DELAY, false); // give the Hub a moment to finish playing the reward sound

        // if successful interaction, present  using PresentAndCheckFoodtreat state machine
        do {
            foodtreat_state=hub.PresentAndCheckFoodtreat(FOODTREAT_DURATION); //time pres (ms)
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
    }

    String extras = String::format( "{\"targets\":\"%c%c%c\",\"pressed\":\"%c%c%c\"}",
                (target & 0b001 ? '1' : '0'),
                (target & 0b010 ? '1' : '0'),
                (target & 0b100 ? '1' : '0'),
                (pressed & 0b001 ? '1' : '0'),
                (pressed & 0b010 ? '1' : '0'),
                (pressed & 0b100 ? '1' : '0'));

    if (!timeout)
    {
        // Only record when there was an interaction
        hub.Report( Time.format(playstart, TIME_FORMAT_ISO8601_FULL),    // play_start_time
                    PlayerName,                                         // player
                    4-num_pads,     // difficulty increases with level. So level 1 is 3 pads, level 2 is 2 pads, level 3 is 1 pad
                    String(accurate), // (c string) result
                    timer_ms, // (ms) duration
                    accurate,   // since we're presenting foodtreats 100% of the time
                    accurate ? foodtreat_state == hub.PACT_RESPONSE_FOODTREAT_TAKEN : 0,// foodtreat_eaten
                    extras
                );
    }
    hub.SetDIResetLock(false);  // allow DI board to reset if needed between interactions

    yield_finish();
    return true;
}

void setup()
{
  // Initializes the hub and passes the current filename as ID for reporting
  hub.Initialize(__FILE__);
}

// Particle run-loop
void loop()
{
    // advance the device layer state machine, but with 20 millisecond max time spent per loop cycle
    hub.Run(20);

    OneTwoThreeButtonGame(NUM_PADS);
}
