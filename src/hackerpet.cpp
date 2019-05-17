#include "hackerpet.h"

using namespace std;

#include <algorithm>  // what is this used for??

Logger libLog("app.hackerpet");

/*
                            <<<                             >>>
                            <<<     Default Constructor     s>>>
                            <<<                             >>>


            <<<GOAL>>>
                |   The default constructor.                            |
            <<</GOAL>>>

*/

HubInterface::HubInterface()
{
    _error_code                 = 0         ;// no error at start
    _len_reply_buffer           = 0         ;// indicates the filled length of _reply_buffer
    _diag_check_rest_ms         = 500       ;// do the diag check every _diag_check_rest_ms MS
    _last_diag_request_ms       = 0         ;// when was the last time diag check was called
    _last_diag_update_ms        = 0         ;// when was the last time diag was updated
    _time_left_button_pressed   = 0         ;// the last time when a button was pressed
    _time_middle_button_pressed = 0         ;
    _time_right_button_pressed  = 0         ;
    _packet_number              = 0         ;// packet sequence number
    _diag_btn_poll_rest_ms      = 50        ;// rest in MS between button polls
    _diag_indlight_rest_ms      = 1000      ;// rest in MS between ind light polls
    _last_btn_poll_ms           = 0         ;// last time that buttons were polled
    _run_loop_state             = STATE_BEFORE_SEND ;//before sending command to DL
    _num_send_retries           = 0         ;// retries in sending command to DL
    _max_num_send_retries       = 3         ;// max number of retries for sending a command
    sprintf(LightsNum2Token, "%s", "ABCDEFGHIJKLMNO");
    _max_listen_time            = 20        ;// 5 ms to listen to DL when expecting a reply
    _start_listen               = 0         ;
    _platter_error_count        = 0         ;
    _platter_stuck              = false     ;
    _bootup_time                = millis()  ;
    libLog("HubInterface::HubInterface Constructor finished");
    //REGISTER THE EXPORTED FUNCTIONS
//    Interface::AddInterfaceFunction("SetLightsFlash",&HubInterface::SetLightsFlash);
//    Interface::AddInterfaceFunction("SetLightsSlew",&HubInterface::SetLightsSlew);
}

bool HubInterface::Initialize(char * longFileName){
    Serial1.begin(38400);  // needed for device layer (hub) communication
    ResetDI(); // Reset DI board, just to be sure
    SetDoPollDiagnostics(true); //start polling the diagnostics
    SetDoPollButtons(true); //start polling the touchpads/buttons
    SetDoPollIndLight(true); //start polling the indicator light
    PlayTone(0, 5, 10); // turn off sound
    SetLights(LIGHT_BTNS, 0, 0, 0);  // turn off lights

    // set challenge id for report
    const char * fileName = (strrchr(longFileName, SLASH) + 1); // remove path
    if (fileName[0] == 0) // if our new filename is empty
        fileName = longFileName; // use long version
    sprintf(challenge_id, "%s#%sT%s", fileName , __NICEDATE__, __TIME__ );

    return 1;
}

unsigned char HubInterface::_milliseconds_to_deciseconds_for_DL_T(unsigned long desiredMilliseconds){
    unsigned long desiredDeciseconds = (desiredMilliseconds / 100);
    if (desiredDeciseconds <= 99){
        return (unsigned char)desiredDeciseconds;
    }
    else{
        return 255; //cannot represent desired milliseconds in 0-99 range required by DL API command token 'T'
    }
}

/*
                            <<<                             >>>
                            <<<     SetLights w/ slew       >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   This function sets the specfieid set of buttons     |
                |   to the specified color using slew option.           |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |   whichLights : bit pattern of which lights           |
                |                 to be set. LMRC XXXX.                 |
                |        yellow : yellow color in [0-99]                |
                |          blue : blue color in [0-99]                  |
                |          slew : slew value in [0-99]                  |
                |   RETURN:                                             |
                |           True if successfull, False otherwise        |
            <<</PARAMS>>>
*/
bool HubInterface::SetLights(unsigned char whichLights, unsigned char yellow, unsigned char blue, unsigned char slew)
{

    if (!_light_enabled) {
        // Serial.println("HubInterface::SetLights lights Disabled");
        return false;
    }

    //create a command to set the lights with slew, then put the command into queue
    char    slew_cmd_payload[8];
    sprintf(slew_cmd_payload, "%c%02d%02d%02d", LightsNum2Token[whichLights - 1],
            map(yellow, 0, 99, 0, _light_amplitude_max), map(blue, 0, 99, 0, _light_amplitude_max), slew);
    // Serial.println("HubInterface::SetLights");
    // Serial.println(slew_cmd_payload);
    dlimsg_t cmd;
    if (_create_dl_cmd_with('M', slew_cmd_payload, &cmd)) //if command creation was successfull, add the command to the queue to be sent on later
    {
        // Serial.println("HubInterface::SetLightsslew enqueue cmd");
        _cmd_queue.push(cmd);
        return true;
    }
    // Serial.println("HubInterface::SetLights w/ slew finished");
    return false;
}

bool HubInterface::SetLightsRGB(unsigned char whichLights, unsigned char red, unsigned char green, unsigned char blue, unsigned char slew)
{

    if (!_light_enabled) {
        // Serial.println("HubInterface::SetLightsRGB lights Disabled");
        return false;
    }

    //create a command to set the lights with slew, then put the command into queue
    char    slew_cmd_payload[10];
    sprintf(slew_cmd_payload, "%c%02d%02d%02d%02d", LightsNum2Token[whichLights - 1],
            map(red, 0, 99, 0, _light_amplitude_max), map(green, 0, 99, 0, _light_amplitude_max), map(blue, 0, 99, 0, _light_amplitude_max), slew);
    // Serial.println("HubInterface::SetLightsRGB");
    // Serial.println(slew_cmd_payload);
    dlimsg_t cmd;
    if (_create_dl_cmd_with('I', slew_cmd_payload, &cmd)) //if command creation was successfull, add the command to the queue to be sent on later
    {
        // libLog("HubInterface::SetLightsRGB enqueue cmd");
        _cmd_queue.push(cmd);
        return true;
    }
    // Serial.println("HubInterface::SetLightsRGB w/ slew finished");
    return false;
}


/*
                            <<<                             >>>
                            <<<     SetLights w/ flash      >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   This function sets the specfieid set of buttons     |
                |   to the specified color using the specified period   |
                |   and on durations.                                   |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |   whichLights : bit pattern of which lights           |
                |                 to be set. LMRC XXXX.                 |
                |        yellow : yellow color in [0-99]                |
                |          blue : blue color in [0-99]                  |
                |        period : period of flashing in                 |
                |                 multiples of 0.01 sec                 |
                |            on : the period in which the               |
                |                 lights are on, multiples of 0.01 sec  |
                |   RETURN:                                             |
                |           True if successfull, False otherwise        |
            <<</PARAMS>>>
*/
bool HubInterface::SetLights(unsigned char whichLights, unsigned char yellow, unsigned char blue, unsigned char period, unsigned char on)
{

    if (!_light_enabled) {
        // Serial.println("HubInterface::SetLights lights Disabled");
        return false;
    }

    // Serial.println("HubInterface::SetLights:: Set lights w/ flash");
    //create a command to set the lights with flash, then put the command into queue
    char    flash_cmd_payload[10];
    sprintf(flash_cmd_payload, "%c%02d%02d%02d%02d", LightsNum2Token[whichLights - 1],
            map(yellow, 0, 99, 0, _light_amplitude_max), map(blue, 0, 99, 0, _light_amplitude_max), period, on);
    dlimsg_t cmd;
    if (_create_dl_cmd_with('L', flash_cmd_payload, &cmd)) //if command creation was successfull, add the command to the queue to be sent on later
    {
        libLog("HubInterface::SetLights w/ flash enqueuing command");
        _cmd_queue.push(cmd);
        return true;
    }
    libLog("HubInterface::SetLights w/ flash finished");
    return false;
}
bool HubInterface::SetLightsRGB(unsigned char whichLights, unsigned char red, unsigned char green, unsigned char blue, unsigned char period, unsigned char on)
{

    if (!_light_enabled) {
        // Serial.println("HubInterface::SetLightsRGB lights Disabled");
        return false;
    }

    // Serial.println("HubInterface::SetLights:: Set SetLightsRGB w/ flash");
    //create a command to set the lights with flash, then put the command into queue
    char    flash_cmd_payload[12];
    sprintf(flash_cmd_payload, "%c%02d%02d%02d%02d%02d", LightsNum2Token[whichLights - 1],
            map(red, 0, 99, 0, _light_amplitude_max), map(green, 0, 99, 0, _light_amplitude_max), map(blue, 0, 99, 0, _light_amplitude_max), period, on);
    dlimsg_t cmd;
    if (_create_dl_cmd_with('H', flash_cmd_payload, &cmd)) //if command creation was successfull, add the command to the queue to be sent on later
    {
        libLog("HubInterface::SetLightsRGB w/ flash enqueuing command");
        _cmd_queue.push(cmd);
        return true;
    }
    libLog("HubInterface::SetLightsRGB w/ flash finished");
    return false;
}
/*
                            <<<                             >>>
                            <<<     SetRndLights w/ flash   >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   This function sets the specfieid set of buttons     |
                |   to the specified color using the specified period   |
                |   and on durations.                                   |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |     numLights : number of lights to randomly pick     |
                |      tgtLight : target light, should be on            |
                |        yellow : yellow color in [0-99]                |
                |          blue : blue color in [0-99]                  |
                |        period : period of flashing in                 |
                |                 multiples of 0.01 sec                 |
                |            on : the period in which the               |
                |                 lights are on, multiples of 0.01 sec  |
                |   RETURN:                                             |
                |           True if successfull, False otherwise        |
            <<</PARAMS>>>
*/

