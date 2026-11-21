# EH3D-NODE-PNP-FEEDER: scope

Status: **simulator**. EH3D-NODE-PNP-FEEDER is the logic of a node for the control of a chain of tape feeders of a pick-and-place machine: it advances the tape of each feeder by a pitch of 2, 4, 8, 12 or 16 mm, counts the parts, watches the pocket, the tension and the cover film, holds a fault when the tape jams, and stops by itself when it loses the node or the server. It is written in C++17 for the ESP32-S3 and is tried today only on a computer, against a simulator. It has not been built into firmware, flashed or connected to anything.

## Purpose

The node that will stand between the pick-and-place program of the suite (EH3D-PHOENIX-PNP), through the server, and the real tape feeders of a machine. The program plans and describes; the node carries out one small, checked action at a time and refuses what is unsafe.

It is for the assembly of boards and of their components. It is not for taking up objects, for arms or for robots.

## What it does

* Speaks the frame `eh3d.feeder/1` (a CRC-16, an address, a sequence number) to up to 32 feeders on a bus that is RS-485, CAN or I2C (a setting), sends a frame again when no good answer comes back, and never makes the same move twice when only the answer was lost.
* Advances the tape by a pitch of 2, 4, 8, 12 or 16 mm, one to eight pitches at a time, and checks after the move that the pocket is where it should be (with a few small moves to find it).
* Counts the parts left when a person tells it, warns when few are left and refuses to go on when none are.
* Holds a fault for a jam, a slack or a tight tape, a cover film that is not peeled, a pocket that is not found and a lost node, and does not clear a fault while its cause is there; a feeder that faults too often in a short window disables itself until a person re-enables it.
* Answers over HTTP with the words of the other nodes of the suite: a status, one feeder, advance, retract (off by default), identify, count, pitch, acknowledge, stop and discover, all behind a token.
* Stops the chain by itself when the server is not heard, and the feeders stop by themselves when the node is not heard.
* Carries the same frame over RS-485 (with the direction line of its transceiver), CAN (in pieces of seven bytes) or I2C (with 7-bit addresses), refuses a chain that does not fit its bus, and counts what the bus held, lost or delayed.
* Comes with a simulator of the chain (a tape that moves, sensors, a bus of the kind the settings name that loses, damages and delays frames) so that all of this is tried on a computer with a fixed seed.

## What it will not do

* No decision about the limits of a machine, its interlocks or its emergency stop (see below).
* No handling of objects, no arms and no robots: this is the feeder of a machine that places components on boards.
* No tape sensor, motor, transceiver, CAN controller or I2C master is driven yet: that is the firmware, which is not written.

## What it never decides

The stop, the limits of the axes, the interlocks and the emergency stop of the machine belong to the machine and its controller. The interlocks and the safe state of this node are a second line behind those, they only ever make a move refused or slower, and they never replace them. A change that would relax one after a trouble is a defect.

## Where it sits

* **EH3D-PHOENIX-PNP** plans the jobs and describes the nodes; its register already waits for this node.
* **EH3D-SERVER** has a plugin for pick-and-place machines that today simulates them; this node is what will stand behind it.
* **EH3D-COMMON** holds the firmware base (the plain helpers of every node) that this repository takes ten files from.
* **EH3D-NODE-PNP-HEAD** is the other half: the node of the head.
