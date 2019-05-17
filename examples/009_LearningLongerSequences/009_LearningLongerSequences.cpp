/**
  Learning Longer Sequences
  =========================

    This is the tenth challenge from the original CleverPet learning curriculum.

    The sequences get longer! Now, your player is challenged to complete
    patterns of up to nine moves in a row. Beginning with move sequences of
    length three, the sequences get progressively longer with the player's
    success, and shorter when the difficulty is too high.

    To master this challenge, players need to stay attentive and accurate over
    progressively longer periods of time. They are given a few "lives" so that
    one missed touchpad doesn’t always send them back to the beginning.

    **Challenge logic:** The sequence increases if 12 of the last 15
    interactions were successes, it also decreases if 12 of the last 15
    interactions were misses.

    The first interaction in the challenge is a "stimulator round" where all 3
    touchpads light up and any combination of pads can be touched to advance the
    interaction. The second interaction starts the sequence. At every step of
    the sequence the current target will light up, along with following target
    at lower intensity. The target must to be touched exactly, with no
    combinations or double touches allowed. On a wrong touchpad touch the player
    loses a "life" and is allowed to continue the interaction. If all 3 of the
    player's lives run out the interaction becomes a miss.

  Authors: CleverPet Inc.
           Jelmer Tiete <jelmer@tiete.be>

  Copyright 2019
  Licensed under the AGPL 3.0
*/

#include <hackerpet.h>
#include <algorithm>

// Set this to the name of your player (dog, cat, etc.)
const char PlayerName[] = "Pet, Clever";

/**
 * Challenge settings
 * -------------
 *
 * These constants (capitalized CamelCase) and variables (camelCase) define the
 * gameplay
 */
const int HISTORY_LENGTH=      15;   // Number of past interactions to look at for performance
const int ENOUGH_SUCCESSES=    12;   // if successes >= ENOUGH_SUCCESSES level-up
const int TOO_MANY_MISSES=     12;   // if num misses >= TOO_MANY_MISSES level-down
const int LIVES_START_STATE = 3; // total number of lives per interaction
int sequenceLength = 3; // start sequence length
const int SEQUENCE_LENGTHMax = 9;
const int TARGET_INTENSITY = 75;
const int NEXT_TARGET_INTENSITY = 10;
const int SLEW = 90;
const unsigned long FOODTREAT_DURATION = 6000; // (ms) how long to present foodtreat
const unsigned long TIMEOUT_STIMULUS_MS = 300000; // (ms) how long to wait until restarting the
                                                  // interaction
const unsigned long TIMEOUT_INTERACTIONS_MS = 5000; // (ms) how long to wait until restarting the
                                                    // interaction
const unsigned long INTER_GAME_DELAY = 10000;

/**
 * Global variables and constants
 * ------------------------------
 */
const unsigned long SOUND_FOODTREAT_DELAY = 1200; // (ms) delay for reward sound
const unsigned long SOUND_TOUCHPAD_DELAY = 300; // (ms) delay for touchpad sound

bool performance[HISTORY_LENGTH] = {0}; // store the progress in this challenge
unsigned char perfPos = 0; // to keep our position in the performance array
unsigned char perfDepth = 0; // to keep the size of the number of perf numbers to consider

// Use primary serial over USB interface for logging output (9600)
// Choose logging level here (ERROR, WARN, INFO)
SerialLogHandler logHandler(LOG_LEVEL_INFO, { // Logging level for all messages
    { "app.hackerpet", LOG_LEVEL_ERROR }, // Logging level for library messages
    { "app", LOG_LEVEL_INFO } // Logging level for application messages
});

// access to hub functionality (lights, foodtreats, etc.)
HubInterface hub;

// enables simultaneous execution of application and system thread
SYSTEM_THREAD(ENABLED);

/**
 * Helper functions
 * ----------------
 */

/// return the number of successes in performance history for current level
unsigned int countSuccesses(){
    unsigned int total = 0;
    for (unsigned char i = 0; i <= perfDepth-1 ; i++)
        if(performance[i]==1)
            total++;
    return total;
}

/// return the number of misss in performance history for current level
unsigned int countMisses(){
    unsigned int total = 0;
    for (unsigned char i = 0; i <= perfDepth-1 ; i++)
        if(performance[i]==0)
            total++;
    return total;
}

/// reset performance history to 0
void resetPerformanceHistory(){
    for (unsigned char i = 0; i < HISTORY_LENGTH ; i++)
        performance[i] = 0;
    perfPos = 0;
    perfDepth = 0;
}

