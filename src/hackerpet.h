#ifndef HACKERPET_H
#define HACKERPET_H

#include "application.h"
#include <queue>
#include <string>

using namespace std;

#define MAX_LEN_REPLY_BUFFER 64
// the maximum length of message buffer, which is used to receive a command from DL

#define STR_CARRIAGE_RETURN 0

// Gets us compilation date as yyyy-Mmm-dd instead of Mmm dd yyyy
#define __NICEDATE__ (char const[]){ \
__DATE__[7], __DATE__[8], __DATE__[9], __DATE__[10], '-', \
__DATE__[0], __DATE__[1], __DATE__[2], '-', \
__DATE__[4], __DATE__[5], '\0' }

#ifdef _WIN32
    #define SLASH '\\'
#else
    #define SLASH '/'
#endif

/*
                            <<<     Yield support with      >>>
                            <<<        macro magic          >>>
                            <<<                             >>>

    Eliminate the cumbersome state machine!

 * How-To
 * - Place yield_begin() and yield_finish() at start and end of your function
 *   block.
 * - Call yield(ret) whenever you want to yield execution in your function,
 *   while returning ret.
 * - Unmatched curly braces in your code will result in weird compile time
 *   errors, so be sure that braces are correctly matched.
 * - All local variables in functions that use yield must be static.
 * - Recursion is not permitted.

 * Use yield macros whenever your code would block. E.g., when waiting
 * for an external event or during a pause/delay.
*/

/* Yield Begin
 *
 * Place this at start of any function that uses yield statement
 */
#define yield_begin()                               \
    static void *_yield_state = nullptr;            \
    if (_yield_state != nullptr) goto *_yield_state

/* Yield Finish
 *
 * Place at end of any function that uses yield statement
 */
#define yield_finish()                                  \
    _yield_state = nullptr

#define _concat(a, b) a ## b
#define _yield_label(label, line)  _concat(label, line)
#define yield_label _yield_label(YIELD_LABEL_, __LINE__)

/* Yield
 *
 * Yields execution of your yield enabled function while returning ret.
 * Next time your yield enabled function is called, execution will continue the
 * last yield point.
 */
#define yield(ret)                                     \
    do {                                               \
        _yield_state = && yield_label;                 \
        return ret;                                    \
        yield_label: ;                                 \
    } while (0)

/* Yield Wait For
 *
 * Waits for a condition while yielding whenever the condition is not true.
 * Passes the ret parameter whenever yielding.
 */
#define yield_wait_for(condition, ret)             \
    do {                                           \
      while (!(condition)) {                       \
        yield(ret);                                \
      }                                            \
    } while (0)

/* Yield Sleep
 *
 * Waits for the specified number of microseconds, yielding while waiting.
 * Uses particle micros() function, which overflows when it reaches 2^32
 * (i.e., every ~71.6 minutes)
 */
#define yield_sleep(wait_time_in_microseconds, ret)                           \
  do {                                                                        \
    static unsigned long t1 = 0;                                              \
    t1 = micros();                                                            \
    while ((micros() - t1)                                                    \
           < wait_time_in_microseconds) {                                     \
      yield(ret);                                                             \
    }                                                                         \
  } while (0)

/* Yield Sleep milliseconds
 *
 * Waits for the specified number of milliseconds, yielding while waiting.
 * Uses millis() function, which overflows and returns to zero every ~49 days
 */
#define yield_sleep_ms(wait_time_in_milliseconds, ret)                         \
  do {                                                                         \
    static unsigned long t1 = 0;                                               \
    t1 = millis();                                                             \
    while ((millis() - t1)                                                     \
           < wait_time_in_milliseconds) {                                      \
      yield(ret);                                                              \
    }                                                                          \
  } while (0)

/* Yield If Condition
 *
 * Yields only if a condition is true (e.g. enough time has passed).
 */
