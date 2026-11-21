# What the node says: on the network and on the chain

The server, the desktop programs and the phones talk to the node over HTTP; the node talks to its feeders over a bus (RS-485, CAN or I2C, chosen in the settings file) with the frame `eh3d.feeder/1`. The words of the first are built in `core/feeder_node.hpp`, the frame in `core/feeder_frame.hpp`, how the frame travels on each bus in `core/feeder_transport.hpp`, what a feeder does with a frame in `core/feeder_unit.hpp`, and `core/feeder_sim.hpp` is a chain on a computer that answers the very same words. A host test (`tests/test_node.cpp`) reads this page and fails when a key or a code is in the page and not in the answer, or the other way round, so the page cannot go stale.

Nothing in this page has been checked against a real feeder or a real bus: it is checked against the simulator. The numbers of the simulated tape and noise are made up.

The node reuses the way of the other nodes of the suite: one line of JSON per answer, a token as `Authorization: Bearer <token>` or `?token=`, a program ignores a key it does not know. It adds nothing to the server's own protocol: the server's plugin for pick-and-place machines keeps its feeders by slot, and this node is what stands behind a slot when a real machine is connected.

## `GET /api/status`

Always answered, with or without the token. One line of JSON.

| Key | Type | Meaning |
|---|---|---|
| `node` | text | the name of the node |
| `version` | text | the version of the firmware |
| `slots` | number | the slots of the chain, from the settings |
| `online` | number | the slots that answered the last time they were asked |
| `safe` | bool | the node is in its safe state: it stopped the chain because the server was not heard, and moves nothing until a person acknowledges |
| `safe_reason` | text | `server_lost`, or `none` |
| `bus_exchanges` | number | frames sent on the bus since the start, the repeats included |
| `bus_retries` | number | frames sent again because no good answer came back |
| `bus_errors` | number | exchanges without a good answer |
| `bus` | text | the bus of the chain, from the settings: `rs485`, `can` or `i2c` |
| `bus_busy` | number | tries given up because the bus was held by somebody else |
| `bus_collisions` | number | times two talkers were on the bus at once |
| `uptime_s` | number | seconds since the start |
| `selfcheck` | text | `ok`, `warn` or `fail` (see below) |
| `selfcheck_codes` | text | the codes behind the verdict, separated by commas; empty when there are none |
| `feeders` | list | one entry per slot, as in the next section (without `firmware`) |

## `GET /api/feeder?slot=N`

Needs the token. One feeder.

| Key | Type | Meaning |
|---|---|---|
| `slot` | number | 1 to `slots` |
| `online` | bool | the feeder answers |
| `state` | text | `unknown` (not asked yet), `idle`, `advancing`, `retracting`, `fault`, `empty`, `disabled` or `safe` |
| `fault` | text | the code of the fault it holds, or `none` |
| `remaining` | number or null | the advances left in the tape, as a person counted them; null when it is not counted |
| `low` | bool | the count is at or below the `low_pockets` setting |
| `pitch_mm` | number | how far one advance moves the tape: 2, 4, 8, 12 or 16 |
| `tape` | bool | the sensor sees tape |
| `pocket` | bool | the sensor sees a pocket at the picking place |
| `tension_ok` | bool | the tension is inside its band |
| `film_clear` | bool | the cover film is peeled |
| `firmware` | text | the version of the feeder's firmware, read by `identify` (only in this answer) |

## The requests that move or change

They need the token (a node with no token refuses them with `401`). A word that does not fit is a `400` with `{"error":"bad_request","key":"<word>"}`. While the node is in its safe state, every request that moves or changes a feeder is a `409` with `{"error":"safe_state"}`.

* `POST /api/feeder/advance?slot=N[&count=K]` moves the tape K pitches (1 to 8, 1 by default) and answers when the move is over. The answer is `{"ok":true,"why":"ok","slot":N,"feeder":{...}}` or, with `ok` false, `why` says what stopped it: `bad_argument`, `busy`, `fault`, `empty`, `tension`, `disabled`, `safe_state`, `timeout` (the move did not finish within `advance_timeout_ms` and was stopped), `offline` or `no_answer`. `offline` and `no_answer` are `503`, the rest `409`.
* `POST /api/feeder/retract?slot=N[&count=K]` the same backwards; `retract_off` unless the setting `retract_enabled` is on.
* `POST /api/feeder/identify?slot=N` the feeder blinks its LED and gives its firmware version.
* `POST /api/feeder/count?slot=N&n=K` says how many advances are left in the tape (0 to 65534).
* `POST /api/feeder/pitch?slot=N&mm=P` sets the pitch of one feeder (2, 4, 8, 12 or 16), when it is still.
* `POST /api/feeder/ack?slot=N[&reenable=1]` clears a fault whose cause is gone (`still_active` while it is there); `reenable=1` is needed for a feeder disabled by too many faults. Without a slot it clears the safe state of the node (`cleared`, or `not_held`).
* `POST /api/feeder/stop` stops every feeder at once.
* `POST /api/discover` asks every slot again who is there.

The server's own routes and codes (for example `409 pnp_feeder_short`) are not changed by this node.

## The self-check

The node judges itself and says it in `selfcheck` and `selfcheck_codes`. It never moves anything to do so. The verdict is `fail` when any code of severity `fail` is raised, `warn` when the worst is `warn`, `ok` otherwise (`info` codes do not change it). A code is never reused for another meaning.

