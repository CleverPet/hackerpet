/*
 *  ColorMatch Enhanced (Beta)
 *  ==========================
 *
 *  This is a game that lets you see your dog or cat (called a "player" here)
 *  trying out different strategies for solving puzzles. Touching a touchpad
 *  toggles it to a different color, and when all the touchpad colors match, the
 *  game ends.
 *
 *  This game is designed to train a player as quickly as possible to solve
 *  progressively higher numbers of touchpad color states, and to do so in fewer
 *  and fewer touches. Most of the time, a player is presented with a
 *  "must-solve" interaction that they must solve in the fewest possible touches
 *  (starting with 1). Occasionally, the player is shown a multi-touch version,
 *  both so they can learn how the game works, and also for the sake of variety.
 *
 *  The easiest way to adjust the behavior of the game is to change the values
 *  of the named constants, such as DEFAULT_NUM_STATES, DRILL_PAR_VALUE, and
 *  PAUSE_AFTER_... values.
 *
 *  Here is a description of the features added to the game since the release of
 *  the original ColorMatch.
 *
 *  "Par"
 *  -----
 *  This is the minimum number of touches required to solve a given ColorMatch
 *  combination. E.g., in 2-color ColorMatch (BLUE, YELLOW), every game is a
 *  "Par 1" because it's always possible to solve the game in one touch by
 *  simply pressing the touchpad that's different. No matter how many colors are
 *  involved, there will always be starting color combinations that are "Par 1"
 *  and can be solved in a single press. Potential par values increase as the
 *  number of colors involved increases. E.g., with 6 different colors we can
 *  get as high as a Par 6 (with the starting states BWL and YDS).
 *
 *  Drills (must-solve interactions)
 *  --------------------------------
 *  Part of the purpose of ColorMatch is to demonstrate player exploration and
 *  show the player making decisions. But it's still helpful to work on "drills"
 *  that show the player combinations that are easily solved. In this latest
 *  version of ColorMatch, with the given default settings, most of the
 *  interactions are expected to be drills, and the player "must" press the
 *  correct touchpad to move to a different interaction. If they don't solve it,
 *  they're given a "re-do" until they get it right. This helps discourage
 *  random guessing or default touching behaviors that ignore the color state of
 *  the touchpads.
 *
 *  Multitouch/drill alternation
 *  ----------------------------
 *  With the current default settings, after successfully completing a drill,
 *  MULTITOUCH_EXIT_PERCENT (25%) of the time the player will be given a
 *  multitouch version, where the player has up to DEFAULT_MAX_TOUCHES
 *  (currently 20) touches to solve the puzzle. After they solve it, they'll be
 *  back to their drilling.
 *
 *  Variable reinforcement
 *  ----------------------
 *  By only providing foodtreats some of the time, instead of always, it's
 *  possible to increase engagement while holding constant the number of
 *  foodtreats we offer. While the reward chime (a secondary reinforcer just
 *  like a clicker) is always played when the player is successful, a foodtreat
 *  (the primary reinforcer) is only provided randomly a given % of the time.
 *  Here the default has been set to 50%, and it's possible, depending on the
 *  level of motivation of your player, to set it even as low as 10%. If you
 *  would like to reduce the reinforcement ratio further, it's recommended that
 *  you do so gradually over at least a few days.
 *
 *  Streaks
 *  -------
 *  A player that solves multiple interactions in a row will hear multiple
 *  reward chimes corresponding to the number of successes they've had. When
 *  they reach STREAK_THRESHOLD (currently 4) their probability of gettting a
 *  foodtreat goes to 100%, rewarding the player's sustained attention and
 *  performance. When they go STREAK_BONUS_THRESHOLD beyond STREAK_THRESHOLD
 *  they will get *two* foodtreats as extra encouragement.
 *
 *  Extra pauses
 *  ------------
 *  To discourage guessing, after getting it wrong two times in a row the delay
 *  between interactions is increased to EXTRA_PAUSE_AFTER_MAX_TOUCHES,
 *  currently set to 10000 ms (10 seconds).
 *
 *  Pre-calculated par combinations
 *  -------------------------------
 *  In order to train higher levels of multitouch, pre-calculated par values
 *  have been included here. You'll see them in variables with names such as
 *  LEVEL_3_PAR_1, LEVEL_4_PAR_1, ... While they are not used on the two color
 *  version, when DEFAULT_NUM_STATES is set to 3 or 4 and DRILL_PAR_VALUE is set
 *  to 1, they will be used for single-touch drills.
 *
 *  Touches-running-out warning indicator
 *  -------------------------------------
 *  When playing a multitouch version of the game, as the player approaches the
 *  maximum number of touches available, the indicator light will flash
 *  increasingly quickly to warn them that they're running out of touches. This
 *  also serves to indicate to the player whether they are on a drill or a
 *  multitouch interaction.
 *
 *  Author: CleverPet, Inc.
 *
 *  Copyright 2019
 *  Licensed under the AGPL 3.0
 *
 */

