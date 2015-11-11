var ai={
	charbuf:'',
	ansi_started:0,

	// Called every 100ms *and* every char.
	add:function(ch) {
		var i;
		var q;
		var byte;
		var m;
		q = new Queue("dorkit_input");

		if (ch == '\x1b') {
			if (this.ansi_started) {
				for(i=0; i<this.charbuf.length; i++) {
					byte = this.charbuf.substr(i,1)
					q.write(byte);
				}
			}
			this.ansi_started = Date.now();
			this.charbuf = ch;
			return;
		}

		// Add the character to charbuf.
		if (ch !== undefined && ch.length > 0) {
			if (!this.ansi_started) {
				q.write(ch);
				return;
			}

			this.charbuf += ch;

			// Now check for ANSI sequences and translate.
			if (this.charbuf.length >= 2) {
				switch(this.charbuf) {
					case '\x1b[A':
					case '\x1bOA':
					case '\x1bA':
						q.write("KEY_UP\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b[B':
					case '\x1bOB':
					case '\x1bB':
						q.write("KEY_DOWN\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b[C':
					case '\x1bOC':
					case '\x1bC':
						q.write("KEY_RIGHT\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b[D':
					case '\x1bOD':
					case '\x1bD':
						q.write("KEY_LEFT\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1bH':
					case '\x1b[H':
					case '\x1b[1~':
					case '\x1b[L':
					case '\x1b[w':
					case '\x1b[OH':
						q.write("KEY_HOME\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1bK':
					case '\x1b[K':
					case '\x1b[4~':
					case '\x1bOK':
						q.write("KEY_END\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1bP':
					case '\x1bOP':
					case '\x1b[11~':
						q.write("KEY_F1\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1bQ':
					case '\x1bOQ':
					case '\x1b[12~':
						q.write("KEY_F2\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b?w':
					case '\x1bOR':
					case '\x1bOw':
					case '\x1b[13~':
						q.write("KEY_F3\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b?x':
					case '\x1bOS':
					case '\x1bOx':
					case '\x1b[14~':
						q.write("KEY_F4\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b?t':
					case '\x1bOt':
					case '\x1b[15~':
						q.write("KEY_F5\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b?u':
					case '\x1b[17~':
					case '\x1bOu':
						q.write("KEY_F6\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b?q':
					case '\x1b[18~':
					case '\x1bOq':
						q.write("KEY_F7\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b?r':
					case '\x1b[19~':
					case '\x1bOr':
						q.write("KEY_F8\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b?p':
					case '\x1b[20~':
					case '\x1bOp':
						q.write("KEY_F9\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b[21~':
						q.write("KEY_F10\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b[23~':
						q.write("KEY_F11\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b[24~':
						q.write("KEY_F12\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b[M':
					case '\x1b[V':
					case '\x1b[5~':
						q.write("KEY_PGUP\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b[U':
					case '\x1b[6~':
						q.write("KEY_PGDOWN\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b[@':
					case '\x1b[2~':
						q.write("KEY_INS\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
					case '\x1b[3~':
						q.write("KEY_DEL\x00"+this.charbuf);
						this.ansi_started = 0;
						break;
				}
				m = this.charbuf.match(/^\x1b\[([0-9]+);([0-9]+)R$/);
				if (m !== null) {
					q.write(format("POSITION_"+m[1]+"_"+m[2]+"\x00"+this.charbuf));
					this.ansi_started = 0;
				}
				if (!this.ansi_started)
					return;
			}
		}

		// Timeout out waiting for escape sequence.
		if (this.charbuf.length > 10 || (this.ansi_started && this.ansi_started + 100 < Date.now())) {
			for(i=0; i<this.charbuf.length; i++) {
				byte = this.charbuf.substr(i,1)
				q.write(byte);
			}
			this.ansi_started = 0;
		}
		return;
	}
};
