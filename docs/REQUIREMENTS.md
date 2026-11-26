# EH3D-NODE-PNP-FEEDER: requirements

This register is the single source of the state of each capability: `STATUS.md`, the README files and the capability matrix of the documentation hub cite it and never state a state that it does not. A state is one of `planned`, `prototype`, `implemented` and `validated`. `validated` means that a person has run the capability on real hardware and the evidence of the manifest says `bench` or `field`; nothing here is validated, because nothing of this node has run on a board.

The register has 90 requirements: 83 implemented, 0 prototype, 7 planned and 0 validated. By progress: 83 done, 0 partial, 4 pending and 3 blocked. High coverage is not proof of correctness, so each row says how it is checked: the test that names it, and what it shows.

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
| PNPF-051 | The drivers of the bus transceivers: the direction line (DE/RE) of the RS-485 transceiver, the CAN controller and the I2C master, with the timing of a frame. | planned | blocked | none | `planned:test_frame::the_driver_of_the_bus_sends_and_receives_frames` | Waits for a transceiver, a CAN controller and a long chain on a bench; the three buses are simulated in `core/feeder_transport.hpp`. |
| PNPF-052 | Automatic addressing of the chain and finding the feeders when the power is switched on. | planned | pending | none | `planned:test_unit::the_feeders_get_their_address_in_the_order_of_the_chain` | Today the address of a feeder is its slot. |
| PNPF-053 | The correction of the pick position from the error the lower camera measures. | planned | pending | none | `planned:test_unit::the_pick_error_of_the_camera_moves_the_pick_position` | Needs the camera program; the model of the correction is in the pick-and-place program. |
| PNPF-054 | The servo actuator: the mapping from millimetres to degrees and its limits. | planned | pending | none | `planned:test_unit::a_servo_feeder_moves_the_tape_by_degrees` | The setting exists; no code uses it yet. |

## The three buses

