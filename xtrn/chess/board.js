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
	this.draw_counter_turns;
	this.position_count={};
	this.moves=[];
	if(moves) {
		for(move in moves) {
			if(!this.handleMove(moves[move]))
				throw("Illegal set of moves, game corrupy");
		}
	}
}
Board.prototype.drawn=false;
Board.prototype.resigned=false;
Board.prototype.handleMove=function(move)
{
	m=move.match(/^([a-h][1-8])([a-h][1-8])(.*)$/);
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
				case 'n':
					piece.promote_to=Knight;
					break;
			}
		}
		if(!piece.moveTo(parsePos(m[2])))
			return false;
	}
	if(move=='R')
		this.resigned=true;
	else {
		if(move.search(/'D$/) != -1) {
			if(this.draw_offer && this.draw_offer != this.turn())
				this.drawn=true;
			else if(m==null)
				return false;
			this.draw_offer = this.turn();
		}
		else {
			if(m==null)
				return false;
			delete this.draw_offer;
		}
	}
	this.moves.push(move);
	return true;
}
Board.prototype.checkForMove=function(t)
{
	var x,y,p;
	var x2,y2;

	/*
	 * This tests for stalemate and checkmate.  Checkmate is simply
	 * a stalemate where the king is in check
	 */
	for(y=0;y<=8;y++) {
		for(x=0;x<=8;x++) {
			p=this.getPiece({x:x,y:y});
			if(p==null)
				continue;
			if(p.colour != t)
				continue;
			for(y2=1;y2<=8;y2++) {
				for(x2=1;x2<=8;x2++) {
					if(p.moveTo({x:x2,y:y2},false))
						return true;
				}
			}
		}
	}
	return false;
}
Board.prototype.newTurn=function()
{
	var t=this.turn();
	var s,smaterial,i,count,piece,material_array,x,y,c,c2;

	if(this.resigned)
		return 'RESIGNED';

	if(this.drawn)
		return 'DRAW';

	/* Check for check and check mate */
	if(!this.checkForMove(t)) {
		if(this.check(t))
			return 'CHECKMATE';
		return 'STALEMATE';
	}

	/* Check for threefold repitition */
	s=this.stateString();
	if(this.position_count[s]==undefined)
		this.position_count[s]=1;
	else
		this.position_count[s]++;

	/* Check for 50 turn limit */
	this.draw_counter_turns++;
	if(this.draw_counter_turns >= 100) {
		if(this.draw_offer && this.draw_offer != t)
			return 'DRAW';
		else
			this.draw_offer = t;
	}
	if(this.position_count[s]>=3) {
		if(this.draw_offer && this.draw_offer != t)
			return 'DRAW';
		else
			this.draw_offer = t;
	}

	/* Check for insufficient material */
	s=s.substr(1);
	smaterial=s.replace(/[WB\*]/,'');
	smaterial=smaterial.split(/\b|\B/).sort().join('');
	if(smaterial=='kk' || smaterial=='bkk' || smaterial=='kkn') {
		if(this.draw_offer && this.draw_offer != t)
			return 'DRAW';
		else
			this.draw_offer = t;
	}
	/*
	 * In the case of kb vs kb (or kbbbb vs kb), all bishops must
	 * be on the same colour, and both players must have at least
	 * one bishop.
	 */
	count={};
	do {
		if(smaterial.search(/^b{2,}kk$/)!=-1) {
			material_array=s.match(/[WB]b/);
			for(i in material_array) {
				if(count[material_array[i]]==undefined)
					count[material_array[i]]=1;
				else 
					count[material_array[i]]++;
			}
			if(count['Wb']==undefined || count['Bb']==undefined)
				break;
			for(y=1;y<=8;y++) {
				for(x=1;x<=8;x++) {
					piece=this.getPiece({x:x,y:y});
					if(piece.constructor.name=='Bishop') {
						c2=x%2;
						if(y%2)
							c2^=1;
						if(c==undefined)
							c=c2;
						else
							if(c2 != c)
								break;
					}
				}
			}
			if(c2==c) {
				if(this.draw_offer && this.draw_offer != t)
					return 'DRAW';
				else
					this.draw_offer = t;
			}
		}
	} while(0);

	return '';
}
Board.prototype.pieces=null;
Board.prototype._domove=function(from, to)
{
	var piece=this.getPiece(from);
	if(piece==null)
		return false;
	if(piece.constructor.name=='Pawn' || this.pieces[to.y-1][to.x-1])
		this.draw_counter_turns=-1;
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
Board.prototype.turn=function()
{
	if(this.moves.length % 2)
		return COLOUR.black;
	return COLOUR.white;
}
Board.prototype.lastTurn=function()
{
	if(this.moves.length % 2)
		return COLOUR.white;
	return COLOUR.black;
}
// TODO: There's a notation (FEN?) for this.
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
						ret += 'n';
						break;
				}
			}
		}
	}
	return ret;
}
