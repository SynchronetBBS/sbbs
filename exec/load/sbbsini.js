new function() {
	var f = new File(file_cfgname(system.ctrl_dir, "sbbs.ini"));
	if (!f.open("r", true))
		throw("Unable to open "+f.name);
	this.bbs = {};
	this.ftp = {};
	this.web = {};
	this.mail = {};
	this.services = {};
	var tmpval;
	var sect;
	var obj;

	function split_array(val) {
		return val.split(/,/);
	}

	function get_login_attempt_settings(f, section, global) {
		var ret = {};

		ret.delay = f.iniGetValue(section, "LoginAttemptDelay", global === undefined ? 5000 : global.login_attempt.delay);
		ret.throttle = f.iniGetValue(section, "LoginAttemptThrottle", global === undefined ? 1000 : global.login_attempt.throttle);
		ret.hack_threshold = f.iniGetValue(section, "LoginAttemptHackThreshold", global === undefined ? 10 : global.login_attempt.hack_threshold);
		ret.tempban_threshold = f.iniGetValue(section, "LoginAttemptTempBanThreshold", global === undefined ? 20 : global.login_attempt.tempban_threshold);
		ret.tempban_duration = f.iniGetValue(section, "LoginAttemptTempBanDuration", global === undefined ? 600 : global.login_attempt.tempban_duration);
		ret.filter_threshold = f.iniGetValue(section, "LoginAttemptTempBanDuration", global === undefined ? 0 : global.login_attempt.filter_threshold);
		return ret;
	}

	function get_js_settings(f, section, global) {
		var ret = {};

		ret.max_bytes = f.iniGetValue(section, "JavaScriptMaxBytes", global === undefined ? 16 * 1024 * 1024 /* JAVASCRIPT_MAX_BYTES */ : global.js.max_bytes);
		ret.time_limit = f.iniGetValue(section, "JavaScriptTimeLimit", global === undefined ? 24 * 60 * 600 /* JAVASCRIPT_TIME_LIMIT */ : global.js.time_limit);
		ret.gc_interval = f.iniGetValue(section, "JavaScriptGcInterval", global === undefined ? 100 /* JAVASCRIPT_GC_INTERVAL */ : global.js.gc_interval);
		ret.yield_interval = f.iniGetValue(section, "JavaScriptYieldInterval", global === undefined ? 10000 /* JAVASCRIPT_YIELD_INTERVAL */ : global.js.yield_interval);
		ret.load_path = f.iniGetValue(section, "JavaScriptLoadPath", global === undefined ? "load" /* JAVASCRIPT_LOAD_PATH */ : global.js.load_path);
		return ret;
	}

	sect = null;
	this.ctrl_dir = backslash(f.iniGetValue(sect, "CtrlDirectory", system.ctrl_dir));
	this.temp_dir = backslash(f.iniGetValue(sect, "TempDirectory", system.temp_dir));
	this.host_name = f.iniGetValue(sect, "Hostname", "");
	if (this.host_name === "")
		this.host_name = system.inet_addr;
	this.sem_chk_freq = f.iniGetValue(sect, "SemFileCheckFrequency", 2 /* DEFAULT_SEM_CHK_FREQ */);
	this.interfaces = split_array(f.iniGetValue(sect, "Interface", "0.0.0.0,::"));
	this.outgoing4 = f.iniGetValue(sect, "OutboundInterface", "");
	if (this.outgoing4 === "")
		this.outgoing4 = "0.0.0.0";
	this.outgoing6 = f.iniGetValue(sect, "OutboundV6Interface", "");
	if (this.outgoing6 === "")
		this.outgoing6 = "::";
	this.log_level = f.iniGetValue(sect, "LogLevel", 7 /* DEFAULT_LOG_LEVEL */);
	this.bind_retry_count = f.iniGetValue(sect, "BindRetryCount", 2 /* DEFAULT_BIND_RETRY_COUNT */);
	this.bind_retry_delay = f.iniGetValue(sect, "BindRetryDelay", 15 /* DEFAULT_BIND_RETRY_DELAY */);
	this.login_attempt = get_login_attempt_settings(f, sect);
	this.js = get_js_settings(f, sect);

	sect = "BBS";
	this.bbs.autostart = f.iniGetValue(sect, "AutoStart", true);
	this.bbs.outgoing4 = f.iniGetValue(sect, "OutboundInterface", this.outgoing4);
	this.bbs.outgoing6 = f.iniGetValue(sect, "OutboundV6Interface", this.outgoing6);
	this.bbs.telnet_port = f.iniGetValue(sect, "TelnetPort", 23);
	this.bbs.telnet_interfaces = split_array(f.iniGetValue(sect, "TelnetInterface", this.interfaces.join(",")));
	this.bbs.rlogin_port = f.iniGetValue(sect, "RLoginPort", 513);
	this.bbs.rlogin_interfaces = split_array(f.iniGetValue(sect, "RLoginInterface", this.interfaces.join(",")));
	this.bbs.pet40_port = f.iniGetValue(sect, "Pet40Port", 64);
	this.bbs.pet80_port = f.iniGetValue(sect, "Pet80Port", 128);
	this.bbs.ssh_port = f.iniGetValue(sect, "SSHPort", 22);
	this.bbs.ssh_connect_timeout = f.iniGetValue(sect, "SSHConnectTimeout", 10);
	this.bbs.ssh_interfaces = split_array(f.iniGetValue(sect, "SSHInterface", this.interfaces.join(",")));
	this.bbs.first_node = f.iniGetValue(sect, "FirstNode", 1);
	this.bbs.last_node = f.iniGetValue(sect, "LastNode", 4);
	// TODO: This has an #ifdef in C source for the default. :(
	this.bbs.outbuf_highwater_mark = f.iniGetValue(sect, "OutbufHighwaterMark", 0);
	this.bbs.outbuf_drain_timeout = f.iniGetValue(sect, "OutbufDrainTimeout", 10);
	this.bbs.sem_chk_freq = f.iniGetValue(sect, "SemFileCheckFrequency", this.sem_chk_freq);
	this.bbs.js = get_js_settings(f, sect, this);
	this.bbs.host_name = f.iniGetValue(sect, "Hostname", this.host_name);
	this.bbs.temp_dir = f.iniGetValue(sect, "TempDirectory", this.temp_dir);
	// TODO: This has an #ifdef in C source for the default. :(
	this.bbs.xtrn_term_ansi = f.iniGetValue(sect, "ExternalTermANSI", "default");
	this.bbs.xtrn_term_dumb = f.iniGetValue(sect, "ExternalTermDumn", "dumb");
	this.bbs.usedosemu = f.iniGetValue(sect, "UseDOSEmu", true);
	// TODO: This has an #ifdef in C source for the default. :(
	this.bbs.dosemu_path = f.iniGetValue(sect, "DOSemuPath", "/usr/bin/dosemu.bin");
	this.bbs.dosemuconf_path = f.iniGetValue(sect, "DOSemuConfPath", "");
	this.bbs.answer_sound = f.iniGetValue(sect, "AnswerSound", "");
	this.bbs.hangup_sound = f.iniGetValue(sect, "HangupSound", "");
	this.bbs.log_level = f.iniGetValue(sect, "LogLevel", this.log_level);
	this.bbs.options = f.iniGetValue(sect, "Options", 2);
	this.bbs.bind_retry_count = f.iniGetValue(sect, "BindRetryCount", this.bind_retry_count);
	this.bbs.bind_retry_delay = f.iniGetValue(sect, "BindRetryDelay", this.bind_retry_delay);
	this.bbs.login_attempt = get_login_attempt_settings(f, sect, this);
	this.bbs.max_concurrent_connections = f.iniGetValue(sect, "MaxConcurrentConnections", 0);

	sect = "FTP";
	this.ftp.autostart = f.iniGetValue(sect, "AutoStart", true);
	this.ftp.outgoing4 = f.iniGetValue(sect, "OutboundInterface", this.outgoing4);
	this.ftp.outgoing6 = f.iniGetValue(sect, "OutboundV6Interface", this.outgoing6);
	this.ftp.port = f.iniGetValue(sect, "Port", 23);
	this.ftp.interfaces = split_array(f.iniGetValue(sect, "Interface", this.interfaces.join(",")));
	this.ftp.max_clients = f.iniGetValue(sect, "MaxClients", 10 /* FTP_DEFAULT_MAX_CLIENTS */);
	this.ftp.max_inactivity = f.iniGetValue(sect, "MaxInactivity", 300 /* FTP_DEFAULT_MAX_INACTIVITY */);
	this.ftp.qwk_timeout = f.iniGetValue(sect, "QwkTimeout", 600 /* FTP_DEFAULT_QWK_TIMEOUT */);
	this.ftp.sem_chk_freq = f.iniGetValue(sect, "SemFileCheckFrequency", this.sem_chk_freq);
	this.ftp.min_fsize = f.iniGetValue(sect, "MinFileSize", 0);
	this.ftp.max_fsize = f.iniGetValue(sect, "MaxFileSize", 0);
	this.ftp.pasv_ip_addr = f.iniGetValue(sect, "PasvIpAddress", "0.0.0.0");
	this.ftp.pasv_ip6_addr = f.iniGetValue(sect, "PasvIp6Address", "::");
	this.ftp.pasv_port_low = f.iniGetValue(sect, "PasvPortLow", 1024);
	this.ftp.pasv_port_high = f.iniGetValue(sect, "PasvPortHigh", 65535);
	this.ftp.host_name = f.iniGetValue(sect, "Hostname", this.host_name);
	this.ftp.index_file_name = f.iniGetValue(sect, "IndexFileName", "00index");
	this.ftp.answer_sound = f.iniGetValue(sect, "AnswerSound", "");
	this.ftp.hangup_sound = f.iniGetValue(sect, "HangupSound", "");
	this.ftp.hack_sound = f.iniGetValue(sect, "HangupSound", "");
	this.ftp.temp_dir = f.iniGetValue(sect, "TempDirectory", this.temp_dir);
	this.ftp.log_level = f.iniGetValue(sect, "LogLevel", this.log_level);
	this.ftp.options = f.iniGetValue(sect, "Options", 12);
	this.ftp.bind_retry_count = f.iniGetValue(sect, "BindRetryCount", this.bind_retry_count);
	this.ftp.bind_retry_delay = f.iniGetValue(sect, "BindRetryDelay", this.bind_retry_delay);
	this.ftp.login_attempt = get_login_attempt_settings(f, sect, this);
	this.ftp.max_concurrent_connections = f.iniGetValue(sect, "MaxConcurrentConnections", 0);
	
	sect = "Mail";
	this.mail.autostart = f.iniGetValue(sect, "AutoStart", true);
	this.mail.outgoing4 = f.iniGetValue(sect, "OutboundInterface", this.outgoing4);
	this.mail.outgoing6 = f.iniGetValue(sect, "OutboundV6Interface", this.outgoing6);
	this.mail.sem_chk_freq = f.iniGetValue(sect, "SemFileCheckFrequency", this.sem_chk_freq);
	this.mail.host_name = f.iniGetValue(sect, "Hostname", this.host_name);
	this.mail.temp_dir = f.iniGetValue(sect, "TempDirectory", this.temp_dir);
	this.mail.log_level = f.iniGetValue(sect, "LogLevel", this.log_level);
	this.mail.js = get_js_settings(f, sect, this);
	this.mail.bind_retry_count = f.iniGetValue(sect, "BindRetryCount", this.bind_retry_count);
	this.mail.bind_retry_delay = f.iniGetValue(sect, "BindRetryDelay", this.bind_retry_delay);
	this.mail.login_attempt = get_login_attempt_settings(f, sect, this);
	this.mail.interfaces = split_array(f.iniGetValue(sect, "Interface", this.interfaces.join(",")));
	this.mail.pop3_interfaces = split_array(f.iniGetValue(sect, "POP3Interface", this.interfaces.join(",")));
	this.mail.smtp_port = f.iniGetValue(sect, "SMTPPort", 25);
	this.mail.submission_port = f.iniGetValue(sect, "SubmissionPort", 587);
	this.mail.submissions_port = f.iniGetValue(sect, "TLSSubmissionPort", 465);
	this.mail.pop3_port = f.iniGetValue(sect, "POP3Port", 110);
	this.mail.pop3s_port = f.iniGetValue(sect, "TLSPOP3Port", 995);
	this.mail.relay_port = f.iniGetValue(sect, "RelayPort", 25);
	this.mail.max_clients = f.iniGetValue(sect, "MaxClients", 10 /* MAIL_DEFAULT_MAX_CLIENTS */);
	this.mail.max_inactivity = f.iniGetValue(sect, "MaxInactivity", 120 /* MAIL_DEFAULT_MAX_INACTIVITY */);
	this.mail.max_delivery_attempts = f.iniGetValue(sect, "MaxDeliveryAttempts", 50 /* MAIL_DEFAULT_MAX_DELIVERY_ATTEMPTS */);
	this.mail.rescan_frequency = f.iniGetValue(sect, "RescanFrequency", 3600 /* MAIL_DEFAULT_RESCAN_FREQUENCY */);
	this.mail.lines_per_yield = f.iniGetValue(sect, "LinesPerYield", 10 /* MAIL_DEFAULT_LINES_PER_YIELD */);
	this.mail.max_recipients = f.iniGetValue(sect, "MaxRecipients", 100 /* MAIL_DEFAULT_MAX_RECIPIENTS */);
	this.mail.max_msg_size = f.iniGetValue(sect, "MaxMsgSize", (20*1024*1024) /* MAIL_DEFAULT_MAX_MSG_SIZE */);
	this.mail.max_msgs_waiting = f.iniGetValue(sect, "MaxMsgsWaiting", 100 /* MAIL_DEFAULT_MAX_MSGS_WAITING */);
	this.mail.connect_timeout = f.iniGetValue(sect, "ConnectTimeout", 30 /* MAIL_DEFAULT_CONNECT_TIMEOUT */);
	this.mail.relay_server = f.iniGetValue(sect, "RelayServer", "");
	this.mail.relay_user = f.iniGetValue(sect, "RelayUsername", "");
	this.mail.relay_pass = f.iniGetValue(sect, "RelayPassword", "");
	this.mail.dns_server = f.iniGetValue(sect, "DNSServer", "");
	this.mail.default_user = f.iniGetValue(sect, "DefaultUser", "");
	this.mail.dnsbl_hdr = f.iniGetValue(sect, "DNSBlacklistHeader", "X-DNSBL");
	this.mail.dnsbl_tag = f.iniGetValue(sect, "DNSBlacklistSubject", "SPAM");
	this.mail.pop3_sound = f.iniGetValue(sect, "POP3Sound", "");
	this.mail.inbound_sound = f.iniGetValue(sect, "InboundSound", "");
	this.mail.outbound_sound = f.iniGetValue(sect, "OutboundSound", "");
	this.mail.newmail_notice = f.iniGetValue(sect, "NewMailNotice", "%.0s\x01n\x01mNew e-mail from \x01h%s \x01n<\x01h%s\x01n>\r\n");
	this.mail.forward_notice = f.iniGetValue(sect, "ForwardNotice", "\x01n\x01mand it was automatically forwarded to: \x01h%s\x01n\r\n");
	this.mail.options = f.iniGetValue(sect, "Options", 4);
	this.mail.max_concurrent_connections = f.iniGetValue(sect, "MaxConcurrentConnections", 0);

	sect = "Services";
	this.services.autostart = f.iniGetValue(sect, "AutoStart", true);
	this.services.interfaces = split_array(f.iniGetValue(sect, "Interface", this.interfaces.join(",")));
	this.services.outgoing4 = f.iniGetValue(sect, "OutboundInterface", this.outgoing4);
	this.services.outgoing6 = f.iniGetValue(sect, "OutboundV6Interface", this.outgoing6);
	this.services.sem_chk_freq = f.iniGetValue(sect, "SemFileCheckFrequency", this.sem_chk_freq);
	this.services.js = get_js_settings(f, sect, this);
	this.services.host_name = f.iniGetValue(sect, "Hostname", this.host_name);
	this.services.temp_dir = f.iniGetValue(sect, "TempDirectory", this.temp_dir);
	this.services.log_level = f.iniGetValue(sect, "LogLevel", this.log_level);
	this.services.bind_retry_count = f.iniGetValue(sect, "BindRetryCount", this.bind_retry_count);
	this.services.bind_retry_delay = f.iniGetValue(sect, "BindRetryDelay", this.bind_retry_delay);
	this.services.login_attempt = get_login_attempt_settings(f, sect, this);
	this.services.answer_sound = f.iniGetValue(sect, "AnswerSound", "");
	this.services.hangup_sound = f.iniGetValue(sect, "HangupSound", "");
	this.services.options = f.iniGetValue(sect, "Options", 2048 /* BBS_OPT_NO_HOST_LOOKUP */);

	sect = "Web";
	this.web.autostart = f.iniGetValue(sect, "AutoStart", true);
	this.web.tls_interfaces = split_array(f.iniGetValue(sect, "TLSInterface", this.interfaces.join(",")));
	this.web.interfaces = split_array(f.iniGetValue(sect, "Interface", this.interfaces.join(",")));
	this.web.sem_chk_freq = f.iniGetValue(sect, "SemFileCheckFrequency", this.sem_chk_freq);
	this.web.js = get_js_settings(f, sect, this);
	this.web.host_name = f.iniGetValue(sect, "Hostname", this.host_name);
	this.web.temp_dir = f.iniGetValue(sect, "TempDirectory", this.temp_dir);
	this.web.log_level = f.iniGetValue(sect, "LogLevel", this.log_level);
	this.web.bind_retry_count = f.iniGetValue(sect, "BindRetryCount", this.bind_retry_count);
	this.web.bind_retry_delay = f.iniGetValue(sect, "BindRetryDelay", this.bind_retry_delay);
	this.web.login_attempt = get_login_attempt_settings(f, sect, this);

	this.web.port = f.iniGetValue(sect, "Port", 80);
	this.web.tls_port = f.iniGetValue(sect, "TLSPort", 443);
	this.web.max_clients = f.iniGetValue(sect, "MaxClients", 10 /* MAIL_DEFAULT_MAX_CLIENTS */);
	this.web.max_inactivity = f.iniGetValue(sect, "MaxInactivity", 120 /* MAIL_DEFAULT_MAX_INACTIVITY */);
	this.web.root_dir = f.iniGetValue(sect, "RootDirectory", "../web/root" /* WEB_DEFAULT_ROOT_DIR */);
	this.web.error_dir = f.iniGetValue(sect, "ErrorDirectory", "error" /* WEB_DEFAULT_ERROR_DIR */);
	this.web.cgi_dir = f.iniGetValue(sect, "CGIDirectory", "cgi-bin" /* WEB_DEFAULT_CGI_DIR */);
	this.web.default_auth_list = split_array(f.iniGetValue(sect, "Authentication", "Basic,Digest,TLS-PSK" /* WEB_DEFAULT_AUTH_LIST */));
	this.web.logfile_base = f.iniGetValue(sect, "HttpLogFile", "");
	this.web.default_cgi_content = f.iniGetValue(sect, "DefaultCGIContent", "text/plain");
	this.web.index_file_name = split_array(f.iniGetValue(sect, "IndexFileNames", "index.html,index.ssjs"));
	this.web.cgi_ext = split_array(f.iniGetValue(sect, "CGIExtensions", ".cgi"));
	this.web.ssjs_ext = split_array(f.iniGetValue(sect, "JavaScriptExtension", ".ssjs"));
	this.web.max_cgi_inactivity = f.iniGetValue(sect, "MaxCgiInactivity", 120 /* WEB_DEFAULT_MAX_CGI_INACTIVITY */);
	this.web.answer_sound = f.iniGetValue(sect, "AnswerSound", "");
	this.web.hangup_sound = f.iniGetValue(sect, "HangupSound", "");
	this.web.hack_sound = f.iniGetValue(sect, "HackAttemptSound", "");
	this.web.options = f.iniGetValue(sect, "Options", 2112 /* BBS_OPT_NO_HOST_LOOKUP | WEB_OPT_HTTP_LOGGING */);
	this.web.outbuf_drain_timeout = f.iniGetValue(sect, "OutbufDrainTimeout", 10);
}();
