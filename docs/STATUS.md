# EH3D-NODE-PNP-FEEDER: status

## Current state

EH3D-NODE-PNP-FEEDER is the logic of a node, tested against a simulator. It has a settings file, the rules of the tape feeders, the words of the node on the network and a simulator, and the frame of the chain bus. The tests run on Windows and Linux in the CI. Nothing has moved a real tape feeders: the firmware is not written, and nothing has been connected to a motor, a pump, a valve or a sensor.

## Capabilities

The official state of each capability is in `REQUIREMENTS.md`. The register has 54 requirements: 47 implemented, 0 prototype, 7 planned and 0 validated. By progress: 47 done, 0 partial, 4 pending and 3 blocked. This page never states a state that `REQUIREMENTS.md` does not.

## What is simulated

Everything that touches the world: the tape, the motor, the sensors and the bus (feeder), or the pump, the valves, the parts, the sensors and the axes (head). The numbers of the simulation are made up; no value in the settings has been tuned on hardware.

## Open questions

* Which bus the chain uses: RS-485 is assumed, CAN or I2C would carry the same frame; the choice waits for the first board. Options: RS-485 (long cable, many feeders, recommended), CAN (the same, with its own arbitration), I2C (short, fewer feeders).
* Whether there is one node for the chain with up to 32 feeders behind it (what is written) or a small board in each feeder that is itself a node of the network.
* Whether the default actuator is a stepper or a servo; both are settings, the servo is not written yet.
* Whether the frame stays our own (`eh3d.feeder/1`) or also speaks the protocol of another open feeder: the plan is to design our own and not to copy another.
