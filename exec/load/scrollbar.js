/*	Scrollbars for frame.js
	echicken -at- bbs.electronicchicken.com

	Usage: var s = new ScrollBar(frame, options);
	
		'frame' is the frame you want to assign the scrollbar to.  If you want
		to put a scrollbar beside a Tree, supply the Tree object instead.

		'options' is an object, and is ... optional.  You can supply an	object
		with any of the following properties:

			'bg' - The background color.  Default: BG_BLACK
			'fg' - The foreground color.  Default: LIGHTGRAY
			'autohide' - Hide if content < frame width/height.  Default: false
			'orientation' - "vertical" or "horizontal".  Default: "vertical"

	Methods:

		ScrollBar.cycle()

			Adjusts size and position of the bar.  Should be called whenever
			the frame has been scrolled or frame content changes.  (Just call
			it whenever frame.cycle() returns true.)

	Working example:

		load("sbbsdefs.js");
		load("frame.js");
		load("scrollbar.js");

		var f = new Frame(1, 1, 10, 10, LIGHTGRAY);
		for(var n = 0; n < f.height * 2; n++)
		    f.putmsg(n + "\r\n");
		var s = new ScrollBar(f);
		f.open();

		var userInput = "";
		while(userInput != "\x1B") {
		    userInput = console.inkey(K_NONE, 5);
		    switch(userInput) {
		        case KEY_UP:
		            f.scroll(0, -1);
		            break;
		        case KEY_DOWN:
		            f.scroll(0, 1);
		            break;
		        default:
		            break;
		    }
		    if(f.cycle()) {
		        s.cycle();
		        console.gotoxy(console.screen_columns, console.screen_rows);
		    }
		}

		f.close();
*/

load("sbbsdefs.js");
load("tree.js"); // instanceof

var ScrollBar = function(frame, opts) {

	var self = this;

	var settings = {
		bg : BG_BLACK,
		fg : LIGHTGRAY,
		autohide : false,
		orientation : "vertical"
	};

	var scrollBarFrame, scrollArea, bar;

	var init = function() {

		for(var o in opts) {
			if(typeof settings[o] == typeof opts[o])
				settings[o] = opts[o];
		}
		if(frame instanceof Tree) {
			settings.tree = frame;
			frame = frame.frame;
		}

		scrollBarFrame = new Frame(
			(settings.orientation == "vertical") ? frame.x + frame.width - 1 : frame.x,
			(settings.orientation == "vertical") ? frame.y : frame.y + frame.height - 1,
			(settings.orientation == "vertical") ? 1 : frame.width,
			(settings.orientation == "vertical") ? frame.height : 1,
			settings.bg|settings.fg,
			frame
		);

		scrollArea = new Frame(
			(settings.orientation == "vertical") ? scrollBarFrame.x : scrollBarFrame.x + 1,
			(settings.orientation == "vertical") ? scrollBarFrame.y + 1 : scrollBarFrame.y,
			(settings.orientation == "vertical") ? scrollBarFrame.width : scrollBarFrame.width - 2,
			(settings.orientation == "vertical") ? scrollBarFrame.height - 2 : scrollBarFrame.height,
			settings.bg|settings.fg,
			scrollBarFrame
		);

		bar = new Frame(
			scrollArea.x,
			scrollArea.y,
			scrollArea.width,
			scrollArea.height,
			settings.bg|settings.fg,
			scrollArea
		);

		scrollBarFrame.putmsg((settings.orientation == "vertical") ? ascii(30) : "<");
		scrollBarFrame.end();
		scrollBarFrame.putmsg((settings.orientation == "vertical") ? ascii(31) : ">");

		if(settings.orientation == "vertical") {
			for(var y = 1; y <= scrollArea.height; y++)
				scrollArea.putmsg(ascii(176));
			for(var y = 1; y <= bar.height; y++)
				bar.putmsg(ascii(219));
		} else {
			for(var x = 1; x <= scrollArea.width; x++)
				scrollArea.putmsg(ascii(176));
			for(var x = 1; x <= bar.width; x++)
				bar.putmsg(ascii(219));
		}

		self.cycle();

	}

	var cycleTree = function() {

		var height = settings.tree.height;
		var bh = bar.height;
		var by = bar.y;

		if(settings.autohide && height <= frame.height && scrollBarFrame.is_open)
			self.close();
		else if(settings.autohide && height > frame.height && !scrollBarFrame.is_open)
			self.open();

		var adj = Math.min(
			scrollArea.height,
			Math.round(scrollArea.height * (frame.height / height))
		);
		bar.height = (isNaN(adj) || adj < 1) ? 1 : adj;

		var adj = Math.min(
			scrollArea.height - bar.height,
			Math.round(
				(scrollArea.height - bar.height) * (
					Math.max(0, settings.tree.line - frame.height) / (height - frame.height)
				)
			)
		);
		bar.y = scrollArea.y + (isNaN(adj) ? 0 : adj);

		if(bh != bar.height || by != bar.y)
			scrollBarFrame.invalidate();

	}

	var cycleVertical = function() {

		if(settings.autohide && frame.data_height <= frame.height && scrollBarFrame.is_open)
			self.close();
		else if(settings.autohide && frame.data_height > frame.height && !scrollBarFrame.is_open)
			self.open();

		var adj = Math.min(
			scrollArea.height,
			Math.round(scrollArea.height * (frame.height / frame.data_height))
		);
		bar.height = (isNaN(adj) || adj < 1) ? 1 : adj;

		var adj = Math.min(
			scrollArea.height - bar.height,
			Math.round(
				(scrollArea.height - bar.height) * (
					frame.offset.y / (frame.data_height - frame.height)
				)
			)
		);
		bar.y = scrollArea.y + (isNaN(adj) ? 0 : adj);

	}

	var cycleHorizontal = function() {

		if(settings.autohide && frame.data_width <= frame.width && scrollBarFrame.is_open)
			self.close();
		else if(settings.autohide && frame.data_width > frame.width && !scrollBarFrame.is_open)
			self.open();

		var adj = Math.min(
			scrollArea.width,
			Math.round(scrollArea.width * (frame.width / frame.data_width))
		);
		bar.width = (isNaN(adj) || adj < 1) ? 1 : adj;

		var adj = Math.min(
			scrollArea.width - bar.width,
			Math.round(
				(scrollArea.width - bar.width) * (
					frame.offset.x / (frame.data_width - frame.width)
				)
			)
		);
		bar.x = scrollArea.x + (isNaN(adj) ? 0 : adj);

	}

	this.open = function() {
		scrollBarFrame.open();
		scrollBarFrame.top();
	}

	this.close = function() {
		scrollBarFrame.close();
	}

	this.cycle = function() {
		if(typeof settings.tree != "undefined")
			cycleTree();
		else if(settings.orientation == "vertical")
			cycleVertical();
		else
			cycleHorizontal();
	}

	init();

};

