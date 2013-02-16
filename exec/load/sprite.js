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
					sprite (eg. n,ne,e,se,s,sw,w,nw). Bearings must
					be in the order you wish to cycle through
					them, if movement is "rotating" (REQUIRED)
					
		positions	A comma separated list of positions available to this
					sprite (e.g. crouch,stand,jump,kick,explode)

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
					this is enabled. (REQUIRED FOR PROFILE SPRITES ONLY)

		jumpheight	How many character cells high should the sprite be
					able to jump?  Should be used in conjunction with
					'gravity' and with 'bearings = e,w' (eg. a platform-
					type game.)  (Recommended if gravity = 1)
					(REQUIRED FOR PROFILE SPRITES ONLY)

		weapon		The name of another <sprite>.ini/<sprite>.bin pair,
					which will be produced when KEY_FIRE is pressed.

		range		Applicable only to weapon sprites, how far the sprite
					can travel before disappearing.
		
		You can add any number of other key=value pairs to a <sprite>.ini file
		for your own use.  They are accessible as properties of your sprite
		object via Sprite.ini.<keyname>.
	
	<sprite>.bin format:
	
		Each sprite can be comprised of many different graphics.  You're
		free to give your sprite any width and height that you wish.  
		Each "graphic" should be as wide and as	tall as the 'width' and 'height' 
		values defined in <sprite>.ini. To give a sprite different graphics
		depending on the sprite's bearing, draw additional graphics in a vertical
		sequence (with no spaces in between) in the same order as your 'bearings' 
		in <sprite>.ini. To give your sprite different positions (which you can 
		manipulate as you see fit), draw additional graphics in horizontal
		sequence (with no spaces in between) in the same order as your
		'positions' in <sprite>.ini, respectively, for each bearing.
		
		Example <sprite>.bin layout:
		
			STAND	JUMP	SIT
		N	[...]	[...]	[...]		
		NE	[...]	[...]	[...]			
		E	[...]	[...]	[...]		
		SE	[...]	[...]	[...]		
		S	[...]	[...]	[...]		
		SW	[...]	[...]	[...]		
		W	[...]	[...]	[...]		
		NW	[...]	[...]	[...]		
		
	The 'Sprite' namespace:
	
		After you load this file, a namespace object called 'Sprite' is created.  Each
		new Sprite.<type> that you create is pushed into an array corresponding to
		the type of sprite you created. You will need to add Sprite.cycle() to your 
		main loop (or any loop that updates the sprites) in order to process updates
		to the screen (this is in addition to calling frame.cycle(), as documented in
		frame.js).
		
	Creating a Sprite:
		
		Aerial sprites (top-down):
			var s = new Sprite.Aerial(fileName, parentFrame, x, y, bearing, position);
			
		Profile sprites (side-view):
			var s = new Sprite.Profile(fileName, parentFrame, x, y, bearing, position);
			
		Platform sprites (walls, floors):
			var s = new Sprite.Platform(parentFrame, x, y, width, height, ch, attr);
			
		Where 'fileName' is the name of a pair of files in the 'sprites'
		subdirectory. 'fileName' can either be an absolute path and root file name
		(without extension), or it can be simply the root filename of a sprite,
		if the files are located in ./sprites/
		
		If you have ./sprites/<sprite name>.ini and ./sprites/<sprite name>.bin, 
		supply "<sprite name>" for 'fileName'. If you have the files elsewhere, 
		you must supply the full path/file name (without extension). 
		
		'parentFrame' is a reference to a Frame object of which this sprite
		will be a child.  You must already have created this frame, and
		your script will need to cycle the parent frame in order for any
		changes to your sprites to be seen.
		
		'x' and 'y' are the coordinates of the top left-hand (northwest)
		corner of this sprite, eg. 1, 1 for the top left corner of the
		screen.
		
		'bearing' is the direction which this sprite will be facing when
		it is created.
		
		'position' is the variation of the sprite's appearance/posture when
		it is created.
		
		'width' & 'height' are the width and height of a platform sprite.
		
		'ch' is the character to fill in a platform sprite.
		
		'attr' is the color attribute to fill in a platform sprite.
		
	Sprite Object Methods (all sprites):
	
		sprite.getcmd(cmd)	Process and acts upon user input (eg. a value
							returned by console.inkey()).  Calls 'turn'
							'move' or 'putWeapon' as required.
								  
		sprite.cycle()		Various housekeeping tasks.  Must be called
							in order to apply any changes to sprite's
							position or orientation.
							  
		sprite.remove()		Removes the sprite from the screen and sets
							the value of its 'open' property to false.
	
		sprite.moveTo(x, y)	Move the sprite to an absolute position within
							its parent frame.

	Sprite Object Properties (All sprites):
	
		Sprite.x								  
		Sprite.y			The current x and y coordinates of the top
							left corner of this sprite. (Normally the
							same as Sprite.frame.x and Sprite.frame.y)
										  
		Sprite.origin.x
		Sprite.origin.y		The x and y coordinates where this sprite
							first appeared on the screen.
								  
		Sprite.frame		A Frame object; how this Sprite is actually
							displayed on the screen.
								  
		Sprite.open			true or false, whether or not this sprite is
							currently displayed on the screen.
	
	**	Additional properties and methods are listed above each sprite 
		definition below, as some properties and methods are not shared
		but every type. 
