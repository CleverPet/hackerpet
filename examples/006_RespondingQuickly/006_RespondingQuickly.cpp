/**
  Responding Quickly
  ====================

    This is the seventh challenge from the original CleverPet learning
    curriculum.

    In this challenge, immediately after your player presses one illuminated
    touchpad, a different one lights up and must be touched. Your player now has
    to press two touchpads in a row to solve one puzzle. Chaining behaviors
    together requires that your player complete multiple correct actions in
    sequence to earn a reward. Your player must learn to stay attentive for a
    second instruction even after the first press.

    **Challenge logic:** This challenge has 4 levels. With each higher level the
    maximum reaction time decreases. One interaction consists of 2 moves, in
    each move 1 touchpad lights up and has to be touched to advance. The
    reaction time is the time between the first and second moves. If 17
    successful interactions occur over  the previous 20, the player levels up.
    After 17 misses the player levels down. On a miss there will be a
    between-interaction delay. For the player's first move  multiple touchpads
    can be touched, as long as the lit up touchpad was one of them (player has
    unlimited opportunities to try and get it right). The second touch, however,
    allows only the lit up touchpad to be touched, and the interaction will
    result in a miss if any of the lit touchpads is pressed more than once, or
    if the unlit touchpad is pressed at all.

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
const int MAX_LEVEL = 4;         // Maximum number of levels
const unsigned long MAX_REACTION_TIME[MAX_LEVEL] = {30000, 8000, 4000, 2000};
const int HISTORY_LENGTH = 20;   // Number of previous interactions to look at for
                                // performance
const int ENOUGH_SUCCESSES = 17; // if num successes >= ENOUGH_SUCCESSES level-up
const int TOO_MANY_MISSES = 17;  // if num misses >= TOO_MANY_MISSES level-down
const long FOODTREAT_DURATION = 6000;   // (ms) how long to present foodtreat
const int YELLOW = 80;
const int BLUE = 80;
const int SLEW = 20; // touchpad fade-in time [0-99]
const unsigned long TIMEOUT_MS = 300000; // (ms) how long to wait until
                                        // restarting the interaction
const unsigned long INTER_GAME_DELAY = 6000;

/**
 * Global variables and constants
 * ------------------------------
 */
const unsigned long SOUND_FOODTREAT_DELAY = 1200; // (ms) delay for reward sound
const unsigned long SOUND_TOUCHPAD_DELAY = 300; // (ms) delay for touchpad sound
const unsigned long VIEW_WINDOW = 500; // time delay for viewing the touchpad

bool performance[HISTORY_LENGTH] = {0}; // store the progress in this challenge
int perfPos = 0;   // to keep our position in the performance array
int perfDepth = 0; // to keep the size of the number of perf numbers to
                    // consider

// Use primary serial over USB interface for logging output (9600)
// Choose logging level here (ERROR, WARN, INFO)
SerialLogHandler logHandler(LOG_LEVEL_INFO, { // Logging level for all messages
    { "app.hackerpet", LOG_LEVEL_ERROR }, // Logging level for library messages
    { "app", LOG_LEVEL_INFO } // Logging level for application messages
});

// initialize hub interface (from hackerpet)
HubInterface hub;

// enables simultaneous execution of application and system thread
SYSTEM_THREAD(ENABLED);

/**
 * Helper functions
 * ----------------
 */

/// return the number of successful interactions in performance history for current level
unsigned int countSuccesses() {
  unsigned int total = 0;
  for (unsigned char i = 0; i <= perfDepth - 1; i++)
    if (performance[i] == 1)
      total++;
  return total;
}

/// return the number of misses in performance history for current level
unsigned int countMisses() {
  unsigned int total = 0;
  for (unsigned char i = 0; i <= perfDepth - 1; i++)
    if (performance[i] == 0)
      total++;
  return total;
}

/// reset performance history to 0
void resetPerformanceHistory() {
  for (unsigned char i = 0; i < HISTORY_LENGTH; i++)
    performance[i] = 0;
  perfPos = 0;
  perfDepth = 0;
}

/// add an interaction result to the performance history
void addResultToPerformanceHistory(bool entry) {
  // Log.info("Adding %u", entry);
  performance[perfPos] = entry;
  perfPos++;
  if (perfDepth < HISTORY_LENGTH)
    perfDepth++;
  if (perfPos > (HISTORY_LENGTH - 1)) { // make our performance array circular
    perfPos = 0;
  }
  // Log.info("perfPos %u, perfDepth %u", perfPos, perfDepth);
  Log.info("New successful interactions: %u, misses: %u", countSuccesses(),
           countMisses());
}