// mcmlxxix's previous Scrollbar object, just in case anyone was using it.
function Scrollbar(x,y,length,orientation,color)
{
	this.index;
	this.bar;
	this.x=x;
	this.y=y;
	this.orientation=orientation;
	this.length=length-2;
	this.color=color?color:'';

	this.draw=function(index, range)
	{
		if(index>range)
			return;
		if((isNaN(index) || isNaN(range)) &&
			(!this.index || !this.bar))
			return;

		this.index=Math.ceil(((index+1)/range)*this.length);
		this.bar=Math.ceil((this.length/range)*this.length);


		if(this.orientation == "vertical")
			this.drawVert(this.index,this.index+this.bar);
		else
			this.drawHoriz(this.index,this.index+this.bar);
	}
	this.drawVert=function(s,f)
	{
		console.gotoxy(this);
		console.putmsg('\1n' + this.color + ascii(30),P_SAVEATR);
		var ch='\xB0';
		for(var i=1;i<=this.length;i++)	{
			console.gotoxy(this.x,this.y+i);
			if(i == s) 
				ch='\1h\xDB';
			else if(i > f) 
				ch='\xB0';
			console.putmsg(ch,P_SAVEATR);
		}
		console.gotoxy(this.x,this.y+this.length+1);
		console.putmsg(ascii(31),P_SAVEATR);
	}
	this.drawHoriz=function(s,f)
	{
		if(f > this.length) {
			s--;
			f--;
		}
		console.gotoxy(this);
		console.putmsg('\1n' + this.color + ascii(17),P_SAVEATR);
		for(i=1;i<=this.length;i++)	{
			if(i == s) 
				ch='\1h\xDB';
			else if(i == f+1)
				ch='\xB0';
			console.putmsg(ch,P_SAVEATR);
		}
		console.putmsg(ascii(16),P_SAVEATR);
	}
}
