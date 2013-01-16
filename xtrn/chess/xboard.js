#!/synchronet/src/src/sbbs3/gcc.freebsd.exe.stephen.hurd.local/jsexec
load("utils.js");
load("pieces.js");
load("board.js");

var b=new Board();
var cmd;
var m;
var piece;
var reason;
var f=new File("/tmp/chesslog.txt");
f.open("w");

while(1) {
	cmd=readln();
	f.writeln("CMD: "+cmd);
	f.flush();

	switch(cmd) {
		default:
			if((m=cmd.match(/^([a-h][1-8])([a-h][1-8])([qkbr]?)$/))!=null) {
				if(b.handleMove(cmd)) {
					writeln('move '+cmd);
					f.writeln('move '+cmd);
				}
				else {
					writeln('Illegal move: '+cmd);
					f.writeln('Illegal move: '+cmd);
				}
			}
			else if((m=cmd.match(/^ping ([0-9])$/))!=null) {
				writeln("pong "+m[1]);
				f.writeln("pong "+m[1]);
			}
			break;
		case 'protover 2':
			writeln('feature done=0');
			writeln('feature ping=1');
			writeln('feature time=0');
			writeln('feature draw=0');
			writeln('feature analyze=0');
			writeln('feature myname="JS Engine"');
			writeln('feature variants="normal"');
			writeln('feature colors=0');
			writeln('feature name=0');
			writeln('feature nps=0');
			writeln('feature done=1');
			f.writeln('feature done=0');
			f.writeln('feature ping=1');
			f.writeln('feature time=0');
			f.writeln('feature draw=0');
			f.writeln('feature analyze=0');
			f.writeln('feature myname="JS Engine"');
			f.writeln('feature variants="normal"');
			f.writeln('feature colors=0');
			f.writeln('feature name=0');
			f.writeln('feature nps=0');
			f.writeln('feature done=1');
			break;
		case 'new':
			b=new Board();
			break;
		case 'quit':
			exit(0);
	}
	f.flush();
}
