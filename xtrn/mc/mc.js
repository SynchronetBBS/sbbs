/*
{                             Milliways Casino                               }
{                  Copyright (C) 1987 by Charles Ezzell & Matthew Warner     }
{                            All Rights Reserved                             }
{                                                                            }
{  No part of this program may be reprinted or reproduced or utilized in any }
{  form or any electronic, mechanical, or other means no known or hereafter  }
{  invented, including photocopying and recording, or in any information     }
{  storage and retrieval system, without permission in writing from the      }
{  author, except for those uses listed below.                               }
{                                                                            }
{  1) This program is 'freeware'.  No charges may be made by anyone or any   }
{     service for copying this program for the use of others.                }
{  2) This program can be freely distributed in UNMODIFIED form by any       }
{     Bulletin Board System Operator (hereafter known as Sysop), or any      }
{     user of a BBS system.                                                  }
{  3) A Sysop has the right to modify this program only for his/her use.  He }
{     cannot distribute a modified copy without permission of the author.    }
{  4) Any modifications or improvements made by a Sysop (or user/programmer) }
{     may be sent to the author.                                             }
{  5) The copyright notice and name of the program can not be removed from   }
{     the program.  It must be shown each time the program is ran.           }
{     (Its the law, not my idea)                                             }
{  6) By running this program, you agree to all the above conditions.        }
{                                                                            }
{                                                                            }
{  Questions and Answers about the copyright by me.                          }
{  What rights does this give a sysop?                                       }
{  Very simple.  You can use it just like any other WWIV chn file, as long   }
{  as you don't remove the copyright notice or change the name of the game.  }
{  You can modify it, re-do the variables, or whatever, in order to get it   }
{  to work on your system.  I give you these rights.  You CAN NOT distribute }
{  a modified copy.  I have done things this way in order to protect my      }
{  rights, and those of others who have helped me in writing this.           }
{  All official releases will come through me, and be distributed through    }
{  the people listed below who have volunteered their services.              }
{  If you have any new ideas or suggestion to improve the game, please send  }
{  them to me or one of the distributors and they will see that I get them.  }
{  I've worked long and hard on this game, and, not wanting to do like       }
{  others who only send out .chn files of their programs, am releasing the   }
{  source code since many sysops have modified common.pas.                   }
{  This program has been compiled with both the original common.pas and with }
{  mine, with no problems, so most everyone of you out there should have no  }
{  problems.  ( Mine is approx 38k long, so most of you should not get       }
{  get memory overflow errors).                                              }
{                                                                            }
{                               DISTRIBUTORS                                 }
{                               ------------                                 }
{    Charles Ezzell   Milliways                         (919)823-5897        }
{    Matthew Warner   The United Star Ship Saratoga     (919)443-9343        }
{    Jami Chism       The Pits                          (919)362-8053        }
{    Brandon Poole    The Boinger Board                 (919)846-3734        }
{                                                                            }
{  Any sysop or user may distribute the program (unmodified).  If you do get }
{  a copy that does not seem to work, contact one of the above, and let them }
{  know who sent you the program.  If you feel the program has been modified }
{  LET ME KNOW!                                                              }
{                                                                            }
{  If any problems occur, feel free to contact me.  I plan on adding new     }
{  features into the game myself, and any help would be appreciated.         }
{                                                                            }
{  All those who send in mods that are used in future releases get first     }
{  crack at the new release.                                                 }
{                                                                            }
{  A note about some of the procedures used:                                 }
{  procedure setcol(byte):  this procedure checks to see if the user has     }
{    selected ansi graphics, and if so, sets the colors using Wayne Bell's   }
{    setc(byte) procedure.  0-15 give foreground colors, add 16 to get the   }
{    background (haven't tried it with 24-31), and add 128 to get blink.     }
{    setcol(132) should give a blinking red on most systems.                 }
{  procedure casdisplay(str):  modified from my graphics mods and Othello    }
{    game, this procedure is used to move cursor, clear screen, and in the   }
{    roulette and slots games, used for color control (this procedure was    }
{    written before the setcol).                                             }
{  procedure print and print1(str):  These 2 procedures are modified         }
{    versions of Wayne Bell's print and print1 procedures.  The only         }
{    difference is they do not allow the user to stop printing the line out. }
{    It also allows for formatting the line length to the users length,      }
{    unlike the print command.  Colors could be added into (haven't checked) }
{    the lines by inserting the correct codes at the beginning of each       }
{    string.  This routines do not do a line feed or carriage return at the  }
{    end of the line, so that must be inserted in (or use procedure nl from  }
{    common.pas.                                                             }
{    Example:  Printaa(#1+#2+'Hi there'+#13+#10);                            }
{              this should give you print Hi there in ansic(2), with a cr/lf }
{              print('Hi there');                                            }
{              this would print 'Hi there', leaving the cursor at the end of }
{              of the line.                                                  }
{                                                                            }
{  If any questions, feel free to contact me on any of the BBS's systems     }
{  listed above, or write me.                                                }
{                                                                            }
{  Charles N. Ezzell                                                         }
{  Route 2, Box JV41                                                         }
{  Tarboro, NC 27886                                                         }
{  (919) 823-5897  Milliways                                                 }
{                                                                            }
{  Revision History                                                          }
{  Nov. 18, 1987  Version 1.0 released.                                      }
{                                                                            }
*/

