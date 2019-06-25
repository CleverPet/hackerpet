/* ColorSounds
 *
 * After powering up the Hub, keep your pet back from the Hub as you select the game's Settings (below)
 *
 * Setting 1. Which colors: Select which combination of colors to use. This is done in three
 *    steps, allowing you to make three choices. One way to make the game easier
 *    is to present fewer colors (perhaps as few as just one). 
 *
 * Setting 2. How many: 1, 2, 3
 *
 * 1 (darkest): Game will only ask for the color picked in the first step (above) 
 * 2: Game will randomly choose from the first two selected colors 
 * 3: Game will randomly choose from all three selected colors
 *
 * Setting 3. Number of acceptable mistakes
 *
 * 0 (brightest): Game will tolerate 0 incorrect touches (more challenging)
 * 1: Game will tolerate 1 incorrect touch
 * 2: Game will tolerate 2 incorrect touches (better for learning)
 *
 * This game requires an external PC that has the SoundPlayer.py running on port 4888
 *
 * Start with: python3 SoundPlayer.py 4888
 */

#include <Particle.h>

// This #include statement was automatically added by the Particle IDE.
#include <papertrail.h>

#include <hackerpet.h>
#include "remote_util.h"

using namespace std;

SYSTEM_THREAD(ENABLED);

// initialize device layer interface (from CleverpetLibrary)
HubInterface hub;

const char BLUE = 'b';
const char WHITE = 'w';
const char YELLOW = 'y';

// Use the IP address and port where the receiver is listening
PapertrailLogHandler papertailHandler("192.168.0.255", 4888, "RemoteSound");

IPAddress broadcastAddress;

unsigned char combinationIndex (int combination, int index) {
    /* Possible combinations
     * 012, 021, 102, 120, 210, 201
     */
    
    if (index == 0) {
        return (combination < 2) ? 0 : (combination < 4) ? 1 : 2;
    }
    if (index == 1) {
        return (combination == 2 || combination == 5) ? 0 : (combination == 0 || combination == 4) ? 1 : 2;
    }
    if (index == 2) {
        return (combination == 3 || combination == 4) ? 0 : (combination == 1 || combination == 5) ? 1 : 2;
    }
    
    Log.error("Unknown combination number %d", combination);
    return 0;
}



class ColorTouch
{
    public: 
        ColorTouch(HubInterface *hub)  // num_buttons must be 1, 2, or 3
        {
            _hub = hub;  // keep a pointer to device layer interface
        }


        unsigned char indexToLight (unsigned char index) {
            if (index == 0) return _hub->LIGHT_LEFT;
            if (index == 1) return _hub->LIGHT_MIDDLE;
            if (index == 2) return _hub->LIGHT_RIGHT;
            return 0;
        }
        
        unsigned char lightToIndex(unsigned char light) {
            if (light == _hub->LIGHT_LEFT)    return 0;
            if (light == _hub->LIGHT_MIDDLE)  return 1;
            if (light == _hub->LIGHT_RIGHT)   return 2;
            return 0;
        }

        void setColorLight(char color, unsigned char light) {
            if (color == YELLOW) {
                _hub->SetLights(light, 60, 0, 0);
                _current_display[lightToIndex(light)] = YELLOW;
            } else if (color == BLUE) {
                _hub->SetLights(light, 0, 60, 0);
                _current_display[lightToIndex(light)] = BLUE;
            } else if (color == WHITE) {
                _hub->SetLights(light, 60, 60, 0);
                _current_display[lightToIndex(light)] = WHITE;
            } else {
                Log.error("Invalid color");
            }
        }

        void playColorName(char color) {
            switch (color)
            {
                case BLUE:
                    playRemoteSound("blue", broadcastAddress);
                    break;
                
                case YELLOW:
                    playRemoteSound("yellow", broadcastAddress);
                    break;

                case WHITE:
                    playRemoteSound("white", broadcastAddress);
                    break;
            }
        }


