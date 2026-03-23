// avatars-float.js
// Extracted AvatarsFloat animation for focused debugging.
// Provides a frame-only avatar sprite system using avatar_lib.js (mandatory).
// Guarded load to avoid double-loading (which caused 'redeclaration of const defs').
(function () {
	"use strict";
	// avatar_lib is passed via opts.avatar_lib from the main app
	function getMaxEntities() {
		var minimumEntities = 4;
		var squeeze = 8;
		var size = console.screen_rows * console.screen_columns;
		var avatarSize = 10 * 6; // width * height
		var maxEntities = parseInt(Math.floor(size / avatarSize) / squeeze);
		return maxEntities > minimumEntities ? maxEntities : minimumEntities;
	}
	function rand(a, b) { return a + Math.floor(Math.random() * (b - a + 1)); }

	function AvatarsFloat() {
		log("AvatarsFloat:ctor:start");
		this.f = null; this.entities = []; this.tickCount = 0; this.lastGreetingTick = 0;
		this.maxEntities = getMaxEntities(); this.speed = 1; this.greetingDuration = 20; // frames (shorter)
		this._avatarLib = null; this._opts = {}; this._debug = true; // forced on for now
		this._loadAttempted = false; // ensure we only try after opts available
		this._initLogged = false; this._firstTickLogged = false;
		log("AvatarsFloat:ctor:end");

	}

	AvatarsFloat.prototype._log = function (msg) { log(LOG_DEBUG, '[AvatarsFloat] ' + msg); };

	AvatarsFloat.prototype._loadAvatars = function () {
		if (this._avatarLib) return;
		if (this._opts && this._opts.avatar_lib) { this._avatarLib = this._opts.avatar_lib; }
		if (!this._avatarLib) throw 'avatars-float: required avatar_lib unavailable';
	};

	AvatarsFloat.prototype._collectChatNickAvatars = function () {
		var out = [];
		// Call getChatNicks() callback for fresh data each time (users may have joined since bootstrap)
		var nicks = (this._opts && typeof this._opts.getChatNicks === 'function') ? this._opts.getChatNicks() : (this._opts && this._opts.chat_nicks ? this._opts.chat_nicks : []);
		if (!nicks || !nicks.length) return out;
		var lib = this._avatarLib;
		var localQwkId = '';
		try { localQwkId = system.qwk_id ? system.qwk_id.toUpperCase() : ''; } catch(e) {}

		for (var i = 0; i < nicks.length; i++) {
			var nick = nicks[i];
			if (!nick || !nick.name) continue;

			// 1. Embedded avatar data (remote users with avatar in payload)
			var embeddedData = nick.avatar ? String(nick.avatar).replace(/^\s+|\s+$/g, '') : '';
			if (embeddedData.length) {
				out.push({ alias: nick.name, avatar: { data: embeddedData } });
				this._log('chat nick embedded: ' + nick.name);
				continue;
			}

			// 2. Local user match
			var nickQwkId = nick.qwkid ? nick.qwkid.toUpperCase() : '';
			var localNum = 0;
			if (!nickQwkId || nickQwkId === localQwkId) {
				try { localNum = system.matchuser(nick.name); } catch(e) {}
			}
			if (localNum > 0) {
				try {
					var av = lib.read(localNum, nick.name, null, null);
					if (av && av.data && !av.disabled) {
						out.push({ alias: nick.name, avatar: av });
						this._log('chat nick local: ' + nick.name + ' user#' + localNum);
						continue;
					}
				} catch(e) {}
			}

			// 3. Remote lookup by netaddr/host
			var netaddr = nick.qwkid || nick.host || null;
			if (netaddr) {
				try {
					var av2 = lib.read(0, nick.name, netaddr, nick.host || null);
					if (av2 && av2.data && !av2.disabled) {
						out.push({ alias: nick.name, avatar: av2 });
						this._log('chat nick remote: ' + nick.name + '@' + netaddr);
						continue;
					}
				} catch(e) {}
			}

			this._log('chat nick no avatar: ' + nick.name);
		}
		return out;
	};

	AvatarsFloat.prototype._collectUsersWithAvatars = function () {
		// Prefer chat room participants (includes embedded avatar data from messages)
		var chatPicks = this._collectChatNickAvatars();
		var remaining = this.maxEntities - chatPicks.length;
		var seen = {};
		for (var ci = 0; ci < chatPicks.length; ci++) {
			if (chatPicks[ci] && chatPicks[ci].alias) seen[chatPicks[ci].alias.toUpperCase()] = true;
		}

		if (chatPicks.length) this._log('chat nicks with avatars: ' + chatPicks.length);

		// Backfill with system users (skip anyone already collected from chat)
		var out = chatPicks.slice();
		if (remaining > 0) {
			var total = system.lastuser; var U = new User; var withAv = 0; var scanned = 0;
			for (var i = 1; i <= total && out.length < this.maxEntities * 3; i++) {
				U.number = i;
				if (U.settings & (USER_DELETED | USER_INACTIVE)) continue;
				scanned++;
				if (seen[U.alias.toUpperCase()]) continue;
				var av = this._avatarLib.read(U.number);
				if (av && av.data) { withAv++; out.push({ alias: U.alias, avatar: av }); }
			}
			this._log('system backfill: scanned=' + scanned + ' withAv=' + withAv);
		}

		this._log('total pre-shuffle=' + out.length);
		for (var x = out.length - 1; x > 0; x--) { var j = (Math.random() * (x + 1)) | 0; var t = out[x]; out[x] = out[j]; out[j] = t; }
		return out.slice(0, this.maxEntities);
	};

	AvatarsFloat.prototype._makeGreeting = function (ent, other) {
		var base = ['Hello ' + other.name, 'Hey ' + other.name, 'Hi ' + other.name, 'Yo ' + other.name, 'Greetings ' + other.name];
		return base[rand(0, base.length - 1)];
	};
	AvatarsFloat.prototype._makeResponse = function (ent, other) {
		var resp = ['Hi ' + other.name, 'Hey there ' + other.name, 'Yo ' + other.name, 'Howdy ' + other.name, '👋'];
		return resp[rand(0, resp.length - 1)];
	};


	AvatarsFloat.prototype.init = function (frame, opts) {
		this._log('init:start');
		this.f = frame; this._opts = opts || {}; this._debug = true; this.entities = []; this.tickCount = 0;
		if (opts && opts.max_entities) this.maxEntities = opts.max_entities;
		if (opts && opts.greeting_duration) this.greetingDuration = Math.max(5, opts.greeting_duration | 0);
		this._loadAvatars();
		if (!this._avatarLib) throw 'avatars-float: avatar_lib missing (post-load)';
		var picks = this._collectUsersWithAvatars();
		var w = frame.width, h = frame.height;
		if (!picks.length) {
			for (var k = 0; k < this.maxEntities; k++) picks.push({ alias: 'P' + (k + 1), avatar: null });
			this._log('placeholders only');
		}
		for (var i = 0; i < picks.length; i++) {
			var p = picks[i]; var av = p.avatar; var lines = [], aw = 0, ah = 0;
			if (av && av.data) {
				// Store raw for potential binary blit later
				var raw = av.data;
				// Heuristic: if contains newline(s), treat as ASCII lines; else maybe base64 sprite
				if (/\n|\r/.test(raw)) {
					lines = (typeof raw === 'string') ? raw.split(/\r?\n/) : (raw.push ? raw : []);
					while (lines.length && lines[lines.length - 1] === '') lines.pop();
					ah = lines.length; for (var li = 0; li < lines.length; li++) { if (lines[li].length > aw) aw = lines[li].length; }
				} else {
					// Assume base64 encoded 2-byte-per-cell sprite (char+attr) for 10x6 if length matches
					p.avatar._base64 = raw; // annotate for frame blit
					aw = 10; ah = 6; // expected logical size
				}
			}
			if (!lines.length) {
				var initial = (p.alias || '?').charAt(0).toUpperCase();
				lines = ['+---+', '| ' + initial + ' |', '+---+']; aw = 5; ah = 3;
			}
			this.entities.push({
				name: p.alias, avatarLines: lines, aw: aw, ah: ah, avatarObj: av || null,
				x: rand(2, Math.max(2, w - Math.max(1, aw))),
				y: rand(2, Math.max(2, h - Math.max(1, ah))),
				dx: (Math.random() < 0.5 ? -1 : 1), dy: (Math.random() < 0.5 ? -1 : 1),
				greet: null, greetUntil: 0, frame: null, greetFrame: null
			});
		}
		// Determine avatar sprite dimensions; clamp height to available canvas to avoid invalid Frame height errors.
		var AVW = 10; var AVH = 6;
		if (this.f && this.f.height) {
			if (this.f.height < AVH) AVH = Math.max(1, this.f.height); // clamp down
			if (AVH < 6) this._log('clamped avatar height to ' + AVH + ' (canvas height ' + this.f.height + ')');
		}
		for (var ei = 0; ei < this.entities.length; ei++) {
			var ent = this.entities[ei]; ent.aw = AVW; ent.ah = AVH;
			// Ensure Y fits within parent
			if (ent.y + AVH - 1 > this.f.height) ent.y = Math.max(1, this.f.height - AVH + 1);
			// Clamp width/X to parent (invalid width errors previously occurred when AVW exceeded remaining space?)
			var px = Math.round(ent.x), py = Math.round(ent.y);
			var maxWAvail = this.f.width - px + 1; // remaining columns including px
			var effW = AVW;
			if (maxWAvail < effW) { effW = Math.max(1, maxWAvail); if (effW < AVW) this._log('clamp avatar width ' + ent.name + ' to ' + effW + ' (parent remaining ' + maxWAvail + ')'); }
			// If effW dropped below nominal sprite width, adjust px leftwards if possible to recover space
			if (effW < AVW && px > 1) {
				var deficit = AVW - effW;
				var shift = Math.min(deficit, px - 1);
				if (shift > 0) { px -= shift; maxWAvail = this.f.width - px + 1; effW = Math.min(AVW, maxWAvail); this._log('shift left ' + ent.name + ' by ' + shift + ' new x=' + px + ' effW=' + effW); }
			}
			// Final safety
			if (px < 1) px = 1;
			if (effW < 1) effW = 1;
			if (this._debug) this._log('create avatar frame ' + ent.name + ' x=' + px + ' y=' + py + ' w=' + effW + ' h=' + AVH + ' parentW=' + this.f.width + ' parentH=' + this.f.height);
			ent.frame = new Frame(px, py, effW, AVH, BG_BLACK | LIGHTGRAY, this.f);
			if (this._opts && typeof this._opts.ownFrame === 'function') this._opts.ownFrame(ent.frame);
			ent.frame.transparent = true; ent.frame.open();
			// If we have a base64 sprite attempt to decode & blit; else draw ASCII lines
			if (ent.avatarObj && ent.avatarObj._base64) {
				if (typeof base64_decode !== 'function') throw 'avatars-float: base64_decode missing';
				var bin = base64_decode(ent.avatarObj._base64);
				this._log('decode ' + ent.name + ' len=' + (bin ? bin.length : 'null'));
				if (!bin || bin.length < (AVW * AVH * 2)) throw 'avatars-float: invalid avatar binary length for ' + ent.name;
				if (typeof ent.frame.setData !== 'function') throw 'avatars-float: Frame.setData missing';
				var off = 0; var centerX = 1; // already sized 10
				for (var y = 0; y < AVH; y++) {
					for (var x = 0; x < AVW; x++) {
						if (off + 1 >= bin.length) break;
						var ch = bin.substr(off++, 1);
						var attr = ascii(bin.substr(off++, 1));
						ent.frame.setData(centerX + x - 1, 1 + y - 1, ch, attr, false);
					}
				}
				this._log('blitted binary avatar ' + ent.name);
			} else {
				for (var ly2 = 0; ly2 < AVH; ly2++) {
					var line2 = (ly2 < ent.avatarLines.length) ? ent.avatarLines[ly2] : '';
					if (line2.length < AVW) line2 += Array(AVW - line2.length + 1).join(' ');
					else if (line2.length > AVW) line2 = line2.substr(0, AVW);
					ent.frame.gotoxy(1, 1 + ly2); ent.frame.putmsg(line2);
				}
			}
			this._log('frame created ' + ent.name + ' at ' + ent.x + ',' + ent.y);
		}
		this.f.cycle();
		this._log('entities=' + this.entities.length);
		this._log('init:done');
	};

	AvatarsFloat.prototype._bounce = function (e, w, h) {
		var maxX = Math.max(1, w - (e.aw || 1) + 1);
		var maxY = Math.max(1, h - (e.ah || 1) + 1);
		if (e.x <= 1) { e.x = 1; e.dx = 1; }
		if (e.x >= maxX) { e.x = maxX; e.dx = -1; }
		if (e.y <= 1) { e.y = 1; e.dy = 1; }
		if (e.y >= maxY) { e.y = maxY; e.dy = -1; }
	};

	AvatarsFloat.prototype._avoidAndGreet = function () {
		for (var i = 0; i < this.entities.length; i++) {
			for (var j = i + 1; j < this.entities.length; j++) {
				var a = this.entities[i], b = this.entities[j];
				var ax = a.x + (a.aw || 1) / 2, ay = a.y + (a.ah || 1) / 2;
				var bx = b.x + (b.aw || 1) / 2, by = b.y + (b.ah || 1) / 2;
				var dx = ax - bx, dy = ay - by; var dist2 = dx * dx + dy * dy;
				var nearDist = Math.max(4, ((a.aw || 1) + (b.aw || 1)) / 2);
				if (dist2 <= nearDist) {
					var tx = a.dx; a.dx = b.dx; b.dx = tx; tx = a.dy; a.dy = b.dy; b.dy = tx;
					var now = this.tickCount;
					if (now >= a.greetUntil && now >= b.greetUntil) {
						var g1 = this._makeGreeting(a, b);
						var g2 = this._makeResponse(b, a);
						a.greet = g1; b.greet = g2;
						a.greetUntil = now + this.greetingDuration; b.greetUntil = now + this.greetingDuration;
						this._log('dialogue ' + a.name + ':"' + g1 + '" / ' + b.name + ':"' + g2 + '"');
					}
				}
			}
		}
	};

	AvatarsFloat.prototype.tick = function () {
		if (!this._firstTickLogged) { this._log('tick:first'); this._firstTickLogged = true; }
		var f = this.f; if (!f) throw 'avatars-float: tick without frame'; var w = f.width, h = f.height; this.tickCount++;
		for (var i = 0; i < this.entities.length; i++) {
			var e = this.entities[i]; e.x += e.dx * this.speed; e.y += e.dy * this.speed; this._bounce(e, w, h);
			if (e.greet && this.tickCount >= e.greetUntil) {
				e.greet = null;
				if (e.greetFrame) {
					try { e.greetFrame.close(); }
					catch (err) { log(err); throw err; }
					e.greetFrame = null;
				}
			}
		}
		this._avoidAndGreet();
		for (var m = 0; m < this.entities.length; m++) {
			var ent = this.entities[m]; if (ent.frame) ent.frame.moveTo(Math.round(ent.x), Math.round(ent.y));
			if (ent.greet) {
				if (!ent.greetFrame) {
					var gwFull = Math.max(3, ent.greet.length);
					var gx = Math.max(1, Math.round(ent.x)); var gy = Math.max(1, Math.round(ent.y) - 1);
					if (gx + gwFull - 1 > w) { gwFull = Math.max(1, w - gx + 1); }
					ent.greetFrame = new Frame(gx, gy, gwFull, 1, BG_BLACK | WHITE, this.f);
					ent.greetFrame.transparent = true; ent.greetFrame.open();
					if (this._opts && typeof this._opts.ownFrame === 'function') this._opts.ownFrame(ent.greetFrame);
					if (this._debug) this._log('create greet frame ' + ent.name + ' x=' + gx + ' y=' + gy + ' w=' + gwFull);
				}
				if (ent.greetFrame) {
					var nx = Math.max(1, Math.round(ent.x)); var ny = Math.max(1, Math.round(ent.y) - 1);
					if (nx + ent.greetFrame.width - 1 > w) nx = Math.max(1, w - ent.greetFrame.width + 1);
					ent.greetFrame.moveTo(nx, ny); ent.greetFrame.clear(); ent.greetFrame.gotoxy(1, 1); ent.greetFrame.putmsg(ent.greet.substr(0, ent.greetFrame.width));
				}
			} else if (ent.greetFrame) { ent.greetFrame.close(); ent.greetFrame = null; }
		}
		f.cycle();
	};

	AvatarsFloat.prototype.dispose = function () {
		for (var i = 0; i < this.entities.length; i++) {
			var e = this.entities[i];
			if (e.greetFrame) { e.greetFrame.close(); e.greetFrame = null; }
			if (e.frame) { e.frame.close(); e.frame = null; }
		}
		this.entities = []; this._log('disposed');
	};

	// Export constructor & set global for load() consumers
	var _exports = { AvatarsFloat: AvatarsFloat };
	var _g = (typeof js !== 'undefined' && js && js.global) ? js.global : undefined;
	if (_g) { try { _g.AvatarsFloat = _exports; } catch(e){} }
	return _exports;

})();
