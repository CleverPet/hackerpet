/**
  Eating The Food
  ===============

    The goal of this challenge is to help your player get comfortable with the
    Hub’s sounds and movements.

    The Hub's dish will rotate out and offer a free foodtreat to your player at
    varying intervals. If your player does not take the foodtreat, the dish will
    stay out for progressively longer periods of time.

    **Challenge logic**: This challenge has 6 levels with 6 corresponding
    foodtreat offer durations. A foodtreat is automatically offered to the
    player for the corresponding time. If the foodtreat isn't eaten the level is
    increased and the offer duration will be extended. If the foodtreat was
    eaten the level will go down and presentation time will be shortened. If 3
    foodtreats were eaten in the previous 5 interactions, the challenge is
    completed and the performance array will be reset.

  Authors: CleverPet Inc. Jelmer Tiete <jelmer@tiete.be>

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
int currentLevel = 4;        // level to start at
const int ANCHOR_LEVEL = 4;  // "Anchor" level (will fall back here if level is
                             // high and foodtreat is eaten)
const int ENOUGH_SUCCESSES = 3;  // Number of interactions required to proceed
const int HISTORY_LENGTH = 5;    // Number of interactions to keep track of
const int MAX_LEVEL = 6;         // Length of the FOODTREAT_DURATIONS array
const unsigned long FOODTREAT_DURATIONS[MAX_LEVEL] = {192000, 96000, 48000,
                                                      24000,  12000, 6000};

/**
 * Global variables and constants
 * ------------------------------
 */
const unsigned long SOUND_FOODTREAT_DELAY = 1200;  // (ms) delay for reward
                                                   // sound

int performance[HISTORY_LENGTH] = {0};  // store the progress in this challenge
int perfPos = 0;                        // position in the performance array
bool foodtreatWasEaten = false;  // store if foodtreat was eaten in last interaction

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

/// The actual Eating The Food challenge. Function must be be called in a loop.
bool playEatingTheFood() {
  yield_begin();

  static unsigned long gameStartTime, timestamp_before, activityDuration = 0;
  static unsigned char foodtreatState = 99;

  // Static variables and constants are only initialized once, and need to be
  // re-initialized on subsequent calls
  gameStartTime, timestamp_before, activityDuration = 0;
  foodtreatState = 99;

  Log.info("-------------------------------------------");
  Log.info("Starting new \"Eating The Food\" challenge");

  gameStartTime = Time.now();

  // before starting interaction, wait until:
  //  1. device layer is ready (in a good state)
  //  2. foodmachine is "idle", meaning it is not spinning or dispensing and
  //  tray is retracted (see FOODMACHINE_... constants)
  //  3. no button is currently pressed
  yield_wait_for(
      (hub.IsReady() && hub.FoodmachineState() == hub.FOODMACHINE_IDLE &&
       not hub.AnyButtonPressed()),
      false);

  // DI reset occurs if, for example, device  layer detects that touchpads need
  // re-calibration
  hub.SetDIResetLock(true);

  Log.info("At level %u", currentLevel);
  Log.info("Presenting foodtreat for %lu ms",
           FOODTREAT_DURATIONS[currentLevel - 1]);

  // Record start timestamp for performance logging
  timestamp_before = millis();

  // play "reward" sound
  hub.PlayAudio(hub.AUDIO_POSITIVE, 20);
  // give the Hub a moment to finish playing the reward sound
  yield_sleep_ms(SOUND_FOODTREAT_DELAY, false);

  // dispense a foodtreat and wait until the tray is closed again
  do {
    foodtreatState =
        hub.PresentAndCheckFoodtreat(FOODTREAT_DURATIONS[currentLevel - 1]);
    yield(false);
  } while (foodtreatState != hub.PACT_RESPONSE_FOODTREAT_NOT_TAKEN &&
           foodtreatState != hub.PACT_RESPONSE_FOODTREAT_TAKEN);

  // record time period for performance logging
  // (activity duration will always be interaction time + tray movement)
  activityDuration = millis() - timestamp_before;

  // check if foodtreat was eaten
  if (foodtreatState == hub.PACT_RESPONSE_FOODTREAT_TAKEN) {
    Log.info("Foodtreat was eaten, reaction time: %lu", activityDuration);
    foodtreatWasEaten = true;
  } else {
    Log.info("Foodtreat not eaten");
    foodtreatWasEaten = false;
  }

  // send report
  Log.info("Sending report");
  hub.Report(
      Time.format(gameStartTime, TIME_FORMAT_ISO8601_FULL),  // play_start_time
      playerName,                                            // player
      currentLevel,               // level -> lower level is better
      String(foodtreatWasEaten),  // result
      activityDuration,  // duration -> linked to level and includes tray
                         // movement
      1,                 // foodtreat_presented
      foodtreatWasEaten  // foodtreat_eaten
  );

  // decide if level is going up or down
  if (foodtreatWasEaten == true) {
    if (currentLevel < MAX_LEVEL) {
      if (currentLevel < ANCHOR_LEVEL) {
        currentLevel = ANCHOR_LEVEL;  // Jump to the anchor level
      } else {
        currentLevel++;  // Let's go faster!
      }
    }
  } else {
    if (currentLevel > 1) {
      currentLevel--;  // Foodtreat not eaten, increase the foodtreat offer
                       // time.
    }
  }
  hub.SetDIResetLock(false);  // allow DI board to reset if needed between
                              // interactions
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
  // You can also pass your own ID like so
  // hub.Initialize("MyAwesomeGame");
}

/**
 * Main loop function
 * ------------------
 */
void loop() {
  unsigned int perf_total = 0;  // sum of performance of the
                                // HISTORY_LENGTH last interactions
  bool gameIsComplete = false;

  // Advance the device layer state machine, but with 20 ms max time
  // spent per loop cycle.
  hub.Run(20);

  // Play 1 level of the Eating The Food challenge
  gameIsComplete = playEatingTheFood();  // Will return true if level is done

  // Store level result in performance array
  if (gameIsComplete) {
    performance[perfPos] = foodtreatWasEaten;  // store the interaction result
                                               // in the performance array
    perfPos++;
    if (perfPos > (HISTORY_LENGTH - 1)) {  // make our performance array circular
      perfPos = 0;
    }
  }

  // Check performance array if we're ready to pass to next challenge
  for (int i = 0; i < HISTORY_LENGTH; ++i) {
    perf_total += performance[i];
    if (perf_total >= ENOUGH_SUCCESSES) {
      Log.info("Challenge completed!");
      // TODO go to next challenge code
      // reset performance
      perf_total = 0;
      for (unsigned char i = 0; i < HISTORY_LENGTH; ++i) performance[i] = 0;
    }
  }
}
