# ColorSounds

Color matching game. Match the color to the voice command.

## Getting Started

The audio output is played on a remote computer in the same network. The computer needs to have the SoundPlayer.py running on port 4888 for the audio output.

### Prerequisites

The SoundPlayer.py requires python3 and the playsound library

```
pip3 install playsound
```

## Running the game

Start the soundplayer on a computer with

```
python3 SoundPlayer.py 4888
```

then start the game on the hub.

### Game instructions

The game will start in setup mode displaying three rounds of 3 colors (yellow, blue, white) with which you can select the colors the game will choose from.
I.e if you select blue, blue, blue it will only show blue and every button will be correct. if you press blue, yellow, white it will show random permutations of those colors.

In the 4th step you can choose the difficulty, from easy to hard. The first button means that only the first color that was selected will be asked for. So if you initialize the game with yellow  blue, white and then press the weakest light it will always ask for yellow even though it will show the colors in a random order.

If you press yellow, blue, white and the middle light it will only ask for yellow and blue and if you press yellow, blue, white and the brightes color button it will ask for all colors in all combinations. There are of course a few modes that make no sense, like blue, blue, blue and the strongest button since it will randomly select one of the three blues but it does not do any harm either.

## Authors

* **Michael Gschwandtner** - *Initial work* 