#define yield_if(condition, ret)                 \
  do {                                           \
    if (condition) {                             \
      yield(ret);                                \
    }                                            \
  } while (0)



struct dlimsg_t {
    char buf[MAX_LEN_REPLY_BUFFER];
};

class HubInterface
{

public:
    HubInterface();

    bool Initialize(char * fileName);
    //
    //

    bool Run(unsigned long forHowLong);
    // advance the device layer state machine, but with forHowLong millisecond max time spent
    // meant to be run every cycle of a loop() function

    bool IsReady();
     // Whether or not the dli is ready for communication

    bool SetLights(unsigned char whichLights, unsigned char yellow, unsigned char blue, unsigned char slew);
    // set light colors with slew (overloaded below for flashing)
    // whichLights: see LIGHT_... constants in this class
    // colors and slew: can be [0, 99] each

    bool SetLightsRGB(unsigned char whichLights, unsigned char red, unsigned char green, unsigned char blue, unsigned char slew);
    // set light colors with slew using RGB, not BY
    // colors and slew: can be [0, 99] each

    bool SetLights(unsigned char whichLights, unsigned char yellow, unsigned char blue, unsigned char period, unsigned char on);
    // set lights to flash with given period
    // period in 10 ms increments, 0=no flash
    // on is duty cycle, 10 ms increments, must be < period
    // colors, period, on: can be [0, 99] each

    bool SetLightsRGB(unsigned char whichLights, unsigned char red, unsigned char green, unsigned char blue, unsigned char period, unsigned char on);
    // set lights to flash with given period, using RGB not BY
    // period in 10 ms increments, 0=no flash
    // on is duty cycle, 10 ms increments, must be < period
    // colors, period, on: can be [0, 99] each

    unsigned char SetRandomButtonLights(unsigned char numLights, unsigned char yellow, unsigned char blue, unsigned char period, unsigned char on);
    // illuminates randomly selected touchpad lights (numLights: how many)
    // returns tgtLight: bitwise OR of light ID's selected as "targets"
    // period in 10 ms increments, 0=no flash
    // on is duty cycle, 10 ms increments, must be < period
    // colors, period, on: can be [0, 99] each

    bool PlayAudio(unsigned char whichAudio, unsigned char volume);
    // play audio according to specified audio file ID
    // whichAudio: see AUDIO_... constants in this class
    // volume: [0, 99]

    bool PlayTone(unsigned int frequency, unsigned char volume, unsigned char slew);
    // play specified frequency through DL speaker
    //  frequency: in Hz. frequency=0: turn off tone.
    //  volume: [0, 99]
    //  slew: [0, 99]

    bool PresentFoodtreat(unsigned char duration_decisec);
    // presents foodtreat for specified duration, then close tray
    // duration_decisec: specified amount of time (duration x 0.1 secs)
    // duration_decisec = 0: present indefinitely (in this case, must use RetractTray below to retract)

    unsigned char PresentAndCheckFoodtreat(unsigned long duration_ms);
    // must be run in a loop as state machine!
    // presents foodtreat for specified duration (duration_ms in millseconds) and returns whether was eaten
    // returns a PACT_... state as defined in this class
    // advantages: some error checking, returns if food was eaten or not.
    // example:
    //      unsigned char pact_state = dli.PACT_BEFORE_PRESENT;  // dli.PACT_... are states of PresentAndCHeckFoodtreat state machine
    //      while (!(pact_state == dli.PACT_RESPONSE_FOODTREAT_TAKEN || pact_state == dli.PACT_RESPONSE_FOODTREAT_NOT_TAKEN))  // run until one of two possible final states
    //      {
    //          pact_state = dli.PresentAndCheckFoodtreat(1000);  // state machine
    //          dli.Run(200);
    //      }

    bool RetractTray();
    // retract the tray
    // for use with PresentFoodtreat(0)

    bool SetConfigValue(unsigned char configID, int value);
    // sets config value on DL board (see Run function implementation for use)

