/**
  Learning Double Sequences
  =========================

    This is the ninth challenge from the original CleverPet learning curriculum.

    This challenge engages your player with sequences of actions. For a
    successful interaction, your player must press the brightest touchpad
    accurately twice in a row.

    During this challenge, the Hub gives your player information that will help
    them plan their next move. The dim touchpad during the first sequence
    becomes the the player's next target. Clever players will catch onto this
    and use this “hint” to prepare their next press.

    **Challenge logic:** This challenge has only one level. The challenge has 3
    moves per interaction: 1 stimulator round and 2 moves that are part of a
    sequence. The stimulator round lights up all 3 touchpads and waits for the
    player to touch any of them in any combination.

    For the player's second move the touchpads light up 1 random target and also
    light up the next target at a lower intensity. The target, and only the
    target pad, needs to be touched for the interaction to continue to the next
    round in the sequence.

    When the player has had 40 successful games in the last 50 games, the player
    has reached the maximum level.

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
int currentLevel = 1;
const int MAX_LEVEL=           1;   // Maximum number of levels
const int HISTORY_LENGTH=      50;   // Number of previous interactions to look at for performance
const int ENOUGH_SUCCESSES=    40;   // if num successes >= ENOUGH_SUCCESSES level-up
const int SEQUENCE_LENGTH = 2;
const int TARGET_INTENSITY = 75;
const int NEXT_TARGET_INTENSITY = 10;
const int SLEW = 90;
const unsigned long FOODTREAT_DURATION = 6000; // (ms) how long to present foodtreat
const unsigned long TIMEOUT_MS = 300002; // (ms) how long to wait until restarting the interaction
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

/// return the number of successful interactions in performance history for current level
unsigned int countSuccesses(){
    unsigned int total = 0;
    for (unsigned char i = 0; i <= perfDepth-1 ; i++)
        if(performance[i]==1)
            total++;
    return total;
}

/// return the number of misses in performance history for current level
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

/// add an interaction result to the performance history
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
    Log.info("New successful interactions: %u, misses: %u", countSuccesses(), countMisses());

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
/// letter. Requested bitfield should have only one bit set. Touched pad bitfield
/// can have multiple bits set. If correct, the correct pad will be returned, if
/// wrong only the wrong pad will be returned, if multiple wrong pads pressed,
/// only one wrong pad will be returned in order L -> M -> R
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

/// The actual LearningDoubleSequences challenge. This function needs to be called in a loop.
bool playLearningDoubleSequences(){
    yield_begin();

    static unsigned long gameStartTime, time_start_wait, timestamp_before, activityDuration = 0;
    static unsigned char foodtreatState = 99;
    static unsigned char touchpads[3]={hub.BUTTON_LEFT,
                                     hub.BUTTON_MIDDLE,
                                     hub.BUTTON_RIGHT};
    static unsigned char sequence_pos = 0;
    static unsigned char touchpad_sequence[SEQUENCE_LENGTH]={};
    static unsigned char pressed[SEQUENCE_LENGTH+1] = {};
    static bool accurate = false;
    static bool timeout = false;
    static bool foodtreatWasEaten = false; // store if foodtreat was eaten in last interaction

  // Static variables and constants are only initialized once, and need to be re-initialized
  // on subsequent calls
    gameStartTime=0;
    time_start_wait=0;
    timestamp_before =0;
    activityDuration = 0;
    foodtreatState = 99;
    sequence_pos = 0;
    touchpads[0]=hub.BUTTON_LEFT;
    touchpads[1]=hub.BUTTON_MIDDLE;
    touchpads[2]=hub.BUTTON_RIGHT;
    fill(touchpad_sequence, touchpad_sequence+SEQUENCE_LENGTH, 0);
    fill(pressed, pressed+SEQUENCE_LENGTH+1, 0);
    accurate = false;
    timeout = false;
    foodtreatWasEaten = false; // store if foodtreat was eaten in last interaction

    Log.info("-------------------------------------------");
    // Log.info("Starting new \"Learning Double Sequences\" challenge");
    // Log.info("Current level: %u, successes: %u, number of misses: %u",
        // currentLevel, countSuccesses(), countMisses());

    gameStartTime = Time.now();

    // before starting interaction, wait until:
    //  1. device layer is ready (in a good state)
    //  2. foodmachine is "idle", meaning it is not spinning or dispensing
    //      and tray is retracted (see FOODMACHINE_... constants)
    //  3. no touchpad is currently pressed
    yield_wait_for((hub.IsReady()
                    && hub.FoodmachineState() == hub.FOODMACHINE_IDLE
                    && not hub.AnyButtonPressed()), false);

    // DI reset occurs if, for example, device  layer detects that touchpads need re-calibration
    hub.SetDIResetLock(true);

    // fill touchpad_sequence
    for (int i = 0; i < SEQUENCE_LENGTH; ++i)
    {
        random_shuffle(&touchpads[0], &touchpads[3]);
        touchpad_sequence[i] = touchpads[0];
    }

    // turn on all touchpads
    hub.SetLights(hub.LIGHT_BTNS,TARGET_INTENSITY,TARGET_INTENSITY,SLEW);

    // progress to next state
    time_start_wait = millis();

    do
    {
        // detect any touchpads currently pressed
        pressed[0] = hub.AnyButtonPressed();
        yield(false); // use yields statements any time the hub is pausing or waiting
    }
    while (!(pressed[0] != 0) //0 if any touchpad is touched
            && millis()  < time_start_wait + TIMEOUT_MS); //0 if timed out

    hub.SetLights(hub.LIGHT_BTNS, 0, 0, 0); // turn off all touchpad lights

    // wait until: no touchpad is currently pressed
    yield_wait_for((!hub.AnyButtonPressed()), false);

    if (pressed[0] != 0)
    {
        Log.info("Stimulator touchpad touched, starting interactions");
        sequence_pos = 0;
        timeout = false;
        accurate = true;
    } else {
        Log.info("No touchpad pressed, timeout");
        sequence_pos = SEQUENCE_LENGTH;
        accurate = false;
        timeout = true;
    }

    // Record start timestamp for performance logging
    timestamp_before = millis();

    // Start main interactions loop
    while (sequence_pos < SEQUENCE_LENGTH) {
        Log.info("Interaction %d. Target touchpad: %c%c%c", (sequence_pos+1),
            (touchpad_sequence[sequence_pos]&hub.BUTTON_LEFT)?'1':'0',
            (touchpad_sequence[sequence_pos]&hub.BUTTON_MIDDLE)?'1':'0',
            (touchpad_sequence[sequence_pos]&hub.BUTTON_RIGHT)?'1':'0');

        // wait until: no touchpad is currently pressed
        yield_wait_for((!hub.AnyButtonPressed()), false);
        // TODO fix slobber

        // set next touchpad and this touchpad
        if (sequence_pos < SEQUENCE_LENGTH-1)
          hub.SetLights(touchpad_sequence[sequence_pos + 1],
                        NEXT_TARGET_INTENSITY, NEXT_TARGET_INTENSITY, SLEW);
        hub.SetLights(touchpad_sequence[sequence_pos],TARGET_INTENSITY,TARGET_INTENSITY,SLEW);

        // progress to next state
        time_start_wait = millis();

        do
        {
            // detect any touchpads currently pressed
            pressed[sequence_pos+1] = hub.AnyButtonPressed();
            yield(false); // use yields statements any time the hub is pausing or waiting
        }
        while (!(pressed[sequence_pos+1] != 0) //0 if any touchpad is touched
                && millis()  < time_start_wait + TIMEOUT_MS); //0 if timed out


        if (pressed[sequence_pos+1] == 0) {
            Log.info("No touchpad pressed, timeout");
            timeout = true;
            accurate = false;
            sequence_pos = SEQUENCE_LENGTH;
        } else if (pressed[sequence_pos+1] == touchpad_sequence[sequence_pos]) {
             Log.info("Correct touchpad pressed");
            // turn off lights on last touchpad
            hub.SetLights(touchpad_sequence[sequence_pos],0,0,0);
            sequence_pos++;
            accurate = true;
            timeout = false;
        } else { // asuming wrong
            Log.info("Wrong touchpad pressed");
            accurate = false;
            timeout = false;
            sequence_pos = SEQUENCE_LENGTH;
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
            Log.info("Foodtreat was eaten");
            foodtreatWasEaten = true;
        } else {
            Log.info("Foodtreat was not eaten");
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

    // keep track of performance
    if (!timeout){
        addResultToPerformanceHistory(accurate);
    }

    // Check if we're ready for next challenge
    if (currentLevel == MAX_LEVEL){
        if (countSuccesses() >= ENOUGH_SUCCESSES){
            Log.info("At MAX level! %u", currentLevel);
            //TODO At MAX level CHALLENGE DONE
            resetPerformanceHistory();
        }
    }

    // Send report
    // if(!timeout){
        Log.info("Sending report");

        String extra ="{";

        extra += "\"targetSeq\":\"";

        for (int i = 0; i < SEQUENCE_LENGTH; ++i){
            extra += convertBitfieldToLetter(touchpad_sequence[i]);
        }

        extra += "\",\"pressedSeq\":\"";

        for (int i = 0; i < SEQUENCE_LENGTH; ++i){
            extra += convertBitfieldToSingleLetter(touchpad_sequence[i],pressed[i+1]);
        }

        extra += "\"}";

        // Log.info(extra);

        hub.Report(Time.format(gameStartTime,
                               TIME_FORMAT_ISO8601_FULL),  // play_start_time
                   PlayerName,                             // player
                   currentLevel,                           // level
                   String(accurate),                       // result
                   activityDuration,                       // duration
                   accurate,           // foodtreat_presented
                   foodtreatWasEaten,  // foodtreatWasEaten
                   extra               // extra field
        );
        // }

        // printPerformanceArray();

        if (!accurate) {
          // between interaction wait time if a miss
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

    // Play 1 interaction of the Learning Double Sequences challenge
    // Will return true if level is done
    gameIsComplete = playLearningDoubleSequences();

    if(gameIsComplete){
        // Interaction end
        return;
    }

}
