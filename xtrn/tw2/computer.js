function ComputerMenu()
{
	console.crlf();
	console.attributes="HW";
	console.writeln("<Computer activated>");
	var showhelp=true;

	for(;;) {
		if(showhelp) {
			console.crlf();
			console.printfile(fname("computer.asc"));
			showhelp=false;
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
				return;
			case '2':
				/* 33780 */
				console.write("What sector number is the port in? ");
				var sec=InputFunc([{min:0,max:sectors.length-1}]);
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
				var sec=InputFunc([{min:0,max:sectors.length-1}]);
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
				TWRank();
				console.printfile(fname(Settings.TopTenFile));
				console.crlf();
				break;
			case '5':
				/* 33960 */
				SendRadioMessage();
				break;
			default:
				console.crlf();
				console.writeln("Invalid command.  Enter ? for help");
		}
	}
}
