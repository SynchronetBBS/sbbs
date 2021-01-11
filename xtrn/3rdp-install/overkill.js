"use strict";

var gamedir = fullpath(js.startup_dir);

if (file_exists(js.startup_dir + 'ooiidoor.bat')) {
	file_rename(js.startup_dir + 'ooiidoor.bat', js.startup_dir + 'ooiidoor.bat.old');
}

var file = new File(js.startup_dir + 'ooiidoor.bat');
if (!file.open("w")) {
	writeln("Error " + file.error + " writing to ooiidoor.bat");
	exit(1)
}
file.writeln("OOINFO 2 " + gamedir);
file.writeln("MAINTOO");
file.writeln("OOII");
file.close();

if (file_exists(js.startup_dir + 'ooconfig.dat')) {
	file_rename(js.startup_dir + 'ooconfig.dat', js.startup_dir + 'ooconfig.dat.old');
}

var file = new File(js.startup_dir + 'ooconfig.dat');
if (!file.open('w')) {
	writeln("Error " + file.error + " writing to ooconfig.dat");
	exit(1)
}
file.writeln("OVERKILL              | Name of the BBS System |");
file.writeln("75777899             | OOII Registration Code |");
file.writeln("NONE             | Obsolete Reg Code |");
file.writeln("GAP               | BBS System |");
file.writeln(gamedir + "            | Path to BBS Data Files |");
file.writeln(system.text_dir + "       | Path to TOP10/FRONTIER Ansi Bulletins |");
file.writeln(system.text_dir + "       | Path to TOP10/FRONTIER Ascii Bulletins |");
file.writeln("NONE             | Pause String for Bulletins |");
file.writeln("YES               | Multi-Node? |");
file.writeln("DIRECT           | Video mode BIOS/DIRECT |");
file.writeln("FOSSIL           | Comm Support FOSSIL/NORMAL |");
file.writeln("42               | Fight Delay |");
file.writeln("ZZEXLIA          | Complex Name |");
file.writeln("VIDLAND.MAP      | Map Files |");
file.writeln("60               | Minutes per Play |");
file.writeln("3                | Game Plays per Day |");
file.writeln("240              | Minutes Between Plays |");
file.writeln("16:00            | Starting Chat Hour |");
file.writeln("22:00            | Ending Chat Hour |");
file.writeln("4                | Player Level for Hydrite Kidnapping |");
file.writeln("21               | Days Until Inactive Player Deleted |");
file.writeln("30               | Oracle Threshold Point |");
file.writeln("150              | Frontier Log Length |");
file.writeln("10               | Bases per Player |");
file.writeln("10               | Game Plays in Gaming Room |");
file.writeln("90               | Hunger Moves |");
file.writeln("60               | Total Stats New Players |");
file.writeln("4000             | Crystals for New Players |");
file.writeln("NIL              | LR Weapon for New Players |");
file.writeln("NIL              | HTH Weapon for New Players |");
file.writeln("NIL              | Armor for New Players |");
file.writeln(gamedir + "           | Path to Game's Data Files |");
file.writeln(gamedir + "           | Path to Game's Text Files |");
file.writeln("4                | Minimum Average Player Level |");
file.writeln("120              | Total Minutes per Day |");
file.writeln(gamedir + "           | Path to Game's NEWS.ASC Text File |");
file.writeln(gamedir + "           | Path to Game's BANNED.DAT Data File |");
file.close();
