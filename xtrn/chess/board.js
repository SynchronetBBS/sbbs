function Board()
{
	this.pieces=[
		[
			new Rook('w', 'a1', this),
			new Knight('w', 'b1', this),
			new Bishop('w', 'c1', this),
			new Queen('w', 'd1', this),
			new King('w', 'e1', this),
			new Bishop('w', 'f1', this),
			new Knight('w', 'g1', this),
			new Rook('w', 'h1', this),
		],
		[
			new Pawn('w', 'a2', this),
			new Pawn('w', 'b2', this),
			new Pawn('w', 'c2', this),
			new Pawn('w', 'd2', this),
			new Pawn('w', 'e2', this),
			new Pawn('w', 'f2', this),
			new Pawn('w', 'g2', this),
			new Pawn('w', 'h2', this),
		],
		new Array(8),
		new Array(8),
		new Array(8),
		new Array(8),
		[
			new Pawn('b', 'a7', this),
			new Pawn('b', 'b7', this),
			new Pawn('b', 'c7', this),
			new Pawn('b', 'd7', this),
			new Pawn('b', 'e7', this),
			new Pawn('b', 'f7', this),
			new Pawn('b', 'g7', this),
			new Pawn('b', 'h7', this),
		],
		[
			new Rook('b', 'a8', this),
			new Knight('b', 'b8', this),
			new Bishop('b', 'c8', this),
			new Queen('b', 'd8', this),
			new King('b', 'e8', this),
			new Bishop('b', 'f8', this),
			new Knight('b', 'g8', this),
			new Rook('b', 'h8', this),
		],
	];
	this.king = {}
	this.king[COLOUR.white] = this.pieces[0][4];
	this.king[COLOUR.black] = this.pieces[7][4];
}
Board.prototype.movenum={value: 0};
Board.prototype.pieces=null;
Board.prototype._domove=function(from, to)
{
	var piece=this.getPiece(from);
	if(piece==null)
		return false;
	this.pieces[to.y-1][to.x-1]=piece;
	this.pieces[piece.y-1][piece.x-1]=null;
	piece.x=to.x;
	piece.y=to.y;
	return true;
}
Board.prototype.getPiece=function(pos)
{
	return this.pieces[pos.y-1][pos.x-1];
}
Board.prototype.check=function(colour)
{
	var x,y,piece,kp;

	kp=this.king[colour].position;
	for(y=0;y<this.pieces.length;y++) {
		for(x=0;x<this.pieces[y].length;x++) {
			piece=this.getPiece({x:x, y:y});
			if(piece != null) {
				if(piece.moveTo(kp, false))
					return true;
			}
		}
	}
	return false;
}
