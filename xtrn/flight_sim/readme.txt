                              Flight Simulator
                                 Version 1.00
                           Release date: 2026-03-09

                                   From:

                                Eric Oulashin
                          Sysop of Digital Distortion
                  BBS internet address: digitaldistortionbbs.com
                     Alternate address: digdist.synchro.net
                        Email: eric.oulashin@gmail.com

Contents
========
1. Disclaimer
2. Introduction
3. Installation
4. Configuration file

1. Disclaimer
=============
I cannot guarantee that this script is 100% free of bugs.  However, I have
tested it, and I used it often during development, so I have some confidence
that there are no serious issues with it (at least, none that I have seen).


2. Introduction
===============
This is a simple flight simulator game written in JavaScript for Synchronet.
I used Claude AI to completely code this (aside from a few small edits, such as
allowing configured weather probability, adding a version number, & editing the
main screen/mode screen input text to specify you can press Q to quit). I
created this as a sort of proof-of-concept and fun little flight simulator game
for Synchronet to see what Claude AI could come up with as far as a flight
simulator game for a BBS.

This game requires an ANSI terminal and has 3 modes: 2D side-scrolling, full 3D
view (which shows a view of the outside of the airplane from the rear), and
full 3D with RIP (if the user has a RIP-capable terminal).  When making this
game, I asked Claude AI to model the graphics in Microsoft Flight Simulator 4.0
(1989), which seemed at least somewhat feasible to model with ASCII/CP437 and
should be reasonbly comparable to RIP graphics; I think the graphics only
roughly look like that, but I thought it looked okay enough to release.

In RIP mode, the graphics are flickery; I suspect that may be a limitation of
RIP.

I also asked Claude AI to cycle through simulated weather: Sunny, cloudy,
rainy, and thunderstorm. Probabilties for the types of weather are configurable
in the configuration file (see section 4).

The controls are fairly simple (and are shown on screen): The user can use the
arrow keys to control the airplane (with the down arrow pulling up and up arrow
pushing down).

This game is single-player for now.  In the future, it may be cool to update
this to allow multi-player, showing other peoples' aircraft in the sky; in
particular, real-time inter-BBS play could be cool.


3. Installation
===============
To install, go to SCFG > External Programs > Online Programs (Doors), choose one of your
configured sections, and create an entry that looks like this:
   Name                            Flight Simulator
   Internal Code                   FLIGHT_SIM
   Start-up Directory              ../xtrn/flight_sim
   Command line                    ?flight_sim.js
   Clean-up Command Line
   Execution Cost                  None
   Access Requirements             ANSI
   Multiple Concurrent Users       Yes
   Native Executable               No
   I/O method                      FOSSIL or UART
   Use Shell or New Context        No
   Modify User Data                No
   Execute on Event                No
   Pause After Execution           No
   Disable Local Display           No
   BBS Drop File Type              None
   Place Drop File In              Node Directory


4. Configuration file
=====================
flight_sim.example.ini is an example of a configuration file for the flight
simulator.  If you want to customize your configuration, copy
flight_sim.example.ini to flight_sim.ini (it can be in the same directory or in
sbbs/mods) and make your customizations.

Currently, there is a [WEATHER] section, which contains weather settings. The
weather settings include probabilities for certain types of weather, and each
probability needs to be a number betweeen 0.0 (0%) and 1.0 (100%). They should
all add up to 1.0.

Configuration Settings
----------------------
Setting                               Description
-------                               -----------
listInterfaceStyle                    String: The user interface to use for message

sunny_probability                     The probability of sunny weather. This
                                      should be a number between 0.0 (0%) and
                                      1.0 (100%).

cloudy_probability                    The probability of cloudy weather. This
                                      should be a number between 0.0 (0%) and
                                      1.0 (100%).

rainy_probability                     The probability of rainy weather. This
                                      should be a number between 0.0 (0%) and
                                      1.0 (100%).

thunderstorm_probability              The probability of thunderstorms. This
                                      should be a number between 0.0 (0%) and
                                      1.0 (100%).
