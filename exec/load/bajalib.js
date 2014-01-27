// [6.1] - Quick Function Reference 
var baja = {};
var globals = {};
var functions = {};
var variables = {};
var labels = [];
var labelCount = 0;
var ifLevel = false;

// String Manipulation Functions
// -----------------------------
// STR <str_var> [str_var] [...]
baja["STR"] = function() {
	var line = "var " + arguments[0];
	variables[arguments[0]] = "STR";
	for(var a=1;a<arguments.length;a++) {
		line += ", " + arguments[a];
		variables[arguments[a]] = "STR";
	}
	return line + ";";
}
// GLOBAL_STR <str_var> [str_var] [...]
baja["GLOBAL_STR"] = function() {
	var line = "";
	for(var a=0;a<arguments.length;a++) {
		line += arguments[a] + " = ";
		variables[arguments[a]] = "STR";
	}
	return line + "undefined;";
}
// SET <str_var> <"cstr">
// SET <int_var> <#>
baja["SET"] = function(var1, var2) {
	if(variables[var1] == "STR") 
		return var1 + " = " + var2 + ";";
	if(variables[var1] == "INT") 
		return var1 + " = Number(" + var2 + ");";
}
// COPY <str_var> <any_var>
baja["COPY"] = baja["SET"];
// SWAP <str_var> <any_var>
baja["SWAP"] = function(var1,var2) {
	var line = "system.baja.temp = " + var1 + ";\r\n";
	line += var1 + " = " + var2 + ";\r\n";
	line += var2 + " = system.baja.temp;";
	return line;
}
// STRCAT <str_var> <str_var or "cstr">
baja["STRCAT"] = function() {
	var line = arguments[0] + " = " + arguments[0];
	for(var a=1;a<arguments.length;a++) 
		line += " + " + arguments[a];
	return line + ";";
}
// SPRINTF <str_var> <"cstr"> <any_var> [any_var] [...]
baja["SPRINTF"] = function() {
	line = arguments[0] + " = " + "format(" + arguments[1];
	for(var a=2;a<arguments.length;a++) 
		line += ", " + arguments[a];
	return line + ");";
}
// TIME_STR <str_var> <int_var>
baja["TIME_STR"] = function(var1,var2) { 
	return var1 + " = system.timestr(" + var2 + ");";
}
// DATE_STR <str_var> <int_var>
baja["DATE_STR"] = function(var1,var2) { 
	return var1 + " = system.datestr(" + var2 + ");";
}
// SECOND_STR <str_var> <int_var>
baja["SECOND_STR"] = function(var1,var2) { 
	return var1 + " = system.secondstr(" + var2 + ");";
}
// FTIME_STR <str_var> <"cstr"> <int_var>
baja["FTIME_STR"] = function(var1,var2,var3) { 
	return format("%s = strftime(%s,%s);",var1,var2,var3);
}
// SHIFT_STR [str_var] <#>
baja["SHIFT_STR"] = function(var1,var2) { 
	if(!var2)
		return "bbs.command_str = bbs.command_str.substr(" + var1 + ");";
	return var1 + " = " + var1 + ".substr(" + var2 + ");";
}
// STRIP_CTRL [str_var]
baja["STRIP_CTRL"] = function(var1) { return "strip_ctrl(" + var1 + ");"; }
// TRUNCSP [str_var]
baja["TRUNCSP"] = function(var1) { return "truncsp(" + var1 + ");"; }
// STRUPR [str_var]
baja["STRUPR"] = function(var1) { return var1 + ".toUpperCase();"; }
// STRLWR [str_var]
baja["STRLWR"] = function(var1) { return var1 + ".toLowerCase();"; }
// SETSTR <"cstr">
baja["SETSTR"] = function(var1) { return "bbs.command_str = " + var1 + ";"; }
// REPLACE_TEXT <#> <"cstr">
baja["REPLACE_TEXT"] = function(var1,var2) { return format("bbs.replace_text(%s,%s);",var1,var2); }
// LOAD_TEXT <"str">
baja["LOAD_TEXT"] = function(var1) { return format("bbs.load_text(%s);",var1); }
// REVERT_TEXT <# or ALL>
baja["REVERT_TEXT"] = function(var1) { return format("bbs.revert_text(%s);",var1); }

// Integer Manipulation Functions
// ------------------------------
// INT <int_var> [int_var] [...]
baja["INT"] = function() {
	var line = "var " + arguments[0];
	variables[arguments[0]] = "INT";
	for(var a=1;a<arguments.length;a++) {
		line += ", " + arguments[a];
		variables[arguments[a]] = "INT";
	}
	return line + ";";
}
// GLOBAL_INT <int_var> [int_var] [...]
baja["GLOBAL_INT"] = function() {
	var line = "";
	for(var a=0;a<arguments.length;a++) {
		line += arguments[a] + " = ";
		variables[arguments[a]] = "INT";
	}
	return line + "undefined;";
}
// ADD <int_var> <any_var or #>
baja["ADD"] = function(num1,num2) { return num1 + " += " + num2 + ";"; }
// SUB <int_var> <any_var or #>
baja["SUB"] = function(num1,num2) { return num1 + " -= " + num2 + ";"; }
// MUL <int_var> <any_var or #>
baja["MUL"] = function(num1,num2) { return num1 + " *= " + num2 + ";"; }
// DIV <int_var> <any_var or #>
baja["DIV"] = function(num1,num2) { return num1 + " /= " + num2 + ";"; }
// MOD <int_var> <any_var or #>
baja["MOD"] = function(num1,num2) { return num1 + " %= " + num2 + ";"; }
// AND <int_var> <any_var or #>
baja["AND"] = function(num1,num2) { return num1 + " &= " + num2 + ";"; }
// OR  <int_var> <any_var or #>
baja["OR"] = function(num1,num2) { return num1 + " |= " + num2 + ";"; }
// NOT <int_var> <any_var or #>
baja["NOT"] = function(num1,num2) { return num1 + " &= ~" + num2 + ";"; }
// XOR <int_var> <any_var or #>
baja["XOR"] = function(num1,num2) { return num1 + " ^= " + num2 + ";"; }
// COPY <int_var> <any_var>
baja["COPY"] = function(var1, var2) { return 'var1 = var2.toString();'; }
// SWAP <int_var> <any_var>
baja["SWAP"] = function(var1, var2) { return 'var2 = [var1, var1 = var2][0];' }; 
// RANDOM <int_var> <#>
baja["RANDOM"] = function(var1,num1) { return var1 + " = Math.random() * " + num1 + ";"; }
// TIME <int_var>
baja["TIME"] = function(var1) { return var1 + " = time();"; }
// STRLEN <int_var> <str_var>
baja["STRLEN"] = function(var1,var2) { return var1 + " = console.strlen(" + var2 + ");"; }
// DATE_INT <int_var> <str_var>
baja["DATE_INT"] = function(var1,var2) { 
	var line = var1 + " = new Date(";
	var2 = var2.replace(/(\d{2})\/(\d{2})\/(\d{2})/,"$1/$2/20$3");
	return line + var2 + ");";
}
// CRC16 <int_var> <str_var>
baja["CRC16"] = function(var1,var2) { return var1 + " = crc16_calc(" + var2 + ");"; }
// CRC32 <int_var> <str_var>
baja["CRC32"] = function(var1,var2) { return var1 + " = crc32_calc" + var2 + ");"; }
// CHKSUM <int_var> <str_var>
baja["CHKSUM"] = function(var1,var2) { return var1 + " = chksum_calc" + var2 + ");"; }
// CHARVAL <int_var> <str_var>
baja["CHARVAL"] = function(var1,var2) { return var1 + " = " + var2 + ".charCodeAt(0);"; }

