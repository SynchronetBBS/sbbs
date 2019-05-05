load("sbbsdefs.js");

js.global.log = function(log_level, message) { 
	if(js.global.parent_queue != undefined) {
		parent_queue.write({LOG_LEVEL:log_level,message:message},"log");
	}
	else {
		var ef = new File(exec_path + "/e.log");
		var log_type = "";
		switch(log_level) {
			case LOG_EMERG:
				log_type = "EMERGENCY";
				break;
			case LOG_ALERT:
				log_type = "EMERGENCY";
				break;
			case LOG_CRIT:
				log_type = "CRITICAL";
				break;
			case LOG_ERR:
				log_type = "ERROR";
				break;
			case LOG_WARNING:
				log_type = "WARNING";
				break;
			case LOG_NOTICE:
				log_type = "NOTICE";
				break;
			case LOG_DEBUG:
				log_type = "DEBUG";
				break;
			case LOG_INFO:
			default:
				log_type = "INFO";
				break;
		}
		ef.open('a',true);
		ef.writeln(system.timestr() + ": " + log_type + ": " + message);
		ef.close();
	}
};

