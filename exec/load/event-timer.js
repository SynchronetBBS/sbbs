/*     
	Event Timer  - for Synchronet 3.15a+ (2011)
	
	-	code by mcmlxxix
	
	methods:
	
	-	Timer.cycle()
	-	Timer.addEvent(interval,repeat,action,arguments,context)
		
		- interval = time to wait before running event (in milliseconds)
		- repeat = how many times to run the event (TRUE = run infinitely, FALSE = run once)
		- action = the function to call when running the event
		- arguments = the arguments to pass to the action 
		- context = the context (scope) in which to run the event
		- pause = stop the event from running (toggle TRUE/FALSE)
		- abort = cause the event to be flagged for deletion by timer.cycle() (toggle TRUE/FALSE)
	
	sample usage: 
	
		load(event-timer.js);
		var timer=new Timer();
		
		function hello() {
			print("hello\r\n");
		}

		//will print "hello" 10 times at 10 second intervals
		var event1 = timer.addEvent(10000,10,hello);
		
		//will print "hello" infinitely at 20 second intervals
		var event2 = timer.addEvent(20000,true,print,"hello");
		as
		//will print "hello" once after 30 seconds
		var event3 = timer.addEvent(30000,false,
		
		while(timer.events.length > 0) {
			
			// iterate events list and run any events that are scheduled
			timer.cycle();
			mswait(1000);
			
			// mark event1 for deletion on next timer.cycle() 
			event1.abort = true;
		}
*/
function Timer() {
	this.VERSION = "$Revision: 1.8 $".replace(/\$/g,'').split(' ')[1];
	this.events = [];
	
	/* called by parent script, generally in a loop, and generally with a pause or timeout to minimize cpu usage */
	this.cycle = function() {
		var now = Date.now();
		var count = 0;
		/* scan all events and run any that are due */
		for(var e = 0; e<this.events.length; e++) {
			var event = this.events[e];
			if(event.abort) {
				this.events.splice(e--,1);
			}
			else if(event.pause) {
				continue;
			}
			else if(now - event.lastrun >= event.interval) {
				/* run event */
				event.run();
				count++;
				/* an event with a repeat set to TRUE will never expire */
				if(event.repeat === true)
					continue;
				/* decrement event repeat counter */
				else if(event.repeat > 0)
					event.repeat--;
				/* if event has expired, or is set to run only once, delete it */
				if(event.repeat === false || event.repeat === 0)
					this.events.splice(e--,1);
			}
		}
		/* return the number of events run this cycle */
		return count;
	}
	
	/* create a new event, do not include () on action parameter */
	this.addEvent = function(interval,repeat,action,arguments,context) {
		var event=new TimerEvent(interval,repeat,action,arguments,context);
		this.events.push(event);
		return event;
	}
	
	/* event object created by addEvent */
	function TimerEvent(interval,repeat,action,arguments,context) {
		/* last time_t at which event was executed */
		this.lastrun = Date.now();
		/* seconds between event occurance */
		this.interval=interval;
		/* number of times to repeat, true to repeat indefinitely, false to run only once */
		this.repeat=repeat;
		/* function called when event is run */
		this.action=action;
		/* arguments passed to function */
		this.arguments=arguments;
		/* context in which to run function */
		this.context=context;
		/* toggles for parent script intervention */
		this.pause=false;
		this.abort=false;
		/* run event */
		this.run = function() {
			this.lastrun = Date.now();
			this.action.apply(this.context,this.arguments);
		}
		
		/* poll event for time remaining until next run */
		this.__defineGetter__("nextrun",function() {
			return (this.interval - (Date.now() - this.lastrun));
		});
	}
};

/* event emitter/handler adapter */
function Event() { 
	this.adapt = function(object) { 
		object.prototype.__events__ = {};
		object.prototype.on = function(event,handler) { 
			this.__events__[event.toUpperCase()] = handler;
		} 
		object.prototype.emit = function(event,args) {
			args = Array.prototype.slice.call(arguments).slice(1);
			if(typeof this.__events__[event.toUpperCase()] == "function") {
				this.__events__[event.toUpperCase()](args);
			}
		}
	}
};

//js.global.timer = new Timer();
//js.global.event = new Event();

