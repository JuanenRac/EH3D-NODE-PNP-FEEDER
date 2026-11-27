# Changelog

All notable changes to this project are documented here.

## [Unreleased]

- **The shared lessons check reads only what is part of the repository:** what git ignores, and folders such as `managed_components` that a firmware build downloads, are no longer analysed, so the local validation no longer fails over downloaded code.
- **The node says how long it goes without being asked:** `GET /api/status` has `server_lost_ms`, the value of the settings the node runs with, with or without the token, so a server can tell when it asks too slowly. Two new tests, on the simulator only; nothing has run on a board.
- **The READMEs say what the node is today:** it works in simulation with tested logic, with no firmware and no hardware; they no longer call it a scaffold.
- **A long advance no longer sends the other feeders to their safe state (fix):** while the node waited for the end of a move it sent no heartbeat, so a move longer than `link_lost_ms` (eight large pitches, or a slow speed) left every other feeder of the chain holding `FDR-L01` until a person acknowledged it. The heartbeat now goes on during the move. One new test, on the simulator only.
- The shared workflow checker (`tools/workflow_hygiene.py`) is the current one of the documentation hub: the filters under a trigger (branches, paths, types) are no longer read as events.

## [0.0.2] - Three buses for the feeders, a pressure sensor or a vacuum switch for every nozzle and the lock of the X and Y axes

- **Three buses under one frame:** the chain can be wired with RS-485, CAN or I2C, chosen with `bus` in `feeder.cfg`, and the frame `eh3d.feeder/1` is the same on all of them; the node says its bus in the status.
- **RS-485:** the direction line of the transceiver and its turnaround are modelled, with the baud rate and the turnaround as settings.
- **CAN:** a frame is cut in pieces of seven bytes, each in a CAN frame with an identifier that names the feeder, and put back together; a lost or misplaced piece loses the frame and is counted; the bit rate and the first identifier are settings.
- **I2C:** each feeder has a 7-bit address from a settings base, a feeder that is not there does not acknowledge, a feeder may hold the clock up to a limit that is a setting, and the general call reaches all of them.
- **Limits of the chain:** up to 32 feeders on any bus; a chain that does not fit its bus, or a rate so slow that a frame does not fit in the reply time, goes back to the defaults with the key named.
- **Faults that can be put on a bus** with a fixed seed: lost, damaged and late frames, a bus held by somebody else, two talkers at once, a clock held too long and a cut cable; the status counts what was busy or collided.
- **Documents:** the page of the contract has a section on the buses and what changes in the wiring, the length and the rate; the hardware page has a bench test for each bus.
- **Tests:** 34 new tests for the buses, 102 in all.

## [0.0.1] - The logic of the node and its simulator

- **The settings of the node:** `feeder.cfg` with safe defaults (no token, no backward move, the pocket and the film checked), a pitch of 2, 4, 8, 12 or 16 mm, ranges for every value, and a refusal with the key for a value out of range or a key nobody knows.
- **The frame `eh3d.feeder/1` of the chain bus:** an address, a command, a sequence number and a CRC-16, a reader that finds the next frame after noise, and a repeat that never makes the same move twice.
- **The feeder:** advance, count, a tape that ends, a jam, a slack or tight tape, a cover film that is not peeled, a pocket that is not found, a feeder that disables itself after too many faults, and a safe state when the node is lost.
- **The chain and the node:** repeats and discovery, an advance that is followed until the move is over, the words of the node over HTTP behind a token, the stop of the chain when the server is not heard, and a self-check with codes.
- **A simulator of the chain** with a tape, sensors and a bus that loses and damages frames, with a fixed seed.
- **68 tests** and a register of 54 requirements that names the test of each; the page `docs/CONTRACT.md` is read by a test and cannot go stale.
- **Ten plain helpers of the firmware base** of EH3D-COMMON, kept identical by its tool; the firmware itself is not written.