    bool GetConfigValue(unsigned char configID);
    // gets config value on DL board (see Run function implementation for use)

    bool GetDLInitValues();
    // triggers get / refresh of DL init values (see Run function implementation for use)

    bool SetDLInitValues(int left, int middle, int right, int tray_speed, int tray_current_threshold, int foodtreat_tx_power_level, int foodtreat_detect_threshold); // sets button threshold and RESETS DI BOARD
    // sets dl init values for tray speed, button threshold, tray current threshold and RESETS DI BOARD
    // WARNING: THIS WILL RESET THE DI BOARD - make sure you're not using it when this function is run! Button Lights, etc.
    // (see Run function implementation for use)

    bool GetNeedsDIReset();
    // return value of _csf_needs_DI_reset
    // (see Run function implementation for use)

    bool SetDIResetLock(bool);
    // pass 1 to prevent DI reset, set to 0 to allow it - allows a game to control when dli may reset DI board

    bool ResetDI();
    // WARNING: THIS WILL RESET THE DI BOARD - make sure you're not using it when this function is called! Button Lights, etc.
    // (see Run function implementation for use)

    bool ResetFoodMachine();
    // This will reset the food state machine to hopefully fix a platter in error.

    int GetButtonVal(unsigned char whichButton);
    // returns analog button reading
    // whichButton: see BUTTON_... constants

    unsigned char AnyButtonPressed();
    // returns byte representing bitwise OR of any pressed buttons

    bool IsButtonPressed(unsigned char whichButton);
    // returns bool whether specified button is pressed
    // whichButton: see BUTTON_... constants

    unsigned char AnyButtonSupraThresholdInWindow(unsigned long since);
    // check if any button was suprathreshold since the specfied time
    // since: millis() at a particular time in the past

    bool WasButtonSupraThresholdInWindow(unsigned char whichButton, unsigned long since);
    // check if the specified button was suprathreshold since the specfied time
    // whichButton: see BUTTON_... constants
    // since: millis() at a particular time in the past

    unsigned char FoodmachineState();
    // returns state of food machine: any FOODMACHINE_... values defined in this class

    int GetDomeOpen();
    // returns dome open state
    // -1=dunno 0=closed 1=open

    bool IsDomeRemoved();
    // returns true/false for hub's dome off

    bool UpdateButtonAudioEnabled();
    // update button audio enable state for all buttons (enabled by default)
    // meant to be used after running SetAudioEnabled

    bool SetAudioEnabled(bool audioEnabled);
    // enable/disable button audio

    bool SetButtonAudioEnabled(bool buttonAudioEnabled);
    // enable/disable standard audio when buttons are pushed

    bool SetMaxAudioAmplitude(unsigned char max_audio_amplitude);
    // set max audio amplitude
    // max_audio_amplitude: [0, 99]

    bool SetLightEnabled(bool lightEnabled);
    // enable/disable lights

    bool SetMaxLightAmplitude(unsigned char max_light_amplitude);
    // set max light amplitude
    // max_light_amplitude: [0, 99]

    bool SetDoPollButtons(bool buttonPollingEnable);
    // turn button polling on and off

    bool SetDoPollDiagnostics(bool diagPollingEnable);
    // turn diagnostic polling on and off

    bool SetDoPollIndLight(bool indLightPollingEnable);
    // turn indicator light updating on or off

    bool IsHubOutOfFood();
    // returns true if hub is out of food

    bool IsSingulatorError();
    // returns true if singulator error, for example if singulator jammed

    bool Report(String play_start_time, String player, uint32_t level, String result, uint32_t duration, bool foodtreat_presented, bool foodtreat_eaten);
    // sends a report message with standard fields to the particle cloud. Returns true if successful.