// Logic/Control Flow Functions
// ----------------------------
// GOTO <txt>
baja["GOTO"] = function(label) { return "continue " + label + ";"; }
// CALL <txt>
baja["CALL"] = function(label) { return label + "();"; }
// RETURN
baja["RETURN"] = function() { return "return;\r\n})();"; }
// SETLOGIC <TRUE or FALSE or GREATER or LESS>
baja["SETLOGIC"] = function(var1) { return "system.baja.logicState = " + var1.toUpperCase() + ";"; }
// COMPARE <any_var> <any_var or "cstr" or #>
baja["COMPARE"] = function(var1,var2) { 
	var line = "";
	if(!functions["compare"]) {
		functions["compare"] = true;
		line += "system.baja.compare = " + compare.toSource() + "\r\n";
	}
	return line + "system.baja.compare(" + var1 + "," + var2 + ");";
}
// IF_EQUAL
baja["IF_EQUAL"] = function() {
	ifLevel = true;
	return "if(system.baja.logicState == TRUE) {";
}
// IF_TRUE
baja["IF_TRUE"] = baja["IF_EQUAL"];
// IF_NOT_EQUAL
baja["IF_NOT_EQUAL"] = function() {
	ifLevel = true;
	return "if(system.baja.logicState == FALSE) {";
}
// IF_FALSE
baja["IF_FALSE"] = baja["IF_NOT_EQUAL"];
// IF_GREATER
baja["IF_GREATER"] = function() {
	ifLevel = true;
	return "if(system.baja.logicState == GREATER) {";
}
// IF_GREATER_OR_EQUAL
baja["IF_GREATER_OR_EQUAL"] = function() {
	ifLevel = true;
	return "if(system.baja.logicState == GREATER || system.baja.logicState == TRUE) {";
}
// IF_LESS
baja["IF_LESS"] = function() {
	ifLevel = true;
	return "if(system.baja.logicState == LESS) {";
}
// IF_LESS_OR_EQUAL
baja["IF_LESS_OR_EQUAL"] = function() {
	ifLevel = true;
	return "if(system.baja.logicState == LESS || system.baja.logicState == EQUAL) {";
}
// ELSE
baja["ELSE"] = function() {	return "}\r\nelse {"; }
// END_IF
baja["END_IF"] = function() { 
	ifLevel = false;
	return "}"; 
}
baja["ENDIF"] = baja["END_IF"];
// SWITCH <int_var>
baja["SWITCH"] = function(var1) { return "switch(" + var1 + ") {"; }
// CASE <#>
baja["CASE"] = function(var1) { return "case " + var1 + ":"; }
// DEFAULT
baja["DEFAULT"] = function() { return "default:"; }
// END_CASE
baja["END_CASE"] = function() { return "break;"; }
// END_SWITCH
baja["END_SWITCH"] = function() { return "}"; }
// CMD_HOME
baja["CMD_HOME"] = function() {
	var newLabel = "label" + labelCount++;
	labels.push(newLabel);
	var line = newLabel + ":\r\n";
	return line + "while(!js.terminated) {";
}
// CMDKEY <key>
baja["CMDKEY"] = function(var1) {
	ifLevel = false;
	if(var1.toUpperCase() == "DIGIT")
		return "if(system.baja.cmdKey.match(/\d/)) {";
	if(var1.toUpperCase() == "EDIGIT")
		return "if(system.baja.cmdKey.match(/\/\d/)) {";
	if(var1.match(/'[^']*/))
		return "if(\"" + var1 + "\" == system.baja.cmdKey) {";
	if(var1.match(/\^*/))
		return "if(ctrl(\"" + var1[1] + "\") == system.baja.cmdKey) {";
	return "if(\"" + var1.toUpperCase() + "\" == system.baja.cmdKey.toUpperCase()) {";
}
// CMDKEYS <keylist>
baja["CMDKEYS"] = function(var1) {
	ifLevel = false;
	return "if(\"" + var1 + "\".indexOf(system.baja.cmdKey) >= 0) {";
}
// CMDSTR <"cstr">
baja["CMDSTR"] = function(var1) {
	ifLevel = false;
	return "if(\"" + var1 + "\".toUpperCase() == bbs.command_str.toUpperCase()) {";
}
// END_CMD
baja["END_CMD"] = function() {
	if(!labels.length) 
		return "}";
	var line = "continue " + labels[labels.length-1] + ";";
	if(!ifLevel)
		line += "\r\n}";
	return line;
}
// CMD_POP
baja["CMD_POP"] = function() { 
	labels.pop();
	return "}";
}
// COMPARE_KEY <key>
baja["COMPARE_KEY"] = function(var1) { 
	return "system.baja.logicState = (" + var1 + " == system.baja.cmdKey?TRUE:FALSE);";
}
// COMPARE_KEYS <keylist>
baja["COMPARE_KEYS"] = function(var1) {
	return "system.baja.logicState = (" + var1 + ".indexOf(system.baja.cmdKey) >= 0?TRUE:FALSE);";
}
// COMPARE_STR <"cstr">
baja["COMPARE_STR"] = function(var1) { 
	return "system.baja.logicState = (" + var1 + 
		".toUpperCase() == bbs.command_str.toUpperCase()?TRUE:FALSE);";
}
// COMPARE_WORD <"cstr">
baja["COMPARE_WORD"] = function(var1) { 
	return "system.baja.logicState = (bbs.command_str.toUpperCase().indexOf(" + 
		var1 + ".toUpperCase()) == 0?TRUE:FALSE);";
}
// COMPARE_ARS <ars>
baja["COMPARE_ARS"] = function(var1) { 
	return "system.baja.logicState = (user.compare_ars(" + var1 + ")?TRUE:FALSE);";
}
// COMPARE_STRN <#> <str_var> <str_var or "cstr">
baja["COMPARE_STRN"] = function(var1,var2,var3) { 
	return format("system.baja.logicState = (%s.substr(0,%s).toUpperCase() == %s.substr(0,%s).toUpperCase()?TRUE:FALSE);",
		var2,var1,var3,var1);
}
// COMPARE_SUBSTR <str_var> <str_var or "cstr">
baja["COMPARE_SUBSTR"] = function(var1,var2) { 
	return "system.baja.logicState = (" + var1 + ".indexOf(" + 
		var2 + ") >= 0?TRUE:FALSE);";
}

// Display Functions
// -----------------
// PRINT <"cstr" or any_var>
baja["PRINT"] = function(var1) { return "console.print(" + var1 + ");"; }
// PRINTF <"cstr"> <any_var> [any_var] [...]
baja["PRINTF"] = function() {
	var line = "printf(" + arguments[0];
	for(var a=1;a<arguments.length;a++) 
		line += ", " + arguments[a];
	return line + ");";
}
// PRINT_LOCAL <"cstr">
baja["PRINT_LOCAL"] = function(var1) { return "log(" + var1 + ");"; }
// PRINT_REMOTE <"cstr">
baja["PRINT_REMOTE"] = baja["PRINT"];
// PRINTSTR
baja["PRINTSTR"] = function(var1) { return "console.print(bbs.command_str);"; }
// PRINTKEY
baja["PRINTKEY"] = function(var1) { return "console.print(system.baja.cmdKey);"; }
// MNEMONICS <"cstr">
baja["MNEMONICS"] = function(var1) { return "console.mnemonics(" + var1 + ");"; }
// CLS
baja["CLS"] = function() { return "console.clear();"; }
// CRLF
baja["CRLF"] = function() { return "console.crlf();"; }
// PRINTFILE <"str" or str_var> [#]
baja["PRINTFILE"] = function(var1, var2) { 
	return format("console.printfile(%s,%s);",var1,var2); 
}
// PRINTTAIL <str_var> <#> <#>
baja["PRINTTAIL"] = function(var1,var2,var3) { 
	return format("console.printtail(%s,%s,%s);",var1,var3,var2); 
}
// PRINTFILE_STR
baja["PRINTFILE_STR"] = function(var1) { 
	return "console.printfile(bbs.command_str);"; 
}
// PRINTFILE_LOCAL <"str">
baja["PRINTFILE_LOCAL"] = baja["PRINTFILE"];
// PRINTFILE_REMOTE <"str">
baja["PRINTFILE_REMOTE"] = baja["PRINTFILE"];
// LIST_TEXT_FILE
baja["LIST_TEXT_FILE"] = function(var1, var2) { 
	var line = "if(bbs.check_syspass()) {\r\n";
	return line + ("console.printfile(%s,%s);\r\n}",var1,var2); 
}
// EDIT_TEXT_FILE
baja["EDIT_TEXT_FILE"] = function(var1, var2) { 
	var line = "if(bbs.check_syspass()) {\r\n";
	return line + ("console.editfile(%s,%s);\r\n}",var1,var2); 
}
// PAUSE
baja["PAUSE"] = function() { return "console.pause();"; }
// MENU <"str">
baja["MENU"] = function(var1) { return "bbs.menu(" + var1 + ");"; }
// NODELIST_ALL
baja["NODELIST_ALL"] = function() { return "bbs.list_nodes();"; }
// NODELIST_USERS
baja["NODELIST_USERS"] = function() { return "bbs.whos_online();"; }
// USERLIST_SUB
baja["USERLIST_SUB"] = function() { return "bbs.list_users(2);"; }
// USERLIST_DIR
baja["USERLIST_DIR"] = function() { return "bbs.list_users(1);"; }
// USERLIST_ALL
baja["USERLIST_ALL"] = function() { return "bbs.list_users(0);"; }
// USERLIST_LOGONS
baja["USERLIST_LOGONS"] = function() { return "bbs.list_logons();"; }
// YES_NO <"cstr">
baja["YES_NO"] = function(var1) { 
	return "system.baja.logicState = (console.yesno(" + var1 + ")?TRUE:FALSE);"; 
}
// NO_YES <"cstr">
baja["NO_YES"] = function(var1) { 
	return "system.baja.logicState = (console.noyes(" + var1 + ")?TRUE:FALSE);"; 
}
// READ_SIF <"str">
baja["READ_SIF"] = function(var1) {
	var line = '';
	if(!functions["SIF"]) {
		functions["SIF"] = true;
		line += 'load("siflib.js");\r\n';
	}
	return line + 'SIF.read(' + var1 + '.sif,bbs.command_str);'; 
}
// SAVELINE
baja["SAVELINE"] = function() { return "console.saveline();"; }
// RESTORELINE
baja["RESTORELINE"] = function() { return "console.restoreline();"; }

