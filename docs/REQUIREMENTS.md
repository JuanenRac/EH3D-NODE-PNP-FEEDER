# EH3D-NODE-PNP-FEEDER: requirements

This register is the single source of the state of each capability: `STATUS.md`, the README files and the capability matrix of the documentation hub cite it and never state a state that it does not. A state is one of `planned`, `prototype`, `implemented` and `validated`. `validated` means that a person has run the capability on real hardware and the evidence of the manifest says `bench` or `field`; nothing here is validated, because nothing of this node has run on a board.

The register has 54 requirements: 47 implemented, 0 prototype, 7 planned and 0 validated. By progress: 47 done, 0 partial, 4 pending and 3 blocked. High coverage is not proof of correctness, so each row says how it is checked: the test that names it, and what it shows.

## How to read the register

* **ID:** `PNPF-NNN`, never reused and never renumbered; a new capability takes the next number.
* **State:** `implemented` is code that is complete and tested on a computer, `prototype` is part of the capability, `planned` is not written.
* **Progress:** `done`, `partial`, `pending` (can be written without hardware) or `blocked` (waits for hardware or for another program of the suite).
* **Evidence:** `simulator` when the test runs against the simulated tape feeders, `host-test` when it checks a calculation or a reader with no simulated world, `none` when nothing is written yet.
* **Test:** the acceptance criterion: `file::name` is the test `name` of `tests/file.cpp`, which must exist. For a capability that is not written the cell says `planned:` and the name of the test that will show it; the day that test exists, the row must change.
* **Limits:** what is not done or not proven; a row that is `partial` or `blocked` always says it.

The check is `tools/test_node.py`: it reads this table, finds each test and fails when a state, a test or a count does not agree.

## Settings

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| PNPF-001 | The node is a plain settings file; a value out of range or a key nobody knows is refused with its key and the default stays. | implemented | done | host-test | `test_frame::a_value_out_of_range_or_a_key_nobody_knows_is_reported_and_the_default_stays` | - |
| PNPF-002 | The pitch of the tape is one of 2, 4, 8, 12 and 16 mm and nothing else. | implemented | done | host-test | `test_frame::the_pitches_are_two_four_eight_twelve_and_sixteen` | - |
| PNPF-003 | Settings that disagree with each other (the band of the tension, the window and the limit of faults) go back to the defaults. | implemented | done | host-test | `test_frame::settings_that_disagree_go_back_to_the_defaults` | - |
| PNPF-004 | The token is never written back to the settings text, and it has a limit of length. | implemented | done | host-test | `test_frame::the_settings_text_is_read_back_the_same_and_never_holds_the_token` | - |
| PNPF-005 | The defaults are the conservative ones: no token, no backward move, pocket and film checks on. | implemented | done | host-test | `test_frame::defaults_are_the_conservative_ones` | - |
| PNPF-006 | The example file shows every setting with its default and its range, and it is the defaults. | implemented | done | host-test | `test_frame::the_example_file_is_the_defaults_names_every_key_and_has_no_problem` | - |

## The frame of the chain bus

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| PNPF-007 | A frame of the bus carries a version, an address, a command, a sequence number and a CRC-16, and survives a round trip. | implemented | done | host-test | `test_frame::a_frame_survives_a_round_trip` | The frame has been checked on a computer only, never on a transceiver. |
| PNPF-008 | The CRC is the published CRC-16/CCITT-FALSE. | implemented | done | host-test | `test_frame::crc_matches_the_published_check_value` | - |
| PNPF-009 | The reader finds the next frame after noise, and a start byte inside a payload does not confuse it. | implemented | done | host-test | `test_frame::noise_between_frames_costs_nothing` | - |
| PNPF-010 | Any single changed bit of a frame is detected. | implemented | done | host-test | `test_frame::a_changed_bit_is_always_seen_and_the_next_frame_is_read` | - |
| PNPF-011 | A frame of another version, or with a length no frame has, is refused. | implemented | done | host-test | `test_frame::a_wrong_version_and_a_wrong_length_are_refused` | - |
| PNPF-012 | A lost or damaged frame is sent again, an order that was already carried out is not carried out twice, and a run with the same seed is the same run. | implemented | done | simulator | `test_unit::a_lost_or_damaged_frame_is_sent_again_and_the_run_is_the_same_every_time` | The loss and the damage are simulated; a real RS-485 line has not been measured. |