unsigned char HubInterface::SetRandomButtonLights(unsigned char numLights, unsigned char yellow, unsigned char blue, unsigned char period, unsigned char on)
{
    unsigned char        tgt_light = 0;
    //add the random lights to tgtLight
    vector<unsigned char> myvector;
    myvector.push_back(1);//left
    myvector.push_back(2);//middle
    myvector.push_back(4);//right

    random_shuffle ( myvector.begin(), myvector.end() );
    for (unsigned char cntr = 0; cntr < numLights; cntr++)
    {
        tgt_light = tgt_light | myvector[cntr];
    }

    if (SetLights(tgt_light, yellow, blue, period, on))
        return tgt_light;
    return 0;
}


bool HubInterface::SetAudioEnabled(bool audioEnabled)
{
    _audio_enabled = audioEnabled;
    return true;
}

bool HubInterface::SetButtonAudioEnabled(bool buttonAudioEnabled)
{
    _button_audio_mute = !buttonAudioEnabled;
    return true;
}

bool HubInterface::SetMaxAudioAmplitude(unsigned char max_audio_amplitude)
{
    _audio_amplitude_max = max_audio_amplitude;
    return true;
}

bool HubInterface::SetLightEnabled(bool lightEnabled)
{
    _light_enabled = lightEnabled;
    return true;
}

bool HubInterface::SetMaxLightAmplitude(unsigned char max_light_amplitude)
{
    _light_amplitude_max = max_light_amplitude;
    return true;
}

/*
                            <<<                             >>>
                            <<<         Play Audio          >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   This function plays the specified audio file on     |
                |   DL. The file is specified using an index number.    |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |    whichAudio : index of the audio file.              |
                |                 always in [1-9]                       |
                |        volume : The volume for playing the audio      |
                |                 file. in [0-99]                       |
                |   RETURN:                                             |
                |           True if successfull, False otherwise        |
            <<</PARAMS>>>
*/


bool HubInterface::PlayAudio(unsigned char whichAudio, unsigned char volume)
{

    if (!_audio_enabled) {
        // Serial.println("HubInterface::PlayAudio Audio Disabled");
        return false;
    }

    //create a command to play audio, then put the command into queue
    char    audio_cmd_payload[4];
    sprintf(audio_cmd_payload, "%d%02d", whichAudio, map(volume, 0, 99, 0, _audio_amplitude_max));
    dlimsg_t cmd;
    if (_create_dl_cmd_with('P', audio_cmd_payload, &cmd)) //if command creation was successful, add the command to the queue to be sent on later
    {
        _cmd_queue.push(cmd);
        return true;
    }
    libLog.error("HubInterface::PlayAudio ERROR: could not push command");
    return false;
}

/*
                            <<<                             >>>
                            <<<         Play Tone           >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   Plays the specified frequency through the DL        |
                |   speaker. This will only stop if 0 is played.        |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |      frequecy : frequency of the audio to             |
                |                 be played. in [00000-99999]           |
                |        volume : The volume for playing the audio      |
                |                 file. in [0-99]                       |
                |          slew : slew for playing the tone. multiplied |
                |                 by 0.1 sec                            |
                |   RETURN:                                             |
                |           True if successfull, False otherwise        |
            <<</PARAMS>>>
*/
bool HubInterface::PlayTone(unsigned int frequecy, unsigned char volume, unsigned char slew)
{
    if (!_audio_enabled) {
        // Serial.println("HubInterface::PlayAudio Audio Disabled");
        return false;
    }

    //create a command to set the lights with flash, then put the command into queue
    char    tone_cmd_payload[9];
    sprintf(tone_cmd_payload, "%02d%05d%1d", map(volume, 0, 99, 0, _audio_amplitude_max), frequecy, slew);
    dlimsg_t cmd;
    if (_create_dl_cmd_with('Q', tone_cmd_payload, &cmd)) //if command creation was successfull, add the command to the queue to be sent on later
    {
        //char msg[64];
        //sprintf(msg,"HubInterface::PlayTone Length of Queue %d",_cmd_queue.size());
        //Serial.println(msg);
        _cmd_queue.push(cmd);
        return true;
    }
    libLog.error("HubInterface::PlayTone ERROR: could not push command");
    return false;
}


/*
                            <<<                             >>>
                            <<<     Present foodtreat           >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   This function presents foodtreat for the specified      |
                |   amount of time which is (duration x 0.1 sec)        |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |      duration : the amount of time (in multiples      |
                |                 of 0.1 sec) to present foodtreat.         |
                |   RETURN:                                             |
                |           True if successfull, False otherwise        |
            <<</PARAMS>>>
*/
bool HubInterface::PresentFoodtreat(unsigned char duration_decisec)
{
    //create a command to present a foodtreat (duration in deciseconds 00-99), then put the command into queue
    char    foodtreat_cmd_payload[2];
    sprintf(foodtreat_cmd_payload, "%02d", duration_decisec);
    dlimsg_t cmd;
    if (_create_dl_cmd_with('T', foodtreat_cmd_payload, &cmd)) //if command creation was successfull, add the command to the queue to be sent on later
    {
        _cmd_queue.push(cmd);
        return true;
    }
    libLog("HubInterface::PresenFoodtreat finished");
    return false;
}

bool HubInterface::RetractTray()
{
    //create a command to retract the tray
    dlimsg_t cmd;
    if (_create_dl_cmd_with('X', "00", &cmd))
    {
        _cmd_queue.push(cmd);
        return true;
    }
    libLog("HubInterface::RetractTray finished");
    return false;
}

