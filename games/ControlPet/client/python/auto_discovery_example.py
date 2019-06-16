import controlpet
from controlpet import Cleverpet
import sys

if __name__=="__main__":

    discovery = Cleverpet.discover_hub(20) 
    if not discovery:
        print ("Hub not found")
        sys.exit(0)

    hub = Cleverpet (discovery["address"], discovery["port"])
    
    print ("Hub address: {}:{}".format(discovery["address"],discovery["port"]))

    hub.connect()

    hub.all_buttons_off()

    hub.new_round()


    hub.button_color(0, "blue")

    hub.button_color("cue", "yellow")

    hub.button_color(controlpet.HUB_BUTTON_MIDDLE, "blue", 30) 

    hub.button_color(controlpet.HUB_BUTTON_RIGHT, "blue", 10) 

    buttons = hub.wait_for_button(20)


    hub.disconnect()
