// canvas-animations.js
"use strict";
// Simple animation manager and a few lightweight animations for use in
// the future-login canvasFrame (Frame-based rendering).
// Each animation implements: init(frame), tick(), dispose()
// Animations should avoid blocking; keep work per tick minimal.

if(typeof(load) === 'function') {
	// assume frame.js already loaded by caller
}

(function(){
"use strict";

// Attempt to load Synchronet's event-timer library for lightweight scheduling.
try { if(typeof Timer === 'undefined') load('event-timer.js'); } catch(e) { /* optional */ }

function rand(a,b){ return a + Math.floor(Math.random()*(b-a+1)); }

// Base helper to write char at x,y (1-based) with optional attr
function put(frame,x,y,ch,attr){
	try {
		if(attr!==undefined) frame.attr=attr;
		frame.gotoxy(x,y); frame.putmsg(ch);
	} catch(e){}
}

// TV Static (CP437 noise)
function TvStatic(){
	this.chars = '\xB0\xB1\xB2\xDB .:;!+*=?%#@';
	this.colors = [WHITE,LIGHTGRAY,LIGHTCYAN,CYAN,LIGHTGREEN,LIGHTMAGENTA,LIGHTBLUE,LIGHTRED,YELLOW];
}
TvStatic.prototype.init = function(frame){ this.f=frame; };
TvStatic.prototype.tick = function(){
	var f=this.f; if(!f) return;
	var w=f.width, h=f.height;
	// Draw a sparse static: random subset per tick for performance
	for(var i=0;i<w*h/4;i++){
		var x=rand(1,w), y=rand(1,h);
		var ch=this.chars.charAt(rand(0,this.chars.length-1));
		f.gotoxy(x,y);
		f.attr=this.colors[rand(0,this.colors.length-1)];
		f.putmsg(ch);
	}
	f.cycle();
};
TvStatic.prototype.dispose = function(){};

// Matrix Rain (simplified)
function MatrixRain(){
	this.columns = [];
	this.glyphs = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ#$%&*@';
}
MatrixRain.prototype.init = function(frame, opts){
	this.f=frame; this.columns=[];
	this.sparse = Math.max(1,(opts && opts.matrix_sparse)||1); // update 1/sparse of columns per tick
	this.phase = 0;
	for(var x=1;x<=frame.width;x++){
		if(Math.random()<0.6) this.columns.push({x:x,y:rand(1,frame.height),speed:rand(1,2)});
	}
};
MatrixRain.prototype.tick = function(){
	var f=this.f; if(!f) return;
	var h=f.height;
	f.attr=GREEN;
	var phaseMod = this.phase % this.sparse;
	for(var i=0;i<this.columns.length;i++){
		if((i % this.sparse)!==phaseMod) continue; // throttle column updates
		var c=this.columns[i];
		c.y += c.speed; if(c.y>h+5){ c.y=0; }
		var trailLen = 5;
		for(var t=0;t<trailLen;t++){
			var yy = c.y - t;
			if(yy>=1 && yy<=h){
				f.gotoxy(c.x,yy);
				var ch=this.glyphs.charAt(rand(0,this.glyphs.length-1));
				if(t===0) f.attr=WHITE; else if(t<3) f.attr=LIGHTGREEN; else f.attr=GREEN;
				f.putmsg(ch);
			}
		}
	}
	this.phase++;
	f.cycle();
};
MatrixRain.prototype.dispose = function(){};

// Game of Life (enhanced): wrap-around edges, half-block compression, color cycle, reseed on stagnation
function Life(){
	this.grid=null; this.next=null; this.w=0; this.h=0; this.tickCount=0; this.lastHash='';
	this.density=0.28; // initial fill
	this.reseedAfter=400; // fallback reseed timer
	this.stagnantFrames=0; this.maxStagnant=60; // reseed if unchanged pattern ~60 frames
	this.palette=[LIGHTGREEN,GREEN,LIGHTCYAN,CYAN,LIGHTMAGENTA,MAGENTA,YELLOW,WHITE];
	this.colorIndex=0;
}
Life.prototype._alloc = function(){
	this.grid=[]; this.next=[];
	for(var y=0;y<this.h;y++){
		var r=[], n=[]; for(var x=0;x<this.w;x++){ r.push(Math.random()<this.density?1:0); n.push(0);} this.grid.push(r); this.next.push(n);
	}
};
Life.prototype.init = function(frame){
	this.f=frame; this.w=frame.width; this.h=frame.height*2; // half-block vertical compression
	this._alloc(); this.tickCount=0; this.lastHash=''; this.stagnantFrames=0; this.colorIndex=0;
};
Life.prototype._step = function(){
	var w=this.w, h=this.h, g=this.grid, ngrid=this.next;
	for(var y=0;y<h;y++){
		var y1=(y+1)%h, y_1=(y-1+h)%h;
		for(var x=0;x<w;x++){
			var x1=(x+1)%w, x_1=(x-1+w)%w;
			var alive=g[y][x];
			var count = g[y_1][x_1]+g[y_1][x]+g[y_1][x1]
					   +g[y][x_1]              +g[y][x1]
					   +g[y1][x_1]+g[y1][x]+g[y1][x1];
			ngrid[y][x] = (alive && (count===2||count===3)) || (!alive && count===3) ? 1:0;
		}
	}
	var tmp=this.grid; this.grid=this.next; this.next=tmp;
};
Life.prototype._hash = function(){
	// lightweight hash: sample every 4th cell
	var g=this.grid, w=this.w, h=this.h, acc=0;
	for(var y=0;y<h;y+=4) for(var x=0;x<w;x+=4) acc = (acc*131 + g[y][x]) & 0x7fffffff;
	return acc.toString(36);
};
Life.prototype._draw = function(){
	var f=this.f; if(!f) return;
	var w=this.w; var visH=Math.floor(this.h/2);
	var attr = (this.palette[this.colorIndex%this.palette.length] || LIGHTGREEN);
	if(!this._drawCache || this._drawCache.length !== visH){
		this._drawCache = [];
		for(var i=0;i<visH;i++) this._drawCache[i] = [];
	}
	for(var vy=0; vy<visH; vy++){
		var rowCache = this._drawCache[vy];
		var yTop=vy*2, yBot=yTop+1;
		for(var x=0;x<w && x < f.width;x++){
			var top=this.grid[yTop][x], bot=this.grid[yBot][x];
			var ch;
			if(top && bot) ch='\xDB';
			else if(top) ch='\xDF';
			else if(bot) ch='\xDC';
			else ch=' ';
			if(rowCache[x] === ch && this._lastAttr === attr) continue;
			if(ch === ' ') f.clearData(x, vy, false);
			else f.setData(x, vy, ch, attr, false);
			rowCache[x] = ch;
		}
	}
	this._lastAttr = attr;
	if(typeof f.cycle === 'function') f.cycle();
};
Life.prototype.tick = function(){
	if(!this.f) return;
	this._step();
	this.tickCount++;
	if(this.tickCount % 15 === 0) this.colorIndex++; // slow color cycle
	var h=this._hash();
	if(h===this.lastHash) this.stagnantFrames++; else this.stagnantFrames=0;
	this.lastHash=h;
	if(this.stagnantFrames>this.maxStagnant || this.tickCount>this.reseedAfter){
		this._alloc(); this.tickCount=0; this.stagnantFrames=0; this.colorIndex=0;
	}
	this._draw();
};
Life.prototype.dispose = function(){};

// Starfield: simple lateral parallax star scroller
function Starfield(){
	this.f=null; this.stars=[]; this.speedBase=0.6; this.charSet='.:*'; this.palette=[LIGHTGRAY,LIGHTCYAN,WHITE];
}
Starfield.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	if(opts.star_speed !== undefined) this.speedBase = Math.max(0.1, parseFloat(opts.star_speed) || this.speedBase);
	var count = parseInt(opts.star_count, 10);
	if(isNaN(count) || count <= 0) count = Math.max(12, Math.floor((frame.width * frame.height) / 10));
	this.stars.length = 0;
	for(var i=0;i<count;i++) this.stars.push(this._spawn(frame));
};
Starfield.prototype._spawn = function(frame){
	return {
		x: 1 + Math.random() * frame.width,
		y: 1 + Math.random() * frame.height,
		speed: this.speedBase + Math.random() * this.speedBase,
		depth: Math.random()
	};
};
Starfield.prototype.tick = function(){
	var f=this.f; if(!f) return;
	try { f.clear(); } catch(e){}
	for(var i=0;i<this.stars.length;i++){
		var s=this.stars[i];
		s.x -= s.speed;
		if(s.x < 1){
			s.x = this.f.width;
			s.y = 1 + Math.random()*this.f.height;
			s.speed = this.speedBase + Math.random()*this.speedBase;
			s.depth = Math.random();
		}
		var cx = Math.round(s.x);
		var cy = Math.round(s.y);
		if(cx < 1 || cy < 1 || cx > this.f.width || cy > this.f.height) continue;
		var brightness = (s.depth < 0.33) ? 0 : (s.depth < 0.66 ? 1 : 2);
		var ch = this.charSet.charAt(Math.min(brightness, this.charSet.length-1));
		this.f.gotoxy(cx, cy);
		this.f.attr = (this.palette[Math.min(brightness,this.palette.length-1)] || LIGHTGRAY) | BG_BLACK;
		this.f.putmsg(ch);
	}
	this.f.cycle();
};
Starfield.prototype.dispose = function(){ this.stars.length = 0; };

// Fireflies: wandering glowing dots
function Fireflies(){
	this.f=null; this.entities=[]; this.tickCount=0; this.palette=[LIGHTGREEN,LIGHTCYAN,LIGHTMAGENTA,YELLOW];
}
Fireflies.prototype.init = function(frame, opts){
	this.f=frame; opts = opts || {};
	var count = parseInt(opts.firefly_count, 10);
	if(isNaN(count) || count <= 0) count = Math.max(4, Math.round(Math.min(14, (frame.width+frame.height)/3)));
	this.entities.length = 0;
	for(var i=0;i<count;i++){
		this.entities.push({
			x: 1 + Math.random()*frame.width,
			y: 1 + Math.random()*frame.height,
			dx: (Math.random()*0.8)-0.4,
			dy: (Math.random()*0.6)-0.3,
			colorIndex: rand(0,this.palette.length-1)
		});
	}
	this.tickCount = 0;
};
Fireflies.prototype.tick = function(){
	var f=this.f; if(!f) return;
	this.tickCount++;
	try { f.clear(); } catch(e){}
	for(var i=0;i<this.entities.length;i++){
		var e=this.entities[i];
		if(Math.random() < 0.2){
			e.dx += (Math.random()*0.2)-0.1;
			e.dy += (Math.random()*0.2)-0.1;
			e.dx = Math.max(-0.7, Math.min(0.7, e.dx));
			e.dy = Math.max(-0.5, Math.min(0.5, e.dy));
		}
		e.x += e.dx;
		e.y += e.dy;
		if(e.x < 1){ e.x = 1; e.dx = Math.abs(e.dx); }
		if(e.x > f.width){ e.x = f.width; e.dx = -Math.abs(e.dx); }
		if(e.y < 1){ e.y = 1; e.dy = Math.abs(e.dy); }
		if(e.y > f.height){ e.y = f.height; e.dy = -Math.abs(e.dy); }
		if(this.tickCount % 18 === 0 && Math.random() < 0.6){
			e.colorIndex = (e.colorIndex + 1) % this.palette.length;
		}
		var cx = Math.round(e.x);
		var cy = Math.round(e.y);
		if(cx < 1 || cy < 1 || cx > f.width || cy > f.height) continue;
		f.gotoxy(cx, cy);
		f.attr = (this.palette[e.colorIndex] || LIGHTGREEN) | BG_BLACK;
		f.putmsg('*');
	}
	f.cycle();
};
Fireflies.prototype.dispose = function(){ this.entities.length = 0; };

// SineWave: sweeping sine wave glyphs
function SineWave(){
	this.f=null; this.phase=0; this.freq=1.2; this.amp=3; this.speed=0.3; this.char='~'; this.palette=[LIGHTBLUE,CYAN,LIGHTCYAN,WHITE];
}
SineWave.prototype.init = function(frame, opts){
	this.f=frame; opts = opts || {};
	if(opts.wave_frequency !== undefined){ var fq = parseFloat(opts.wave_frequency); if(!isNaN(fq) && fq > 0) this.freq = fq; }
	var maxAmp = Math.max(1, Math.floor(frame.height/2));
	if(opts.wave_amplitude !== undefined){
		var amp = parseFloat(opts.wave_amplitude);
		if(!isNaN(amp) && amp > 0) this.amp = Math.min(maxAmp, Math.max(1, amp));
	} else {
		this.amp = Math.max(1, Math.min(maxAmp, Math.floor(frame.height/2) - 1));
	}
	if(opts.wave_speed !== undefined){ var sp = parseFloat(opts.wave_speed); if(!isNaN(sp) && sp > 0) this.speed = sp; }
	if(opts.wave_char){ this.char = String(opts.wave_char).charAt(0) || this.char; }
	this.phase = 0;
};
SineWave.prototype.tick = function(){
	var f=this.f; if(!f) return;
	this.phase += this.speed;
	try { f.clear(); } catch(e){}
	var mid = Math.floor(f.height/2) || 1;
	for(var x=1;x<=f.width;x++){
		var angle = (x/Math.max(1,f.width)) * Math.PI * 2 * this.freq + this.phase;
		var y = Math.round(mid + Math.sin(angle) * this.amp);
		if(y < 1) y = 1; if(y > f.height) y = f.height;
		f.gotoxy(x, y);
		var idx = ((x + Math.floor(this.phase)) % this.palette.length + this.palette.length) % this.palette.length;
		f.attr = (this.palette[idx] || LIGHTBLUE) | BG_BLACK;
		f.putmsg(this.char);
	}
	f.cycle();
};
SineWave.prototype.dispose = function(){};

// CometTrails: bouncing glowing comets with fading trails
function CometTrails(){
	this.f=null; this.comets=[]; this.palette=[WHITE,LIGHTCYAN,LIGHTBLUE,LIGHTGRAY];
}
CometTrails.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	var count = parseInt(opts.comet_count, 10);
	if(isNaN(count) || count <= 0) count = Math.max(3, Math.floor((frame.width + frame.height) / 20));
	var speed = parseFloat(opts.comet_speed);
	if(isNaN(speed) || speed <= 0) speed = 0.8;
	this.comets.length = 0;
	for(var i=0;i<count;i++){
		var angle = Math.random() * Math.PI * 2;
		this.comets.push({
			x: 1 + Math.random()*frame.width,
			y: 1 + Math.random()*frame.height,
			dx: Math.cos(angle) * speed,
			dy: Math.sin(angle) * speed,
			trail: []
		});
	}
};
CometTrails.prototype._drawPoint = function(x,y,char,colorIdx){
	var f=this.f;
	var cx = Math.round(x);
	var cy = Math.round(y);
	if(cx < 1 || cy < 1 || cx > f.width || cy > f.height) return;
	f.gotoxy(cx, cy);
	f.attr = (this.palette[Math.min(colorIdx,this.palette.length-1)] || LIGHTGRAY) | BG_BLACK;
	f.putmsg(char);
};
CometTrails.prototype.tick = function(){
	var f=this.f; if(!f) return;
	try { f.clear(); } catch(e){}
	for(var i=0;i<this.comets.length;i++){
		var c=this.comets[i];
		c.x += c.dx;
		c.y += c.dy;
		if(c.x < 1){ c.x = 1; c.dx = Math.abs(c.dx); }
		if(c.x > f.width){ c.x = f.width; c.dx = -Math.abs(c.dx); }
		if(c.y < 1){ c.y = 1; c.dy = Math.abs(c.dy); }
		if(c.y > f.height){ c.y = f.height; c.dy = -Math.abs(c.dy); }
		c.trail.unshift({ x:c.x, y:c.y });
		if(c.trail.length > 10) c.trail.pop();
		for(var t=0;t<c.trail.length;t++){
			var entry = c.trail[t];
			var char = (t===0)?'@':(t<3?'O':(t<6?'+':'.'));
			this._drawPoint(entry.x, entry.y, char, t);
			if(t <= 1){
				this._drawPoint(entry.x+0.6, entry.y, t===0?'*':'+', t+1);
				this._drawPoint(entry.x-0.6, entry.y, t===0?'*':'+', t+1);
			}
		}
	}
	f.cycle();
};
CometTrails.prototype.dispose = function(){ this.comets.length = 0; };

