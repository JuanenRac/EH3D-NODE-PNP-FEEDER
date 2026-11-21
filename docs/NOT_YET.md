# EH3D-NODE-PNP-FEEDER: not yet

Good ideas that are not in the current version. Writing them here keeps anybody from implementing them early or twice.

| Idea | Why not yet | Revisit when |
|---|---|---|
| The firmware for the board | The logic is written and tested first; the network, panel and update parts of the shared base are imported when the firmware is | the first board is chosen |
| A motor driver and the tape sensors | They need a motor and sensors on a bench to be tuned | a bench with one feeder is there |
| Another bus, or two buses at once | The chain uses one of RS-485, CAN or I2C, chosen in the settings; a mixed chain needs a bridge that nobody asked for | a machine needs a feeder on a bus the node does not speak |
| Automatic addressing of the chain | The address is the slot today; the order of the chain needs the real wiring | the wiring of the chain is decided |
| A feeder for trays, tubes or loose parts | This node is for tape; the pick-and-place program already models the other kinds as data | someone needs a powered tray |
| The correction of the pick position from the lower camera | It needs the camera program and a feeder that moves in two axes | after the live camera |
| A compatible mode with the protocol of another open feeder | The plan is to design our own frame and not to copy another | somebody asks for it and the licence allows it |
