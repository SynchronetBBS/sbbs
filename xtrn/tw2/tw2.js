var startup_path='.';
try { throw new Error() } catch(e) { startup_path=e.fileName }
startup_path=startup_path.replace(/[\/\\][^\/\\]*$/,'');
startup_path=backslash(startup_path);

load("recordfile.js");
load("lockfile.js");
load(startup_path+"filename.js");
load(fname("gamesettings.js"));
var Settings=new GameSettings();

load(fname("structs.js"));

var on_at=time();
var i;

var Commodities=[
	{
		 name:"Ore"
		,abbr:"Ore"
		,disp:"Ore......."
		,price:10
	}
	,{
		 name:"Organics"
		,abbr:"Org"
		,disp:"Organics.."
		,price:20
	}
	,{
		 name:"Equipment"
		,abbr:"Equ"
		,disp:"Equipment."
		,price:35
	}
];

var sectors=new RecordFile(fname("sectors.dat"), SectorProperties);
var ports=new RecordFile(fname("ports.dat"), PortProperties);
var players=new RecordFile(fname("players.dat"), PlayerProperties);
var playerLocation=new RecordFile(fname("player-loc.dat"), PlayerLocation);
var teams=new RecordFile(fname("teams.dat"), TeamProperties);
var planets=new RecordFile(fname("planets.dat"), PlanetProperties);
var twmsg=new File(fname("twmesg.dat"));
var twopeng=new File(fname("twopeng.dat"));
twopeng.open("a", true);
twmsg.open("a", true);
var twpmsg=new RecordFile(fname("twpmesg.dat"), PlayerMessageProperties);
var twrmsg=new RecordFile(fname("twrmesg.dat"), RadioMessageProperties);
var cabals=new RecordFile(fname("cabal.dat"), CabalProperties);

var today=system.datestr(system.datestr());

js.on_exit("do_exit()");
/* Run maintenance */
if(Settings.MaintLastRan != system.datestr()) {
	Settings.MaintLastRan = system.datestr();
	Settings.save();
	console.attributes="Y";
	console.writeln("Running maintence program...");
	console.crlf();
	console.crlf();
	console.writeln("Trade Wars maintence program");
	console.writeln("  by Chris Sherrick (PTL)");
	console.crlf();
	console.writeln("This program is run once per day.");
	twmsg.writeln(system.timestr()+": "+user.alias+": Maintence program ran");
	console.writeln("Deleting inactive players");

	var oldest_allowed=today-Settings.DaysInactivity*86400;
	for(i=1; i<players.length; i++) {
		var p=players.Get(i);
		if(p.UserNumber > 0) {
			if((!file_exists(system.data_dir+format("user/%04d.tw2",p.UserNumber))) || (system.datestr(p.LastOnDay) < oldest_allowed && p.KilledBy != 0)) {
				DeletePlayer(p)
			}
		}
	}
	console.crlf();
	MoveCabal();

	RankPlayers();
}

console.attributes="C";
console.crlf();
console.crlf();
console.center("Trade Wars (v.ii)");
console.center("By Chris Sherrick (PTL)");
console.center("Copyright 1986 Chris Sherrick");
console.crlf();
console.center(system.name);
console.center("Sysop  "+system.operator);
console.crlf();
console.crlf();
console.printfile(fname("twopeng.asc"));
if(file_exists(fname("twopeng.dat")))
	console.printfile(fname("twopeng.dat"));
console.crlf();
console.attributes="W";
console.writeln("Initializing...");
console.writeln("Searching my records for your name.");
var player;
var firstnew;
for(i=1; i<players.length; i++) {
	player=players.Get(i);
	if(player.UserNumber == user.number && (!file_exists(system.data_dir+format("user/%04d.tw2",player.UserNumber))))
		DeletePlayer(player);
	if(player.UserNumber==0 && firstnew==undefined)
		firstnew=player;
	if(player.UserNumber == user.number)
		break;
}
if(player==undefined || player.UserNumber!=user.number) {
	console.attributes="G";
	console.writeln("I can't find your record, so I am assuming you are a new player.");
	console.attributes="M";
	console.writeln("Entering a new player...");
	player=firstnew;
	if(player==undefined) {
		console.writeln("I'm sorry but the game is full.");
		console.writeln("Please leave a message for the Sysop so");
		console.writeln("he can save a space for you when one opens up.");
		twmsg.writeln(system.timestr()+": New player not allowed - game full.");
		exit(0);
	}
	console.crlf();
	console.writeln("Notice: If you don't play this game for "+Settings.DaysInactivity+" days, you will");
	console.writeln("be deleted to make room for someone else.");
	console.crlf();
	console.writeln("Your ship is being initialized.");

	/* Lock the player file and mark us online... */
	if(!Lock(players.file.name, bbs.node_num, true, 10)) {
		console.writeln("!!!Unable to lock player data!");
		twmsg.writeln("!!!Unable to lock player data!");
		exit(1);
	}
	player.ReInit();
	player.Online=true;
	player.Put();
	Unlock(players.file.name);

	twmsg.writeln(system.timestr()+" "+user.alias+": New Player logged on");
	Instructions();
}
else {
	/* Lock the player file and mark us online... */
	if(!Lock(players.file.name, bbs.node_num, true, 10)) {
		console.writeln("!!!Unable to lock player data!");
		twmsg.writeln("!!!Unable to lock player data!");
		exit(1);
	}
	player.Online=true;
	player.Put();
	Unlock(players.file.name);

	console.crlf();		/* TODO: BASIC magic... N$ appears empty tw2.bas: 242 */
	twmsg.writeln(system.timestr()+" "+user.alias+": Logged on");
	if(today < system.datestr(player.LastOnDay)) {
		console.writeln("I'm sorry, but you won't be allowed on for another " + parseInt((system.datestr(player.LastOnDay)-today)/86400) + " day(s)");
		exit(0);
	}
	if(today==system.datestr(player.LastOnDay)) {
		if(player.KilledBy != -99) {
			console.writeln("You have been on today.");
			if(player.TurnsLeft<1) {
				console.writeln("You don't have any turns left today. You will be allowed to play tomorrow.");
				exit(0);
			}
			if(player.KilledBy==player.Record) {
				console.writeln("You killed yourself today. You will be allowed to play tomorrow.");
				exit(0);
			}
			console.writeln("The following happened to your ship since your last time on:");
			var count=0;
			for(i=0; i<twpmsg.length; i++) {
				var msg=twpmsg.Get(i);

				if(msg.To==player.Record && !msg.Read) {
					count++;
					if(msg.From==-98)
						console.writeln("A deleted player destroyed "+msg.Destroyed+" fighters.");
					else if(msg.From==-1) {
						console.attributes="R";
						console.writeln("The Cabal destroyed "+msg.Destroyed+" fighters.");
					}
					else {
						var otherplayer=players.Get(msg.From);

						console.writeln(otherplayer.Alias+" "+(otherplayer.TeamNumber?" Team["+otherplayer.TeamNumber+"] ":"")+"destroyed "+msg.Destroyed+" of your fighters.");
					}
					msg.Read=true;
					msg.Put();
				}
			}
			if(count==0)
				console.writeln("Nothing");
		}
	}
	else {
		player.TurnsLeft=Settings.TurnsPerDay;
	}
	player.LastOnDay=system.datestr();
	if(player.KilledBy != 0) {
		switch(player.KilledBy) {
			case player.Record:
				console.attributes="R";
				console.writeln("You managed to kill yourself on your last time on.");
				break;
			case -1:
				console.attributes="R";
				console.writeln("You have been killed by the Cabal!");
				break;
			case -98:
				console.writeln("You have been killed by a deleted player.");
				break;
			default:
				var otherplayer=players.Get(player.KilledBy);

				console.writeln(otherplayer.Alias+" destroyed your ship!");
				break;
		}
		player.ReInit();
	}
}
file_touch(system.data_dir+format("user/%04d.tw2",player.UserNumber))
ReadRadio();
if(player.Sector < 1 || player.Sector >= sectors.length) {
	console.writeln("You are being moved to sector 1");
	player.Sector=1;
}
location=playerLocation.Get(player.Record);
location.Sector=player.Sector;
location.Put();

if(player.Credits > 25000) {
	console.crlf();
	console.writeln("Tax time! You are being taxed 5000 credits to help support the resistance");
	console.writeln("against the evil Cabal.");
	player.Credits -= 5000;
}

var exit_tw2=false;

while(player.KilledBy==0 && exit_tw2==false) {
	if(EnterSector()) {
		if(CheckSector())
			Menu();
	}
}