// Plasma: colourful procedural plasma effect
function Plasma(){
	this.f=null; this.t=0; this.palette=[BLUE,LIGHTBLUE,LIGHTCYAN,CYAN,LIGHTMAGENTA,MAGENTA,LIGHTRED,YELLOW,WHITE];
}
Plasma.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	this.speed = (!isNaN(parseFloat(opts.plasma_speed))) ? parseFloat(opts.plasma_speed) : 0.18;
	this.scale = (!isNaN(parseFloat(opts.plasma_scale))) ? parseFloat(opts.plasma_scale) : 0.12;
	this.t = 0;
};
Plasma.prototype.tick = function(){
	var f=this.f; if(!f) return;
	this.t += this.speed;
	for(var y=1;y<=f.height;y++){
		for(var x=1;x<=f.width;x++){
			var nx = x*this.scale;
			var ny = y*this.scale;
			var val = Math.sin(nx + this.t) + Math.sin((ny + this.t*0.7)*1.3) + Math.sin(Math.sqrt(nx*nx + ny*ny) + this.t*0.4);
			var norm = (val + 3) / 6; // 0..1
			var paletteIndex = Math.min(this.palette.length-1, Math.max(0, Math.floor(norm * this.palette.length)));
			var ch = norm < 0.2 ? ' ' : norm < 0.4 ? '.' : norm < 0.6 ? '*' : norm < 0.8 ? 'o' : '@';
			f.setData(x-1, y-1, ch, (this.palette[paletteIndex] || LIGHTBLUE) | BG_BLACK, false);
		}
	}
	try { f.cycle(); } catch(e){}
};
Plasma.prototype.dispose = function(){};