load("recordfile.js");
load("sbbsdefs.js");

var game_dir='.';
try { throw barfitty.barf(barf) } catch(e) { game_dir=e.fileName }
game_dir=game_dir.replace(/[\/\\][^\/\\]*$/,'');
game_dir=backslash(game_dir);

load(game_dir+"shared.js");
js.on_exit('leave()');
load(game_dir+"roulette.js");
load(game_dir+"slots.js");
load(game_dir+"baccarat.js");
load(game_dir+"twenty1.js");

var roulette=new Roulette();
var slots=new Slots();
var baccarat=new Baccarat();
var twenty1=new Twenty1();

function create_player()
{
	player=players.new();
}

function enter_casino()
{
	console.print('  As you walk though the large, glass, double doors, you catch\r\n');
	console.print('your reflection in the glass.  You are wearing Bermuda shorts,\r\n');
	console.print('a LOUD Hawaiian shirt, and sandals.  You can smell the odor of\r\n');
	console.print('stale cigars, pipes, and cheap perfume.\r\n');
	console.crlf();
	console.crlf();
	console.print('  You check your wallet.  $1,000.  Your life savings.  You pause,\r\n');
	console.print('briefly, deciding on whether to give the money to your church,\r\n');
	console.print('or try your luck.\r\n');
	console.crlf();
	console.crlf();
	console.pause();
	console.print('Oh, What the hell...\r\n');
	console.crlf();
	console.crlf();
	console.print("  One of the beautiful, young, almost naked, hostess's comes up to you\r\n");
	console.print('and offers you a drink.  You accept, and when she returns, she offers\r\n');
	console.print("to take your money to the cashier to exchange it for one of Milliway's\r\n");
	console.print('credit cards (needed to play all games!).  You agree to this\r\n');
	console.print("since the Casino is so large, you are afraid you'd get lost if you\r\n");
	console.print('tried to do this yourself.  You sit back, in amazement, watching\r\n');
	console.print('all the people around you.\r\n');
	console.crlf();
	console.crlf();
	console.pause();
	console.crlf();
	console.crlf();
	console.crlf();
	console.print ('After a short time, the hostess returns with your card.  You notice\r\n');
	console.print ('that your name is embossed on it, along with a logo that looks like\r\n');
	console.crlf();
	console.crlf();
	console.crlf();
	console.print ('...a space ship with a golden heart ?????\r\n');
	console.crlf();
	console.crlf();
}