function Menu()
{
	/* 22000 */
	while(1) {
		console.crlf();
		console.attributes="HC";
		console.write("Command (?=Help)? ");
		var valid=new Array('A','C','D',/*'E' TODO: Editor ,*/'F','G','I','L','M','P','Q','T','Z','?');
		var sector=sectors.Get(player.Sector);
		var i;
		for(i=0; i<sector.Warps.length; i++) {
			if(sector.Warps[i]>0)
				valid.push(sector.Warps[i].toString());
		}
		var inp=InputFunc(valid)
		switch(inp) {
			case '':
				console.writeln("? = Help");
				break;
			case 'A':
				/* 25000 */
				console.writeln("<Attack>");
				var count=0;
				var i;

				if(player.Fighters < 1) {
					console.writeln("You don't have any fighters.");
					break;
				}
				for(i=1;i<players.length; i++) {
					var otherloc=playerLocation.Get(i);
					if(otherloc.Sector==Player.Sector) {
						var otherplayer=players.Get(i);
						if(otherplayer.Sector==player.Sector
								&& otherplayer.Record!=player.Record
								&& otherplayer.KilledBy!=0
								&& otherplayer.UserNumber!=0
								&& !otherplayer.Online) {
							count++;
							console.write("Attack "+otherplayer.Alias+" (Y/N)[Y]? ");
							if(GetKeyEcho()!='N') {
								console.writeln("<Yes>");
								break;
							}
						}
					}
					otherplayer=null;
				}
				if(otherplayer!=null) {
					DoBattle(otherplayer, otherplayer.TeamNumber > 0);
					if(otherplayer.Fighters==0) {
						KilledBy(otherplayer, player, true);
						/* 15600 player salvages ship from otherplayer */
						var salvaged=parseInt(otherplayer.Holds/4)+1;
						if(player.Holds + salvaged > 50)
							salvaged=50-player.Holds;
						if(salvaged < 1)
							console.writeln("You destroyed the ship, but you can't salvage anything from it");
						else {
							var j;

							console.writeln("You destroyed the ship and salvaged these cargo holds:");
							var holds=new Array(Commodities.length+1);
							for(i=0; i<holds.length+1; i++)
								holds[i]=0;
							for(i=0; i<otherplayer.Holds; i++) {
								var limit=0;
								var r=random(otherplayer.Holds)+1;
								for(j=0; j<Commodities.length; j++) {
									limit += otherplayer.Commodities[j];
									if(r<limit) {
										otherplayer.Commodities[j]--;
										holds[j]++;
										r=0;
										break;
									}
								}
								if(r==0)
									holds[Commodities.length]++;
							}
							if(holds[Commodities.length]>0) {
								console.writeln("  "+holds[Commodities.length]+" empty.");
								player.Holds+=holds[Commodities.length];
							}
							for(j=0; j<Commodities.length; j++) {
								if(holds[j]>0) {
									console.writeln("  "+holds[j]+" with "+Commodities[j].name.toLowerCase()+".");
									player.Holds+=holds[j];
									player.Commodities[j]+=holds[j];
								}
							}
						}						
					}
				}
				else {
					if(count)
						console.writeln("There are no other ships in here");
					else
						console.writeln("There are no ships here to attack.");
				}
				break;
			case 'C':
				/* 33640 */
				console.writeln("<Computer>");
				console.crlf();
				console.attributes="HW";
				console.writeln("<Computer activated>");
				var showhelp=true;
				var exitcomputer=false;
				while(!exitcomputer) {
					if(showhelp) {
						console.crlf();
						console.printfile(fname("computer.asc"));
					}
					console.attributes="HW";
					console.write("Computer command (?=help)? ");
					switch(InputFunc(['?','1','2','3','4','5'])) {
						case '?':
							showhelp=true;
							break;
						case '1':
							/* 33990 */
							console.crlf();
							console.attributes="Y";
							console.writeln("<Computer deactivated>");
							exitcomputer=true;
							break;
						case '2':
							/* 33780 */
							console.write("What sector number is the port in? ");
							var sec=console.getnum(sectors.length-1);
							if(sec > 0 && sec < sectors.length) {
								var sector=sectors.Get(sec);
								if(sector.Port==0 || (sector.Fighters>0 && sector.Fighters!=player.Record)) {
									console.crlf();
									console.writeln("I have no information about that port.");
									break;
								}
								if(sec==1) {
									SolReport();
								}
								else {
									PortReport(sector);
								}
							}
							break;
						case '3':
							/* 33830 */
							console.write("What sector do you want to get to? ");
							var sec=console.getnum(sectors.length-1);
							if(sec > 0 && sec < sectors.length) {
								if(sec==player.Sector) {
									console.writeln("You are in that sector.");
									break;
								}
								console.crlf();
								console.writeln("Computing shortest path.");
								var path=ShortestPath(player.Sector, sec);
								if(path==null) {
									console.writeln("There was an error in computation between sectors.");
								}
								else {
									console.writeln("The shortest path from sector "+player.Sector+" to sector "+sec+" is:");
									var i;
									for(i in path) {
										if(i!=0) 
											console.write(" - ");
										console.write(path[i]);
									}
								}
							}
							break;
						case '4':
							/* 33900 */
							console.crlf();
							console.write("Ranking Players.");
							var ranked=RankPlayers();
							console.crlf();

							console.attributes="W";
							console.writeln("Player Rankings: "+system.timestr());
							console.writeln("Rank     Value      Team  Player");
							console.writeln("==== ============= ====== ======")
							for(rank in ranked) {
								var p=players.Get(ranked[rank].Record);
								console.writeln(format("%4d %13d %-6s %s"
										,rank+1
										,ranked[rank].Score
										,p.TeamNumber==0?"":format("  %-2d  "
										,p.TeamNumber),p.Alias));
							}
							break;
						case '5':
							/* 33960 */
							console.crlf();
							console.writeln("Warming up sub-space radio.");
							mswait(500);
							console.crlf();
							console.write("Who do you wish to send this message to (search string)? ");
							var sendto=console.getstr(42);
							var p=MatchPlayer(sendto);
							if(p==null)
								break;
							console.writeln("Tuning in to " + p.Alias + "'s frequency.");
							console.crlf();
							console.writeln("Due to the distances involved, messages are limited to 74 chars.");
							console.writeln("Pressing [ENTER] with no input quits");
							console.writeln("  [------------------------------------------------------------------------]");
							console.attributes="HY";
							console.write("? ");
							console.attributes="W";
							var msg=console.getstr(74);
							if(msg==null)
								break;
							msg=msg.replace(/\s*$/,'');
							if(msg=='')
								break;
							RadioMessage(player.Record, p.Record, msg);
							break;
						default:
							console.crlf();
							console.writeln("Invalid command.  Enter ? for help");
					}
				}
				break;
			case 'D':
				console.writeln("<Display>");
				DisplaySector(player.Sector);
				break;
//			case 'E':
//				/* TODO: 22800 */
//				if(user.level < 90)
//					break;
//				console.writeln("<TW Editor>");
//				console.write("Do you wish to use the editor? Y/N [N] ");
//				switch(GetKeyEcho()) {
//					case 'Y':
//						console.writeln("Running Tradewars ][ Editor...");
//						/* TODO: TWEdit */
//				}
//				break;
			case 'F':
				/* 24000 */
				console.writeln("<Drop/Take Fighters>");
				if(player.Sector < 8) {
					console.writeln("You can't leave fighters in the Union (sectors 1-7)");
					break;
				}
				var sector=sectors.Get(player.Sector);
				if(sector.Fighters > 0 && sector.FighterOwner != player.Record) {
					console.writeln("There are already fighters in this sector!");
					break;
				}
				console.writeln("You have " + (player.Fighters+sector.Fighters) + " fighters available.");
				console.write("How many fighters do you want defending this sector? ");
				var newf=player.Fighters+sector.Fighters;
				if(newf > 9999)
					newf=9999;
				var newf=console.getnum(newf);
				if(newf >= 0 && newf <=player.Fighters+sector.Fighters) {
					if((player.Fighters+sector.Fighters)-newf > 9999) {
						console.writeln("Too many ships in your fleet!  You are limited to 9999");
						break;
					}
					player.Fighters=(player.Fighters+sector.Fighters)-newf;
					sector.Fighters=newf;
					if(sector.Fighters > 0)
						sector.FighterOwner=player.Record;
					sector.Put();
					player.Put();
					console.writeln("Done.  You have " + player.Fighters + " fighter(s) in your fleet.");
				}
				break;
			case 'G':
				/* 27500 */
				console.writeln("<Gamble>");
				console.attributes="HW";
				console.writeln("You have "+player.Credits+" credits.");
				console.writeln("How much do you want to gamble at double or nothing (50-50 odds)");
				console.write("[0]? ");
				var gamble=console.getnum(player.Credits > 99999?99999:player.Credits);
				if(gamble > 0 && gamble <=player.Credits) {
					console.write("Flipping the coin...");
					mswait(250);
					if(random(2)==1) {
						console.crlf();
						player.Credits-=gamble;
						console.writeln("Sorry, you lost.");
					}
					else {
						console.crlf();
						player.Credits+=gamble;
						console.writeln("You won!");
					}
					player.Put();
					console.writeln("You now have "+player.Credits+" credits.");
				}
				break;
			case 'I':
				console.writeln("<Info>");
				PlayerInfo(player.Record);
				break;
			case 'L':
				/* 31000 */
				console.writeln("<Land/Create planet>");
				console.crlf();
				console.attributes="HY";
				console.writeln("Landing...");
				var sector=sectors.Get(player.Sector);
				var planet=null;
				if(sector.Planet==0) {
					/* TODO: Race condition? */
					CreatePlanet(sector);
				}
				else {
					/* 32310 */
					/* Lock the planet file and ensure we can land... */
					if(!Lock(planets.file.name, bbs.node_num, true, 5)) {
						console.writeln("The spaceport is busy.  Try again later.");
						break;
					}
					planet=planets.Get(sector.Planet);
					if(planet.OccupiedBy != 0) {
						console.writeln("The spaceport is busy.  Try again later.");
						Unlock(planets.file.name);
						break;
					}
					planet.OccupiedBy=player.Record;
					planet.Put();
					Unlock(ports.file.name);
					player.Landed=true;
					player.Put();

					Production(planet);
					PlanetReport(planet);
					console.attributes="HW";
					PlanetMenu(planet);

					if(!Lock(planets.file.name, bbs.node_num, true, 10))
						twmsg.writeln("!!! Error locking ports file!");
					planet.OccupiedBy=0;
					planet.Put();
					Unlock(planets.file.name);
					player.Landed=false;
					player.Put();
				}
				break;
			case 'M':
				/* 23000 */
				console.writeln("<Move>");
				if(player.TurnsLeft < 1) {
					console.writeln("I'm sorry but you don't have any turns left.");
					break;
				}
				console.attributes="HW";
				console.write("Warps lead to: ");
				var sector=sectors.Get(player.Sector);
				var i;
				var count=0;
				for(i=0; i<sector.Warps.length; i++) {
					if(sector.Warps[i]>0) {
						if(count>0)
							console.write(", ");
						console.write(sector.Warps[i]);
						count++;
					}
				}
				console.crlf();
				console.write("To which sector? ");
				var to=console.getnum(sectors.length-1);
				if(MoveTo(to))
					return;
				break;
			case 'P':
				console.writeln("<Port>");
				console.attributes="HR";
				console.crlf();
				console.writeln("Docking...");
				if(player.TurnsLeft<1) {
					console.writeln("Sorry, but you don't have any turns left.");
					break;
				}
				var sector=sectors.Get(player.Sector);
				if(sector.Port<1) {
					console.writeln("There are no ports in this sector.");
					break;
				}
				player.TurnsLeft--;
				player.Ported=true;
				player.Put();
				console.writeln("One turn deducted");
				if(player.Sector==1) {
					/* 33200 */
					var prices=SolReport();
					console.attributes="HR";
					console.writeln("You have " + player.Credits + " credits.");
					console.crlf();
					/* Holds */
					var maxbuy=parseInt(player.Credits/prices[0]);
					if(player.Holds+maxbuy>50)
						maxbuy=50-player.Holds;
					if(maxbuy > 0) {
						console.write("How many holds do you want to buy [0]? ");
						var buy=console.getnum(maxbuy);
						if(buy>0) {
							player.Holds+=buy;
							player.Credits -= buy*prices[0];
							player.Put();
							console.writeln("You have " + player.Credits + " credits.");
						}
					}
					/* Fighters */
					var maxbuy=parseInt(player.Credits/prices[1]);
					if(player.Fighters+maxbuy>9999)
						maxbuy=9999-player.Fighters;
					if(maxbuy > 0) {
						console.write("How many fighters do you want to buy [0]? ");
						var buy=console.getnum(maxbuy);
						if(buy>0) {
							player.Fighters+=buy;
							player.Credits -= buy*prices[1];
							player.Put();
							console.writeln("You have " + player.Credits + " credits.");
						}
					}
					/* Turns */
					var maxbuy=parseInt(player.Credits/prices[2]);
					if(maxbuy > 0) {
						console.write("How many turns do you want to buy [0]? ");
						var buy=console.getnum(maxbuy);
						if(buy>0) {
							player.TurnsLeft+=buy;
							player.Credits -= buy*prices[2];
							player.Put();
							console.writeln("You have " + player.Credits + " credits.");
						}
					}
				}
				else {
					var sector=sectors.Get(player.Sector);
					
					/* Lock the port file and ensure we can dock... */
					if(!Lock(ports.file.name, bbs.node_num, true, 5)) {
						console.writeln("The port is busy.  Try again later.");
						break;
					}
					var port=ports.Get(sector.Port);
					if(port.OccupiedBy != 0) {
						console.writeln("The port is busy.  Try again later.");
						Unlock(ports.file.name);
						break;
					}
					port.OccupiedBy=player.Record;
					port.Put();
					Unlock(ports.file.name);
					Production(port);
					var sale=PortReport(sector);
					console.attributes="HR";
					var count=0;

					console.writeln();

					console.attributes="HY";
					/* Sell... */
					for(i=0; i<Commodities.length; i++) {
						if(sale[i].Vary < 0) {
							var amount=Transact(i, sale[i].Price, sale[i].Vary, sale[i].Number);
							if(amount >= 0) {
								count++;
								port.Commodities[i] += amount;
								port.Put();
							}
						}
					}
					/* Buy... */
					for(i=0; i<Commodities.length; i++) {
						if(sale[i].Vary > 0) {
							var amount=Transact(i, sale[i].Price, sale[i].Vary, sale[i].Number);
							if(amount >= 0) {
								count++;
								port.Commodities[i] -= amount;
								port.Put();
							}
						}
					}
					if(count==0) {
						console.attributes="IC";
						console.writeln("You don't have anything they want, and they don't have anything you can buy");
						console.attributes="C";
					}

					if(!Lock(ports.file.name, bbs.node_num, true, 10))
						twmsg.writeln("!!! Error locking ports file!");
					port.OccupiedBy=0;
					port.Put();
					Unlock(ports.file.name);
				}
				player.Ported=false;
				player.Put();
				break;
			case 'T':
				/* 32799 */
				console.attributes="HW";
				console.writeln("<Team menu>");
				TeamMenu();
				break;
			case '?':
				console.attributes="C";
				console.writeln("<Help>");
				console.crlf();
				console.printfile(fname("main.asc"));
				break;

			case 'Z':
				console.writeln("<Instructions>");
				Instructions();
				break;
			case 'Q':
				console.attributes="W";
				console.writeln("<Quit>");
				console.attributes="W";
				console.write("Are you sure (Y/N)? ");
				if(console.getkeys("YN")=='Y') {
					exit_tw2=true;
					return;
				}
				break;
			default:
				if(inp.search(/^[0-9]*$/)!=-1) {
					if(MoveTo(parseInt(inp)))
						return;
				}
				break;
		}
	}
}

