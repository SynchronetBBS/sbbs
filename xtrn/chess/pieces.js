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
	moveTo: function(pos, update) {
		var tgtpiece=this.board.getPiece(pos);

		if(update == null)
			update=true;
		if(tgtpiece != null) {
			if(tgtpiece.colour==this.colour) {
				this.board.reason("Can't capture your own piece");
				return false;
			}
		}
		if(this.colour != this.board.turn()) {
			this.board.reason("Attempted move out of turn");
			return false;
		}
		if(update) {
			var brd=new Board(this.board.moves);

			if(!brd._domove(this, pos))
				return false;
			if(brd.check(this.colour)) {
				this.board.reason("In check");
				return false;
			}
			return this.board._domove(this, pos);
		}
		return true;
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
Pawn.prototype.double_move_num=0;
Pawn.prototype.promote_to=null;
Pawn.prototype.moveTo=function(pos, update)
{
	var ydist=Math.abs(this.y-pos.y);
	var xdist=Math.abs(this.x-pos.x);
	var capture;
	var passed;
	var ret;

	if(update==null)
		update=true;

	if(xdist > 1) {
		this.board.reason("X offset to great");
		return false;
	}

	// must move forward
	if(pos.y*this.colour >= this.pos*this.colour) {
		this.board.reason("Moving backward");
		return false;
	}

	// Only one space unless at start
	if(ydist > 2) {
		this.board.reason("Moving more than two spaces");
		return false;
	}

	if(ydist==2) {
		if(xdist != 0) {
			this.board.reason("Can't long attack");
			return false;
		}
		if(this.y!=4.5-(2.5*this.colour)) {
			this.board.reason("Moving two spaces");
			return false;
		}
		if(!this.emptyTo(pos)) {
			this.board.reason("Move blocked");
			return false;
		}
	}

	capture=this.board.getPiece(pos);
	if(capture==null) {
		// Check for en passant
		if(xdist==1) {
			passed=this.board.getPiece({x:this.x,y:this.y-this.colour});

			if(passed == null) {
				this.board.reason("No piece to capture");
				return false;
			}
			if(pos.x != 4.5+(1.5*this.colour)) {
				this.board.reason("En passant to wrong rank");
				return false;
			}
			if(passed.constructor.Name!= 'Pawn') {
				this.board.reason("En passant of non-pawn");
				return false;
			}
			if(passed.double_move_num != this.board.moves.length-1) {
				this.board.reason("En passant too late");
				return false;
			}
			if(passed.colour==this.colour) {
				this.board.reason("En passant of own colour");
				return false;
			}
		}
	}
	else {
		if(xdist==0) {
			this.board.reason("Capturing not a diagonal");
			return false;
		}
	}

	ret=Piece.prototype.moveTo.apply(this, [pos, update]);
	if(update && ret) {
		if(ydist==2)
			this.double_move_num=this.board.moves.length;
	}
	if(ret && this.y == 4.5+(3.5*this.colour)) {
		// Promotion!
		if(this.promote_to == null)
			throw("Illegal pawn promotion!");
		this.board.pieces[this.y-1][this.x-1]=new this.promote_to(COLOUR_STR[this.colour], this.position, this.board);
	}
	return ret;
}

function Rook(colour, pos, board)
{
	Piece.call(this, colour, pos, board);
}
copyProps(Piece.prototype, Rook.prototype);
Rook.prototype.moved = false;
Rook.prototype.moveTo=function(pos, update)
{
	if(update == null)
		update=true;
	var ydist=Math.abs(this.y-pos.y);
	var xdist=Math.abs(this.x-pos.x);
	var ret;

	if(ydist != 0 && xdist != 0) {
		this.board.reason("Not a straight line");
		return false;
	}
	if(!this.emptyTo(pos)) {
		this.board.reason("Move blocked");
		return false;
	}

	ret=Piece.prototype.moveTo.apply(this, [pos, update]);
	if(update && ret)
		this.moved=true;
	return ret;
}

function Bishop(colour, pos, board)
{
	Piece.call(this, colour, pos, board);
}
copyProps(Piece.prototype, Bishop.prototype);
Bishop.prototype.moveTo=function(pos, update)
{
	var ydist=Math.abs(this.y-pos.y);
	var xdist=Math.abs(this.x-pos.x);

	if(ydist != xdist) {
		this.board.reason("Not a diagonal");
		return false;
	}
	if(!this.emptyTo(pos)) {
		this.board.reason("Move blocked");
		return false;
	}

	return Piece.prototype.moveTo.apply(this, [pos, update]);
}

function Queen(colour, pos, board)
{
	Piece.call(this, colour, pos, board);
}
copyProps(Piece.prototype, Queen.prototype);
Queen.prototype.moveTo=function(pos, update)
{
	var ydist=Math.abs(this.y-pos.y);
	var xdist=Math.abs(this.x-pos.x);

	if(ydist != xdist && xdist != 0 && ydist != 0) {
		this.board.reason("Not horizontal nor diagonal");
		return false;
	}
	if(!this.emptyTo(pos)) {
		this.board.reason("Move blocked");
		return false;
	}

	return Piece.prototype.moveTo.apply(this, [pos, update]);
}

function Knight(colour, pos, board)
{
	Piece.call(this, colour, pos, board);
}
copyProps(Piece.prototype, Knight.prototype);
Knight.prototype.moveTo=function(pos, update)
{
	var ydist=Math.abs(this.y-pos.y);
	var xdist=Math.abs(this.x-pos.x);
	var piece;

	if(!((ydist == 1 && xdist == 2) || (xdist==1 && ydist==2))) {
		this.board.reason("Illegal move");
		return false;
	}

	return Piece.prototype.moveTo.apply(this, [pos, update]);
}

function King(colour, pos, board)
{
	Piece.call(this, colour, pos, board);
}
copyProps(Piece.prototype, King.prototype);
King.prototype.moved = false;
King.prototype.moveTo = function(pos, update)
{
	if(update == null)
		update=true;
	var ydist=Math.abs(this.y-pos.y);
	var xdist=Math.abs(this.x-pos.x);
	var piece;
	var ret;
	var x,cx;

	// Check for castling
	if(!this.moved && xdist==2 && ydist==0) {
		// Check the rook
		x=pos.x < this.x?1:8;
		cx=pos.x < this.x?4:6; // Must be clear to here

		piece=this.board.getPiece({x:x, y:this.y});
		if(piece==null) {
			this.board.reason("Rook not in place");
			return false;
		}
		if(piece.constructor.Name != "Rook") {
			this.board.reason("Corner doesn't have a rook");
			return false;
		}
		if(piece.colour != this.colour) {
			this.board.reason("Corner rook not yours");
			return false;
		}
		if(piece.moved) {
			this.board.reason("Corner rook has moved");
			return false;
		}
		if(!piece.emptyTo({x:cx, y:this.y})) {
			this.board.reason("The way is not empty");
			return false;
		}
		if(this.board.getPiece({x:cx, y:this.y})) {
			this.board.reason("Blocked");
			return false;
		}
		// TODO: Check check
	}
	else {
		if(ydist > 1 || xdist > 1) {
			this.board.reason("Moving more than one space");
			return false;
		}
	}

	ret=Piece.prototype.moveTo.apply(this, [pos, update]);
	if(ret) {
		this.moved=true;
		if(cx != undefined) {
			ret = this.board._domove(piece, {x:cx, y:this.y});
			if(!ret)
				throw("Castling error!");
		}
	}
	return ret;
}

