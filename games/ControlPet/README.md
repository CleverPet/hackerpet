# ControlPet

Control the hub remotely

## Getting Started

The software on the hub receives commands over a local network connection and executes the corresponding hub calls.

## Client

### Webbrowser

If your device supports mDNS (Avahi/Zeroconf) open the address [http://cleverpet.local](http://cleverpet.local) to access the remote control. 

If mDNS is not supported you need to go directly to http://<ip address of your hub> or if on Android you may install the "Bonjour Browser" app to browse the local services and click on "Cleverpet interface" to open the remote control.

### Android

After installing the APK from (clients/android) make sure your mobile is connected to the same network as the hub. If broadcast messages are not blocked the address of the hub should appear automatically in the mobile client. Otherwise the address has to be entered manually.

To connect press the broken chain icon. If the connection is successful the icon should change to a closed chain.

### Python

Supported python version: 3.6+

Communication with the hub is done via the Cleverpet object from the controlpet module in client/python. After instantiating Controlpet with the IP and Port of the hub call **.connect()**

Communication is happening in a background thread. Messages to the hub are put in a FIFO queue and most calls are asynchronous.

To end communication with the hub call **.disconnect()**

The hub needs a reinitialization routine after each round played. To ensure that the hub is in a good state call **.new_round()** after each interaction of your pet with the hub.

A simple game workflow would be

    1 Connect
    2 Show colors
    3 Wait for key press
    4 Evaluate result and present treat if correct
    5 Evaluate if treat was taken
    6 Initialize new round
    7 Go back to 2 or disconnect if finished

A simple example can be found [here](client/python/basic_example.py)


## Server

**Required libraries**

* hackerpet
* papertrail

Communication with the hub:

TCP Port: 4889

commands are encapsulated in a start marker **@** and an end marker **;**
Text outside those markes is discarded, so if a line based interface like telnet is used the newline characters are ignored.

The end marker can not be escaped inside a command and is prohibited.

Parameters inside a command are separated by **:** and must start and end with a **:** as well.
For example 

    :param1:param2:param3:  

**List of commands**

    @buttons;
    Request the buttons pressed since the last @buttons; request
    returns: @buttons:<int>:;
    The integer returned has the first three bits set to the state of the three buttons 

    @dispense;
    Present the treat
    returns: @ok;

    @light;<int>:<int>:<int>:;
    Parameter 1 ... the number of the light (0,1,2)
    Parameter 2 ... How much yellow (0 ... 100)
    Parameter 3 ... How much blue (0 ... 100)
    Switch on/off a button light
    returns: @ok;

    @playaudio:<string>:;
    Parameter 1 ... The name of the sound to play (one of negative,positive,do,click,squeak,left,middle,right)
    returns: @ok;

    @reinitialize;
    Allow the hub to do re-initialize. Should be done after each round
    returns: @ok;

**List of events**
Events are messages sent by the hub asynchronously

    @shout:<string>:;
    This message is broadcast via UDP on port 4888 and sends the DeviceID of the hub in intervalls

    @button_event:<int>:;
    Parameter 1 ... Button that was pressed (1 .. left, 2 .. middle, 4 .. right)
    When a button is pressed the hub sends a button_event with the button that has been pressed.
    Each button has a minimum interval before sending the next button_event (currently 500ms) to prevent double taps


## Authors

* **Michael Gschwandtner** - *Initial work* 


