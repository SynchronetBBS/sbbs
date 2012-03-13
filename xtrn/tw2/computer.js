function ComputerMenu()
{
	console.crlf();
	console.attributes="HW";
	console.writeln("<Computer activated>");
	var showhelp=true;
	var sec;
	var seclen;

	for(;;) {
		if(showhelp) {
			console.crlf();
			if(user.settings&USER_ANSI)
				console.printfile(fname("computer.ans"));
			else
				console.printfile(fname("computer.asc"));
			showhelp=false;
		}
		console.attributes="HW";
		console.crlf();
		console.write("Computer command (?=help)? ");
		switch(InputFunc(['?','X','P','S','R','M'])) {
			case '?':
				showhelp=true;
				break;
			case 'X':
				/* 33990 */
				console.crlf();
				console.attributes="Y";
				console.writeln("<Computer deactivated>");
				return;
			case 'P':
				/* 33780 */
				console.write("What sector number is the port in? ");
				seclen=db.read(Settings.DB,'sectors.length',LOCK_READ);
				sec=InputFunc([{min:0,max:seclen-1}]);
				if(sec > 0 && sec < seclen) {
					var sector=db.read(Settings.DB,'sectors.'+sec,LOCK_READ);
					if(sector.Port==0 || (sector.Fighters>0 && sector.Fighters!=player.Record)) {
						console.crlf();
						console.writeln("I have no information about that port.");
						break;
					}
					if(sec==1) {
						SolReport();
					}
					else {
						PortReport(sector.Port);
					}
				}
				break;
			case 'S':
				/* 33830 */
				console.write("What sector do you want to get to? ");
				seclen=db.read(Settings.DB,'sectors.length',LOCK_READ);
				sec=InputFunc([{min:0,max:seclen-1}]);
				if(sec > 0 && sec < seclen) {
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
						console.crlf();
						console.crlf();
					}
				}
				break;
			case 'R':
				/* 33900 */
				console.crlf();
				console.write("Ranking Players.");
				TWRank();
				console.writeln(db.read(Settings.DB,'ranking',LOCK_READ));
				break;
			case 'M':
				/* 33960 */
				SendRadioMessage();
				break;
			default:
				console.crlf();
				console.writeln("Invalid command.  Enter ? for help");
		}
	}
}