*/

load("sbbsdefs.js");
load("funclib.js");
load("frame.js");

var KEY_WEAPON = "F"; // Redefine this in your scripts as desired
var KEY_JUMP = " "; // Redefine this in your scripts as desired

/* Sprite Class Namespace

	Methods (static):
	
		Sprite.cycle() 		Iterates all sprites stored in the namespace
							allowing sprites to update display, movement
							and check for interference.
	
		Sprite.checkOverlap(sprite,margin)
							Returns an array of all other sprites that
							overlap on the screen with this one, or
							false if there is no overlap. (It's up to
							you to decide what to do in case of overlap,
							eg. let the player collect a prize, or make
							it explode if it stepped on a landmine.)
		
		Sprite.checkBelow(sprite)
		Sprite.checkAbove(sprite)	
							These are mostly for internal use.  They
							return true if there is something directly
							above or below the sprite.  (This is what
							stops a jump or a fall.)

*/
var Sprite = {
	/* library objects */
	aerials:[],
	profiles:[],
	platforms:[],
	
	/* library methods */
	checkOverlap:function(sprite,margin) {
		var yarg = [];
		if(margin === undefined)
			margin = 0;
		function checkOverlap(sprites,sprite,margin) {
			for(var s = 0; s < sprites.length; s++) {
				if(sprites[s] == sprite)
					continue;
				if(sprite.x >= sprites[s].x + sprites[s].frame.width + margin 
					|| sprite.x + sprite.frame.width + margin <= sprites[s].x)
					continue;
				if(sprite.y >= sprites[s].y + sprites[s].frame.height + margin 
					|| sprite.y + sprite.frame.height + margin <= sprites[s].y)
					continue;
				yarg.push(sprites[s]);
			}
		}
		checkOverlap(this.aerials,sprite,margin);
		checkOverlap(this.profiles,sprite,margin);
		checkOverlap(this.platforms,sprite,margin);
		if(yarg.length > 0)
			return yarg;
		return false;
	},
	checkBelow:function(sprite) {
		var yarg=[];
		function checkBelow(sprites,sprite) {
			for(var s = 0; s < sprites.length; s++) {
				if(sprite.y + sprite.frame.height != sprites[s].y)
					continue;
				if(sprite.x >= sprites[s].x + sprites[s].frame.width 
					|| sprite.x + sprite.frame.width <= sprites[s].x)
					continue;
				yarg.push(sprites[s]);
			}
		}
		checkBelow(this.aerials,sprite);
		checkBelow(this.profiles,sprite);
		checkBelow(this.platforms,sprite);
		if(yarg.length > 0)
			return yarg;
		return false;
	},
	checkAbove:function(sprite) {
		var yarg=[];
		function checkAbove(sprites,sprite) {
			for(var s = 0; s < sprites.length; s++) {
				if(sprite.y <= sprites[s].y 
					|| sprite.y > sprites[s].y + sprites[s].frame.height)
					continue;
				if(sprite.x >= sprites[s].x + sprites[s].frame.width 
					|| sprite.x + sprite.frame.width <= sprites[s].x)
					continue;
				yarg.push(sprites[s]);
			}
		}
		checkAbove(this.aerials,sprite);
		checkAbove(this.profiles,sprite);
		checkAbove(this.platforms,sprite);
		if(yarg.length > 0)
			return yarg;
		return false;
	},
	cycle:function() {
		function cycle(sprites) {
			for(var s = 0;s<sprites.length;s++) {
				sprites[s].cycle();
			}
		}
		cycle(this.aerials);
		cycle(this.profiles);
		cycle(this.platforms);
	}
};