        void run()
        {
            switch (_state) 
            {
                case _STATE_INIT:
                {   
                    _pressed = _hub->AnyButtonPressed();

                    if (_hub->IsReady() && _hub->FoodmachineState() == _hub->FOODMACHINE_IDLE && _pressed == 0)
                    {
                        // DI reset occurs if, for example, DL detects that touchpads need re-calibration, like if hub was moved to a different surface
                        _hub->SetDIResetLock(true);  // prevent DI board from resetting during a trial (would cause lights to go blank etc.)
                        _time_start_wait = 0;
                        _state = _STATE_DISPLAY_COLOR_SELECTOR;

                    }    
                    return;
                    break;
                }
                case _STATE_DISPLAY_COLOR_SELECTOR:
                {
                    if ( _colors_selected > 2)
                    {
                        //Colors already selected. Skipping
                        _state = _STATE_DISPLAY_MODE_SELECTOR;
                        return;
                    }
                    
                    //Show that the hub is in game setup mode
                    _hub->SetLights(_hub->LIGHT_CUE, 60, 60, 0);


                    if (millis() < _time_start_wait + 1000) {
                        //Prevent double taps
                        return;
                    }
                    
                    Log.info("Display color selector");

                    setColorLight(YELLOW, _hub->LIGHT_LEFT);
                    setColorLight(BLUE, _hub->LIGHT_MIDDLE);
                    setColorLight(WHITE, _hub->LIGHT_RIGHT);
                    
                    _state = _STATE_SELECT_COLOR;
                    
                    return;
                    break;
                }
                case _STATE_SELECT_COLOR:
                {
                    
                    // detect any buttons currently pressed
                    _pressed = _hub->AnyButtonPressed();

                    if (_pressed != 0)
                    {
                        _hub->SetLights(_hub->LIGHT_BTNS, 0, 0, 0);   // turn off lights 

                        //Store the currently pressed color in the color selection
                        unsigned char button = lightToIndex(_pressed);
                        _color_selection[_colors_selected] = _current_display[button];
                        
                        Log.info("Color %d is %d", _colors_selected, _color_selection[_colors_selected]);

                        _colors_selected ++; 
                        //Select the next color
                        _state = _STATE_DISPLAY_COLOR_SELECTOR;
                        _time_start_wait = millis();
                    }
                    
                    return;
                    break;
                }
                case _STATE_DISPLAY_MODE_SELECTOR:
                {
                    if (_play_mode > 0)
                    {   
                        //Mode already selected. Skipping
                        _state = _STATE_DISPLAY_INCORRECTS_SELECTOR;
                        return;
                    }
                    
                    if (millis() < _time_start_wait + 1000) {
                        //Prevent double taps
                        return;
                    }

                    
                    Log.info("Display mode selector");

                    _hub->SetLights(_hub->LIGHT_LEFT, 10, 10, 0); //Easy
                    _hub->SetLights(_hub->LIGHT_MIDDLE, 40, 40, 0); //Medium
                    _hub->SetLights(_hub->LIGHT_RIGHT, 80, 80, 0); //Hard
                    
                    _state =  _STATE_SELECT_MODE;
                    
                    return;
                    break;
                }
                case _STATE_SELECT_MODE:
                {

                    // detect any buttons currently pressed
                    _pressed = _hub->AnyButtonPressed();

                    if (_pressed != 0)
                    {
                        _hub->SetLights(_hub->LIGHT_BTNS, 0, 0, 0);   // turn off lights 
                        _play_mode = lightToIndex(_pressed)+1;
                        _state =  _STATE_DISPLAY_INCORRECTS_SELECTOR;
                        _time_start_wait = millis();
                    }

                    return;
                    break;
                }
                case _STATE_DISPLAY_INCORRECTS_SELECTOR:
                {

                    if (_max_incorrects > -1) {
                        _state = _STATE_PAUSE; // already selected; skip
                        return;
                    }

                    if (millis() < _time_start_wait + 1000) {
                        //Prevent double taps
                        return; 
            			break;
                    } else {
                        Log.info("Display incorrects selector");
                        
                        _hub->SetLights(_hub->LIGHT_LEFT, 80, 0, 0); //0
                        _hub->SetLights(_hub->LIGHT_MIDDLE, 40, 0, 0); //1
                        _hub->SetLights(_hub->LIGHT_RIGHT, 10, 0, 0); //2
                        
                        _state =  _STATE_SELECT_NUM_INCORRECTS;
    		        }
                    
                    return;
                    break;
                }
                case _STATE_SELECT_NUM_INCORRECTS:
                {

                    // detect any buttons currently pressed
                    _pressed = _hub->AnyButtonPressed();

                    if (_pressed != 0)
                    {
                        _hub->SetLights(_hub->LIGHT_BTNS, 0, 0, 0);   // turn off lights 
                        _max_incorrects = lightToIndex(_pressed);
                        _state =  _STATE_PAUSE;
                        Log.info("Max incorrects is %d", _max_incorrects);
                    }

                    return;
                    break;
                }
                case _STATE_PAUSE:
                {
                    _hub->SetLights(_hub->LIGHT_ALL, 0, 0, 0);   // turn off lights 
                    _state = _STATE_CHOOSE_TOUCH;
                    _time_start_wait = millis();
                    return;
                    break;
                    
                }
                case _STATE_CHOOSE_TOUCH:
                {
                    
                    bool keep_waiting = false;
                    
                    if (_accurate) {
                        if (millis() < _time_start_wait + 2000) 
                            keep_waiting = true;
                    } else {
                        if (millis() < _time_start_wait + 5000)
                            keep_waiting = true;
                    }
                    
                    if (keep_waiting) {
                        if (_hub->AnyButtonPressed() != 0) {
                            //BEHAVIOR: Pressing buttons during waiting time is trial and error and should be avoided.
                            //Reset the waiting time
                            _time_start_wait = millis();
                        }
                        return;
                    }
                    
                    // choose some target lights, and store which targets were randomly chosen
                    _hub->SetLights(_hub->LIGHT_BTNS, 0, 0, 0);   // turn off lights 
                    
                    //Create a random color combination
                    unsigned char combination = random(0,6);
                    Log.info("Combination no %d",combination);

                    setColorLight(_color_selection[combinationIndex(combination,0)], _hub->LIGHT_LEFT);
                    setColorLight(_color_selection[combinationIndex(combination,1)], _hub->LIGHT_MIDDLE);
                    setColorLight(_color_selection[combinationIndex(combination,2)], _hub->LIGHT_RIGHT);

                    //Select a random button
                    unsigned char button_index = random(0,_play_mode); 

                    Log.info("Selected button %d",button_index);

                    _target = indexToLight(button_index);

                    //Play the color name of the selected button
                    playColorName(_color_selection[button_index]);

                    // progress to next state
                    _move_wait = round_timeout; 
                    
                    _time_start_wait = millis();
                    _incorrects = 0;

                    _state = _STATE_WAIT_TOUCH;
                    return;
                    break;
                }
                case _STATE_WAIT_TOUCH:
                {
                    unsigned long timeout_ms = 60000;

                    if (millis() < _last_touch + 200){
                        // avoid double touches
                        return;
                        break;
                    }
		    
                    // detect any buttons currently pressed
                    _pressed = _hub->AnyButtonPressed();
                    _last_touch = millis();

                    if (_pressed != 0 || millis() > _time_start_wait + timeout_ms)
                    {
                        if (_current_display[lightToIndex(_pressed)] != _color_selection[lightToIndex(_target)]
                            and _incorrects < _max_incorrects) {
                            _incorrects++;
                            Log.info("Wrong-touch forgiven");
                        } else {
                            // if _incorrects has run out, or if trial timed out, progress to next state
                            _state = _STATE_EVAL;
                        }
                    }
                    else if (_pressed == 0 && millis() > _time_start_wait + _move_wait)
                    {
                        // if no button was pressed, and the random move period has elapsed
                        //    pick a different touchpad to flash
                        _accurate = true;
                        _state = _STATE_PAUSE;
                    }

                    return;
                    break;
                }
                case _STATE_EVAL: 
                {
                    _hub->SetLights(_hub->LIGHT_BTNS, 0, 0, 0);   // turn off lights 

                    if (_pressed == 0)
                    {
                        // if we are in this state and no button was pressed, it is a timeout
                        _timeout = true;
                        _accurate = false;
                    } 
                    else
                    {
                        
                        unsigned char color_pressed = _current_display[lightToIndex(_pressed)];
                        unsigned char color_target = _color_selection[lightToIndex(_target)];
                        
                        unsigned char match = color_pressed == color_target;
                        if (match == 0)
                        {
                            // if button was pressed but not a match to any target: unsuccessful trial
                            _accurate = false;
                        }
                        else
                        {
                            // if button was pressed but and a match to one of targets: successful trial
                            _accurate = true;
                        }
                    }
                    
                    // next state
                    _state = _STATE_PLAY_AUDIO;

                    return;
                    break;
                }
                case _STATE_PLAY_AUDIO: 
                {
                    // delay 600 ms in case a button sound still playing
                    delay(600);

                    if (_accurate)
                    {
                        // if successful trial: play "reward" sound
                        _hub->PlayAudio(_hub->AUDIO_POSITIVE, 20);
                    }
                    else
                    {
                        if (!_timeout) // don't play any audio if time out (no response -> no consequence)
                        {
                            // if unsuccessful trial: play "try again" sound
                            _hub->PlayAudio(_hub->AUDIO_NEGATIVE, 20);
                        }
                    }

                    // next state
                    _state = _STATE_CONSEQUATE;
                    return;
                    break;
                }
                case _STATE_CONSEQUATE: 
                {
                    delay(600);  // wait for audio to finish playing 
                    if (_accurate)
                    {
                        unsigned long foodtreat_duration = 5000; // milliseconds 

                        // if successful trial, present foodtreat using PresentAndCheckFoodtreat state machine
                        unsigned char foodtreat_state = _hub->PresentAndCheckFoodtreat(foodtreat_duration);  // state machine
                        
                        // if state machine in one of two end states (food eaten or food not eaten), go to next trial
                        if (foodtreat_state == _hub->PACT_RESPONSE_FOODTREAT_NOT_TAKEN || foodtreat_state == _hub->PACT_RESPONSE_FOODTREAT_TAKEN) // state machine finished
                        {
                            // start a new trial, reset variables
                            _state = _STATE_INIT;
                            _pressed = 0;
                            _timeout = false;
                            _hub->SetDIResetLock(false);  // allow DI board to reset if needed between trials
                        }
                    }
                    else
                    {
                        // wrong button pressed; start a new trial without giving food
                        // start a new trial, reset variables
                        _state = _STATE_INIT;
                        _pressed = 0;
                        _timeout = false;
                        _hub->SetDIResetLock(false);  // allow DI board to reset if needed between trials
                    }
                    return;
                    break;
                }
                default: 
                {
                    return;
                    break;
                }
            }
        }