// Fireworks: colourful bursting particles
function Fireworks(){
	this.f=null; this.bursts=[]; this.palette=[YELLOW,WHITE,LIGHTRED,LIGHTMAGENTA,LIGHTCYAN,LIGHTGREEN];
}
Fireworks.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	this.maxBursts = parseInt(opts.fireworks_bursts,10);
	if(isNaN(this.maxBursts) || this.maxBursts<=0) this.maxBursts = 4;
	this.gravity = (!isNaN(parseFloat(opts.fireworks_gravity))) ? parseFloat(opts.fireworks_gravity) : 0.08;
	this.spawnDelay = 0;
	this.bursts.length = 0;
};
Fireworks.prototype._spawnBurst = function(){
	var f=this.f;
	var count = 24 + Math.floor(Math.random()*22);
	var particles = [];
	for(var i=0;i<count;i++){
		var angle = (Math.PI * 2) * (i / count);
		var speed = 0.7 + Math.random()*1.3;
		particles.push({
			x: 1 + Math.random()*f.width,
			y: 1 + Math.random()*(f.height/2),
			dx: Math.cos(angle)*speed,
			dy: Math.sin(angle)*speed,
			life: 25 + Math.floor(Math.random()*18),
			colorIndex: Math.floor(Math.random()*this.palette.length)
		});
	}
	this.bursts.push({ particles: particles });
};
Fireworks.prototype.tick = function(){
	var f=this.f; if(!f) return;
	this.spawnDelay--;
	if(this.spawnDelay <= 0 && this.bursts.length < this.maxBursts){
		this._spawnBurst();
		this.spawnDelay = 18 + Math.floor(Math.random()*18);
	}
	try { f.clear(); } catch(e){}
	for(var b=this.bursts.length-1;b>=0;b--){
		var burst = this.bursts[b];
		var particles = burst.particles;
		for(var p=particles.length-1;p>=0;p--){
			var part = particles[p];
			part.x += part.dx;
			part.y += part.dy;
			part.dy += this.gravity;
			part.life--;
			if(part.life <= 0 || part.y > f.height+1){
				particles.splice(p,1);
				continue;
			}
			var intensity = Math.max(0, Math.min(this.palette.length-1, part.colorIndex + Math.floor((part.life/10))));
			var ch = (part.life > 20) ? '*' : (part.life > 10 ? '+' : '.');
			var cx = Math.round(part.x);
			var cy = Math.round(part.y);
			if(cx < 1 || cy < 1 || cx > f.width || cy > f.height) continue;
			f.gotoxy(cx, cy);
			f.attr = (this.palette[intensity] || YELLOW) | BG_BLACK;
			f.putmsg(ch);
		}
		if(!particles.length) this.bursts.splice(b,1);
	}
	try { f.cycle(); } catch(e){}
};
Fireworks.prototype.dispose = function(){ this.bursts.length = 0; };

// Aurora: flowing vertical light bands
function Aurora(){
	this.f=null; this.time=0; this.columns=[]; this.palette=[LIGHTCYAN,CYAN,LIGHTGREEN,LIGHTMAGENTA,YELLOW,WHITE];
}
Aurora.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	this.speed = (!isNaN(parseFloat(opts.aurora_speed))) ? parseFloat(opts.aurora_speed) : 0.12;
	this.wave = (!isNaN(parseFloat(opts.aurora_wave))) ? parseFloat(opts.aurora_wave) : 0.35;
	this.columns.length = 0;
	for(var x=0;x<frame.width;x++) this.columns.push(Math.random()*Math.PI*2);
	this.time = 0;
};
Aurora.prototype.tick = function(){
	var f=this.f; if(!f) return;
	this.time += this.speed;
	if(!this._auroraCache || this._auroraCache.length !== f.width){
		this._auroraCache = [];
		for(var cx=0; cx<f.width; cx++) this._auroraCache[cx] = [];
	}
	for(var x=1;x<=f.width;x++){
		var cacheCol = this._auroraCache[x-1];
		var phase = this.columns[x-1] + this.time;
		var center = Math.sin(phase) * (f.height/4) + (f.height/2);
		var thickness = (Math.cos(phase*0.7)+1) * (f.height/6) + 2;
		for(var y=1;y<=f.height;y++){
			var dist = Math.abs(y - center);
			if(dist > thickness){
				if(cacheCol[y-1]){
					f.clearData(x-1, y-1, false);
					cacheCol[y-1] = 0;
				}
				continue;
			}
			var norm = 1 - (dist / (thickness+0.01));
			var idx = Math.min(this.palette.length-1, Math.floor(norm * this.palette.length));
			var attr = (this.palette[idx] || LIGHTCYAN) | BG_BLACK;
			if(cacheCol[y-1] !== attr){
				f.setData(x-1, y-1, '|', attr, false);
				cacheCol[y-1] = attr;
			}
		}
	}
	if(typeof f.cycle === 'function') try { f.cycle(); } catch(e){}
};
Aurora.prototype.dispose = function(){ this.columns.length = 0; };

// OceanRipple: expanding circular waves
function OceanRipple(){
	this.f=null; this.ripples=[]; this.tickCount=0; this.maxRipples=4;
	this.palette=[LIGHTBLUE,CYAN,LIGHTCYAN,WHITE];
}
OceanRipple.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	this.ripples=[];
	this.tickCount=0;
	this.maxRipples = Math.max(2, parseInt(opts.ripple_count,10) || 4);
	for(var i=0;i<this.maxRipples;i++) this._spawn();
};
OceanRipple.prototype._spawn = function(){
	var f=this.f; if(!f) return;
	this.ripples.push({
		x: Math.random()*f.width,
		y: Math.random()*f.height,
		radius: 0,
		speed: 0.6 + Math.random()*0.5,
		max: Math.max(f.width, f.height) + 10
	});
};
OceanRipple.prototype.tick = function(){
	var f=this.f; if(!f) return;
	this.tickCount++;
	if(this.ripples.length < this.maxRipples && this.tickCount % 45 === 0) this._spawn();
	for(var r=this.ripples.length-1;r>=0;r--){
		var ripple=this.ripples[r];
		ripple.radius += ripple.speed;
		if(ripple.radius > ripple.max) this.ripples.splice(r,1);
	}
	for(var y=1;y<=f.height;y++){
		for(var x=1;x<=f.width;x++){
			var value = 0;
			for(var j=0;j<this.ripples.length;j++){
				var rp=this.ripples[j];
				var dx = x - rp.x;
				var dy = y - rp.y;
				var dist = Math.sqrt(dx*dx + dy*dy) + 0.01;
				var wave = Math.sin(dist*0.35 - rp.radius*0.5);
				value += wave / (1 + dist*0.15);
			}
			var norm = (value + 1.2)/2.4; // approx 0..1
			if(norm < 0) norm = 0; else if(norm>1) norm = 1;
			var idx = Math.min(this.palette.length-1, Math.floor(norm * this.palette.length));
			var ch = norm < 0.2 ? ' ' : norm < 0.4 ? '.' : norm < 0.6 ? '~' : norm < 0.8 ? '-' : '=';
			f.setData(x-1, y-1, ch, (this.palette[idx] || LIGHTBLUE) | BG_BLACK, false);
		}
	}
	try { f.cycle(); } catch(e){}
};
OceanRipple.prototype.dispose = function(){ this.ripples=[]; };

// LissajousTrails: flowing harmonic curves
function LissajousTrails(){
	this.f=null; this.time=0; this.curves=[]; this.trail=[]; this.speed=0.15;
}
LissajousTrails.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	this.speed = (!isNaN(parseFloat(opts.lissajous_speed))) ? parseFloat(opts.lissajous_speed) : 0.12;
	this.time = 0;
	this.trail=[];
	this.curves=[
		{ a:3, b:2, phase:0, color: LIGHTMAGENTA },
		{ a:4, b:5, phase:Math.PI/2, color: LIGHTCYAN },
		{ a:5, b:4, phase:Math.PI/3, color: YELLOW }
	];
};
LissajousTrails.prototype.tick = function(){
	var f=this.f; if(!f) return;
	this.time += this.speed;
	var midX = (f.width-1)/2;
	var midY = (f.height-1)/2;
	var ampX = Math.max(1, f.width/2 - 2);
	var ampY = Math.max(1, f.height/2 - 2);
	for(var i=0;i<this.curves.length;i++){
		var c=this.curves[i];
		var x = midX + Math.sin(this.time*c.a + c.phase) * ampX;
		var y = midY + Math.cos(this.time*c.b + c.phase) * ampY;
		this.trail.push({ x:x, y:y, color:c.color, life:28 });
	}
	try { f.clear(); } catch(e){}
	for(var t=this.trail.length-1; t>=0; t--){
		var point=this.trail[t];
		point.life -= 1.2;
		if(point.life <= 0){ this.trail.splice(t,1); continue; }
		var sx = Math.round(point.x);
		var sy = Math.round(point.y);
		if(sx < 0 || sy < 0 || sx >= f.width || sy >= f.height) continue;
		var norm = point.life/28;
		var ch = norm > 0.7 ? '@' : norm > 0.45 ? 'o' : norm > 0.2 ? '+' : '.';
		var attr = (point.color || LIGHTMAGENTA) | BG_BLACK;
		if(norm < 0.25) attr = (point.color === YELLOW ? LIGHTGRAY : point.color) | BG_BLACK;
		f.setData(sx, sy, ch, attr, false);
	}
	try { f.cycle(); } catch(e){}
};
LissajousTrails.prototype.dispose = function(){ this.trail=[]; };