unsigned char HubInterface::PresentAndCheckFoodtreat(unsigned long duration_ms)
{
    //This function is written as a reentrant state machine and needs to be handled somewhere within a loop
    // Serial.println("HubInterface::PresentAndCheckFoodtreat PresentFoodtreat duration_ms");
    // Serial.println(duration_ms);

    if ((_foodmachine_state == FOODMACHINE_LID_OPEN) || //lid open
            (_foodmachine_state > FOODMACHINE_WAIT) ) { //some sort of error
        libLog("HubInterface::PresentAndCheckFoodtreat failed. lid open OR _foodmachine_state > FOODMACHINE_WAIT");

        if (_need_foodtreat_reset == true) {
            libLog("resetting foodmachine from HubInterface::PresentAndCheckFoodtreat");
            ResetFoodMachine();
            _need_foodtreat_reset = false;
        }

        return 99;
    }

    switch (_pact_foodtreat_state) {
    case PACT_BEFORE_PRESENT: //wait until food state machine is idle with foodtreat in bowl

        if (_foodmachine_state == FOODMACHINE_IDLE) {
            unsigned char duration_decisec = _milliseconds_to_deciseconds_for_DL_T(duration_ms);
            libLog("HubInterface::PresentAndCheckFoodtreat: PACT_BEFORE_PRESENT duration_decisec: %u", duration_decisec);

            if (duration_decisec >= 99){
                if (PresentFoodtreat(0)) { //this will present tray indefinitely
                    libLog("HubInterface::PresentAndCheckFoodtreat: PACT_BEFORE_PRESENT presenting foodtreat INDEFINITELY ");
                    _indefinite_tray_presentation = true;
                    _foodtreat_presented_time = millis();
                    _pact_foodtreat_state = PACT_PLATTER_OUT;
                }
                else {
                    libLog("HubInterface::PresentAndCheckFoodtreat PresentFoodtreat(0) returned false");
                }
            }
            else{
                if (PresentFoodtreat(duration_decisec)) {
                    _foodtreat_presented_time = millis();
                    _pact_foodtreat_state = PACT_PLATTER_OUT;
                }
                else {
                    libLog("HubInterface::PresentAndCheckFoodtreat PresentFoodtreat(duration_decisec) returned false");
                }
            }
        }
        break;
    case PACT_PLATTER_OUT: //wait here while tray is away from home
        if  (
                (_foodmachine_state == FOODMACHINE_MOVING_HOME) ||      //moving platter towards home
                (_foodmachine_state == FOODMACHINE_CHECK) ||            //checking for foodtreat in bowl
                (_foodmachine_state == FOODMACHINE_MOVING_PRESENT) ||   //moving the platter towards present
                (_foodmachine_state == FOODMACHINE_WAIT)                //waiting at present for a time
            )
        {
            //if we detect any of these states, we know the platter went out and now we need to wait until it's back
            //WARNING: ASSUMPTION: we will catch one of these states with our regular _poll_diag() requests
            _pact_foodtreat_state = PACT_WAIT_TIL_BACK;
            return _pact_foodtreat_state;
        }

        break;
    case PACT_WAIT_TIL_BACK: //wait here until have evidence that tray is back home
        if  (
                (_foodmachine_state == FOODMACHINE_DISPENSING) ||       //dispensing a foodtreat
                (_foodmachine_state == FOODMACHINE_IDLE)                //waiting for a command with a foodtreat in the bowl
            )
        {
            //we now know the foodtreat has been presented and that we are
            //in a foodmachine state after the foodtreat has been checked for and updated by DL
            _foodtreat_retracted_time = 0;
            _want_tray_closed = false;
            _pact_platter_return_time = millis();
            _pact_foodtreat_state = PACT_WAIT_DIAG;
            return _pact_foodtreat_state;
        }

        if (_indefinite_tray_presentation == true){
            if ((_want_tray_closed) || ((millis()-_foodtreat_presented_time) > duration_ms)){
                if (RetractTray()){
                    _foodtreat_retracted_time = millis();
                    _indefinite_tray_presentation = false;
                    libLog("HubInterface::PresentAndCheckFoodtreat retracting tray");
                }
                else{
                    libLog.error("HubInterface::PresentAndCheckFoodtreat ERROR retracting tray");
                }
            }
        }
        else {
            if  (
                    (_foodmachine_state == FOODMACHINE_MOVING_HOME) ||      // moving home
                    (_foodmachine_state == FOODMACHINE_CHECK) ||            // checking for foodtreat
                    (_foodmachine_state == FOODMACHINE_DISPENSING) ||       // dispensing a foodtreat
                    (_foodmachine_state == FOODMACHINE_IDLE)                // waiting for a command with a foodtreat in the bowl
                )
            {
                return _pact_foodtreat_state;
            }
            else{
                if ((_foodtreat_retracted_time != 0)&&((millis()-_foodtreat_retracted_time) > 500)) { // 500 allows some slop in DL communication before raising error
                    libLog.error("HubInterface::PresentAndCheckFoodtreat ERROR - Tray should be on its way back by now!!");
                }
            }
        }
        break;
    case PACT_WAIT_DIAG:
        //this state makes sure we get at least one diag update after platter back,
        //so we're sure we have proper state of _previous Foodtreat eaten
        if (_pact_platter_return_time < _last_diag_update_ms)
        {
            _pact_foodtreat_state = PACT_BEFORE_PRESENT;
            //then check _previous_foodtreat_taken
            if (_previous_foodtreat_taken) {
                // Serial.println("HubInterface::PresentAndCheckFoodtreat return PACT_RESPONSE_FOODTREAT_TAKEN");
                return PACT_RESPONSE_FOODTREAT_TAKEN;
            }
            else {
                // Serial.println("HubInterface::PresentAndCheckFoodtreat return PACT_RESPONSE_FOODTREAT_NOT_TAKEN");
                return PACT_RESPONSE_FOODTREAT_NOT_TAKEN;
            }
        }
        break;
    default:
        libLog("HubInterface::PresentAndCheckFoodtreat got to default: VERY BAD");
        return _pact_foodtreat_state;
        break;
    }

    return _pact_foodtreat_state;
}

    /*
                            <<<                                 >>>
                            <<<    Indicator light update query >>>
                            <<<                                 >>>


            <<<GOAL>>>
                |   This function creates a command to periodically     |
                |   update the indicator light on the hub               |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |      None                                             |
                |   RETURN:                                             |
                |           True if successful, False otherwise         |
            <<</PARAMS>>>
*/
bool HubInterface::SetDoPollIndLight(bool indLightPollingEnable) {
    _do_poll_indlight = indLightPollingEnable;
    return true;
}

bool HubInterface::_poll_indlight()
{
    _apply_desired_indicator_light(IndicatorState);
}

bool HubInterface::_apply_desired_indicator_light(unsigned char ilstate)
{
    if (ilstate == _current_ilstate) {
        return true; //don't update ilstate to same thing
    }

    // _logger->Log("DeviceLayerInterface::_apply_desired_indicator_light ilstate: ", Logger::LOG_LEVEL_DEBUG);
    // _logger->Log(ilstate, Logger::LOG_LEVEL_DEBUG);

    switch (ilstate) {
    case IL_DLI_NULL:
        SetLights(LIGHT_CUE, 0, 0, 99);                   // off
        break;
    case IL_DLI_OOF:
        SetLightsRGB(LIGHT_CUE, 10, 0, 0, 99, 5);        // red slow blink         out of food
        break;
    case IL_DLI_JAM:
        SetLightsRGB(LIGHT_CUE, 80, 35, 0, 50, 30);       // yellow fast blink      Tray Jam
        break;
    case IL_DLI_JAM_ERROR:
        SetLightsRGB(LIGHT_CUE, 80, 0, 0, 99);            // red medium constant    Tray Jam: Stuck
        break;
    default:
        break;
    }
    _current_ilstate = ilstate;
    return true;

}

    /*
                            <<<                             >>>
                            <<<    Buttons status query     >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   This function creates a command to query the        |
                |   status of buttons on the device.                    |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |      None                                             |
                |   RETURN:                                             |
                |           True if successful, False otherwise         |
            <<</PARAMS>>>
*/
bool HubInterface::SetDoPollButtons(bool buttonPollingEnable) {
    _do_poll_buttons = buttonPollingEnable;
    return true;
}

bool HubInterface::_poll_buttons()
{
    dlimsg_t cmd;
    //if command creation was successfull, add the command to the queue to be sent on later
    if (_create_dl_cmd_with('B', "", &cmd))
    {
        _cmd_queue.push(cmd);
        return true;
    }
    libLog("HubInterface::_poll_buttons finished");
    return false;
}

int HubInterface::GetButtonVal(unsigned char whichButton){
    if (whichButton == BUTTON_LEFT){
        return ((int)_left_baseline) - ((int)_left_read);
    }
    if (whichButton == BUTTON_MIDDLE){
        return ((int)_midd_baseline) - ((int)_midd_read);
    }
    if (whichButton == BUTTON_RIGHT){
        return ((int)_right_baseline) - ((int)_right_read);
    }
}

/*
                            <<<                             >>>
                            <<<       Check Any Buttons     >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   This function checks if any button has been pressed |
                |   in the specified window of time.                    |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |     sinceWhen : looks back in time in MS and return   |
                |                 true if buttons were pressed          |
                |   RETURN:                                             |
                |           True if button pressed, False otherwise     |
            <<</PARAMS>>>
*/
bool HubInterface::_update_button_pressed_state(unsigned char left, unsigned char middle, unsigned char right)
{
    // Serial.println("HubInterface::_update_button_pressed_state::entering");
    // Serial.println("L M R incoming");
    // Serial.println(left);
    // Serial.println(middle);
    // Serial.println(right);

    // Serial.println("L M R button_state");
    // Serial.println(_l_button_state);
    // Serial.println(_m_button_state);
    // Serial.println(_r_button_state);

    if (left) {
        _l_button_timeout = millis() + _button_liftoff_ms;
        if (_l_button_state == false) {
            _l_button_state = true;
            if (_button_audio_enabled == true) {
                PlayAudio(AUDIO_L, _button_audio_amplitude);
            }
        }
    }
    else {
        if (millis() > _l_button_timeout) {
            _l_button_state = false;
        }
    }

    if (middle) {
        _m_button_timeout = millis() + _button_liftoff_ms;
        if (_m_button_state == false) {
            _m_button_state = true;
            if (_button_audio_enabled == true) {
                PlayAudio(AUDIO_M, _button_audio_amplitude);
            }
        }
    }
    else {
        if (millis() > _m_button_timeout) {
            _m_button_state = false;
        }
    }

    if (right) {
        _r_button_timeout = millis() + _button_liftoff_ms;
        if (_r_button_state == false) {
            _r_button_state = true;
            if (_button_audio_enabled == true) {
                PlayAudio(AUDIO_R, _button_audio_amplitude);
            }
        }
    }
    else {
        if (millis() > _r_button_timeout) {
            _r_button_state = false;
        }
    }
    return true;
}


unsigned char HubInterface::AnyButtonPressed()
{
    unsigned char pressed            = 0;

    pressed = _l_button_state ? pressed | BUTTON_LEFT   : pressed;
    pressed = _m_button_state ? pressed | BUTTON_MIDDLE : pressed;
    pressed = _r_button_state ? pressed | BUTTON_RIGHT  : pressed;

    // Serial.println("dli::AnyButtonPressed finished");
    // Serial.println(pressed);
    return pressed;
}

unsigned char HubInterface::AnyButtonSupraThresholdInWindow(unsigned long sinceWhen)
{
    unsigned char pressed            = 0;
    unsigned long   now             = millis();
    unsigned long   window_start    = now > sinceWhen ? now - sinceWhen : 0;
    //for each button if it was suprathreshold within time window
    if (window_start > 0)
    {
        pressed = window_start <= _time_left_button_pressed    ? pressed | BUTTON_LEFT   : pressed;
        pressed = window_start <= _time_middle_button_pressed  ? pressed | BUTTON_MIDDLE : pressed;
        pressed = window_start <= _time_right_button_pressed   ? pressed | BUTTON_RIGHT  : pressed;
    }
    // Serial.println("dli::AnyButtonSupraThresholdInWindow finished");
    // Serial.println(pressed);
    return pressed;
}


