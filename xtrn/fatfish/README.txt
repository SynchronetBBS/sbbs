------------------------------------------------
 FatFish: the fishing simulation door game.
------------------------------------------------
FatFish is a fishing simulation door game for Synchronet BBS systems.

Website: http://art.poorcoding.com/fatfish.

--------
Features
--------
- Unique lake every game. Random terrain generation thanks to mcmlxxix's library creates unique fishing holes every time.

- Dynamic screen sizes. 80x24, 80x25, 10x100, 100x400, any terminal size is supported. Larger resolutions create larger lakes, with more fish.

- "Real" fish AI: your chances of catching fish are not based on random luck. Fish are real objects, that swim around, move into their preferred depths, feed and decide to do things on their own accord. The fish act independent of what the player is doing (or not doing).

- Depth: fish consider depths, go into preferred areas in lakes, and so on. Position yourself strategically.

- Rods: various types of fishing rods are available, depending on what you want to catch.

- InterBBS: scores can be submitted to the central server. Gloat about "the big one" across boards!

- Way more one-trick ponies! Give it a whirl, and discover the forgotten glory that is textual fishing.

------------
Instructions
------------
Use '?' during the game to view context-sensitive help.

Move around the lake, to find a spot to cast. Take note of the depth display. You can use fish finder points, and purchase more at the shop.

Cast, and wait for the float the bob.

Try to snag the fish:
- Press SPACEBAR when you see the float bobbing.

When reeling the fish in:
- The line distance and the line tension will be shown.
- Toggle pressing the UP/DOWN arrows to slacken/reel in the line.
- If the distance is too much, the fish will escape.
- If the tension is too high, your line will snap.

Fish behavior:
- Fish swim into areas of preferred depth:
	Eel: very shallow.
	Trout: shallow.
	Bass: mid-range.
	Pike: deep.	
- Fish may be attracted to your lure when casting, if nearby. Some fish have better vision than others. They will swim to your location.
- The larger the fish, the harder it will fight when reeling them in.
- Small fish do not bite when using heavy rods, and large fish do not bite with light rods.

Sell fish you have caught at the shop, to purchase goods. Your credits and equipment are saved and persist across games.

Shop:
- Sell all unsold fish caught in the current game. Note the varying prices per species.
- The various rods in the shop make it easier/more difficult to catch varying fish.
- Choose your rod based on the size of fish you want to catch.

InterBBS scores:
- The game will connect to fatcatsbbs.com:10088 in order to fetch the scores.
- Your firewall(s), if any, must allow this.

-------
Credits
-------
         AUTHOR: art, at fatcatsbbs dot com.
        LICENSE: Free for personal use.
         THANKS: mcmlxxix, for the excellent libraries.
        SUPPORT: art on #synchronet, irc.bbs-scene.org.
   CONTRIBUTORS:        
         mcmlxxix: frame.js-related modifications.

That is all, and thanks for all the fish. :|

