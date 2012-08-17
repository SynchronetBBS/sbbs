// sprite.js for Synchronet 3.16+
// echicken -at- bbs.electronicchicken.com, 2012

/*	Usage:
	
	Create a "sprites" subdirectory in the directory where your script resides.
	For each sprite, you must create two files in the "sprites" directory:
	
	<sprite>.ini	- Settings and additional information about this sprite.
	<sprite>.bin	- ANSI art saved in .bin format (PabloDraw recommended)
	
	<sprite>.ini format:
	
		width		The width of the sprite (REQUIRED)

		height		The height of the sprite (ie. the height of one
					graphic  in <sprite>.bin) (REQUIRED)

		bearings	A comma separated list of bearings available to this
					sprite (eg. n,ne,e,se,s,sw,w,nw) (REQUIRED)

		movement	"rotating" or "directional"
					If "rotating", the sprite will turn clockwise or
					counterclockwise when the left and right arrow keys
					are pressed.  If "directional" the sprite will
					turn to face the direction of the arrow key that
					was pressed.
					
		constantmotion
					1 or 0, whether or not this sprite is constantly
					moving or only moves when a key is pressed.
					(REQUIRED)

		speed		Time (in seconds, fractional seconds (eg. .5) are
					allowed) between sprite movements.  The lower the
					number, the faster the sprite can move. (REQUIRED)

		speedstep	Applicable only if constantmotion = 1, is the amount
					by which 'speed' is increased or decreased when the
					up and down cursor keys are pressed.  (Recommended
					if constantmotion = 1)

		gravity		1 or 0, 0 is default, should really only be used if
					bearings = e,w.  Sprites will fall toward the bottom
					of the screen until an obstacle is encountered if
					this is enabled.

		jumpheight	How many character cells high should the sprite be
					able to jump?  Should be used in conjunction with
					'gravity' and with 'bearings = e,w' (eg. a platform-
					type game.)  (Recommended if gravity = 1)

		weapon		The name of another <sprite>.ini/<sprite>.bin pair,
					which will be produced when KEY_FIRE is pressed.

		range		Applicable only to weapon sprites, how far the sprite
					can travel before disappearing.
		
		You can add any number of other key=value pairs to a <sprite>.ini file
		for your own use.  They are accessible as properties of your sprite
		object via Sprite.ini.<keyname>.
	
	<sprite>.bin format:
	
		Each sprite can be comprised of up to nine different graphics.  You're
		free to give your sprite any width and height that you wish.  Draw the
		first graphic at the top of the file, and then immediately beneath it
		draw the next one, and so on.  Each "graphic" should be as wide and as
		tall as the 'width' and 'height' values defined in <sprite>.ini.
		
		The order of the graphics in <sprite>.bin are as follows, from top to
		bottom:
		
		N		- Shown when the sprite is facing north
		NE		- Shown when the sprite is facing northeast
		E		- Shown when the sprite is facing east
		SE		- Shown when the sprite is facing southeast
		S		- Shown when the sprite is facing south
		SW		- Shown when the sprite is facing southwest
		W		- Shown when the sprite is facing west
		Spare	- Access with Sprite.frame.scrollTo(1, Sprite.ini.height * 8)
		
		You may omit graphics for any bearings that your sprite will not be
		using, but remember to leave blank spaces where these graphics would
		otherwise have been. (Eg. If you only want E and W graphics, and your
		sprite is four characters tall, then draw your E graphic on lines 9
		through 12 and your W graphic on lines 25 through 28 of <sprite>.bin
		leaving the rest of the lines blank.)
		
	The 'sprites' array:
	
		After you load this file, an array called 'sprites' is created.  Each
		new Sprite object that you create is pushed onto this array.  You'll
		likely want to iterate over this array with a for loop once for each
		loop of your script in order to pass input to your player's sprite
		and move any other sprites around on the screen.
		
	Creating a Sprite object:
	
		var s = new Sprite(spriteName, parentFrame, x, y, bearing);
			
		Where 'spriteName' is the name of a pair of files in the 'sprites'
		subdirectory.  (Eg. if you have ./sprites/dwer.ini and
		./sprites/dwer.bin, supply "dwer" for 'spriteName'.)
		
		'parentFrame' is a reference to a Frame object of which this sprite
		will be a child.  You must already have created this frame, and
		your script will need to cycle the parent frame in order for any
		changes to your sprites to be seen.
		
		'x' and 'y' are the coordinates of the top left-hand (northwest)
		corner of this sprite, eg. 1, 1 for the top left corner of the
		screen.
		
		'bearing' is the direction which this sprite will be facing when
		it is created.

	Methods:
	
		Sprite.getCmd(cmd)	Process and acts upon user input (eg. a value
							returned by console.inkey()).  Calls 'turn'
							'move' or 'putWeapon' as required.
								  
		Sprite.cycle()		Various housekeeping tasks.  Must be called
							in order to apply any changes to Sprite's
							position or orientation.
							  
		Sprite.move(direction)
							Where 'direction' is either "forward" or
							"reverse", will cause the sprite to move in
							one space in or away from the direction
							it is currently facing (ie. its bearing.)

		Sprite.moveTo(x, y)	Move the sprite to an absolute position within
							its parent frame.
		
		Sprite.turn(direction)
							Where 'direction' is either 'cw' or 'ccw'
							(clockwise or counter-clockwise,) will
							cause the sprite to change to the next
							available bearing in that direction and
							load the applicable graphic.
							
		Sprite.turnTo(bearing)
							Rotate the sprite to an absolute bearing.
								  
		Sprite.putWeapon()	Finds the center of the sprite's forward
							facing side and produces a weapon at that
							point. (The weapon will be another sprite
							which must exist in the 'sprites' sub-
							directory, and should have constantmotion
							set to 1 in its own ini file, and have a
							range defined.)
								  
		Sprite.checkOverlap(margin)
							Returns an array of all other sprites that
							overlap on the screen with this one, or
							false if there is no overlap. (It's up to
							you to decide what to do in case of overlap,
							eg. let the player collect a prize, or make
							it explode if it stepped on a landmine.)
		
		Sprite.checkBelow()
		Sprite.checkAbove()	These are mostly for internal use.  They
							return true if there is something directly
							above or below the sprite.  (This is what
							stops a jump or a fall.)
								  
		Sprite.pursue(sprite)
							Causes Sprite to turn clockwise, once, toward
							'sprite'.  Returns true if Sprite is facing
							toward 'sprite'. (If Sprite.move() is then
							called, Sprite will start moving toward
							'sprite'.)  Useful for causing enemies to
							chase after the player, etc.

		Sprite.remove()		Removes the sprite from the screen and sets
							the value of its 'open' property to false.
		
	Properties:
	
		Sprite.x								  
		Sprite.y			The current x and y coordinates of the top
							left corner of this sprite. (Normally the
							same as Sprite.frame.x and Sprite.frame.y)
										  
		Sprite.origin.x
		Sprite.origin.y		The x and y coordinates where this sprite
							first appeared on the screen.
								  
		Sprite.bearing		The direction that the sprite is facing, one
							of n,ne,e,se,s,sw, or w. (Top of screen is
							north, right side of screen is east, and so
							on.)
								  
		Sprite.lastMove		The time when the sprite last moved (as read
							from system.timer
								  
		Sprite.lastAttack	The time when the sprite last used its weapon
							(as read from system.timer)
								  
		Sprite.inJump		True if the sprite is jumping
		
		Sprite.inFall		True if the sprite is falling
		
		Sprite.jumpStart	The time when a jump began (as read from
							system.timer)
								  
		Sprite.weaponCoordinates
							Where the sprite's weapon should appear on
							the screen (calculated by Sprite.putWeapon())
								  
		Sprite.ini			Has sub-properties for every value defined in
							this sprite's ini file.
								  
		Sprite.frame		A Frame object; how this Sprite is actually
							displayed on the screen.
								  
		Sprite.offsets		An array of offsets to the first lines of
							each of the n,ne,e,se,s,sw,w,spare graphics
							in the <sprite>.bin file.
								  
		Sprite.open			true or false, whether or not this sprite is
							currently displayed on the screen.

*/

