/**
 * $Id: event-emitter.js,v 1.1 2019/07/20 04:37:01 echicken Exp $
 * 
 * Same API as the node.js events.EventEmitter except:
 * - Has no concept of MaxListeners
 * - Does not allow prependListener or prependOnceListener
 * Both can be added if needed, it just seems like a hassle right now.
 *  
 * Example:
 * 
 * load('event-emitter.js');
 * 
 * const MyClass = function () {
 *		EventEmitter.call(this);
 *		this.triggerHelloWorld = function () {
 *			this.emit('helloWorld', 'Hello world!');
 *		}
 *		this.triggerSay = function (str) {
 *			this.emit('say', str);
 *		}
 *	}
 *	inherits(MyClass, EventEmitter);
 *	
 *	const myInstance = new MyClass();
 *	
 *	const hwl = myInstance.on('helloWorld', function (str) {
 *		writeln(str);
 *	});
 *	
 *	const sl = myInstance.on('say', function (str) {
 *		writeln(str);
 *	});
 *	
 *	myInstance.on('newListener', function (e, l) {
 *		writeln('New listener for ' + e + ', ' + l);
 *	});
 *	
 *	myInstance.on('removeListener', function (e, l) {
 *		writeln('Removed listener for ' + e + ', ' + l);
 *	});
 *	
 *	myInstance.triggerHelloWorld();
 *	myInstance.triggerSay('World, hello!');
 *	
 *	myInstance.removeListener('say', sl);
 *	myInstance.triggerSay("This won't happen.");
 *	
 *	myInstance.removeAllListeners('helloWorld');
 *	myInstance.triggerHelloWorld(); // Also won't happen
 */

// Ripped from ye olden node.js
function inherits(ctor, superCtor) {
	ctor.super_ = superCtor;
	ctor.prototype = Object.create(superCtor.prototype, {
		constructor: {
			value: ctor,
			enumerable: false,
			writable: true,
			configurable: true
		}
	});
};

function EventEmitter() {

	const callbacks = {};

	function addListener(event, callback, once) {
		if (typeof event != 'string') throw 'Invalid event name';
		if (typeof callback != 'function') throw 'Invalid callback';
		if (!Array.isArray(callbacks[event])) callbacks[event] = [];
		this.emit('newListener', event, callback);
		callbacks[event].push({
			callback: callback,
			once: once
		});
		return callbacks[event].length - 1;
	}

	this.on = function (event, callback) {
		return addListener.apply(this, [event, callback, false]);
	}

	this.once = function (event, callback) {
		return addListener.apply(this, [event, callback, true]);
	}

	this.removeListener = function (event, id) {
		if (!Array.isArray(callbacks[event])) return false;
		if (id < 0 || id >= callbacks[event].length) return false;
		const listener = callbacks[event][id];
		callbacks[event][id] = undefined;
		this.emit('removeListener', event, listener);
		return true;
	}
	this.off = this.removeListener;

	this.removeAllListeners = function (event) {
		if (!Array.isArray(callbacks[event])) return false;
		var ret = true;
		for (var i = 0; i < callbacks[event].length && ret; i++) {
			ret = this.removeListener(event, i);
		}
		return ret;
	}

	this.emit = function () {
		if (arguments.length < 1) throw 'emit: no event specified.';
		const event = arguments[0];
		if (!Array.isArray(callbacks[event])) return;
		const self = this;
		const args = (
			arguments.length == 2
			? [arguments[1]]
			: Array.apply(null, arguments).slice(1)
		);
		callbacks[event].forEach(function (e, i) {
			if (typeof e != 'object' || typeof e.callback != 'function') return;
			e.callback.apply(self, args);
			if (e.once) this.removeListener(event, i);
		});
	}

	this.eventNames = function () {
		return Object.keys(callbacks).reduce(function (a, c) {
			if (!Array.isArray(callbacks[c])) return a;
			if (callbacks[c].some(function (e) {
				return typeof e != 'undefined';
			})) {
				a.push(c);
			}
			return a;
		}, []);
	}

	this.listenerCount = function (event) {
		if (!Array.isArray(callbacks[event])) return 0;
		return callbacks[event].reduce(function (a, c) {
			if (typeof c == 'undefined') return a;
			return a + 1;
		}, 0);
	}

	this.listeners = function (event) {
		if (!Array.isArray(callbacks[event])) return [];
		return callbacks[event].filter(function (e) {
			return typeof e != 'undefined';
		});
	}

	this.rawListeners = function (event) {
		if (!Array.isArray(callbacks[event])) return [];
		return callbacks[event].slice();
	}

}
