# SPSC

Lockless Single Producer Single Consumer queue in C

![Band](readme/band.png)

This is a simple C implementation of the lockless SPSC queue, most commonly used for communicating to hard-real-time audio threads.

Documentation can be found here: [splinter.com.au/2026/05/13/lockless-spsc-queue](https://www.splinter.com.au/2026/05/13/lockless-spsc-queue/).

To run, simply do:

    make run

This is less a library than sample code that you are encouraged to copy and paste into your project, modifying `QueueEntry` to suit the items you want to put in your queue.

Extracted from [BrickWarrior](https://github.com/chrishulbert/brickwarrior)'s sound engine.

![Band](readme/brothers.png)