function CheckSector()
{
	if(player.Sector==85) {
		console.crlf();
		console.attributes="W";
		console.writeln("Congratulations!  You've defeated the cabal and gained 100,000 points!");
		player.Points += 100000;
		player.Put();
		console.writeln("Unfortunately, victory is fleeting...");

		var sector=sectors.Get(85);
		sector.Fighters=3000;
		sector.FighterOwner=-1;
		sector.Put();
		twopeng.writeln("Congratulations go to "+player.Alias+
           " who invaded the Cabal empire on " + system.datestr() + 
		   " and received 100,000 points!");
		return(false);
	}
	return(true);
}

function EnterSector()	/* 20000 */
{
	var sector=sectors.Get(player.Sector);
	var fighterteam=-1;

	console.attributes="Y";
	DisplaySector(sector.Record);
	if(sector.FighterOwner > 0) {
		var otherplayer=players.Get(sector.FighterOwner);
		if(otherplayer.TeamNumber > 0)
			fighterteam=otherplayer.TeamNumber;
	}
	while(sector.FighterOwner != player.Record && sector.Fighters > 0 && fighterteam != player.TeamNumber) {
		console.writeln("You have to destroy the fighters before you can enter this sector.");
		console.writeln("Fighters: "+player.Fighters+" / "+sector.Fighters);
		console.write("Option? (A,D,I,Q,R,?):? ");
		switch(InputFunc(['A','D','I','Q','R','?'])) {
			case 'A':
				console.writeln("<Attack>");
				var otherteam=false;

				if(sector.FighterOwner > 0) {
					var otherplayer=players.Get(sector.FighterOwner);
					if(otherplayer.TeamNumber>0)
						otherteam-true;
				}
				var killed=DoBattle(sector, otherteam);
				if(killed > 0) {
					player.Points+=k*100;
					console.writeln("You just recieved "+(k*100)+" points for that.");
				}
				if(sector.Fighters==0)
					console.writeln("You destroyed all the fighters.");
				break;
			case 'D':
				console.writeln("<Display>");
				DisplaySector(player.Sector);
				break;
			case 'I':
				console.writeln("<Info>");
				PlayerInfo(player.Record);
				break;
			case 'Q':
				console.write("Are you sure (Y/N)? ");
				if(console.getkeys("YN")=='Y') {
					exit_tw2=true;
					return(false);
				}
				break;
			case 'R':
				console.writeln("<Retreat>");
				if(player.LastIn<1 || player.LastIn>=sectors.length)
					player.LastIn=random(sectors.length-1)+1;
				if(player.Fighters<1) {
					if(random(2)==1) {
						console.writeln("You escaped!");
						player.Sector=player.LastIn;
						player.Put();
						location.Sector=player.Sector; location.Put();
						return(false);
					}
					console.attributes="R";
					console.writeln("You didn't escape!");
					console.writeln("Your ship has been destroyed!");
					console.crlf();
					console.attributes="R";
					console.writeln("You will start over tomorrow with a new ship.");
					KilledBy(player, player, true);
					return(false);
				}
				else {
					player.Fighters--;
					console.writeln("You have "+player.Fighters+" fighter(s) left.");
					player.Sector=player.LastIn;
					player.Put();
					location.Sector=player.Sector; location.Put();
					return(false);
				}
				break;
			case '':
				console.writeln("?=<Help>");
				break;
			case '?':
				console.writeln("<Help>");
				console.crlf();
				console.printfile(fname("entersector.asc"));
				break;
			default:
				console.crlf();
				console.writeln("Invalid command.  ? = Help");
		}
	}
	return(true);
}

