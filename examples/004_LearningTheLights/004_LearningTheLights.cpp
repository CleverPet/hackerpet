/**
  Learning The Lights
  ===================

    This is the fifth challenge from the original CleverPet learning curriculum.

    Only one touchpad is lit. This is the challenge where most players will
    start to "see the lights". If your player wasn’t using the information
    provided by the lights on the previous challenge, they could expect to get
    rewarded about two-thirds of the time, just by chance. Now, chance is only 1
    in 3 guesses. The lights are twice as important as before!

    **Challenge logic:** This challenge has only 1 level. if 30 successful
    interactions were played in the last 50 games, the challenge is done (will
    reset). Every interaction, 1 random random target will be picked and
    compared with the pressed touchpad. The player's performance is recorded,
    and only correct presses count.  When an incorrect touchpad is pressed, the
    next interaction will be a redo interaction with the same target. There is a
    random between interactions pause ranging from 1 to 8 seconds

  Authors: CleverPet Inc.
           Jelmer Tiete <jelmer@tiete.be>

  Copyright 2019
  Licensed under the AGPL 3.0
*/

#include <hackerpet.h>

// Set this to the name of your player (dog, cat, etc.)
const char PlayerName[] = "Pet, Clever";

/**
 * Challenge settings
 * -------------
 *
 * These constants (capitalized CamelCase) and variables (camelCase) define the
 * gameplay
 */
int currentLevel = 1;          // current and starting level
const int MAX_LEVEL = 1;         // Maximum number of levels
const int HISTORY_LENGTH = 50;   // Number of previous interactions to look at for
                                // performance
const int ENOUGH_SUCCESSES = 30; // if num successes >= ENOUGH_SUCCESSES level-up
const unsigned long FOODTREAT_DURATION = 5000;   // (ms) how long to present
                                                // foodtreat
const int FLASHING = 0;
const int FLASHING_DUTY_CYCLE = 99;
const unsigned long TIMEOUT_MS = 60000; // (ms) how long to wait until restarting
                                       // the interaction
const int NUM_PADS = 1; // Choose number of pads to light up at once (1, 2 or 3)

/**
 * Global variables and constants
 * ------------------------------
 */
const unsigned long SOUND_FOODTREAT_DELAY = 1200; // (ms) delay for reward sound
const unsigned long SOUND_TOUCHPAD_DELAY = 300; // (ms) delay for touchpad sound

bool performance[HISTORY_LENGTH] = {0}; // store the progress in this challenge
unsigned char perfPos = 0;   // to keep our position in the performance array
unsigned char perfDepth = 0; // size of the number of perf numbers to consider

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

