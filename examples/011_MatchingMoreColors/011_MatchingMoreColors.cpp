/**
  Matching More Colors
  ====================

    This challenge is similar to Matching Two Colors, but now the pads can also
    turn white.

    By adding another color, the number of possible solutions grows
    exponentially! Adding a third color means there are over 24 initial
    combinations to solve, and many different paths to solving a given puzzle.

    **Challenge logic:** This interaction has 4 different levels with 4
    corresponding touches. There are 3 different colors: yellow, white and blue.

    At the start of the interaction a random combination of colors is chosen for
    the touchpads (starting state). Every iteration of the main while loop
    checks if the colors match and advances the color of the touched touchpad to
    the next one. If the player is about to run out of remaining touches, the
    'Do' sound is played with every touch as they are close to running out.

    When all touches are used, the interaction becomes a miss, if a match is
    found, the player gets presented with a foodtreat.

    If 4 successful games are played within the last 5 games the player will
    level up. On 3 missed games in the last 5 games, the player will level down.

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
const int STARTING_LEVEL = 1;
const int MAX_LEVEL         =  4;// Maximum number of levels
const int HISTORY_LENGTH    =  5;// Number of previous interactions to look
                                // at for performance
const int ENOUGH_SUCCESSES  =  4; // if num successes >= ENOUGH_SUCCESSES level-up
const int TOO_MANY_MISSES   =  3; // if number of misses >= TOO_MANY_MISSES level-down
const unsigned long FOODTREAT_DURATION = 6000; // (ms) how long to present foodtreat
const unsigned long TIMEOUT_MS = 300002; // (ms) how long to wait until restarting the interaction
const unsigned long WRONG_INTERACTION_DELAY = 6000;
const unsigned char TOUCHPADS[3][3][2] = { //[pads][colors][Y,B] //TODO make this easier
        {{90, 00},{00, 90},{90, 90}}, // Y, B, W
        {{90, 00},{00, 90},{90, 90}},
        {{90, 00},{00, 90},{90, 90}},
        };
const unsigned char REPORT_COLORS[3] = {'Y','B','W'};

/**
 * Global variables and constants
 * ------------------------------
 */
const unsigned long SOUND_DO_DELAY = 600; // (ms) delay between reward sound and
                                        // foodtreat
const unsigned long SOUND_FOODTREAT_DELAY = 600; // (ms) delay for reward sound
const unsigned long SOUND_TOUCHPAD_DELAY = 300; // (ms) delay for touchpad sound
// level 1 is 2 colors, so 3 presses
// level 2 and 3 is 2 or 3 colors so 35 and 10 presses
// level 4 is 3 colors with 6 presses
// make sure you understand the challenge logic before changing
const unsigned char PADS_PRESSED_MAX[MAX_LEVEL] = {3,35,10,6};

bool performance[HISTORY_LENGTH] = {0}; // store the progress in this challenge
unsigned char perfPos = 0; // to keep our position in the performance array
unsigned char perfDepth = 0;  // to keep the size of the number of perf numbers
                              // to consider
int numberOfColors = 2; // this is chosen at random between 2 or 3
unsigned char touchpadsColor[3] = {};


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
    Log.info("New successes: %u, misses: %u", countSuccesses(), countMisses());

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

/// advance a touchpad to the next color
void advanceTouchpad(unsigned char pad){
    touchpadsColor[pad]++;
    if (touchpadsColor[pad]>numberOfColors-1)
        touchpadsColor[pad]=0;
}

/// update the touchpad lights on the hub
void updateTouchpadLights() {
  hub.SetLights(hub.LIGHT_LEFT, TOUCHPADS[0][touchpadsColor[0]][0],
                TOUCHPADS[0][touchpadsColor[0]][1], 0);
  hub.SetLights(hub.LIGHT_MIDDLE, TOUCHPADS[1][touchpadsColor[1]][0],
                TOUCHPADS[1][touchpadsColor[1]][1], 0);
  hub.SetLights(hub.LIGHT_RIGHT, TOUCHPADS[2][touchpadsColor[2]][0],
                TOUCHPADS[2][touchpadsColor[2]][1], 0);
};

/// check if all touchpad colors match
bool checkMatch(){
    if ((touchpadsColor[0]==touchpadsColor[1]) && (touchpadsColor[1]==touchpadsColor[2]))
        return true;
    return false;
}

