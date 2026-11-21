# EH3D-NODE-PNP-FEEDER: hardware

Nothing in this page has been built or tried: it is the preliminary shape of the hardware of the node, written so that the first bench has a list to start from. No manufacturer or part number is named, because none has been checked; every row below is `verified: no`.

## Preliminary parts

| Part | What it is for | What it could be | Verified |
|---|---|---|---|
| Controller board | The node of the chain: network, the bus and the logic | an ESP32-S3 module, as in the other nodes of the suite | no |
| Bus transceiver | Talks to the feeders over the bus chosen in the settings (`bus` = `rs485`, `can` or `i2c`) | for RS-485, a transceiver with a direction line (DE/RE) and a termination at both ends; for CAN, a transceiver and a termination at both ends; for I2C, the lines of the controller with pull-ups and no transceiver | no |
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
| F-11 | RS-485: look at the direction line (DE/RE) and the data line with an oscilloscope while the node sends a frame and the feeder answers | The driver is enabled `rs485_turnaround_us` before the first bit and released after the last stop bit; two drivers are never on the line at once; set the turnaround from what is measured |
| F-12 | RS-485: a chain of 32 feeders on the longest cable of the machine, at each `rs485_baud` from 115200 up, with the termination at both ends and then without it | The rate of damaged frames and of repeats for each baud rate and cable; the highest rate with no repeats; write down the cable and the termination |
| F-13 | CAN: a chain of 32 feeders at each `can_bitrate`, on the longest cable, with the termination at both ends | The pieces of every frame arrive in order, no frame is lost, the error counters of the controller stay still; the highest bitrate for the cable; the time of the longest frame with its answer (with bit stuffing) is shorter than `reply_timeout_ms` |
| F-14 | CAN: two feeders answer at the same moment to a command to all of them, and the node sends while another device talks | The arbitration gives the frames one after the other, no piece is mixed with a piece of another frame, and the node counts the extra time |
| F-15 | I2C: 32 feeders on the address range from `i2c_base_addr`, at 100 kHz and at 400 kHz, with the pull-ups of the board | The capacitance of the lines and the rise time with all 32 on; the fastest clock with no repeats; no address answered by two feeders; the feeders keep to the reserved addresses |
| F-16 | I2C: a feeder holds the clock while its motor driver is busy | The longest time the clock is held, and that `i2c_stretch_limit_us` is above it; a feeder unplugged is a NACK of its address, seen as offline after `offline_after` tries |
| F-17 | The same 100 advances of F-01 with each of the three settings of `bus` on the same feeders | The same tape moves in each; write down the repeats, the offline events and the time of the advance for each bus; the choice of the bus for a machine is made from this table |

## Licences

* **Software:** GPL-3.0-or-later (the file `LICENSE`).
* **Mechanical parts** (models, drawings): CC-BY-SA. The version is confirmed when the first model is added.
* **Electronics** (schematics, boards): CERN-OHL-S. The version is confirmed when the first design file is added.

There are no hardware files in this repository yet; the text of each licence goes into the folder of the files it covers when the first one is added.