#include <Particle.h>
#include "papertrail.h"
#include <hackerpet.h>

#define LEN(arr) ((int) (sizeof (arr) / sizeof (arr)[0]))

using namespace std;

SYSTEM_THREAD(ENABLED);

// access to hub functionality (lights, foodtreats, etc.)
HubInterface hub;

//Below is very useful for wireless logging. See 106_UTDPDebug.cpp for more info
//PapertrailLogHandler papertailHandler("192.168.1.122", 4888, "ColorMatch");

// Use primary serial over USB interface for logging output (9600)
// Choose logging level here (ERROR, WARN, INFO)
SerialLogHandler logHandler(LOG_LEVEL_ERROR, { // Logging level for all messages
    { "app.hackerpet", LOG_LEVEL_ERROR }, // Logging level for library messages
    { "app", LOG_LEVEL_INFO } // Logging level for application messages
});


/*
                            <<<                             >>>
                            <<<         Constants           >>>
                            <<<                             >>>

   Settings for the color match game

*/

// Set this to the name of your player (dog, cat, etc.)
const char PLAYER_NAME[] = "Pet, Clever";

// Adjust the next block of constants to change the operation of the game
const int DEFAULT_MAX_TOUCHES = 20; // maximum number of allowed touches in multitouch version of the game
const int DEFAULT_NUM_STATES = 2; // maximum number of touchpad/button color states
const int DRILL_PAR_VALUE = 1; // What par value the player should drill. See from_random_set_states.
const int REINFORCEMENT_PERCENT = 50; // what percent of correct presses/successes result in a foodtreat
const int STREAK_THRESHOLD = 4; // when player is on a "winning streak" of STREAK_THRESHOLD or more, ignore REINFORCEMENT_PERCENT and always presents a foodtreat
const int STREAK_BONUS_THRESHOLD = 3; // (ms) time after getting it wrong twice in a row
const int MAX_STREAK_LENGTH = 10; // Winning streaks cannot get any longer than this number. Resets to zero after
const int REDO_EXIT_PERCENT = -1; // randomly escape from a re-do sequence this % of the time. -1 indicates "never"
const int MULTITOUCH_EXIT_PERCENT = 25; // After a successful drill interaction, randomly go to multitouch this percent of the time
const int KIBBLE_PRESENTATION_TIME = 5000; // (ms) tray stays out when kibble presented
const int PAUSE_AFTER_SUCCESS = 1200; // (ms) time between successful interactions
const int PAUSE_AFTER_TIMEDOUT = 4000; // (ms) time after timed out interactions
const int PAUSE_AFTER_MAX_TOUCHES = 6000; // (ms) time after getting it wrong
const int EXTRA_PAUSE_AFTER_MAX_TOUCHES = 10000; // (ms) time after getting it wrong twice in a row

const int MAX_TIME_ON_TOUCHPADS = 10; // tells player to get off the touchpads after MAX_TIME_... seconds. Only sometimes works.

// COLOR BRIGHTNESS SETTINGS
const int MAX_YELLOW = 40;
const int MAX_BLUE = 99;
const int MIX_YELLOW = 20;
const int MIX_BLUE = 50;
const int MED_BRIGHT = 10;
const int MIN_BRIGHT = 0;

// Currently hard max for states is 6 different colors
const unsigned char MAX_BUTTON_STATES = 6;

const String STATE_NAMES[MAX_BUTTON_STATES] = {"BLUE", "YELLOW", "WHITE",  "DIMWHITE", "LIGHTYELLOW", "SKYBLUE"};
const char STATE_SHORT[MAX_BUTTON_STATES] = {'B', 'Y', 'W', 'D', 'L', 'S'};

const unsigned char GET_BLUE_FOR_STATE[] =   {MAX_BLUE, MIN_BRIGHT, MAX_BLUE, MED_BRIGHT, MIX_BLUE, MAX_BLUE};