    bool Report(String play_start_time, String player, uint32_t level, String result, uint32_t duration, bool foodtreat_presented, bool foodtreat_eaten, String extra);
    // sends a report message with standard fields and extra field to the particle cloud. Returns true if successful.

//PRIVATE FUNCTIONS
private:
    bool _initialize();

    bool _process_DL();

    bool _process_config_init();

    bool _poll_buttons();
    // poll the status of buttons (includes all values)

    bool _poll_indlight();
    // update the indicator light

    bool _apply_desired_indicator_light(unsigned char ilstate);
    // set indicator light according to ilstate
    
    bool _poll_diag();
    // poll the state of the DL including (importantly) food state machine state, dispense motor active, LEDs active, sound playing, dispense detected

    bool _transmit_cmd(dlimsg_t *cmd);
    // writes the specified command to the device using Serial1. cmd is terminated by CR

    bool _receive_cmd(dlimsg_t *cmd);
    // picks through the serial port and reads a reply from DL if available

    bool _process_reply_from_dl(dlimsg_t *cmd);
    // given a command that is received from DL, process it and set the appropriate internal fields

    bool _parse_msg(unsigned char token, unsigned char rplystatus, char* payload, unsigned short lenPayload);
    // given a token, payload and length of payload, prcess the payload and set the appropriate variables within calss to reflect DL status

    bool _handle_dl_errors();
    //looks at _error_code and calls the appropriate procedures for handling the error

    bool _create_dl_cmd_with(unsigned char token, const char* payload, dlimsg_t *cmd);
    //given a token and payload, creates a dl command in cmd using new

    bool _send_top_cmd();
    // grab the next msg to be send and send it

    bool _process_next_msg();
    // grab the next received msg and process it

    bool _update_button_pressed_state(unsigned char left, unsigned char middle, unsigned char right);
    // keep track of buttons' states across time, only gets 'unpressed' if not above threshold for some amount of debounce time

    void    _update_cap_reset(int left, int middle, int right);

    unsigned char _milliseconds_to_deciseconds_for_DL_T(unsigned long);
    // convert milliseconds unsigned long to deciseconds unsigned char for use with DL API

//PRIVATE STATIC CONSTANTS
private:
    //Initialize DL state machine
    static const unsigned char DLINIT_WAIT_BOOT = 1;
    static const unsigned char DLINIT_SEND = 2;
    static const unsigned char DLINIT_PROCESS = 3;

    //Audio replay state machine
    static const unsigned char ARS_BEFORE_REPLAY = 1;
    static const unsigned char ARS_DURING_REPLAY = 2;

    // STATES FOR CONFIG OF INIT VALUES
    static const unsigned char CONFIG_INIT_BOOTUP = 0;
    static const unsigned char CONFIG_INIT_GET = 1;
    static const unsigned char CONFIG_INIT_WAIT_GET = 2;
    static const unsigned char CONFIG_INIT_SET = 3;
    static const unsigned char CONFIG_INIT_DONE = 4;

    unsigned long _bootup_time;
    unsigned long _config_init_delay = 20000;
    unsigned char _config_init_state = CONFIG_INIT_BOOTUP;

    // Variables related to reporting
    char challenge_id[125] = ""; // Will store a combination of __FILE__, __DATE__, and __TIME__ here

//PRIVATE VARIABLES
private:
    /*THESE VARIABLES ARE TIME IN MILLISECONDS, FOR KEEPING LOG OF WHAT TIME AND EVENT HAPPENED*/
    unsigned long _time_left_button_pressed = 0; // the last time in milliseconds that button press was detected for LEFT:0, middle:1 and right:2.
    unsigned long _time_middle_button_pressed = 0; // the last time in milliseconds that button press was detected for LEFT:0, middle:1 and right:2.
    unsigned long _time_right_button_pressed = 0; // the last time in milliseconds that button press was detected for LEFT:0, middle:1 and right:2.

// PRIVATE VARIABLES RELATED TO INTERNAL FUNCTIONALITY SUCH AS QUEUES etc
private:
    // config init
    bool   _get_config_done                   =    false                       ; // get values complete during init
    int    _num_config_values_recvd           = 0                              ;
    int    _left_from_dl                                                       ;
    int    _middle_from_dl                                                     ;
    int    _right_from_dl                                                      ;
    int    _tray_speed_pwm_from_dl                                             ;
    int    _tray_current_threshold_from_dl                                     ;
    int    _foodtreat_tx_power_level_from_dl                                        ;
    int    _foodtreat_detect_threshold_from_dl                                     ;