load("sbbsdefs.js");
load("frame.js");

var sprites = [];
var KEY_WEAPON = "F"; // Redefine this in your scripts as desired
var KEY_JUMP = " "; // Redefine this in your scripts as desired

function Sprite(spriteName, parentFrame, x, y, bearing) {

	this.x = x;
	this.y = y;
	this.origin = { x : x, y : y };
	this.bearing = bearing;
	this.lastBearing = bearing;
	this.lastMove = system.timer;
	this.lastAttack = system.timer;
	this.open = true;
	this.inJump = false;
	this.inFall = false;
	this.jumpStart = 0;
	this.weaponCoordinates = { x : 0, y : 0 };

	if(!file_exists(js.exec_dir + "sprites/" + spriteName + ".ini"))
		return false;
	var f = new File(js.exec_dir + "sprites/" + spriteName + ".ini");
	f.open("r");
	this.ini = f.iniGetObject();
	f.close();
	this.ini.bearings = this.ini.bearings.split(",");
	this.ini.speed = parseFloat(this.ini.speed);
	this.ini.speedstep = parseFloat(this.ini.speedstep);
	this.ini.speedmax = parseFloat(this.ini.maximumspeed);
	this.ini.speedmin = parseFloat(this.ini.minimumspeed);
	if(!this.ini.hasOwnProperty("movement"))
		this.ini.movement = "rotating";
	if(this.ini.hasOwnProperty("constantmotion"))
		this.ini.constantmotion = parseInt(this.ini.constantmotion);
	else
		this.ini.constantmotion = 0;
	if(this.ini.hasOwnProperty("gravity"))
		this.ini.gravity = parseInt(this.ini.gravity);
	else
		this.ini.gravity = 0;
	if(this.ini.hasOwnProperty("jumpheight"))
		this.ini.jumpheight = parseInt(this.ini.jumpheight);
	else
		this.ini.jumpheight = 0;
	if(this.ini.hasOwnProperty("weapon"))
		this.ini.weapon = this.ini.weapon;
	else
		this.ini.weapon = false;
	if(this.ini.hasOwnProperty("range"))
		this.ini.range = parseInt(this.ini.range);
	else
		this.ini.range = 0;
		
	if(this.ini.weapon) {
		var f = new File(js.exec_dir + "sprites/" + this.ini.weapon + ".ini");
		f.open("r");
		var wi = f.iniGetObject();
		f.close();
		this.ini.weaponWidth = wi.width;
		this.ini.weaponHeight = wi.height;
	}

	this.frame = new Frame(x, y, this.ini.width, this.ini.height, 0, parentFrame);
	this.frame.checkbounds = false;
	this.frame.load(js.exec_dir + "sprites/" + spriteName + ".bin", 80, (this.ini.height * 9));
	this.frame.top();
	this.offsets = {
		"n" : 0,
		"ne" : this.ini.height,
		"e" : this.ini.height * 2,
		"se" : this.ini.height * 3,
		"s" : this.ini.height * 4,
		"sw" : this.ini.height * 5,
		"w" : this.ini.height * 6,
		"nw" : this.ini.height * 7
	}
	this.frame.scrollTo(0, this.offsets[this.bearing]);

	sprites.push(this);

	this.move = function(direction) {
		if(
			(this.bearing == "ne" || this.bearing == "se" || this.bearing == "sw" || this.bearing == "nw")
			&&
			(system.timer - this.lastMove < this.ini.speed * 2)
		)
			return;
		this.lastMove = system.timer;
		switch(this.bearing) {
			case "n":
				if(direction == "forward") {
					this.frame.move(0, -1);
					this.y = this.y - 1;
				} else {
					this.frame.move(0, 1);
					this.y++;
				}
				break;
			case "ne":
				if(direction == "forward") {
					this.frame.move(1, -1);
					this.x++;
					this.y = this.y - 1;
				} else {
					this.frame.move(-1, 1);
					this.x = this.x - 1;
					this.y++;
				}
				break;
			case "e":
				if(direction == "forward") {
					this.frame.move(1, 0);
					this.x++;
				} else {
					this.frame.move(-1, 0);
					this.x = this.x - 1;
				}
				break;
			case "se":
				if(direction == "forward") {
					this.frame.move(1, 1);
					this.x++;
					this.y++;
				} else {
					this.frame.move(-1, -1);
					this.x = this.x - 1;
					this.y = this.y - 1;
				}
				break;
			case "s":
				if(direction == "forward") {
					this.frame.move(0, 1);
					this.y++;
				} else {
					this.frame.move(0, -1);
					this.y = this.y - 1;
				}
				break;
			case "sw":
				if(direction == "forward") {
					this.frame.move(-1, 1);
					this.x = this.x - 1;
					this.y++;
				} else {
					this.frame.move(1, -1);
					this.x++;
					this.y = this.y - 1;
				}
				break;
			case "w":
				if(direction == "forward") {
					this.frame.move(-1, 0);
					this.x = this.x - 1;
				} else {
					this.frame.move(1, 0);
					this.x++;
				}
				break;
			case "nw":
				if(direction == "forward") {
					this.frame.move(-1, -1);
					this.x = this.x - 1;
					this.y = this.y - 1;
				} else {
					this.frame.move(1, 1);
					this.x++;
					this.y++;
				}
				break;
			default:
				break;
		}
	}

	this.turn = function(direction) {
		switch(this.bearing) {
			case "n":
				if(direction == "ccw") {
					this.bearing = "nw";
				} else {
					this.bearing = "ne";
				}
				break;
			case "ne":
				if(direction == "ccw") {
					this.bearing = "n";
				} else {
					this.bearing = "e";
				}
				break;
			case "e":
				if(direction == "ccw") {
					this.bearing = "ne";
				} else {
					this.bearing = "se";
				}
				break;
			case "se":
				if(direction == "ccw") {
					this.bearing = "e";
				} else {
					this.bearing = "s";
				}
				break;
			case "s":
				if(direction == "ccw") {
					this.bearing = "se";
				} else {
					this.bearing = "sw";
				}
				break;
			case "sw":
				if(direction == "ccw") {
					this.bearing = "s";
				} else {
					this.bearing = "w";
				}
				break;
			case "w":
				if(direction == "ccw") {
					this.bearing = "sw";
				} else {
					this.bearing = "nw";
				}
				break;
			case "nw":
				if(direction == "ccw") {
					this.bearing = "w";
				} else {
					this.bearing = "n";
				}
				break;
			default:
				break;
		}
	}
	
	this.putWeapon = function() {
		if(!this.ini.weapon || system.timer - this.lastAttack < this.ini.attackspeed)
			return false;
		this.lastAttack = system.timer;
		switch(this.bearing) {
			case "n":
				this.weaponCoordinates = {
					x : parseInt(this.x + (this.frame.width / 2)),
					y : this.y - this.ini.weaponHeight
				}
				break;
			case "ne":
				this.weaponCoordinates = {
					x : this.x + this.frame.width,
					y : this.y - this.ini.weaponHeight
				}			
				break;
			case "e":
				this.weaponCoordinates = {
					x : this.x + this.frame.width,
					y : parseInt(this.y + (this.frame.height / 2))
				}
				break;
			case "se":
				this.weaponCoordinates = {
					x : this.x + this.frame.width,
					y : this.y + this.frame.height
				}
				break;
			case "s":
				this.weaponCoordinates = {
					x : parseInt(this.x + (this.frame.width / 2)),
					y : this.y + this.frame.height
				}
				break;
			case "sw":
				this.weaponCoordinates = {
					x : this.x - this.ini.weaponWidth,
					y : this.y + this.frame.height
				}
				break;
			case "w":
				this.weaponCoordinates = {
					x : this.x - this.ini.weaponWidth,
					y : parseInt(this.y + (this.frame.height / 2))
				}
				break;
			case "nw":
				this.weaponCoordinates = {
					x : this.x - this.ini.weaponWidth,
					y : this.y - this.ini.weaponHeight
				}
				break;
			default:
				break;
		}
		if(this.weaponCoordinates.x < 1 || this.weaponCoordinates.x > 80 || this.weaponCoordinates.y < 1 || this.weaponCoordinates.y > 24)
			return false;
		var w = new Sprite(this.ini.weapon, this.frame.parent, this.weaponCoordinates.x, this.weaponCoordinates.y, this.bearing);
		w.owner = this;
		w.frame.draw(); // Shouldn't be necessary, but sprite doesn't appear unless I do this
	}
	
	this.checkOverlap = function(margin) {
		if(margin === undefined)
			margin = 0;
		var yarg = [];
		for(var s = 0; s < sprites.length; s++) {
			if(sprites[s] == this)
				continue;
			if(this.x >= sprites[s].x + sprites[s].frame.width + margin || this.x + this.frame.width + margin <= sprites[s].x)
				continue;
			if(this.y >= sprites[s].y + sprites[s].frame.height + margin || this.y + this.frame.height + margin <= sprites[s].y)
				continue;
			yarg.push(sprites[s]);
		}
		if(yarg.length < 1)
			return false;
		else
			return yarg;
	}

	this.checkBelow = function() {
		for(var s = 0; s < sprites.length; s++) {
			if(this.y + this.frame.height != sprites[s].y)
				continue;
			if(this.x >= sprites[s].x + sprites[s].frame.width || this.x + this.frame.width <= sprites[s].x)
				continue;
			return sprites[s];
		}
		return false;
	}
	
	this.checkAbove = function() {
		for(var s = 0; s < sprites.length; s++) {
			if(this.y <= sprites[s].y || this.y > sprites[s].y + sprites[s].frame.height)
				continue;
			if(this.x >= sprites[s].x + sprites[s].frame.width || this.x + this.frame.width <= sprites[s].x)
				continue;
			return sprites[s];
		}
		return false;
	}
	
	this.getCmd = function(userInput) {
		switch(userInput.toUpperCase()) {
			case KEY_LEFT:
				if(this.ini.movement == "directional" && this.ini.bearings.indexOf("w") >= 0) {
					if(this.bearing != "w")
						this.turnTo("w");
					else if(this.ini.constantmotion == 0)
						this.move("forward");
				} else if(this.ini.movement == "rotating") {
					this.turn("ccw");
					while(this.ini.bearings.indexOf(this.bearing) < 0) {
						this.turn("ccw");
					}
				}
				break;
			case KEY_RIGHT:
				if(this.ini.movement == "directional" && this.ini.bearings.indexOf("e") >= 0) {
					if(this.bearing != "e")
						this.turnTo("e");
					else if(this.ini.constantmotion == 0)
						this.move("forward");
				} else if(this.ini.movement == "rotating") {
					this.turn("cw");
					while(this.ini.bearings.indexOf(this.bearing) < 0) {
						this.turn("cw");
					}
				}
				break;
			case KEY_UP:
				if(this.ini.movement == "directional" && this.ini.bearings.indexOf("e") >= 0) {
					if(this.bearing != "n")
						this.turnTo("n");
					else if(this.ini.constantmotion < 1)
						this.move("forward");				
				} else if(this.ini.movement == "rotating" && this.ini.constantmotion == 0) {
					this.move("forward");
				} else {
					if(this.ini.speed > this.ini.speedmax)
						this.ini.speed = this.ini.speed - this.ini.speedstep;
					if(this.ini.speed <= this.ini.speedmax && this.ini.speed != 0)
						this.ini.speed = this.ini.speedmax;
					if(this.ini.speed == 0)
						this.ini.speed = this.ini.speedmin;
				}
				break;
			case KEY_DOWN:
				if(this.ini.movement == "directional" && this.ini.bearings.indexOf("s") >= 0) {
					if(this.bearing != "s")
						this.turnTo("s");
					else if(this.ini.constantmotion < 1)
						this.move("forward");
				} else if(this.ini.movement == "rotating" && this.ini.constantmotion == 0) {
					this.move("forward");
				} else {
					if(this.ini.speed == this.ini.speedmin)
						this.ini.speed = 0;
					if(this.ini.speed <= this.ini.speedmin && this.ini.speed != 0)
						this.ini.speed = this.ini.speed + this.ini.speedstep;
				}
				break;
			case KEY_JUMP:
				if(this.inJump || this.inFall)
					break;
				this.jumpStart = this.y;
				this.inJump = true;
				break;
			case KEY_WEAPON:
				this.putWeapon();
				break;
			default:
				break;
		}
	}
	
	this.cycle = function() {
		var ret = false;
		if(this.ini.constantmotion > 0 && this.ini.speed > 0 && system.timer - this.lastMove > this.ini.speed)
			this.move("forward");
		if(this.inJump && system.timer - this.lastMove > this.ini.speed) {
			if(this.checkAbove() || this.y == this.jumpStart - this.ini.jumpheight) {
				this.inJump = false;
				this.inFall = true;
			} else {
				this.y = this.y - 1;
				this.lastMove = system.timer;
			}
		}
		if(this.ini.gravity > 0 && !this.inJump && system.timer - this.lastMove > this.ini.speed) {
			if(!this.checkBelow()) {
				this.y = this.y + 1;
				this.lastMove = system.timer;
			} else {
				this.inFall = false;
			}
		}
		if(this.bearing != this.lastBearing) {
			ret = true;
			this.lastBearing = this.bearing;
			this.frame.scrollTo(0, this.offsets[this.bearing]);			
		}
		if(this.x != this.frame.x || this.y != this.frame.y) {
			ret = true;
			this.frame.moveTo(this.x, this.y);
		}
		if(
			this.ini.range > 0
			&&
			(
				this.x - this.origin.x >= this.ini.range
				||
				this.origin.x - this.x >= this.ini.range
				||
				this.y - this.origin.y >= this.ini.range / 2
				||
				this.origin.y - this.y >= this.ini.range / 2
			)
		) {
			this.frame.close();
			this.open = false;
		}
		return ret;
	}
	
	this.pursue = function(s) {
		var shoot = false;
		if(this.x >= s.x + s.frame.width && this.y >= s.y + s.frame.height && this.ini.bearings.indexOf("nw") >= 0) {
			if(this.bearing != "nw")
				this.turn("cw");
			else
				shoot = true;
		} else if(this.x + this.frame.width <= s.x && this.y >= s.y + s.frame.height && this.ini.bearings.indexOf("ne") >= 0) {
			if(this.bearing != "ne")
				this.turn("cw");
			else
				shoot = true;
		} else if(this.x >= s.x + s.frame.width && this.y + this.frame.height <= s.y && this.ini.bearings.indexOf("sw") >= 0) {
			if(this.bearing != "sw")
				this.turn("cw");
			else
				shoot = true;
		} else if(this.x + this.frame.width <= s.x && this.y + this.frame.height <= s.y && this.ini.bearings.indexOf("se") >= 0) {
			if(this.bearing != "se")
				this.turn("cw");
			else
				shoot = true;
		} else if((this.x >= s.x && this.x <= s.x + s.frame.width) && this.y >= s.y + s.frame.height) {
			if(this.bearing != "n")
				this.turn("cw");
			else
				shoot = true;
		} else if((this.x >= s.x && this.x <= s.x + s.frame.width) && this.y + this.frame.height <= s.y) {
			if(this.bearing != "s")
				this.turn("cw");
			else
				shoot = true;
		} else if((this.y >= s.y && this.y <= s.y + s.frame.height) && this.x >= s.x + s.frame.width) {
			if(this.bearing != "w")
				this.turn("cw");
			else
				shoot = true;
		} else if((this.y >= s.y && this.y <= s.y + s.frame.height) && this.x + this.frame.width <= s.x) {
			if(this.bearing != "e")
				this.turn("cw");
			else
				shoot = true;
		}
		return shoot;
	}

	this.remove = function() {
		this.open = false;
		this.frame.close();	
	}
	
	this.moveTo = function(x, y) {
		this.x = x;
		this.y = y;
		this.frame.moveTo(x, y);
	}
	
	this.turnTo = function(bearing) {
		if(this.ini.bearings.indexOf(bearing) < 0)
			return false;
		this.bearing = bearing;
		this.frame.scrollTo(0, this.offsets[this.bearing]);			
	}
}