// LightningStorm: jagged bolts with afterglow
function LightningStorm(){
	this.f=null; this.w=0; this.h=0;
	this.fade=[]; this.charGrid=[]; this.bolts=[]; this.cooldown=0;
}
LightningStorm.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	this.w = frame.width;
	this.h = frame.height;
	this.cooldown = 0;
	this.fade=[]; this.charGrid=[]; this.bolts=[];
	for(var y=0;y<this.h;y++){
		var fadeRow = []; var charRow=[];
		for(var x=0;x<this.w;x++){ fadeRow.push(0); charRow.push(' '); }
		this.fade.push(fadeRow); this.charGrid.push(charRow);
	}
};
LightningStorm.prototype._spawnBolt = function(){
	var path=[];
	var x=Math.floor(Math.random()*this.w);
	var y=0;
	path.push({x:x,y:y});
	while(y < this.h-1){
		y += (Math.random()<0.25)?2:1;
		if(y >= this.h) y = this.h-1;
		x += (Math.floor(Math.random()*3)-1);
		if(x<0) x=0; else if(x>=this.w) x=this.w-1;
		path.push({x:x,y:y});
	}
	this.bolts.push({ path:path, life:6 });
};
LightningStorm.prototype.tick = function(){
	var f=this.f; if(!f) return;
	if(this.cooldown <= 0){
		this._spawnBolt();
		this.cooldown = 15 + Math.floor(Math.random()*25);
	}else this.cooldown--;
	for(var y=0;y<this.h;y++){
		for(var x=0;x<this.w;x++){
			var val = this.fade[y][x] - 6;
			if(val < 0){ val = 0; if(this.charGrid[y][x] !== ' ') this.charGrid[y][x] = ' '; }
			this.fade[y][x] = val;
		}
	}
	for(var b=this.bolts.length-1;b>=0;b--){
		var bolt=this.bolts[b];
		bolt.life--;
		for(var i=0;i<bolt.path.length;i++){
			var seg=bolt.path[i];
			var x=seg.x, y=seg.y;
			if(x<0||y<0||x>=this.w||y>=this.h) continue;
			var prev = (i>0)?bolt.path[i-1]:null;
			var ch='|';
			if(prev){
				var dx = seg.x - prev.x;
				if(dx > 0) ch = '/';
				else if(dx < 0) ch = '\\';
			}
			this.charGrid[y][x] = ch;
			this.fade[y][x] = 255;
			if(y+1 < this.h) this.fade[y+1][x] = Math.max(this.fade[y+1][x], 120);
			if(x>0) this.fade[y][x-1] = Math.max(this.fade[y][x-1], 80);
			if(x+1 < this.w) this.fade[y][x+1] = Math.max(this.fade[y][x+1], 80);
		}
		if(bolt.life <= 0) this.bolts.splice(b,1);
	}
	for(var yy=0; yy<this.h; yy++){
		for(var xx=0; xx<this.w; xx++){
			var intensity = this.fade[yy][xx];
			var ch = this.charGrid[yy][xx];
			if(intensity <= 0){
				f.setData(xx, yy, ' ', BG_BLACK|BLACK, false);
				continue;
			}
			var attr = (intensity > 180) ? WHITE|BG_BLACK
				: intensity > 120 ? LIGHTCYAN|BG_BLACK
				: intensity > 60 ? CYAN|BG_BLACK
				: LIGHTGRAY|BG_BLACK;
			f.setData(xx, yy, ch, attr, false);
		}
	}
	try { f.cycle(); } catch(e){}
};
LightningStorm.prototype.dispose = function(){ this.fade=[]; this.charGrid=[]; this.bolts=[]; };

// RecursiveTunnel: infinite tunnel illusion
function RecursiveTunnel(){
	this.f=null; this.time=0; this.speed=0.25; this.scale=0.15;
	this.palette=[LIGHTBLUE,CYAN,LIGHTMAGENTA,LIGHTGRAY,WHITE];
}
RecursiveTunnel.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	this.speed = (!isNaN(parseFloat(opts.tunnel_speed))) ? parseFloat(opts.tunnel_speed) : 0.22;
	this.scale = (!isNaN(parseFloat(opts.tunnel_scale))) ? parseFloat(opts.tunnel_scale) : 0.17;
	this.time = 0;
};
RecursiveTunnel.prototype.tick = function(){
	var f=this.f; if(!f) return;
	this.time += this.speed;
	var cx = (f.width-1)/2;
	var cy = (f.height-1)/2;
	for(var y=1;y<=f.height;y++){
		for(var x=1;x<=f.width;x++){
			var dx = x-1 - cx;
			var dy = y-1 - cy;
			var dist = Math.sqrt(dx*dx + dy*dy) * this.scale;
			var angle = Math.atan2(dy, dx);
			var band = Math.sin(dist - this.time) * 0.5 + 0.5;
			var twist = Math.sin(angle*3 + this.time*0.7) * 0.5 + 0.5;
			var mix = (band + twist) / 2;
			if(mix < 0) mix = 0; else if(mix > 1) mix = 1;
			var idx = Math.min(this.palette.length-1, Math.floor(mix * this.palette.length));
			var ch = mix > 0.8 ? '#'
				: mix > 0.6 ? '='
				: mix > 0.4 ? '-'
				: mix > 0.25 ? '+'
				: mix > 0.1 ? '.' : ' ';
			f.setData(x-1, y-1, ch, (this.palette[idx] || LIGHTBLUE) | BG_BLACK, false);
		}
	}
	try { f.cycle(); } catch(e){}
};
RecursiveTunnel.prototype.dispose = function(){};

// FigletMessage: render text using FIGlet/tdfonts

var FIGLET_SHADE_CODES = [176, 177, 178];
function _figletBuildColorPalette(){
    var candidates = [
        (typeof BLUE !== 'undefined' ? BLUE : undefined),
        (typeof GREEN !== 'undefined' ? GREEN : undefined),
        (typeof CYAN !== 'undefined' ? CYAN : undefined),
        (typeof RED !== 'undefined' ? RED : undefined),
        (typeof MAGENTA !== 'undefined' ? MAGENTA : undefined),
        (typeof BROWN !== 'undefined' ? BROWN : undefined),
        (typeof LIGHTGRAY !== 'undefined' ? LIGHTGRAY : undefined),
        (typeof LIGHTBLUE !== 'undefined' ? LIGHTBLUE : undefined),
        (typeof LIGHTGREEN !== 'undefined' ? LIGHTGREEN : undefined),
        (typeof LIGHTCYAN !== 'undefined' ? LIGHTCYAN : undefined),
        (typeof LIGHTRED !== 'undefined' ? LIGHTRED : undefined),
        (typeof LIGHTMAGENTA !== 'undefined' ? LIGHTMAGENTA : undefined),
        (typeof YELLOW !== 'undefined' ? YELLOW : undefined),
        (typeof WHITE !== 'undefined' ? WHITE : undefined)
    ];
    var palette = [];
    for(var i=0;i<candidates.length;i++)
    {
        var c = candidates[i];
        if(typeof c !== 'number') continue;
        if(typeof BLACK === 'number' && c === BLACK) continue;
        if(typeof DARKGRAY === 'number' && c === DARKGRAY) continue;
        if(typeof LIGHTGRAY === 'number' && c === LIGHTGRAY) continue;
        if(palette.indexOf(c) === -1) palette.push(c);
    }
    if(!palette.length && typeof WHITE === 'number') palette.push(WHITE);
    return palette;
}
function _figletBuildBackgroundPalette(){
    var candidates = [
        (typeof BG_BLUE !== 'undefined' ? BG_BLUE : undefined),
        (typeof BG_GREEN !== 'undefined' ? BG_GREEN : undefined),
        (typeof BG_CYAN !== 'undefined' ? BG_CYAN : undefined),
        (typeof BG_RED !== 'undefined' ? BG_RED : undefined),
        (typeof BG_MAGENTA !== 'undefined' ? BG_MAGENTA : undefined),
        (typeof BG_BROWN !== 'undefined' ? BG_BROWN : undefined)
    ];
    var palette = [];
    for(var i=0;i<candidates.length;i++)
    {
        var bg = candidates[i];
        if(typeof bg !== 'number') continue;
        if(palette.indexOf(bg) === -1) palette.push(bg);
    }
    if(!palette.length && typeof BG_BLUE === 'number') palette.push(BG_BLUE);
    return palette;
}