    // general
    bool _platter_stuck; // max retries of platter error exceeded. red light.
    int _platter_error_count; // number of platter errors encountered in a row
    unsigned char _current_ilstate = IL_DLI_NULL; // curent indicator lght state
    int _max_platter_error_count = 5;
    queue <dlimsg_t>   _cmd_queue; // this is the command queue to be sent to the DL, the last element in the queue is defined by this pointer
    queue <dlimsg_t>   _dl_reply_queue; // all the message received from DL are stored for future processing
    unsigned short _error_code; // last error code
    char _reply_buffer[MAX_LEN_REPLY_BUFFER]; // temp buffer to receive data from DL
    unsigned short _len_reply_buffer; // size of the reply buffer
    unsigned char _packet_number; // packet sequence number
    unsigned char _run_loop_state; // state in the Run loop
    unsigned char _num_send_retries; // number of retries when sending cmd to DL
    unsigned char _max_num_send_retries; // max number of send retries for a cmd
    unsigned long _start_listen; // start to listen to DL for response
    unsigned long _max_listen_time; // max listen time

    bool _dl_is_ready = false;
    unsigned char _init_dl_state = DLINIT_WAIT_BOOT;
    unsigned long _wait_dl_boot_ms = 3050;; //give the DL app a chance to load from bootloader; needs 3 seconds
    unsigned long _init_dl_start;
    unsigned long _init_dl_process_ms = 300;

    bool _do_poll_diag = false; // whether _poll_diag should run or not
    unsigned long _last_diag_request_ms; // when was the last time that diag check was called
    unsigned long _last_diag_update_ms; // when was the last time that diag was updated
    unsigned long _diag_check_rest_ms; // the rest time between two

    bool _do_poll_buttons = false; // whether _poll_buttons should run or not
    unsigned long _last_btn_poll_ms; // last time buttons states were polled
    unsigned long _diag_btn_poll_rest_ms; // rest betwen button polls

    bool _do_poll_indlight = false; // whether _poll_indlight should run or not
    unsigned long _last_indlight_poll_ms; // last time indicator light was updated
    unsigned long _diag_indlight_rest_ms; // rest between indlight polls

    int	_dome_open_int =	-1; // -1=dunno 0=closed 1=open
    bool _dome_open = false; // is the dome off of the Hub?

    unsigned char _foodmachine_state; // state of food machine
    bool _previous_foodtreat_taken = false; // was the previously presented foodtreat removed from food dish while presented

    bool _hub_out_of_food = false; //keep track of whether the DL thinks there is food or not.
    bool _platter_error = false; //keep track of platter errors.
    bool _singulator_error = false; //keep track of singulator errors.

    bool _need_foodtreat_reset = false; // do we need to reset foodmachine state?

    unsigned char _pact_foodtreat_state = PACT_BEFORE_PRESENT; // state machine variable for present and check foodtreat
    unsigned long _foodtreat_presented_time = 0; // keep track of when the foodtreat was dispensed to keep track of eaten state
    unsigned long _foodtreat_retracted_time = 0; // keep track of when tray was retracted
    unsigned long _pact_platter_return_time = 0; // keep track of when platter back
    bool _indefinite_tray_presentation = false; // keep track of whether tray has been presented with T(0) - will leave tray out indefinitely

