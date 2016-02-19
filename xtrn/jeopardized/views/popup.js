var PopUp = function (frame, message, prompt, promptType, state) {

	var frames = {
		border : null,
		top : null
	};

	var typeahead;

	function initDisplay() {
		frames.border = new Frame(1, 1, 50, 8, WHITE, frame);
		frames.border.centralize();
		frames.border.drawBorder([LIGHTBLUE,CYAN,LIGHTCYAN,WHITE]);
		frames.top = new Frame(1, 1, 1, 1, WHITE, frames.border);
		frames.top.nest(1, 1);
		frames.top.centerWrap(message);
		frames.border.open();
		if (promptType === PROMPT_ANY || promptType === PROMPT_YN) {
			frames.top.putmsg('\r\n\r\n');
			frames.top.center(prompt);
		} else {
			typeahead = new Typeahead(
				{	x : frames.top.x,
					y : frames.top.y + frames.top.height - 1,
					frame : frames.top,
					fg : WHITE,
					prompt : prompt,
					len : frames.top.width - prompt.length
				}
			);
		}
	}

	this.getcmd = function (cmd) {
		if (typeof typeahead !== 'undefined') cmd = typeahead.inkey(cmd);
		if (typeof cmd !== 'string') return;
		switch (promptType) {
			case PROMPT_YN:
				cmd = cmd.toUpperCase();
				cmd = (cmd === 'Y' ? true : (cmd === 'N' ? false : undefined));
				break;
			case PROMPT_NUMBER:
				cmd = (
					cmd === '' || cmd.search(/[^\d]/) > -1
					? undefined
					: parseInt(cmd)
				);
				break;
			case PROMPT_ANY:
			case PROMPT_TEXT:
				if (typeof cmd !== 'string' || cmd === '') cmd = undefined;
				break;
			default:
				break;
		}
		return cmd;
	}

	this.cycle = function () {
		if (typeof typeahead !== 'undefined') typeahead.cycle();
	}

	this.close = function () {
		if (typeof typeahead !== 'undefined') typeahead.close();
		frames.border.close();
		if (typeof state !== 'undefined') return state;
	}

	initDisplay();

}