    private: 
        static const unsigned char _STATE_INIT = 0;
        static const unsigned char _STATE_WAIT_TOUCH = 1;
        static const unsigned char _STATE_EVAL = 2;
        static const unsigned char _STATE_PLAY_AUDIO = 3;
        static const unsigned char _STATE_CONSEQUATE = 4;
        static const unsigned char _STATE_CHOOSE_TOUCH = 5;
        static const unsigned char _STATE_PAUSE = 6;

        static const unsigned char _STATE_DISPLAY_COLOR_SELECTOR = 7;
        static const unsigned char _STATE_DISPLAY_MODE_SELECTOR = 9;

        static const unsigned char _STATE_SELECT_MODE = 10;
        static const unsigned char _STATE_SELECT_COLOR = 8;

    	static const unsigned char _STATE_DISPLAY_INCORRECTS_SELECTOR = 11;
        static const unsigned char _STATE_SELECT_NUM_INCORRECTS = 12;

        static const unsigned char _PLAY_MODE_ONE_COLOR = 1;
        static const unsigned char _PLAY_MODE_TWO_COLORS = 2;
        static const unsigned char _PLAY_MODE_THREE_COLORS = 3;





        HubInterface * _hub;        
        unsigned char _state = _STATE_INIT;  // current state of game's state machine
        unsigned char _target;
        unsigned long _time_start_wait;
        unsigned long _move_wait;
        unsigned long _last_touch;
        bool _accurate;
        unsigned char _pressed = 0;
        bool _timeout = false;
        
