""" This tool listens on a specific udp port and when a play message is received 
    the corresponding sound from the sounds dict is played

    If severeal messages with the same timestamp are received only one command is executed

    Requirements: 
    * python3
    * playsound   (sudo pip3 install playsound)
"""
import argparse
import socket
import re
from playsound import playsound as play

#Specify which sounds are supported
sounds = {
    "blue":     "sounds/blue.wav",
    "green":    "sounds/green.wav",
    "red":   	"sounds/red.wav",
    "white":   	"sounds/white.wav",
    "yellow":  	"sounds/yellow.wav",
    "one":      "sounds/one.wav",
    "two":      "sounds/two.wav",
    "three":    "sounds/three.wav"
    }

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "port", type=int, help="The UDP port to listen on")
    parser.add_argument(
        "-v", "--verbose", help="Shows the received messages on the console", action="store_true")
    args = parser.parse_args()

    client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    client.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    client.bind(("", args.port))

    last_recv_timestamp = -1

    while True:
        data, addr = client.recvfrom(1024)

        data = data.decode("utf-8")

        if args.verbose:
            print ("%s" % data)

        if data[:2] == "@[":
            if args.verbose:
                print ("Control message")

            match = re.search("@\[(\d+)\]\[(\w+)\]<(\w+)>", data)
            
            if match and len(match.groups()) == 3:
                ts, cmd, param = match.groups()
                if ts != last_recv_timestamp:
                    # Only process if it is not a repeated message
                    last_recv_timestamp = ts

                    if cmd == "play":
                        if args.verbose:
                            print ("Play sound " + param)
                        snd = sounds.get(param, None)
                        if snd:
                            play(snd)
                    else:
                        if args.verbose:
                            print ("Command not supported")
            else:
                if args.verbose:
                    print ("Could not parse message")
