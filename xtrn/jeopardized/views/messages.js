var Messages = function (frame, x, y, w, h, input) {

	var frames = {
		border : null,
		messages : null
	};

	var scrollbar,
		subscription,
		scroll = false,
		focus = false,
		typeahead,
		title;

	this.__defineGetter__(
		'focus',
		function () {
			return focus;
		}
	);

	this.__defineSetter__(
		'focus',
		function (bool) {
			focus = bool;
			title.attr = bool ? WHITE : LIGHTGRAY;
			frames.border.drawBorder(
				(	bool
					? [LIGHTBLUE,CYAN,LIGHTCYAN,WHITE]
					: LIGHTGRAY
				),
				title
			);
			if (typeof typeahead !== 'undefined' &&
				typeof typeahead.focus !== 'undefined'
			) {
				typeahead.focus = bool;
			}
		}
	);

	function printMessage(msg) {
		frames.messages.putmsg(
			(frames.messages.data_height > 0 ? '\r\n' : '') +
			(	typeof msg.alias === 'undefined'
				? ''
				: ('\1h\1c<' + msg.alias + '> \1h\1w')
			) + msg.message
		);
		scroll = true;		
	}

	function onMessage(update) {
		log(JSON.stringify(update));
		printMessage(update.data);
	}

	function init() {
		frames.border = new Frame(x, y, w, h, WHITE, frame);
		title = {
			x : frames.border.width - 11,
			y : 1,
			attr : WHITE,
			text : 'Messages'
		};
		frames.border.drawBorder(LIGHTGRAY, title);
		frames.messages = new Frame(1, 1, 1, 1, WHITE, frames.border);
		frames.messages.nest(1, 1);
		scrollbar = new ScrollBar(frames.messages);
		frames.border.open();
		if (typeof input === 'boolean' && input) {
			frames.messages.height = frames.messages.height - 1;
			typeahead = new Typeahead(
				{	x : frames.border.x + 1,
					y : frames.messages.y + frames.messages.height,
					frame : frames.border,
					fg : WHITE,
					len : frames.messages.width - 1
				}
			);
		}
		var msgs = database.getMessages();
		if (typeof msgs !== 'undefined' && Array.isArray(msgs)) {
			msgs.reverse();
			msgs.forEach(printMessage);
		}
		subscription = database.on('messages.latest', onMessage);
		var who = [];
		database.who().forEach(
			function (u) {
				if (u.nick === user.alias) return;
				who.push(u.nick);
			}
		);
		if (who.length > 0) {
			printMessage(
				{ message :
					'\1n\1mOther users online: \1h\1m' +
						who.join('\1n\1m,\1h\1m ')
				}
			);
		}
	}

	this.cycle = function () {
		if (scroll) {
			scrollbar.cycle();
			buryCursor = true;
		}
		if (typeof typeahead !== 'undefined' && focus) typeahead.cycle();
		if (system.node_list[bbs.node_num - 1].misc&NODE_MSGW) {
			var msg = system.get_telegram(user.number).replace(/\r\n$/, '');
			printMessage({ message : msg });
			log('telegram');
		}
		if (system.node_list[bbs.node_num - 1].misc&NODE_NMSG) {
			var msg = system.get_node_message(bbs.node_num);
			msg = msg.replace(/\r\n.n$/, '');
			printMessage({ message : msg });
			frames.messages.scroll(0, -1);
			log('nmsg');
		}
	}

	this.getcmd = function (cmd) {
		switch (cmd) {
			case KEY_UP:
				if (frames.messages.data_height > frames.messages.height) {
					frames.messages.scroll(0, -1);
				}
				break;
			case KEY_DOWN:
				if (frames.messages.data_height > frames.messages.height &&
					frames.messages.offset.y + frames.messages.height <
						frames.messages.data_height
				) {
					frames.messages.scroll(0, 1);
				}
				break;
			default:
				if (typeof typeahead !== 'undefined') {
					var ret = typeahead.inkey(cmd);
					if (typeof ret === 'string' && ret.length > 0) {
						database.postMessage(ret, user);
					}
				}
				break;
		}
	}

	this.close = function () {
		database.off('messages', subscription);
		frames.border.close();
	}

	init();

	this.focus = false;

}