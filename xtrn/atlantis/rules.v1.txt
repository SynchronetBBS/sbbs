Return-Path: RWALLACE@vax1.tcd.ie
Received: from cgl.ucsf.edu by celeste.mmwb.ucsf.EDU (920330.SGI/GSC4.22)
	id AA09530; Mon, 14 Jun 93 01:07:23 -0700
Received: from vax1.tcd.ie by cgl.ucsf.EDU (5.65/GSC4.22)
	id AA17383 for troyer@celeste.mmwb.ucsf.EDU; Mon, 14 Jun 93 01:06:40 -0700
Received: from vax1.tcd.ie by vax1.tcd.ie (PMDF #12050) id
 <01GZD2WD5H3S004KXE@vax1.tcd.ie>; Mon, 14 Jun 1993 09:06 GMT
Date: Mon, 14 Jun 1993 09:06 GMT
From: RWALLACE@vax1.tcd.ie
Subject: Atlantis rules update
To: troyer@cgl.ucsf.edu
Message-Id: <01GZD2WD5H3S004KXE@vax1.tcd.ie>
X-Envelope-To: troyer@cgl.ucsf.edu
X-Vms-To: IN%"troyer@cgl.ucsf.edu"
Status: O

			Rules for Atlantis v1.0
			8 June 1993
			Copyright 1993 by Russell Wallace


Introduction
------------

Atlantis is an open-ended computer moderated fantasy game for any number of
players.  Players may attempt to carve out huge empires, become master
magicians, intrepid explorers, rich traders or any other career that comes to
mind.  There is no declared winner of the game; players set their own
objectives, and one can join at any time.

A player's position is called a "faction".  Each faction has a name and a
number (the number is assigned by the computer, and used for entering orders).
Each faction is composed of a number of "units", each unit being a group of one
or more people loyal to the faction.  You start the game with a single unit
consisting of one character, plus a sum of money.  More people can be hired
during the course of the game, and formed into more units.  (In these rules,
the word "character" generally refers either to a unit consisting of only one
person, or to a person within a larger unit.)

An example faction is shown below, consisting of a starting character, Merlin
the Magician, who has formed two more units, Merlin's Guards and Merlin's
Workers.  Each unit is assigned a unit number by the computer (completely
independent of the faction number), this is used for entering orders.  Here,
the player has chosen to give his faction the same name ("Merlin the Magician")
as his starting character.  Alternatively, you can call your faction something
like "The Great Northern Mining Company" or whatever.

  - Merlin the Magician (17), faction Merlin the Magician (27), $2300, default
    order "study magic".
  - Merlin's Guards (33), faction Merlin the Magician (27), number: 20, default
    order "study sword".
  - Merlin's Workers (34), faction Merlin the Magician (27), number: 50,
    default order "work".

A faction is considered destroyed, and the player knocked out of the game, if
ever all its people are killed or disbanded (i.e. the faction has no units
left).  The program does not consider your starting character to be special; if
your starting character gets killed, you will probably have been thinking of
that character as the leader of your faction, so some other character can be
regarded as having taken the dead leader's place (assuming of course that you
have at least one surviving unit!).  As far as the computer is concerned, as
long as any unit of the faction survives, the faction is not wiped out.  (If
your faction is wiped out, you can rejoin the game with a new starting
character.)

The reason for having units of many people, rather than keeping track of
individuals, is to simplify the game play.  The computer does not keep track of
individual names, possessions or skills for people in the same unit, and all
the people in a particular unit must be in the same place at all times.  If you
want to send people in the same unit to different places, you must split up the
unit.  Apart from this, there is no difference between having one unit of 50
people, or 50 units of one person each, except that the former is very much
easier to handle.


The World
---------

The Atlantis world is divided for game purposes into square regions, each 50
miles on a side.  Each region can have a terrain type of Plain, Mountain,
Forest, Swamp or Ocean.  Regions can contain units belonging to players, of
course; they can also have populations of peasants, as well as buildings
(usually constructed by players for the purpose of defense) and ships
(constructed by players for crossing oceans).

A region's location is given as X,Y coordinates, e.g. if you are in region 5,3
then to your east is 6,3 and south is 5,4.  The computer provides these for the
convenience of players; region coordinates are never entered in orders.  Each
region also has a name; again, this is provided only for the convenience of
players, and is never used in orders.