/// converts a bitfield of pressed touchpads to letters
/// multiple consecutive touches will be reported as X
/// @returns String
String convertBitfieldToLetter(unsigned char pad){
  if ((pad & (pad-1)) != 0) // targetPad has multiple pads set
    return "X";

  String letters = "";
  if (pad & hub.BUTTON_LEFT)
    letters += 'L';
  if (pad & hub.BUTTON_MIDDLE)
    letters += 'M';
  if (pad & hub.BUTTON_RIGHT)
    letters += 'R';
  return letters;
}

/// The actual MatchingMoreColors challenge. This function needs to be called in a loop.
bool playMatchingMoreColors(){
    yield_begin();

    static unsigned long timestamp_before, gameStartTime, time_start_wait, activityDuration = 0;
    static unsigned char foodtreatState = 99;
    static unsigned char touchpadsColorStart[3] = {};
    static unsigned char pressed = 0;
    static int currentLevel = STARTING_LEVEL; // starting level
    static int PADS_PRESSED_MAX_override = 0;
    static int padsPressed = 0;
    static bool match = false;
    static bool retryGame = false; // should not be re-initialized
    static bool accurate = false;
    static bool timeout = false;
    static bool foodtreatWasEaten = false; // store if foodtreat was eaten in last interaction
    static String pressedSeq = "";

    // Static variables and constants are only initialized once, and need to be re-initialized
    // on subsequent calls
    timestamp_before = 0;
    gameStartTime = 0;
    time_start_wait = 0;
    activityDuration = 0;
    foodtreatState = 99;
    PADS_PRESSED_MAX_override = 0;
    padsPressed = 0;
    pressed = 0;
    match = false;
    retryGame = false; // should not be re-initialized
    accurate = false;
    timeout = false;
    foodtreatWasEaten = false; // store if foodtreat was eaten in last interaction
    fill(touchpadsColorStart, touchpadsColorStart+3, 0);
    pressedSeq = "";

    Log.info("-------------------------------------------");
    // Log.info("Starting new \"Matching More Colors\" challenge");
    // Log.info("Current level: %u, successes: %u, number of misses: %u",
        // currentLevel, countSuccesses(), countMisses());

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

    gameStartTime = Time.now();

    // establish number of colors for interaction
    if(!retryGame){
        numberOfColors = random(2,4); // 2 or 3 colors
    }

    // overrides like the original interactions
    if(currentLevel == 1){
        numberOfColors = 2;
    } else if(currentLevel == 4){
        numberOfColors = 3;
    }

    // override for levels 2 and 3 with 2 colors
    if (numberOfColors == 2){
        PADS_PRESSED_MAX_override = 3;
    } else {
        PADS_PRESSED_MAX_override = PADS_PRESSED_MAX[currentLevel-1];
    }

    // Rndomly pick start state, except on retry interaction
    if (!retryGame){
        do{
            touchpadsColor[0] = random(0,numberOfColors);
            touchpadsColor[1] = random(0,numberOfColors);
            touchpadsColor[2] = random(0,numberOfColors);
        } while (checkMatch());
    } else {
        Log.info("Doing a retry interaction");
    }

    Log.info("Number of colors: %u", numberOfColors);

    // save start state for report
    copy(begin(touchpadsColor), end(touchpadsColor), begin(touchpadsColorStart));

    Log.info("Start state: %c%c%c", REPORT_COLORS[touchpadsColor[0]],
                                    REPORT_COLORS[touchpadsColor[1]],
                                    REPORT_COLORS[touchpadsColor[2]]);

    updateTouchpadLights();

    // Record start timestamp for performance logging
    timestamp_before = millis();

    // main while loop
    while (match == false){
        time_start_wait = millis();
        do
        {
            yield(false); // use yields statements any time the hub is pausing or waiting
            // detect any touchpads currently pressed
            pressed = hub.AnyButtonPressed();
        }
        while (!(pressed != 0) //0 if any touchpad is touched
                && millis()  < time_start_wait + TIMEOUT_MS); //0 if timed out

        activityDuration = millis() - timestamp_before;

        if (pressed == hub.BUTTON_LEFT){
            Log.info("Left touchpad pressed");
            advanceTouchpad(0);
        } else if (pressed == hub.BUTTON_MIDDLE){
            Log.info("Middle touchpad pressed");
            advanceTouchpad(1);
        } else if (pressed == hub.BUTTON_RIGHT){
            Log.info("Right touchpad pressed");
            advanceTouchpad(2);
        } else if (pressed == 0) {
            timeout = true;
            accurate = false;
        } else {
            // double pad press
        }

        // add our press to the reporting sequence
        pressedSeq += convertBitfieldToLetter(pressed);

        // update lights
        updateTouchpadLights();
        // increase pressed counter
        padsPressed++;
        Log.info("Remaining presses: %u", PADS_PRESSED_MAX_override-padsPressed);
        // check for timeout
        if (activityDuration > TIMEOUT_MS ){
            Log.info("Timeout");
            timeout = true;
            break;
        }
        // check for match
        if (checkMatch()){
            Log.info("We have a match");
            match = true;
            break;
        }
        // check for last tries
        if (padsPressed >= PADS_PRESSED_MAX_override-2){
            // Log.info("almost out");
            // give the Hub a moment to finish playing the touchpad sound
            yield_sleep_ms(SOUND_TOUCHPAD_DELAY, false);
            hub.PlayAudio(hub.AUDIO_DO, 60);
            // give the Hub a moment to finish playing the sound
            yield_sleep_ms(SOUND_DO_DELAY, false);
        }
        //check for max tries
        if (padsPressed == PADS_PRESSED_MAX_override){
            Log.info("Max presses");
            break;
        }

        // wait until: no touchpad is currently pressed
        yield_wait_for((!hub.AnyButtonPressed()), false);
    }

    // check if we have a match on all touchpads
    accurate = checkMatch();

    if (accurate){
        timeout = false; // rare case
        retryGame = false;
        Log.info("Match, dispensing foodtreat");
        // give the Hub a moment to finish playing the touchpad sound
        yield_sleep_ms(SOUND_TOUCHPAD_DELAY, false);
        hub.PlayAudio(hub.AUDIO_POSITIVE, 80);
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
        retryGame = true;
        if (!timeout){
            // give the Hub a moment to finish playing the touchpad sound
            yield_sleep_ms(SOUND_TOUCHPAD_DELAY, false);
            hub.PlayAudio(hub.AUDIO_NEGATIVE, 80);
            // give the Hub a moment to finish playing the reward sound
            yield_sleep_ms(SOUND_FOODTREAT_DELAY, false);

            yield_sleep_ms(WRONG_INTERACTION_DELAY, false);
        }
    }

    hub.SetLights(hub.LIGHT_BTNS, 0, 0, 0); // turn off all touchpad lights

    // keep track of performance
    if (!timeout){
        addResultToPerformanceHistory(accurate);
    }

    // Check if we're ready for next challenge
    if (currentLevel == MAX_LEVEL){
        if (countSuccesses() >= ENOUGH_SUCCESSES){
            Log.info("At MAX level! %u", currentLevel);
            //TODO At MAX level CHALLENGE DONE
            retryGame = false;
            resetPerformanceHistory();
        }
    }

    if (currentLevel < MAX_LEVEL){
        if (countSuccesses() >= ENOUGH_SUCCESSES){
            currentLevel++;
            Log.info("Leveling UP %u", currentLevel);
            retryGame = false;
            resetPerformanceHistory();
        }
    }

    if (countMisses() >= TOO_MANY_MISSES){
        if (currentLevel > 1){
            currentLevel--;
            Log.info("Leveling DOWN %u", currentLevel);
            retryGame = false;
            resetPerformanceHistory();
        }
    }

    // Send report
    if(!timeout){
        Log.info("Sending report");

        String extra = String::format(
            "{\"start_state\":\"%c%c%c\",\"pressedSeq\":\"%s\",\"presses\":%u,"
            "\"num_colors\":%u,\"retryGame\":%c}",
            REPORT_COLORS[touchpadsColorStart[0]],
            REPORT_COLORS[touchpadsColorStart[1]],
            REPORT_COLORS[touchpadsColorStart[2]], pressedSeq.c_str(),
            padsPressed, numberOfColors,
            retryGame ? '1' : '0');  // TODO this is the new value

        // Log.info(extra);

        hub.Report( Time.format(gameStartTime, TIME_FORMAT_ISO8601_FULL), // play_start_time
                    PlayerName,            // player
                    currentLevel,          // level
                    String(accurate),       // result
                    activityDuration,      // duration
                    accurate,               // foodtreat_presented
                    foodtreatWasEaten,    // foodtreatWasEaten
                    extra                   // extra field
                    );

    }

    // printPerformanceArray();

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

    // Play 1 interaction of the Matching More Colors challenge
    gameIsComplete = playMatchingMoreColors();

    if(gameIsComplete){
        // Interaction end
        return;
    }

}
