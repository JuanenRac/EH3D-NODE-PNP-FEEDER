# Changelog

All notable changes to this project are documented here.

## [Unreleased]

## [0.0.1] - The logic of the node and its simulator

- **The settings of the node:** `feeder.cfg` with safe defaults (no token, no backward move, the pocket and the film checked), a pitch of 2, 4, 8, 12 or 16 mm, ranges for every value, and a refusal with the key for a value out of range or a key nobody knows.
- **The frame `eh3d.feeder/1` of the chain bus:** an address, a command, a sequence number and a CRC-16, a reader that finds the next frame after noise, and a repeat that never makes the same move twice.
- **The feeder:** advance, count, a tape that ends, a jam, a slack or tight tape, a cover film that is not peeled, a pocket that is not found, a feeder that disables itself after too many faults, and a safe state when the node is lost.
- **The chain and the node:** repeats and discovery, an advance that is followed until the move is over, the words of the node over HTTP behind a token, the stop of the chain when the server is not heard, and a self-check with codes.
- **A simulator of the chain** with a tape, sensors and a bus that loses and damages frames, with a fixed seed.
- **68 tests** and a register of 54 requirements that names the test of each; the page `docs/CONTRACT.md` is read by a test and cannot go stale.
- **Ten plain helpers of the firmware base** of EH3D-COMMON, kept identical by its tool; the firmware itself is not written.