| Code | Severity | Raised when |
|---|---|---|
| `FDN-S01` | warn | the node has no token: it refuses every command that moves or changes something |
| `FDN-S02` | fail | the node is in its safe state |
| `FDN-B01` | warn | some slots do not answer |
| `FDN-B02` | fail | no slot answers |
| `FDN-F01` | warn | a feeder holds a fault, is disabled or is in its safe state |
| `FDN-E01` | info | a tape is low or empty |

## The faults of a feeder

A feeder that holds a fault moves nothing until it is acknowledged, and it is acknowledged only when the cause is gone. `FDR-D01` is held by a feeder that had `fault_limit` faults in its last `fault_window` advances; only `reenable=1` clears it.

| Code | Held when |
|---|---|
| `FDR-J01` | the motor stalled against the tape (a jam) |
| `FDR-J02` | the pocket was not found after the advance and its small repeat moves |
| `FDR-T01` | the tape is slack |
| `FDR-T02` | the tape is too tight |
| `FDR-F01` | the cover film is not peeled |
| `FDR-L01` | the feeder heard nothing from the node for `link_lost_ms`: it stopped and holds the safe state |
| `FDR-D01` | disabled by too many faults |

## The safe state

* A feeder that has heard the node once and then nothing for `link_lost_ms` stops the tape where it is and holds `FDR-L01`.
* The node sends a heartbeat to all the feeders three times in every `link_lost_ms`, only while it is itself fine.
* A node that has not heard the server (a request with the token) for `server_lost_ms` sends a stop to all, stops the heartbeat (so the feeders go to their safe state a moment later) and refuses to move anything until a person acknowledges it. The server must therefore ask something with the token at least once in that time.
* A cut bus makes the feeders stop on their own and the node mark the slots offline after `offline_after` exchanges without an answer.

## The frame `eh3d.feeder/1`

```
0x7E | version (1) | address | command | sequence | length | payload (0 to 32 bytes) | CRC-16 (big endian)
```

The CRC is CRC-16/CCITT-FALSE (polynomial 0x1021, start 0xFFFF) over everything between the start byte and the CRC. Address 0 is the node, 1 to 32 a feeder, 0xFF all of them (never answered). The answer carries the command with the high bit set (0x80); an error is the command 0xFF with a code in the first byte of the payload. A feeder answers an order it already ran (the same sequence number, sent again because the answer was lost) with its status and does not move twice.

| Command | Code | Payload |
|---|---|---|
| PING | 0x01 | none; answered with the status |
| IDENTIFY | 0x02 | none; answered with the firmware version, 3 bytes |
| ADVANCE | 0x03 | pitches, 1 to 8 |
| RETRACT | 0x04 | pitches, 1 to 8 |
| STATUS | 0x05 | none |
| SET_COUNT | 0x06 | the advances left, 2 bytes (0xFFFF: not counted) |
| STOP | 0x07 | none |
| ACK | 0x08 | 1 byte: 1 also re-enables a disabled feeder |
| SET_PITCH | 0x09 | 2, 4, 8, 12 or 16 |

The status is 6 bytes: the state, the fault, the count (2 bytes), the flags (1 tape, 2 pocket, 4 tension ok, 8 film clear, 16 low) and the pitch.

## The bus under the chain

The frame is the same on the three buses; the setting `bus` says which one the node is wired to, and its own keys give the rate and the addresses. A chain holds up to 32 feeders on any of them. A node whose settings ask for more slots than the bus holds, or whose rate is so slow that the longest frame and its answer do not fit in `reply_timeout_ms`, puts the slots, or the rate, back to the defaults and says which key it refused.

| | RS-485 | CAN | I2C |
|---|---|---|---|
| Settings | `rs485_baud`, `rs485_turnaround_us` | `can_bitrate`, `can_id_base` | `i2c_clock_hz`, `i2c_base_addr`, `i2c_stretch_limit_us` |
| Wiring | one twisted pair, half duplex, a transceiver with a direction line (DE/RE) and a termination at both ends | one twisted pair with the two ends terminated | two lines (clock and data) with pull-ups, short and on one board or one rack |
| Address | the address inside the frame | the identifier: `can_id_base + N` to feeder N, `can_id_base + 64 + N` back, `can_id_base` to all | the 7-bit address `i2c_base_addr + N - 1`, the general call (0) to all |
| Frame on the wire | the whole frame, as it is | cut in pieces of 7 bytes, each in a CAN frame whose first byte says the piece and the count | written whole to the address, then the answer is read |
| Faults the bus can show | a frame lost or damaged, a late answer, a held line, two drivers at once | a lost piece, a frame the hardware sends again, no acknowledgement, arbitration lost | no acknowledgement of the address, a byte not acknowledged, a clock held too long |

A frame that arrives damaged is refused by its CRC on any bus, and the node sends it again `bus_retries` times; a feeder that does not answer `offline_after` times in a row is offline. Everything in this section is checked against the simulator in `core/feeder_transport.hpp`; the rates, the lengths of cable and the number of feeders a real wire holds are listed as bench tests in `HARDWARE.md`.

## The simulator

`core/feeder_sim.hpp` is a chain of feeders on a computer: a tape that moves and is read by sensors, and a bus that can lose or damage frames, with faults that can be switched on (a jam, a tape that ends, a slack or tight tape, a film that is not peeled, a pocket that is off, a feeder that does not answer, a bus that is cut). The random numbers come from a seed, so a run is the same every time. `tests/test_unit.cpp` and `tests/test_node.cpp` run it.