const unsigned char GET_YELLOW_FOR_STATE[] = {MIN_BRIGHT, MAX_YELLOW, MAX_YELLOW, MED_BRIGHT, MAX_YELLOW, MIX_YELLOW};

const int OUTCOME_MATCH = 1;
const int OUTCOME_TIMEOUT = 2;
const int OUTCOME_MAXTOUCHES = 3;
const int OUTCOME_CONTINUE = 0;

const unsigned char BUTTONS[] = {hub.LIGHT_LEFT, hub.LIGHT_MIDDLE, hub.LIGHT_RIGHT};
const unsigned char NUMBUTTONS = 3;


// Below is currently a partial inclusion of different difficulty initial
// touchpad states. More can be added later as needed.
const char LEVEL_3_PAR_1[][3] = {{2,2,1},{2,1,2},{1,2,2},
                                       {2,0,0},{1,1,0},{1,0,1},
                                       {0,2,0},{0,0,2},{0,1,1}};

const char LEVEL_3_PAR_2[][3] = {{0,2,2}, {0,0,1}, {0,1,0}, {1,0,0},
                                        {1,1,2}, {1,2,1}, {2,0,2}, {2,1,1},
                                        {2,2,0}};

const char LEVEL_3_PAR_3[][3] = {{2,1,0}, {2,0,1}, {1,2,0}, {1,2,0},
                                        {1,0,2}, {0,1,2}, {0,2,1}};

const char LEVEL_4_PAR_1[][3] = {{0,0,3}, {0,1,1}, {0,3,0}, {1,0,1},
                                        {1,1,0}, {1,2,2}, {2,1,2}, {2,2,1},
                                        {2,3,3}, {3,0,0}, {3,2,3}, {3,3,2}};

const char LEVEL_4_PAR_2[][3] = {{0,0,1}, {0,0,2}, {0,1,0}, {0,2,0},
                                        {0,2,2}, {0,3,3}, {1,0,0}, {1,1,2},
                                        {1,1,3}, {1,2,1}, {1,3,1}, {1,3,3},
                                        {2,0,0}, {2,0,2}, {2,1,1}, {2,2,0},
                                        {2,2,3}, {2,3,2}, {3,0,3}, {3,1,1},
                                        {3,1,3}, {3,2,2}, {3,3,0}, {3,3,1}};

const char LEVEL_4_PAR_3[][3] = {{0,1,2}, {0,1,3}, {0,2,1}, {0,2,3},
                                        {0,3,1}, {0,3,2}, {1,0,2}, {1,0,3},
                                        {1,2,0}, {1,2,3}, {1,3,0}, {1,3,2},
                                        {2,0,1}, {2,0,3}, {2,1,0}, {2,1,3},
                                        {2,3,0}, {2,3,1}, {3,0,1}, {3,0,2},
                                        {3,1,0}, {3,1,2}, {3,2,0}, {3,2,1}};


const char LEVEL_5_PAR_1[][3] = {{0,0,4}, {0,1,1}, {0,4,0}, {1,0,1},
                                        {1,1,0}, {1,2,2}, {2,1,2}, {2,2,1},
                                        {2,3,3}, {3,2,3}, {3,3,2}, {3,4,4},
                                        {4,0,0}, {4,3,4}, {4,4,3}};

const char LEVEL_6_PAR_1[][3] = {{0,0,5}, {0,1,1}, {0,5,0}, {1,0,1},
                                        {1,1,0}, {1,2,2}, {2,1,2}, {2,2,1},
                                        {2,3,3}, {3,2,3}, {3,3,2}, {3,4,4},
                                        {4,3,4}, {4,4,3}, {4,5,5}, {5,0,0},
                                        {5,4,5}, {5,5,4}};

// Global variables
static char btn_state[] = {0, 0, 0};
static char orig_btn_state[] = {0, 0, 0};
static char btn_state_chars[] = "XXX";
static char foodtreat_state = 0;


static bool next_btn_pending = false;
static int next_num_states = -1;
static int next_max_touches = -1;

static unsigned long time_started_on_touchpads = 0;
static unsigned long reset_timer = 0;

static bool match = false;
static bool timeout = false;
static bool hub_is_on = true;
static bool multitouch = false;

static char next_btn_state[] = {0, 0, 0};
static int num_states = DEFAULT_NUM_STATES;
static int max_touches = DEFAULT_MAX_TOUCHES;
static int touches = 0;
static int par = -1;
static int retry_number = 0;
static int streak = 0;
static int tmp_i = -1;
static int no_eats = 0;