    unsigned long _platter_error_start_ms = 0; // keep track of how long platter in error
    unsigned long _platter_error_reset_wait = 10000; // attempt reset of platter after some time

    bool _want_tray_closed = false;

    //audio settings
    bool _audio_enabled = true; //enable/disable audio output
    bool _button_audio_mute = false; // override button audio sounds
    unsigned char _audio_amplitude_max = 99; //max audio amplitude 0-99

    unsigned char _ars_state = ARS_BEFORE_REPLAY; //start out not replaying
    unsigned long _audio_replay_window_start = 0;
    unsigned long _audio_replay_window = 280;

    //lighting settings
    bool _light_enabled = true; //enable/disable light output
    unsigned char _light_amplitude_max = 99; //max light amplitude 0-99

    //keep track of button state
    unsigned long _button_liftoff_ms = 100; // button liftoff time

    unsigned short _left_baseline, _midd_baseline, _right_baseline;
    unsigned short _left_read, _midd_read, _right_read;

    bool _l_button_state = 1; // assume left button pressed until proven otherwise
    bool _m_button_state = 1; // assume middle button pressed until proven otherwise
    bool _r_button_state = 1; // assume right button pressed until proven otherwise

    unsigned long _l_button_timeout = _button_liftoff_ms; //end of liftoff window for left button
    unsigned long _m_button_timeout = _button_liftoff_ms; //end of liftoff window for middle button
    unsigned long _r_button_timeout = _button_liftoff_ms; //end of liftoff window for right button

    bool _button_audio_enabled = true; //play audio when buttons are pressed
    unsigned char _button_audio_amplitude = 50; //amplitude for button audio

    //capsense fix
    unsigned char _csf_detect_integration_left = 0;
    unsigned long _csf_timer_max_left = 0;
    unsigned char _csf_detect_integration_middle = 0;
    unsigned long _csf_timer_max_middle = 0;
    unsigned char _csf_detect_integration_right = 0;
    unsigned long _csf_timer_max_right = 0;

    unsigned char _csf_integration_thresh = 3;
    unsigned long _csf_max_on_duration = 10000;
    float _csf_hysteresis = 0.5;
    bool _csf_DI_reset_locked = false; // allows tmi to lock it during an interaction
    bool _csf_needs_DI_reset = false;
    bool _csf_DI_reset_sent = false;
    unsigned long _csf_last_DI_reset_millis = 0;
    unsigned long _csf_DI_reset_interval = 500000;

//PUBLIC STATIC VARIABLES
public:
    //PresentAndCheckFoodtreat state machine
    static const unsigned char PACT_BEFORE_PRESENT = 10;
    static const unsigned char PACT_PLATTER_OUT = 11;
    static const unsigned char PACT_WAIT_TIL_BACK = 12;
    static const unsigned char PACT_WAIT_DIAG = 13;

    static const unsigned char PACT_RESPONSE_FOODTREAT_NOT_TAKEN = 0;
    static const unsigned char PACT_RESPONSE_FOODTREAT_TAKEN = 1;

    //Foodmachine codes
    static const unsigned char FOODMACHINE_LID_OPEN = 0; // lid open
    static const unsigned char FOODMACHINE_MOVING_HOME = 1; // moving towards home
    static const unsigned char FOODMACHINE_CHECK = 2; // checking for foodtreat in bowl
    static const unsigned char FOODMACHINE_DISPENSING = 3; // dispensing foodtreat
    static const unsigned char FOODMACHINE_IDLE = 4; // waiting with food in bowl
    static const unsigned char FOODMACHINE_MOVING_PRESENT = 5; // moving platter towards present
    static const unsigned char FOODMACHINE_WAIT = 6; // waiting at present for a time
    static const unsigned char FOODMACHINE_MOVING_REMOVE = 7; // moving platter towards remove
    static const unsigned char FOODMACHINE_PLATTER_ERROR_CODE = 8; // platter jammed
    static const unsigned char FOODMACHINE_SINGULATOR_ERROR_CODE = 9; // singulator jammed
    static const unsigned char FOODMACHINE_FOODTREAT_ERROR_CODE = 17; // singluator is empty; API says 10, DL says 17