function FigletMessage(){
	this.f = null;
	this.tdf = null;
	this.fontPool = [];
	this.messages = [];
	this.lines = [];
	this.refreshTicks = 180;
	this.tickCounter = 0;
	this.box = null;
	this.inner = null;
	this.colorPalette = _figletBuildColorPalette();
	this._shadeBackgrounds = _figletBuildBackgroundPalette();
	this.colorsEnabled = true;
	this.moveEnabled = true;
	this.dx = 0.45;
	this.dy = 0.25;
	this.posX = null;
	this.posY = null;
	this.contentWidth = 10;
	this.contentHeight = 1;
	this.currentColorIndex = -1;
	this.currentColor = (typeof WHITE === 'number') ? WHITE : 7;
	this._colorQueue = [];
	this.borderEnabled = false;
	this._dirty = false;
	this._prevLines = [];
	this._attrCache = [];
	this.baseFg = (typeof WHITE === 'number') ? WHITE : 7;
	this.baseBg = 0;
}
FigletMessage.prototype.init = function(frame, opts){
	this.f = frame;
	opts = opts || {};
	// Clear frame at init to remove any artifacts from previous animations
	// This is critical for transparent overlay frames
	try {
		if(typeof frame.invalidate === 'function') frame.invalidate();
		if(typeof frame.clear === 'function') frame.clear();
		if(typeof frame.cycle === 'function') frame.cycle();
	} catch(_){}
	this.tdf = load('tdfonts_lib.js');
	if(!this.tdf.opt) this.tdf.opt = {};
	this.tdf.opt.width = frame.width;
	this.tdf.opt.justify = this.tdf.CENTER_JUSTIFY;
	this.tdf.opt.margin = 0;
	this.tdf.opt.wrap = false;
	this.tdf.opt.blankline = false;
	this.tdf.opt.ansi = false;
	this.tdf.opt.random = false;
	this.messages = this._parseMessages(opts);
	this.fontPool = this._parseFonts(opts);
	this.colorsEnabled = opts.figlet_colors !== false;
	this.moveEnabled = opts.figlet_move !== false;
	if(opts.figlet_border === true || opts.figlet_border === 'true') this.borderEnabled = true;
	else if(opts.figlet_border === false || opts.figlet_border === 'false') this.borderEnabled = false;
	var customDx = parseFloat(opts.figlet_dx);
	var customDy = parseFloat(opts.figlet_dy);
	if(!isNaN(customDx)) this.dx = customDx || 0;
	if(!isNaN(customDy)) this.dy = customDy || 0;
	if(!this.moveEnabled){ this.dx = 0; this.dy = 0; }
	if(this.moveEnabled && this.dx === 0) this.dx = 0.45;
	if(this.moveEnabled && this.dy === 0) this.dy = 0.25;
	var refresh = parseInt(opts.figlet_refresh, 10);
	if(!isNaN(refresh) && refresh > 0) this.refreshTicks = refresh;
	else this.refreshTicks = 180;
	this.tickCounter = 0;
	this._renderNew(false);
	this._draw();
};
FigletMessage.prototype._parseMessages = function(opts){
	var list = [];
	if(typeof opts.figlet_messages === 'string' && opts.figlet_messages.trim()){
		list = opts.figlet_messages.split('|').map(function(s){ return s.trim(); }).filter(Boolean);
	}
	if(!list.length && typeof opts.figlet_message === 'string' && opts.figlet_message.trim())
		list = [opts.figlet_message.trim()];
	if(!list.length) list = ['Future Land'];
	return list;
};
FigletMessage.prototype._parseFonts = function(opts){
	var fonts = [];
	if(typeof opts.figlet_fonts === 'string' && opts.figlet_fonts.trim()){
		fonts = opts.figlet_fonts.split(',').map(function(s){ return s.trim(); }).filter(Boolean);
	}
	if(!fonts.length){
		var files = this.tdf.getlist() || [];
		for(var i=0;i<files.length;i++) fonts.push(file_getname(files[i]));
	}
	if(!fonts.length && this.tdf.DEFAULT_FONT) fonts.push(this.tdf.DEFAULT_FONT);
	return fonts;
};
FigletMessage.prototype._pickMessage = function(){
	if(!this.messages.length) return 'HELLO WORLD';
	var idx = random(this.messages.length);
	if(idx >= this.messages.length) idx = this.messages.length - 1;
	return this.messages[idx];
};
FigletMessage.prototype._pickFont = function(){
	if(!this.fontPool.length) return this.tdf.DEFAULT_FONT || 'brndamgx';
	var idx = random(this.fontPool.length);
	if(idx >= this.fontPool.length) idx = this.fontPool.length - 1;
	return this.fontPool[idx];
};
FigletMessage.prototype._sanitizeLines = function(text){
	if(!text) return [];
	var plain = text.replace(/\r/g,'').replace(/\x1B\[[0-9;]*m/g,'');
	plain = plain.replace(/\x01./g,'');
	var lines = plain.split('\n');
	while(lines.length && lines[lines.length-1].trim()==='') lines.pop();
	return lines;
};
FigletMessage.prototype._measureContent = function(){
	var lines = this.lines || [];
	var maxLen = 0;
	for(var i=0;i<lines.length;i++){
		var ln = lines[i].replace(/\s+$/,'');
		if(ln.length > maxLen) maxLen = ln.length;
	}
	this.contentHeight = Math.max(1, lines.length);
	var limit = this._maxInnerWidth();
	this.contentWidth = Math.max(1, Math.min(limit, maxLen || 10));
};
FigletMessage.prototype._renderNew = function(preserve){
	var message = this._pickMessage();
	var maxWidth = this._maxInnerWidth();
	var maxHeight = this._maxInnerHeight();
	var tried = {};
	var attemptLimit = Math.max(1, Math.min((this.fontPool && this.fontPool.length) || 1, 8));
	var bestLines = null;
	var bestHeight = Infinity;
	var attempts = 0;
	while(attempts < attemptLimit){
		var fontName = this._pickFont();
		if(!fontName) fontName = this.tdf.DEFAULT_FONT || 'brndamgx';
		if(tried[fontName]) continue;
		tried[fontName] = true;
		attempts++;
		var fontLines = this._renderLinesForFont(message, fontName);
		if(!fontLines || !fontLines.length) fontLines = [message];
		var wrapped = this._wrapFigletLines(fontLines, maxWidth);
		if(!wrapped.length) wrapped = [message];
		var height = wrapped.length;
		if(height <= maxHeight){
			bestLines = wrapped;
			bestHeight = height;
			break;
		}
		if(height < bestHeight){
			bestLines = wrapped;
			bestHeight = height;
		}
	}
	if(!bestLines || bestHeight > maxHeight){
		bestLines = this._wrapPlainMessage(message, maxWidth, maxHeight);
	}
	if(!bestLines.length) bestLines = [message];
	this.lines = bestLines;
	this._selectColor();
	this._measureContent();
	this._createFrames(preserve === true);
	this._dirty = true;
};
FigletMessage.prototype._maxInnerWidth = function(){
	if(!this.f) return 40;
	return Math.max(1, this.f.width - 6);
};
FigletMessage.prototype._maxInnerHeight = function(){
	if(!this.f) return 10;
	return Math.max(1, this.f.height - 2);
};
FigletMessage.prototype._renderLinesForFont = function(message, fontName){
	var fontObj = fontName;
	try {
		fontObj = this.tdf.loadfont(fontName);
	} catch(e){
		try { log(LOG_WARNING, 'figlet loadfont failed for '+fontName+': '+e); } catch(_){}
		fontObj = fontName;
	}
	try {
		this.tdf.opt.width = this._maxInnerWidth();
		var output = this.tdf.output(message, fontObj);
		var lines = this._sanitizeLines(output);
		return lines.length ? lines : [message];
	} catch(e){
		try { log(LOG_WARNING, 'figlet render failed ('+fontName+'): '+e); } catch(_){}
		return [message];
	}
};
FigletMessage.prototype._wrapFigletLines = function(lines, maxWidth){
	if(!lines || !lines.length || maxWidth < 1) return [];
	var cleaned = [];
	var maxLen = 0;
	for(var i=0;i<lines.length;i++){
		var trimmed = lines[i].replace(/\s+$/,'');
		cleaned.push(trimmed);
		if(trimmed.length > maxLen) maxLen = trimmed.length;
	}
	if(maxLen <= maxWidth) return cleaned;
	var chunks = [];
	var height = cleaned.length;
	for(var start=0; start<maxLen; start+=maxWidth){
		for(var row=0; row<height; row++){
			var segment = cleaned[row].substr(start, maxWidth);
			chunks.push(segment.replace(/\s+$/,''));
		}
	}
	while(chunks.length && chunks[chunks.length-1].trim()==='') chunks.pop();
	return chunks;
};
FigletMessage.prototype._wrapPlainMessage = function(text, maxWidth, maxHeight){
	var width = Math.max(1, maxWidth || 1);
	var heightLimit = Math.max(1, maxHeight || 1);
	var words = String(text || '').split(/\s+/);
	var lines = [];
	var line = '';
	for(var i=0;i<words.length;i++){
		var word = words[i];
		if(!word) continue;
		if(word.length >= width){
			if(line){
				lines.push(line);
				if(lines.length >= heightLimit) return lines.slice(0, heightLimit);
				line = '';
			}
			for(var start=0; start<word.length && lines.length < heightLimit; start+=width){
				lines.push(word.substr(start, width));
			}
			if(lines.length >= heightLimit) return lines.slice(0, heightLimit);
			continue;
		}
		var candidate = line ? (line + ' ' + word) : word;
		if(candidate.length > width){
			if(line){
				lines.push(line);
				if(lines.length >= heightLimit) return lines.slice(0, heightLimit);
			}
			line = word;
		} else {
			line = candidate;
		}
	}
	if(line && lines.length < heightLimit) lines.push(line);
	if(!lines.length) lines.push(String(text || '').substr(0, width));
	return lines.slice(0, heightLimit);
};
FigletMessage.prototype._selectColor = function(){
	if(!this.colorsEnabled){
		this.baseFg = (typeof WHITE === 'number') ? WHITE : 7;
		this.baseBg = 0;
		this._dirty = true;
		return;
	}
	if(!this.colorPalette || !this.colorPalette.length){
		this.baseFg = (typeof WHITE === 'number') ? WHITE : 7;
		this.baseBg = 0;
		this._dirty = true;
		return;
	}
	if(!this._colorQueue || !this._colorQueue.length){
		this._colorQueue = this.colorPalette.slice();
		for(var i=this._colorQueue.length-1;i>0;i--){
			var j=Math.floor(Math.random()*(i+1));
			var tmp=this._colorQueue[i]; this._colorQueue[i]=this._colorQueue[j]; this._colorQueue[j]=tmp;
		}
	}
	var next = this._colorQueue.shift();
	if(typeof next !== 'number') next = this.colorPalette[0];
	this.baseFg = next;
	this.baseBg = this._pickShadeBackground(this.baseFg);
	this._dirty = true;
};
FigletMessage.prototype._pickShadeBackground = function(fg){
    var pool = (this._shadeBackgrounds && this._shadeBackgrounds.length) ? this._shadeBackgrounds : _figletBuildBackgroundPalette();
    if(!pool.length) return 0;
    var baseFg = fg & 0x07;
    for(var i=0;i<pool.length;i++){
        var bg = pool[i];
        var baseBg = (bg >> 4) & 0x0F;
        if(baseBg !== baseFg) return bg;
    }
    return pool[0];
};

FigletMessage.prototype._getCharAttr = function(ch){
    var fg = this.colorsEnabled ? (this.baseFg || ((typeof WHITE === 'number') ? WHITE : 7)) : ((typeof WHITE === 'number') ? WHITE : 7);
    var attr = fg | BG_BLACK;
    if(Array.isArray(FIGLET_SHADE_CODES) && this.colorsEnabled && ch && ch.charCodeAt){
        var code = ch.charCodeAt(0);
        if(FIGLET_SHADE_CODES.indexOf(code) !== -1){
            attr = (this.baseFg || fg) | (this.baseBg || 0);
        }
    }
    return attr;
};

FigletMessage.prototype._isTransparentChar = function(ch, attr){
	if(!ch || ch === ' ') return true;
	if(typeof attr !== 'number') return false;
	var fgNibble = attr & 0x0F;
	var bgNibble = (attr >> 4) & 0x0F;
	var blackNibble = (typeof BLACK === 'number') ? (BLACK & 0x0F) : 0;
	var bgBlackNibble = (typeof BG_BLACK === 'number') ? ((BG_BLACK >> 4) & 0x0F) : blackNibble;
	if(fgNibble === blackNibble && bgNibble === bgBlackNibble) return true;
	return false;
};

FigletMessage.prototype._clearFrame = function(frame){
    if(!frame) return;
    if(typeof frame.clear === 'function'){
        try { frame.clear(); } catch(e_clear){}
        return;
    }
    var w = frame.width || 0;
    var h = frame.height || 0;
    for(var y=0; y<h; y++){
        for(var x=0; x<w; x++){
            var refresh = (x === w-1 && y === h-1);
            try { frame.clearData(x, y, refresh); } catch(e_clearData){
                try { frame.setData(x, y, ' ', 0, refresh); } catch(e_setData){}
            }
        }
    }
};

FigletMessage.prototype._createFrames = function(preservePosition){
	var prevX = this.posX;
	var prevY = this.posY;
	if(this.box){
		if(this.inner){ try { this.inner.close(); } catch(e_inner){} }
		try { this.box.close(); } catch(e_box){}
		this.box = null;
		this.inner = null;
		this._clearFrame(this.f);
		if(this.f && typeof this.f.cycle === 'function'){
			try { this.f.cycle(); } catch(_c) {}
		}
	}
	var innerHeight = Math.min(Math.max(1, this.contentHeight), Math.max(1, this.f.height - 2));
	var innerWidth = Math.min(Math.max(1, this.contentWidth), Math.max(1, this.f.width - 2));
	var boxHeight = Math.min(this.f.height, Math.max(3, innerHeight + 2));
	var boxWidth = Math.min(this.f.width, Math.max(4, innerWidth + 2));
	this.box = new Frame(1, 1, boxWidth, boxHeight, BG_BLACK|LIGHTGRAY, this.f);
	this.box.transparent = true;
	if(this.borderEnabled && typeof this.box.drawBorder === 'function') this.box.drawBorder();
	if(typeof this.box.open === 'function') this.box.open();
	else if(typeof this.box.show === 'function') this.box.show();
	if(typeof this.box.top === 'function') this.box.top();
	var innerH = Math.max(1, boxHeight - 2);
	this.inner = new Frame(2, 2, Math.max(1, boxWidth-2), innerH, BG_BLACK|WHITE, this.box);
	this.inner.transparent = true;
	if(typeof this.inner.open === 'function') this.inner.open();
	else if(typeof this.inner.show === 'function') this.inner.show();
	var maxX = Math.max(1, this.f.width - boxWidth + 1);
	var maxY = Math.max(1, this.f.height - boxHeight + 1);
	if(preservePosition && typeof prevX === 'number') this.posX = Math.min(Math.max(1, prevX), maxX);
	else this.posX = Math.max(1, Math.floor((this.f.width - boxWidth)/2) + 1);
	if(preservePosition && typeof prevY === 'number') this.posY = Math.min(Math.max(1, prevY), maxY);
	else this.posY = Math.max(1, Math.floor((this.f.height - boxHeight)/2) + 1);
	this.box.moveTo(Math.round(this.posX), Math.round(this.posY));
	this._prevLines = [];
	this._attrCache = [];
	this._dirty = true;
};
FigletMessage.prototype._updatePosition = function(){
	if(!this.moveEnabled || !this.box) return;
	var maxX = Math.max(1, this.f.width - this.box.width + 1);
	var maxY = Math.max(1, this.f.height - this.box.height + 1);
	this.posX += this.dx;
	this.posY += this.dy;
	if(this.posX < 1){ this.posX = 1; this.dx = Math.abs(this.dx); }
	else if(this.posX > maxX){ this.posX = maxX; this.dx = -Math.abs(this.dx); }
	if(this.posY < 1){ this.posY = 1; this.dy = Math.abs(this.dy); }
	else if(this.posY > maxY){ this.posY = maxY; this.dy = -Math.abs(this.dy); }
	this.box.moveTo(Math.round(this.posX), Math.round(this.posY));
};
FigletMessage.prototype._draw = function(){
	if(!this.box || !this.inner) return;
	var frame = this.inner;
	var width = frame.width;
	var height = frame.height;
	var lines = this.lines || [];
	var maxRows = Math.min(lines.length, height);
	if(!this._prevLines || this._prevLines.length !== height){
		this._prevLines = new Array(height);
	}
	if(!this._attrCache || this._attrCache.length !== height){
		this._attrCache = new Array(height);
	}
	var anyChanges = false;
	if(this._dirty){
		for(var row = 0; row < height; row++){
			var rowChars = this._prevLines[row];
			if(!rowChars || rowChars.length !== width){
				rowChars = new Array(width);
				this._prevLines[row] = rowChars;
			}
			var attrRow = this._attrCache[row];
			if(!attrRow || attrRow.length !== width){
				attrRow = new Array(width);
				this._attrCache[row] = attrRow;
			}
			var rawLine = (row < maxRows) ? (lines[row] || '') : '';
			if(rawLine.length > width) rawLine = rawLine.substr(0, width);
			var trimmed = rawLine.replace(/\s+$/,'');
			var start = Math.max(0, Math.floor((width - trimmed.length) / 2));
			for(var col = 0; col < width; col++){
				var newChar = ' ';
				var newAttr = null;
				var within = (col >= start) && (col - start < trimmed.length);
				if(within){
					newChar = trimmed.charAt(col - start);
					newAttr = this._getCharAttr(newChar);
					if(this._isTransparentChar(newChar, newAttr)){
						newChar = ' ';
						newAttr = null;
					}
				}
				var prevChar = rowChars[col];
				var prevAttr = attrRow[col];
				if(prevChar === newChar && prevAttr === newAttr) continue;
				anyChanges = true;
				if(newChar === ' '){
					try { frame.clearData(col, row, false); } catch(_){}
					rowChars[col] = ' ';
					attrRow[col] = null;
				} else {
					try { frame.setData(col, row, newChar, newAttr, false); } catch(_){}
					rowChars[col] = newChar;
					attrRow[col] = newAttr;
				}
			}
		}
	}
	try {
		if(anyChanges || this._dirty) frame.cycle();
		this.box.cycle();
	} catch(_){ }
	this._dirty = false;
};
FigletMessage.prototype.tick = function(){
	if(++this.tickCounter >= this.refreshTicks){
		this._renderNew(true);
		this.tickCounter = 0;
	}
	this._updatePosition();
	this._draw();
};
FigletMessage.prototype.dispose = function(){
	if(this.inner){
		try { this.inner.clear(); } catch(e){}
		try { this.inner.close(); } catch(e){}
	}
	if(this.box){
		try { this.box.clear(); } catch(e){}
		try { this.box.close(); } catch(e){}
	}
	// Invalidate parent to force full redraw after transparent child frames are closed
	if(this.f){
		try { if(typeof this.f.invalidate === 'function') this.f.invalidate(); } catch(_){}
	}
	this._clearFrame(this.f);
	if(this.f && typeof this.f.cycle === 'function'){
		try { this.f.cycle(); } catch(_c){}
	}
	this.box = this.inner = null;
	this.lines = [];
};

// FireSmoke: classic fire column with drifting smoke
function FireSmoke(){
	this.f=null; this.w=0; this.h=0;
	this.buffer=[]; this.smoke=[];
	this.decay=1;
	this.gradient=[
		{ threshold:20, ch:' ', attr: BG_BLACK|BLACK },
		{ threshold:70, ch:'.', attr: BG_BLACK|DARKGRAY },
		{ threshold:120, ch:'`', attr: BG_RED|YELLOW },
		{ threshold:170, ch:'^', attr: BG_RED|WHITE },
	];
}
FireSmoke.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	this.w = frame.width;
	this.h = frame.height;
	this.decay = Math.max(1, parseInt(opts.fire_decay,10) || 1);
	this.buffer = [];
	for(var y=0;y<this.h+2;y++){
		var row=[];
		for(var x=0;x<this.w;x++) row.push(0);
		this.buffer.push(row);
	}
	this.smoke=[];
};
FireSmoke.prototype._sample = function(row,x){
	var w=this.w;
	if(x<0) x=0; else if(x>=w) x=w-1;
	if(row<0) row=0; else if(row>=this.buffer.length) row=this.buffer.length-1;
	return this.buffer[row][x];
};
FireSmoke.prototype._findGradient = function(value){
	var grad=this.gradient;
	for(var i=0;i<grad.length;i++) if(value <= grad[i].threshold) return grad[i];
	return grad[grad.length-1];
};
FireSmoke.prototype.tick = function(){
	var f=this.f; if(!f) return;
	var w=this.w, h=this.h;
	var buf=this.buffer;
	var bottomIndex = this.h+1;
	for(var x=0;x<w;x++) buf[bottomIndex][x] = (Math.random() < 0.22) ? 255 : 0;
	for(var y=h; y>0; y--){
		var dest = buf[y-1];
		for(var x=0;x<w;x++){
			var val = (
				this._sample(y, x) +
				this._sample(y, x-1) +
				this._sample(y, x+1) +
				this._sample(y+1, x)
				) / 4;
			val -= this.decay + ((Math.random()*3)|0);
			if(val < 0) val = 0;
			dest[x] = val;
			if(val > 200 && y < h-4 && Math.random() < 0.035){
				this.smoke.push({ x:x + Math.random()*0.6 - 0.3, y:y-1, life: 18 });
			}
		}
	}
	for(var sy=this.smoke.length-1; sy>=0; sy--){
		var s=this.smoke[sy];
		s.y -= 0.25 + Math.random()*0.15;
		s.x += (Math.random()*0.5) - 0.25;
		s.life--;
		if(s.y < 0 || s.x < -1 || s.x > w || s.life <= 0){
			this.smoke.splice(sy,1);
		}
	}
	for(var yy=0; yy<h; yy++){
		for(var xx=0; xx<w; xx++){
			var val = buf[yy][xx];
			var grad = this._findGradient(val);
			f.setData(xx, yy, grad.ch, grad.attr, false);
		}
	}
	for(var i=0;i<this.smoke.length;i++){
		var s2=this.smoke[i];
		var sx=Math.round(s2.x);
		var sy2=Math.round(s2.y);
		if(sx>=0 && sx<w && sy2>=0 && sy2<h){
			var ch = (s2.life>12)?'~':(s2.life>6?'.':' ');
			var attr = (s2.life>6 ? LIGHTGRAY : DARKGRAY) | BG_BLACK;
			f.setData(sx, sy2, ch, attr, false);
		}
	}
	try { f.cycle(); } catch(e){}
};
FireSmoke.prototype.dispose = function(){ this.buffer=[]; this.smoke=[]; };

