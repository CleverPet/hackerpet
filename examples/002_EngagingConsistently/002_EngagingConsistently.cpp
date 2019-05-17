/**
  Engaging Consistently
  =====================

    This challenge takes off the training wheels and no longer offers your
    player free foodtreats. Now your player will need to press a touchpad to
    earn a reward.

    Through repeated action and feedback from the Hub, players learn that the
    front of the Hub — where the touchpads are — is the most interesting part.
    Before this challenge, a very patient player could hang out and simply wait
    for foodtreats to arrive on their own.

    **Challenge logic:** This challenge uses a combination of performance and a
    timer to progress the game-play. There are 3 levels and 3 corresponding
    timer durations (10 minutes, 10 minutes, and 5 minutes respectively). The
    timer starts running, and the player can play so long as the timer hasn't
    expired. During the challenge the player levels up if 10 foodtreats have
    been eaten. When the timer expires, the level is checked and the
    corresponding timer length is loaded into the timer. The current count of
    foodtreats eaten is cleared and new interactions can continue to be played
    while the timer is running. Once the player reaches level 3 and eats 10
    foodtreats the max level is reached. Leveling down requires many misses,
    since the player would need to fail to consume 99 foodtreats in one level.
    When this happens, the timer is immediately reset and the duration is
    adjusted. Another way to level down is be to ignore the touchpads, which
    will make the current interaction time-out.

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
int currentLevel = 1;  // starting level
const int MAX_LEVEL = 3;
const unsigned long CHALLENGE_TIMER_DURATIONS[MAX_LEVEL] = {600000,  // level 1: 10 mins
                                                           600000,  // level 2: 10 mins
                                                           300000}; // level 3: 5 mins
const int HISTORY_LENGTH = 100; // Number of previous interactions to look at for
                               // performance This challenge uses a timer, so doesn't
                               // care about history length.
const int ENOUGH_SUCCESSES = 10;     // if pos interactions >= ENOUGH_SUCCESSES level-up
const int TOO_MANY_MISSES = 99;      // if neg interactions >= TOO_MANY_MISSES level-down
const int YELLOW = 60;              // touchpad color
const int BLUE = 60;                // touchpad color
const int FLASHING = 0;             // touchpad 0: no FLASHING
const int FLASHING_DUTY_CYCLE = 99; // ignored since no FLASHING

/**
 * Global variables and constants
 * ------------------------------
 */
const unsigned long SOUND_FOODTREAT_DELAY = 1200; // (ms) delay for reward sound
const unsigned long SOUND_TOUCHPAD_DELAY = 300; // (ms) delay for touchpad sound

bool performance[HISTORY_LENGTH] = {0}; // store the progress in this challenge
unsigned char perfPos = 0;   // to keep our position in the performance array
unsigned char perfDepth = 0; // to keep the size of the number of perf numbers
                              // to consider
// timer values to check if challenge should continue
unsigned long challenge_timer_before, challenge_timer_length = 0;
bool reset_challenge_timer = true; // bool to check if timer time should be
                                   // updated

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

//// return the number of positive interactions in performance history for current level
unsigned int countSuccesses() {
  unsigned int total = 0;
  for (unsigned char i = 0; i <= perfDepth - 1; i++)
    if (performance[i] == 1)
      total++;
  return total;
}

/// return the number of negative interactions in performance history for current level
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
  Log.info("New successes: %u, misses: %u", countSuccesses(),
           countMisses());
}

/// print the performance history for debugging
void printPerformanceArray() {
  Serial.printf("performance: ");
  for (unsigned char i = 0; i < HISTORY_LENGTH; i++)
    Serial.printf("%u", performance[i]);
  Serial.printf("\n");
}

/// The actual EngagingConsistently challenge. This function needs to be called in a loop.
bool playEngagingConsistently() {
  yield_begin();

  static unsigned long gameStartTime, timestamp_before, reaction_time = 0;
  static unsigned char foodtreatState = 99;
  static bool foodtreatWasEaten = false; // store if foodtreat was eaten in last interaction
  static bool timeout = false;
  static int tray_duration = 6000;
  static int timeout_duration = 300000; // 5 mins
  static unsigned char pressed = 0;     // to hold the pressed touchpad

  // Static variables and constants are only initialized once, and need to be re-initialized
  // on subsequent calls
  gameStartTime = 0;
  timestamp_before = 0;
  reaction_time = 0;
  foodtreatState = 99;
  foodtreatWasEaten = false; // store if foodtreat was eaten in last interaction
  timeout = false;
  tray_duration = 6000;
  timeout_duration = 300000; // 5 mins
  pressed = 0;     // to hold the pressed touchpad

  Log.info("-------------------------------------------");
  // Log.info("Starting new \"Engaging Consistently\" challenge");
  Log.info("Current level: %u, successes: %u, number of misses: %u", currentLevel,
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
           millis() < (timestamp_before + timeout_duration));

  // Record time period for performance logging
  reaction_time = millis() - timestamp_before;

  // Turn off lights
  hub.SetLights(hub.LIGHT_BTNS, 0, 0, 0);

  // Check result
  if (pressed == 0) {
    Log.info("No touchpad pressed, we have a timeout");
    timeout = true;
    foodtreatWasEaten = false;
  } else {
    Log.info("Button pressed, dispensing foodtreat");
    timeout = false;

    // give the Hub a moment to finish playing the touchpad sound
    yield_sleep_ms(SOUND_TOUCHPAD_DELAY, false);
    // Play "reward" sound
    hub.PlayAudio(hub.AUDIO_POSITIVE, 20);
    // give the Hub a moment to finish playing the reward sound
    yield_sleep_ms(SOUND_FOODTREAT_DELAY, false);

    // Dispense a foodtreat and wait until the tray is closed again
    do {
      foodtreatState = hub.PresentAndCheckFoodtreat(tray_duration);
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
  }

  // Send report
  Log.info("Sending report");
  String extra = String::format(
      "{\"pos_tries\":%u,\"neg_tries\":%u}", countSuccesses(),
      countMisses()); // TODO these are the current values, need new values
  hub.Report(
      Time.format(gameStartTime, TIME_FORMAT_ISO8601_FULL), // play_start_time
      PlayerName,                                            // player
      currentLevel,                                          // level
      String(foodtreatWasEaten),                            // result
      reaction_time, // duration -> linked to level and includes tray movement
      1,             // foodtreat_presented
      foodtreatWasEaten, // foodtreatWasEaten
      extra                // extra field
  );

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
  // BAD_PERFORMACE is really high, so will never come here
  if (countMisses() >= TOO_MANY_MISSES) {
    if (currentLevel > 1) {
      currentLevel--;
      Log.info("Leveling DOWN %u", currentLevel);
      reset_challenge_timer = true;
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

  // if the challenge timer expired we need to reset it
  if (reset_challenge_timer) {
    // Log.info("Timer reset");
    resetPerformanceHistory();
    challenge_timer_before = millis();
    challenge_timer_length = CHALLENGE_TIMER_DURATIONS[currentLevel - 1];
    reset_challenge_timer = false;
  }

  // Keep playing interactions as long as timer didn't expire
  if (millis() <= (challenge_timer_before + challenge_timer_length)) {
    // Play 1 level of the EngagingConsistently challenge
    // Will return true if level is done
    gameIsComplete = playEngagingConsistently();
  } else {
    // Log.info("Timer expired");
    reset_challenge_timer = true;
  }

  if(gameIsComplete){
    return;
  }
}