// Input Functions
// ---------------
// INKEY
baja["INKEY"] = function() {
	return
		'system.baja.temp = console.inkey();\r\n' +
		'if(system.baja.temp != "") {\r\n' +
		'system.baja.logicState = TRUE;\r\n' +
		'system.baja.cmdKey = system.baja.temp;\r\n' +
		'} else {\r\n' +
		'system.baja.logicState = FALSE;\r\n' +
		'}';
}
// GETKEY
baja["GETKEY"] = function() { return 'system.baja.cmdKey = console.getkey().toUpperCase();'; }
// GETKEYE
baja["GETKEYE"] = function() {
	return
		'system.baja.temp = console.getkey().toUpperCase();\r\n' +
		'if(system.baja.temp == "/")\r\n' +
		'system.baja.cmdKey = "/" + console.getkey().toUpperCase();\r\n' +
		'else\r\n' +
		'system.baja.cmdKey = system.baja.temp;';
}
// GETCMD <"cstr">
baja["GETCMD"] = function(var1) {
	return 'system.baja.cmdKey = console.getkeys(\"' + var1 + '\").toUpperCase();';
}
// GETSTR [str_var] [#] [#]
baja["GETSTR"] = function(var1,var2,var3) { 
	var str = var1;
	var len = var2;
	var mode = var3;
	/* an argument is missing! */
	if(!var3) {
		if(!isNaN(var1)) {
			len = var1;
			str = "bbs.command_str";
			mode = var2;
		}
		else if(var1.indexOf('K_') >= 0) {
			len = 128;
			str = "bbs.command_str";
			mode = var1;
		}
		else if(var1) {
			len = 128;
			mode = var2;
		}
	}
	return str + " = console.getstr(" + len + "," + mode + ");";
}
// GETLINE [str_var] [#]
baja["GETLINE"] = function(var1,var2) { 
	var str = var1;
	var len = var2;
	/* an argument is missing! */
	if(!var2) {
		if(!isNaN(var1)) {
			len = var1;
			str = "bbs.command_str";
		}
		else if(var1) {
			len = 128;
		}
	}
	return str + " = console.getstr(" + len + ",K_LINE);";
}
// GETSTRUPR [str_var] [#]
baja["GETSTRUPR"] = function(var1,var2) { 
	var str = var1;
	var len = var2;
	/* an argument is missing! */
	if(!var2) {
		if(!isNaN(var1)) {
			len = var1;
			str = "bbs.command_str";
		}
		else if(var1) {
			len = 128;
		}
	}
	return str + " = console.getstr(" + len + ",K_UPPER);";
}
// GETNAME [str_var] [#]
baja["GETNAME"] = function(var1,var2) { 
	var str = var1;
	var len = var2;
	/* an argument is missing! */
	if(!var2) {
		if(!isNaN(var1)) {
			len = var1;
			str = "bbs.command_str";
		}
		else if(var1) {
			len = 128;
		}
	}
	return str + " = console.getstr(" + len + ",K_UPRLWR);";
}
// GETFILESPEC
baja["GETFILESPEC"] = function() {
	return
		'system.baja.temp = bbs.get_filespec();\r\n' +
		'if(system.baja.temp = "") {\r\n' +
		'system.baja.logicState = FALSE;\r\n' +
		'} else {\r\n' +
		'system.baja.logicState = TRUE;\r\n' +
		'bbs.command_str = system.baja.temp;\r\n' +
		'}';
}
// GETLINES
baja["GETLINES"] = function() { return "console.getlines();"; }
// GETNUM [any_var] <#>
baja["GETNUM"] = function(var1,var2) { return var1 + " = console.getnum(" + var2 + ");"; }
// GET_TEMPLATE <"str">
baja["GET_TEMPLATE"] = function(var1) { 
	return "bbs.command_str = console.gettemplate(" + var1 + ");";
}
// CHKSYSPASS
baja["CHKSYSPASS"] = function() { return 'system.baja.logicState = bbs.check_syspass();'; }
// CREATE_SIF <"str">
baja["CREATE_SIF"] = function(var1) {
	var line = '';
	if(!functions["SIF"]) {
		functions["SIF"] = true;
		line += 'load("siflib.js");\r\n';
	}
	return line + 'SIF.create(' + var1 + '.sif,bbs.command_str);'; 
}

