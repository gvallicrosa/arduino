# Phone

Escape room puzzle to dial numbers and get a response from the phone.

[TOC]

## Materials

* 1x Old telephone
* 1x Arduino Nano (rev 3.0)
* 3x IN4007 diodes
* 4x 330kΩ resistors
* 1x 2.2kΩ resistors
* 1x switch

## Schematic

![schematic](schematic.png)

## Prepare audio

Using text-to-speech from Projecte Aina.

```bash
docker run --shm-size=2gb -p 8080:8000 projecteaina/tts-api:latest
```

Open navigator [here](http://localhost:8080) and generate the necessary audio files from text (used `m_cen_pau` in this case).

When all files are ready, convert them to 8MHz compatible files for Arduino TMRpcm player with `audio/convert.sh` script.

Copy files  with filenames `XXX.wav` where `X` is each one of the digits of the phone number that will play this file (i.e. `123.wav`). You will also need a `intro.wav` to be played on unhang, and a `none.wav` to be played when the file for a specific phone number does not exist.

## Hardware details

Numpad circuit tracing.

![numpad](numpad.png)
