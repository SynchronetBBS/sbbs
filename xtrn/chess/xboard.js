#!/synchronet/src/src/sbbs3/gcc.freebsd.exe.stephen.hurd.local/jsexec
load("utils.js");
load("pieces.js");
load("board.js");

var b;
var cmd;
var m;
var piece;
var f=new File("/tmp/chesslog.txt");
var reason;
f.open("a");

while(1) {
	cmd=readln();

f.writeln("CMD: "+cmd);
f.flush();
	switch(cmd) {
		default:
			if((m=cmd.match(/^([a-h][1-8])([a-h][1-8])$/))!=null) {
				piece=b.getPiece(parsePos(m[1]));
				if(piece) {
					if(piece.moveTo(m[2])) {
						print('move '+m[1]+m[2]);
						f.writeln("Moved "+m[1]+m[2]);
						f.flush();
						break;
					}
					else {
						print('Illegal move (no worky): '+m[1]+m[2]);
						f.writeln('Illegal move (no worky): '+m[1]+m[2]);
						f.flush();
					}
				}
				else {
					print('Illegal move (no such piece): '+m[1]+m[2]);
					f.writeln('Illegal move (no such piece): '+m[1]+m[2]);
					f.flush();
				}
			}
			break;
		case 'new':
			b=new Board();
			break;
		case 'quit':
			exit(0);
	}
}