// Miscellaneous Functions
// -----------------------
// ONLINE
baja["ONLINE"] = function() { return "bbs.online = (2);"; }
// OFFLINE
baja["OFFLINE"] = function() { return "bbs.online = (1);"; }
// LOGIN <"cstr">
baja["LOGIN"] = function(var1) { return "bbs.login(bbs.command_str," + var1 + ");"; }
// LOGON
baja["LOGON"] = function() { return "bbs.logon();"; }
// LOGOFF
baja["LOGOFF"] = function() { return "bbs.logoff();"; }
// LOGOFF_FAST
baja["LOGOFF_FAST"] = function() { return "bbs.nodesync();\r\nbbs.hangup();"; }
// LOGOUT
baja["LOGOUT"] = function() { return "bbs.logout();"; }
// NEWUSER
baja["NEWUSER"] = function() { return "bbs.newuser();"; }
// SET_MENU_DIR <"str">
baja["SET_MENU_DIR"] = function(var1) { return "bbs.menu_dir = " + var1 + ";"; }
// SET_MENU_FILE <"str">
baja["SET_MENU_FILE"] = function(var1) { return "bbs.menu_file = " + var1 + ";"; }
// SYNC
baja["SYNC"] = function(var1) { return "bbs.nodesync();"; }
// ASYNC
baja["ASYNC"] = baja["SYNC"];
// RIOSYNC
baja["RIOSYNC"] = baja["SYNC"];
// PUT_NODE
baja["PUT_NODE"] = baja["SYNC"];
// PAUSE_RESET
baja["PAUSE_RESET"] = function() { return "console.line_counter = 0;"; }
// CLEAR_ABORT
baja["CLEAR_ABORT"] = function() { return "console.aborted = false;"; }
// UNGETKEY
baja["UNGETKEY"] = function() { return "console.ungetstr(system.baja.cmdKey);"; }
// UNGETSTR
baja["UNGETSTR"] = function() { return "console.ungetstr(bbs.command_str);"; }
// HANGUP
baja["HANGUP"] = function() { return "bbs.hangup();"; }
// EXEC <"str">
baja["EXEC"] = function(var1) { return "bbs.exec(" + var1 + ");"; }
// EXEC_INT <"str">
baja["EXEC_INT"] = function(var1) { return "bbs.exec(" + var1 + ",(1<<1));"; }
// EXEC_BIN <"str">
baja["EXEC_BIN"] = function(var1) { return "bbs.exec(\"*" + var1.replace(/\"/g,'') + "\");"; }
// EXEC_XTRN <"str">
baja["EXEC_XTRN"] = function(var1) { 
	var line = 'if(typeof xtrn_area.prog[' + var1 + '] != "undefined" &&';
	line +=	'user.compare_ars(xtrn_area.prog[' + var1 + '].ars)) {\r\n';
	line += 'bbs.exec_xtrn(' + var1 + ');\r\n';
	line += '}';
	return line;
}
// LOG <"cstr">
baja["LOG"] = function(var1) { return "log(" + var1 + ");"; }
// LOGSTR
baja["LOGSTR"] = function(var1) { return "bbs.log_str(bbs.command_str);"; }
// LOGKEY
baja["LOGKEY"] = function(var1) { return "bbs.log_key(system.baja.cmdKey);"; }
// LOGKEY_COMMA
baja["LOGKEY_COMMA"] = function(var1) { return "bbs.log_key(system.baja.cmdKey,true);"; }
// NODE_STATUS <#>
baja["NODE_STATUS"] = function(var1) { 
	return "system.node_list[bbs.node_num-1].status = " + var1 + ";"; 
}
// NODE_ACTION <#>
baja["NODE_ACTION"] = function(var1) { return "bbs.node_action = " + var1 + ";"; }
// INC_MAIN_CMDS
baja["INC_MAIN_CMDS"] = function(var1) { return "bbs.main_cmds++;"; }
// INC_FILE_CMDS
baja["INC_FILE_CMDS"] = function(var1) { return "bbs.file_cmds++;"; }
// COMPARE_USER_MISC <#>
baja["COMPARE_USER_MISC"] = function(var1) { 
	return format("system.baja.logicState = (user.settings&(%s) == %s)?TRUE:FALSE;",var1,var1);
}
// COMPARE_USER_CHAT <#>
baja["COMPARE_USER_CHAT"] = function(var1) { 
	return format("system.baja.logicState = (user.chat_settings&(%s) == %s)?TRUE:FALSE;",var1,var1);
}
// COMPARE_USER_QWK  <#>
baja["COMPARE_USER_QWK"] = function(var1) { 
	return format("system.baja.logicState = (user.qwk_settings&(%s) == %s)?TRUE:FALSE;",var1,var1);
}
// COMPARE_NODE_MISC <#>
baja["COMPARE_NODE_MISC"] = function(var1) { 
	return format("system.baja.logicState = (system.node_list[bbs.node_num].misc&(%s) == %s)?TRUE:FALSE;",var1,var1);
}
// TOGGLE_USER_MISC <#>
baja["TOGGLE_USER_MISC"] = function(var1) { return format("user.settings ^= (%s);",var1); }
// TOGGLE_USER_CHAT <#>
baja["TOGGLE_USER_CHAT"] = function(var1) { return format("user.chat_settings ^= (%s);",var1); }
// TOGGLE_USER_QWK  <#>
baja["TOGGLE_USER_QWK"] = function(var1) { return format("user.qwk_settings ^= (%s);",var1); }
// TOGGLE_NODE_MISC <#>
baja["TOGGLE_NODE_MISC"] = function(var1) { 
	return format("system.node_list[bbs.node_num].misc ^= (%s);",var1); 
}
// TOGGLE_USER_FLAG <char> <char>
baja["TOGGLE_USER_FLAG"] = function(var1,var2) { 
	return "user.security.flags" + var1 + " ^= (" + var2 + ");";
}
// ADJUST_USER_CREDITS <#>
baja["ADJUST_USER_CREDITS"] = function(var1) { 
	return "user.security.credits += " + var1 + ";";
}
// ADJUST_USER_MINUTES <#>
baja["ADJUST_USER_MINUTES"] = function(var1) { 
	return "user.security.minutes += " + var1 + ";";
}
// SET_USER_LEVEL <#>
baja["SET_USER_LEVEL"] = function(var1) { 
	return "user.security.level = " + var1 + ";";
}
// SET_USER_STRING <#>
baja["SET_USER_STRING"] = function(var1) { 
	// !define USER_STRING_ALIAS	0
	// !define USER_STRING_REALNAME	1
	// !define USER_STRING_HANDLE	2
	// !define USER_STRING_COMPUTER	3
	// !define USER_STRING_NOTE	4
	// !define USER_STRING_ADDRESS	5
	// !define USER_STRING_LOCATION	6
	// !define USER_STRING_ZIPCODE	7
	// !define USER_STRING_PASSWORD	8
	// !define USER_STRING_BIRTHDAY	9
	// !define USER_STRING_PHONE	10
	// !define USER_STRING_MODEM	11
	// !define USER_STRING_COMMENT	12
	// !define USER_STRING_NETMAIL	13	# Requires v3 (03/02/00) or later
	var uStrings = [
		"alias","name","handle","computer",
		"note","address","location","zipcode",
		"security.password","birthdate","phone",
		"modem","comment","netmail"
	];
	return "user." + uStrings[var1] + " = bbs.command_str;";
}
// USER_EVENT <#>
baja["USER_EVENT"] = function(var1) { 
	return "bbs.user_event(" + var1 + ");";
}
// AUTO_MESSAGE
baja["AUTO_MESSAGE"] = function() { 
	var line = "if(!(user.security.restrictions&UFLAG_W)) {\r\n";
	line += "bbs.auto_msg();\r\n";
	line += "}";
	return line;
}
// USER_DEFAULTS
baja["USER_DEFAULTS"] = function() { 
	var line = "if(!(user.security.restrictions&UFLAG_G)) {\r\n";
	line += "bbs.user_config();\r\n";
	line += "}";
	return line;
}
// USER_EDIT
baja["USER_EDIT"] = function() { 
	var line = "if(user.compare_ars(\"SYSOP\") || (bbs.sys_status&SS_TMPSYSOP)) {\r\n";
	line += "bbs.edit_user();\r\n";
	line += "}";
	return line;
}
// TEXT_FILE_SECTION
baja["TEXT_FILE_SECTION"] = function() { 
	return "bbs.text_sec();";
}
// XTRN_EXEC
baja["XTRN_EXEC"] = function() { 
	var line = 'if(typeof xtrn_area.prog[bbs.command_str] != "undefined" &&';
	line +=	'user.compare_ars(xtrn_area.prog[bbs.command_str].ars)) {\r\n';
	line += 'bbs.exec_xtrn(bbs.command_str);\r\n';
	line += '}';
	return line;
}
// XTRN_SECTION
baja["XTRN_SECTION"] = function() { 
	return "bbs.xtrn_sec();";
}
// MINUTE_BANK
baja["MINUTE_BANK"] = function() { 
	return "bbs.time_bank();";
}
// CHANGE_USER
baja["CHANGE_USER"] = function() { 
	var line = "if(user.compare_ars(\"SYSOP\") || (bbs.sys_status&SS_TMPSYSOP)) {\r\n";
	line += "bbs.change_user();\r\n";
	line += "}";
	return line;
}
// ANSI_CAPTURE (v2 only) *NOT SUPPORTED*
// FINDUSER
baja["FINDUSER"] = function() { 
	return "system.baja.logicState = (bbs.finduser(" + bbs.command_str + ")?TRUE:FALSE);";
}
// SELECT_SHELL
baja["SELECT_SHELL"] = function() { 
	return "system.baja.logicState = (bbs.select_shell()?TRUE:FALSE);";
}
// SET_SHELL
baja["SET_SHELL"] = function() { 
	var line = "user.command_shell = bbs.command_str;\r\n";
	line += "system.baja.logicState = (user.command_shell == bbs.command_str?TRUE:FALSE);";
	return line;
}
// SELECT_EDITOR
baja["SELECT_EDITOR"] = function() { 
	return "system.baja.logicState = (bbs.select_editor()?TRUE:FALSE);";
}
// SET_EDITOR
baja["SET_EDITOR"] = function() { 
	var line = "user.editor = bbs.command_str;\r\n";
	line += "system.baja.logicState = (user.editor == bbs.command_str?TRUE:FALSE);\r\n";
	return line;
}
// TRASHCAN <"str">
baja["TRASHCAN"] = function(var1) { 
	var line = "system.baja.logicState = (system.trashcan(system.text_dir" + var1 + ",bbs.command_str)?TRUE:FALSE);";
	line += "if(system.baja.logicState == TRUE && file_exists(system.text_dir + \"bad" + var1 + ".msg\") {\r\n";
	line += "console.printfile(system.text_dir + \"bad" + var1 + ".msg\");\r\n";
	line += "}";
	return line;
}
// GETTIMELEFT
baja["GETTIMELEFT"] = function() { 
	var line = "if(bbs.time_left() == 0) {\r\n";
	line += "console.print(\"Out of time\");\r\n";
	line += "bbs.hangup();\r\n";
	line += "}";
}
// MSWAIT <#>
baja["MSWAIT"] = function(num1) { return "mswait(" + num1 + ");"; }
// SEND_FILE_VIA <char> <"str" or str_var>
baja["SEND_FILE_VIA"] = function(var1,var2) { 
	return format("bbs.send_file(%s,%s);",var2,var1); 
}
// RECEIVE_FILE_VIA <char> <"str" or str_var>
baja["RECEIVE_FILE_VIA"] = function(var1,var2) { 
	return format("bbs.receive_file(%s,%s);",var2,var1); 
}

// Mail Functions
// --------------
// MAIL_READ
baja["MAIL_READ"] = function() {
	return 'bbs.read_mail(MAIL_YOUR);';
}
// MAIL_READ_SENT
baja["MAIL_READ_SENT"] = function() {
	return 'bbs.read_mail(MAIL_SENT);';
}
// MAIL_READ_ALL
baja["MAIL_READ_ALL"] = function() {
	return 'if(bbs.check_syspass()) bbs.read_mail(MAIL_ALL);';
}
// MAIL_SEND
baja["MAIL_SEND"] = function() {
	return 'bbs.email(bbs.command_str);';
}
// MAIL_SEND_FILE
baja["MAIL_SEND_FILE"] = function() {
	return 'bbs.email(bbs.command_str, WM_EMAIL|WM_FILE);';
}
// MAIL_SEND_BULK
baja["MAIL_SEND_BULK"] = function() {
	return 'bbs.bulk_mail();';
}
// MAIL_SEND_FEEDBACK
baja["MAIL_SEND_FEEDBACK"] = function() {
	return
		'var u = new User(1);\r\n' +
		'bbs.email(u.alias, subject="Re: Feedback");';
}
// MAIL_SEND_NETMAIL
baja["MAIL_SEND_NETMAIL"] = function() {
	return
		'system.baja.userInput = console.getstr("", mode=K_LINE);\r\n' +
		'bbs.netmail(system.baja.userInput);';
}
// MAIL_SEND_NETFILE
baja["MAIL_SEND_NETFILE"] = function() {
	return
		'system.baja.userInput = console.getstr("", mode=K_LINE);\r\n' +
		'bbs.netmail(system.baja.userInput, mode=WM_NETMAIL|WM_FILE);'
}

// Message Base Functions
// ----------------------
// MSG_SET_AREA
baja["MSG_SET_AREA"] = function() {
	return
		'if(typeof msg_area.sub[bbs.command_str] == "undefined" || !user.compare_ars(msg_area.sub[bbs.command_str].ars)) {\r\n' +
		'system.baja.logicState = FALSE;\r\n' +
		'} else {\r\n' +
		'system.baja.logicState = TRUE;\r\n' +
		'bbs.cursub = msg_area.sub[bbs.command_str].number;\r\n' +
		'bbs.cursub_code = bbs.command_str;\r\n' +
		'}';
}
// MSG_SET_GROUP
baja["MSG_SET_GROUP"] = function() {
	return
		'if(typeof msg_area.grp[bbs.command_str] == "undefined" || !user.compare_ars(msg_area.grp[bbs.command_str].ars)) {\r\n' +
		'system.baja.logicState = FALSE;\r\n' +
		'} else {\r\n' +
		'system.baja.logicState = TRUE;\r\n' +
		'bbs.curgrp = msg_area.grp[bbs.command_str].number;\r\n' +
		'}';
}
// MSG_SELECT_AREA
baja["MSG_SELECT_AREA"] = function() {
	return
		'system.baja.logicState = FALSE;' +
		'for(var g = 0; g < msg_area.grp_list.length; g++) {\r\n' +
		'console.uselect(g + 1, "Message Group", msg_area.grp_list[g].description);\r\n' +
		'}\r\n' +
		'var grp = console.uselect();\r\n' +
		'if(grp > 0 && grp <= msg_area.grp_list.length) {\r\n' +
		'for(var s = 0; s < msg_area.grp_list[grp - 1].sub_list.length; s++) {\r\n' +
		'console.uselect(s + 1, "Message Area", msg_area.grp_list[grp - 1].sub_list[s].description);\r\n' +
		'}\r\n' +
		'var sub = console.uselect();' +
		'if(sub > 0 && sub <= msg_area.grp_list[grp - 1].sub_list.length) {\r\n' +
		'system.baja.logicState = TRUE;\r\n' +
		'bbs.curgrp = msg_area.grp_list[grp - 1].number;\r\n' +
		'bbs.cursub = msg_area.grp_list[grp - 1].sub_list[sub - 1].number;\r\n' +
		'bbs.cursub_code = msg_area.grp_list[grp - 1].sub_list[sub - 1]. code;\r\n' +
		'}\r\n' +
		'}';
}
// MSG_SHOW_GROUPS
// This is fairly close to the output of the Baja function, but not exact.
// If I can determine which string from text.dat Baja is using, I will update this later. -ec
baja["MSG_SHOW_GROUPS"] = function() {
	return
		'console.putmsg(bbs.text(127));' +
		'for(var g = 0; g < msg_area.grp_list.length; g++) {\r\n' +
		'console.putmsg(format(bbs.text(128), g + 1, msg_area.grp_list[g].description, "", msg_area.grp_list[g].sub_list.length));\r\n' +
		'}';
}
// MSG_SHOW_SUBBOARDS
// Same issue as MSG_SHOW_GROUPS - probably needs to be revised later.
baja["MSG_SHOW_SUBBOARDS"] = function() {
	return
		'for(var g in msg_area.grp_list) {\r\n' +
		'if(msg_area.grp_list[g].number != bbs.curgrp)\r\n' +
		'continue;\r\n' +
		'console.putmsg(format(bbs.text(125), msg_area.grp_list[g].description));\r\n' +
		'for(var s = 0; s < msg_area.grp_list[g].sub_list.length; s++) {\r\n' +
		'var mb = new MsgBase(msg_area.grp_list[g].sub_list[s].code);\r\n' +
		'mb.open();\r\n' +
		'var tm = mb.total_msgs;\r\n' +
		'mb.close();\r\n' +
		'console.putmsg(format(bbs.text(126), s + 1, msg_area.grp_list[g].sub_list[s].description, "", tm));\r\n' +
		'}\r\n' +
		'break;' +
		'}';
}
// MSG_GROUP_UP
baja["MSG_GROUP_UP"] = function() {
	return
		'var cg = bbs.curgrp;\r\n' +
		'bbs.curgrp++;\r\n' +
		'if(cg == bbs.curgrp) bbs.curgrp = 0;\r\n';
}
// MSG_GROUP_DOWN
baja["MSG_GROUP_DOWN"] = function() {
	return
		'var cg = bbs.curgrp;\r\n' +
		'bbs.curgrp--;\r\n' +
		'if(cg == bbs.curgrp) bbs.curgrp = msg_area.grp_list.length - 1;';
}
// MSG_SUBBOARD_UP
baja["MSG_SUBBOARD_UP"] = function() {
	return
		'var cs = bbs.cursub;\r\n' +
		'bbs.cursub++;\r\n' +
		'if(cs == bbs.cursub) bbs.cursub = 0;\r\n';
}
// MSG_SUBBOARD_DOWN
baja["MSG_SUBBOARD_DOWN"] = function() {
	return
		'var cs = bbs.cursub;\r\n' +
		'bbs.cursub--;\r\n' +
		'if(cs == bbs.cursub)\r\n' +
		'bbs.cursub = msg_area.grp_list[bbs.curgrp].sub_list.length - 1;';
}
// MSG_GET_SUB_NUM
// bbs.cursub starts at zero, but Baja's msg sub & group lists start at 1, so we'll adjust for that.
baja["MSG_GET_SUB_NUM"] = function() {
	return 'bbs.cursub = console.getnum(msg_area.grp_list[bbs.curgrp].sub_list.length, bbs.cursub) - 1;';
}
// MSG_GET_GRP_NUM
baja["MSG_GET_GRP_NUM"] = function() {
	return 'bbs.curgrp = console.getnum(msg_area.grp_list.length, bbs.curgrp) - 1;';
}
// MSG_READ
baja["MSG_READ"] = function() {
	return 'bbs.scan_msgs(bbs.cursub_code, SCAN_READ);';
}
// MSG_POST
baja["MSG_POST"] = function() {
	return 'system.baja.logicState = bbs.post_msg();';
}
// MSG_QWK
baja["MSG_QWK"] = function() {
	return 'bbs.qwk_sec();';
}
// MSG_PTRS_CFG
baja["MSG_PTRS_CFG"] = function() {
	return 'bbs.cfg_msg_ptrs();';
}
// MSG_PTRS_REINIT
baja["MSG_PTRS_REINIT"] = function() {
	return 'bbs.reinit_msg_ptrs();';
}
// MSG_NEW_SCAN_CFG
baja["MSG_NEW_SCAN_CFG"] = function() {
	return 'bbs.cfg_msg_scan(SCAN_CFG_NEW);';
}
// MSG_NEW_SCAN
baja["MSG_NEW_SCAN"] = function() {
	return 'bbs.scan_subs();';
}
// MSG_NEW_SCAN_ALL
baja["MSG_NEW_SCAN_ALL"] = function() {
	return 'bbs.scan_subs(SCAN_NEW, true);';
}
// MSG_NEW_SCAN_SUB
baja["MSG_NEW_SCAN_SUB"] = function() {
	return 'system.baja.logicState = bbs.scan_msgs(bbs.cursub_code, SCAN_NEW);';
}
// MSG_CONT_SCAN
baja["MSG_CONT_SCAN"] = function() {
	return 'bbs.scan_subs(SCAN_CONST);';
}
// MSG_CONT_SCAN_ALL
baja["MSG_CONT_SCAN_ALL"] = function() {
	return 'bbs.scan_subs(SCAN_CONST, true);';
}
// MSG_BROWSE_SCAN
baja["MSG_BROWSE_SCAN"] = function() {
	return 'bbs.scan_subs(SCAN_BACK);';
}
// MSG_BROWSE_SCAN_ALL
baja["MSG_BROWSE_SCAN_ALL"] = function() {
	return 'bbs.scan_subs(SCAN_BACK, true);';
}
// MSG_FIND_TEXT
baja["MSG_FIND_TEXT"] = function() {
	return 'bbs.scan_subs(SCAN_FIND);';
}
// MSG_FIND_TEXT_ALL
baja["MSG_FIND_TEXT_ALL"] = function() {
	return 'bbs.scan_subs(SCAN_FIND, true);';
}
// MSG_YOUR_SCAN_CFG
baja["MSG_YOUR_SCAN_CFG"] = function() {
	return 'bbs.cfg_msg_scan(SCAN_CFG_TOYOU);';
}
// MSG_YOUR_SCAN
baja["MSG_YOUR_SCAN"] = function() {
	return 'bbs.scan_subs(SCAN_NEW|SCAN_TOYOU);';
}
// MSG_YOUR_SCAN_ALL
baja["MSG_YOUR_SCAN_ALL"] = function() {
	return 'bbs.scan_subs(SCAN_NEW|SCAN_TOYOU, true);';
}

// File Base Functions
// -------------------
// FILE_SET_AREA
baja["FILE_SET_AREA"] = function() {
	return
		'if(typeof file_area.dir[bbs.command_str.toLowerCase()] != "undefined" && bbs.compare_ars(file_area.dir[bbs.command_str.toLowerCase()].ars)) {\r\n' +
	    'bbs.curdir = file_area.dir[bbs.command_str.toLowerCase()].number;\r\n' +
		'bbs.curdir_code = file_area.dir[bbs.command_str.toLowerCase()].code;\r\n' +
		'system.baja.logicState = true;\r\n' +
		'} else {\r\n' +
		'system.baja.logicState = false;\r\n' +
		'}';
}
// FILE_SET_LIBRARY
baja["FILE_SET_LIBRARY"] = function() {
	return
		'if(typeof file_area.lib[bbs.command_str.toLowerCase()] != "undefined" && bbs.compare_ars(file_area.lib[bbs.command_str.toLowerCase()].ars)) {\r\n' +
		'bbs.curlib = file_area.lib[bbs.command_str.toLowerCase()].number;\r\n' +
		'system.baja.logicState = true;\r\n' +
		'} else {\r\n' +
		'system.baja.logicState = false;\r\n' +
		'}';
}
// FILE_SELECT_AREA
baja["FILE_SELECT_AREA"] = function() {
	return
		baja["FILE_SHOW_LIBRARIES"]() +
		"\r\n" +
		"bbs.curlib = console.getnum(file_area.lib_list.length, bbs.curdir + 1) - 1;\r\n" +
		baja["FILE_SHOW_DIRECTORIES"]() +
		"bbs.curdir = console.getnum(file_area.lib_list[bbs.curlib].dir_list.length, 1) - 1;\r\n";
}
// FILE_SHOW_LIBRARIES
baja["FILE_SHOW_LIBRARIES"] = function() {
	return
		'console.putmsg(bbs.text(181));\r\n' +
		'for(var l = 0; l < file_area.lib_list.length; l++) {\r\n' +
		'console.putmsg(format(bbs.text(182), l + 1, file_area.lib_list[l].description, "",file_area.lib_list[l].dir_list.length));\r\n' +
		'}';
}
// FILE_SHOW_DIRECTORIES
baja["FILE_SHOW_DIRECTORIES"] = function() {
	return 'console.putmsg(format(bbs.text(179), file_area.lib_list[bbs.curlib].description));' +
		'for(var d = 0; d < file_area.lib_list[bbs.curlib].dir_list.length; d++) {\r\n' +
		'console.putmsg(format(bbs.text(180), d, ' +
		'file_area.lib_list[bbs.curlib].dir_list[d].description, "",' +
		'directory(file_area.lib_list[bbs.curlib].dir_list[d].path + "*.*").length));\r\n' +
		'}';
}
// FILE_LIBRARY_UP
baja["FILE_LIBRARY_UP"] = function() {
	return
		'var cl = bbs.curlib;\r\n' +
		'bbs.curlib++;\r\n' +
		'if(cl == bbs.curlib) bbs.curlib = 0;\r\n';
}
// FILE_LIBRARY_DOWN
baja["FILE_LIBRARY_DOWN"] = function() {
	return
		'var cl = bbs.curlib;\r\n' +
		'bbs.curlib--;\r\n' +
		'if(cl == bbs.curlib) bbs.curlib = file_area.lib_list.length - 1;';
}
// FILE_DIRECTORY_UP
baja["FILE_DIRECTORY_UP"] = function() {
	return
		'var cd = bbs.curdir;\r\n' +
		'bbs.curdir++;\r\n' +
		'if(cd == bbs.curdir) bbs.curdir = 0;\r\n';
}
// FILE_DIRECTORY_DOWN
baja["FILE_DIRECTORY_DOWN"] = function() {
	return
		'var cd = bbs.curdir;\r\n' +
		'bbs.curdir--;\r\n' +
		'if(cd == bbs.curdir)\r\n' +
		'bbs.curdir = file_area.lib_list[bbs.curlib].dir_list.length - 1;';
}
// FILE_GET_DIR_NUM
baja["FILE_GET_DIR_NUM"] = function() {
	return 'bbs.curdir = console.getnum(file_area.lib_list[bbs.curlib].dir_list.length, bbs.curdir) - 1;';
}
// FILE_GET_LIB_NUM
baja["FILE_GET_LIB_NUM"] = function() {
	return 'bbs.curlib = console.getnum(file_area.lib_list.length, bbs.curlib) - 1;';
}
// FILE_LIST
baja["FILE_LIST"] = function() {
	return
		'var res = bbs.list_files(bbs.curdir_code, bbs.command_str);' +
		'system.baja.logicState = (res < 0) ? FALSE : TRUE;';
}
// FILE_LIST_EXTENDED
baja["FILE_LIST_EXTENDED"] = function() {
// Baja docs do not state that this function affects the logic state.
// This will need to be verified, the commented code here used instead if so.
//	var line = 'var res = bbs.list_file_info(bbs.curdir_code, bbs.command_str);';
//	line += 'system.baja.logicState = (res < 0) ? FALSE : TRUE;';
//	return line;
	return 'bbs.list_file_info(bbs.curdir_code, bbs.command_str);';
}
// FILE_VIEW
baja["FILE_VIEW"] = function() {
	return 'bbs.list_files(bbs.curdir_code, bbs.command_str, FL_VIEW);';
}
// FILE_UPLOAD
baja["FILE_UPLOAD"] = function() {
	return 'system.baja.logicState = bbs.upload_file();';
}
// FILE_UPLOAD_USER
baja["FILE_UPLOAD_USER"] = function() {
	return 'system.baja.logicState = bbs.upload_file("USER");';
}
// FILE_UPLOAD_SYSOP
baja["FILE_UPLOAD_SYSOP"] = function() {
	return 'system.baja.logicState = bbs.upload_file("SYSOP");';
}
// FILE_DOWNLOAD
baja["FILE_DOWNLOAD"] = function() {
	return 'if(bbs.check_syspass()) bbs.send_file(bbs.command_str);';
}
// FILE_DOWNLOAD_USER
baja["FILE_DOWNLOAD_USER"] = function() {
	return 'system.baja.logicState = bbs.list_files("USER", mode=FI_USERXFER);';
}
// FILE_DOWNLOAD_BATCH
baja["FILE_DOWNLOAD_BATCH"] = function() { return 'system.baja.logicState = (bbs.batch_download()) ? TRUE : FALSE;'; }
// FILE_REMOVE
baja["FILE_REMOVE"] = function() { return 'bbs.list_file_info(bbs.curdir_code, bbs.command_str, FI_REMOVE);'; }
// FILE_BATCH_ADD
baja["FILE_BATCH_ADD"] = function() {
	return
		'var fn = format("%s/user/%04d.batch", system.data_dir, user.number);\r\n' +
		'var f = new File(fn);\r\n' +
		'f.open("w");\r\n' +
		'f.write(bbs.command_str);\r\n' +
		'f.close();\r\n' +
		'bbs.batch_add_list(fn);';
}
// FILE_BATCH_ADD_LIST
baja["FILE_BATCH_ADD_LIST"] = function() { 	return 'bbs.batch_add_list(bbs.command_str);'; }
// FILE_BATCH_CLEAR
baja["FILE_BATCH_CLEAR"] = function() { return 'bbs.batch_dnload_total = 0;'; }
// FILE_BATCH_SECTION
baja["FILE_BATCH_SECTION"] = function() { return 'bbs.batch_menu();'; }
// FILE_TEMP_SECTION
baja["FILE_TEMP_SECTION"] = function() { return 'bbs.temp_xfer();'; }
// FILE_NEW_SCAN
baja["FILE_NEW_SCAN"] = function() { return 'bbs.scan_dirs();'; }
// FILE_NEW_SCAN_ALL
baja["FILE_NEW_SCAN_ALL"] = function() { return 'bbs.scan_dirs(FL_NONE, true);'; }
// FILE_FIND_TEXT
baja["FILE_FIND_TEXT"] = function() { return 'bbs.scan_dirs(FL_FINDDESC);'; }
// FILE_FIND_TEXT_ALL
baja["FILE_FIND_TEXT_ALL"] = function() { return 'bbs.scan_dirs(FL_FINDDESC, true);'; }
// FILE_FIND_NAME
baja["FILE_FIND_NAME"] = function() { return 'bbs.list_files(bbs.curdir_code, bbs.get_filespec());'; }
// FILE_FIND_NAME_ALL
baja["FILE_FIND_NAME_ALL"] = function() {
	return
		'system.baja.temp = bbs.get_filespec();\r\n' +
		'for(var d in file_area.dir) {\r\n' +
		'if(!file_area.dir[d].can_download) continue;\r\n' +
		'bbs.list_files(file_area.dir[d].code, system.baja.temp);\r\n' +
		'}';
}
// FILE_PTRS_CFG
baja["FILE_PTRS_CFG"] = function() {
	return 'system.baja.logicState = (bbs.last_new_file_time == bbs.get_newscantime()) ? FALSE : TRUE;';
}
// FILE_SET_ALT_PATH
baja["FILE_SET_ALT_PATH"] = function() { return 'if(bbs.check_syspass()) bbs.alt_ul_dir = bbs.command_str;'; }
// FILE_RESORT_DIRECTORY
baja["FILE_RESORT_DIRECTORY"] = function() {
	return
		'if(bbs.command_str == "ALL") {\r\n' +
		'for(var d in file_area.dir)\r\n' +
		'bbs.resort_dir(file_area.dir[d].code);\r\n' +
		'} else if(bbs.command_str == "LIB") {\r\n' +
		'for(var d in file_area.lib_list[bbs.curlib].dir_list)\r\n' +
		'bbs.resort_dir(file_area.lib_list[bbs.curlib].dir_list[d].code);\r\n' +
		'} else {\r\n' +
		'bbs.resort_dir(bbs.curdir_code);\r\n' +
		'}';
}
// FILE_SEND
baja["FILE_SEND"] = function() {
	return 'bbs.send_file(bbs.command_str);';
}
// FILE_GET
baja["FILE_GET"] = function() {
	return 'if(bbs.check_syspass()) bbs.send_file(bbs.command_str);';
}
// FILE_PUT
baja["FILE_PUT"] = function() {
	return 'if(bbs.check_syspass()) bbs.receive_file(bbs.command_str);';
}
// FILE_UPLOAD_BULK
baja["FILE_UPLOAD_BULK"] = function() {
	return
		'if(bbs.command_str == "ALL") {\r\n' +
		'for(var d in file_area.dir)\r\n' +
		'bbs.bulk_upload(file_area.dir[d].code);\r\n' +
		'} else if(bbs.command_str == "LIB") {\r\n' +
		'for(var d in file_area.lib_list[bbs.curlib].dir_list)\r\n' +
		'bbs.bulk_upload(file_area.lib_list[bbs.curlib].dir_list[d].code);\r\n' +
		'} else {\r\n' +
		'bbs.bulk_upload(bbs.curdir_code);\r\n' +
		'}';
}
// FILE_FIND_OLD
baja["FILE_FIND_OLD"] = function() {
	return 'if(bbs.check_syspass()) bbs.list_file_info(bbs.command_str, mode=FI_OLD);';
}
// FILE_FIND_OPEN
baja["FILE_FIND_OPEN"] = function() { return 'bbs.list_file_info(bbs.command_str, mode=FI_CLOSE);'; }
// FILE_FIND_OFFLINE
baja["FILE_FIND_OFFLINE"] = function() { return 'bbs.list_file_info(bbs.command_str, mode=FI_OFFLINE);'; }
// FILE_FIND_OLD_UPLOADS
baja["FILE_FIND_OLD_UPLOADS"] = function() {
	return 'if(bbs.check_syspass()) bbs.list_file_info(bbs.command_str, mode=FI_OLDUL);';
}

// Chat Functions
// --------------
// PAGE_SYSOP
baja["PAGE_SYSOP"] = function() { return 'bbs.page_sysop();'; }
// PAGE_GURU
baja["PAGE_GURU"] = function() { return 'bbs.page_guru(bbs.command_str);'; }
// PRIVATE_CHAT
baja["PRIVATE_CHAT"] = function() { return 'bbs.private_chat();'; }
// PRIVATE_MESSAGE
baja["PRIVATE_MESSAGE"] = function() { return 'bbs.private_message();'; }
// CHAT_SECTION
baja["CHAT_SECTION"] = function() { return 'load("chat_sec.js");'; }

// Information Functions
// ---------------------
// INFO_SYSTEM
baja["INFO_SYSTEM"] = function() { return 'bbs.sys_info();'; }
// INFO_SUBBOARD
baja["INFO_SUBBOARD"] = function() { return 'bbs.sub_info();'; }
// INFO_DIRECTORY
baja["INFO_DIRECTORY"] = function() { return 'bbs.dir_info();'; }
// INFO_USER
baja["INFO_USER"] = function() { return 'bbs.user_info();'; }
// INFO_VERSION
baja["INFO_VERSION"] = function() { return 'bbs.ver();'; }
// INFO_XFER_POLICY
baja["INFO_XFER_POLICY"] = function() { return 'bbs.xfer_policy();'; }
// GURU_LOG
baja["GURU_LOG"] = function() { 
	var line = "";
	if(!functions["gurulog"]) {
		line += "system.baja.gurulog = " + gurulog.toSource() + ";\r\n";
		functions["gurulog"] = true;
	}
	return line + "gurulog(\"guru.log\");";
}
// ERROR_LOG
baja["ERROR_LOG"] = function() { 
	var line = "";
	if(!functions["errlog"]) {
		line += "system.baja.errlog = " + errlog.toSource() + ";\r\n";
		functions["errlog"] = true;
	}
	return line + "system.baja.errlog(\"error.log\");";
}
// SYSTEM_LOG
baja["SYSTEM_LOG"] = function() { 
	var line = "";
	if(!functions["syslog"]) {
		line += "system.baja.syslog = " + syslog.toSource() + ";\r\n";
		functions["syslog"] = true;
	}
	return line + "system.baja.syslog(strftime(\"logs/%m%d%y.log\",time()));";
}
// SYSTEM_YLOG
baja["SYSTEM_YLOG"] = function() { 
	var line = "";
	if(!functions["syslog"]) {
		line += "system.baja.syslog = " + syslog.toSource() + ";\r\n";
		functions["syslog"] = true;
	}
	return line + "system.baja.syslog(strftime(\"logs/%m%d%y.log\",time()-24*60*60));";
}
// SYSTEM_STATS
baja["SYSTEM_STATS"] = function() { return 'bbs.sys_stats();'; }
// NODE_STATS
baja["NODE_STATS"] = function() { return 'bbs.node_stats();'; }
// SHOW_MEM

// File I/O Functions
// ------------------
// FOPEN <int_var> <#> <"str" or str_var>
baja["FOPEN"] = function(var1,var2,var3) { 
	var line = var1 + " = new File(" + var3 + ");\r\n";
	var2 = var2.toUpperCase().replace(/O_/ig,'').split('|');
	var share = 'false';
	var mode = '';
	// O_RDONLY	Read Only
	// O_WRONLY	Write Only
	// O_RDWR		Read and write
	// O_CREAT 	Create	  (create if doesn't exist)
	// O_APPEND	Append	  (writes to end of file)
	// O_TRUNC 	Truncate  (truncates file to 0 bytes automatically)
	// O_EXCL		Exclusive (only open/create if file doesn't exist)
	// O_DENYNONE	Deny None (shareable, for use with record locking)	
	var o = {};
	for(var i=0;i<var2.length;i++) 
		o[var2[i]] = true;
	if(o.DENYNONE)
		share = "true";
// r  open for reading; if the file does not exist or cannot be found, the open call fails
	if(o.RDONLY && !o.CREATE)
		mode = 'r';
// w  open an empty file for writing; if the given file exists, its contents are destroyed
	else if(o.WRONLY && o.CREATE && o.TRUNC)
		mode = 'w';
// a  open for writing at the end of the file (appending); creates the file first if it doesn’t exist
	else if(o.APPEND && !o.RDONLY && o.CREAT)
		mode = 'a';
// r+ open for both reading and writing (the file must exist)
	else if(o.RDWR && !o.CREAT && !o.TRUNC)
		mode = 'r+';
// w+ open an empty file for both reading and writing; if the given file exists, its contents are destroyed
	else if(o.RDWR && (o.CREAT || o.TRUNC))
		mode = 'r+';
// a+ open for reading and appending
	else if(o.APPEND && o.RDONLY)
		mode = 'a';
// b  open in binary (untranslated) mode; translations involving carriage-return and linefeed characters are suppressed (e.g. r+b)
// e  open a non-shareable file (that must not already exist) for exclusive access (introduced in 
	else if(o.EXCL)
		mode = 'e';
	var line = "if(system.baja.etx) " + var1 + ".etx = system.baja.etx;";
	return line + "system.baja.logicState = (" + var1 + ".open('" + mode + "'," + share + ")?TRUE:FALSE);";
}
// FCLOSE <int_var>
baja["FCLOSE"] = function(var1) { return var1 + ".close();"; }
// FREAD <int_var> <any_var> [int_var or #]
/* needs to check the length of the variable if the passed var2 is
a string. if that string is 0 length, then 128 bytes are read */
baja["FREAD"] = function(var1,var2,var3) { 
	if(variables[var2] == "STR") {
		if(var3)
			return var2 + " = " + var1 + ".read(" + var3 + ");";
		else 
			return var2 + " = " + var1 + ".read(" + var2 + ".length?" + var2 + ".length:128);";
	}
	if(variables[var2] == "INT") {
		return var2 + " = " + var1 + ".readBin(" + var3 + ");";
	}
}
// FWRITE <int_var> <any_var> [int_var or #]
baja["FWRITE"] = function(var1,var2,var3) { 
	if(variables[var2] == "STR") {
		return var1 + ".write(" + var2 + ", " + (var3?var3:var2+".length") + ");";
	}
	if(variables[var2] == "INT") {
		return var1 + ".writeBin(" + var2 + ", " + (var3?var3:"4") + ");";
	}
}
// FFLUSH <int_var>
baja["FFLUSH"] = function(var1) { return var1 + ".flush();"; }
// FGET_LENGTH <int_var> <int_var>
baja["FGET_LENGTH"] = function(var1,var2) { return var2 + " = " + var1 + ".length;"; }
// FSET_LENGTH <int_var> <int_var or #>
baja["FSET_LENGTH"] = function(var1,var2) { return var1 + ".length = " + var2 + ";"; }
// FGET_TIME <int_var> <int_var>
baja["FGET_TIME"] = function(var1,var2) { return var2 + " = " + var1 + ".date;"; }
// FSET_TIME <int_var> <int_var>
/* not sure if the baja method uses time_t or UTC (time_t * 1000) */
baja["FSET_TIME"] = function(var1,var2) { return var1 + ".date = " + (var2*1000) + ";"; }
// FEOF <int_var>
baja["FEOF"] = function(var1) { return "system.baja.logicState = (" + var1 + ".eof?TRUE:FALSE);"; }
// FGET_POS <int_var> <int_var>
baja["FGET_POS"] = function(var1,var2) { return var2 + " = " + var1 + ".position;"; }
// FSET_POS <int_var> <int_var or #> [#]
baja["FSET_POS"] = function(var1,var2) { return var1 + ".position = " + var2 + ";"; }
// FLOCK <int_var> <int_var or #>
baja["FLOCK"] = function(var1,var2) { return var1 + ".lock(" + var1 + ".position, " + var2 + ");"; }
// FUNLOCK <int_var> <int_var or #>
baja["FUNLOCK"] = function(var1,var2) { return var1 + ".unlock(" + var1 + ".position, " + var2 + ");"; }
// FPRINTF <int_var> <"cstr"> [any_var] [...]
baja["FPRINTF"] = function() {
	var line = var1 + ".printf(" + var2;
	for(var i=2;i<arguments.length;i++) 
		line += ", " + arguments[i];
	return line + ");";
}
// FREAD_LINE <int_var> <any_var>
baja["FREAD_LINE"] = function(var1, var2) { return var2 + " = " + var1 + ".readln();"; }
// FSET_ETX <#>
/* i dont know what to do with this, really */
baja["FSET_ETX"] = function(var1) { return "system.baja.etx = " + var1 + ";"; }

// File System Functions
// ---------------------
// CHKFILE <"str" or str_var>
baja["CHKFILE"] = function(var1) {
	return 'system.baja.logicState = file_exists(' + var1 + ');';
}
// REMOVE_FILE <str_var>
baja["REMOVE_FILE"] = function(var1) {
	return 'system.baja.logicState = file_removecase(' + var1 + ');';
}
// RENAME_FILE <str_var> <str_var>
baja["RENAME_FILE"] = function(var1, var2) {
	return 'system.baja.logicState = file_rename(' + var1 +',' + var2 + ');';
}
// COPY_FILE <str_var> <str_var>
baja["COPY_FILE"] = function(var1, var2) {
	return 'system.baja.logicState = file_copy(' + var1 +',' + var2 + ');';
}
// MOVE_FILE <str_var> <str_var>
baja["MOVE_FILE"] = baja["RENAME_FILE"];
// GET_FILE_ATTRIB <int_var> <str_var>
baja["GET_FILE_ATTRIB"] = function(var1, var2) {
	return var1 + ' = file_attrib(' + var2 + ');';
}
// SET_FILE_ATTRIB <int_var> <str_var>
// Not sure if this is sufficient ... needs testing.
baja["SET_FILE_ATTRIB"] = function(var1, var2) {
	return
		'if(file_exists(var2)) {\r\n' +
		'system.baja.temp = new File(var2);\r\n' +
		'system.baja.temp.attributes = var1;\r\n' +
		'}';
}
// GET_FILE_TIME <int_var> <str_var>
baja["GET_FILE_TIME"] = function(var1, var2) {
	return var1 + ' = file_date(' + var2 + ');';
}
// GET_FILE_LENGTH <int_var> <str_var>
baja["GET_FILE_LENGTH"] = function(var1, var2) {
	return var1 + ' = file_size(' + var2 + ');';
}

// Directory System Functions
// --------------------------
// MAKE_DIR <str_var>
baja["MAKE_DIR"] = function(var1) {
	return
		'system.baja.logicState = (mkdir(' + var1 + ')) ? TRUE : FALSE;\r\n' +
		'if(!system.baja.logicState) system.baja.errNo = errno;';
}
// CHANGE_DIR <str_var>
baja["CHANGE_DIR"] = function(var1) {
	return
		'if(!file_exists(' + var1 + ') || file_isdir(' + var1 + ')) {\r\n' +
		'system.baja.logicState = FALSE;\r\n' +
		'system.baja.errNo = 3;\r\n' + // Path not found.  Kthnx?
		'} else {\r\n' +
		'system.baja.logicState = TRUE;\r\n' +
		'js.exec_dir = ' + var1 + ';\r\n' +
		'}';
}
// REMOVE_DIR <str_var>
baja["REMOVE_DIR"] = function(var1) {
	return
		'system.baja.logicState = (rmdir(' + var1 + ')) ? TRUE : FALSE;\r\n' +
		'if(!system.baja.logicState) system.baja.errNo = errno;';
}
// OPEN_DIR <int_var> <str_var>
baja["OPEN_DIR"] = function(var1,var2) {
	var line = "system.baja.logicState = (file_isdir(" + var2 + ")?TRUE:FALSE);\r\n";
	line += var1 + " = directory(" + var2 + ");";
	return line;
}
// READ_DIR <int_var> <str_var>
baja["READ_DIR"] = function(var1,var2) {
	var line = "system.baja.logicState = (" + var1 + ".length > 0?TRUE:FALSE);\r\n";
	return var2 + " = " + var1 + "[++system.baja.dir];";
}
// REWIND_DIR <int_var>
baja["REWIND_DIR"] = function(var1) {
	return "system.baja.dir = 0;";
}
// CLOSE_DIR <int_var>

/***************************** SUPPORT FUNCTIONS **************************/
function compare(var1,var2) {
	system.baja.logicState = 0;
	if(var1 == var2) 
		system.baja.logicState |= EQUAL;
	else if(var1 > var2) 
		system.baja.logicState = GREATER;
	else if(var1 < var2) 
		system.baja.logicState = EQUAL;
}

function errlog() {
	if((user.compare_ars("SYSOP") || (bbs.sys_status&SS_TMPSYSOP)) && bbs.check_syspass()) {
		var errlog=system.logs_dir+"error.log";
		if(file_exists(errlog)) {
			write(bbs.text(ErrorLogHdr));
			console.printfile(errlog);
			if(!console.noyes(bbs.text(DeleteErrorLogQ)))
				file_remove(errlog);
		}
		else {
			write(format(bbs.text(FileDoesNotExist),errlog));
		}
		var i = 0;
		for(;i<system.nodes;i++) {
			if(system.node_list[i].errors)
				break;
		}
		if(i<system.nodes) {
			if(!console.noyes(bbs.text(ClearErrCounter))) {
				for(i=0;i<system.nodes;i++) {
					system.node_list[i].errors=0;
				}
			}
		}
	}
}

function syslog(file) {
	if((user.compare_ars("SYSOP") || (bbs.sys_status&SS_TMPSYSOP)) && bbs.check_syspass()) {
		var str=system.logs_dir+file;
		console.printfile(str);
	}
}

function gurulog() {
	if((user.compare_ars("SYSOP") || (bbs.sys_status&SS_TMPSYSOP)) && bbs.check_syspass()) {
		if(file_exists(system.logs_dir+"guru.log")) {
			console.printfile(system.logs_dir+"guru.log");
			console.crlf();
			if(!console.noyes(bbs.text(DeleteGuruLogQ)))
				file_remove(system.logs_dir+"guru.log");
		}
	}
}