// OceanRipple: expanding circular waves
function OceanRipple(){
	this.f=null; this.ripples=[]; this.tickCount=0; this.maxRipples=4;
	this.palette=[LIGHTBLUE,CYAN,LIGHTCYAN,WHITE];
}
OceanRipple.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	this.ripples=[];
	this.tickCount=0;
	this.maxRipples = Math.max(2, parseInt(opts.ripple_count,10) || 4);
	for(var i=0;i<this.maxRipples;i++) this._spawn();
};
OceanRipple.prototype._spawn = function(){
	var f=this.f; if(!f) return;
	this.ripples.push({
		x: Math.random()*f.width,
		y: Math.random()*f.height,
		radius: 0,
		speed: 0.6 + Math.random()*0.5,
		max: Math.max(f.width, f.height) + 10
	});
};
OceanRipple.prototype.tick = function(){
	var f=this.f; if(!f) return;
	this.tickCount++;
	if(this.ripples.length < this.maxRipples && this.tickCount % 45 === 0) this._spawn();
	for(var r=this.ripples.length-1;r>=0;r--){
		var ripple=this.ripples[r];
		ripple.radius += ripple.speed;
		if(ripple.radius > ripple.max) this.ripples.splice(r,1);
	}
	for(var y=1;y<=f.height;y++){
		for(var x=1;x<=f.width;x++){
			var value = 0;
			for(var j=0;j<this.ripples.length;j++){
				var rp=this.ripples[j];
				var dx = x - rp.x;
				var dy = y - rp.y;
				var dist = Math.sqrt(dx*dx + dy*dy) + 0.01;
				var wave = Math.sin(dist*0.35 - rp.radius*0.5);
				value += wave / (1 + dist*0.15);
			}
			var norm = (value + 1.2)/2.4; // approx 0..1
			if(norm < 0) norm = 0; else if(norm>1) norm = 1;
			var idx = Math.min(this.palette.length-1, Math.floor(norm * this.palette.length));
			var ch = norm < 0.2 ? ' ' : norm < 0.4 ? '.' : norm < 0.6 ? '~' : norm < 0.8 ? '-' : '=';
			f.setData(x-1, y-1, ch, (this.palette[idx] || LIGHTBLUE) | BG_BLACK, false);
		}
	}
	try { f.cycle(); } catch(e){}
};
OceanRipple.prototype.dispose = function(){ this.ripples=[]; };