        char _color_selection[3] = { BLUE, BLUE, BLUE};
        unsigned char _current_display[3] = { 0, 0, 0 }; 

        unsigned char _colors_selected = 0;
        unsigned char _play_mode = 0;

        unsigned int round_timeout = 10000; //How long the challenge is presented

        int _incorrects = 0; //Number of incorrect presses made
        int _max_incorrects = -1; //Max permitted incorrects
        
        
};
        
ColorTouch game(&hub);

bool system_ready = false;

// to avoid quirks define setup() nearly last, and right before loop()
void setup()
{
    Serial.begin(9600);  // for debug text output
    Serial1.begin(38400);  // needed for hub
    hub.SetDoPollDiagnostics(true); //start polling the diagnostics
    hub.SetDoPollButtons(true); //start polling the buttons
    hub.PlayTone(0, 5, 10); // turn off sound
    hub.SetLights(hub.LIGHT_BTNS, 0, 0, 0);   
    
}

// run-loop function required for Particle
void loop()
{
    // advance the device layer state machine, but with 20 millisecond max time spent per loop cycle
    hub.Run(20);

    if (WiFi.ready() && system_ready == false)
    {
        broadcastAddress = getBroadcastAddress();
        system_ready = true;
    }
    else {
        //Waiting for the Wifi to become ready        
    }

    if (system_ready) 
    {
        game.run(); 
    }
    
}