function PlayerInfo(num)
{
	var p=players.Get(num);

	console.attributes="HW";
	console.write("   Pilot's Name: "+p.Alias);
	if(p.PlayerNumber>0)
		console.write("  Team ["+p.TeamNumber+"]");
	console.crlf();
	console.writeln("       Fighters: "+p.Fighters);
	console.attributes="HG";
	console.writeln("Sector Location: "+p.Sector);
	console.writeln("     Turns left: "+p.TurnsLeft);
	console.writeln("    Cargo Holds: "+p.Holds);
	console.attributes="HR";
	var i;
	for(i=0; i<Commodities.length; i++)
		console.writeln("     # with "+Commodities[i].abbr+": "+p.Commodities[i]+" ");
	console.attributes="HM";
	console.writeln("        Credits: "+p.Credits);
	console.writeln("    Door points: "+p.Points);
}

function DisplaySector(secnum)
{
	var sector=sectors.Get(secnum);
	var i;
	var count=0;

	console.crlf();
	console.attributes="HY";
	console.writeln("Sector "+secnum);
	console.attributes="HR";
	console.write("Port   ");
	if(sector.Port > 0) {
		var port=ports.Get(sector.Port);
		console.write(port.Name+", class "+port.Class)
	}
	else
		console.write("None");
	console.crlf();
	console.attributes="HB";
	if(sector.Planet) {
		var planet=planets.Get(sector.Planet);
		console.writeln("Planet "+planet.Name);
	}
	console.attributes="HC";
	console.writeln("Other Ships");
	console.attributes="C";
	for(i=1;i<players.length;i++) {
		var otherloc=playerLocation.Get(i);

		if(otherloc.Sector==secnum) {
			var otherplayer=players.Get(i);

			if(otherplayer.UserNumber > 0 && otherplayer.Sector==secnum) {
				if(otherplayer.Record==player.Record)
					continue;
				if(otherplayer.KilledBy!=0)
					continue;

				count++;
				console.crlf();
				console.write("   "+otherplayer.Alias);
				if(otherplayer.TeamNumber>0)
					console.write("  Team ["+otherplayer.TeamNumber+"]");
				console.write(" with "+otherplayer.Fighters+" fighters");
				if(otherplayer.Landed)
					console.write(" (on planet)");
				else if(otherplayer.Ported)
					console.write(" (docked)");
				else if(otherplayer.Online)
					console.write(" (online)");
			}
		}
	}
	if(count==0)
		console.write("   None");
	console.crlf();
	console.attributes="HG";
	console.writeln("Fighters defending sector");
	console.attributes="G";
	if(sector.Fighters==0)
		console.writeln("   None");
	else {
		console.write("   "+sector.Fighters);
		if(sector.FighterOwner<0)
			console.writeln(" (Cabal)");
		else if(sector.FighterOwner==player.Record)
			console.writeln(" (yours)");
		else {
			var otherplayer=players.Get(sector.FighterOwner);
			console.write(" (belong to "+otherplayer.Alias);
			if(otherplayer.TeamNumber)
				console.write("  Team ["+otherplayer.TeamNumber+"]")
			console.writeln(")");
		}
	}
	console.attributes="HW";
	console.write("Warps lead to   ");
	count=0;
	for(i=0; i<sector.Warps.length; i++) {
		if(sector.Warps[i] != 0) {
			if(count)
				console.write(", ");
			console.write(sector.Warps[i]);
			count++;
		}
	}
	console.attributes="W";
	console.crlf();
}

function KilledBy(killed, killer, notify)	/* 15300 */
{
	var i;

	killed.KilledBy=killer.Record;
	killed.Put();
	var killedLocation=playerLocation.Get(killed.Record);
	killedLocation.Sector=killed.Sector; killedLocation.Put();

	/* Destroy all deployed fighters */
	for(i=1; i<sectors.length; i++) {
		var sector=sectors.Get(i);
		if(sector.FighterOwner==player.Record) {
			sector.Fighters=0;
			sector.FighterOwner=0;
			sector.Put();
		}
	}

	if(killed.TeamNumber > 0) {
		var team=teams.Get(killed.TeamNumber);
		var i;
		for(i=0; i<team.Members.length; i++) {
			if(team.Members[i]==killed.Record) {
				team.Members[i]=0;
				team.Size--;
			}
		}
	}

	if(notify)
		twmsg.writeln(" - "+killer.Alias+"  killed "+killed.Alias);
}

function RankPlayers()
{
	var i;
	var rank=new Array();
	var fighters=new Array();

	for(i=1; i<sectors.length; i++) {
		var sector=sectors.Get(i);
		if(sector.Fighters > 0 && sector.FighterOwner > 0) {
			if(fighters[sector.FighterOwner]==undefined)
				fighters[sector.FighterOwner]=0;
			fighters[sector.FighterOwner]+=sector.Fighters;
		}
	}
	for(i=1; i<players.length; i++) {
		var p=players.Get(i);
		if(p.UserNumber==0)
			continue;
		if(p.KilledBy!=0)
			continue;
		var robj=new Object();
		robj.Record=p.Record;
		robj.Score=p.Fighters*100 + p.Holds*500 + p.Credits;
		for(i=0; i<Commodities.length; i++)
			robj.Score += Commodities[i].price*p.Commodities[i];
		if(fighters[i]!=undefined)
			robj.Score += fighters[i]*100;
		rank.push(robj);
	}

	function sortfunc(a,b) {
		return(a.Score-b.Score);
	}

	rank.sort(sortfunc);
	return(rank);
}

function TWRank()
{
	var rf=new File(fname(Settings.TopTenFile));
	rf.open("w");
	rf.writeln();
	rf.writeln("  T R A D E W A R S   I I - 500T   S C O R E B O A R D  ");
	rf.writeln();
	rf.writeln("Last updated at: "+system.timestr());
	rf.writeln();
	rf.writeln("Player Rankings");
	rf.writeln("Rank     Value      Team   Player");
	rf.writeln("==== ============= ====== ================");
	var ranked=RankPlayers();
	var i;
	var trank=new Object();

	for(i=0; i<ranked.length; i++) {
		var p=players.Get(ranked[i].Record);
		if(i<10)
			rf.writeln(format("%4d %13d %6s %s",(i+1),ranked[i].Score,p.TeamNumber==0?"":p.TeamNumber.toString(),p.Alias));
		if(p.TeamNumber.toString() != 0) {
			if(trank[p.TeamNumber.toString()]==undefined)
				trank[p.TeamNumber.toString()]=0;
			trank[p.TeamNumber.toString()]+=ranked[i].Score;
		}
	}
	var tsort=new Array();
	for(tr in trank) {
		var ts=new Object();
		ts.Record=tr;
		ts.Score=trank[tr];
		tsort.push(tr);
	}
	if(tsort.length > 0) {
		function sortfunc(a,b) {
			return(a.Score-b.Score);
		}

		tsort.sort(sortfunc);
		rf.writeln();
		rf.writeln("Team Rankings");
		rf.writeln("Rank     Value      Team");
		rf.writeln("==== ============= ======");
		for(i=0; i<tsort.length; i++) {
			if(i>=10)
				break;
			rf.writeln(format("%4d %13d %6d",(i+1),tsort[i].Score,tsort[i].Record));
		}
	}
	rf.close();
}

function do_exit()
{
	player.Online=false;
	player.Ported=false;
	player.Landed=false;
	player.Put();
	console.writeln("Returning to Door monitor...");
	TWRank();
}

function Instructions()
{
	console.write("Do you want instructions (Y/N) [N]? ");
	switch(console.getkey().toUpperCase()) {
		case 'Y':
			console.crlf();
			console.printfile(fname("twinstr.doc"));
			break;
	}
}

function DoBattle(opp, otherteam)
{
	if(player.Fighters<1) {
		console.writeln("You don't have any fighters!");
		return(0);
	}
	else {
		console.write("How many fighters do you wish to use? ");
		var use=console.getnum(player.Fighters);

		if(use > 0 && use <= player.Fighters) {
			var lost=0;
			var killed=0;

			player.Fighters -= use;
			while(use && opp.Fighters) {
				if(player.TeamNumber > 0 && otherteam) {
					if((random(10)+1) > 6) {
						opp.Fighters--;
						killed++;
					}
					else {
						use--;
						lost++;
					}
				}
				else if(otherteam && player.TeamNumber==0) {
					if((random(10)+1) > 6) {
						use--;
						lost++;
					}
					else {
						opp.Fighters--;
						killed++;
					}
				}
				else {
					if(random(2)==0) {
						use--;
						lost++;
					}
					else {
						opp.Fighters--;
						killed++;
					}
				}
			}
			player.Fighters += use;
			opp.Put();
			player.Put();

			console.writeln("You lost "+lost+" fighter(s), "+player.Fighters+" remain.");
			if(opp.Fighters > 0)
				console.write("You destroyed "+killed+" enemy fighters, "+opp.Fighters+" remain.");
			else
				console.write("You destroyed all of the enemy fighters.");
			return(killed);
		}
	}
	return(0);
}