/// add a interaction result to the performance history
void addResultToPerformanceHistory(bool entry){
        // Log.info("Adding %u", entry);
    performance[perfPos] = entry;
    perfPos++;
    if (perfDepth < HISTORY_LENGTH)
        perfDepth++;
    if (perfPos > (HISTORY_LENGTH - 1)){ // make our performance array circular
        perfPos = 0;
    }
    // Log.info("perfPos %u, perfDepth %u", perfPos, perfDepth);
    Log.info("New successes: %u, misss: %u", countSuccesses(),
             countMisses());

}

/// print the performance history for debugging
void printPerformanceArray(){
    Serial.printf("performance: {");
    for (unsigned char i = 0; i < perfDepth ; i++){
        Serial.printf("%u",performance[i]);
        if ((i+1) == perfPos)
            Serial.printf("|");
    }
    Serial.printf("}\n");
}
/// converts a bitfield of pressed touchpads to letters
/// multiple consecutive touches are possible and will be reported L -> M - > R
/// @returns String
String convertBitfieldToLetter(unsigned char pad){
  String letters = "";
  if (pad & hub.BUTTON_LEFT)
    letters += 'L';
  if (pad & hub.BUTTON_MIDDLE)
    letters += 'M';
  if (pad & hub.BUTTON_RIGHT)
    letters += 'R';
  return letters;
}

/// converts requested touchpad bitfield and pressed touchpad bitfield to a
/// letter. Requested bitfield should have only one bit set. Touched pad
/// bitfield can have multiple bits set. If correct, the correct pad will be
/// returned, if wrong only the wrong pad will be returned, if multiple wrong
/// pads pressed, only one wrong pad will be returned in order L -> M -> R
/// @returns String
String convertBitfieldToSingleLetter(unsigned char targetPad, unsigned char pad){
  if ((targetPad & (targetPad-1)) != 0) // error targetPad has multiple pads set
    return "X";

  String letter = "";
  if (targetPad == pad){ // did we get it right?
    letter += convertBitfieldToLetter(targetPad); // report right pad
  }
  else { // we have a wrong pad touched or multiple pads touched (of which one is wrong)
    unsigned char maskedPressed = ~targetPad & pad; // mask out the correct pad
    // check if multiple pads touched (except for correct one)
    if ((maskedPressed & (maskedPressed-1)) != 0)
    {
      // multiple wrong pads touched
      //just pick one to report, L -> M -> R
      if (maskedPressed & hub.BUTTON_LEFT)
        letter += 'L';
      else if (maskedPressed & hub.BUTTON_MIDDLE)
        letter += 'M';
      else if (maskedPressed & hub.BUTTON_RIGHT)
        letter += 'R';
    } else {
      //only one wrong pad touched
      letter += convertBitfieldToLetter(maskedPressed);
    }
  }
  return letter;
}