/*	Aerial Sprite 

	This is the type of sprite you might use for an RTS game, space flying game,
	or anything that requires omnidirectional movement, constant movement.
	It is geared towards a birds-eye perspective, so gravity is generally insignificant.
	Platforms can be used along with Aerial sprites to create walls and obstacles.

	Methods:
		
		sprite.move(direction)
							Where 'direction' is either "forward" or
							"reverse", will cause the sprite to move in
							one space in or away from the direction
							it is currently facing (ie. its bearing.)

		sprite.turn(direction)
							Where 'direction' is either 'cw' or 'ccw'
							(clockwise or counter-clockwise,) will
							cause the sprite to change to the next
							available bearing in that direction and
							load the applicable graphic.
							
		sprite.turnTo(bearing)
							Rotate the sprite to an absolute bearing.
								  
		sprite.putWeapon()	Finds the center of the sprite's forward
							facing side and produces a weapon at that
							point. (The weapon will be another sprite
							which must exist in the 'sprites' sub-
							directory, and should have constantmotion
							set to 1 in its own ini file, and have a
							range defined.)
								  
		sprite.pursue(sprite)
							Causes Sprite to turn clockwise, once, toward
							'sprite'.  Returns true if Sprite is facing
							toward 'sprite'. (If Sprite.move() is then
							called, Sprite will start moving toward
							'sprite'.)  Useful for causing enemies to
							chase after the player, etc.
							
	Properties:

		sprite.bearing		The direction that the sprite is facing
							(ex: n,ne,e,se,s,sw, or w). (Top of screen is
							north, right side of screen is east, and so
							on.)
								  
		sprite.position		The alternate graphic to be shown for the sprite.
							These are specified in your <sprite>.ini file
							and can allow for animated motion by shifting
							the position setting during movement.
								  
		sprite.lastMove		The time when the sprite last moved (as read
							from system.timer
								  
		sprite.lastAttack	The time when the sprite last used its weapon
							(as read from system.timer)
								  
		sprite.weaponCoordinates
							Where the sprite's weapon should appear on
							the screen (calculated by sprite.putWeapon())
								  
		sprite.ini			Has sub-properties for every value defined in
							this sprite's ini file.
								  
		sprite.bearings		An array of vertical offsets to the top row of
							each of the bearing graphics in the <sprite>.bin file.
		
		sprite.positions	An array of horizontal offsets to the left column
							of each additional sprite position as drawn in the 
							<sprite>.bin. file.
								  
*/
Sprite.Aerial = function(fileName, parentFrame, x, y, bearing, position) {
	if(!file_exists(fileName + ".ini"))
		fileName = js.exec_dir + "sprites/" + fileName;
	if(!file_exists(fileName + ".ini")) {
		throw("Sprite file missing: " + fileName + ".ini");
	}
	if(!file_exists(fileName + ".bin")) {
		throw("Sprite file missing: " + fileName + ".bin");
	}
		
	this.x = x;
	this.y = y;
	this.origin = { x : x, y : y };
	this.bearing = bearing;
	this.position = position;
	this.lastBearing = bearing;
	this.lastPosition = position;
	this.lastMove = system.timer;
	this.lastAttack = system.timer;
	this.open = true;
	this.weaponCoordinates = { x : 0, y : 0 };
//	this.bearings = {undefined:0};
//	this.positions = {undefined:0};
	this.bearings = {};
	this.positions = {};

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
		/* locate current bearing in ini settings */
		var b=0;
		for(;b<this.ini.bearings.length;b++) {
			if(this.ini.bearings[b] == this.bearing)
				break;
		}
		/* increment or decrement bearing */
		if(direction == "cw")
			b++;
		else if(direction == "ccw") 
			b--;
		if(b>=this.ini.bearings.length)
			b=0;
		else if(b<0)
			b=this.ini.bearings.length-1;
		this.bearing = this.ini.bearings[b];
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
		if(this.weaponCoordinates.x < 1 || this.weaponCoordinates.x > 80 
			|| this.weaponCoordinates.y < 1 || this.weaponCoordinates.y > 24)
			return false;
		var w = new Sprite.Aerial(this.ini.weapon, this.frame.parent, 
			this.weaponCoordinates.x, this.weaponCoordinates.y, this.bearing);
		w.owner = this;
		w.frame.open(); // Shouldn't be necessary, but sprite doesn't appear unless I do this
		return(w);
	}
	
	this.getcmd = function(userInput) {
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
				if(this.ini.movement == "directional" && this.ini.bearings.indexOf("n") >= 0) {
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
					this.move("reverse");
				} else {
					if(this.ini.speed >= this.ini.speedmin)
						this.ini.speed = 0;
					if(this.ini.speed <= this.ini.speedmin && this.ini.speed != 0)
						this.ini.speed = this.ini.speed + this.ini.speedstep;
				}
				break;
			case KEY_JUMP:
				this.jump();
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
		if(this.bearing != this.lastBearing || this.position != this.lastPosition) {
			ret = true;
			this.lastBearing = this.bearing;
			this.lastPosition = this.position;
			this.frame.scrollTo(this.positions[this.position], this.bearings[this.bearing]);			
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
		this.frame.scrollTo(this.positions[this.position], this.bearings[this.bearing]);			
	}

	function init(fileName, parentFrame, x, y, bearing, position) {
		var f = new File(fileName + ".ini");
		f.open("r",true);
		this.ini = f.iniGetObject();
		f.close();
		
		/* y offset for directional bearings */
		if(this.ini.hasOwnProperty("bearings")) {
			this.ini.bearings = this.ini.bearings.split(",");
			for(var b=0;b<this.ini.bearings.length;b++) {
				var bearing = this.ini.bearings[b];
				this.bearings[bearing] = this.ini.height * b;
			}
		}
		/* x offsets for sprite position */
		if(this.ini.hasOwnProperty("positions")) {
			this.ini.positions = this.ini.positions.split(",");
			for(var p=0;p<this.ini.positions.length;p++) {
				var pos = this.ini.positions[p];
				this.positions[pos] = this.ini.width * p;
			}
		}
		/* parse some ini shit */
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
		if(this.ini.hasOwnProperty("weapon"))
			this.ini.weapon = this.ini.weapon;
		else
			this.ini.weapon = false;
		if(this.ini.hasOwnProperty("range"))
			this.ini.range = parseInt(this.ini.range);
		else
			this.ini.range = 0;
		/* initialize weapon */
		if(this.ini.weapon) {
			var f = new File(js.exec_dir + "sprites/" + this.ini.weapon + ".ini");
			f.open("r");
			var wi = f.iniGetObject();
			f.close();
			this.ini.weaponWidth = wi.width;
			this.ini.weaponHeight = wi.height;
		}

		/* initialize sprite frame */
		this.frame = new Frame(x, y, this.ini.width, this.ini.height, 0, parentFrame);
		this.frame.checkbounds = false;
		this.frame.v_scroll = true;
		this.frame.h_scroll = true;
		this.frame.transparent = true;
		this.frame.top();
		this.frame.load(fileName + ".bin", 
			(this.ini.width * countMembers(this.positions)), 
			(this.ini.height * countMembers(this.bearings)));
		this.frame.scrollTo(this.positions[this.position],this.bearings[this.bearing]);
	}
	init.apply(this,arguments);
	Sprite.aerials.push(this);
}