// LissajousTrails: flowing harmonic curves
function LissajousTrails(){
	this.f=null; this.time=0; this.curves=[]; this.trail=[]; this.speed=0.15;
}
LissajousTrails.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	this.speed = (!isNaN(parseFloat(opts.lissajous_speed))) ? parseFloat(opts.lissajous_speed) : 0.12;
	this.time = 0;
	this.trail=[];
	this.curves=[
		{ a:3, b:2, phase:0, color: LIGHTMAGENTA },
		{ a:4, b:5, phase:Math.PI/2, color: LIGHTCYAN },
		{ a:5, b:4, phase:Math.PI/3, color: YELLOW }
	];
};
LissajousTrails.prototype.tick = function(){
	var f=this.f; if(!f) return;
	this.time += this.speed;
	var midX = (f.width-1)/2;
	var midY = (f.height-1)/2;
	var ampX = Math.max(1, f.width/2 - 2);
	var ampY = Math.max(1, f.height/2 - 2);
	for(var i=0;i<this.curves.length;i++){
		var c=this.curves[i];
		var x = midX + Math.sin(this.time*c.a + c.phase) * ampX;
		var y = midY + Math.cos(this.time*c.b + c.phase) * ampY;
		this.trail.push({ x:x, y:y, color:c.color, life:28 });
	}
	try { f.clear(); } catch(e){}
	for(var t=this.trail.length-1; t>=0; t--){
		var point=this.trail[t];
		point.life -= 1.2;
		if(point.life <= 0){ this.trail.splice(t,1); continue; }
		var sx = Math.round(point.x);
		var sy = Math.round(point.y);
		if(sx < 0 || sy < 0 || sx >= f.width || sy >= f.height) continue;
		var norm = point.life/28;
		var ch = norm > 0.7 ? '@' : norm > 0.45 ? 'o' : norm > 0.2 ? '+' : '.';
		var attr = (point.color || LIGHTMAGENTA) | BG_BLACK;
		if(norm < 0.25) attr = (point.color === YELLOW ? LIGHTGRAY : point.color) | BG_BLACK;
		f.setData(sx, sy, ch, attr, false);
	}
	try { f.cycle(); } catch(e){}
};
LissajousTrails.prototype.dispose = function(){ this.trail=[]; };