/*
                            <<<                             >>>
                            <<<       Check the Buttons     >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   This function checks if the specified buttons       |
                |   have been pressed since the specified time.         |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |   whichButton : indicates the buttons for which       |
                |                 button press will be checked.         |
                |     sinceWhen : looks back in time in MS and return   |
                |                 true if buttons were pressed          |
                |   RETURN:                                             |
                |           True if button pressed, False otherwise     |
            <<</PARAMS>>>
*/
bool HubInterface::IsButtonPressed(unsigned char whichButton)
{
    unsigned char pressed            = false;

    //for each button if it was pressed within time window
    if ( (whichButton & BUTTON_LEFT) == BUTTON_LEFT)
        pressed = pressed || _l_button_state;
    if ( (whichButton & BUTTON_MIDDLE) == BUTTON_MIDDLE)
        pressed = pressed || _m_button_state;
    if ( (whichButton & BUTTON_RIGHT) == BUTTON_RIGHT)
        pressed = pressed || _r_button_state;

    libLog("HubInterface::IsButtonPressed finished");

    // char    buff[8];
    // sprintf(buff, "prsd?%d", pressed);
    // Serial.println(buff);
    return pressed;
}

bool HubInterface::WasButtonSupraThresholdInWindow(unsigned char whichButton, unsigned long sinceWhen)
{
    unsigned char pressed            = false;
    unsigned long   now             = millis();
    unsigned long   window_start    = now > sinceWhen ? now - sinceWhen : 0;
    //for each button if it was suprathreshold within time window
    if ( (whichButton & BUTTON_LEFT) == BUTTON_LEFT)
        pressed = pressed || (window_start <= _time_left_button_pressed) ;
    if ( (whichButton & BUTTON_MIDDLE) == BUTTON_MIDDLE)
        pressed = pressed || (window_start <= _time_middle_button_pressed) ;
    if ( (whichButton & BUTTON_RIGHT) == BUTTON_RIGHT)
        pressed = pressed || (window_start <= _time_right_button_pressed) ;
    libLog("HubInterface::WasButtonSupraThresholdInWindow finished");
    // char    buff[8];
    // sprintf(buff, "prsd?%d", pressed);
    // Serial.println(buff);
    return pressed;
}

void HubInterface::_update_cap_reset(int left, int middle, int right) {

    if (_csf_needs_DI_reset == true) {
        libLog("dli::_update_cap_reset: DI NEEDS RESET");
        return; //wait until DI gets reset
    }

    // Serial.printlnf("LEFT:\tV: %d\tD: %d\tT: %lu\t\tMIDDLE:\tV: %d\tD: %d\tT: %lu\t\tRIGHT:\tV: %d\tD: %d\tT: %lu",
    //     left,_csf_detect_integration_left,millis()-_csf_timer_max_left,
    //     middle,_csf_detect_integration_middle,millis()-_csf_timer_max_middle,
    //     right,_csf_detect_integration_right,millis()-_csf_timer_max_right
    //     );


//DO LEFT
    if (_csf_detect_integration_left >= _csf_integration_thresh) {
        if (left >  (int)(LEFT_THRESHOLD * _csf_hysteresis)) {
            // Serial.printlnf("Tval: %d  bool: %d",(int)(LEFT_THRESHOLD * _csf_hysteresis),(left >  (int)(LEFT_THRESHOLD * _csf_hysteresis)));
            if ((millis() - _csf_timer_max_left) > _csf_max_on_duration) {
                libLog("dli::_update_cap_reset: DI RESET NEEDED: LEFT");
                _csf_needs_DI_reset = true;
                _csf_detect_integration_left = 0;
                _csf_timer_max_left = millis();
                return;
            }
        }
        else {
            _csf_detect_integration_left = 0;
            _csf_timer_max_left = millis();
        }
    }
    else if (left > LEFT_THRESHOLD) {
        _csf_detect_integration_left++;
    }
    else {
        _csf_detect_integration_left = 0;
        _csf_timer_max_left = millis();
    }
//DO MIDDLE
    if (_csf_detect_integration_middle >= _csf_integration_thresh) {
        if (middle >  (int)(MIDDLE_THRESHOLD * _csf_hysteresis)) {
            if ((millis() - _csf_timer_max_middle) > _csf_max_on_duration) {
                libLog("dli::_update_cap_reset: DI RESET NEEDED: MIDDLE");
                _csf_needs_DI_reset = true;
                _csf_detect_integration_middle = 0;
                _csf_timer_max_middle = millis();
                return;
            }
        }
        else {
            _csf_detect_integration_middle = 0;
            _csf_timer_max_middle = millis();
        }
    }
    else if (middle > MIDDLE_THRESHOLD) {
        _csf_detect_integration_middle++;
    }
    else {
        _csf_detect_integration_middle = 0;
        _csf_timer_max_middle = millis();
    }
//DO RIGHT
    if (_csf_detect_integration_right >= _csf_integration_thresh) {
        if (right >  (int)(RIGHT_THRESHOLD * _csf_hysteresis)) {
            if ((millis() - _csf_timer_max_right) > _csf_max_on_duration) {
                libLog("dli::_update_cap_reset: DI RESET NEEDED: RIGHT");
                _csf_needs_DI_reset = true;
                _csf_detect_integration_right = 0;
                _csf_timer_max_right = millis();
                return;
            }
        }
        else {
            _csf_detect_integration_right = 0;
            _csf_timer_max_right = millis();
        }
    }
    else if (right > RIGHT_THRESHOLD) {
        _csf_detect_integration_right++;
    }
    else {
        _csf_detect_integration_right = 0;
        _csf_timer_max_right = millis();
    }

    return;
}

bool HubInterface::SetDLInitValues(int left, int middle, int right, int tray_speed, int tray_current_threshold, int foodtreat_tx_power_level, int foodtreat_detect_threshold)
{

    //WARNING: THIS WILL RESET THE DI BOARD - make sure you're not using it! Button Lights, etc.
    if ((left > 255) || (middle > 255) || (right > 255)) {
        libLog("HubInterface::SetDLInitValues threshold values must be between 0-255");
        return false;
    }

    if (tray_speed > 16) {
        libLog("HubInterface::SetDLInitValues tray_speed must be between 0-16");
        return false;
    }

    SetConfigValue(21, left);
    SetConfigValue(22, middle);
    SetConfigValue(23, right);
    SetConfigValue(11, tray_speed * PLATTER_MOTOR_MAX_DUTY_CYCLE / PLATTER_MOTOR_MAX_PWM_COUNTER);
    SetConfigValue(8, tray_current_threshold);
    SetConfigValue(18, foodtreat_tx_power_level);
    SetConfigValue(20, foodtreat_detect_threshold);


    ResetDI();

    libLog("HubInterface::SetDLInitValues finished");

    return true;
}

bool HubInterface::GetDLInitValues()
{
    GetConfigValue(21);
    GetConfigValue(22);
    GetConfigValue(23);
    GetConfigValue(11);
    GetConfigValue(8);
    GetConfigValue(18);
    GetConfigValue(20);
}

bool HubInterface::GetConfigValue(unsigned char configID)
{
    char    config_payload[3];
    sprintf(config_payload, "%02d", configID);
    dlimsg_t cmd;
    if (_create_dl_cmd_with('U', config_payload, &cmd)) //if command creation was successful, add the command to the queue to be sent on later
    {
        _cmd_queue.push(cmd);
        return true;
    }
    libLog.error("HubInterface::GetConfigValue ERROR: could not push command");
    return false;
}

bool HubInterface::SetConfigValue(unsigned char configID, int value)
{

    char    config_payload[8];
    sprintf(config_payload, "%02d%05d", configID, value);
    dlimsg_t cmd;
    if (_create_dl_cmd_with('N', config_payload, &cmd)) //if command creation was successful, add the command to the queue to be sent on later
    {
        _cmd_queue.push(cmd);
        return true;
    }
    libLog.error("HubInterface::SetConfigValue ERROR: could not push command");
    return false;
}
//
bool HubInterface::GetNeedsDIReset(){
    return _csf_needs_DI_reset;
}
//
bool HubInterface::SetDIResetLock(bool desired_lock_state)
{
    _csf_DI_reset_locked = desired_lock_state;
    return true;
}
//
bool HubInterface::ResetDI()
{
    bool reset_was_sent = false;
    if (_csf_DI_reset_locked == false) {
        libLog("HubInterface::ResetDI Resetting DI");
        dlimsg_t cmd;
        //if command creation was successfull, add the command to the queue to be sent on later
        if (_create_dl_cmd_with('K', "", &cmd))
        {
            _cmd_queue.push(cmd);
            reset_was_sent = true;
            _dl_is_ready = false;
        }
    }
    else {
        // Serial.println("HubInterface::ResetDI - NOT resetting DI - LOCKED");
    }
    libLog("HubInterface::ResetDI finished");
    return reset_was_sent;
}

