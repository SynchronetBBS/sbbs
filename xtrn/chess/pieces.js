function Piece(colour, pos, board)
{
	this.black=parseColour(colour);
	var pos=parsePos(pos);
	this.x=pos.x;
	this.y=pos.y;
	this.board=board;
}
Piece.prototype={
	black: null,
	x: null,
	y: null,
	board: null,
	moveTo: function() {
		var tgtpos=parsePos(pos);
		return this.board._domove(this, tgtpos);
	}
}

function Pawn(colour, pos, board)
{
	Piece.call(this, colour, pos, board);
}
Pawn.prototype = Object.create(eval(Piece.prototype.toSource()), {
	double_move_num: { value: 0 },
	moveTo: function(pos)
	{
		var tgtpos=parsePos(pos);
		var ydist=Math.abs(this.y-tgtpos.y);
		var xdist=Math.abs(this.x-tgtpos.x);

		if(xdist > 1)
			return false;

		// must move forward
		if(this.black) {
			if(tgtpos.y >= this.pos)
				return false;
		}
		else
			if(tgtpos.y <= this.pos)
				return false;

		// Only one space unless at start
		if(ydist > 2)
			return false;
		if(ydist==2) {
			if(this.x!=black?7:2)
				return false;
		}

		var capture=this.board.getpiece(tgtpos);
		if(capture==null) {
			// Check for en passant
			if(xdist==1) {
				var passed=this.board.getpiece({x:this.x,y:this.y-(black?-1:1)});
	
				if(pass == null)
					return false;
				if(tgtpos.x != this.black?3:6)
					return false;
				if(typeOf(passed)!= 'Pawn')
					return false;
				if(passed.double_move_num != this.board.movenum-1)
					return false;
			}
		}
		else
			if(xdist==0)
				return false;

		Piece.prototype.moveTo.apply(this, pos);
	}
})