/*
                            <<<                             >>>
                            <<<      Helper functions       >>>
                            <<<                             >>>

   Functions here depend on global parameters being appropriately set.

*/

/**
 * Function:  set_lights_for_all_button_states
 * ---------------------------------------
 *
 *  Sets the light states of the touchpads/touchpads
 *
 */
int set_lights_for_all_button_states()
{
    for (int i = 0; i < NUMBUTTONS; i++)
    {
        hub.SetLights(BUTTONS[i],
                    GET_YELLOW_FOR_STATE[btn_state[i]],
                    GET_BLUE_FOR_STATE[btn_state[i]],
                    30);
        delay(10);
    }
    return true;
}

/**
 * Function:  set_touches_warning
 * ------------------------------
 *
 *  Sets the indicator light to flash faster as there are fewer touches remaining
 *
 */
const int FLASH_PERIODS[] = {0, 12, 25, 50, 99}; // warns of running out of touches

void set_touches_warning(int remaining)
{
    int period = FLASH_PERIODS[remaining];

    if (remaining < 5)
    {
        hub.SetLights(hub.LIGHT_CUE, 40, 40, period, period/2);
    }
    else
    {
        hub.SetLights(hub.LIGHT_CUE, 5, 5, 0, 0);
    }
}


/**
 * Function:  toggle_multitouch
 * ----------------------------
 *
 *  Switches the game from being in single-touch to mulit-touch mode
 *
 *  Single-touch requires that a particular touchpad be touched, and helps to
 *  train the player to notice which touchpad is different.
 *
 *  Multitouch enables more exploration, and shows how the cycles of the touchpads
 *  toggle from one state to the other.
 *
 */
void toggle_multitouch()
{
    Log.info("Toggling multitouch");
    if (!multitouch)
    {
        max_touches = DEFAULT_MAX_TOUCHES;
        num_states = DEFAULT_NUM_STATES;
        par = -1;
    }
    else
    {
        par = DRILL_PAR_VALUE;
        max_touches = par;
        num_states = DEFAULT_NUM_STATES;
    }

    multitouch = !multitouch;
    Log.info("Multitouch is now %d", multitouch);
    Log.info("Par is %d", par);
}

/**
 * Function:  from_next_set_states
 * -------------------------------
 *
 * Sets states based on remote requested next move.
 *
 */
void from_next_set_states()
{
    btn_state[0] = next_btn_state[0];
    btn_state[1] = next_btn_state[1];
    btn_state[2] = next_btn_state[2];
    num_states = next_num_states;
    max_touches = next_max_touches;

    next_btn_pending = false;

    next_btn_state[0] = next_btn_state[1] = next_btn_state[2] = 99;
    next_num_states = 0;
    next_max_touches = 0;
}

/**
 * Function:  from_random_set_states
 * ---------------------------------
 *
 * Sets random states depending on global parameters
 *
 */
void from_random_set_states()
{

    if (par == 1 and num_states == 3)
    {
        int choice = random(0, LEN(LEVEL_3_PAR_1));

        Log.info("Length of array is %d", LEN(LEVEL_3_PAR_1));
        Log.info("Picking a random state %d", choice);

        btn_state[0] = LEVEL_3_PAR_1[choice][0];
        btn_state[1] = LEVEL_3_PAR_1[choice][1];
        btn_state[2] = LEVEL_3_PAR_1[choice][2];

    }
    else if (par == 1 and num_states == 4)
    {
        int choice = random(0, LEN(LEVEL_4_PAR_1));

        Log.info("Length of array is %d", LEN(LEVEL_4_PAR_1));
        Log.info("Picking a random state %d", choice);

        btn_state[0] = LEVEL_4_PAR_1[choice][0];
        btn_state[1] = LEVEL_4_PAR_1[choice][1];
        btn_state[2] = LEVEL_4_PAR_1[choice][2];
    }
    else if (num_states == 4)
    {
        if (par == 2){
            int choice = random(0, LEN(LEVEL_4_PAR_2));

            btn_state[0] = LEVEL_4_PAR_2[choice][0];
            btn_state[1] = LEVEL_4_PAR_2[choice][1];
            btn_state[2] = LEVEL_4_PAR_2[choice][2];

        }
        else if (par == 3){
            int choice = random(0, LEN(LEVEL_4_PAR_3));

            btn_state[0] = LEVEL_4_PAR_3[choice][0];
            btn_state[1] = LEVEL_4_PAR_3[choice][1];
            btn_state[2] = LEVEL_4_PAR_3[choice][2];

        }
        else
        {
            Log.error("SHOULDN'T GET HERE!");
        }
    }
    else
    {
        while (btn_state[0] == btn_state[1] && btn_state[1] == btn_state[2])
        {
            // Init a non-matching state
            // pick one for each button until non-matching init state.
           btn_state[0] = random(0, num_states);  // [min, max)
           btn_state[1] = random(0, num_states);  // [min, max)
           btn_state[2] = random(0, num_states);  // [min, max)
        }
    }
    Log.info("Set random states. Par is %d");

}