bool HubInterface::ResetFoodMachine()
{
    dlimsg_t cmd;

    //if command creation was successfull, add the command to the queue to be sent on later
    if (_create_dl_cmd_with('F', "", &cmd))
    {
        _cmd_queue.push(cmd);
        libLog("HubInterface::ResetFoodMachine sent command to DL");
        _need_foodtreat_reset = false;
        return true;
    }
    libLog("HubInterface::ResetFoodMachine was unable to send command");
    return false;
}

unsigned char HubInterface::FoodmachineState()
{
    //Serial.println("HubInterface::FoodmachineState finished");
    return (_foodmachine_state);
}

/*
                            <<<                             >>>
                            <<<         CAP status          >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   Checks if device was detected as 'on' in the        |
                |   specified time window.                              |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |     sinceWhen : looks back in time in MS and return   |
                |                 true if cap was present since then    |
                |   RETURN:                                             |
                |           True if cap was on, False otherwise         |
            <<</PARAMS>>>
*/
int HubInterface::GetDomeOpen()
{
    return _dome_open_int;
}

bool HubInterface::IsDomeRemoved()
{
    libLog("HubInterface::IsDomeRemoved finished");
    return _dome_open;
}

/*
                            <<<                             >>>
                            <<<         poll DL state       >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   polls the status of device and fills the app.       |
                |   time variables for present events. The diag info    |
                |   includes Dispense motor active, present motor       |
                |   active, LEDs active, sound playing, dispense        |
                |   detect, foodtreat detect, foodtreat state machine and       |
                |   lid open state.                                     |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |           None                                        |
                |   RETURN:                                             |
                |           True if no error happens, False otherwise   |
            <<</PARAMS>>>
*/
bool HubInterface::SetDoPollDiagnostics(bool diagPollingEnable) {
    _do_poll_diag = diagPollingEnable;
    return true;
}

bool HubInterface::_poll_diag()
{
    dlimsg_t cmd;
    //if command creation was successfull, add the command to the queue to be sent on later
    if (_create_dl_cmd_with('Z', "00", &cmd))
    {
        _cmd_queue.push(cmd);
        return true;
    }
    libLog("HubInterface::_poll_diag finished");
    return false;
}



/*
                            <<<                             >>>
                            <<<     transmit cmd to DL      >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   This function writes the command down to DL using   |
                |   the serial port dedicated to DL (Serial1). If       |
                |   any errors happened during transmission, the        |
                |   appropriate flags are set for exception handling    |
                |   rountine.                                           |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |      cmd: a char array to be sent usnig Serial1.      |
                |   RETURN:                                             |
                |           True if no error happens, False otherwise   |
            <<</PARAMS>>>
*/
bool HubInterface::_transmit_cmd(dlimsg_t *cmd)
{
    //check _cmd format for mis-spells?
    unsigned short        len_sent;
    //len_sent    =   Serial1.write(cmd,strlen(cmd));
    // Serial.println("HubInterface::_transmit_cmd:: sending message:");
    //Serial.println(cmd);
    len_sent    =   Serial1.write((*cmd).buf);
    Serial1.flush();
    // Serial.printlnf("Sent DL %d unsigned chars of cmd: %s",len_sent,(*cmd).buf);
    // Serial.println("HubInterface::_send_cmd finished");
    // Serial.println("dli _send_cmd sent unsigned chars:");
    // Serial.println(len_sent);
    // Serial.println("command length was:");
    // Serial.println(strlen(cmd));
    return (strlen((*cmd).buf) == len_sent);
}


/*
                            <<<                             >>>
                            <<<        received command     >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   This function picks at the serial1 port and         |
                |   reads a reply from DL if something is available     |
                |   to receive from DL.                                 |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |      cmd: the received message will be copied         |
                |           into heap and cmd will point to that.       |
                |   RETURN:                                             |
                |           True  if full message received, False o.w.  |
            <<</PARAMS>>>
*/
bool HubInterface::_receive_cmd(dlimsg_t *cmd)
{
    // read the unsigned chars until an end delimiter is reached. Then create a new buffer and return the message
    // Serial.println("HubInterface::_receive_cmd:: buffer length is");
    // Serial.println(_len_reply_buffer);
    // Serial.println("send message queue length is");
    // Serial.println(_cmd_queue.size());
    while (Serial1.available() > 0)
    {
        if (_len_reply_buffer >= MAX_LEN_REPLY_BUFFER - 1)
        {
            _error_code     = ERROR_CMD_RECEIVED_TOO_LONG;
            libLog("dli::_receive_cmd received msg exceeds buffer length");
            return false;
        }
        // Serial.println("In receive cmd: data available, current buff size:");
        // Serial.println(_len_reply_buffer);
        _reply_buffer[_len_reply_buffer] = Serial1.read();
        // Serial.println(_reply_buffer[_len_reply_buffer]);
        _len_reply_buffer ++;
        if ( _reply_buffer[_len_reply_buffer - 1] == '.')
            break;
    }
    if ((_len_reply_buffer > 0) && (_reply_buffer[_len_reply_buffer - 1] == '.') )
    {
        _reply_buffer[_len_reply_buffer++]  = STR_CARRIAGE_RETURN;//end the command with CR
        strcpy((*cmd).buf, _reply_buffer);
        // Serial.println("dli _receive_cmd message received");
        // Serial.println(cmd);
        _len_reply_buffer                   = 0;//reset the _reply_buffer length
        return true;
    }
    // Serial.println("HubInterface::_receive_cmd finished with false");
    return false;
}

/*
                            <<<                             >>>
                            <<< SEND THE CMD FIRST IN QUEUE >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   Tries to send a command (if any) from the head of   |
                |   outgoing queue. If there are no messages or sending |
                |   fails, it returns false.                            |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |     None                                              |
                |   RETURN:                                             |
                |           True if a msg was sent successfully         |
                |           False otherwise                             |
            <<</PARAMS>>>
*/
bool HubInterface::_send_top_cmd()
{
    dlimsg_t cmd;
    bool        rslt;
    if (_cmd_queue.size() > 0)
    {
        //Serial.println("commands to send");
        cmd = _cmd_queue.front();
        // Serial.println("dli sending cmd");
        // Serial.println(cmd);
        if (_transmit_cmd(&cmd)) //if successfully transmitted the command, remove it from the queue
        {
            //Serial.println("sending cmd, going to before rcv");
            rslt = true;
        }
        else
        {
            libLog.error("dli error sending top cmd failed");
            rslt = false;
            _num_send_retries ++;
        }
    }
    else
        rslt        = false;

    return rslt;
}

bool HubInterface::IsReady() {
    return _dl_is_ready;
}

bool HubInterface::_initialize() {

    switch (_init_dl_state) {
    case DLINIT_WAIT_BOOT:
    {
        _dl_is_ready = false;
        if (millis() > _wait_dl_boot_ms) {
            _init_dl_state = DLINIT_SEND;
        }
        break;
    }
    case DLINIT_SEND:
    {
        SetLights(LIGHT_ALL, 0, 0, 0);
        PlayTone(1000, 0, 2);
        RetractTray();
        _init_dl_start = millis();
        _init_dl_state = DLINIT_PROCESS;
        break;
    }
    case DLINIT_PROCESS:
    {
        if (millis() > (_init_dl_start + _init_dl_process_ms)) {
            _init_dl_state = DLINIT_WAIT_BOOT;
            _dl_is_ready = true;
        }
        _process_DL();
        break;
    }
    }
    return _dl_is_ready;
}

bool HubInterface::_process_DL() {
    dlimsg_t reply;

    if (_run_loop_state == STATE_BEFORE_SEND)
    {
        if (_send_top_cmd())
        {
            _run_loop_state     = STATE_AFTER_SEND_BEFORE_RCV; // change state to before rcv reply from DL
            _start_listen       = millis();
            _len_reply_buffer   = 0;
        }
    }
    //check to receive anything from device, if a full reply is received, process it
    else if (_run_loop_state == STATE_AFTER_SEND_BEFORE_RCV)
    {
        //_receive_cmd returns true only if memory allocated to reply
        if (_receive_cmd(&reply)) //if a full reply received from DL, enqueue it for further processing
        {
            _dl_reply_queue.push(reply);
            _run_loop_state = STATE_AFTER_RCV_BEFORE_PROCESS;
            // Serial.println("dli response received");
            // Serial.println(reply);
        }
        else if ((millis() - _start_listen) > _max_listen_time) // if listen timed out, go back to sending
        {
            libLog("listening for response from DL failed: %u", _reply_buffer);
            _num_send_retries ++;
            _run_loop_state = STATE_BEFORE_SEND;
            _len_reply_buffer = 0;//reset the incoming buffer length
        }

    }

    //process the received messages if any is available
    else if ( _run_loop_state == STATE_AFTER_RCV_BEFORE_PROCESS)
    {
        if (!_process_next_msg()) //if msg parsed successfully, delete the send command too
        {
            libLog("dli Processing next resp failed, moving on...");
        }
        _cmd_queue.pop(); // remove the cmd from queue, move to the next command
        _num_send_retries   = 0;
        _run_loop_state = STATE_BEFORE_SEND;
    }
    if (_num_send_retries >= _max_num_send_retries)
    {
        libLog("max num retries reached, deleting command");
        _cmd_queue.pop(); // remove the cmd from queue, move to the next command
        _num_send_retries   = 0;
    }
    return true;
}
/*
                            <<<                             >>>
                            <<<       RUN function          >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   The main run funciton. This function will gain      |
                |   cpu cycles and will use it to update the deivce     |
                |   status, to communicate with the DL and to receive   |
                |   message from it. check operations or things that    |
                |   change something on the device has to put their     |
                |   request on a queue, which will be served by the     |
                |   Run function below.                                 |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |     None                                              |
                |   RETURN:                                             |
                |           True                                        |
            <<</PARAMS>>>
*/