A turn is equal to one game month; every month is assumed to have 30 days for
the sake of simplicity.  (To maintain playability, the distance and time scales
are not always entirely realistic.)  A unit can do many actions at the start of
the month, that only take a matter of hours, such as buying and selling
commodities, or fighting an opposing faction.  Each unit can also do exactly
one action that takes up the entire month, such as harvesting timber or moving
from one region to another.  The commands which take an entire month are BUILD,
CAST, ENTERTAIN, MOVE, PRODUCE, RESEARCH, SAIL, STUDY, TEACH and WORK.

Any unit which is not given a command for a particular month will repeat
whatever it did last month.  Movement commands are the exception to this, so if
a unit gets the command WORK in January, then MOVE NORTH in February, and no
command in March, it will settle down to WORK again, rather than continue to
MOVE NORTH.  All newly created units start off with WORK as the default
command.


Movement
--------

In one month, it is possible to move from a region to an adjacent region.  Each
region has four adjacent regions, these being to the north, south, east and
west.  Thus, to travel to a region immediately northeast would take two months
(unless powerful magic is used).  Also, to enter an ocean region, one must be
on board a ship.  (Ships can enter ocean regions, or coastal regions with an
adjacent ocean region.  They are always constructed in coastal regions.)

Movement can be prevented if the units are trying to carry too much weight.  If
a ship is trying to sail from one region to another, the capacity of the ship
is compared with the total weight on board.  If the weight is greater, the ship
cannot move.  The costs and capacities of the different types of ships are as
follows:

Type		Cost		Capacity

Longboat	100		200
Clipper		200		800
Galleon		300		1800

People weigh 10 units each.  Money has negligible weight.  All other items
weigh 1 unit each, except for stone and horses, which weigh 50 each.

For movement on land, the weight of people and horses is not taken into
account; instead, the capacity of a unit is equal to 5 per person, plus 50 per
horse; this must be no less than the weight of other items being carried, or
the unit will not be able to move.

The rule for ship movement means that any number of characters can be on board
a ship, but there is a limit to how many the ship can sail with.  The first
unit listed under a ship in the report is the "owner", this is the unit that
created the ship in the first place.  Only this unit can issue SAIL orders for
the ship.

The owner can PROMOTE another unit, which then becomes the new owner.  If the
owner leaves without promoting anyone, then the first unit of the same faction
on the ship is promoted.  If there are no other units of the same faction on
board, then the first unit on board is promoted.  If there are no other units
at all, and the ship is at sea, then it sinks.  Unoccupied ships in harbor
(i.e. in a coastal region) become owned by the first unit to board them.

Ownership of buildings is handled in the same way.  Any number of units can be
listed as inside a building, but the number of characters that can gain
protection from it in combat is limited by the building's size.


Skills
------

The most important thing distinguishing one character from another in Atlantis
is skills.  The skills available are as follows:

Mining
Lumberjack
Quarrying
Horse Training
Weaponsmith
Armorer
Building
Shipbuilding
Entertainment
Stealth
Observation
Tactics
Riding
Sword
Crossbow
Longbow
Magic

Each unit can have one or more skills.  These are gained by training (using the
STUDY command) and also by experience.  It takes 1 month of study for a unit to
attain level 1 in a skill, another 2 months to attain level 2, another 3 months
to attain level 3 and so on.  (These don't have to be consecutive; to go from
level 1 to level 2, you can study for a month, do something else for a month,
and then go back and complete your second month of study.)

Each month of studying Tactics or Magic costs $200 per person (in addition to
the normal unit maintenance fee).  Study of other skills does not cost
anything, apart from normal maintenance.

If units are merged together, their skills are averaged out.  No rounding off
is done; rather, the computer keeps track for each unit of how many total days
of training that unit has in each skill.  When units are split up, these days
are divided as evenly as possible among the people in the unit; but no days are
ever lost.