| ID | Requirement | State | Progress | Evidence | Test | Limits |
|---|---|---|---|---|---|---|
| PNPF-055 | The bus of the chain (RS-485, CAN or I2C) is chosen in the settings, RS-485 is the default, and the names are read back the same. | implemented | done | host-test | `test_bus::the_bus_is_chosen_in_the_settings_and_each_name_is_read` | - |
| PNPF-056 | A bus, rate, identifier, address or time-out out of its range or not in its list is refused with its key and the default stays. | implemented | done | host-test | `test_bus::a_bus_value_nobody_knows_is_refused_and_the_default_stays` | - |
| PNPF-057 | The example file gives the range of every key of the buses next to its default. | implemented | done | host-test | `test_bus::the_example_file_gives_the_range_of_every_bus_key` | - |
| PNPF-058 | A chain holds up to 32 feeders on any bus; on I2C the addresses from the base must not pass the last usable one, and a longer chain goes back to the defaults. | implemented | done | host-test | `test_bus::a_chain_cannot_be_longer_than_its_bus_holds` | The 32 are the 32 addresses of the frame; the number of feeders a real wire holds is a bench test. |
| PNPF-059 | A rate so slow that the longest frame and its answer do not fit in the reply time-out goes back to the default. | implemented | done | host-test | `test_bus::a_rate_too_slow_for_the_reply_timeout_goes_back_to_the_default` | The times are the nominal ones of each bus. |
| PNPF-060 | The same advance works on the three buses and moves the same tape. | implemented | done | simulator | `test_bus::the_same_advance_works_on_the_three_buses` | The bus is simulated; a real wire has not been measured. |
| PNPF-061 | A chain of 32 feeders is found and driven on every bus, up to the last address. | implemented | done | simulator | `test_bus::a_chain_of_thirty_two_feeders_is_found_and_driven_on_every_bus` | The bus is simulated; a real wire has not been measured. |
| PNPF-062 | The frame the feeders are given is the same bytes on every bus. | implemented | done | simulator | `test_bus::the_frame_is_the_same_bytes_on_every_bus` | The bus is simulated; a real wire has not been measured. |
| PNPF-063 | A command to all is heard by all and never answered on every bus (the general call on I2C). | implemented | done | simulator | `test_bus::a_command_to_all_is_heard_by_all_and_never_answered_on_every_bus` | The bus is simulated; a real wire has not been measured. |
| PNPF-064 | A cut bus is the same on every bus: the slot goes offline after the repeats and comes back at the discovery. | implemented | done | simulator | `test_bus::a_cut_bus_is_the_same_thing_on_every_bus_and_comes_back` | The bus is simulated; a real wire has not been measured. |
| PNPF-065 | RS-485 waits the turnaround of the direction line before each frame and takes the time of the bytes at the baud rate. | implemented | done | simulator | `test_bus::rs485_waits_the_turnaround_before_each_frame_and_the_time_of_the_bytes` | The turnaround is a setting; the real one is measured on a bench. |
| PNPF-066 | A slower RS-485 rate and a longer turnaround take longer by the amount they should. | implemented | done | simulator | `test_bus::a_slower_rs485_rate_and_a_longer_turnaround_take_longer` | The bus is simulated; a real wire has not been measured. |
| PNPF-067 | Two talkers at once on RS-485 lose both frames and are counted. | implemented | done | simulator | `test_bus::two_talkers_at_once_lose_both_frames_on_rs485` | The bus is simulated; a real wire has not been measured. |
| PNPF-068 | CAN cuts a frame of the chain in pieces of seven bytes (eight data bytes in each CAN frame) and puts it back, for every length of payload. | implemented | done | host-test | `test_bus::can_cuts_a_frame_in_pieces_of_seven_bytes_and_puts_it_back` | The cut is our own; it has not met a real CAN controller. |
| PNPF-069 | CAN pieces that are missing, out of order or of another identifier do not make a frame and are counted. | implemented | done | host-test | `test_bus::can_pieces_out_of_order_or_missing_or_of_another_frame_do_not_make_a_frame` | - |
| PNPF-070 | CAN identifiers name the feeder, the answers have their own, and all stay inside eleven bits. | implemented | done | host-test | `test_bus::can_identifiers_name_the_feeder_and_stay_inside_eleven_bits` | - |
| PNPF-071 | The time of a CAN frame is the bits over the bit rate. | implemented | done | simulator | `test_bus::can_time_on_the_wire_is_the_bits_over_the_bitrate` | Bit stuffing, which adds up to a fifth, is not counted. |
| PNPF-072 | The CAN hardware sends a damaged frame again and a lost piece loses the frame. | implemented | done | simulator | `test_bus::can_hardware_sends_a_damaged_frame_again_and_a_lost_piece_loses_the_frame` | The bus is simulated; a real wire has not been measured. |
| PNPF-073 | CAN arbitration only costs a frame time, and a busy bus gives the try up. | implemented | done | simulator | `test_bus::can_arbitration_costs_a_frame_time_and_a_busy_bus_gives_the_try_up` | The bus is simulated; a real wire has not been measured. |
| PNPF-074 | CAN frames that nobody acknowledges are counted and the slot goes offline. | implemented | done | simulator | `test_bus::can_frames_that_nobody_acknowledges_are_counted_and_the_slot_goes_offline` | The bus is simulated; a real wire has not been measured. |
| PNPF-075 | I2C addresses are seven bits from the base, never a reserved one, and the general call is address 0. | implemented | done | host-test | `test_bus::i2c_addresses_are_seven_bits_from_the_base_and_never_a_reserved_one` | - |
| PNPF-076 | The time of an I2C exchange is nine clocks a byte at the clock of the bus. | implemented | done | simulator | `test_bus::i2c_time_on_the_wire_is_nine_clocks_a_byte` | The bus is simulated; a real wire has not been measured. |
| PNPF-077 | An I2C feeder that is not there does not acknowledge its address: a NACK, counted, that costs a few clocks. | implemented | done | simulator | `test_bus::i2c_a_feeder_that_is_not_there_does_not_acknowledge_its_address` | The bus is simulated; a real wire has not been measured. |
| PNPF-078 | A clock held within the limit costs time and beyond the limit is a time-out. | implemented | done | simulator | `test_bus::i2c_a_clock_held_within_the_limit_costs_time_and_beyond_it_is_a_timeout` | The limit is a setting; the longest hold of a real feeder is a bench test. |
| PNPF-079 | The I2C general call reaches every feeder and is not answered. | implemented | done | simulator | `test_bus::i2c_the_general_call_reaches_every_feeder_and_is_not_answered` | The bus is simulated; a real wire has not been measured. |
| PNPF-080 | A second I2C master costs the time of the frame again. | implemented | done | simulator | `test_bus::i2c_a_second_master_costs_the_frame_time_again` | The bus is simulated; a real wire has not been measured. |
| PNPF-081 | Lost and damaged frames are sent again on every bus. | implemented | done | simulator | `test_bus::lost_and_damaged_frames_are_sent_again_on_every_bus` | The bus is simulated; a real wire has not been measured. |
| PNPF-082 | A bus held by somebody else costs a try and is counted on every bus. | implemented | done | simulator | `test_bus::a_bus_held_by_somebody_else_costs_a_try_and_is_counted_on_every_bus` | The bus is simulated; a real wire has not been measured. |
| PNPF-083 | Two talkers at once only delay CAN and I2C and cost RS-485 its frames. | implemented | done | simulator | `test_bus::two_talkers_at_once_only_delay_can_and_i2c_and_cost_rs485_its_frames` | The bus is simulated; a real wire has not been measured. |
| PNPF-084 | An answer slower than the reply time-out is late and is not delivered, on every bus. | implemented | done | simulator | `test_bus::an_answer_slower_than_the_reply_timeout_is_late_and_not_delivered_on_every_bus` | The bus is simulated; a real wire has not been measured. |
| PNPF-085 | The same seed gives the same run on every bus and another seed another run. | implemented | done | simulator | `test_bus::the_same_seed_gives_the_same_run_on_every_bus_and_another_seed_another_run` | The bus is simulated; a real wire has not been measured. |
| PNPF-086 | A slower bus makes the same advance take longer but move the same tape. | implemented | done | simulator | `test_bus::a_slower_bus_makes_the_same_advance_take_longer_but_move_the_same_tape` | The bus is simulated; a real wire has not been measured. |
| PNPF-087 | The node says its bus in the status and counts what was busy or collided. | implemented | done | simulator | `test_bus::the_node_says_the_bus_and_counts_what_was_busy_or_collided` | The bus is simulated; a real wire has not been measured. |
| PNPF-088 | The page of the contract names the three buses and every key of them, and the page of the hardware lists the bench test of each. | implemented | done | host-test | `test_bus::the_page_of_the_contract_names_the_three_buses_and_every_key_of_them` | - |
| PNPF-089 | The status says the time of the watch over the server, with or without the token: the value of the settings the node runs with. | implemented | done | host-test | `test_node::the_status_says_the_server_time_of_the_settings_with_or_without_the_token` | - |
| PNPF-090 | The time the status says is the time the node keeps, and reading the status without the token does not keep the watch. | implemented | done | simulator | `test_node::the_server_time_of_the_status_is_the_time_the_node_keeps` | The time it takes on a board is a bench test. |