/*	Profile Sprite 

	This is the type of sprite you would create for a 'platformer' type
	environment. Typically side-scrolling games where gravity is a factor.
	Platform and Profile sprites are best friends.

	Methods:
		
		sprite.move(direction)
							Where 'direction' is either "forward" or
							"reverse", will cause the sprite to move in
							one space in or away from the direction
							it is currently facing (ie. its bearing.)

		sprite.turnTo(bearing)
							Turn the sprite to an absolute bearing.
								  
		sprite.putWeapon()	Finds the center of the sprite's forward
							facing side and produces a weapon at that
							point. (The weapon will be another sprite
							which must exist in the 'sprites' sub-
							directory, and should have constantmotion
							set to 1 in its own ini file, and have a
							range defined.)
								  
		sprite.pursue(sprite)
							Causes Sprite to turn towards
							'sprite'.  Returns true if sprite is facing
							toward 'sprite'. (If sprite.move() is then
							called, sprite will start moving toward
							'sprite'.)  Useful for causing enemies to
							chase after the player, etc.
		
		sprite.jump()
							Initiate a sprite's jump sequence.
							Will only work if the sprite is not already
							jumping.
	
	Properties:
		
		sprite.bearing		The direction that the sprite is facing. For
							the vast majority of Profile sprites, this 
							will likely be either "e" or "w"
							
		sprite.position		The alternate graphic to be shown for the sprite.
							These are specified in your <sprite>.ini file
							and can allow for animated motion by shifting
							the position setting during movement.
								  
		sprite.lastMove		The time when the sprite last moved (as read
							from system.timer
								  
		sprite.lastAttack	The time when the sprite last used its weapon
							(as read from system.timer)
								  
		sprite.weaponCoordinates
							Where the sprite's weapon should appear on
							the screen (calculated by sprite.putWeapon())
								  
		sprite.ini			Has sub-properties for every value defined in
							this sprite's ini file.
								  
		sprite.bearings		An array of vertical offsets to the top row of
							each of the bearing graphics in the <sprite>.bin file.
		
		sprite.positions	An array of horizontal offsets to the left column
							of each additional sprite position as drawn in the 
							<sprite>.bin. file
							
		sprite.inJump		True if the sprite is jumping
		
		sprite.inFall		True if the sprite is falling
		
		sprite.jumpStart	The time when a jump began (as read from
							system.timer)
*/
Sprite.Profile = function(fileName, parentFrame, x, y, bearing, position) {
	if(!file_exists(fileName + ".ini"))
		fileName = js.exec_dir + "sprites/" + fileName;
	if(!file_exists(fileName + ".ini")) {
		throw("Sprite file missing: " + fileName + ".ini");
	}
	if(!file_exists(fileName + ".bin")) {
		throw("Sprite file missing: " + fileName + ".bin");
	}
	
	this.x = x;
	this.y = y;
	this.bearing = bearing;
	this.position = position;
	this.origin = { x : x, y : y };
	this.weaponCoordinates = { x : 0, y : 0 };
	this.lastBearing = bearing;
	this.lastPosition = position;
	this.lastMove = system.timer;
	this.lastAttack = system.timer;
	this.open = true;
	this.inJump = false;
	this.inFall = false;
	this.jumpStart = 0;
	this.bearings = {undefined:0};
	this.positions = {undefined:0};



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
		/* locate current bearing in ini settings */
		var b=0;
		for(;b<this.ini.bearings.length;b++) {
			if(this.ini.bearings[b] == this.bearing)
				break;
		}
		/* increment or decrement bearing */
		if(direction = "cw")
			b++;
		else if(direction == "ccw") 
			b--;
		if(b>=this.ini.bearings.length)
			b=0;
		else if(b<0)
			b=this.ini.bearings.length-1;
		this.bearing = this.ini.bearings[b];
	}
	
	this.putWeapon = function() {
		if(!this.ini.weapon || system.timer - this.lastAttack < this.ini.attackspeed)
			return false;
		this.lastAttack = system.timer;
		switch(this.bearing) {
			case "e":
				this.weaponCoordinates = {
					x : this.x + this.frame.width,
					y : parseInt(this.y + (this.frame.height / 2))
				}
				break;
			case "w":
				this.weaponCoordinates = {
					x : this.x - this.ini.weaponWidth,
					y : parseInt(this.y + (this.frame.height / 2))
				}
				break;
			default:
				break;
		}
		if(this.weaponCoordinates.x < 1 || this.weaponCoordinates.x > 80 
			|| this.weaponCoordinates.y < 1 || this.weaponCoordinates.y > 24)
			return false;
		var w = new Sprite(this.ini.weapon, this.frame.parent, 
			this.weaponCoordinates.x, this.weaponCoordinates.y, this.bearing);
		w.owner = this;
		w.frame.open();
	}
	
	this.getcmd = function(userInput) {
		switch(userInput.toUpperCase()) {
			case KEY_LEFT:
				if(this.ini.bearings.indexOf("w") >= 0) {
					if(this.bearing != "w")
						this.turnTo("w");
					else if(this.ini.constantmotion == 0)
						this.move("forward");
				}
				break;
			case KEY_RIGHT:
				if(this.ini.bearings.indexOf("e") >= 0) {
					if(this.bearing != "e")
						this.turnTo("e");
					else if(this.ini.constantmotion == 0)
						this.move("forward");
				} 
				break;
			case KEY_UP:
				if(this.ini.bearings.indexOf("n") >= 0) {
					if(this.bearing != "n")
						this.turnTo("n");
					else if(this.ini.constantmotion == 0)
						this.move("forward");				
				} else if(this.ini.constantmotion == 1) {
					if(this.ini.speed > this.ini.speedmax)
						this.ini.speed = this.ini.speed - this.ini.speedstep;
					if(this.ini.speed <= this.ini.speedmax && this.ini.speed != 0)
						this.ini.speed = this.ini.speedmax;
					if(this.ini.speed == 0)
						this.ini.speed = this.ini.speedmin;
				}
				break;
			case KEY_DOWN:
				if(this.ini.bearings.indexOf("s") >= 0) {
					if(this.bearing != "s")
						this.turnTo("s");
					else if(this.ini.constantmotion == 0)
						this.move("forward");
				} else if(this.ini.constantmotion == 1) {
					if(this.ini.speed == this.ini.speedmin)
						this.ini.speed = 0;
					if(this.ini.speed <= this.ini.speedmin && this.ini.speed != 0)
						this.ini.speed = this.ini.speed + this.ini.speedstep;
				}
				break;
			case KEY_JUMP:
				this.jump();
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
			if(Sprite.checkAbove(this) || this.y == this.jumpStart - this.ini.jumpheight) {
				this.inJump = false;
				this.inFall = true;
				if(this.positions["fall"] != undefined)
					this.changePosition("fall");
			} else {
				this.y = this.y - 1;
				this.lastMove = system.timer;
			}
		}
		if(this.ini.gravity > 0 && !this.inJump && system.timer - this.lastMove > this.ini.speed) {
		
			if(!Sprite.checkBelow(this)) {
				this.y = this.y + 1;
				this.lastMove = system.timer;
			} 
			
			if(this.inFall && Sprite.checkBelow(this)) {
				this.inFall = false;
				if(this.positions["normal"] != undefined)
					this.changePosition("normal");
			}
		}
		if(this.bearing != this.lastBearing || this.position != this.lastPosition) {
			ret = true;
			this.lastBearing = this.bearing;
			this.lastPosition = this.position;
			this.frame.scrollTo(this.positions[this.position], this.bearings[this.bearing]);			
		}
		if(this.x != this.frame.x || this.y != this.frame.y) {
			ret = true;
			this.frame.moveTo(this.x, this.y);
		}
		if(this.ini.range > 0 &&
			(	this.x - this.origin.x >= this.ini.range
			|| 	this.origin.x - this.x >= this.ini.range
			|| 	this.y - this.origin.y >= this.ini.range / 2
			|| 	this.origin.y - this.y >= this.ini.range / 2)) {
			this.frame.close();
			this.open = false;
		}
		return ret;
	}

	this.jump = function() {
		if(this.inJump || this.inFall)
			return;
		this.jumpStart = this.y;
		this.inJump = true;
		
		if(this.positions["jump"] != undefined)
			this.changePosition("jump");
	}
	
	this.pursue = function(s) {
		var targetBearing;
		var attack = false;
		if(this.x > s.x + s.frame.width) 
			targetBearing = "w";
		else if(this.x + this.width < s.x)
			targetBearing = "e";
		
		if(this.bearing != targetBearing)
			this.turnTo(targetBearing);
		else if(s.inJump && !this.inJump)
			this.jump();
		//else 
			//this.move("forward");
		return attack;
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
		this.frame.scrollTo(this.positions[this.position],this.bearings[this.bearing]);			
	}
	
	this.changePosition = function(position) {
		if(this.ini.positions.indexOf(position) < 0)
			return false;
		this.position = position;
		this.frame.scrollTo(this.positions[this.position],this.bearings[this.bearing]);			
	}

	function init(fileName, parentFrame, x, y, bearing, position) {
		var f = new File(fileName + ".ini");
		f.open("r",true);
		this.ini = f.iniGetObject();
		f.close();

		/* y offset for directional bearings */
		if(this.ini.hasOwnProperty("bearings")) {
			this.ini.bearings = this.ini.bearings.split(",");
			for(var b=0;b<this.ini.bearings.length;b++) {
				var bearing = this.ini.bearings[b];
				this.bearings[bearing] = this.ini.height * b;
			}
		}
		/* x offsets for sprite position */
		if(this.ini.hasOwnProperty("positions")) {
			this.ini.positions = this.ini.positions.split(",");
			for(var p=0;p<this.ini.positions.length;p++) {
				var pos = this.ini.positions[p];
				this.positions[pos] = this.ini.width * p;
			}
		}
		/* parse some ini shit */
		this.ini.speed = parseFloat(this.ini.speed);
		this.ini.speedstep = parseFloat(this.ini.speedstep);
		this.ini.speedmax = parseFloat(this.ini.maximumspeed);
		this.ini.speedmin = parseFloat(this.ini.minimumspeed);
		
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
		/* initialize weapon */
		if(this.ini.weapon) {
			var f = new File(js.exec_dir + "sprites/" + this.ini.weapon + ".ini");
			f.open("r");
			var wi = f.iniGetObject();
			f.close();
			this.ini.weaponWidth = wi.width;
			this.ini.weaponHeight = wi.height;
		}
		/* frame init */
		this.frame = new Frame(x, y, this.ini.width, this.ini.height, 0, parentFrame);
		this.frame.checkbounds = false;
		this.frame.transparent = true;
		this.frame.v_scroll = true;
		this.frame.h_scroll = true;
		this.frame.load(fileName + ".bin", 
			(this.ini.width * countMembers(this.ini.positions)),
			(this.ini.height * countMembers(this.ini.bearings)));
		this.frame.top();
		this.frame.scrollTo(this.positions[this.position],this.bearings[this.bearing]);
	}
	init.apply(this,arguments);
	Sprite.profiles.push(this);	
}