bool HubInterface::_process_config_init()
{
    switch (_config_init_state) {
        case CONFIG_INIT_BOOTUP:
            if (millis() > _bootup_time + _config_init_delay)
            {
                _config_init_state = CONFIG_INIT_GET;
            }
            break;
        case CONFIG_INIT_GET:
            GetDLInitValues();
            _config_init_state = CONFIG_INIT_WAIT_GET;
            break;
        case CONFIG_INIT_WAIT_GET:
            if (_get_config_done)
            {
                bool match = (_left_from_dl == LEFT_THRESHOLD);
                match = match && (_right_from_dl == RIGHT_THRESHOLD);
                match = match && (_middle_from_dl == MIDDLE_THRESHOLD);
                match = match && (_tray_speed_pwm_from_dl == TRAY_SPEED * PLATTER_MOTOR_MAX_DUTY_CYCLE / PLATTER_MOTOR_MAX_PWM_COUNTER);
                match = match && (_tray_current_threshold_from_dl == TRAY_CURRENT_THRESHOLD);
                match = match && (_foodtreat_tx_power_level_from_dl == FOODTREAT_TX_POWER_LEVEL);
                match = match && (_foodtreat_detect_threshold_from_dl == FOODTREAT_DETECT_THRESHOLD);
                if (match)
                {
                    _config_init_state = CONFIG_INIT_DONE;
                    // libLog("HubInterface::_process_config_init: NOT setting config init values...");
                }
                else
                {
                    _config_init_state = CONFIG_INIT_SET;
                    // libLog("HubInterface::_process_config_init: Setting config init values...");
                }
            }
            break;
        case CONFIG_INIT_SET:
            SetDLInitValues(LEFT_THRESHOLD, MIDDLE_THRESHOLD, RIGHT_THRESHOLD, TRAY_SPEED, TRAY_CURRENT_THRESHOLD, FOODTREAT_TX_POWER_LEVEL, FOODTREAT_DETECT_THRESHOLD);
            _config_init_state = CONFIG_INIT_DONE;
            break;
        case CONFIG_INIT_DONE:
            libLog("HubInterface::_process_config_init: Done.");
            break;
        default:
            libLog.error("dli::_process_config_init Error! Invalid state!");
            break;
    }
}

bool HubInterface::UpdateButtonAudioEnabled()
{
    // function must consider:
    //  is hub out of food
    //  is platter error true
    //  is singulator error true

   if (!_button_audio_mute && !_hub_out_of_food && !_singulator_error && !_platter_stuck)
   {
       _button_audio_enabled = true;
   }
   else
   {
       _button_audio_enabled = false;
   }

   return true;
}


bool HubInterface::Run(unsigned long forHowLong)
{

    unsigned long start = millis();
    while (millis() - start < forHowLong)
    {
        if (!_dl_is_ready) {
            _initialize();
        }
        else {

            if (_csf_needs_DI_reset) {
                if (_csf_DI_reset_sent == false) {
                    if (ResetDI()) {
                        _csf_DI_reset_sent = true;
                    }
                }
            }
            else if ((millis() - _csf_last_DI_reset_millis) > _csf_DI_reset_interval) {
                _csf_needs_DI_reset = true;
            }

            if(_config_init_state < CONFIG_INIT_DONE)
            {
                _process_config_init();
            }

            _process_DL();

            //maybe do some polling about the device layer, with a pre-specified frequency
            if (millis() - _last_diag_request_ms > _diag_check_rest_ms)
            {
                if (_do_poll_diag == true) {
                    _poll_diag();
                    _last_diag_request_ms = millis();
                }
            }
            //poll the state of buttons with some specified frequency
            if (millis() - _last_btn_poll_ms > _diag_btn_poll_rest_ms)
            {
                if (_do_poll_buttons == true) {
                    _poll_buttons();
                    _last_btn_poll_ms = millis();
                }
            }
            //update the indicator light
            if (millis() - _last_indlight_poll_ms > _diag_indlight_rest_ms)
            {
                if (_do_poll_indlight == true) {
                    _poll_indlight();
                    _last_indlight_poll_ms = millis();
                }
            }

            //do the error processing here
            _handle_dl_errors();
        }
    }
    return true;
}

bool HubInterface::_process_next_msg()
{
    bool    rslt;
    dlimsg_t cmd;
    if (_dl_reply_queue.size() > 0)
    {
        //Serial.println("grabbing next received msg for process");
        cmd = _dl_reply_queue.front();
        //Serial.println("in process next msg:");
        //Serial.println(cmd);
        if (_process_reply_from_dl(&cmd)) // if successfully process the received message, pop it from the queue
        {
            rslt = true;
            //Serial.println("process next msg succeed, going to before send");
        }
        else
        {
            rslt = false;
            libLog.error("dli Error! process next msg failed");
            _num_send_retries++;
        }
        _dl_reply_queue.pop();
    }
    else
        rslt = false;
    return rslt;
}

/*
                            <<<                             >>>
                            <<<   Process message from DL   >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   Given a message received from DL, this function     |
                |   processes the message and calls another function    |
                |   to parse the message contents.                      |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |       cmd : the command received from DL              |
                |   RETURN:                                             |
                |           True if button pressed, False otherwise     |
            <<</PARAMS>>>
*/
bool HubInterface::_process_reply_from_dl(dlimsg_t *cmd)
{
    // Serial.println("HubInterface::_process_reply_from_dl:: Processing received message");
    // Serial.println(_cmd_queue.size());
    // Serial.println(_dl_reply_queue.size());
    if (strlen((*cmd).buf) < 7) //received message is too short
    {
        _error_code = ERROR_CMD_RECEIVED_TOO_SHORT;
        return false;
    }
    unsigned char    token = (*cmd).buf[5];
    unsigned char    rplystatus = (*cmd).buf[6];
    char*   payload = &(*cmd).buf[7];
    unsigned short    len_payload = 100 * ((*cmd).buf[1] - 48) + 10 * ((*cmd).buf[2] - 48) + ((*cmd).buf[3] - 48);
    //Serial.println("HubInterface::_process_reply_from_dl finished");
    return _parse_msg(token, rplystatus, payload, len_payload); //if the message is parsed with no problem, return true
}

/*
                            <<<                             >>>
                            <<<   parse the msg from DL     >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   Given a token and the corresponding payload, this   |
                |   function parses the payload and extracts the        |
                |   information      |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |     token : One unsigned char indicating the command token     |
                |   payload : char array, the payload of the message    |
                |lenPayload : length of payload                         |
                |   RETURN:                                             |
                |             True if message parsed succesfully,       |
                |             False otherwise                           |
            <<</PARAMS>>>
*/