function find_player()
{
	var is_new=false;
	var found=false;
	var i;

	if((!messagefile.exists) || messagefile.length==0) {
		messagefile.open('a');
		messagefile.writeln('Current file begins on '+system.datestr()+'.');
		messagefile.writeln('');
		messagefile.close();
	}

	stranger=game.get(0);
	if(stranger==null)
		stranger=game.new();
	if(stranger.dating_kathy)
		stranger_dates_kathy=true;
	else
		stranger_dates_kathy=false;
	if(stranger.old_date != system.datestr()) {
		sysoplog("\r\n***  New Day for the Casino  ***\r\n");
		stranger.old_date=system.datestr();
		for(i=0; i<players.length; i++) {
			player=players.get(i);
			player.played_roulette=0;
			player.played_baccarat=0;
			player.played_twenty1=0;
			player.played_slots=0;
			player.won_today=0;
			player.Put();
		}
	}

	for(i=0; i<players.length; i++) {
		player=players.get(i);
		if(player.UserNumber == user.number) {
			if(!file_exists(system.data_dir+format("user/%04d.mc",player.UserNumber))) {
				player.ReInit();
				player.Put();
				file_touch(system.data_dir+format("user/%04d.mc",player.UserNumber));
				found=true;
				is_new=true;
			}
			else {
				found=true;
				break;
			}
		}
	}
	if(!found) {
		is_new=true;
		player=players.new();
	}
	check_winnings();
	if(player.last_on != system.datestr())
		player.times_on++;
	return(is_new);
}

function welcome()
{
	find_player()
	if(player.times_on != 1) {
		console.crlf();
		console.crlf();
		console.crlf();
		logon(2);
		console.crlf();
		console.crlf();
		console.crlf();
		console.print('Welcome back '+player.name+'\r\n');
		console.print('You have '+format_money(player.players_money)+'.  Good luck!\r\n');
		console.crlf();
		console.crlf();
		console.pause();
	}
	else {
		logon(1);
		console.pause();
		console.crlf();
		console.crlf();
		console.crlf();
		console.print('  You tip the cabbie as you exit the yellow cab.  He looks back\r\n');
		console.print("as you walk away from the cab, and yells, \"Man, you'd better\r\n");
		console.print('turn down that volume of yours!" and with a screech of rubber,\r\n');
		console.print('he leaves you in a smokescreen.  You stare in amazement at the\r\n');
		console.print('large neon sign above you.\r\n');
		console.crlf();
		console.crlf();
		console.pause();
		logon(3);
		console.crlf();
		console.crlf();
		enter_casino();
	}
}

function check_played(i)
{
	switch(i) {
		case 1:
			if(player.played_slots >= 10) {
				console.print("I'm sorry, but you can only visit\r\n");
				console.print('the slot machines 10 times a day.\r\n');
				return(false);
			}
			return(true);
		case 2:
			if(player.played_roulette >= 5) {
				console.print("I'm sorry, but you can only visit\r\n");
				console.print('the roulette tables 5 times a day.\r\n');
				return(false);
			}
			return(true);
		case 3:
			if(player.played_twenty1 >= 10) {
				console.print("I'm sorry, but you can only visit\r\n");
				console.print('the blackjack tables 10 times a day.\r\n');
				return(false);
			}
			return(true);
		case 4:
			if(player.played_baccarat >= 3) {
				console.print("I'm sorry, but you can only visit\r\n");
				console.print('the baccarat tables 10 times a day.\r\n');
				return(false);
			}
			return(true);
	}
	return(false);
}

