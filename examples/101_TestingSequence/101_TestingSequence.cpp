/*
 *  TestingSequence
 *  ===============
 *
 *  This is not a game. This is instead a demonstration of various things the
 *  Hubs can do, and how to do them.
 *
 *  Note that while this is running it may be difficult to flash the Hub.
 *
 *  Author: CleverPet
 *
 *  Copyright 2018
 *  Licensed under the AGPL 3.0
 *
 */

#include <hackerpet.h>

// This will demonstrate and test parts of the CleverPet device layer
// Note that the easiest way to stop this code is by power-cycling the Hub
// after you've flashed new code on to it.

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

bool testing_sequence(HubInterface &hub)
{
    yield_begin();

    Log.info("Starting testing sequence!");
    Particle.publish("hackerpet/testing_sequence","setting_all_lights_off", PRIVATE);


    bool ready = hub.IsReady();
    Log.info("Ready: %u", ready);
    yield_sleep_ms(1000, false);

    if (ready)
    {
        Log.info("setting all lights off");
        Particle.publish("hackerpet/testing_sequence","setting_all_lights_off", PRIVATE);
        // SetLights(unsigned char whichLights, unsigned char yellow, unsigned char blue, unsigned char slew)
        //  whichLights: see LIGHT_... constants in CleverpetLibrary.h
        //  other parameters: can be [0, 99] each
        hub.SetLights(hub.LIGHT_ALL, 0, 0, 99); // all off
        yield_sleep_ms(1000, false);  // delays added to make it easy to see what the hub is doing for each operation in the testing script

        Log.info("setting cue light on");
        Particle.publish("hackerpet/testing_sequence","setting_cue_light_on", PRIVATE);
        // SetLights(unsigned char whichLights, unsigned char yellow, unsigned char blue, unsigned char slew)
        // whichLights: see LIGHT_... constants in CleverpetLibrary.h
        // other parameters: can be [0, 99] each
        hub.SetLights(hub.LIGHT_CUE, 30, 60, 10)  ;
        yield_sleep_ms(1000, false);
        hub.SetLights(hub.LIGHT_ALL, 0, 0, 20); // all off
        yield_sleep_ms(1000, false);

        Log.info("setting right touchpad light on");
        Particle.publish("hackerpet/testing_sequence","setting_right_touchpad_light_on", PRIVATE);
        // SetLightsRGB(unsigned char whichLights, unsigned char red, unsigned char green, unsigned char blue, unsigned char slew)
        // slew & colors: can be [0, 99] each
        hub.SetLightsRGB(hub.LIGHT_RIGHT, 99, 5, 20, 10);
        yield_sleep_ms(1000, false);
        hub.SetLights(hub.LIGHT_ALL, 0, 0, 20); // all off
        yield_sleep_ms(1000, false);

        Log.info("setting left touchpad light FLASHING");
        Particle.publish("hackerpet/testing_sequence","setting_left_touchpad_light_FLASHING", PRIVATE);
        // SetLights(unsigned char whichLights, unsigned char yellow, unsigned char blue, unsigned char period, unsigned char on)
        // period in 10 ms increments, 0=no flash
        // on is duty cycle, 10 ms increments, must be < period
        // colors: can be [0, 99] each
        hub.SetLights(hub.LIGHT_LEFT, 70, 5, 20, 10);
        yield_sleep_ms(1000, false);
        hub.SetLights(hub.LIGHT_ALL, 0, 0, 20); // all off
        yield_sleep_ms(1000, false);

        Log.info("setting middle touchpad light FLASHING");
        Particle.publish("hackerpet/testing_sequence","setting_touchpad_light_FLASHING", PRIVATE);
        // SetLightsRGB(unsigned char whichLights, unsigned char red, unsigned char green, unsigned char blue, unsigned char period, unsigned char on)
        // period in 10 ms increments, 0=no flash
        // on is duty cycle, 10 ms increments, must be < period
        // colors: can be [0, 99] each
        hub.SetLightsRGB(hub.LIGHT_MIDDLE, 5, 5, 99, 40, 30)  ;
        yield_sleep_ms(1000, false);
        hub.SetLights(hub.LIGHT_ALL, 0, 0, 20); // all off
        yield_sleep_ms(1000, false);

        Log.info("setting random interaction lights FLASHING");
        Particle.publish("hackerpet/testing_sequence","setting_random_interaction_lights_FLASHING", PRIVATE);
        // unsigned char SetRandomButtonLights(unsigned char numLights, unsigned char yellow, unsigned char blue, unsigned char period, unsigned char on)
        // period in 10 ms increments, 0=no flash
        // on is duty cycle, 10 ms increments, must be < period
        // colors: can be [0, 99] each
        static unsigned char rtn = hub.SetRandomButtonLights(2, 80, 40, 90, 45);
        Log.info("randomly selected target: %u", rtn);
        Particle.publish("hackerpet/testing_sequence","randomly_selected_target " + String(rtn), PRIVATE);
        yield_sleep_ms(2000, false);
        hub.SetLights(hub.LIGHT_ALL, 0, 0, 20); // all off
        yield_sleep_ms(1000, false);

        Log.info("playing positive audio sound");
        Particle.publish("hackerpet/testing_sequence","positive_audio_sound", PRIVATE);
        // PlayAudio(unsigned char whichAudio, unsigned char volume)
        // whichAudio: see AUDIO_... constants in CleverpetLibrary.h
        // volume: [0, 99]
        hub.PlayAudio(hub.AUDIO_POSITIVE, 10);
        yield_sleep_ms(1000, false);

        Log.info("playing a tone");
        Particle.publish("hackerpet/testing_sequence","playing_a_tone", PRIVATE);
        // PlayTone(unsigned int frequency, unsigned char volume, unsigned char slew)
        // frequency: in Hz
        // volume: [0, 99]
        // slew: [0, 99]
        hub.PlayTone(2000, 5, 10);
        yield_sleep_ms(1000, false);
        hub.PlayTone(0, 5, 10); // frequency == 0: turn off tone
        yield_sleep_ms(2000, false);

        Log.info("left touchpad value (after 2 sec):");
        yield_sleep_ms(2000, false);
        // int GetButtonVal(unsigned char whichButton)
        // whichButton: see BUTTON_... constants
        // returns analog touchpad reading
        //Log.info(hub.GetButtonVal(hub.BUTTON_LEFT));
        static unsigned char btn_val = hub.GetButtonVal(hub.BUTTON_LEFT);
        Log.info("%u", btn_val);
        Particle.publish("hackerpet/testing_sequence","left_button_value_after_2_s " + String(btn_val), PRIVATE);

        Log.info("any touchpad pressed (after 2 sec):");
        yield_sleep_ms(2000, false);
        // unsigned char AnyButtonPressed()
        // returns byte representing bitwise OR of any pressed touchpads
        btn_val = hub.AnyButtonPressed();

        Log.info("%u", btn_val);
        Particle.publish("hackerpet/testing_sequence","any_button_pressed_after_2_s " + String(btn_val), PRIVATE);


        Log.info("right touchpad: pressed? (after 2 sec)");
        // bool IsButtonPressed(unsigned char whichButton)
        // whichButton: see BUTTON_... constants
        // returns bool whether specified touchpad is pressed
        btn_val = hub.IsButtonPressed(hub.BUTTON_RIGHT);
        Log.info("%u", btn_val);
        Particle.publish("hackerpet/testing_sequence","is_right_button_pressed_after_2_s " + String(btn_val), PRIVATE);


        Log.info("any touchpad supra threshold in window? (after 2 sec)");
        static unsigned long since = millis();
        // unsigned char AnyButtonSupraThresholdInWindow(unsigned long since)
        btn_val = hub.AnyButtonSupraThresholdInWindow(since);
        Log.info("%u", btn_val);
        Particle.publish("hackerpet/testing_sequence","is_any_button_supra_threshold_in_window_after_2_s " + String(btn_val), PRIVATE);

        Log.info("right touchpad supra threshold in window? (after 2 sec)");
        since = millis();
        yield_sleep_ms(2000, false);
        // bool WasButtonSupraThresholdInWindow(unsigned char whichButton, unsigned long since)
        btn_val = hub.WasButtonSupraThresholdInWindow(hub.BUTTON_RIGHT, since);
        Log.info("%u", btn_val);
        Particle.publish("hackerpet/testing_sequence","is_right_button_supra_threshold_in_window_after_2_s " + String(btn_val), PRIVATE);

        Log.info("dome open? (after 2 sec)");
        yield_sleep_ms(2000, false);
        // int GetDomeOpen()
        // returns: -1=dunno 0=closed 1=open
        rtn = hub.GetDomeOpen();
        Log.info("%u", rtn);
        Particle.publish("hackerpet/testing_sequence","is_dome_open_after_2_s " + String(rtn), PRIVATE);


        Log.info("dome removed? (after 2 sec)");
        yield_sleep_ms(2000, false);
        // bool IsDomeRemoved()
        // returns true/false if dome has been removed from hub
        rtn = hub.IsDomeRemoved();
        Log.info("%u", rtn);
        Particle.publish("hackerpet/testing_sequence","is_dome_removed_after_2_s " + String(rtn), PRIVATE);
        yield_sleep_ms(2000, false);

        // There are 3 ways of presenting a foodtreat.

        // Method 1: PresentFoodtreat with decisec parameter
        Log.info("Presenting foodtreat for 1 second...");
        Particle.publish("hackerpet/testing_sequence","present_treat_for_1_s", PRIVATE);
        // bool PresentFoodtreat(unsigned char duration_decisec)
        hub.PresentFoodtreat(10);
        Log.info("Waiting...");  // wait for tray to retract
        yield_sleep_ms(6000, false);

        // Method 2: PresentFoodtreat with decisec parameter = 0 (indefinitely), then manual RetractTray
        Log.info("Presenting foodtreat indefinitely, then retracting...");
        Particle.publish("hackerpet/testing_sequence","present_treat_then_retract_treat");

        hub.PresentFoodtreat(0);  // present indefinitely
        yield_sleep_ms(1500, false);
        hub.RetractTray();  // retract tray
        Log.info("Waiting...");
        yield_sleep_ms(6000, false);

        // Method 3: (Preferred) use the PresentAndCheckFoodtreat state machine in a loop.
        // Advantages: some error checking, returns if foodtreat was eaten or not.
        Log.info("Using PresentAndCheckFoodtreat state machine, with 1 second...");
        Particle.publish("hackerpet/testing_sequence","PresentAndCheckFoodtreat_state_machine_with_1_second", PRIVATE);
        static unsigned char pact_state = hub.PACT_BEFORE_PRESENT;  // hub.PACT_... are states of PresentAndCHeckFoodtreat state machine
        while (!(pact_state == hub.PACT_RESPONSE_FOODTREAT_TAKEN || pact_state == hub.PACT_RESPONSE_FOODTREAT_NOT_TAKEN))  // run until one of two possible final states
        {
            pact_state = hub.PresentAndCheckFoodtreat(1000);  // state machine
            yield(false);
        }

        Log.info("Food taken: %u", pact_state);
        Particle.publish("hackerpet/testing_sequence","foodtreat_taken", pact_state, PRIVATE);


        Log.info("Waiting...");  // leave some time for singulator to spin if needed
        yield_sleep_ms(6000, false);

        hub.Run(1000);
        Log.info("Is hub out of foodtreats?");
        rtn = hub.IsHubOutOfFood();  // true if there is low/no foodtreats in singulator
        Log.info(" %u", rtn);  // true if there is low/no foodtreats in singulator
        Particle.publish("hackerpet/testing_sequence","is_hub_out_of_foodtreats" + String(rtn), PRIVATE);


        hub.Run(1000);
        Log.info("Is there a singulator error?");
        rtn = hub.IsSingulatorError();
        Log.info(" %u", rtn);  // true if singulator is jammed
        Particle.publish("hackerpet/testing_sequence","is_there_a_singulator_error " + String(rtn), PRIVATE);
    }
    Log.info("Finished testing sequence!");
    Particle.publish("hackerpet/testing_sequence","finished_testing_sequence", PRIVATE);

    yield_finish();

    return true;
}

// setup function for particle
void setup()
{
  // Initializes the hub and passes the current filename as ID for reporting
  hub.Initialize(__FILE__);
}

// loop function for particle
void loop()
{
    // advance the device layer state machine, but with 20 millisecond max time spent per loop cycle
    hub.Run(20);
    testing_sequence(hub);
}