bool HubInterface::_parse_msg(unsigned char token, unsigned char rplystatus, char* payload, unsigned short lenPayload)
{
    unsigned char    num_parsed;//number of elements parsed from the payload
    unsigned char    left, middle, right, cue;

    rplystatus -= 48; //convert to number
    if (rplystatus != 1) {
        libLog.error("HubInterface::_parse_msg message:: ERROR - received non-success reply token: %u", rplystatus);
    }

    switch (token) {
    case 'M'://OK packet for setting BY lights slew
        //Serial.println("HubInterface::_parse_msg message:: M");
        break;
    case 'I'://OK packet for setting RGB lights slew
        //Serial.println("HubInterface::_parse_msg message:: I");
        break;
    case 'B'://button summary + base line and reading for each button
        // Serial.println("HubInterface::_parse_msg message:: B");
        // Serial.println(millis());
        // Serial.println(payload);
        // unsigned short    left_baseline, midd_baseline, right_baseline;
        // unsigned short    left_read, midd_read, right_read;

        num_parsed = sscanf(payload, "%1c%1c%1c%3hu%3hu%3hu%3hu%3hu%3hu.",
                            &left, &middle, &right,
                            &_left_baseline, &_midd_baseline, &_right_baseline,
                            &_left_read, &_midd_read, &_right_read);
        if (num_parsed != 9) //check number of arguments in the payload
        {
            _error_code = ERROR_CMD_RECEIVED_BAD_NUM_ARGS;
            return false;
        }
        //convert from ascii to number
        left    -= 48;
        right   -= 48;
        middle  -= 48;
        // char    msg[32];
        // sprintf(msg, "%1d%1d%1d", left, middle, right);
        // sprintf(msg, "%1d %3d %3d\t%1d %3d %3d\t%1d %3d %3d", left, _left_baseline, _left_read, middle, _midd_baseline, _midd_read, right, _right_baseline, _right_read);
        // Serial.println(msg);

        //update the time of button press if any detected
        _time_left_button_pressed   = left  ? millis()  : _time_left_button_pressed;
        _time_middle_button_pressed = middle ? millis()  : _time_middle_button_pressed;
        _time_right_button_pressed  = right ? millis()  : _time_right_button_pressed;

        _update_button_pressed_state(left, middle, right);
        _update_cap_reset(GetButtonVal(BUTTON_LEFT), GetButtonVal(BUTTON_MIDDLE), GetButtonVal(BUTTON_RIGHT));

        break;
    case 'G'://get button summary: 0/1 if touched or not
        //Serial.println("HubInterface::_parse_msg message:: G");
        //Serial.println(payload);
        num_parsed = sscanf(payload, "%1c%1c%1c.", &left, &middle, &right);
        if (num_parsed != 3)//check number of arguments in the payload
        {
            _error_code = ERROR_CMD_RECEIVED_BAD_NUM_ARGS;
            return false;
        }
        //convert from ascii to number
        left    -= 48;
        right   -= 48;
        middle  -= 48;
        // char    msg[32];
        // sprintf(msg, "%1d%1d%1d", left, middle, right);
//                        Serial.println(msg);

        //update the time of button press if any detected
        _time_left_button_pressed   = left  ? millis()  : _time_left_button_pressed;
        _time_middle_button_pressed = middle ? millis()  : _time_middle_button_pressed;
        _time_right_button_pressed  = right ? millis()  : _time_right_button_pressed;

        _update_button_pressed_state(left, middle, right);

        break;
    case 'Z'://diag message parsing
        //read different components of DL (1: active, 0: inactive)
        // Serial.println("HubInterface::_parse_msg message:: Z");
        // Serial.println(payload);
        unsigned char    dispense_motor, present_motor;
        unsigned char    sound_playing, dispense_detected, foodtreat_still_in_bowl;
        unsigned char    foodtreat_statemachine_state, cap_open;
        num_parsed = sscanf(payload, "%1c%1c%1c%1c%1c%1c%1c%1c%1c%1c%1c.",
                            &dispense_motor, &present_motor,
                            &left, &middle, &right, &cue,
                            &sound_playing,
                            &dispense_detected, &foodtreat_still_in_bowl,
                            &foodtreat_statemachine_state, &cap_open);
        if (num_parsed != 11) //check number of arguments in the payload
        {
            _error_code = ERROR_CMD_RECEIVED_BAD_NUM_ARGS;
            return false;
        }
        //store the state of diag vars
        foodtreat_still_in_bowl         -= 48;
        foodtreat_statemachine_state    -= 48;
        cap_open                    -= 48;
        _last_diag_update_ms    = millis();
        _dome_open              = cap_open == 1; //if the cap is on or not
        _dome_open_int          = cap_open == 1 ? 1 : 0; // -1=dunno 0=closed 1=open
        _previous_foodtreat_taken   = foodtreat_still_in_bowl == 0; //this is the value of the foodtreat detection on platter return after previous dispense
        _foodmachine_state      = foodtreat_statemachine_state; //state of foodtreat state machine

        // Serial.println("HubInterface::_parse_msg message:: Z :: foodtreat_statemachine_state = ");
        // Serial.println(foodtreat_statemachine_state);
        // Serial.println("HubInterface::_parse_msg message:: Z :: _foodmachine_state = ");
        // Serial.println(_foodmachine_state);

        //TRACK FOOD ERRORS
        if (_foodmachine_state == FOODMACHINE_FOODTREAT_ERROR_CODE){
            // Serial.println("HubInterface::_parse_msg message:: Z :: _foodmachine_state == FOODMACHINE_FOODTREAT_ERROR_CODE");
            if (_hub_out_of_food == false){
                libLog.info("HubInterface::_parse_msg message:: Z :: HUB OUT OF FOOD");
                _hub_out_of_food = true;
                UpdateButtonAudioEnabled();
                IndicatorState = IL_DLI_OOF;

                // SetLights(); //Set yellow flashy top light
            }
        }
        else{
            if (_hub_out_of_food == true){
               if (_foodmachine_state == FOODMACHINE_IDLE){
                    libLog.info("HubInterface::_parse_msg message:: Z :: HUB HAS FOOD AGAIN");
                    _hub_out_of_food = false;
                    UpdateButtonAudioEnabled();
                    IndicatorState = IL_DLI_NULL;

                }
            }
        }

        //TRACK PLATTER ERRORS
        if (_foodmachine_state == FOODMACHINE_PLATTER_ERROR_CODE){
            if (_platter_error == false){
                _platter_error = true;
                _platter_error_start_ms = millis();
                IndicatorState = IL_DLI_JAM;
            }
            else{
            //still in error
                if (!_platter_stuck  && millis()>(_platter_error_start_ms+_platter_error_reset_wait)){
                    _platter_error_count += 1;

                    if (_platter_error_count > (_max_platter_error_count - 1))
                    {
                        _platter_stuck = true;
                        UpdateButtonAudioEnabled();
                        IndicatorState = IL_DLI_JAM_ERROR;


                    }
                    else
                    {
                        ResetFoodMachine();
                    }
                }
            }
        }
        else{
            if (_platter_error == true){
                _platter_error = false;

                IndicatorState = IL_DLI_NULL;
                }
            else {
                if (_foodmachine_state != FOODMACHINE_MOVING_HOME)
                {
                    _platter_error_count = 0;
                    _platter_stuck = false;
                    UpdateButtonAudioEnabled();
                }
            }
        }

        //TRACK SINGULATOR ERRORS
        if (_foodmachine_state == FOODMACHINE_SINGULATOR_ERROR_CODE){
            if (_singulator_error == false){
                _singulator_error = true;
                UpdateButtonAudioEnabled();
                IndicatorState = IL_DLI_JAM_ERROR;

                // SetLights(); //Set yellow flashy top light
            }
        }
        else{
            if (_singulator_error == true){
                _singulator_error = false;
                UpdateButtonAudioEnabled();
                IndicatorState = IL_DLI_NULL;

                // SetLights(); //Set lights to what they should be...
            }
        }
        /* //Uncomment to print DL diagnostic update state  //TODO: turn this into a method?
        Serial.println(
            "dli::DIAG UPDATED    ms    dispM    presM    left    midd    right    cue    sndply    disp    trtnbwl    trt_st    domeopen    prvTrtTkn",
            );
        char diagprnt[32];
        sprintf(diagprnt, "\t\t     %lu\t%c\t%c\t%c\t%c\t%c\t%c\t%c\t%c\t%d\t\t%d\t%d\t%d",
                        _last_diag_update_ms, dispense_motor, present_motor,
                        left, middle, right, cue,
                        sound_playing,dispense_detected, foodtreat_still_in_bowl,
                        _foodmachine_state, _dome_open, _previous_foodtreat_taken);
        Serial.println(diagprnt);
        */

        break;
    case 'L'://OK packet for setting BY lights flash
        libLog.trace("HubInterface::_parse_msg message:: L");
        break;
    case 'H'://OK packet for setting RGB lights flash
        libLog.trace("HubInterface::_parse_msg message:: H");
        break;
    case 'Q':
        break;
    case 'P':
        if (rplystatus == 0) {
            libLog.error("HubInterface::_parse_msg message:: P :: ERROR audio did not play - attempting REPLAY");

            switch (_ars_state) {
            case ARS_BEFORE_REPLAY:
                // Serial.println("dli::_parse_msg::P::ARS_BEFORE_REPLAY");
                _audio_replay_window_start = millis();
                if ((millis() - _audio_replay_window_start) <= _audio_replay_window) {
                    unsigned char    whichaudioagain;
                    unsigned char    volumeagain;
                    sscanf(_cmd_queue.front().buf, "%*c%*c%*c%*c%*c%*c%*c%1d%2d.", &whichaudioagain, &volumeagain);
                    // Serial.print("whichaudioagain: ");
                    // Serial.println(whichaudioagain);
                    // Serial.print("volumeagain: ");
                    // Serial.println(volumeagain);
                    PlayAudio(whichaudioagain, volumeagain);
                }
                else {
                    // Serial.println("dli::_parse_msg::P::REPLAY TIMED OUT");
                    _ars_state = ARS_BEFORE_REPLAY;
                }
                _ars_state = ARS_DURING_REPLAY;
                break;
            case ARS_DURING_REPLAY:
                // Serial.println("dli::_parse_msg::P::ARS_DURING_REPLAY");
                if ((millis() - _audio_replay_window_start) <= _audio_replay_window) {
                    unsigned char    whichaudioagain;
                    unsigned char    volumeagain;
                    sscanf(_cmd_queue.front().buf, "%*c%*c%*c%*c%*c%*c%*c%1d%2d.", &whichaudioagain, &volumeagain);
                    PlayAudio(whichaudioagain, volumeagain);
                }
                else {
                    // Serial.println("dli::_parse_msg::P::REPLAY TIMED OUT");
                    _ars_state = ARS_BEFORE_REPLAY;
                }
                break;
            }
        }
        else {
            _ars_state = ARS_BEFORE_REPLAY;
        }
        break;
    case 'T':
        libLog.trace("HubInterface::_parse_msg message:: T :: Presenting Tray");
        break;
    case 'X':
        libLog.trace("HubInterface::_parse_msg message:: X :: Retracting Tray");
        break;
    case 'N':
        libLog.trace("HubInterface::_parse_msg message:: N :: config item set");
        break;
    case 'K':
        _csf_needs_DI_reset = false;
        _csf_DI_reset_sent = false;
        _csf_last_DI_reset_millis = millis();
        libLog.trace("HubInterface::_parse_msg message:: K :: DI rebooted");
        break;
    case 'U':
        libLog.trace("HubInterface::_parse_msg message:: U: %s", payload);
        unsigned long config_id;
        unsigned long config_value;
        num_parsed = sscanf(payload, "%02d%05d", &config_id, &config_value);
        if (num_parsed != 2)//check number of arguments in the payload
        {
            _error_code = ERROR_CMD_RECEIVED_BAD_NUM_ARGS;
            return false;
        }
        // libLog.trace("HubInterface::_parse_msg <U> received:");
        // libLog("config_id");
        // libLog(config_id);
        // libLog("config_value");
        // libLog(config_value);

        switch (config_id) {
            case 21:
                _left_from_dl = config_value;
                break;
            case 22:
                _middle_from_dl = config_value;
                break;
            case 23:
                _right_from_dl = config_value;
                break;
            case 11:
                _tray_speed_pwm_from_dl = config_value;
                break;
            case 8:
                _tray_current_threshold_from_dl = config_value;
                break;
            case 18:
                _foodtreat_tx_power_level_from_dl = config_value;
                break;
            case 20:
                _foodtreat_detect_threshold_from_dl = config_value;
                break;
            default:
                libLog.error("dli::_parse_msg Error! get config message parse not implemented. Token %u, Payload %s", token, payload);
                break;
        }

        _num_config_values_recvd += 1;
        if (_num_config_values_recvd >= 7)
        {
            _get_config_done = true;
        }

        break;
    default:
        libLog.error("dli::_parse_msg Error! message parse not implemented, Token %u, Payload %s", token, payload);
        break;
    }
    //Serial.println("HubInterface::_parse_msg finished");
    dlimsg_t cmd = _cmd_queue.front();
    //Serial.println("comparing to the top-of-queue command:");
    //Serial.println(cmd[5]);
    //Serial.println(token);
    return cmd.buf[5] == token;
}

