function Piece(colour, pos, board)
{
	if(colour==null && pos==null && board==null)
		return;
	var pos=parsePos(pos);
	this.colour=parseColour(colour);
	this.x=pos.x;
	this.y=pos.y;
	this.board=board;
}
Piece.prototype={
	black: null,
	x: null,
	y: null,
	board: null,
	moveTo: function(pos) {
		var tgtpos=parsePos(pos);
		return this.board._domove(this, tgtpos);
	},
	emptyTo: function(target) {
		var x=this.x;
		var y=this.y;
		var piece;

		while(x != target.x || y!=target.y) {
			x = toward(x, target.x);
			y = toward(y, target.y);
			piece=this.board.getPiece({x:x, y:y});
			if(piece) {
				if(x==target.x && y==target.y) {
					if(piece.colour == this.colour)
						return false;
				}
				else
					return false;
			}
		}
		return true;
	},
	get position() {
		return makePos(this);
	}
}

function Pawn(colour, pos, board)
{
	Piece.call(this, colour, pos, board);
}
copyProps(Piece.prototype, Pawn.prototype);
Pawn.prototype.double_move_num={ value: 0 };
Pawn.prototype.moveTo=function(pos, update)
{
	if(update == null)
		update=true;
	var tgtpos=parsePos(pos);
	var ydist=Math.abs(this.y-tgtpos.y);
	var xdist=Math.abs(this.x-tgtpos.x);
	var capture;
	var passed;
	var ret;

	if(xdist > 1)
		return false;

	// must move forward
	if(tgtpos.y*this.colour >= this.pos*this.colour)
		return false;

	// Only one space unless at start
	if(ydist > 2)
		return false;
	if(ydist==2) {
		if(this.y!=4.5-(2.5*this.colour))
			return false;
		if(!this.emptyTo(tgtpos))
			return false;
	}

	capture=this.board.getPiece(tgtpos);
	if(capture==null) {
		// Check for en passant
		if(xdist==1) {
			passed=this.board.getPiece({x:this.x,y:this.y-this.colour});

			if(passed == null)
				return false;
			if(tgtpos.x != 4.5+(1.5*this.colour))
				return false;
			if(passed.constructor.Name!= 'Pawn')
				return false;
			if(passed.double_move_num != this.board.movenum-1)
				return false;
			if(passed.colour==this.colour)
				return false;
		}
	}
	else {
		if(xdist==0)
			return false;
		if(capture.colour==this.colour)
			return false;
	}

	if(update) {
		ret=Piece.prototype.moveTo.apply(this, [pos]);
		if(ret && ydist==2)
			this.double_move_num=this.board.movenum;
		return ret;
	}
}

function Rook(colour, pos, board)
{
	Piece.call(this, colour, pos, board);
}
copyProps(Piece.prototype, Rook.prototype);
Rook.prototype.moved = { value: false };
Rook.prototype.moveTo=function(pos, update)
{
	if(update == null)
		update=true;
	var tgtpos=parsePos(pos);
	var ydist=Math.abs(this.y-tgtpos.y);
	var xdist=Math.abs(this.x-tgtpos.x);
	var ret;

	if(ydist != 0 && xdist != 0)
		return false;
	if(!this.emptyTo(tgtpos))
		return false;

	if(update) {
		ret=Piece.prototype.moveTo.apply(this, [pos]);
		if(ret && ydist==2)
			this.moved=true;
		return ret;
	}
}

function Bishop(colour, pos, board)
{
	Piece.call(this, colour, pos, board);
}
copyProps(Piece.prototype, Bishop.prototype);
Bishop.prototype.moveTo=function(pos, update)
{
	if(update == null)
		update=true;
	var tgtpos=parsePos(pos);
	var ydist=Math.abs(this.y-tgtpos.y);
	var xdist=Math.abs(this.x-tgtpos.x);

	if(ydist != xdist)
		return false;
	if(!this.emptyTo(tgtpos))
		return false;

	if(update)
		return Piece.prototype.moveTo.apply(this, [pos]);
}

function Queen(colour, pos, board)
{
	Piece.call(this, colour, pos, board);
}
copyProps(Piece.prototype, Queen.prototype);
Queen.prototype.moveTo=function(pos, update)
{
	if(update == null)
		update=true;
	var tgtpos=parsePos(pos);
	var ydist=Math.abs(this.y-tgtpos.y);
	var xdist=Math.abs(this.x-tgtpos.x);

	if(ydist != xdist && xdist != 0 && ydist != 0)
		return false;
	if(!this.emptyTo(tgtpos))
		return false;

	if(update)
		return Piece.prototype.moveTo.apply(this, [pos]);
}

function Knight(colour, pos, board)
{
	Piece.call(this, colour, pos, board);
}
copyProps(Piece.prototype, Knight.prototype);
Knight.prototype.moveTo=function(pos, update)
{
	if(update == null)
		update=true;
	var tgtpos=parsePos(pos);
	var ydist=Math.abs(this.y-tgtpos.y);
	var xdist=Math.abs(this.x-tgtpos.x);
	var piece;

	if(!((ydist == 1 && xdist == 2) || (xdist==1 && ydist==2)))
		return false;
	piece=this.board.getPiece(tgtpos);
	if(piece != null && piece.colour == this.colour)
		return false;

	if(update)
		return Piece.prototype.moveTo.apply(this, [pos]);
}

function King(colour, pos, board)
{
	Piece.call(this, colour, pos, board);
}
copyProps(Piece.prototype, King.prototype);
King.prototype.moved = { value: false };
King.prototype.moveTo = function(pos, update)
{
	if(update == null)
		update=true;
	var tgtpos=parsePos(pos);
	var ydist=Math.abs(this.y-tgtpos.y);
	var xdist=Math.abs(this.x-tgtpos.x);
	var piece;
	var ret;
	var x,cx;

	// Check for castling
	if(!this.moved && xdist==2 && ydist==0) {
		// Check the rook
		x=tgtpos.x < this.x?1:8;
		cx=tgtpos.x < this.x?4:6; // Must be clear to here

		piece=this.board.getPiece({x:x, y:this.y});
		if(piece==null)
			return false;
		if(piece.constructor.Name != "Rook")
			return false;
		if(piece.colour != this.colour)
			return false;
		if(piece.moved)
			return false;
		if(!piece.emptyTo({x:cx, y:this.y}))
			return false;
		if(this.board.getPiece({x:cx, y:this.y}))
			return false;
		// TODO: Check check
	}
	else {
		if(ydist > 1 || xdist > 1)
			return false;
		piece=this.board.getPiece(tgtpos);
		if(piece != null && piece.colour == this.colour)
			return false;
	}

	if(update) {
		ret=Piece.prototype.moveTo.apply(this, [pos]);
		if(ret) {
			this.moved=true;
			if(cx != undefined) {
				ret = piece.moveTo({x:cx, y:this.y}, true);
				if(!ret)
					throw("Castling error!");
				// Hack
				this.board.movenum--;
			}
		}
		return ret;
	}
}