function logon(a)
{
	var j='               ';
	ansic(4);
	console.print(j);
	console.print('…ÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕª\r\n');
	ansic(4);
	console.print(j); 
	console.print('∫ ');
	ansic(2);
	console.print('€ﬂ€ﬂ€  €  €    €    €  €   €  €ﬂﬂ€  € €  €ﬂﬂ');
	ansic(4);
	console.print(' ∫\r\n');

	ansic(4); console.print(j); console.print('∫ '); ansic(2);
	console.print('€ ﬂ €  €  €    €    €  € € €  €ﬂﬂ€   €   ﬂﬂ€');
	ansic(4); console.print(' ∫\r\n');

	ansic(4); console.print(j); console.print('∫ '); ansic(2);
	console.print('ﬂ   ﬂ  ﬂ  ﬂﬂﬂ  ﬂﬂﬂ  ﬂ  ﬂﬂﬂﬂﬂ  ﬂ  ﬂ   ﬂ   ﬂﬂﬂ');
	ansic(4); console.print(' ∫\r\n');

	ansic(4); console.print(j); console.print('∫ '); ansic(2);
	console.print('       €ﬂﬂﬂ  €ﬂﬂ€  €ﬂﬂ  €  €‹  €  €ﬂﬂ€      ');
	ansic(4); console.print(' ∫\r\n');

	ansic(4); console.print(j); console.print('∫ '); ansic(2);
	console.print('       €     €ﬂﬂ€  ﬂﬂ€  €  € ﬂ‹€  €  €      ');
	ansic(4); console.print(' ∫\r\n');

	ansic(4); console.print(j); console.print('∫ '); ansic(2);
	console.print('       ﬂﬂﬂﬂ  ﬂ  ﬂ  ﬂﬂﬂ  ﬂ  ﬂ   ﬂ  ﬂﬂﬂﬂ      ');
	ansic(4); console.print(' ∫\r\n');

	ansic(4); console.print(j);
	console.print('»ÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕÕº\r\n');

	if(a==1 || a==2) {
		ansic(5);
		console.center('Copyright (C) 1987 by Charles Ezzell & Matthew Warner');
		console.center('All rights reserved.');
	}
	if(a==1) {
		ansic(1);
		console.center('Conceived, Written, Produced, Directed by');
		ansic(2);
		console.right(15);
		console.print('Charles Ezzell ');
		ansic(1);
		console.print('(Marvin) ');
		ansic(5);
		console.print('\xaf Milliways (919)823-5897\r\n');
		console.right(32);
		ansic(1);
		console.print('Creative help\r\n');
		console.right(1);
		ansic(2);
		console.print('Matthew Warner ');
		ansic(1);
		console.print('(Eagle Fighter) ');
		ansic(5);
		console.print('\xaf The United Star Ship Saratoga (919)443-9343\r\n');
		console.right(9);
		ansic(2);
		console.print('Clint Williams ');
		ansic(0);
		console.print('(Sheriff) ');
		ansic(5);
		console.print('\xaf 64th P-R-E-C-I-N-C-T (919)443-9740\r\n');
		console.right(31);
		ansic(1);
		console.print('Special Thanks To\r\n');
		console.right(11);
		ansic(2)
		console.print('Wayne Bell ');
		ansic(5);
		console.print('Author of WWIV.\r\n');
		console.right(11);
		ansic(2);
		console.print('Preston Stroud ');
		ansic(5);
		console.print('\xaf currently on The Megaboard (919)522-1736\r\n');
		console.right(11);
		ansic(2);
		console.print('Brandon Poole ');
		ansic(5);
		console.print('\xaf The Boinger Board (919)846-3734\r\n');
		console.right(11);
		ansic(2);
		console.print('All the users of Milliways, for putting up with me while\r\n');
		console.right(11);
		ansic(2);
		console.print('writing this program, keeping the system down as much as\r\n');
		console.right(11);
		ansic(2)
		console.print('up, and constantly dropping them off the game.  ');
		ansic(3);
		console.print('THANKS');
		ansic(2);
		console.print('!\r\n');
	}
	ansic(0);
}