/// The actual LearningLongerSequences function. This function needs to be called in a loop.
bool playLearningLongerSequences(){
    yield_begin();

    static unsigned long timestamp_before, time_start_wait, gameStartTime, activityDuration = 0;
    static unsigned char foodtreatState = 99;
    static int lives = LIVES_START_STATE;
    static unsigned char touchpads[3]={hub.BUTTON_LEFT,
                                     hub.BUTTON_MIDDLE,
                                     hub.BUTTON_RIGHT};
    static unsigned char sequence_pos = 0;
    static unsigned char touchpad_sequence[SEQUENCE_LENGTHMax]={};
    static unsigned char pressed[SEQUENCE_LENGTHMax+1] = {};
    static bool accurate = false;
    static bool timeout = false;
    static bool foodtreatWasEaten = false; // store if foodtreat was eaten in last interaction

    // Static variable and constants are only initialized once, and need to be re-initialized
    // on subsequent calls
    timestamp_before = 0;
    time_start_wait = 0;
    gameStartTime = 0;
    activityDuration = 0;
    foodtreatState = 99;
    touchpads[0]=hub.BUTTON_LEFT;
    touchpads[1]=hub.BUTTON_MIDDLE;
    touchpads[2]=hub.BUTTON_RIGHT;
    sequence_pos = 0;
    // reset sequence, lives and pressed touchpads
    fill(touchpad_sequence, touchpad_sequence+SEQUENCE_LENGTHMax, 0);
    fill(pressed, pressed+SEQUENCE_LENGTHMax+1, 0);
    lives = LIVES_START_STATE;
    accurate = false;
    timeout = false;
    foodtreatWasEaten = false; // store if foodtreat was eaten in last interaction

    Log.info("-------------------------------------------");
    // Log.info("Starting new \"Learning Longer Sequences\" challenge");
    // Log.info("Current length: %u, successes: %u, num misses: %u",
        // sequenceLength, countSuccesses(), countMisses());

    // before starting interaction, wait until:
    //  1. device layer is ready (in a good state)
    //  2. foodmachine is "idle", meaning it is not spinning or dispensing foodtreat
    //      and tray is retracted (see FOODMACHINE_... constants)
    //  3. no button is currently pressed
    yield_wait_for((hub.IsReady()
                    && hub.FoodmachineState() == hub.FOODMACHINE_IDLE
                    && not hub.AnyButtonPressed()), false);

    // DI reset occurs if, for example, device  layer detects that touchpads need re-calibration
    hub.SetDIResetLock(true);

    gameStartTime = Time.now();

    // fill touchpad_sequence
    for (int i = 0; i < sequenceLength; ++i)
    {
        random_shuffle(&touchpads[0], &touchpads[3]);
        touchpad_sequence[i] = touchpads[0];
    }

    hub.SetLights(hub.LIGHT_BTNS,TARGET_INTENSITY,TARGET_INTENSITY,SLEW);

    // progress to next state
    time_start_wait = millis();

    do
    {
        // detect any buttons currently pressed
        pressed[0] = hub.AnyButtonPressed();
        // use yields statements any time the hub is pausing or waiting
        yield(false);
    }
    while (!(pressed[0] != 0) //0 if any touchpad is touched
            //0 if timed out
            && millis()  < time_start_wait + TIMEOUT_STIMULUS_MS);

    hub.SetLights(hub.LIGHT_BTNS, 0, 0, 0); // turn off all touchpad lights

    // wait until: no button is currently pressed
    yield_wait_for((!hub.AnyButtonPressed()), false);

    if (pressed[0] != 0)
    {
        Log.info("Stimulator touchpad touched, starting interactions");
        sequence_pos = 0;
        timeout = false;
        accurate = true;
    } else {
        Log.info("No touchpad pressed, timeout");
        sequence_pos = sequenceLength; // skip the interactions
        accurate = false;
        timeout = true;
    }

    // Record start timestamp for performance logging
    timestamp_before = millis();

    // Start main interactions loop
    while (sequence_pos < sequenceLength) {
        Log.info("Interaction %d. Target touchpad: %c%c%c", (sequence_pos+1),
            (touchpad_sequence[sequence_pos]&hub.BUTTON_LEFT)?'1':'0',
            (touchpad_sequence[sequence_pos]&hub.BUTTON_MIDDLE)?'1':'0',
            (touchpad_sequence[sequence_pos]&hub.BUTTON_RIGHT)?'1':'0');

        // wait until: no button is currently pressed
        yield_wait_for((!hub.AnyButtonPressed()), false);
        // TODO fix slobber

        // set next touchpad and this touchpad
        if (sequence_pos < sequenceLength-1)
          hub.SetLights(touchpad_sequence[sequence_pos + 1],
                        NEXT_TARGET_INTENSITY, NEXT_TARGET_INTENSITY, SLEW);
        hub.SetLights(touchpad_sequence[sequence_pos],TARGET_INTENSITY,TARGET_INTENSITY,SLEW);

        // progress to next state
        time_start_wait = millis();
        do
        {
            // detect any buttons currently pressed
            // sequence_pos = 0 already stores stimulus button press
            pressed[sequence_pos+1] = hub.AnyButtonPressed();
            yield(false); // use yields statements any time the hub is pausing or waiting
        }
        while (!(pressed[sequence_pos+1] != 0) //0 if any touchpad is touched
                && millis()  < time_start_wait + TIMEOUT_INTERACTIONS_MS); //0 if timed out

        if (pressed[sequence_pos+1] == 0) {
            Log.info("No touchpad pressed, timeout");
            timeout = true;
            accurate = false;
            sequence_pos = sequenceLength;
        } else if (pressed[sequence_pos+1] == touchpad_sequence[sequence_pos]) {
             Log.info("Correct touchpad pressed");
            // turn off lights on last touchpad
            hub.SetLights(touchpad_sequence[sequence_pos],0,0,0);
            sequence_pos++;
            accurate = true;
            timeout = false;
        } else { // asuming wrong
            Log.info("Wrong button pressed");
            timeout = false;
            lives--;
            if (lives == 0)
            {
                Log.info("Lives depleated");
                accurate = false;
                sequence_pos = sequenceLength;
            } else {
                Log.info("Deducted life. Lives left: %d. Re-interaction",lives);
                // lost life but not done
                // give the Hub a moment to finish playing the touchpad sound
                yield_sleep_ms(SOUND_TOUCHPAD_DELAY, false);
                hub.PlayAudio(hub.AUDIO_NEGATIVE, 60);
                // give the Hub a moment to finish playing the sound
                yield_sleep_ms(SOUND_FOODTREAT_DELAY, false);
                //reset the touched pad
                pressed[sequence_pos+1] = 0;
            }
        }
    }

    // record time period for performance logging
    activityDuration = millis() - timestamp_before;

    hub.SetLights(hub.LIGHT_BTNS, 0, 0, 0); // turn off all touchpad lights

    if (accurate) {
        Log.info("All interactions passed, dispensing foodtreat");
        // give the Hub a moment to finish playing the touchpad sound
        yield_sleep_ms(SOUND_TOUCHPAD_DELAY, false);
        hub.PlayAudio(hub.AUDIO_POSITIVE, 60);
        // give the Hub a moment to finish playing the reward sound
        yield_sleep_ms(SOUND_FOODTREAT_DELAY, false);
        do {
            foodtreatState=hub.PresentAndCheckFoodtreat(FOODTREAT_DURATION);
            yield(false);
        } while (foodtreatState!=hub.PACT_RESPONSE_FOODTREAT_NOT_TAKEN &&
             foodtreatState!=hub.PACT_RESPONSE_FOODTREAT_TAKEN);

        // Check if foodtreat was eaten
        if (foodtreatState == hub.PACT_RESPONSE_FOODTREAT_TAKEN){
            Log.info("Treat was eaten");
            foodtreatWasEaten = true;
        } else {
            Log.info("Treat was not eaten");
            foodtreatWasEaten = false;
        }
    } else {
        if (!timeout) {
            // give the Hub a moment to finish playing the touchpad sound
            yield_sleep_ms(SOUND_TOUCHPAD_DELAY, false);
            hub.PlayAudio(hub.AUDIO_NEGATIVE, 60);
            // give the Hub a moment to finish playing the sound
            yield_sleep_ms(SOUND_FOODTREAT_DELAY, false);
            foodtreatWasEaten = false;
        }
    }

    if (sequenceLength == 9)
    {
        resetPerformanceHistory();
        Log.info("At MAX length! %u", sequenceLength);
        //TODO At MAX length CHALLENGE DONE
    }

    // keep track of performance
    if (!timeout){
        addResultToPerformanceHistory(accurate);
    }

    // adjust sequence length according to performance
    if (countSuccesses() >= ENOUGH_SUCCESSES) {
        if (sequenceLength < SEQUENCE_LENGTHMax)
        {
            Log.info("Increasing sequence length! %u", sequenceLength);
            sequenceLength++;
        }
        resetPerformanceHistory();
    } else if (countMisses() >= TOO_MANY_MISSES) {
        if (sequenceLength > 3)
        {
            Log.info("Decreasing sequence length! %u", sequenceLength);
            sequenceLength--;
        }
        resetPerformanceHistory();
    }

    // Send report
    // TODO this report might get too long for particle publish size limits
    if(!timeout){
        Log.info("Sending report");

        String extra ="{";

        extra += "\"targetSeq\":\"";

        for (int i = 0; i < sequenceLength; ++i){
            extra += convertBitfieldToLetter(touchpad_sequence[i]);
        }

        extra += "\",\"pressedSeq\":\"";
        // TODO also report wrong touches that deducted lives?
        for (int i = 0; i < sequenceLength; ++i){
            extra += convertBitfieldToSingleLetter(touchpad_sequence[i],pressed[i+1]);
        }

        extra += String::format("\",\"lives\":%d}",lives);

        // Log.info(extra);

        hub.Report(Time.format(gameStartTime,
                               TIME_FORMAT_ISO8601_FULL),  // play_start_time
                   PlayerName,                             // player
                   sequenceLength,                         // level
                   String(accurate),                       // result
                   activityDuration,                       // duration
                   accurate,           // foodtreat_presented
                   foodtreatWasEaten,  // foodtreatWasEaten
                   extra               // extra field
        );
    }

    // printPerformanceArray();

    if(!accurate){
        // between interaction time if wrong
        yield_sleep_ms(INTER_GAME_DELAY, false);
    }

    hub.SetDIResetLock(false);  // allow DI board to reset if needed between interactions
    yield_finish();
    return true;
}


/**
 * Setup function
 * --------------
 */
void setup() {
  // Initializes the hub and passes the current filename as ID for reporting
  hub.Initialize(__FILE__);
}

/**
 * Main loop function
 * ------------------
 */
void loop()
{
    bool gameIsComplete = false;

    // Advance the device layer state machine, but with 20 ms max time
    // spent per loop cycle.
    hub.Run(20);

    // Play 1 interaction of the Learning Longer Sequences challenge
    // Will return true if level is done
    gameIsComplete = playLearningLongerSequences();

    if(gameIsComplete){
        // Interaction end
        return;
    }
}