function SolReport()
{
	var holdprice=parseInt(50*Math.sin(.89756*(today/86400)) + .5)+500;
	var fighterprice=parseInt(10*Math.sin(.89714*(today/86400) + 1.5707) + .5)+100;
	var turnprice=300;

	console.writeln("Commerce report for Sol: " + system.timestr());
	console.writeln("  Cargo holds: " + holdprice + " credits/hold");
	console.writeln("  Fighters   : " + fighterprice + " credits/fighter");
	console.writeln("  Turns      : "+turnprice+" credits each.");
	console.crlf();

	return([holdprice,fighterprice,turnprice]);
}

function Production(place)
{
	var newupd=time();
	var diffdays=(newupd-place.LastUpdate)/86400;

	if(diffdays>10)
		diffdays=10;
	for(i=0; i<Commodities.length; i++) {
		if(diffdays > 0) {
			place.Commodities[i] += place.Production[i]*diffdays;
			if(place.Commodities[i] > place.Production[i]*10)
				place.Commodities[i] = place.Production[i]*10;
		}
	}
	place.LastUpdate=newupd;
	place.Put();
}

function PortReport(sec) {
	var port=ports.Get(sec.Port);
	var i;

	if(port==null)
		return(null);
	/* 33000 */
	Production(port);
	var ret=new Array(Commodities.length);
	for(i=0; i<Commodities.length; i++) {
		ret[i]=new Object();
		ret[i].Vary=port.PriceVariance[i];
		ret[i].Number=parseInt(port.Commodities[i]);
		ret[i].Price=parseInt(Commodities[i].price*(1-ret[i].Vary*ret[i].Number/port.Production[i]/1000)+.5);
	}
	console.crlf();
	console.writeln("Commerce report for "+port.Name+": "+system.timestr());
	console.crlf();
	console.writeln(" Items     Status   # units  in holds");
	console.attributes="HK";
	console.writeln(" =====     ======   =======  ========");
	for(i=0; i<Commodities.length; i++) {
		console.attributes=['HR','HG','HY','HB','HM','HC','HW'][i];
		console.writeln(format("%s %-7s  %-7d  %-7d",
				 Commodities[i].disp
				,ret[i].Vary<0?"Buying":"Selling"
				,ret[i].Number
				,player.Commodities[i]));
	}
	return(ret);
}

function GetKeyEcho()
{
	var ret=console.getkey().toUpperCase();
	if(ret=="\x0a" || ret=="\x0d")
		ret='';
	console.writeln(ret);
	return(ret);
}

function ShortestPath(start, end)
{
	var i,j;
	var hops=0;
	var univ=new Array(sectors.length);
	var done=false;
	var ret=new Array();

	function MarkWarps(univ, pos, end, hops)
	{
		var i;

		for(i=0; i<univ[pos].Warps.length; i++) {
			if(univ[pos].Warps[i]==0)
				continue;
			if(univ[univ[pos].Warps[i]]==undefined) {
				univ[univ[pos].Warps[i]]=sectors.Get(univ[pos].Warps[i]);
				univ[univ[pos].Warps[i]].hops=hops;
				if(univ[pos].Warps[i]==end)
					return(true);
			}
		}
		return(false);
	}

	/* Do the expansion */
	univ[start]=sectors.Get(start);
	univ[start].hops=hops;
	while(!done) {
		for(i=1; i<sectors.length; i++) {
			if(univ[i]!=undefined && univ[i].hops==hops) {
				if(MarkWarps(univ, i, end, hops+1)) {
					done=true;
					break;
				}
			}
		}
		hops++;
	}
	ret.push(end);
	for(i=end;i!=start;) {
		var lasthop=i;
		for(j=0; j<univ[i].Warps.length; j++) {
			if(univ[univ[i].Warps[j]]!=undefined && univ[univ[i].Warps[j]].hops<univ[i].hops) {
				i=univ[i].Warps[j];
				if(i!=start)
					ret.unshift(i);
				break;
			}
		}
		if(i==lasthop)
			return(null);
	}
	return(ret);
}

function MatchPlayer(name)
{
	var i;

	name=name.toUpperCase();
	for(i=1; i<players.length; i++) {
		var p=players.Get(i);
		if(p.UserNumber==0)
			continue;
		if(p.KilledBy!=0)
			continue;
		if(p.Alias.toUpperCase().indexOf(name)!=-1) {
			console.write(p.Alias+" (Y/N)[Y]? ");
			switch(GetKeyEcho()) {
				case 'N':
					break;
				default:
					return(p);
			}
		}
	}
	console.writeln("Not found.");
	return(null);
}

function RadioMessage(from, to, msg)
{
	var i;

	var rmsg;
	for(i=0; i<twrmsg.length; i++) {
		var rmsg=twrmsg.Get(i);
		if(rmsg.Read)
			break;
		rmsg=null;
	}
	if(rmsg==null)
		rmsg=twrmsg.New();
	rmsg.Read=false;
	rmsg.From=from;
	rmsg.To=to;
	rmsg.Message=msg;
	rmsg.Put();
	console.writeln("Message sent.");
	return;
}

function CreatePlanet(sector)
{
	console.writeln("There isn't a planet in this sector.");
	console.writeln("Planets cost 10,000 credits.");
	if(player.Credits < 10000) {
		console.writeln("You're too poor to buy one.");
		return;
	}
	console.writeln("You have " + player.Credits + " credits.");
	console.write("Do you wish to buy a planet(Y/N) [N]? ");
	switch(GetKeyEcho()) {
		case 'Y':
			var i;
			var planet;
			for(i=1; i<planets.length; i++) {
				planet=planets.Get(i);
				if(!planet.Created)
					break;
				planet=null;
			}
			if(planet==null) {
				console.crlf();
				console.writeln("I'm sorry, but all planets are taken.");
				console.writeln("One has to be destroyed before you can buy a planet.");
			}
			else {
				planet.Name='';
				while(planet.Name=='') {
					console.write("What do you want to name this planet? (41 chars. max)? ");
					planet.Name=console.getstr(41);
				}
				for(i=0; i<Commodities.length; i++) {
					planet.Commodities[i]=1;
					planet.Production[i]=1;
				}
				player.Credits-=10000;
				player.Put();
				planet.Sector=sector.Record;
				sector.Planet=planet.Record;
				planet.LastUpdated=time();
				planet.Put();
				sector.Put();
				twmsg.writeln("  -  "+player.Alias+" made a planet: "+planet.Name);
				console.crlf();
				console.writeln("Planet created");
				break;
			}

		default:
			break;
	}
}

function PlanetReport(planet)
{
	console.crlf();
	console.attributes="HC";
	console.writeln("Planet: "+planet.Name);
	console.writeln(" Item      Prod.  Amount  in holds");
	console.attributes="HK";
	console.writeln(" ====      =====  ======  ========");
	var i;
	var freeholds=player.Holds;
	for(i=0; i<Commodities.length; i++) {
		console.attributes=['HR','HG','HY','HB','HM','HC','HW'][i];
		console.writeln(format("%s   %2d  %6d %8d"
				,Commodities[i].disp
				,planet.Production[i]
				,parseInt(planet.Commodities[i])
				,player.Commodities[i]));
		freeholds-=player.Commodities[i];
	}
	console.attributes="HG";
	console.writeln("You have "+freeholds+" free cargo holds.");
}