    //LIGHT CONSTANTS, BITMAP=LMRCXXXX
    static const unsigned char LIGHT_LEFT = 0b00000001;
    static const unsigned char LIGHT_MIDDLE = 0b00000010;
    static const unsigned char LIGHT_RIGHT = 0b00000100;
    static const unsigned char LIGHT_CUE = 0b00001000;
    static const unsigned char LIGHT_BTNS = 0b00000111;
    static const unsigned char LIGHT_ALL = 0b00001111;
    char LightsNum2Token[15];
    //BUTTON CONSTANTS, BITMAP=LMRXXXXX
    static const unsigned char BUTTON_LEFT       = LIGHT_LEFT                        ; //for convenience of blocks
    static const unsigned char BUTTON_MIDDLE     = LIGHT_MIDDLE                      ;
    static const unsigned char BUTTON_RIGHT      = LIGHT_RIGHT                       ;

    static const int LEFT_THRESHOLD   = 29                                ;
    static const int MIDDLE_THRESHOLD = 30                                ;
    static const int RIGHT_THRESHOLD  = 30                                ;
    static const int TRAY_SPEED       = 14                                ;
    static const int TRAY_CURRENT_THRESHOLD = 200                         ;
    static const int FOODTREAT_TX_POWER_LEVEL = 0                              ; //0 = low (default), 1 = medium, 2 = high
    static const int FOODTREAT_DETECT_THRESHOLD = 60                          ; // 60 (default), 40 (addresses empty dish issues)

    // these are constants in the dl firmware:
    static const int PLATTER_MOTOR_MAX_DUTY_CYCLE = 100;
    static const int PLATTER_MOTOR_MAX_PWM_COUNTER = 16;

    static const unsigned char IL_DLI_NULL = 0;
    static const unsigned char IL_DLI_JAM = 1;
    static const unsigned char IL_DLI_OOF = 2;
    static const unsigned char IL_DLI_JAM_ERROR = 3;

    unsigned char IndicatorState = IL_DLI_NULL;

    //STATE OF THE MAIN LOOP
    static const unsigned char STATE_BEFORE_SEND = 1; // in main loop, before sending a command
    static const unsigned char STATE_AFTER_SEND_BEFORE_RCV = 2; // after sending cmd, before rcving rply
    static const unsigned char STATE_AFTER_RCV_BEFORE_PROCESS = 3; // reply received but not processed yet

    static const unsigned char MAX_CMD_SEND_RETRIES = 3; // max # of retries in sending cmd to DL
    //ERROR CODES START AT 1
    static const unsigned short ERROR_CMD_QUEUE_FULL = 1; // command queue is full
    static const unsigned short ERROR_CMD_RECEIVED_BAD_START = 2; // command queue is full
    static const unsigned short ERROR_CMD_RECEIVED_TOO_SHORT = 3; // command queue is full
    static const unsigned short ERROR_CMD_RECEIVED_BAD_NUM_ARGS = 4; // command queue is full
    static const unsigned short ERROR_CMD_RECEIVED_TOO_LONG = 5; // command queue is full

    //AUDIO SLOT IDS
    static const unsigned char AUDIO_ENTICE = 1;
    static const unsigned char AUDIO_POSITIVE = 2;
    static const unsigned char AUDIO_DO = 3;
    static const unsigned char AUDIO_CLICK = 4;
    static const unsigned char AUDIO_SQUEAK = 5;
    static const unsigned char AUDIO_NEGATIVE = 6;
    static const unsigned char AUDIO_L = 7;
    static const unsigned char AUDIO_M = 8;
    static const unsigned char AUDIO_R = 9;

};


#endif