/// print the performance history for debugging
void printPerformanceArray() {
  Serial.printf("performance: {");
  for (unsigned char i = 0; i < perfDepth; i++) {
    Serial.printf("%u", performance[i]);
    if ((i + 1) == perfPos)
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
    if ((maskedPressed & (maskedPressed-1)) != 0) // check if multiple pads touched
                                                  //(except for correct one)
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

/// The actual RespondingQuickly challenge. This function needs to be called in a loop.
bool playRespondingQuickly() {
  yield_begin();

  static unsigned long gameStartTime, time_start_wait, timestamp_before, activityDuration = 0;
  static unsigned char foodtreatState = 99;
  static unsigned char touchpads[3] = { hub.BUTTON_LEFT,  // should not be re-initialized
                                        hub.BUTTON_MIDDLE,
                                        hub.BUTTON_RIGHT};
  static bool retryTarget = false; // should not be re-initialized
  static bool accurate = false;
  static bool timeout = false;
  static unsigned char pressed[2] = {0, 0};
  static bool foodtreatWasEaten = false; // store if foodtreat was eaten in last interaction

  // Static variables and constants are only initialized once, and need to be re-initialized
  // on subsequent calls
   gameStartTime = 0;
   time_start_wait = 0;
   timestamp_before = 0;
   activityDuration = 0;
   foodtreatState = 99;
   accurate = false;
   timeout = false;
   pressed[0] = 0;
   pressed[1] = 0;
   foodtreatWasEaten = false; // store if foodtreat was eaten in last interaction

  Log.info("-------------------------------------------");
  // Log.info("Starting new \"Responding Quickly\" challenge");
  Log.info("Current level: %u, successes: %u, num misses: %u", currentLevel,
           countSuccesses(), countMisses());

  gameStartTime = Time.now();

  // before starting interaction, wait until:
  //  1. device layer is ready (in a good state)
  //  2. foodmachine is "idle", meaning it is not spinning or dispensing
  //      and tray is retracted (see FOODMACHINE_... constants)
  //  3. no touchpad is currently pressed
  yield_wait_for((hub.IsReady() &&
                  hub.FoodmachineState() == hub.FOODMACHINE_IDLE &&
                  not hub.AnyButtonPressed()),
                 false);

  // DI reset occurs if, for example, device  layer detects that touchpads need
  // re-calibration
  hub.SetDIResetLock(true);

  // Record start timestamp for performance logging
  timestamp_before = millis();

  if (retryTarget) {
    Log.info("We're doing a retry interaction");
  } else {
    // randomly shuffle our 3 touchpads in an array
    random_shuffle(&touchpads[0], &touchpads[3]);
  }

  hub.SetLights(touchpads[0], YELLOW, BLUE, SLEW);

  // progress to next state
  time_start_wait = millis();

  do {
    // detect any touchpads currently pressed
    pressed[0] = hub.AnyButtonPressed();
    yield(false);
    // Ignore non-target touches
  } while (!(pressed[0] & touchpads[0]) // 0 if any pressed touchpad match
           && millis() < time_start_wait + TIMEOUT_MS); // 0 if timed out

  hub.SetLights(hub.LIGHT_BTNS, 0, 0, 0); // turn off lights

  // wait until: no touchpad is currently pressed
  yield_wait_for((!hub.AnyButtonPressed()), false);

  // Check touchpads and accuracy
  if (pressed[0] == 0) {
    Log.info("No touchpad pressed, first interaction timeout");
    timeout = true;
    accurate = false;
  } else {
    Log.info("First interaction: correct touchpad pressed");
    timeout = false;
    hub.SetLights(touchpads[1], YELLOW, BLUE, SLEW);
    // make sure the player has seen the touchpad
    yield_sleep_ms(VIEW_WINDOW, false);

    // progress to next state
    time_start_wait = millis();

    do {
      // detect any touchpads currently pressed
      pressed[1] = hub.AnyButtonPressed();
      yield(false); // use yields statements any time the hub is pausing or
                    // waiting
    } while (       // 0 if any touchpad except interaction1 pad is touched
        !(pressed[1] & (touchpads[1] | touchpads[2])) &&
        // 0 if timed out
        millis() < time_start_wait + MAX_REACTION_TIME[currentLevel - 1]);

    hub.SetLights(hub.LIGHT_BTNS, 0, 0, 0); // turn off lights

    if (pressed[1] == 0) {
      Log.info("No touchpad pressed, second interaction timeout");
      timeout = true;
      accurate = false;
    } else {
      timeout = false;
      if (pressed[1] == touchpads[1]) {
        accurate = true;

        Log.info("Second interaction: correct touchpad pressed, dispensing foodtreat");
        // give the Hub a moment to finish playing the touchpad sound
        yield_sleep_ms(SOUND_TOUCHPAD_DELAY, false);
        hub.PlayAudio(hub.AUDIO_POSITIVE, 20);
        // give the Hub a moment to finish playing the reward sound
        yield_sleep_ms(SOUND_FOODTREAT_DELAY, false);
        // if successful interaction, present foodtreat using
        // PresentAndCheckFoodtreat state machine
        do {
          foodtreatState = hub.PresentAndCheckFoodtreat(FOODTREAT_DURATION);
          yield(false);
        } while (foodtreatState != hub.PACT_RESPONSE_FOODTREAT_NOT_TAKEN &&
                 foodtreatState != hub.PACT_RESPONSE_FOODTREAT_TAKEN);

        // Check if foodtreat was eaten
        if (foodtreatState == hub.PACT_RESPONSE_FOODTREAT_TAKEN) {
          Log.info("Foodtreat was eaten");
          foodtreatWasEaten = true;
        } else {
          Log.info("Foodtreat was not eaten");
          foodtreatWasEaten = false;
        }
      } else {
        Log.info("Second interaction: wrong touchpad pressed");
        accurate = false;
        foodtreatWasEaten = false;
        // give the Hub a moment to finish playing the touchpad sound
        yield_sleep_ms(SOUND_TOUCHPAD_DELAY, false);
        hub.PlayAudio(hub.AUDIO_NEGATIVE, 5);
        // give the Hub a moment to finish playing the sound
        yield_sleep_ms(SOUND_FOODTREAT_DELAY, false);
      }
    }
  }

  // record time period for performance logging
  activityDuration = millis() - timestamp_before;

  if (!timeout) {
    addResultToPerformanceHistory(accurate);
  }

  // Check if we're ready for next challenge
  if (currentLevel == MAX_LEVEL) {
    if (countSuccesses() >= ENOUGH_SUCCESSES) {
      Log.info("At MAX level! %u", currentLevel);
      // TODO At MAX level CHALLENGE DONE
      resetPerformanceHistory();
    }
  } else {
    // Increase level if foodtreat eaten and good performance in this level
    if (countSuccesses() >= ENOUGH_SUCCESSES) {
      if (currentLevel < MAX_LEVEL) {
        currentLevel++;
        Log.info("Leveling UP %u", currentLevel);
        resetPerformanceHistory();
      }
    }
  }
  // Decrease level if bad performance in this level
  if (countMisses() >= TOO_MANY_MISSES) {
    if (currentLevel > 1) {
      currentLevel--;
      Log.info("Leveling DOWN %u", currentLevel);
      resetPerformanceHistory();
    }
  }

  if (!timeout) {
    // Send report
    Log.info("Sending report");
    String extra = "{\"targetSeq\":\"";
    extra += convertBitfieldToLetter(touchpads[0]);
    extra += convertBitfieldToLetter(touchpads[1]);
    extra += "\",\"pressedSeq\":\"";
    // Multiple touches possible, but irrelevant for reporting
    extra += convertBitfieldToLetter(touchpads[0]);
    // multiple touches possible, only report a single wrong one if a miss
    extra += convertBitfieldToSingleLetter(touchpads[1],pressed[1]);
    extra += String::format("\",\"retryGame\":\"%c\"}", retryTarget ? '1' : '0');

    hub.Report(Time.format(gameStartTime,
                           TIME_FORMAT_ISO8601_FULL),  // play_start_time
               PlayerName,                             // player
               currentLevel,                           // level
               String(accurate),                       // result
               activityDuration,                       // duration
               accurate,                               // foodtreat_presented
               foodtreatWasEaten,                      // foodtreatWasEaten
               extra                                   // extra field
    );
  }

  // Check if we need to do, or reset a retry interaction
  if (accurate) {
    // Reset retry interaction
    retryTarget = false;
  } else if (!timeout) {
    // Set retry interaction
    retryTarget = true;
  }

  // printPerformanceArray();

  if (retryTarget) {
    // Log.info("in the doghouose");
    // between interaction wait time if a miss
    yield_sleep_ms(INTER_GAME_DELAY, false);
  }

  hub.SetDIResetLock(false); // allow DI board to reset if needed between interactions
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
void loop() {
  bool gameIsComplete = false;

  // Advance the device layer state machine, but with 20 ms max time
  // spent per loop cycle.
  hub.Run(20);

  // Play 1 interaction of the Responding Quickly challenge
  gameIsComplete = playRespondingQuickly(); // Will return true if level is done

  if (gameIsComplete) {
    // Interaction end
    return;
  }
}