function PlanetMenu(planet)
{
	var key;
	var freeholds=player.Holds;
	var i;
	var values=new Array('A','I','D','L','R');

	for(i=0; i<Commodities.length; i++) {
		freeholds-=player.Commodities[i];
		values.push((i+1).toString());
	}

	while(true) {
		console.crlf();
		console.write("Planet command (?=help)[A]? ");
		key=InputFunc(values);
		switch(key) {
			case '':
			case 'A':
				console.writeln("<Take all>");
				if(freeholds < 1) {
					console.writeln("You don't have any free holds.");
					continue;
				}
				for(i=Commodities.length-1; i>=0; i--) {
					var take=parseInt(planet.Commodities[i]);
					if(take > freeholds)
						take=freeholds;
					if(take) {
						console.writeln("You took " + take + " holds of "+Commodities[i].name.toLowerCase());
						player.Commodities[i]+=take;
						planet.Commodities[i]-=take;
						freeholds-=take;
					}
					if(freeholds < 1) {
						console.writeln("Your cargo holds are full.");
						break;
					}
				}
				player.Put();
				planet.Put();
				break;
			case 'I':
				console.writeln("<Increase productivity>");
				console.crlf();
				console.writeln("You have "+player.Credits+" credits.");
				var opts="";
				var values=new Array();
				for(i=0; i<Commodities.length; i++) {
					console.writeln((i+1)+" - "+Commodities[i].name+" costs "+(Commodities[i].price*20));
					if(i)
						opts += ',';
					opts += (i+1).toString();
					values.push((i+1).toString());
				}
				console.write("Which one do you want to increase ("+opts+")? ");
				var keynum=parseInt(InputFunc(values));
				if(keynum > 0 && keynum <= Commodities.length) {
					keynum--;
					if(planet.Production[keynum]>19) {
						console.writeln("It's at its maximum value.");
						continue;
					}
					var max=parseInt(player.Credits/(Commodities[keynum].price*20));
					if(max<1) {
						console.writeln("You're too poor.  You only have "+player.Credits+" credits.");
						continue;
					}
					if(planet.Production[keynum]+max > 19)
						max=20-planet.Production[keynum];
					console.write(Commodities[keynum].name+": Increase by how many units? ");
					var incr=console.getnum(max);
					if(incr > 0 && incr <= max) {
						player.Credits -= incr*Commodities[keynum].price*20;
						planet.Production[keynum]+=incr;
						planet.Put();
						player.Put();
					}
				}
				break;
			case 'D':
				console.crlf();
				console.attributes="RI7H";
				console.writeln("*** DESTROY THE PLANET ***");
				console.crlf();
				console.attributes="Y";
				console.write("Are you sure (Y/N)[N]? ");
				if(GetKeyEcho()=='Y') {
					planet.Created=false;
					var sector=sectors.Get(planet.Sector);
					sector.Planet=0;
					planet.Sector=0;
					planet.Put();
					sector.Put();
					twmsg.writeln("  -  " + player.Alias + " destroyed the planet in sector " + sector.Record);
					console.writeln("Planet destroyed.");
					return;
				}
				break;
			case 'L':
				return;
			case 'R':
				PlanetReport(planet);
				break;
			case '?':
				console.crlf();
				console.printfile(fname("planet.asc"));
				break;
			default:
				var keynum=parseInt(key);
				if(keynum > 0 && keynum <= Commodities.length) {
					keynum--;
					var max=freeholds;
					if(max > parseInt(planet.Commodities[keynum]))
						max=parseInt(planet.Commodities[keynum]);
					console.writeln("<Take "+Commodities[keynum].name.toLowerCase()+">");
					console.write("How much [" + max + "]? ");
					var take=console.getnum(max);
					if(take > max) {
						console.writeln("They don't have that many.");
						continue;
					}
					if(take > freeholds) {
						console.writeln("You don't have enough free cargo holds.");
						continue;
					}
					planet.Commodities[keynum]-=take;
					player.Commodities[keynum]+=take;
					freeholds -= take;
					planet.Put();
					player.Put();
				}
				else {
					console.writeln("Invalid command.  Enter ? for help");
				}
		}
	}
}

function ReadRadio()
{
	var i;
	var rmsg;
	var count=0;
	
	console.crlf();
	console.writeln("Checking for Radio Messages sent to you.");
	for(i=0; i<twrmsg.length; i++) {
		var rmsg=twrmsg.Get(i);
		if(rmsg.Read)
			continue;
		if(rmsg.To != player.Record)
			continue;
		console.write("Message from ");
		if(rmsg.From > 0) {
			var p=players.Get(rmsg.From);
			console.write(p.Alias);
		}
		else {
			console.write("A deleted player");
		}
		console.writeln(":");
		console.writeln(rmsg.Message);
		rmsg.Read=true;
		rmsg.Put();
		count++;
	}
	if(count < 1)
		console.writeln("None Received.");
}

function Transact(type, price, vary, avail)
{
	var buy=vary<0?false:true;
	var youare=buy?"buy":"sell";
	var weare=buy?"sell":"buy";
	var emptyholds=player.Holds;
	var max=0;

	var i;
	for(i=0; i<Commodities.length; i++)
		emptyholds -= player.Commodities[i];

	if(buy)
		max=emptyholds;
	else
		max=player.Commodities[type];

	if(max > parseInt(avail))
		max=parseInt(avail);

	if(max < 1)
		return(-1);

	console.crlf();
	console.writeln("You have "+player.Credits+" credits and "+emptyholds+" empty cargo holds.");
	console.crlf();
	console.writeln("We are "+weare+"ing up to "+avail+".  You have "+player.Commodities[type]+" in your holds.");

	console.write("How many holds of "+Commodities[type].name.toLowerCase()+" do you want to "+youare+" ["+max+"]? ");
	var amount=console.getnum(max,max);

	if(amount>0 && amount<=max) {
		var done=false;
		console.writeln("Agreed, "+amount+" units.");
		var haggles=random(3)+2;
		var offer=0;
		for(var hag=0; hag<haggles; hag++) {
			if(hag==haggles-1)
				console.write("Our final offer is ");
			else
				console.write("We'll "+weare+" them for ");
			console.writeln(parseInt(price*amount*(1+vary/1000)+.5));
			offer=0;
			while(offer==0) {
				console.write("Your offer? ");
				offer=console.getnum(buy?player.Credits:(price*amount*10));
				if(offer==0)
					break;
				if(offer < price*amount/10 || offer > price*amount*10) {
					console.writeln("That was pretty funny, Now make a serious offer.");
					offer=0;
				}
			}
			if(offer==0)
				break;
			if(!buy && offer <= price*amount) {
				console.writeln("We'll take them!");
				player.Credits += offer;
				player.Commodities[type] -= amount;
				player.Put();
				return(amount);
			}
			if(buy && offer >= price*amount) {
				console.writeln("Sold!");
				player.Credits -= offer;
				player.Commodities[type] += amount;
				player.Put();
				return(amount);
			}
			var worstoffer=parseInt(amount*price*(1-vary/250/(hag+1)) + .5);
			if(buy && offer < worstoffer)
				break;
			if(!buy && offer > worstoffer)
				break;
			price = .7*price + .3*offer/amount;
		}

		if(offer > 0) {
			if(buy) {
				console.writeln("Too low.  We'll sell them to local traders.");
				return(0);
			}
			console.writeln("Too high.  We'll buy them from local traders.");
		}
		return(0);
	}
	return(0);
}

