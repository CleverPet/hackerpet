from controlpet import Cleverpet

if __name__=="__main__":

    hub = Cleverpet ("192.168.0.136",4889)

    hub.connect()

    hub.all_buttons_off()

    for round in range (10):
        hub.new_round()

        hub.button_color(0, "white")

        buttons = hub.wait_for_button(20)
        print (f"Buttons pressed {buttons}")

        hub.all_buttons_off()

        if buttons[0]:
            hub.positive_sound()
            taken = hub.dispense()
            print (f"Treat taken? {taken}")
        else:
            hub.negative_sound()

    hub.all_buttons_off()

    hub.disconnect()