/*
                            <<<                             >>>
                            <<<        handle errors        >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   handle the errors if any occured by calling         |
                |   the appropriate functions or sending message        |
                |   to the device layer.                                |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |           None                                        |
                |   RETURN:                                             |
                |           True if no error happens, False otherwise   |
            <<</PARAMS>>>
*/
bool HubInterface::_handle_dl_errors()
{
    //for now only send a message through _logger
    switch (_error_code) {
    case ERROR_CMD_QUEUE_FULL:
        libLog.error("HubInterface::_handle_dl_errors cmd queue full");
        break;
    case ERROR_CMD_RECEIVED_BAD_START:
        libLog.error("HubInterface::_handle_dl_errors cmd received has a bad start char");
        break;
    case ERROR_CMD_RECEIVED_TOO_SHORT:
        libLog.error("HubInterface::_handle_dl_errors cmd received is too short");
        break;
    case ERROR_CMD_RECEIVED_BAD_NUM_ARGS:
        libLog.error("HubInterface::_handle_dl_errors cmd received has bad number of arguments");
        break;
    default:
        break;
    }
    _error_code     =   0;//resetting the error code is important after each call to _handle_dl_errors
    // this means that the error is handled by this function and no error is lost
    //  Serial.println("HubInterface::_handle_dl_errors finished");
    return true;
}

/*
                            <<<                             >>>
                            <<<    create cmd packet        >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   given a token and payload, ths function create      |
                |   a full command in cmd that can be sent to DL.       |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |       token : The command token, one unsigned char             |
                |     payload : a buffer containing the payload of      |
                |               of the message                          |
                |         cmd : the created command buffer              |
                |   RETURN:                                             |
                |           True if no error happens, False otherwise   |
            <<</PARAMS>>>
*/

bool HubInterface::_create_dl_cmd_with(unsigned char token, const char* payload, dlimsg_t *cmd)
{
    if (strlen(payload) >= 999) //for now allow only payload size for single packet communication
    {
        libLog.error("HubInterface::_create_dl_cmd_with payload too large for one packet");
        return false;
    }
    (*cmd).buf[7 + strlen(payload)] = STR_CARRIAGE_RETURN;//always mark the end of strings with CR
    //print the token, lengh of payload and payload into the packet

    sprintf((*cmd).buf, "$%03d%d%c1%s.", strlen(payload), _packet_number, token, payload);
    _packet_number              = (_packet_number + 1) % 9; //packet sequence number, always in [0-9]
    // libLog.trace("HubInterface::_create_dl_cmd_with message created");
    // Serial.println((*cmd).buf);
    // libLog.trace("HubInterface::_create_dl_cmd_with finished");
    return true;
}



bool HubInterface::IsHubOutOfFood()
{
    return _hub_out_of_food;
}

bool HubInterface::IsSingulatorError()
{
    return _singulator_error;
}

/*
                            <<<                             >>>
                            <<<        send report          >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   Send a report formatted in JSON to the particle     |
                |   Cloud with Particle.publish(). The standard name    |
                |   For the variable is "report". There are 8 standard  |
                |   values.                                             |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |                             |
                |           play_start_time: UTC start time of game     |
                |               game (c string)                         |
                |           player: string id of player (char)          |
                |           result: interaction result (c string)              |
                |           level: game level (unsigned int)            |
                |           duration: duration of game in ms            |
                |               (unsigned int)                          |
                |           foodtreat_presented: if food was presented  |
                |               (bool)                                  |
                |           foodtreat_eaten: if food was eaten (bool)   |
                |   RETURN:                                             |
                |           True if successful, False otherwise         |
            <<</PARAMS>>>
*/


bool HubInterface::Report(String play_start_time, String player, uint32_t level, String result, uint32_t duration, bool foodtreat_presented, bool foodtreat_eaten){

    char report[621]; // max particle publish length

    //build report
    unsigned int string_length = snprintf(report, sizeof(report),
            "{\"challenge_id\":\"%s\",\"play_start_time\":\"%s\",\"player\":\"%s\",\"timestamp\":\"%lu\",\"result\":\"%s\",\"level\":\"%lu\",\"duration\":\"%lu\",\"foodtreat_presented\":\"%d\",\"foodtreat_eaten\":\"%d\"}",
            challenge_id,
            play_start_time.c_str(),
            player.c_str(),
            Time.now(),
            result.c_str(),
            level,
            duration,
            foodtreat_presented,
            foodtreat_eaten
            );

    if ((string_length >= 0) and (string_length < sizeof(report))){
        // succesfully constructed report string
        if (Particle.connected()) {
            // connected to particle cloud, send report
            return Particle.publish("report", report, 60, PRIVATE);
        }
        else {
            // not connected to particle cloud
            return false;
        };
    }
    else {
        // something went wrong in constructing report string
        return false;
    }
}

/*
                            <<<                             >>>
                            <<<        send report          >>>
                            <<<                             >>>


            <<<GOAL>>>
                |   Send a report formatted in JSON to the particle     |
                |   Cloud with Particle.publish(). The standard name    |
                |   For the variable is "report". There are 8 standard  |
                |   values and 1 extra field for custom metrics.        |
            <<</GOAL>>>


            <<<PARAMS>>>
                |   INPUT:                                              |
                |           play_start_time: UTC start time of game     |
                |               game (c string)                         |
                |           player: string id of player (char)          |
                |           result: interaction result (c string)              |
                |           level: game level (unsigned int)            |
                |           duration: duration of game in ms            |
                |               (unsigned int)                          |
                |           foodtreat_presented: if food was presented  |
                |               (bool)                                  |
                |           foodtreat_eaten: if food was eaten (bool)   |
                |           extra: custom field for extra metrics (char)|
                |   RETURN:                                             |
                |           True if successful, False otherwise         |
            <<</PARAMS>>>
*/

bool HubInterface::Report(String play_start_time, String player, uint32_t level, String result, uint32_t duration, bool foodtreat_presented, bool foodtreat_eaten, String extra){

    char report[621]; // max particle publish length

    //build report
    unsigned int string_length = snprintf(report, sizeof(report),
            "{\"challenge_id\":\"%s\",\"play_start_time\":\"%s\",\"player\":\"%s\",\"timestamp\":\"%lu\",\"result\":\"%s\",\"level\":\"%lu\",\"duration\":\"%lu\",\"foodtreat_presented\":\"%d\",\"foodtreat_eaten\":\"%d\",\"extra\":%s}",
            challenge_id,
            play_start_time.c_str(),
            player.c_str(),
            Time.now(),
            result.c_str(),
            level,
            duration,
            foodtreat_presented,
            foodtreat_eaten,
            extra.c_str()
            );

    if ((string_length >= 0) and (string_length < sizeof(report))){
        // succesfully constructed report string
        if (Particle.connected()) {
            // connected to particle cloud, send report
            return Particle.publish("report", report, 60, PRIVATE);
        }
        else {
            // not connected to particle cloud
            return false;
        };
    }
    else {
        // something went wrong in constructing report string
        return false;
    }
}