## The feeder

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| PNPF-013 | An advance moves the tape one pitch, counts a part and ends in the idle state. | implemented | done | simulator | `test_unit::an_advance_moves_one_pitch_and_counts_a_part` | The motor and the tape are simulated. |
| PNPF-014 | Each of the five pitches moves the tape by that pitch, alone or several at a time. | implemented | done | simulator | `test_unit::each_pitch_moves_the_tape_by_that_pitch` | - |
| PNPF-015 | The pitch of one feeder can be changed when it is still, and a wrong one is refused. | implemented | done | simulator | `test_unit::the_pitch_of_one_feeder_can_be_changed_when_it_is_still` | - |
| PNPF-016 | A tape that ends is reported as empty and the feeder refuses more. | implemented | done | simulator | `test_unit::a_tape_that_ends_is_reported_as_empty_and_refuses_more` | Needs a real tape sensor to be proven. |
| PNPF-017 | A counted reel stops when its count is used up and warns before. | implemented | done | simulator | `test_unit::a_counted_reel_stops_when_the_count_is_used_up_and_warns_before` | The count is told by a person; there is no reading of the reel. |
| PNPF-018 | A new spool clears the empty state. | implemented | done | simulator | `test_unit::a_new_spool_clears_the_empty_state` | - |
| PNPF-019 | A jam holds a fault, and three faults in a row disable the feeder until a person re-enables it. | implemented | done | simulator | `test_unit::a_jam_holds_a_fault_and_three_in_a_row_disable_the_feeder` | The stall is a flag in the simulator; reading it from a real driver is not written. |
| PNPF-020 | A feeder that works between its faults is not disabled (the window slides). | implemented | done | simulator | `test_unit::a_feeder_that_works_between_faults_is_not_disabled` | - |
| PNPF-021 | A slack or a tight tape is refused before the move. | implemented | done | simulator | `test_unit::a_slack_or_tight_tape_is_refused_before_the_move` | The tension sensor is a number in the simulator. |
| PNPF-022 | A tape that goes slack during a move stops it and holds a fault that is not cleared while the cause lasts. | implemented | done | simulator | `test_unit::a_tape_that_goes_slack_during_the_move_holds_a_fault_until_it_is_tight` | - |
| PNPF-023 | A cover film that is not peeled stops the feeder, and the fault is not cleared while it is there. | implemented | done | simulator | `test_unit::a_film_that_is_not_peeled_stops_the_feeder_and_is_not_cleared_while_it_is_there` | - |
| PNPF-024 | The film check can be switched off in the settings. | implemented | done | simulator | `test_unit::the_film_check_can_be_switched_off` | - |
| PNPF-025 | A pocket a little off is found with one small move after the advance. | implemented | done | simulator | `test_unit::a_pocket_a_little_off_is_found_with_one_small_move` | - |
| PNPF-026 | A pocket far off is not found: after the small moves the feeder holds a fault. | implemented | done | simulator | `test_unit::a_pocket_far_off_is_not_found_and_the_feeder_holds_a_fault` | - |
| PNPF-027 | A pitch set wrong for the tape in the feeder is caught by the pocket check. | implemented | done | simulator | `test_unit::a_pitch_set_wrong_for_the_tape_is_caught_by_the_pocket_check` | - |
| PNPF-028 | Without the pocket check a misplaced tape goes unnoticed (the check is a setting). | implemented | done | simulator | `test_unit::without_the_pocket_check_a_misplaced_tape_goes_unnoticed` | - |
| PNPF-029 | A backward move is refused unless the setting allows it. | implemented | done | simulator | `test_unit::a_backward_move_is_refused_unless_it_is_switched_on` | - |
| PNPF-030 | A command nobody knows, a bad argument and a bad slot are answered with an error. | implemented | done | simulator | `test_unit::a_command_nobody_knows_and_a_bad_argument_are_answered_with_an_error` | - |
| PNPF-031 | Identify tells the firmware version of the feeder. | implemented | done | simulator | `test_unit::identify_gives_the_firmware_version` | The blink of the LED is only a time in the simulator. |
| PNPF-032 | A whole reel with a slipping tape and a noisy bus ends in the same place every time. | implemented | done | simulator | `test_unit::the_simulation_of_a_whole_reel_ends_in_the_same_place_every_time` | - |

