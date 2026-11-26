# EH3D-NODE-PNP-FEEDER: status

## Current state

EH3D-NODE-PNP-FEEDER is the logic of a node, tested against a simulator. It has a settings file, the rules of the tape feeders, the words of the node on the network and a simulator, the frame of the chain bus and the three buses it can travel on (RS-485, CAN and I2C). The tests run on Windows and Linux in the CI. Nothing has moved a real tape feeders: the firmware is not written, and nothing has been connected to a motor, a pump, a valve or a sensor.

## Capabilities

The official state of each capability is in `REQUIREMENTS.md`. The register has 90 requirements: 83 implemented, 0 prototype, 7 planned and 0 validated. By progress: 83 done, 0 partial, 4 pending and 3 blocked. This page never states a state that `REQUIREMENTS.md` does not.

## What is simulated

Everything that touches the world: the tape, the motor, the sensors and the bus (feeder), or the pump, the valves, the parts, the sensors and the axes (head). The numbers of the simulation are made up; no value in the settings has been tuned on hardware.

## Open questions

* Which bus a machine uses is a setting (`bus` = `rs485`, `can` or `i2c`); all three are written and tried against a simulator. Which one is best is decided on a bench: the table of bench tests in `HARDWARE.md` (F-11 to F-17) says what to measure for each, and the result of the same advances on each bus is the table that chooses.
* The chain is one node with up to 32 feeders behind it (decided). A small board in each feeder that is itself a node of the network is not planned.
* Whether the default actuator is a stepper or a servo; both are settings, the servo is not written yet.
* Whether the frame stays our own (`eh3d.feeder/1`) or also speaks the protocol of another open feeder: the plan is to design our own and not to copy another.