function general_instructions()
{
	console.print('  The goal of the game is simple.  Accumulate $1,000,000, and marry\r\n');
	console.print('Kathy.  However, there are several obstacles in your way.  First of\r\n');
	console.print("all, you have a rival for Kathy's affections--the 'Stranger', who\r\n");
	console.print('has an unlimited supply of money.  Your goal is to win her affections,\r\n');
	console.print("before he does.  Kathy is a 'working' girl, if you get my meaning here,\r\n");
	console.print('therefore you will have to spend some money on her.\r\n');
	console.crlf();
	console.crlf();
	console.pause();
	console.print('  Another obstacle in the game is Bruno.  He is a friendly chap, and\r\n');
	console.print("has been known to help out players that don't have enough money to\r\n");
	console.print('cover a bet by buying different items from the player, then selling\r\n');
	console.print('them back after the player has enough money to do so.  However,\r\n');
	console.print("there does come a time when you won't have anything to sell, in which\r\n");
	console.print('case your best bet is to get out of town quick!\r\n');
	console.crlf();
	console.crlf();
	console.pause();
	console.print('  Most of the casino games are familiar to all.  There is also a\r\n');
	console.print('constantly changing slots jackpot.  The money for the jackpot changes\r\n');
	console.print('according to what has been won or lost.  If the house wins, 10% of the\r\n');
	console.print('BET is added to the jackpot.  However, if the house losses, 5% of the\r\n');
	console.print('WIN is deducted to help cover expenses.  The house does not add\r\n');
	console.print('or subtract any money won at baccarat.  This game is provided as a\r\n');
	console.print("service of Milliways to let you try to win money from the 'Stranger'\r\n");
	console.crlf();
	console.crlf();
	console.pause();
	console.print('GOOD LUCK!\r\n');
}

function do_stats()
{
	var all_players=new Array();
	var i, real_money;

	player.Put();
	for(i=0; i<players.length; i++)
		all_players.push(players.get(i));

	all_players=all_players.sort(function(a,b) {
		return(real_dough(b)-real_dough(a));
	});
	console.print("Player's Name                                     Money         Real\r\n");
	console.print('---------------------------------------------   ----------   -----------\r\n');
	for(i in all_players) {
		real_money=real_dough(all_players[i]);
		console.print(format("%-41s   %10s  %11s\r\n",all_players[i].name,format_money(all_players[i].players_money),format_money(real_money)));
	}
	tleft();
	console.print('Time Left: '+tlef()+"\r\n");
	console.crlf();
	console.pause();
}

/* Main... */
function mc_main()
{
	sysoplog("     Played Milliway's Casino");
	won=false;
	welcome();
	setcol(1);
	console.print("Do you wish to see what's\r\n");
	console.print("happened to the others in the game? ");
	setcol(0);
	if(yn()) {
		console.printfile(messagefile.name);
		console.pause();
	}
	done=false;
	slot=roul=twenty=bacc=false;
	do {
		tleft();
		check_winnings();
		console.print('You are standing in front of 4 doors.\r\n');
		console.print('Each door is numbered, and has a sign underneath.\r\n');
		console.crlf();
		console.print('1 : Slots\r\n');
		console.print('2 : Roulette\r\n');
		console.print('3 : Blackjack\r\n');
		console.print('4 : Baccarat\r\n');
		console.crlf();
		console.print('I : Instructions, Q: Quit,\r\n');
		console.print('S : Global Stats, Y: Your Stats\r\n');
		console.print('Which door do you enter? ');
		switch(console.getkeys('1234IQSY')) {
			case 'Q':
				done=true;
				break;
			case 'I':
				general_instructions();
				break;
			case '1':
				if(check_played(1)) {
					slot=true;
					player.played_slots++;
					slots.play();
					slot=false;
				}
				break;
			case '2':
				if(check_played(2)) {
					roul=true;
					player.played_roulette++;
					roulette.play();
					roul=false;
				}
				break;
			case '3':
				if(check_played(3)) {
					twenty=true;
					player.played_twenty1++;
					twenty1.play();
					twenty=false;
				}
				break;
			case '4':
				if(stranger_dates_kathy) {
					console.print('All the tables are filled right now.\r\n');
					console.print('Please try again later.\r\n');
				}
				else {
					if(check_played(4)) {
						bacc=true;
						player.played_baccarat++;
						baccarat.play();
						bacc=false;
					}
				}
				break;
			case 'S':
				do_stats();
				break;
			case 'Y':
				player_stats();
				break;
		}
	} while(!done);
	leave();
}

mc_main();