/* Platform Sprite

	This is what you would use to construct platforms, walls, ceilings.
	By being a subclass of Sprite, overlaps and other interference can be detected
	just like any other sprite, thus giving you an easy way to construct 
	a sprite-friendly environment.
	
	Methods:
	
		sprite.move(bearing)	Where 'bearing' is one of n,ne,e,se,s,sw,w,nw.
								The sprite will move one space in the bearing 
								specified.

*/
Sprite.Platform = function(parentFrame, x, y, width, height, ch, attr) {

	this.x = x;
	this.y = y;
	this.origin = { x : x, y : y };
	this.open = true;

	/* frame init */
	this.frame = new Frame(x, y, width, height, attr, parentFrame);
	this.frame.checkbounds = false;
	this.frame.top();
	
	if(ch != undefined) {
		for(var x=0;x<this.frame.width;x++) {
			for(var y=0;y<this.frame.height;y++) {
				this.frame.setData(x,y,ch);
			}
		}
	}
	
	/* push this sprite into the stack */
	Sprite.platforms.push(this);

	this.cycle = function() {
		//ToDo: add movement support, maybe?
		return false;
	
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
		if(this.bearing != this.lastBearing || this.position != this.lastPosition) {
			ret = true;
			this.lastBearing = this.bearing;
			this.lastPosition = this.position;
			this.frame.scrollTo(this.positions[this.position], this.offsets[this.bearing]);			
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

	this.remove = function() {
		this.open = false;
		this.frame.close();	
	}
	
	this.move = function(direction) {
		switch(direction) {
			case "n":
				this.frame.move(0, -1);
				this.y = this.y - 1;
				break;
			case "ne":
				this.frame.move(1, -1);
				this.x++;
				this.y = this.y - 1;
				break;
			case "e":
				this.frame.move(1, 0);
				this.x++;
				break;
			case "se":
				this.frame.move(1, 1);
				this.x++;
				this.y++;
				break;
			case "s":
				this.frame.move(0, 1);
				this.y++;
				break;
			case "sw":
				this.frame.move(-1, 1);
				this.x = this.x - 1;
				this.y++;
				break;
			case "w":
				this.frame.move(-1, 0);
				this.x = this.x - 1;
				break;
			case "nw":
				this.frame.move(-1, -1);
				this.x = this.x - 1;
				this.y = this.y - 1;
				break;
			default:
				break;
		}
	}
	
	this.moveTo = function(x, y) {
		this.x = x;
		this.y = y;
		this.frame.moveTo(x, y);
	}
	
}