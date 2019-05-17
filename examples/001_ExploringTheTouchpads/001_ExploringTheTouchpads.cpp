/**
  Exploring The Touchpads
  =======================

    This is the second challenge from the original CleverPet learning
    curriculum.

    A foodtreat is still offered periodically “for free”, but the player will
    also earn a reward when they press a touchpad — even if just "by accident".

    This challenge is designed to have players begin to associate their own
    behavior with getting a foodtreat. Through a combination of classical and
    operant conditioning, players begin to understand that the Hub is a tool
    they can control to get tasty snacks.

    **Challenge logic:** The challenge consists of 4 levels with accompanying
    time-out speeds and foodtreat presentation intervals. If 3 foodtreats were
    eaten in the last 6 interactions, the player levels up. If 4 foodtreats were
    left uneaten within the last 6 interactions, the player levels down. When
    the player reaches level 4 and eats 3 foodtreats, the level resets back to
    level 4.

  Authors:    CleverPet Inc.
              Jelmer Tiete <jelmer@tiete.be>

  Copyright 2019
  Licensed under the AGPL 3.0
*/

#include <hackerpet.h>

// Set this to the name of your player (dog, cat, etc.)
const char playerName[] = "Pet, Clever";

/**
 * Challenge settings
 * -------------
 *
 * These constants (capitalized CamelCase) and variables (camelCase) define the
 * gameplay
 */
const int ENOUGH_SUCCESSES = 3; // if num successes >= ENOUGH_SUCCESSES level-up
const int TOO_MANY_MISSES = 4;  // if num misses >= TOO_MANY_MISSES level-down
const int HISTORY_LENGTH = 6;  // Number of previous interactions to look at for
                               // performance
int currentLevel = 1;          // starting level
const int MAX_LEVEL = 4;       // holds the highest possible level
const unsigned long TRAY_PRESENT_DURATION[MAX_LEVEL] = {12000, 10000, 8000,
                                                        6000};
const unsigned long TIMEOUT_DURATIONS[MAX_LEVEL] = {60000, 180000, 600000,
                                                    99999999};
const int YELLOW = 60;          // touchpad color
const int BLUE = 60;            // touchpad color
const int FLASHING = 0;             // touchpad 0: no FLASHING
const int FLASHING_DUTY_CYCLE = 99; // ignored since no FLASHING

/**
 * Global variables and constants
 * ------------------------------
 */
const unsigned long SOUND_FOODTREAT_DELAY = 1200; // (ms) delay for reward sound
const unsigned long SOUND_TOUCHPAD_DELAY = 300; // (ms) delay for touchpad sound

bool performance[HISTORY_LENGTH] = {0}; // store the progress in this challenge
unsigned char perfPos = 0;   // to keep our pos in the performance array
unsigned char perfDepth = 0; // to keep the size of the number of perf numbers
                              // to consider

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
int countSuccesses() {
  unsigned int total = 0;
  for (unsigned char i = 0; i <= perfDepth - 1; i++)
    if (performance[i] == 1)
      total++;
  return total;
}

/// return the number of misses in performance history for current level
int countMisses() {
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

/// add a interaction result to the performance history
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
  Serial.printf("performance: ");
  for (unsigned char i = 0; i < HISTORY_LENGTH; i++)
    Serial.printf("%u", performance[i]);
  Serial.printf("\n");
}

