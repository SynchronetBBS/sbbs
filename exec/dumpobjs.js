// system
console.clear();
print("\1hproperties of \1ysystem \1wobject\1n");
for(i in system)
	print("system." + i + " = " + system[i]);

// system.stats
console.clear();
print("\1hproperties of \1ysystem.stats \1wobject\1n");
for(i in system.stats)
	print("system.stats." + i + " = " + system.stats[i]);

// system.node_list[]
console.clear();
print("\1helements of \1ysystem.node_list \1warray\1n");
for(n in system.node_list)
	for(i in system.node_list[n])
		print("system.node_list[" + n + "]." + i + " = " + system.node_list[n][i]);

// client
console.clear();
print("\1hproperties of \1yclient \1wobject\1n");
for(i in client)
	print("client." + i + " = " + client[i]);

// client.socket
console.clear();
print("\1hproperties of \1yclient.socket \1wobject\1n");
for(i in client.socket)
	print("client.socket." + i + " = " + client.socket[i]);

// server
console.clear();
print("\1hproperties of \1yserver \1wobject\1n");
for(i in server)
	print("server." + i + " = " + server[i]);

// user
console.clear();
print("\1hproperties of \1yuser \1wobject\1n");
for(i in user)
	print("user." + i + " = " + user[i]);

// user.stats
console.clear();
print("\1hproperties of \1yuser.stats \1wobject\1n");
for(i in user.stats)
	print("user.stats." + i + " = " + user.stats[i]);

// user.security
console.clear();
print("\1hproperties of \1yuser.security \1wobject\1n");
for(i in user.security)
	print("user.security." + i + " = " + user.security[i]);

// file_area
console.clear();
print("\1hproperties of \1yfile_area \1wobject\1n");
for(i in file_area)
	print("file_area." + i + " = " + file_area[i]);

// file_area.lib_list[]
print("\1helements of \1yfile_area.lib_list \1warray\1n");
for(n in file_area.lib_list) {
	for(i in file_area.lib_list[n])
		print("file_area.lib_list[" + n + "]." + i + " = " + file_area.lib_list[n][i]);
	for(d in file_area.lib_list[n].dir_list)
		for(i in file_area.lib_list[n].dir_list[d])
			print("file_area.lib_list[" + n + "].dir_list[" + d + "]." + i + " = " + 
				file_area.lib_list[n].dir_list[d][i]);
}

// bbs
console.clear();
print("\1hproperties of \1ybbs \1wobject\1n");
for(i in bbs)
	print("bbs." + i + " = " + bbs[i]);

// console
console.clear();
print("\1hproperties of \1yconsole \1wobject\1n");
for(i in console)
	print("console." + i + " = " + console[i]);
