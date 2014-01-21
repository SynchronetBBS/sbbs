/* 
SIF questionaire library -
emulates baja functions CREATE_SIF, READ_SIF
$id: $
mcmlxxix (2014)
*/
SIF = new (function() {
	load("sbbsdefs.js");
	/* load questionaire data from sif file */
	this.read_file=function(sifFile) {
		var f = new File(system.text_dir + sifFile);
		f.open('r',true);
		var d = f.read();
		f.close();
		var q = [];
		
		// <STX>text<ETX>mode[mod][l][r][x][.n]["str"]
		while(d.length > 0) {
			// STX     is the ASCII code for start of text (ASCII 2 / Ctrl-B)
			var stx = d.indexOf('\x02');
			/* if there is no more text to display, exit */
			if(stx < 0)
				break;
			// ETX     is the ASCII code for end of text   (ASCII 3 / Ctrl-C)
			var etx = d.indexOf('\x03');
			// text    is any number of ASCII characters - Synchronet Ctrl-A codes supported
			var text = lfexpand(d.substring(stx+1,etx));
			/* pull that shit out */
			d = d.substr(etx+1);
			// mode    text input mode desired for this field. Possible mode values are:
							// c       single character
							// s       string of characters
			var match = d.match(/([cs]?)([nuf]?)(l?)(r?)(\d*)\.?(\d*)(\"?.*\"?)/);
			/* mode must exist in order for the rest to exist */
			if(!match || !match[1]) continue;
			var mode = match.shift();
			// mod     optional mode modifier. Possible mode modifiers are:
							// n       numeric characters only
							// u       input converted to uppercase
							// f       forced word capitalization ('s' mode only)
			var mod = match[2];
			// l       input line will be displayed (inverse bar of maximum input length)
			var input = match[3];
			// r       a carriage return / line feed pair will be appended to this field
					// in the data buffer. Only use this field if you want the data buffer
					// or file to be more readable. All data is on one line otherwise.
			var crlf = match[4];
			// x       maximum string length allowed (required for non-template 's' mode)
			var max = match[5];
			// n       minimum string length allowed (only applicable with 's' input mode)
			var min = match[6];
			// str  1: in 's' modes, a template string that defines what will be displayed
					// at the prompt and what type of characters the user can input. All
					// characters other than 'N', 'A' or '!' are printed at the prompt.
					// Occurances of 'N', 'A' or '!' define which type of character the user
					// can input for each character position. 'N' allows the user only to
					// enter a numeric character, 'A' allows only alphabetic, and '!' allows
					// any character. Popular templates are "NNN-NNN-NNNN" for phone number
					// input or "NN/NN/NN" for date input.
				 // 2: in 'c' modes, a string that defines which characters the user will
					// be allowed to input (not case sensitive), usually used for multiple
					// choice answers. Most common allowed characters are "ABCD..." or
					// "1234...". If this string is specified in 'c' input mode, 'u' and 'n'
					// have no effect and input will be converted to uppercase automatically.
			var str = match[7];
			
			/* strip out remaining crap */
			d = d.substr(d.indexOf("\n"));
			
			/* store question object */ 
			q.push(new Question(text,mode,mod,input,crlf,max,min,str));
		}
		
		return q;
	}

	/* load user answer data from file */
	this.read_answers=function(sifData,answerFile) {
		var f = new File(answerFile);
		f.open('r',true);
		for(var q=0;q<sifData.length;q++) {
			if(sifData[q].crlf)
				sifData[q].answer = f.readln();
			if(sifData[q].mode == "c")
				sifData[q].answer = f.read(1);
			else if(sifData[q].max)
				sifData[q].answer = f.read(sifData[q].max);
			else if(sifData[q].str)
				sifData[q].answer = f.read(sifData[q].str.length);
		}
		f.close();
		return sifData;
	}

	/* store user answer data to file */
	this.write_answers=function(sifData,answerFile) {
		var f = new File(answerFile);
		f.open('w',false);
		for(var q=0;q<sifData.length;q++) {
			if(sifData[q].crlf)
				f.writeln(sifData[q].answer);
			else if(sifData[q].mode == "c")
				f.write(sifData[q].answer);
			else if(sifData[q].max)
				f.write(padAnswer(sifData[q].answer,sifData[q].max));
			else if(sifData[q].str)
				f.write(padAnswer(sifData[q].answer,sifData[q].str.length));
		}
		f.close();
	}
	
	/* run questionaire routine */
	this.create=function(sifFile,answerFile) {
		var sifData = this.read_file(sifFile);
		console.crlf();
		for(var i=0;i<sifData.length;i++) {
			var q = sifData[i];
			console.print(q.text);
			switch(q.mode) {
			case 'c':
				q.answer = getChar(q);
				break;
			case 's':
				q.answer = getStr(q);
				break;
			default:
				continue;
			}
			if(q.answer == undefined)
				q--;
		}
		console.crlf();
		this.write_answers(sifData,answerFile);
	}
	
	/* show questions and answers */
	this.read=function(sifFile,answerFile) {
		var sifData = this.read_answers(this.read_file(sifFile));
		console.crlf();
		for(var i=0;i<sifData.length;i++) {
			var q = sifData[i];
			console.print(q.text + q.answer);
			console.crlf();
		}
	}
	
	/* get a character */
	function getChar(q) {
		if(q.str) 
			return console.getkeys(q.str).toUpperCase();
		switch(q.mod) {
		case 'n':
			return console.getkey(K_NUMBER);
		case 'u':
			return console.getkey(K_UPPER);
		default:
			return console.getkey(K_NONE);
		}
	}
	
	/* get a string */
	function getStr(q) {
		if(q.str) 
			return console.gettemplate(q.str);
		var mode = K_NONE;
		switch(q.mod) {
		case 'n':
			mode |= K_NUMBER;
			break;
		case 'u':
			mode |= K_UPPER;
			break;
		case 'f':
			mode |= K_UPRLWR;
			break;
		}
		if(q.input)
			mode |= K_LINE;
		var str = console.getstr('',q.max,mode);
		if(q.min && str.length < min)
			return undefined;
		return str;
	}

	/* pad a string with ETX */
	function padAnswer(str,len) {
		while(str.length < len) 
			str += "";
		return str;
	}
	
	/* SIF question object */
	function Question(text,mode,mod,input,crlf,max,min,str,answer) {
		this.text=text;
		this.mode=mode;
		this.mod=mod;
		this.input=input;
		this.crlf=crlf;
		this.max=max;
		this.min=min;
		this.str=str;
		this.answer=answer;
	}
})();