A unit with a teacher can learn twice as fast as normal.  The TEACH command is
used to spend the month teaching one or more other units (your own or another
faction's).  The unit doing the teaching must have a skill level greater than
the units doing the studying (the units doing the studying simply issue the
STUDY command as normal).  Each person can only teach up to 10 students in a
month, so a unit of 20 trying to learn from a single teacher would gain a total
of 900 days of training per month (rather than the usual 600 or the optimum
1200).

Note that it is quite possible for a single unit to teach two or more other
units different skills in the same month, provided that the teacher has a
higher skill level than each student in the skill that that student is
studying, and that there are no more than 10 students per teacher.

Mining, Lumberjack, Quarrying, Horse Training, Weaponsmith and Armorer are used
to produce commodities; this is covered in the section on the economy.

Building and Shipbuilding are used to construct buildings and ships,
respectively.

Entertainment is used to earn money by entertaining the populace.  Each month
spent entertaining will earn the performer $20 per level of skill, as well as
giving the performer the equivalent of 10 days training.  However, the money
earned from entertainment is taken from the surplus available in the region,
and no more than 1/20 of the amount available can be earned by entertainers in
any one month; if the amount would be more than this, then the maximum of 1/20
of the available money is distributed among the entertainers.

Tactics, Riding, Sword, Crossbow and Longbow are combat skills, and are covered
in the section on combat.

The Magic skill is covered in the section on magic.

Stealth is used to hide units.  A unit can only be seen if you have at least
one unit in the same region, with an Observation skill at least as high as that
unit's Stealth skill.  However, units are always visible when participating in
combat; and a unit set on guard, or in a building, or on board a ship, is also
always visible.  You can only see which faction a unit belongs to if you have a
unit with Observation skill higher than that unit's Stealth skill.


The Economy
-----------

The unit of currency in Atlantis is the silver piece, shown using the dollar
sign ($) for convenience.  The dollar sign is shown on reports only to make
them easier to read; the sign is never entered in orders, e.g. 500 silver
pieces will be shown on the report as $500, but should be entered in orders as
just 500.  The words "food", "silver" and "money" are here used
interchangeably, as one of the main uses for money is of course to buy food,
but for the sake of simplicity, the game does not bother to keep track of these
separately.

IMPORTANT: Each and every person (player character or peasant) in the Atlantis
world requires $10 every month for maintenance.  Anyone who ends the month
without this maintenance cost available will starve to death.  It is up to you
to make sure that your people have enough available.  Food will be shared
automatically between your units in the same region, if one is starving and
another has more than enough; but this will not happen between units in
different regions.

The primary commodities are silver/food, iron, wood, stone, and horses; they
are referred to as "primary commodities" because they do not require raw
materials to produce, but only labor.

The peasants in a region produce food each month; some of this is consumed
directly, but some may be available as a surplus which players can collect in
tax.

Player units can also be ordered to produce food, with the WORK command. This
requires no skill; a player character is equivalent to a peasant in this
context.

The other primary commodities do require skill to produce.  The PRODUCE command
is used to order a unit to mine/quarry/etc. one of these commodities.  A level
of at least 1 in the appropriate skill is required.  The amount produced is
proportional to the skill level, e.g. a unit with skill 3 will produce three
times as much as a unit with skill 1.  Of course, the amount produced is also
proportional to the number of people in the unit.  The skill required for each
type of primary commodity is as follows:

Commodity	Skill

Iron		Mining
Wood		Lumberjack
Stone		Quarrying
Horse		Horse Training

The amount produced also depends on the terrain type; different types of
terrain are better places for different types of economic activity.  The
productivity values (how many units of the commodity one person with skill 1
can produce in a month) are as follows:

Terrain		$	Iron	Wood	Stone	Horse

Plain		15				1
Mountain	12	1		1
Forest		12		1
Swamp		12		1

Too many cooks spoil the broth; there is a limit to how many people can
profitably work at producing the same commodity in a particular region.  These
limits are as follows:

Terrain		$	Iron	Wood	Stone	Horse

Plain		100000				200
Mountain	 20000	200		200
Forest		 20000		200
Swamp		 10000		100

For example, if 20000 people (including peasants) were growing food in a Plain
region, each would get on average 1/3 of the expected amount.  If the skill
levels involved are high, it takes even fewer people to fully exploit the
resources of a region.  (For food production, player characters always take
priority over peasants, if the production limit is exceeded.)

Each of the secondary commodities takes one unit of raw material to produce, as
well as one month of work by someone with skill 1 in the appropriate skill.
(With skill 2, two units can be produced per month, etc.)  The exception is
plate armor, which takes a month of work by someone with Armorer skill of at
least 3 to produce (skill at least 6 to produce 2, etc.).  The secondary
commodities, raw materials and skills required to produce them are as follows:

Commodity	Raw Material	Skill

Sword		Iron		Weaponsmith
Crossbow	Wood		Weaponsmith
Longbow		Wood		Weaponsmith
Chain Mail	Iron		Armorer
Plate Armor	Iron		Armorer

Anyone who spends a month producing any commodity (except food) gains the
equivalent of 10 days of study of the appropriate skill.

The construction of buildings works as follows: A new building can be created
at any time.  All buildings start with a size of 0.  Each unit of construction
work done on a building increases its size by 1, and requires one unit of
stone.  A month of work by a character with Building skill of 1 can do one unit
of construction work; a character with skill 2 can do two units, etc.

The construction of ships works similarly.  A new ship can be created at any
time.  However, until a number of units of construction work equal to the
ship's cost are done, the ship will remain "under construction" and unable to
move.  Each unit of construction work requires one unit of wood, and labor by
characters with the Shipbuilding skill.

It is possible for player units to collect taxes from the peasants.  If a unit
issues the TAX command, the amount of income collected is equal to $200 for
every armed, combat-trained man in the unit (i.e. a tax collector must have a
sword, crossbow or longbow, plus a skill of at least 1 in the use of the
weapon).  The amount of money that can be collected is of course limited by the
amount available in the region (this is shown on the report for the region).

To safeguard one's tax income from other players, the GUARD command can be
used; if a unit is set on guard, this will prevent a unit of any other faction
from collecting taxes in the region, unless the guarding faction has issued an
ADMIT command for the taxing faction.  (Of course, a sufficiently powerful
enemy can kill the guarding units, and then collect taxes.)

Each month, the peasant population in each region grows by 5% (food supply
permitting).  To be exact, each peasant has a 5% chance of producing another
peasant.  Each month also, 5% of the peasants in each region migrate to a
randomly selected neighbouring region.


Magic
-----

One starts off a career as a magician in Atlantis by studying the Magic skill.
This is studied like any other skill, with the exception that no faction may
have more than three magicians at any one time; any command that would cause
this limit to be exceeded simply fails.  The reason for this restriction is
that it would completely change the atmosphere of the game to have players
accumulating large armies of magicians.

Having acquired the appropriate skill, one can then learn spells.  Each spell
has a difficulty level.  It is possible to learn spells whose difficulty level
is no more than half your Magic skill, rounded up, e.g. at skill level 3 you
can start learning difficulty 2 spells.  (It is reasonable to assume that
magicians have other abilities in the game world, but the spells provided are
what is relevant in terms of game rules.)

The basic method of acquiring spells is to research them.  Each month spent
researching results in one previously unknown spell being acquired (the
computer selects the spell at random from those available).  When issuing a
RESEARCH command, it is necessary to specify the level to be researched (the
default is the highest possible level).  If no more spells exist at that level,
this will be noted on your report for next month.  Research costs $200 per
month.

Note, however, that while you can learn spells whose difficulty is no more than
half your Magic skill, rounded up, you can only research spells whose
difficulty is no more than half your Magic skill, rounded down.  Thus, you can
start learning difficulty 2 spells at skill level 3, if given them by someone
else; but you cannot research them yourself until skill level 4.

An easier way to acquire spells is to be given copies of them.  A magician who
knows a spell can give a copy to another magician with the appropriate skill.
This does not take a significant amount of time, so an entire library of spells
can be copied in a single month without interfering with research or other
activities.

Once spells are learned, they can be cast.  There are two basic types of spells
in this regard.  One type takes a whole month of involved ritual to cast
(spells for creating magic items, for example, fall into this category).  The
CAST command is used for this.

The other type of spell is cast in mere seconds; combat spells fall into this
category.  The way to order a magician to cast these is with the COMBAT command
which sets the combat magic spells a magician will use.  Once a magician has
been ordered to have a particular combat spell prepared, he will from then on
automatically use it whenever he is involved in fighting of any kind.  Casting
combat spells in battle does not interfere with the casting of ritual spells
that month.

Each month spent either researching spells or casting a ritual spell gains the
magician the equivalent of 10 days study of the Magic skill.

Magic items can be created by means of certain ritual spells.  These are
handled in the same way as ordinary items as regards things like the GIVE
command.  Should you ever buy, steal or be given a magic item created by
someone else, however, remember that possession of the item does not
necessarily mean you know how to use it.


Combat
------

Combat occurs when a unit issues an ATTACK command.  The computer then gathers
together all the units on the attacking side, and all the units on the
defending side, and the two sides fight until an outcome is reached.  In this
section, "attacker" means the unit that originally issued the ATTACK command,
and "defender" means the unit that was the target of the command.

On the attacking side are all the units of the attacker's faction, that are in
the region.  Also are all other factions, one of whose units issued an ATTACK
command against the defender's faction.  This means that if several factions
are attacking an enemy, each attacking faction need only have one of its units
issue an ATTACK command against one of the target faction's units, and the
attackers will automatically fight together.  You cannot attack your own units,
or an ally's units.

On the defending side are all the units of the defender's faction.  Also are
all other factions which are allied to the defender.  If the defender is the
peasants of the region, then any factions which have a unit on guard will fight
on the side of the peasants.  (If several factions are planning to attack one,
they should not only all issue ATTACK commands, but make sure they are allied
as well (using the ALLY command), in case the enemy gets in an attack first,
and picks them off piecemeal.)

The computer first checks which unit on each side has the best Tactics skill;
the unit with the best Tactics is assumed to be leading all the troops on that
side.  If one side's leader has a higher Tactics skill, then that side gets a
free round of attacks.

In each combat round, the combatants each get to attack once, in a random
order.  (In a free round of attacks, only one side's forces get to attack.)
Each combatant will attempt to hit a randomly selected enemy.  If he hits, and
the target has no armor, then the target is automatically killed.  Chain mail
gives a 1/3 chance of surviving a hit, and plate armor gives a 2/3 chance.

For a soldier using a sword against an enemy soldier similarly equipped,
whether he hits or not is decided by a contest between the two Sword skills.
Against an enemy with a bow, the contest is of Sword skill versus an effective
skill of -2.  (It is difficult for an archer to defend himself against a
swordsman in hand-to-hand combat.)

For a crossbow, whether the target is hit is decided by a contest of Crossbow
skill versus 0.  For a longbow, the contest is of Longbow skill versus 2 (a
longbow is considerably more difficult to use than a crossbow).  This
disadvantage is offset by the fact that a longbow can be fired every round,
whereas a crossbow takes two rounds to reload after being fired (so it fires on
the first, fourth, seventh etc. rounds).

Unarmed combatants are assumed to be fighting with knives, pitchforks or other
improvised weapons; they are treated as if they were using swords, but with an
effective skill of -2.  (A combatant with a weapon, but without a skill of at
least 1 in its use, is treated as unarmed.  Combatants with more than one
weapon will select the first one with which they have a skill of at least 1.)

A swordsman with a horse, and with Riding skill of at least 2, gains a bonus of
2 to his effective Sword skill, provided that the region terrain is Plain;
horses are of little use in battle in any other terrain.

People inside buildings gain protection; there is a penalty of 2 for anyone to
hit them.  (This is true even if they are on the attacking side.)  The maximum
number of people who can gain protection from a single building is equal to the
building's size.

The contest of skills works as follows: The attacker's and defender's skills
are compared (here "attacker" and "defender" mean the individual soldiers,
without reference to who issued the original ATTACK command).  The chance of
hitting is determined by which skill is higher, and by how much:

Skill Diff.	Chance

-1		10
 0		25
 1		40

Example: if a soldier with a Sword skill of 3 is trying to hit one with a Sword
skill of 2, there is a 40% chance of success.  If the attacker's skill is more
than 1 greater than the defender's, there is a 49% chance of success; if the
defender's skill is more than 1 greater, there is only a 1% chance.

Particularly valuable or lightly-armed units can be protected using the BEHIND
command.  Any unit with the BEHIND flag set is considered to be at the rear of
the battle, and cannot be attacked by any means until all the troops in front
on that side have been killed.  However, units at the back cannot use swords,
only missile weapons.

A magician using a combat spell is considered to be using a missile weapon, and
can attack even while BEHIND.  A magician without a combat spell set will fight
with normal weapons like everyone else.  The battle report will indicate which
magicians cast what combat spells, if any.

After the free round of attacks (if any), comes the main part of the battle.
Combat rounds are fought until one side is wiped out.  (Realistically, it is
reasonable to assume that some of the troops on the losing side have fled and
dispersed into the countryside.  Either way, they are no longer with their
faction.)

Finally, the dead are counted.  Any money or items owned by dead combatants on
either side are collected by the winning side; the loot is distributed evenly
among the troops, so factions on the winning side will generally pick up loot
in proportion to their number of surviving men.  However, any items other than
iron, wood or stone, whose owners are killed, have a 50% chance of being
destroyed in the fighting, e.g. if 50 swordsmen are killed, the winners will
pick up about 25 swords.

Every armed soldier on the winning side gains the equivalent of 10 days study
with the weapon he was using.  Magicians who were using a combat spell gain the
equivalent of 10 days study of the Magic skill.  In addition, the leader of the
winning side gains the equivalent of 10 days study of Tactics.  However, these
skill gains only happen if the winning side took at least one casualty (so that
the victory was at least something of a challenge).

Combat between units on board ships at sea is handled exactly like land combat,
except that horses can't be used in naval battles.

If you are expecting to fight an enemy who is carrying so much equipment that
you would not be able to move after picking it up, and you want to move to
another region later that month, it may be worth issuing some orders to drop
items (with the GIVE 0 command) in case you win the battle!


Order Format
------------

To enter orders for Atlantis, you should send a mail message (no particular
subject line required), containing the following:

#ATLANTIS faction-no

UNIT unit-no
...orders...

UNIT unit-no
...orders...

#END

For example, if your faction number (shown at the top of your report) is 27,
and you have two units numbered 5 and 17:

#ATLANTIS 27

UNIT 5
...orders...

UNIT 17
...orders...

#END

Thus, orders for each unit are given separately, and indicated with the UNIT
keyword.  (In the case of orders, such as the command to rename your faction,
that are not really for any particular unit, it does not matter which unit
issues the command; but some particular unit must still issue it.)

Several sets of orders may be sent for the same turn, and the last one received
before the deadline will be used.

IMPORTANT: You MUST use the correct #ATLANTIS line or else your orders will be
silently ignored.

Each type of order is designated by giving a keyword as the first non-blank
item on a line.  Parameters are given after this, separated by spaces or tabs.
Blank lines are permitted, as are comments; anything after a semicolon on a
line is treated as a comment and ignored.

The parser is not case sensitive, so all commands may be given in upper case,
low case or a mixture of the two.  However, when supplying names containing
spaces, the name must be surrounded by double quotes, or else underscore
characters must be used in place of spaces in the name.  (These things apply to
the #ATLANTIS and #END lines as well as to order lines.)

Any player not sending in orders for four turns in a row is removed from the
game, e.g. if your last set of orders was for turn 20, you will receive reports
for turns 20, 21, 22, 23 and 24; your turn 24 report will contain a request to
send orders; and you will not receive a turn 25 report.  Note that an empty
order set still counts as sending in orders.

IMPORTANT: If you are dropped from the game for not sending orders, your
faction is completely and permanently destroyed, and is NOT retrievable.
Please contact the moderator if you have difficulty getting orders through.


Orders
------


ACCEPT  faction-no

If any of your units issues this command, then all of your units will accept
gifts from any units of the target faction for that month.  The ACCEPT status
only lasts for a single month.  ACCEPT status is always assumed for allies.

It is necessary to issue an ACCEPT command to receive things given with any of
the following commands: GIVE, PROMOTE, TEACH, TRANSFER.


ADDRESS  new-address

Change the email address to which your reports are sent (up to 80 characters).


ADMIT  faction-no

If any of your units issues this command, then any unit of the target faction
will be admitted into any building owned by you, or aboard any ship owned by
you, and allowed to collect taxes in any region you are guarding, for the
duration of the month.  The ADMIT status only lasts for a single month.  ADMIT
status is always assumed for allies.


ALLY  faction-no  flag

If you ally with a faction, then your units will fight to defend units of that
faction when they are attacked.  ALLY faction-no 1 allies with the indicated
faction.  ALLY faction-no 0 cancels this.

The ALLY status also implies ACCEPT and ADMIT status, i.e. you will always
ACCEPT and ADMIT any faction you are allied to.

Note that even if you are allied to a faction, this does not necessarily mean
that this faction is allied to you.


ATTACK  unit-no
ATTACK  PEASANTS

Attack either a target unit, or the peasants in the current region.


BEHIND  flag

BEHIND 1 sets the unit issuing the command to be behind other units in combat.
BEHIND 0 cancels this.


BOARD  ship-no

The unit issuing the command will attempt to board the specified ship.  If
issued from inside a building or aboard a ship, the unit will first
leave the current building or ship.  The command will only work if the target
ship is unoccupied, or is owned by a unit in your faction, or is owned by a
unit in a faction which has issued an ADMIT command for you that month.


BUILD  BUILDING  [building-no]
BUILD  SHIP  ship-no
BUILD  ship-type

Perform building work on a building or ship.  BUILD BUILDING with a building
number specified will perform work on that building.  BUILD BUILDING alone will
create a new building and perform work on it.  BUILD SHIP with a ship number
specified will perform work on that ship.  BUILD LONGBOAT, CLIPPER or GALLEON
will create a ship of the indicated type and perform work on it.

Note that since the number of a newly created building or ship is not known
until next month, only the creating unit can work on it in the first month.


CAST  spell

Spend the month casting the indicated spell (must be a ritual magic spell which
the unit knows).


COMBAT  spell

Set the unit's combat spell to the indicated value (must be a combat spell
which the unit knows).  If no spell is specified, the unit's combat spell will
be set to nothing (i.e. the magician will henceforth fight in hand-to-hand
combat).


DEMOLISH

Demolish the building of which the unit is currently the owner.


DISPLAY  UNIT  text
DISPLAY  BUILDING  text
DISPLAY  SHIP  text

Set the display string for the unit itself, or a building or ship (of which the
unit must currently be the owner).  This is a text string (up to 160
characters) which will be shown after the entity's description on everyone's
report.


ENTER  building-no

The unit issuing the command will attempt to enter the specified building.  If
issued from inside a building or aboard a ship, the unit will first leave the
building or ship.  The command will only work if the target building is
unoccupied, or is owned by a unit in your faction, or is owned by a unit in a
faction which has issued an ADMIT command for you that month.


ENTERTAIN

The unit issuing the command will spent the month entertaining the locals to
earn money.


FIND  faction-no

Find the email address of the specified faction.


FORM  alias

Forms a new unit.  The newly created unit will be in your faction, in the same
region as the unit which formed it, with the same BEHIND and GUARD status, and
in the same building or ship, if any.  It will start off, however, with no
people or items; you should, in the same month, issue orders to transfer people
into the new unit, or have it recruit new members.

The FORM command is followed by a list of commands for the newly created unit. 
This list is terminated by the END keyword, after which commands for the unit
issuing the FORM command start again.

The purpose of the "alias" parameter is so that you can refer to the new unit. 
You will not know the new unit's number until next month.  To refer to the new
unit that month, pick an alias number (the only restriction on this is that it
must be at least 1, and you cannot create two new units in the same region in
the same month, with the same alias numbers).  The new unit can then be
referred to as NEW alias in place of the regular unit number.

Example:

UNIT 17
FORM 1
    NAME UNIT "Merlin's Guards"
    RECRUIT 20
    STUDY SWORD
END
FORM 2
    NAME UNIT "Merlin's Workers"
    DISPLAY UNIT "wearing dirty overalls and carrying shovels"
    RECRUIT 50
END
PAY NEW 1 1200
PAY NEW 2 3000

This set of orders for unit 17 would create two new units with alias numbers 1
and 2, name them Merlin's Guards and Merlin's Workers, set the display string
for Merlin's Workers, have both units recruit men, and have Merlin's Guards
study swordsmanship.  Merlin's Workers will have the default command WORK, as
all newly created units do.  The unit that created these two then pays them
enough money (using the NEW keyword to refer to them by alias numbers) to cover
the costs of recruitment and the month's maintenance.


GIVE  unit-no  quantity  item
GIVE  0  quantity  item
GIVE  unit-no  spell

The first form of the command gives a quantity of an item to another unit.  If
the target unit is not a member of your faction, then its faction must have
issued an ACCEPT command for you that month.

The second form of the command discards a quantity of an item.  The items
discarded are permanently lost.

The third form of the command gives a copy of a spell to another unit.  The
other unit must have sufficient Magic skill to understand the spell, and its
faction must have issued an ACCEPT command for you that month.


GUARD  flag

GUARD 1 sets the unit issuing the command to guard the peasants of the region
against attack, or taxation by other factions (except those for which one has
issued an ADMIT command).  GUARD 0 cancels this.


LEAVE

Leave whatever building the unit is in, or ship it is on board.  The command
cannot be used at sea.


MOVE  direction

The unit will move in the specified direction (NORTH, SOUTH, EAST or WEST). 
This command is for use on land only; use SAIL to move in a ship.


NAME  FACTION  new-name
NAME  UNIT  new-name
NAME  BUILDING  new-name
NAME  SHIP  new-name

Changes the name of your faction, or of the unit, or of a building or ship (of
which the unit must currently be the owner).  The name can be up to 80
characters.  All entities start off named "Unit 5", "Faction 23" or whatever
until they are given proper names.  Names may not contain brackets (square
brackets can be used instead if necessary).


PAY  unit-no  amount
PAY  PEASANTS  amount
PAY  0  amount

Pay another unit the specified amount of money (does not require the target
unit's faction to have issued an ACCEPT command).  PAY PEASANTS will give the
money to the peasants of the region, and PAY 0 will simply throw it away.


PRODUCE  item

Spend the month producing as much as possible of the indicated item.


PROMOTE  unit

Promote the specified unit to owner of the building or ship of which the unit
issuing the command is currently the owner.  If the specified unit is not a
member of your faction, then its faction must have issued an ACCEPT command for
you that month.


QUIT  faction-no

Quit the game.  Your faction number must be provided as a check to prevent Quit
orders being issued by accident.  On issuing this order, your faction will be
completely and permanently destroyed.


RECRUIT  number

Attempt to recruit the specified number of people into the unit.  Each person
recruited costs $50 (this is either paid as compensation to their previous
employers, or paid to the new recruits as a signing-on fee, who then spend it
in the local tavern celebrating their new employment status).

The total number of people who can be recruited in a region in one month is 1/4
of the number of peasants in the region.  If more RECRUIT orders than this are
issued, the available recruits are distributed at random among the units
issuing the orders.

Since new recruits have no skills, a trained unit which takes in recruits will
find its skills diluted.


RESEARCH  [level]

Spend the month researching a spell at the specified difficulty level.  For
example, a magician with a magic skill of 4 or 5 can research spells of up to
level 2.  If the level is not specified, the research will be at the highest
possible level.


RESHOW  spell

When a unit in your faction researches, or is given a copy of, a new spell for
the first time, your report will contain a paragraph of information on the
spell.  The RESHOW command will show this information again, in case you lose
the report that originally showed it.  This only works if there is still at
least one unit in your faction that knows the spell.


SAIL  direction

The ship of which the unit is the owner will sail in the specified direction
(NORTH, SOUTH, EAST or WEST).


SINK

Sink the ship of which the unit is currently the owner.  This can only be done
in a coastal region, as sinking one's ship at sea would be suicide.


STUDY  skill

Spend the month studying the specified skill.


TAX

Attempt to raise tax income from the region.


TEACH  unit-no  ...

Attempt to teach the specified units whatever skill they are studying that
month.  A list of several unit numbers may be specified.  If any of these units
is not a member of your faction, then its faction must have issued an ACCEPT
command for you that month.


TRANSFER  unit-no  number
TRANSFER  PEASANTS  number

Transfer the specified number of people to another unit.  If the specified
unit is not a member of your faction, then its faction must have issued an
ACCEPT command for you that month.  The TRANSFER PEASANTS order will transfer
the people into the peasant population (will work anywhere except in an ocean
region).


WORK

Spend the month working in agriculture.


Sequence of Events
------------------

Each turn, the following sequence of events occurs:

FORM orders are processed.
ACCEPT, ADDRESS, ADMIT, ALLY, BEHIND, COMBAT, DISPLAY, GUARD 0, NAME and RESHOW
orders are processed.
FIND orders are processed.
BOARD, ENTER, LEAVE and PROMOTE orders are processed.
ATTACK orders are processed.
DEMOLISH, GIVE, PAY and SINK orders are processed.
TRANSFER orders are processed.
TAX orders are processed.
GUARD 1 and RECRUIT orders are processed.
QUIT orders are processed.
Empty units are deleted.
Unoccupied ships at sea are sunk.
MOVE orders are processed.
SAIL orders are processed.
BUILD, ENTERTAIN, PRODUCE, RESEARCH, STUDY, TEACH and WORK orders are
processed.
CAST orders are processed.
Peasant population growth occurs.
Maintenance costs are assessed.
Peasant migration occurs.


Hints for New Players
---------------------

Make sure to use the correct #ATLANTIS and UNIT lines in your orders.

Be careful not to lose any people through starvation.  It is a good idea to
store a month's supply of money in each region in which you have units, as a
safety measure.

Don't try to do everything at the start; pick a particular occupation to
specialize in, and trade with other players for anything else you need.  You
can always diversify later, when you have the resources.

Keep an eye on what the other players are doing, so as not to miss
opportunities for profitable trade, or get caught in a surprise attack.
Remember, in time of war there is safety in numbers.

Remember to use ACCEPT and ADMIT where necessary.