function TeamMenu()
{

	function TeamHelp() {
		console.crlf();
		if(player.TeamNumber > 0)
			console.printfile(fname("team-member.asc"));
		else
			console.printfile(fname("team-none.asc"));
	}

	TeamHelp();

	while(1) {
		var values=new Array();
		if(player.TeamNumber > 0)
			values=['1','2','3','4','5','6','7','?'];
		else
			values=['1','2','3','?'];
		console.crlf();
		console.attributes="W";
		console.write("Team Command (?=help)? ");
		switch(InputFunc(values)) {
			case '?':
				TeamHelp();
				break;
			case '1':
				return;
			case '2':
				if(player.TeamNumber > 0) {
					console.writeln("You already belong to a Team! ");
					console.writeln("You must quit a team to form another Team.");
					break;
				}
				var password='';
				while(password.length != 4) {
					console.writeln("To form a Team you will need to input a 4 character Team Password");
					console.write("Enter Password [ENTER]=quit? ");
					password=console.getstr(4);
					if(password=='')
						break;
					if(password.length != 4)
						console.writeln("Password MUST be 4 characters");
				}
				if(password=='')
					break;
				console.crlf();
				console.writeln("REMEMBER YOUR TEAM PASSWORD!");
				var team=null;
				for(i=1; i<teams.length; i++) {
					team=teams.Get(i);
					if(team.Size > 0)
						break;
					team=null;
				}
				if(team==null)
					team=teams.New();
				team.Password=password;
				team.Size=1;
				team.Members[0]=player.Record;
				player.TeamNumber=team.Record;
				team.Put();
				player.Put();
				console.attributes="HYI";
				twmsg.writeln(player.Alias+" Created Team "+team.Record+" the " /* TNAM$ */);
				console.writeln("Team number [" + team.Record + "] CREATED!");
				console.attributes="HK";
				break;
			case '3':
				if(player.TeamNumber > 0) {
					console.writeln("You already belong to a Team! ");
					console.writeln("You must quit one team to join another Team.");
					break;
				}
				console.write("What Team number do you wish to join (0=quit)? ");
				var tnum=console.getnum(teams.length-1);
				if(tnum > 0 && tnum < teams.length) {
					var team=teams.Get(tnum);
					if(team.Size > 3) {
						console.writeln("The Team you picked has a maximum amount of members!");
						break;
					}
					console.write("Please enter Password to Join Team #"+team.Record+" ? ");
					var pass=console.getstr(4);
					if(pass != team.Password) {
						console.writeln("Invalid Password entered!");
						break;
					}
					var i;
					for(i=0; i<team.Members.length; i++) {
						if(team.Members[i]==0) {
							team.Members[i]=player.Record;
							team.Size++;
							player.TeamNumber=team.Record;
							team.Put();
							player.Put();
							twmsg.writeln(player.Alias + " Joined Team "+team.Record);
							console.attributes="HC";
							console.writeln("Your Team info has been recorded!  Have fun!");
							console.attributes="HK";
							break;
						}
					}
				}
				break;
			case '4':
				if(player.TeamNumber != 0) {
					console.writeln("You don't belong to a Team!");
					break;
				}
				console.write("Are you sure you wish to quit your Team [N]? ");
				if(GetKeyEcho()=='Y') {
					var team=teams.Get(player.TeamNumber);
					player.TeamNumber=0;
					var i;
					for(i=0; i<team.Members.length; i++) {
						if(team.Members[i]==player.Record) {
							team.Members[i]=0;
							team.Size--;
							player.Put();
							team.Put();
							console.crlf();
							console.attributes="HG";
							console.writeln("You have been removed from Team play");
							console.attributes="HK";
						}
					}
				}
				break;
			case '5':
				console.write("Transfer Credits to another Team member [N]? ");
				if(GetKeyEcho()=='Y') {
					if(player.Credits < 1) {
						console.writeln("You don't have any Credits.");
						break;
					}
					var i;

					var otherplayer=null;
					for(i=1;i<players.length; i++) {
						var otherloc=playerLocation.Get(i);
						if(otherloc.Sector==Player.Sector) {
							otherplayer=players.Get(i);
							if(otherplayer.Sector==player.Sector
									&& otherplayer.Record!=player.Record
									&& otherplayer.KilledBy!=0
									&& otherplayer.UserNumber!=0
									&& otherplayer.TeamNumber==player.TeamNumber) {
								console.write("Transfer Credits to " + otherplayer.Alias + " (Y/[N])? ");
								if(GetKeyEcho()=='Y')
									break;
							}
							otherplayer=null;
						}
					}
					if(otherplayer==null) {
						console.writeln("There is no one else in this sector");
						break;
					}
					while(1) {
						console.writeln("You have" + player.Credits + " credits.");
						console.writeln(otherplayer.Alias + " has" + otherplayer.Credits + " credits.");
						console.write("How many Credits do you wish to Transfer? ");
						var transfer=console.getnum(player.Credits);
						if(transfer<1 || transfer > player.Credits)
							break;
						if(otherplayer.Credits + transfer > 25000) {
							console.writeln("You Team mate will have more than 25,000 credits");
							console.write("Do you wish to complete the Transfer [N]? ");
							if(GetKeyEcho()=='Y')
								break;
						}
						else
							break;
					}
					otherplayer.Credits += transfer;
					player.Credits -= transfer;
					player.Put();
					otherplayer.Put();
					console.writeln("Credits Transfered!");
				}
			case '6':
				console.write("Transfer Fighters to another Team member [N]? ");
				if(GetKeyEcho()=='Y') {
					if(player.Fighters < 1) {
						console.writeln("You don't have any Fighters.");
						break;
					}
					var i;

					var otherplayer=null;
					for(i=1;i<players.length; i++) {
						var otherloc=playerLocation.Get(i);
						if(otherloc.Sector==Player.Sector) {
							otherplayer=players.Get(i);
							if(otherplayer.Sector==player.Sector
									&& otherplayer.Record!=player.Record
									&& otherplayer.KilledBy!=0
									&& otherplayer.UserNumber!=0
									&& otherplayer.TeamNumber==player.TeamNumber) {
								console.write("Transfer Fighters to " + otherplayer.Alias + " (Y/[N])? ");
								if(GetKeyEcho()=='Y')
									break;
							}
						}
						otherplayer=null;
					}
					if(otherplayer==null) {
						console.writeln("There is no one else in this sector");
						break;
					}
					while(1) {
						console.writeln("You have" + player.Fighters + " fighters.");
						console.writeln(otherplayer.Alias + " has" + otherplayer.Fighters + " fighters.");
						console.write("How many Fighters do you wish to Transfer? ");
						var transfer=console.getnum(player.Fighters);
						if(transfer<1 || transfer > player.Fighters)
							break;
						if(otherplayer.Fighters + transfer > 9999)
							console.writeln("Maximum amount of fighters per player is 9999");
						else
							break;
					}
					otherplayer.Fighters += transfer;
					player.Fighters -= transfer;
					player.Put();
					otherplayer.Put();
					console.writeln("Fighters Transfered!");
				}
			case '7':
				console.writeln("Locating Other Team members.");
				console.crlf();
				console.writeln("Name                                     Sector");
				console.writeln("====================                     ======");
				var count=0;
				var i;
				for(i=1;i<players.length; i++) {
					otherplayer=players.Get(i);
					if(otherplayer.Record!=player.Record
							&& otherplayer.UserNumber!=0
							&& otherplayer.TeamNumber==player.TeamNumber) {
						count++;
					}
					console.write(format("%-41s ",otherplayer.Alias));
					if(otherplayer.KilledBy != 0)
						console.write(" Dead!");
					else
						console.write(otherplayer.Sector);
					console.crlf();
				}
				console.crlf();
				if(count==0)
					console.writeln("None Found");
				break;
		}
	}
}

function CabalAttack(cabal)
{
	/* Cabal groups lower than 6 (wandering and defence) do not attack players */
	if(cabal.Record < 6)
		return;

	/* Look for another player in the sector (attacking by rank) */
	var i;
	var attackwith=0;

	var ranks=RankPlayers();
	var sector=sectors.Get(cabal.Sector);
	if(sector.FighterOwner != -1)	/* Huh? */
		return;

	sector.Fighters -= cabal.Size;
	if(sector.Fighters < 0)
		sector.Fighters=0;

	for(i=0; i<ranks.length; i++) {
		var otherplayer=players.Get(ranks[i].Record);

		if(otherplayer.Sector != cabal.Sector)
			continue;
		if(otherplayer.UserNumber==0)
			continue;
		if(otherplayer.KilledBy != 0)
			continue;
		else if(cabal.Record<9)	/* Attack Group */
			attackwith=20;
		else if(cabal.Record==9) { /* Top player hunter */
			if(i==0)
				attackwith=cabal.Size;
			else
				attackwith=0;
		}

		if(attackwith==0)
			continue;
		if(attackwith > cabal.Size)
			attackwith = cabal.Size;

		cabal.Size -= attackwith;
		/* Attack the player */
		var killed=0;
		var lost=0;
		while(otherplayer.Fighters > 0 && attackwith > 0) {
			if(random(2)==1) {
				attackwith--;
				lost++;
			}
			else {
				otherplayer.Fighters--;
				killed++;
			}
		}
		cabal.Size += attackwith;
		twmsg.writeln("      Group "+cabal.Record+" --> "+otherplayer.Alias+": lost "+killed+ ", dstrd "+killed+" ("+(cabal.Size==0?"Cabal":"Player")+" dstrd)");
		if(cabal.Size==0)
			cabal.ReInit();
		else {				/* Player destroyed by the cabal! */
			otherplayer.KilledBy=-1;
			var loc=playerLocation.Get(otherplayer.Record);
			loc.Sector=0;
			loc.Put();
		}
		otherplayer.Put();
		cabal.Put();
	}
	sector.Fighters += cabal.Size;
	if(cabal.Size > 0)
		sector.FighterOwner=-1;
	sector.Put();
}

function MoveCabalGroup(cabal, target)
{
	if(cabal.Sector==0 || cabal.Size < 1)
		return;
	var sector=sectors.Get(cabal.Sector);

	sector.Fighters -= cabal.Size;
	if(sector.Fighters < 1) {
		sector.Fighters=0;
		sector.FighterOwner=0;
	}
	sector.Put();

	/* Set new sector */
	cabal.Sector=target;
	sector=sectors.Get(cabal.Sector);

	/* Attack dropped fighters */
	if(sector.Fighters > 0 && sector.FighterOwner != -1) {
		var killed=0;
		var lost=0;
		var ownername="Deleted Player";
		if(sector.FighterOwner > 0) {
			var fowner=players.Get(sector.FighterOwner);
			ownername=fowner.Alias;
		}
		while(cabal.Size > 0 && sector.Fighters > 0) {
			if(random(2)==1) {
				cabal.Size--;
				lost++;
			}
			else {
				sector.Fighters--;
				killed++;
			}
		}
		/* Place into twpmsg.dat */
		var i;
		var msg=null;
		for(i=0; i<twpmsg.length; i++) {
			msg=twpmsg.Get(i);
			if(msg.Read)
				break;
			msg=null;
		}
		if(msg==null)
			msg=twpmsg.New();
		msg.Read=false;
		msg.To=sector.FighterOwner;
		msg.From=-1;
		msg.Destroyed=killed;
		msg.Put();
		twmsg.writeln("      Group "+cabal.Record+" --> Sector "+target+" ("+ownername+"): lost "+killed+", dstrd "+lost+" ("+(sector.Fighters==0?"Player":"Cabal")+" ftrs dstrd)");
		if(cabal.Size==0) {
			cabal.ReInit();
			cabal.Put();
			return;
		}
		else {
			sector.FighterOwner=-1;
			sector.Fighters=cabal.Size;
		}
		sector.Put();
	}

	CabalAttack(cabal);
	if(cabal.Sector==0 || cabal.Size < 1)
		return;

	/* Merge into another group */
	var i;
	for(i=1; i<cabals.length; i++) {
		if(i==cabal.Record)
			continue;
		var othercabal=cabals.Get(i);
		/* Merge Groups */
		if(othercabal.Sector==cabal.Sector) {
			if(othercabal.Record < cabal.Record) {
				othercabal.Size += cabal.Size;
				cabal.ReInit();
			}
			else {
				cabal.Size += othercabal.Size;
				othercabal.ReInit();
			}
			cabal.Put();
			othercabal.Put();
		}
	}
}

