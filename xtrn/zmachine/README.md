# JSZM
JSZM is a public-domain [Z-machine](https://en.wikipedia.org/wiki/Z-machine) interpreter, originally written in JavaScript/ES6 by zzo38.

This repository is the **Synchronet** build: an ES5 / SpiderMonkey-1.8.5 port of the engine plus a terminal-server **door** (`zmachine.js`) that plays Z-machine **v3/4/5/8** interactive-fiction story files — with game categories, IFDB-resolved titles, in-game save/restore (Quetzal), automatic resume on disconnect, color/Unicode output, and timed (real-time) input.

**Sysops:** see [SYSOP.md](SYSOP.md) for installation and configuration.

## Files
- `jszm.js` — the Z-machine interpreter engine
- `quetzal.js` — Quetzal save-file codec
- `zmachine.js` — the Synchronet door front-end
- `games.ini` — curated game catalog (for the provisioner)
- `getgames.js` — story-file provisioner script (fetches and installs games)
- `test/` — the test suite

## Authorship
The initial commit to master is a snapshot of [JSZM v2.0.2](http://zzo38computer.org/jszm/jszm.js) by zzo38, taken with the author's permission. The ES5 port and Synchronet door are by Rob Swindell (Digital Man).
