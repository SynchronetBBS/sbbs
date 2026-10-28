/* Short-lived FileServ catalog/DCC worker. No IRC sockets or credentials.
 * The parent grants renewable leases; EOF or an expired lease stops I/O.
 * GPL-2.0-or-later.
 */
"use strict";
load("sbbsdefs.js"); load("sockdefs.js"); load("ircd/fileserv-catalog.js");
js.branch_limit = 0;
var files_job_path = argv[0], files_job_file = new File(files_job_path);
if (!files_job_file.open("r", false)) throw new Error("FileServ job unavailable");
var files_job;
try { files_job = JSON.parse(files_job_file.read()); } finally { files_job_file.close(); file_remove(files_job_path); }
var files_control = new Socket(), files_listener = null, files_data = null, files_input = "", files_out = "", files_done = false;
var files_authenticated=false,files_terminal=false;
var files_lease = system.timer + 5, files_last = system.timer, files_resume = 0, files_sent = 0, files_acked = 0, files_ackbuf = "", files_source = null;
function files_emit(value) {
	var data = JSON.stringify(value) + "\n";
	while (data.length) { var n = files_control.send(data); if (n <= 0) throw new Error("FileServ parent closed"); data = data.substr(n); }
	if (["result","complete","error"].indexOf(value.type)>=0) files_terminal=true;
}
function files_controls() {
	if (!files_control.is_connected || js.terminated || system.timer > files_lease) throw new Error("FileServ session authorization ended.");
	while (files_control.data_waiting) {
		var data = files_control.recv(8192,0); if (!data) throw new Error("FileServ parent closed"); files_input += data;
		if (files_input.length > 65536) throw new Error("Invalid FileServ control frame");
		var nl;
		while ((nl=files_input.indexOf("\n")) >= 0) {
			var m = JSON.parse(files_input.substr(0,nl)); files_input=files_input.substr(nl+1);
			if (!files_authenticated) {
				if (m.type!="ready" || m.proof!==IRC_files_mac(files_job.token,"parent:"+files_job.id)) throw new Error("FileServ parent authentication failed.");
				files_authenticated=true;files_lease=system.timer+6;continue;
			}
			if (m.type == "lease") files_lease = system.timer + 6;
			else if (m.type == "cancel") throw new Error("Cancelled.");
			else if (m.type == "resume" && !files_data) {
				if (!isFinite(m.offset) || m.offset < 0 || Math.floor(m.offset) != m.offset || m.offset >= files_job.file.size) throw new Error("Invalid resume offset.");
				files_resume=m.offset; files_emit({type:"resumed",offset:files_resume});
			} else if (m.type == "text") files_out += String(m.text).replace(/[\x00-\x08\x0b-\x1f\x7f]/g,"") + "\r\n";
			else if (m.type == "exit") files_done = true;
		}
	}
}
function files_check() {
	var user=IRC_files_user(files_job.identity,files_job.config);
	if (files_job.file) {
		var f=IRC_files_file(files_job.file.dir,files_job.file.name,user,files_job.config,!files_job.identity,true);
		if (f.path != files_job.file.path || f.size != files_job.file.size) throw new Error("File changed; request it again.");
	}
}
try {
	if (!files_control.connect("127.0.0.1",files_job.control,3)) throw new Error("FileServ parent unavailable");

	files_emit({type:"hello",id:files_job.id,proof:IRC_files_mac(files_job.token,"worker:"+files_job.id)});
	while (!files_authenticated) { files_controls();mswait(10); }
	if (files_job.kind == "catalog") {
		files_emit({type:"result",result:IRC_files_catalog(files_job.config,files_job.identity,files_job.command,files_job.args)});
	} else {
		files_check();
		var cfg=files_job.config, first=Number(cfg.port_first), last=Number(cfg.port_last);
		files_listener=new Socket(SOCK_STREAM,"FileServ DCC",cfg.bind.indexOf(":") >= 0);
		var bound=false;
		if (!first) bound=files_listener.bind(0,cfg.bind);
		else for (var port=first;port<=last;port++) if (files_listener.bind(port,cfg.bind)) { bound=true; break; }
		if (!bound || !files_listener.listen(4)) throw new Error("No DCC listening port available.");
		files_emit({type:"offer",port:files_listener.local_port});
		var deadline=system.timer+cfg.timeout;
		while (system.timer<deadline && !files_data) {
			files_controls();
			if (!files_listener.poll(0.05)) continue;
			var incoming=files_listener.accept();
			if (!incoming) continue;
			var ip=IRC_files_ip(incoming.remote_ip_address);
			if (!ip || ip != IRC_files_ip(files_job.address)) { incoming.close(); continue; }
			files_data=incoming;
		}
		files_listener.close();files_listener=null;
		if (!files_data) throw new Error("DCC connection timed out.");

		if (files_job.tls) {
			/* The native TLS loader retries a missing keyset indefinitely. Check
			   on this accepted connection, then let the actual TLS handshake
			   validate it; the parent still bounds failures and races. */
			if (!file_exists(system.ctrl_dir+"ssl.cert")) throw new Error("BBS TLS certificate is unavailable.");
			files_data.ssl_server=true;if (!files_data.ssl_session) throw new Error("DCC TLS handshake failed."); }
		files_controls();files_check();
		files_emit({type:"connected"});
		files_last=system.timer;
		if (files_job.kind == "send") {
			files_source=new File(files_job.file.path);
			if (!files_source.open("rb",true) || files_source.length != files_job.file.size) throw new Error("File unavailable or changed.");
			files_source.position=files_resume;files_sent=files_acked=files_resume;
			var pending="",nextcheck=0;
			while (files_acked<files_job.file.size) {
				files_controls();
				if (system.timer>=nextcheck) { nextcheck=system.timer+2;files_check(); }
				if (system.timer-files_last>cfg.timeout) throw new Error("DCC transfer interrupted or timed out.");
				if (files_sent<files_job.file.size && files_sent-files_acked<65536 && files_data.poll(0,true)) {
					if (!pending) pending=files_source.read(Math.min(8192,files_job.file.size-files_sent));
					if (!pending) throw new Error("File read failed.");
					var written=files_data.send(pending);
					if (written>0) { files_sent+=written;pending=pending.substr(written);files_last=system.timer; }
				}
				if ((files_data.data_waiting || files_data.poll(0.02))) {
					var ack=files_data.recv(4096,0);if (!ack) { if (!files_data.is_connected) throw new Error("DCC transfer interrupted: receiver closed.");continue; }files_ackbuf+=ack;
					while (files_ackbuf.length>=4) {
						var n=0;for (var ai=0;ai<4;ai++) n=n*256+files_ackbuf.charCodeAt(ai);
						files_ackbuf=files_ackbuf.substr(4);
						if (n<files_acked || n>files_sent) throw new Error("Invalid DCC acknowledgement.");
						if (n>files_acked) { files_acked=n;files_last=system.timer;files_emit({type:"progress",bytes:files_acked-files_resume}); }
					}
				}
			}
			files_source.close();files_source=null;
			files_emit({type:"complete",bytes:files_acked-files_resume});
		} else {
			files_out="FileServ virtual browser. HELP for commands.\r\n";var chatbuf="";
			while (!files_done) {
				files_controls();
				if (system.timer-files_last>cfg.timeout*5) throw new Error("DCC chat idle timeout.");
				if (files_out && files_data.poll(0,true)) { var out=files_data.send(files_out);if (out>0) files_out=files_out.substr(out); }
				if (files_out.length>32768) throw new Error("DCC chat output limit exceeded.");
				if ((files_data.data_waiting || files_data.poll(0.05))) {
					var part=files_data.recv(1024,0);if (!part) { if (!files_data.is_connected) break;continue; }chatbuf+=part;
					if (chatbuf.length>2048) throw new Error("DCC chat command too long.");
					var idx;
					while ((idx=chatbuf.indexOf("\n"))>=0) { var cmd=chatbuf.substr(0,idx).replace(/\r$/,"");chatbuf=chatbuf.substr(idx+1);files_last=system.timer;files_emit({type:"command",text:cmd}); }
				}
			}
			files_emit({type:"complete",bytes:0});
		}
	}
} catch (error) {
	try { files_emit({type:"error",error:IRC_files_clean(error.message || error),bytes:Math.max(0,files_acked-files_resume)}); } catch (_) {}
} finally {
	if (files_source) files_source.close();if (files_data) files_data.close();if (files_listener) files_listener.close();
	/* Keep the control socket alive until the parent consumes the terminal
				frame and closes it. Closing with an unread heartbeat can reset TCP and
				discard our result. Drain late control input while waiting for EOF. */
	var drained_until=system.timer+5;
	while (files_terminal && files_control.is_connected && system.timer<drained_until) {
		if (files_control.poll(0.05)) { if (!files_control.recv(8192,0)) break; }
	}
	files_control.close();
}