function MoveCabal()
{
	var total=0;
	var i;

	console.writeln("Moving the Cabal.");
	twmsg.writeln("    Cabal report:");
	/* Validate Groups and count the total */
	for(i=1; i<cabals.length; i++) {
		var cabal=cabals.Get(i);
		var sector=sectors.Get(cabal.Sector);
		cabal.Size=sector.Fighters;
		if(sector.FighterOwner != -1 || sector.Fighters==0) {
			cabal.Sector=0;
			cabal.Size=0;
			cabal.Goal=0;
		}
		cabal.Put();
		total+=cabal.Size;
	}

	/* Move group 2 into sector 85 (merge into group 1) */
	var cabal=cabals.Get(2);
	MoveCabalGroup(cabal, 85);

	/* Note, this seems to have a max limit of 2000 for regeneration */
	var regen=Settings.CabalRegeneration;
	if(total+regen > 2000)
		regen=2000-total;
	if(regen<1)
		regen=0;

	cabal=cabals.Get(1);
	cabal.Size += regen;

	/* Split off group 2 */
	group2=cabals.Get(2);
	if(cabal.Size > 1000) {
		group2.Size=cabal.Size-1000;
		group2.Sector=85;
		MoveCabalGroup(group2, 83);
	}

	/* Create wandering groups */
	for(i=3; i<6; i++) {
		var wgroup=cabals.Get(i);
		if(wgroup.Size < 1 || wgroup.Sector < 1 || wgroup.Sector >= sectors.length) {
			if(group2.size >= 600) {
				wgroup.Size=100;
				group2.size-=100;
				wgroup.Sector=83;
				wgroup.Goal=0;
				while(wgroup.Goal < 8)
					wgroup.Goal=random(sectors.length);
			}
			else {
				wgroup.ReInit();
			}
			wgroup.Put();
		}
	}

	/* Create attack groups */
	for(i=6; i<9; i++) {
		var agroup=cabals.Get(i);
		if(agroup.Size < 1 || agroup.Sector < 1 || agroup.Sector >= sectors.length) {
			if(group2.size >= 550) {
				agroup.Size=50;
				group2.size-=50;
				agroup.Sector=83;
				agroup.Goal=0;
				while(agroup.Goal < 8)
					agroup.Goal=random(sectors.length);
			}
			else {
				agroup.ReInit();
			}
			agroup.Put();
		}
	}
	
	/* Create hunter group */
	var hgroup=cabals.Get(9);
	if(hgroup.Size < 1 || hgroup.Sector < 1 || hgroup.Sector >= sectors.length) {
		if(group2.size >= 500) {
			hgroup.Size=group2.Size-500;
			group2.size-=hgroup.Size;
			hgroup.Sector=83;
			hgroup.Goal=0;
		}
		else {
			hgroup.ReInit();
		}
		hgroup.Put();
	}
	group2.Put();

	/* Move wandering groups */
	for(i=3; i<6; i++) {
		var wgroup=cabals.Get(i);
		if(wgroup.Size < 1 || wgroup.Sector == 0)
			continue;
		/* Choose new target */
		if(wgroup.Sector == wgroup.Goal) {
			wgroup.Goal=0;
			while(wgroup.Goal < 8)
				wgroup.Goal=random(sectors.length);
		}
		if(wgroup.Size < 50 || wgroup.Size > 100)
			wgroup.Goal=85;
		var path=ShortestPath(wgroup.Sector, wgroup.Goal);
		while(path[0] < 8)
			path.shift();
		MoveCabalGroup(wgroup,path[0]);
	}

	/* Move Attack Groups */
	for(i=6; i<9; i++) {
		var agroup=cabals.Get(i);
		if(agroup.Size < 1 || agroup.Sector == 0)
			continue;
		/* Choose new target */
		if(agroup.Sector == agroup.Goal) {
			agroup.Goal=0;
			while(agroup.Goal < 8)
				agroup.Goal=random(sectors.length);
		}
		if(agroup.Size < 20 || agroup.Size > 50)
			agroup.Goal=85;
		var path=ShortestPath(agroup.Sector, agroup.Goal);
		var next;
		while(path.length > 0 && agroup.Size > 0) {
			var next=path.shift();
			if(next < 8)
				continue;
			MoveCabalGroup(agroup,next);
		}
	}

	/* Move hunter groups... */
	for(i=9; i<10; i++) {
		var hgroup=cabals.Get(i);
		if(hgroup.Size < 1 || hgroup.Sector == 0)
			continue;
		/* Choose target */
		var ranks=RankPlayers();
		if(ranks.length < 0) {
			var player=players.Get(ranks[0].Record);
			hgroup.Goal=player.Sector;
			var path=ShortestPath(hgroup.Sector, hgroup.Goal);
			var next;
			while(path.length > 0 && hgroup.Size > 0) {
				var next=path.shift();
				MoveCabalGroup(agroup,next);
			}
		}
	}
}

function DeletePlayer(player)
{
	/* Delete player */
	player.ReInit();
	player.UserNumber=0;
	player.Alias="<Deleted>";
	player.Sector=0;
	player.Put();
	/* Move to sector zero */
	var loc=playerLocation.Get(player.Record);
	loc.Sector=0;
	loc.Put();
	/* Set fighter owner to "Deleted Player" */
	var i;
	for(i=1; i<sectors.length; i++) {
		var sector=sectors.Get(i);
		if(sector.FighterOwner==player.Record) {
			sector.FighterOwner=-98;
			sector.Put();
		}
	}
	/* Set messages TO the deleted player as read and FROM as from deleted */
	for(i=0; i<twpmsg.length; i++) {
		var msg=twpmsg.Get(i);

		if(msg.To==player.Record && !msg.Read) {
			msg.Read=true;
			msg.Put();
		}
		else if(msg.From==player.Record) {
			msg.From=-98;
			msg.Put();
		}
	}
	/* Set radio messages TO the deleted player as read and FROM as from deleted */
	for(i=0; i<twrmsg.length; i++) {
		var msg=twrmsg.Get(i);

		if(msg.To==player.Record && !msg.Read) {
			msg.Read=true;
			msg.Put();
		}
		else if(msg.From==player.Record) {
			msg.From=-98;
			msg.Put();
		}
	}
	/* Set killed bys to Deleted Player */
	for(i=1; i<players.length; i++) {
		var otherplayer=players.Get(i);

		if(otherplayer.KilledBy==player.Record) {
			otherplayer.KilledBy=-98;
			otherplayer.Put();
		}
	}
	twmsg.writeln("  - "+player.Alias+" deleted from game");
}

function InputFunc(values)
{
	var str='';
	var pos=0;
	var insertmode=true;
	var key;
	var origattr=console.attributes;
	var matchstr=''

	console.line_counter=0;
	console.attributes="N";
InputFuncMainLoop:
	for(;;) {
		key=console.inkey(100);
		if(key == '') {
			/* Busy loop checking */
		}
		else {
			switch(key) {
				case '\x1b':	/* Escape */
					matchstr='';
					pos=0;
					break InputFuncMainLoop;
				case '\r':
					break InputFuncMainLoop;
				case '\x08':	/* Backspace */
					if(pos==0)
						break;
					console.write('\x08 \x08');
					str=str.substr(0,-1);
					pos--;
					/* Fall-through */
				default:
					if(key != '\x08') {
						/* No CTRL chars (evar!) */
						if(ascii(key)<32)
							break;
						str=str.substr(0,pos)+key+str.substr(pos);
						pos++;
						console.write(key);
					}
					/* Is this an exact match AND the longest possible match? */
					var value;
					var exact_match=false;
					var longer_match=false;
					matchstr='';
					for(value in values) {
						if(typeof(values[value])=='string') {
							var ucv=values[value].toUpperCase();
							var ucs=str.toUpperCase();
							if(ucv==ucs) {
								exact_match=true;
								matchstr=values[value];
							}
							else if(ucv.indexOf(ucs)==0)
								longer_match=true;
						}
						else if(typeof(values[value])=='object') {
							if(str.search(/^[0-9]*$/)!=-1) {
								var min=0;
								var max=4294967296;
								var cur=parseInt(str);
								if(values[value].min != undefined)
									min=values[value].min;
								if(values[value].max != undefined)
									max=values[value].max;
								if(cur >= min && cur <= max) {
									exact_match=true;
									matchstr=cur.toString();
								}
								if(cur*10 <= max)
									longer_match=true;
							}
						}
					}
					if(exact_match && !longer_match)
						break InputFuncMainLoop;
					/* That's not valid! */
					if((!longer_match) && pos > 0) {
						console.write('\x08 \x08');
						str=str.substr(0,-1);
						pos--;
					}
			}
		}
	}

	while(pos > 0) {
		console.write('\x08');
		pos--;
	}
	console.write(matchstr);
	console.attributes=origattr;
	console.crlf();
	return(matchstr);
}

function MoveTo(to)
{
	var sector=sectors.Get(player.Sector);

	if(to > 0) {
		for(i=0; i<sector.Warps.length; i++) {
			if(sector.Warps[i]==to) {
				player.TurnsLeft--;
				player.LastIn=player.Sector;
				player.Sector=to;
				player.Put();
				location.Sector=player.Sector; location.Put();
				if(player.TurnsLeft==10 || player.TurnsLeft < 6) {
					console.writeln("You have " + player.TurnsLeft + " turns left.");
				}
				return(true);
			}
		}
		console.writeln("You can't get there from here.");
	}
	return(false);
}