//// The actual ExploringTheTouchpads function. This function needs to be called in a loop.
bool playExploringTheTouchpads() {
  yield_begin();

  static bool foodtreatWasEaten = false; // store if foodtreat was eaten in last interaction
  static unsigned long timestamp_before, reaction_time, gameStartTime = 0;
  static bool timeout = false;          // holds timeout state
  static int foodtreatState = 99;       // holds return value of foodmachine
  static int FLASHING = 0;              // 0: no FLASHING
  static int FLASHING_DUTY_CYCLE = 99;  // ignored since no FLASHING
  static unsigned char pressed = 0;     // bit field that holds which button was
                                        //    pressed

  // Static variables and constants are only initialized once, and need to be
  // re-initialized on subsequent calls
  foodtreatWasEaten = false;       // store if foodtreat was eaten in last interaction
  timestamp_before, reaction_time, gameStartTime = 0;
  timeout = false;         // holds timeout state
  foodtreatState = 99;     // holds return value of foodmachine
  pressed = 0;    // bit field that holds which touchpad was


  Log.info("-------------------------------------------");
  Log.info("Starting new \"Exploring The Touchpads\" challenge");
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
                  !hub.AnyButtonPressed()),
                 false);

  // DI reset occurs if, for example, device layer detects that touchpads
  // need re-calibration
  hub.SetDIResetLock(true);

  // Record start timestamp for performance logging
  timestamp_before = millis();

  // Turn on touchpad lights
  hub.SetRandomButtonLights(3, YELLOW, BLUE, FLASHING, FLASHING_DUTY_CYCLE);

  // Wait here until a touchpad is pressed or until we have a timeout
  do {
    pressed = hub.AnyButtonPressed();
    yield(false);
  } while ((pressed != hub.BUTTON_LEFT && pressed != hub.BUTTON_MIDDLE &&
            pressed != hub.BUTTON_RIGHT) &&
           millis() <
                (timestamp_before + TIMEOUT_DURATIONS[currentLevel - 1]));

  // Record time period for performance logging
  reaction_time = millis() - timestamp_before;

  // Turn off lights
  hub.SetLights(hub.LIGHT_BTNS, 0, 0, 0);

  // Check result
  if (pressed == 0) {
    Log.info("No touchpad pressed, we have a timeout, dispensing foodtreat");
    timeout = true;
  } else {
    Log.info("Button pressed, dispensing foodtreat");
    timeout = false;
  }

  // give the Hub a moment to finish playing the touchpad sound
  yield_sleep_ms(SOUND_TOUCHPAD_DELAY, false);
  // Play "reward" sound
  hub.PlayAudio(hub.AUDIO_POSITIVE, 20);
  // give the Hub a moment to finish playing the reward sound
  yield_sleep_ms(SOUND_FOODTREAT_DELAY, false);

  // Dispense a foodtreat and wait until the tray is closed again
  do {
    foodtreatState =
        hub.PresentAndCheckFoodtreat(TRAY_PRESENT_DURATION[currentLevel - 1]);
    yield(false);
  } while (foodtreatState != hub.PACT_RESPONSE_FOODTREAT_NOT_TAKEN &&
           foodtreatState != hub.PACT_RESPONSE_FOODTREAT_TAKEN);

  // Check if foodtreat was eaten
  if (foodtreatState == hub.PACT_RESPONSE_FOODTREAT_TAKEN) {
    Log.info("Treat was eaten");
    foodtreatWasEaten = true;
  } else {
    Log.info("Treat was not eaten");
    foodtreatWasEaten = false;
  }

  if (!timeout) {
    // Send report
    Log.info("Sending report");
    hub.Report(
        Time.format(gameStartTime,
                    TIME_FORMAT_ISO8601_FULL), // play_start_time
        playerName,                           // player
        currentLevel,                         // level
        String(foodtreatWasEaten),           // result
        reaction_time, // duration -> linked to level and includes tray movement
        1,             // foodtreat_presented
        foodtreatWasEaten // foodtreatWasEaten
    );
  }

  // Check if we're ready for next challenge
  if (currentLevel == MAX_LEVEL) {
    addResultToPerformanceHistory(foodtreatWasEaten);
    if (countSuccesses() >= ENOUGH_SUCCESSES) {
      Log.info("At MAX level! %u", currentLevel);
      // TODO At MAX level CHALLENGE DONE
      resetPerformanceHistory();
    }
  } else {
    // Increase level if foodtreat eaten and good performance in this level
    addResultToPerformanceHistory(foodtreatWasEaten);
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

  // printPerformanceArray();

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
  Log.info("Starting new \"Exploring The Touchpads\" challenge");
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

  // Play 1 level of the ExploringTheTouchpads challenge
  gameIsComplete = playExploringTheTouchpads(); // Will return true if level is done

  if (gameIsComplete) {
    return;
  }
}
