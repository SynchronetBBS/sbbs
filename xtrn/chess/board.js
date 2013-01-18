function Board(moves)
{
	var m,piece;

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
	this.moves=[];
	if(moves) {
		for(move in moves) {
			if(!this.handleMove(moves[move]))
				throw("Illegal set of moves, game corrupy");
		}
	}
}
Board.prototype.handleMove=function(move)
{
	// TODO: Promotions
	m=move.match(/^([a-h][1-8])([a-h][1-8])(.*)$/);
	if(m==null)
		return false;
	if(m!=null) {
		piece=this.getPiece(parsePos(m[1]));
		if(piece.constructor.name=='Pawn') {
			switch(m[3]) {
				case 'q':
					piece.promote_to=Queen;
					break;
				case 'r':
					piece.promote_to=Rook;
					break;
				case 'b':
					piece.promote_to=Bishop;
					break;
				case 'k':
					piece.promote_to=Knight;
					break;
			}
		}
		if(!piece.moveTo(parsePos(m[2])))
			return false;
	}
	this.moves.push(move);
	return true;
}
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
	var x,y,piece;

	for(y=0;y<this.pieces.length;y++) {
		for(x=0;x<this.pieces[y].length;x++) {
			piece=this.getPiece({x:x+1, y:y+1});
			if(piece != null) {
				if(piece.moveTo(this.king[colour], false))
					return true;
			}
		}
	}
	return false;
}
Board.prototype.reason=function(str)
{
}
Board.prototype.stateString= function()
{
	var ret='';
	var x,y,p,p2,cx;

	ret += this.moves.length%2;
	for(y=1; y<=8; y++) {
		for(x=1; x<=8; x++) {
			p=this.getPiece({x:x,y:y});
			if(p==null)
				ret += '*';
			else {
				if(p.colour==COLOUR.white)
					ret += 'W';
				else
					ret += 'B';
				switch(p.constructor.name) {
					case 'Pawn':
						/*
						 * If this pawn can be captured en passant,
						 * capitalize it
						 */
						if(p.double_move_num==this.moves.length-1) {
							p2=this.getPiece({x:x-1,y:y});
							if(p2 != null && p2.constructor.name=='Pawn' && p2.colour != p.colour) {
								ret += 'P';
								break;
							}
							p2=this.getPiece({x:x+1,y:y});
							if(p2 != null && p2.constructor.name=='Pawn' && p2.colour != p.colour) {
								ret += 'P';
								break;
							}
						}
						ret += 'p';
						break;
					case 'Bishop':
						ret += 'b';
						break;
					case 'Queen':
						ret += 'q';
						break;
					case 'King':
						ret += 'k';
						break;
					case 'Rook':
						/*
						 *  If the king can castle with this rook,
						 * use a capital
						 */
						p2=this.king[p.colour];
						if((!p.moved) && (!p2.moved)) {
							if(p.x < p2.x)
								cx=p2.x-1;
							else
								cx=p2.x+1;
							if(p.emptyTo({x:cx, y:p.y})) {
								if(this.getPiece({x:cx, y:p.y})==null) {
									ret + 'R';
									break;
								}
							}
						}
						ret += 'r';
						break;
					case 'Knight':
						ret += 'k';
						break;
				}
			}
		}
	}
	return ret;
}