/**
 * Function:  set_redo
 * -------------------
 *
 * Makes the next interaction be the same as the previous one to prevent
 * a player from succeeding by just touching randomly.
 *
 */
void set_redo()
{
    Log.info("Setting redo");

    next_btn_state[0] = orig_btn_state[0];
    next_btn_state[1] = orig_btn_state[1];
    next_btn_state[2] = orig_btn_state[2];

    next_num_states = num_states;
    next_max_touches = max_touches;

    next_btn_pending = true;
}

/**
 * Function:  which_button
 * -----------------------
 *
 * Returns the button/touchpad number corresponding to a given press.
 * (Press values are coded as bitmaps and in doing so can indicate multiple
 *  simultaneous touchpads)
 *
 */
int which_button(char pressed)
{
    if (pressed == 1 or pressed % 2 == 0)
    {
        // The touchpads are 0, 1, and 2
        return pressed >> 1;
    }
    else
    {
        return -1; // combinations of more than one button
    }
}

/**
 * Function:  advance_button_state_for_pressed
 * -------------------------------------------
 *
 * Adjusts touchpad/button color state given touchpad pressed
 *
 */
void advance_button_state_for_pressed(unsigned char pressed)
{
    int i = which_button(pressed);
    if (i > -1)
    {
        btn_state[i] = (btn_state[i]+1)%num_states;
    }
}

/**
 * Function:  full_reset
 * ---------------------
 *
 * Reset dli and reset Photon
 *
 */
void full_reset()
{
    Log.info("Doing full_reset");
    hub.ResetDI();
    hub.ResetFoodMachine();
    hub.Run(500);
    System.reset();
}

/*
                            <<<                             >>>
                            <<<        Particle hooks       >>>
                            <<<                             >>>
*/


/**
 * Function:  isOn
 * ---------------
 *
 * Turn the hub off or on (accepts commands 0 or 1)
 *
 */
int isOn(String command) {
    Log.info("Received isOn", command);
    hub_is_on = command.toInt();

    return 1;
}

/**
 * Function:  triggerReset
 * -----------------------
 *
 * Triggers a full reset of the Hub
 *
 */
int triggerReset(String command)
{
    Log.info("Received triggerReset.");
    full_reset();
    return 1;
}

int giveFoodtreat(String command)
{
    Log.info("Received giveFoodtreat.");
    int foodtreat_duration = 5000;
    if (command.length() > 0)
    {
        foodtreat_duration = command.toInt();
    }

    Log.info("Foodtreat present duration is %d", foodtreat_duration);

    hub.PresentAndCheckFoodtreat(foodtreat_duration);
    return 1;
}


/**
 * Function:  ColorMatchGame
 * -------------------------
 *
 * A yield macro magic-friendly way to run the Color Match game.
 *
 */