## The chain and the safe state

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| PNPF-033 | A feeder that loses the node stops the tape and holds the safe state until a person acknowledges. | implemented | done | simulator | `test_unit::a_feeder_that_loses_the_node_stops_the_tape_and_holds_the_safe_state` | - |
| PNPF-034 | A stop to all the feeders stops every move. | implemented | done | simulator | `test_unit::a_stop_to_all_stops_every_feeder` | - |
| PNPF-035 | An advance that does not finish in time is stopped and reported. | implemented | done | simulator | `test_unit::an_advance_that_does_not_finish_in_time_is_stopped` | - |
| PNPF-036 | A cut bus makes the slots offline, and they come back when it is restored. | implemented | done | simulator | `test_unit::a_bus_that_is_cut_makes_the_slots_offline_and_they_come_back` | - |
| PNPF-037 | A feeder that is unplugged is not listed and the others go on working. | implemented | done | simulator | `test_unit::a_feeder_that_is_unplugged_is_not_listed_and_the_others_work` | - |

## The node on the network

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| PNPF-038 | The status is always answered and is valid JSON. | implemented | done | simulator | `test_node::the_status_is_always_answered_and_is_valid_json` | - |
| PNPF-039 | A node without a token refuses everything that moves or changes something, and says so in its check. | implemented | done | simulator | `test_node::a_node_without_a_token_refuses_everything_that_moves_and_says_so` | - |
| PNPF-040 | The token goes in the header or in the query, and a wrong or a shorter one is refused. | implemented | done | host-test | `test_node::the_token_goes_in_the_header_or_in_the_query_and_a_wrong_one_is_refused` | - |
| PNPF-041 | An advance over HTTP moves the tape and says how it ended. | implemented | done | simulator | `test_node::an_advance_over_http_moves_the_tape_and_says_how_it_ended` | - |
| PNPF-042 | A bad request is a 400 with the word that is wrong, and an unknown path or verb is a 404 or a 405. | implemented | done | host-test | `test_node::a_bad_request_is_a_400_with_the_word_that_is_wrong` | - |
| PNPF-043 | What a feeder refuses comes back as a 409 with the reason, and a slot that is not there is a 503. | implemented | done | simulator | `test_node::what_the_feeder_refuses_comes_back_as_a_409_with_the_reason` | - |
| PNPF-044 | Count, pitch, identify and acknowledge work on one feeder over HTTP. | implemented | done | simulator | `test_node::count_pitch_identify_and_ack_work_on_one_feeder` | - |
| PNPF-045 | A server that keeps asking keeps the chain awake; a server that goes silent puts the node and the feeders in the safe state. | implemented | done | simulator | `test_node::a_server_that_goes_silent_puts_the_node_and_the_feeders_in_the_safe_state` | The server's plugin does not ask on this rhythm yet. |
| PNPF-046 | A heartbeat that stops is what the feeders see as a lost node. | implemented | done | simulator | `test_node::a_stopped_heartbeat_is_what_the_feeders_see_as_a_lost_node` | - |
| PNPF-047 | The words of the status are the words of the contract page, and every code and route of the node is in it. | implemented | done | host-test | `test_node::the_status_words_are_the_words_of_the_page_of_the_contract` | - |

## Not written yet

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| PNPF-048 | The firmware for the ESP32-S3 (network, panel, update, storage) on the shared base. | planned | pending | none | `planned:test_node::the_firmware_answers_the_words_of_the_contract` | Only ten plain helpers of the shared base are imported; the network, panel and update parts of the base are not. |
| PNPF-049 | A driver for the stepper motor or the servo, with the counting of steps and the stall reading. | planned | blocked | none | `planned:test_unit::a_motor_driver_moves_the_tape_by_the_steps_it_counts` | Waits for a motor and a driver on a bench. |
| PNPF-050 | The tape sensors (an infrared reflection and an array of phototransistors) and the tension sensor read for real. | planned | blocked | none | `planned:test_unit::the_tape_sensors_are_read_through_a_filter` | Waits for the sensors on a bench; the thresholds of the settings are unproven. |
| PNPF-051 | The driver of the RS-485 transceiver, with the direction line and the timing of a frame. | planned | blocked | none | `planned:test_frame::the_driver_of_the_bus_sends_and_receives_frames` | Waits for a transceiver and a long chain on a bench. |
| PNPF-052 | Automatic addressing of the chain and finding the feeders when the power is switched on. | planned | pending | none | `planned:test_unit::the_feeders_get_their_address_in_the_order_of_the_chain` | Today the address of a feeder is its slot. |
| PNPF-053 | The correction of the pick position from the error the lower camera measures. | planned | pending | none | `planned:test_unit::the_pick_error_of_the_camera_moves_the_pick_position` | Needs the camera program; the model of the correction is in the pick-and-place program. |
| PNPF-054 | The servo actuator: the mapping from millimetres to degrees and its limits. | planned | pending | none | `planned:test_unit::a_servo_feeder_moves_the_tape_by_degrees` | The setting exists; no code uses it yet. |