/// The actual LearningTheLights challenge. This function needs to be called in a loop.
bool playLearningTheLights() {
  yield_begin();

  static unsigned long timestampBefore, activityDuration, timestampTouchpad = 0;
  static int foodtreatState = 99;
  static unsigned long gameStartTime = 0; // store the interaction start time for report
  static int yellow = 60; // touchpad color, is randomized
  static int blue = 60; // touchpad color, is randomized
  static unsigned char target, pressed = 0;
  static unsigned char retryTarget = 0; // should not be re-initialized
  static bool accurate = false;
  static bool timeout = false;
  static bool foodtreatWasEaten = false; // store if foodtreat was eaten
  static bool challengeComplete = false; // do not re-initialize

  // Static variables and constants are only initialized once, and need to be re-initialized
  // on subsequent calls
  timestampBefore = 0;
  activityDuration =0;
  timestampTouchpad = 0;
  foodtreatState = 99;
  gameStartTime = 0; // store the interaction start time for report
  yellow = 60; // touchpad color, is randomized
  blue = 60; // touchpad color, is randomized
  target, pressed = 0;
  accurate = false;
  timeout = false;
  foodtreatWasEaten = false; // store if foodtreat was eaten

  Log.info("-------------------------------------------");
  Log.info("Starting new \"Learning The Lights\" challenge");
  // Log.info("Current level: %u, successes: %u, num misses: %u",
  //     currentLevel, countSuccesses(), countMisses());

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
  timestampBefore = millis();

  yellow = random(20, 90); // pick a yellow for interaction
  blue = random(20, 90);    // pick a blue for interaction

  if (retryTarget != 0) {
    Log.info("We're doing a retry interaction");
    target = retryTarget;
    hub.SetLights(target, yellow, blue, FLASHING, FLASHING_DUTY_CYCLE);
  } else {
    // choose some target lights, and store which targets were randomly chosen
    target = hub.SetRandomButtonLights(NUM_PADS, yellow, blue, FLASHING,
                                       FLASHING_DUTY_CYCLE);
  }

  // progress to next state
  timestampTouchpad = millis();

  do {
    // detect any touchpads currently pressed
    pressed = hub.AnyButtonPressed();
    // use yields statements any time the hub is pausing or waiting
    yield(false);
  } while ((pressed != hub.BUTTON_LEFT // we want just a single touchpad
            && pressed != hub.BUTTON_MIDDLE && pressed != hub.BUTTON_RIGHT) &&
           millis() < timestampTouchpad + TIMEOUT_MS);

  // record time period for performance logging
  activityDuration = millis() - timestampBefore;

  hub.SetLights(hub.LIGHT_BTNS, 0, 0, 0); // turn off lights

  // Check touchpads and accuracy
  if (pressed == 0) {
    Log.info("No touchpad pressed, we have a timeout");
    timeout = true;
    accurate = false;
  } else {
    // Log.info("Button pressed");
    timeout = false;
    // will be zero if and only if the wrong touchpad was touched
    accurate = !((pressed & target) == 0);
  }

  foodtreatWasEaten = false;

  // Check result and consequences
  if (accurate) {
    Log.info("Correct touchpad pressed, dispensing foodtreat");
    // give the Hub a moment to finish playing the touchpad sound
    yield_sleep_ms(SOUND_TOUCHPAD_DELAY, false);
    hub.PlayAudio(hub.AUDIO_POSITIVE, 20);
    // give the Hub a moment to finish playing the reward sound
    yield_sleep_ms(SOUND_FOODTREAT_DELAY, false);
    // if successful interaction, present foodtreat using PresentAndCheckFoodtreat
    // state machine
    do {
      foodtreatState =
          hub.PresentAndCheckFoodtreat(FOODTREAT_DURATION); // time pres (ms)
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
    if (!timeout) // don't play any audio if there's a timeout (no response -> no
                  // consequence)
    {
      Log.info("Wrong touchpad pressed");
      // give the Hub a moment to finish playing the touchpad sound
      yield_sleep_ms(SOUND_TOUCHPAD_DELAY, false);
      // if unsuccessful interaction: play negative feedback sound at low volume
      hub.PlayAudio(hub.AUDIO_NEGATIVE, 5);
      yield_sleep_ms(
          SOUND_FOODTREAT_DELAY,
          false); // give the Hub a moment to finish playing the sound
    }
  }

  // Update performance, even on timeout
  // Check if we're ready for next challenge
  if (currentLevel == MAX_LEVEL) {
    addResultToPerformanceHistory(accurate);
    if (countSuccesses() >= ENOUGH_SUCCESSES) {
      Log.info("At MAX level! %u", currentLevel);
      challengeComplete = true;
      resetPerformanceHistory();
    }
  } else {
    // Increase level if foodtreat eaten and good performance in this level
    addResultToPerformanceHistory(accurate);
    if (countSuccesses() >= ENOUGH_SUCCESSES) {
      if (currentLevel < MAX_LEVEL) {
        currentLevel++;
        Log.info("Leveling UP %u", currentLevel);
        resetPerformanceHistory();
      }
    }
  }

  if (!timeout) {
    // Send report
    Log.info("Sending report");

    String extra = "{\"targets\":\"";
    extra += convertBitfieldToLetter(target);
    extra += "\",\"pressed\":\"";
    extra += convertBitfieldToLetter(pressed);
    extra += String::format("\",\"retryGame\":\"%c\"", retryTarget ? '1' : '0');
    if (challengeComplete) {extra += ",\"challengeComplete\":1";}
    extra += "}";
    
    hub.Report(Time.format(gameStartTime,
                           TIME_FORMAT_ISO8601_FULL), // play_start_time
               PlayerName,                            // player
               currentLevel,                         // level
               String(accurate),                      // result
               activityDuration,                     // duration
               accurate,                              // foodtreat_presented
               foodtreatWasEaten,                   // foodtreatWasEaten
               extra                                  // extra field
    );
  }

  // Check if we need to do, or reset a retry interaction
  if (accurate) {
    // Reset retry interaction
    retryTarget = 0;
  } else if (!timeout) {
    // Set retry interaction
    retryTarget = target;
  }

  // printPerformanceArray();

  // Random between interaction wait time
  yield_sleep_ms((unsigned long)random(1000, 8000), false);

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

  // Play 1 interaction of the Learning The Lights challenge
  gameIsComplete = playLearningTheLights(); // Will return true if level is done

  if (gameIsComplete) {
    // Interaction end
    return;
  }
}