bool ColorMatchGame()
{
    yield_begin();

    // Yield macro magic requires all local variables be static, otherwise
    // they will be reset every time the function is re-entered
    static unsigned long playstart;
    static unsigned long timeout_s = 30;
    static unsigned long cur_timeout_start = 0;
    static unsigned long time_start_ms = 0;
    static unsigned long cur_time = 0;
    static unsigned char pressed = 0;
    static unsigned char button_id = 0;
    static unsigned long start_wait = 0;
    static unsigned long pause_duration = 0;

    static int outcome = -1;
    static int foodtreat_presented = -1;
    static int foodtreat_presented_record = -1;

    static bool enticed;
    static bool will_toggle_multitouch;
    static bool reset_retry_number;
    static bool incr_retry_number;
    static char init_btn_state_chars[4] = {'X', 'X', 'X', 0};
    static String touches_seq;
    static const char * RESULT_STR;

    // Static variable are only initialized once, and need to be re-initialized
    // on subsequent calls
    btn_state[0] = btn_state[1] = btn_state[2] = 0;
    foodtreat_state = -1;
    timeout = false;
    match = false;
    will_toggle_multitouch = false;
    reset_retry_number = false;
    incr_retry_number = false;
    outcome = -1;
    touches = 0;
    touches_seq = "";
    RESULT_STR = "";

    foodtreat_presented = 0;
    foodtreat_presented_record = 0;

    if (max_touches < 0)
    {
        max_touches = DEFAULT_MAX_TOUCHES;
    }
    enticed = false;

    start_wait = 0;
    pause_duration = 0;


    time_started_on_touchpads = 0;

    // before starting interaction, wait until:
    //  0. Device is on.
    //  1. device layer is ready (in a good state)
    //  2. foodmachine is "idle", meaning it is not spinning or dispensing
    //         and tray is retracted (see FOODMACHINE_... constants)
    //  3. no button is currently pressed

    while (not (hub_is_on
                && hub.IsReady()
                && hub.FoodmachineState() == hub.FOODMACHINE_IDLE
                && not hub.AnyButtonPressed()))
    {
        if (hub_is_on && hub.FoodmachineState() == hub.FOODMACHINE_IDLE)
        {
            if (time_started_on_touchpads == 0)
            {
                time_started_on_touchpads = Time.now();
            }
            else if (Time.now() - time_started_on_touchpads > MAX_TIME_ON_TOUCHPADS)
            {

                time_started_on_touchpads = 0; // reset timer
                while (hub.AnyButtonPressed())
                {
                    Log.info("GET OFF THE TOUCHPADS!");

                    // ask him to get off the touch pads
                    hub.PlayAudio(hub.AUDIO_NEGATIVE, 50);
                    yield_sleep_ms(300, false);
                }
            }
        }
        yield(false);
    }

    if (next_btn_pending)
    {
        from_next_set_states();
    }
    else
    {
        from_random_set_states();
    }

    set_lights_for_all_button_states(); // false -> not animated

    // DI reset occurs if, for example, DL detects that touchpads need
    // re-calibration, e.g. if hub was moved to a different surface
    // We want to prevent resets from occurring in the middle of an interaction.
    hub.SetDIResetLock(true);

    btn_state_chars[0] = STATE_SHORT[btn_state[0]];
    btn_state_chars[1] = STATE_SHORT[btn_state[1]];
    btn_state_chars[2] = STATE_SHORT[btn_state[2]];

    // remember what the last state was, in order to potentially repeat later
    orig_btn_state[0] = btn_state[0];
    orig_btn_state[1] = btn_state[1];
    orig_btn_state[2] = btn_state[2];

    strcpy(init_btn_state_chars, btn_state_chars);

    // Surrounding a hub command by yields helps ensure that it gets the message
    yield_sleep_ms(100, false);
    hub.SetButtonAudioEnabled(true);
    yield_sleep_ms(40, false);

    time_start_ms = millis(); // interaction time in MILLISECONDS
    cur_time = 0;

    cur_timeout_start = playstart = Time.now(); // UTC epoch time in SECONDS

    set_touches_warning(max_touches - touches); // warn re touches running out

    outcome = OUTCOME_CONTINUE;

    while (outcome == OUTCOME_CONTINUE)
    {
        // Wait here until a button is pressed or until we have a timeout
        do {
            if (touches > 0
                and not enticed
                && (millis() - (cur_time + time_start_ms)) > ((timeout_s*1000)/2)) {

                // entice with squeak
                hub.PlayAudio(hub.AUDIO_SQUEAK, 10);
                yield_sleep_ms(320, false);

                enticed = true;
            }

            pressed = hub.AnyButtonPressed();
            yield(false);

        } while ((hub_is_on
                    && (pressed != hub.BUTTON_LEFT
                        && pressed != hub.BUTTON_MIDDLE
                        && pressed != hub.BUTTON_RIGHT
                        )
                && touches < max_touches // TODO: should this be here?
                && Time.now() < cur_timeout_start + timeout_s
                ));

        if ( !hub_is_on) {

            Log.info("HUB IS OFF STOP EVEYTHING");
            hub.SetDIResetLock(false);
            hub.SetButtonAudioEnabled(false);
            hub.SetLights(hub.LIGHT_ALL, 0, 0, 0);   // turn off lights

            yield_finish();
            return true;
        }

        cur_time = millis() - time_start_ms;


        if (pressed != 0)
        {
            hub.SetButtonAudioEnabled(false);
            yield_sleep_ms(40, false);

            // reset timeout if player continues to play
            cur_timeout_start = Time.now();

            touches++;
            enticed = false;

            // advance state based on button press
            advance_button_state_for_pressed(pressed);

            // set the lights for current new state of touchpads
            button_id = which_button(pressed);

            hub.SetLights(BUTTONS[button_id],
                        GET_YELLOW_FOR_STATE[btn_state[button_id]],
                        GET_BLUE_FOR_STATE[btn_state[button_id]],
                        0);

            // give it a few ms to finish playing the sound before sending messages etc.
            yield_sleep_ms(250, false);


            set_touches_warning(max_touches - touches);

            touches_seq.concat(String(button_id));

            // check if match in state (all touchpads have same state and
            // therefore same color)
            if (btn_state[0] == btn_state[1] && btn_state[1] == btn_state[2])
            {
                outcome = OUTCOME_MATCH;
                break;
            }
            else if (touches >= max_touches) // only if no match
            {
                outcome = OUTCOME_MAXTOUCHES;
                break;
            }
            else
            {
                outcome = OUTCOME_CONTINUE;

                // wait for no touchpads to be pressed before looking for new
                // button presses (so we don't process same button press
                // repeatedly)

                yield_wait_for((hub.AnyButtonPressed() == 0
                    || Time.now() > (cur_timeout_start + timeout_s)), false);

                hub.SetButtonAudioEnabled(true);
                yield_sleep_ms(40, false);
            }
        }
        else if (Time.now() >= (cur_timeout_start + timeout_s))
        {
            outcome = OUTCOME_TIMEOUT;
            break;
        }
        else
        {
            Log.error("Should not be in this else!");
        }
    } // Loop end

    switch (outcome)
    {
        case OUTCOME_MATCH:
            RESULT_STR = STATE_NAMES[btn_state[0]].c_str();

            hub.SetLights(hub.LIGHT_CUE, 0, 0, 0);
            hub.SetLights(hub.LIGHT_CUE, 99, 99, 90);
            streak++;
            Log.info("Streak: %d", streak);

            for (tmp_i = 0; tmp_i < streak; tmp_i++) {
                hub.PlayAudio(hub.AUDIO_POSITIVE, 40);
                Log.info("Playing AUDIO_POSITIVE");
                yield_sleep_ms(400, false);
            }

            if (par > 1 && touches == par)
            {
                Log.info("JACKPOT!");
                // Nailed it: Jackpot
                foodtreat_presented = 2;

                hub.PlayAudio(hub.AUDIO_POSITIVE, 40);
                yield_sleep_ms(300, false);

                hub.PlayAudio(hub.AUDIO_POSITIVE, 40);
                yield_sleep_ms(300, false);

                hub.PlayAudio(hub.AUDIO_POSITIVE, 40);
                yield_sleep_ms(300, false);
            }
            else if (streak >= STREAK_THRESHOLD)
            {
                foodtreat_presented = true;
            }
            else
            {
                foodtreat_presented = random(0,100) < REINFORCEMENT_PERCENT;
            }

            if (streak >= STREAK_THRESHOLD + STREAK_BONUS_THRESHOLD)
            {
                // Doing great! Extra!
                foodtreat_presented++;
                Log.info("Adding extra reward for super-streak");
            }

            pause_duration = PAUSE_AFTER_SUCCESS; // time

            foodtreat_presented_record = foodtreat_presented;

            while (foodtreat_presented)
            {
                do {
                    foodtreat_state=hub.PresentAndCheckFoodtreat(KIBBLE_PRESENTATION_TIME); //time pres (ms)
                    yield(false);
                } while (foodtreat_state!=hub.PACT_RESPONSE_FOODTREAT_NOT_TAKEN &&
                     foodtreat_state!=hub.PACT_RESPONSE_FOODTREAT_TAKEN);

                Log.info("Treat presented");
                foodtreat_presented--;

                // The below assumes that there's still kibble/treats in the dish eaten after two
                // presentations, there's probably sommething wrong with the sensor reading.
                if (foodtreat_state == hub.PACT_RESPONSE_FOODTREAT_NOT_TAKEN)
                {
                    no_eats++;
                }
                else
                {
                    no_eats = 0;
                }

                if (no_eats > 1)
                {
                    hub.ResetFoodMachine();
                    no_eats = 0;
                }
            }

            if (max_touches == 1 && random(0,100) < MULTITOUCH_EXIT_PERCENT)
            {
                // Randomly switch to multitouch
                will_toggle_multitouch = true;
            }
            else if (max_touches > 1)
            {
                // Already multitouch. Switch to single touch
                will_toggle_multitouch = true;
            }
            // Otherwise keep the game the same

            if (streak >= MAX_STREAK_LENGTH)
            {
                streak = 0;
            }

            reset_retry_number = true;

            break;

        case OUTCOME_TIMEOUT:
            RESULT_STR = "TIMEOUT";
            streak = 0;
            cur_time = millis() - time_start_ms;

            hub.SetLights(hub.LIGHT_ALL, 0, 0, 0);   // turn off lights
            yield_sleep_ms(30, false);

            if (max_touches == 1)
            {
                // If we're doing single-touch teaching, have the player try again
                set_redo();
            }


            if (touches > 0)
            {
                hub.PlayAudio(hub.AUDIO_NEGATIVE, 3);
                pause_duration = PAUSE_AFTER_TIMEDOUT;
            }
            break;

        case OUTCOME_MAXTOUCHES:
            RESULT_STR = "MAX_TOUCHES_REACHED";
            streak = 0;

            hub.PlayAudio(hub.AUDIO_NEGATIVE, 10);
            yield_sleep_ms(400, false);

            hub.SetLights(hub.LIGHT_ALL, 0, 0, 0);   // turn off lights
            yield_sleep_ms(30, false);

            pause_duration = PAUSE_AFTER_MAX_TOUCHES;


            if (max_touches == 1)
            {
                if (random(0,100) > REDO_EXIT_PERCENT && max_touches == 1)
                {
                    incr_retry_number = true;;

                    if (retry_number > 0)
                    {
                        pause_duration += EXTRA_PAUSE_AFTER_MAX_TOUCHES;
                    }

                    set_redo();
                }
                else
                {
                    reset_retry_number = true;
                    will_toggle_multitouch = true;
                }
            }
            else
            {
                will_toggle_multitouch = true;
            }
            break;

        default:
            Log.error("Outcome switch FELL THROUGH");
            break;
    }

    if (touches > 0) { // Don't report when nothing happened
      String extras = String::format(
          "{\"touches\":%u,\"sequence\":\"%s\",\"startstate\":\"%s\","
          "\"maxtouches\":%u,\"retry_number\":%u}",
          touches, touches_seq.c_str(), init_btn_state_chars, max_touches,
          retry_number);

      hub.Report(Time.format(playstart,
                             TIME_FORMAT_ISO8601_FULL),  // play_start_time
                 PLAYER_NAME,                            // player
                 num_states,  // number of states -> more is harder
                 RESULT_STR,  // result
                 cur_time,
                 foodtreat_presented_record,  // foodtreat_presented
                 foodtreat_state ==
                     hub.PACT_RESPONSE_FOODTREAT_TAKEN,  // foodtreat_eaten
                 extras);
        }

    // allow DI board to reset if needed between interactions -- should
    // probably be last thing done before hub.Run() and
    // waiting until hub.IsReady(), etc.
    hub.SetDIResetLock(false);

    hub.SetButtonAudioEnabled(false);
    hub.SetLights(hub.LIGHT_ALL, 0, 0, 0);   // turn off lights

    // Make these changes after logging.
    if (will_toggle_multitouch)
    {
        toggle_multitouch();
    }

    if (reset_retry_number)
    {
        retry_number = 0;
        reset_retry_number = false;
    }
    else if (incr_retry_number)
    {
        retry_number++;
        incr_retry_number = false;
    }

    yield_sleep_ms(pause_duration, false);



    yield_finish();
    return true;
}

bool system_ready = false;

unsigned long last_timestamp = 0;


// setup function for particle
void setup()
{
    Particle.function("triggerReset", triggerReset);
    Particle.function("isOn", isOn);
    Particle.function("giveFoodtreat", giveFoodtreat);

    Serial.begin(9600);

    // Initializes the hub and passes the current filename as ID for reporting
    hub.Initialize(__FILE__);
}

// loop function for particle
void loop()
{
    // advance the device layer state machine,
    // with 20 millisecond max time spent per loop cycle
    hub.Run(20);

    if (WiFi.ready() && system_ready == false)
    {
        system_ready = true;
        Particle.publish("ColorMatch Started", PRIVATE);
        hub.PlayAudio(hub.AUDIO_SQUEAK, 10);
        delay(200);
    }

    if (system_ready)
    {
        ColorMatchGame();
    }

}
