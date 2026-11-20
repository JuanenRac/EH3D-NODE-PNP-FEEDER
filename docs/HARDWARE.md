# EH3D-NODE-PNP-FEEDER: hardware

Nothing in this page has been built or tried: it is the preliminary shape of the hardware of the node, written so that the first bench has a list to start from. No manufacturer or part number is named, because none has been checked; every row below is `verified: no`.

## Preliminary parts

| Part | What it is for | What it could be | Verified |
|---|---|---|---|
| Controller board | The node of the chain: network, the bus and the logic | an ESP32-S3 module, as in the other nodes of the suite | no |
| Bus transceiver | Talks to the feeders over the serial bus, with a direction line | an RS-485 transceiver and its termination | no |
| Feeder board | One per feeder: a small microcontroller, the driver of the motor and the sensors | the firmware of a feeder is the `Unit` of `core/feeder_unit.hpp` | no |
| Motor | Turns the sprocket that advances the tape | a small stepper with a driver, or a servo; the torque needed for a 16 mm tape is not measured | no |
| Sprocket wheel | Engages the holes of the tape (a 4 mm pitch in the usual standard of embossed tape) | to be drawn; the mechanical parts are CC-BY-SA | no |
| Tape sensors | Presence of tape, the pocket and the holes | infrared reflection and an array of phototransistors | no |
| Tension sensor | Tells a slack or a tight tape | a lever with a switch or a potentiometer | no |
| Film peeler | Lifts the cover film | to be drawn; a sensor tells that the film is clear | no |
| Power supply | The motors and the electronics | to be sized when the motor is chosen | no |

## Tests on a bench

The node is tried on a computer against a simulator. Each test below is what a person must run on the real hardware before any requirement is called validated; until then the evidence stays `simulator`.

| Test | What to do | What must be seen |
|---|---|---|
| F-01 | Advance of 100 pitches of 4 mm on a real tape | The pocket is found after every advance without a retry; write down the retries |
| F-02 | The five pitches on real tapes of 8, 12 and 16 mm | Each pitch moves the tape by that pitch within the tolerance of the pocket sensor |
| F-03 | Block the tape by hand during an advance | FDR-J01 and the motor stopped within one second |
| F-04 | A slack tape and a tight tape | FDR-T01 and FDR-T02, and no move at all when the tension is out of the band before the advance |
| F-05 | A tape with the film not peeled | FDR-F01 and no more advances until the film is clear |
| F-06 | Run a tape to its end | The state empty, and no more advances |
| F-07 | Unplug the bus during an advance | FDR-L01 within the link time, the motor stopped, and the safe state until acknowledged |
| F-08 | Stop the polls of the server | The node in its safe state within the server time, and the feeders in theirs a moment later |
| F-09 | A chain of 32 feeders on a long cable | The rate of damaged frames and of repeats, with the cable at its longest |
| F-10 | Cut the power during an advance and switch it on again | The feeder wakes up still and holds no stale order |

## Licences

* **Software:** GPL-3.0-or-later (the file `LICENSE`).
* **Mechanical parts** (models, drawings): CC-BY-SA. The version is confirmed when the first model is added.
* **Electronics** (schematics, boards): CERN-OHL-S. The version is confirmed when the first design file is added.

There are no hardware files in this repository yet; the text of each licence goes into the folder of the files it covers when the first one is added.
