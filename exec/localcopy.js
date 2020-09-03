// $Id: localcopy.js,v 1.3 2020/05/14 01:25:32 rswindell Exp $

// A simple script that just copies a file on the local/server side

// Install using 'jsexec localcopy.js install'

// Or manually in SCFG->File Options->File Transfer Protcools:
// Mnemonic (Command Key)        L                                   
// Protocol Name                 Local                               
// Access Requirements           SYSOP                               
// Upload Command Line           ?localcopy send %f                    
// Download Command Line         ?localcopy recv %f                  
// Batch Upload Command Line     ?localcopy send %g                                    
// Batch Download Command Line   ?localcopy recv %s
// Bi-dir Command Line                                               
// Native (32-bit) Executable    No                                  
// Supports DSZLOG               No                                  
// Socket I/O                    No                                  

// Copy a file, confirm over-write, preserving original date/time stamp
function fcopy(src, dest)
{
	if(file_exists(dest) && !confirm("Overwrite \1w" + dest))
		return false;
	if(!file_copy(src, dest)) {
		alert(format("Error %d copying '%s' to '%s'", errno, src, dest));
		return false;
	}
	if(!file_utime(dest, time(), file_date(src)))
		return false;
	log(LOG_DEBUG, format("'%s' copied to '%s'", src, dest));
	return true;
}

function main(cmd) {

	if(!cmd) {
		alert("usage: <install | send | recv> [<file> [...]]");
		return 1;
	}

	switch(cmd) {
		case 'install':
			var cnflib = load({}, "cnflib.js");
			var file_cnf = cnflib.read("file.cnf");
			if(!file_cnf) {
				alert("Failed to read file.cnf");
				return(-1);
			}
			file_cnf.prot.push({ 
				  key: 'L'
				, name: 'Local Copy'
				, ulcmd: '?localcopy send %f' 
				, dlcmd: '?localcopy recv %f'
				, batulcmd: '?localcopy send %g'
				, batdlcmd: '?localcopy recv %s'
				, ars: 'SYSOP'
				});
		
			if(!cnflib.write("file.cnf", undefined, file_cnf)) {
				alert("Failed to write file.cnf");
				return(-1);
			}
			return(0);
	
		case 'send':
			if(file_isdir(argv[1])) { /* batch upload */
				var dest = backslash(argv[1]);
				var src = prompt("Source files (with wildcards, e.g. *)");
				if(!src)
					return(1);
				var list = directory(src);
				if(!list || !list.length) {
					alert("No files found: " + src);
					return(1);
				}
				print(list.length + " files found");
				var count = 0;
				for(var i in list) {
					if(js.terminated || console.aborted || !bbs.online)
						break;
					var path = dest + file_getname(list[i]);
					if(fcopy(list[i], path))
						count++;
				}
				print(count + " files copied");
				return(0);
			}
			var src = prompt("Source file or directory");
			if(!src)
				return(1);
			if(file_isdir(src))
				src = backslash(src);
			for(var i = 1; i < argc; i++) {
				var path = src;
				if(file_isdir(path))
					path += file_getname(argv[i]);
				fcopy(path, argv[i]);
			}
			return(0);
		
		case 'recv':
			var dest = prompt("Destination file or directory");
			if(!dest)
				return(1);
			if(file_isdir(dest))
				dest = backslash(dest);
			for(var i = 1; i < argc; i++) {
				var path = dest;
				if(file_isdir(path))
					path += file_getname(argv[i]);
				fcopy(argv[i], path);
			}
			return(0);
	}
}

var result = main(argv[0]);
// The BBS flushes I/O buffers when returning from transfer prots:
while(bbs.online && console.output_buffer_level && !js.terminated) {
	sleep(100);
}
result;