// LightningStorm: jagged bolts with afterglow
function LightningStorm(){
	this.f=null; this.w=0; this.h=0;
	this.fade=[]; this.charGrid=[]; this.bolts=[]; this.cooldown=0;
}
LightningStorm.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	this.w = frame.width;
	this.h = frame.height;
	this.cooldown = 0;
	this.fade=[]; this.charGrid=[]; this.bolts=[];
	for(var y=0;y<this.h;y++){
		var fadeRow = []; var charRow=[];
		for(var x=0;x<this.w;x++){ fadeRow.push(0); charRow.push(' '); }
		this.fade.push(fadeRow); this.charGrid.push(charRow);
	}
};
LightningStorm.prototype._spawnBolt = function(){
	var path=[];
	var x=Math.floor(Math.random()*this.w);
	var y=0;
	path.push({x:x,y:y});
	while(y < this.h-1){
		y += (Math.random()<0.25)?2:1;
		if(y >= this.h) y = this.h-1;
		x += (Math.floor(Math.random()*3)-1);
		if(x<0) x=0; else if(x>=this.w) x=this.w-1;
		path.push({x:x,y:y});
	}
	this.bolts.push({ path:path, life:6 });
};
LightningStorm.prototype.tick = function(){
	var f=this.f; if(!f) return;
	if(this.cooldown <= 0){
		this._spawnBolt();
		this.cooldown = 15 + Math.floor(Math.random()*25);
	}else this.cooldown--;
	for(var y=0;y<this.h;y++){
		for(var x=0;x<this.w;x++){
			var val = this.fade[y][x] - 6;
			if(val < 0){ val = 0; if(this.charGrid[y][x] !== ' ') this.charGrid[y][x] = ' '; }
			this.fade[y][x] = val;
		}
	}
	for(var b=this.bolts.length-1;b>=0;b--){
		var bolt=this.bolts[b];
		bolt.life--;
		for(var i=0;i<bolt.path.length;i++){
			var seg=bolt.path[i];
			var x=seg.x, y=seg.y;
			if(x<0||y<0||x>=this.w||y>=this.h) continue;
			var prev = (i>0)?bolt.path[i-1]:null;
			var ch='|';
			if(prev){
				var dx = seg.x - prev.x;
				if(dx > 0) ch = '/';
				else if(dx < 0) ch = '\\';
			}
			this.charGrid[y][x] = ch;
			this.fade[y][x] = 255;
			if(y+1 < this.h) this.fade[y+1][x] = Math.max(this.fade[y+1][x], 120);
			if(x>0) this.fade[y][x-1] = Math.max(this.fade[y][x-1], 80);
			if(x+1 < this.w) this.fade[y][x+1] = Math.max(this.fade[y][x+1], 80);
		}
		if(bolt.life <= 0) this.bolts.splice(b,1);
	}
	for(var yy=0; yy<this.h; yy++){
		for(var xx=0; xx<this.w; xx++){
			var intensity = this.fade[yy][xx];
			var ch = this.charGrid[yy][xx];
			if(intensity <= 0){
				f.setData(xx, yy, ' ', BG_BLACK|BLACK, false);
				continue;
			}
			var attr = (intensity > 180) ? WHITE|BG_BLACK
				: intensity > 120 ? LIGHTCYAN|BG_BLACK
				: intensity > 60 ? CYAN|BG_BLACK
				: LIGHTGRAY|BG_BLACK;
			f.setData(xx, yy, ch, attr, false);
		}
	}
	try { f.cycle(); } catch(e){}
};
LightningStorm.prototype.dispose = function(){ this.fade=[]; this.charGrid=[]; this.bolts=[]; };

// RecursiveTunnel: infinite tunnel illusion
function RecursiveTunnel(){
	this.f=null; this.time=0; this.speed=0.25; this.scale=0.15;
	this.palette=[LIGHTBLUE,CYAN,LIGHTMAGENTA,LIGHTGRAY,WHITE];
}
RecursiveTunnel.prototype.init = function(frame, opts){
	this.f = frame; opts = opts || {};
	this.speed = (!isNaN(parseFloat(opts.tunnel_speed))) ? parseFloat(opts.tunnel_speed) : 0.22;
	this.scale = (!isNaN(parseFloat(opts.tunnel_scale))) ? parseFloat(opts.tunnel_scale) : 0.17;
	this.time = 0;
};
RecursiveTunnel.prototype.tick = function(){
	var f=this.f; if(!f) return;
	this.time += this.speed;
	var cx = (f.width-1)/2;
	var cy = (f.height-1)/2;
	for(var y=1;y<=f.height;y++){
		for(var x=1;x<=f.width;x++){
			var dx = x-1 - cx;
			var dy = y-1 - cy;
			var dist = Math.sqrt(dx*dx + dy*dy) * this.scale;
			var angle = Math.atan2(dy, dx);
			var band = Math.sin(dist - this.time) * 0.5 + 0.5;
			var twist = Math.sin(angle*3 + this.time*0.7) * 0.5 + 0.5;
			var mix = (band + twist) / 2;
			if(mix < 0) mix = 0; else if(mix > 1) mix = 1;
			var idx = Math.min(this.palette.length-1, Math.floor(mix * this.palette.length));
			var ch = mix > 0.8 ? '#'
				: mix > 0.6 ? '='
				: mix > 0.4 ? '-'
				: mix > 0.25 ? '+'
				: mix > 0.1 ? '.' : ' ';
			f.setData(x-1, y-1, ch, (this.palette[idx] || LIGHTBLUE) | BG_BLACK, false);
		}
	}
	try { f.cycle(); } catch(e){}
};
RecursiveTunnel.prototype.dispose = function(){};

// Animation Manager
function AnimationManager(frame, opts){
	this.frame=frame;
	this.opts=opts||{};
	this.animations={};
	this.order=[];
	this.current=null;
	this.lastSwitch=js.global ? js.global.uptime : time();
	this.interval = this.opts.switch_interval || 30; // seconds (external scheduler will trigger switches)
	this.prevName = null;
	this.fps = Math.max(1, this.opts.fps || 8);
	this._ownedFrames=[]; // frames created by animations (child frames) to close on dispose
	// Internal timers removed; external owner drives switching & rendering.
}
AnimationManager.prototype.add = function(name, ctor){ this.animations[name]=ctor; this.order.push(name); };
AnimationManager.prototype._pickNext = function(){
	if(!this.order.length) return null;
	// If explicit sequence provided and not in random mode
	if(!this.opts.random && this.opts.sequence && this.opts.sequence.length){
		if(this._seqIndex===undefined) this._seqIndex=0;
		var name = this.opts.sequence[this._seqIndex % this.opts.sequence.length];
		this._seqIndex++;
		return name;
	}
	// Random mode: pick any registered animation except the previous (avoid immediate repeat)
	if(this.opts.random){
		if(this.order.length===1) return this.order[0];
		var pick=null; var tries=0;
		do {
			pick = this.order[rand(0,this.order.length-1)];
			tries++;
		} while(pick===this.prevName && tries < 10);
		return pick;
	}
	// Default non-random mode (no sequence): weighted random avoiding immediate repeat
	if(this.order.length===1) return this.order[0];
	var name; var attempts=0;
	do {
		name = this.order[rand(0,this.order.length-1)];
		attempts++;
	} while(name===this.prevName && attempts<5);
	return name;
};
AnimationManager.prototype.start = function(name){
	// Dispose previous animation and any owned frames
	if(this.current && this.current.dispose){
		try { this.current.dispose(); } catch(e) {}
	}
	if(this._ownedFrames && this._ownedFrames.length){
		for(var i=0;i<this._ownedFrames.length;i++){
			try { if(this._ownedFrames[i]) this._ownedFrames[i].close(); } catch(e) {}
		}
		this._ownedFrames.length=0;
	}
	if(!name) name=this._pickNext();
	if(!name) return;
	var ctor=this.animations[name];
	if(!ctor) return;
	// Allow external owner to supply the correct frame per animation
	var targetFrame = this.frame;
	if(typeof this.opts.frameForAnim === 'function'){
		var resolved = this.opts.frameForAnim(name, this.frame);
		if(resolved) targetFrame = resolved;
	}
	this.activeFrame = targetFrame;
	this.current = new ctor();
	// Provide a shallow wrapper opts with ownFrame registration hook
	var passOpts={};
	for(var k in this.opts) if(Object.prototype.hasOwnProperty.call(this.opts,k)) passOpts[k]=this.opts[k];
	var self=this;
	passOpts.ownFrame=function(fr){ if(fr) self._ownedFrames.push(fr); };
	try { this.current.init(targetFrame, passOpts); } catch(initErr){ try { log(LOG_ERR,'animation init error '+name+': '+initErr); }catch(_){}; throw initErr; }
	if(this.opts.clear_on_switch){ try{ targetFrame.clear(); }catch(e){} }
	if(this.opts.debug){ try{ log(LOG_DEBUG,'anim switch -> '+name); }catch(e){} }
	this.prevName = name;
	this.lastSwitch=time();
};
AnimationManager.prototype.tick = function(){
	if(!this.frame) return;
	if(!this.current) this.start();
	if(!this.current) return;
	try { this.current.tick(); } catch(e) { try{ log(LOG_ERR,'animation tick error '+(this.prevName||'?')+': '+e);}catch(_){}; throw e; }
};
AnimationManager.prototype.dispose = function(){
	// No internal timers to stop now.
	// Dispose current animation
	try { if(this.current && this.current.dispose) this.current.dispose(); } catch(e){}
	this.current=null;
	// Close any remaining owned frames
	if(this._ownedFrames){
		for(var i=0;i<this._ownedFrames.length;i++){
			try { if(this._ownedFrames[i]) this._ownedFrames[i].close(); } catch(e){}
		}
		this._ownedFrames.length=0;
	}
};

// Return module object instead of mutating global namespace
var moduleExports = {
	AnimationManager: AnimationManager,
	TvStatic: TvStatic,
	MatrixRain: MatrixRain,
	Life: Life,
	Starfield: Starfield,
	Fireflies: Fireflies,
	SineWave: SineWave,
	CometTrails: CometTrails,
	Plasma: Plasma,
	Fireworks: Fireworks,
	Aurora: Aurora,
	FireSmoke: FireSmoke,
	OceanRipple: OceanRipple,
	LissajousTrails: LissajousTrails,
	LightningStorm: LightningStorm,
	RecursiveTunnel: RecursiveTunnel,
	FigletMessage: FigletMessage
};

var _global = (typeof globalThis !== 'undefined') ? globalThis : (typeof js !== 'undefined' && js && js.global) ? js.global : undefined;
if (_global) {
	try { _global.CanvasAnimations = moduleExports; } catch(e){}
}

return moduleExports;

})();
