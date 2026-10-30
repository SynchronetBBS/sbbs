/* Exercise the actual configurator with scripted UIFC input and private files. */
"use strict";
load(js.exec_dir + "lib.sjs");

function check(value, message) { if (!value) throw new Error(message || "Assertion failed"); }
var editor_files = [];
function editor_path(suffix) {
    var path = IRCD_TEST_TEMP_DIR + "ircd-editor-" + random(0x7fffffff) + suffix;
    editor_files.push(path);
    return path;
}
var editor_config = [
    "[Info]", "Servername=SYSTEM_QWKID.example.test", "Networkname=TestNet",
    "[Port:6667]", "Default=true", "[Class:1]", "PingFrequency=120",
    "ConnectFrequency=0", "Maximum=100", "SendQ=1000000",
    "[Class:30]", "PingFrequency=120", "ConnectFrequency=60", "Maximum=10", "SendQ=1000000",
    "[Services]", "Disabled=true"
].join("\n");

function editor(options) {
    options = options || {};
    var path = editor_path(options.suffix || ".ini");
    if (!options.missing) {
        var file = new File(path);
        check(file.open("w"));
        file.write(options.config === undefined ? editor_config : options.config);
        file.close();
    }
    var s = {steps:options.steps || [{title:"Main Menu", pick:"Quit Immediately (No Save)"}],
        inputs:options.inputs || [], messages:[], warnings:[], bailed:0,
        CLines:[], NLines:[], HLines:[], ILines:[], KLines:[], OLines:[], PLines:[],
        QLines:[], ULines:[], YLines:[], ZLines:[], WLines:[], RBL:[],
        ServerName:"", ServerDesc:"", Default_Port:6667, Admin1:"", Admin2:"", Admin3:"",
        Die_Password:"", Restart_Password:"", CapabEnforce:false};
    s.uifc = {
        init:function() { return true; }, bail:function() { s.bailed++; },
        msg:function(message) { s.messages.push(message); },
        input:function() {
            check(s.inputs.length, "Unexpected input: " + arguments[1]);
            return s.inputs.shift();
        },
        list:function(mode, title, choices, ctx) {
            if (title == "Help" || title == " ") return -1;
            check(s.steps.length, "Unexpected menu: " + title);
            var step = s.steps.shift();
            check(typeof step.title == "string" ? step.title == title : step.title.test(title),
                "Expected " + step.title + "; got " + title);
            if (step.before) step.before(s, choices, ctx);
            if (typeof step.pick == "number") return step.pick;
            var index = choices.indexOf(step.pick);
            check(index >= 0, "Missing menu choice: " + step.pick);
            return index;
        }
    };
    s.uifc.list.CTX = function() { this.cur = 0; };
    s.log = function(level, message) { s.warnings.push(message); };
    load(s, js.exec_dir + "../../ircdcfg.js", "-f", path);
    check(!s.steps.length && !s.inputs.length, "Unused startup input");
    check(s.bailed == 1, "UIFC was not closed");
    return s;
}

function menu(s, name, steps, inputs, argument) {
    s.steps = steps;
    s.inputs = inputs || [];
    s[name](argument);
    check(!s.steps.length && !s.inputs.length, "Unused input in " + name);
}
function edit_settings(s, method, title, fields) {
    var steps = [], inputs = [];
    fields.forEach(function(field) {
        steps.push({title:title, pick:field[0]});
        if (field.length == 3) steps.push({title:field[1], pick:field[2]});
        else inputs.push(field[1]);
    });
    steps.push({title:title, pick:-1});
    menu(s, method, steps, inputs);
}

try {
    run_tests({
        "missing configuration initializes defaults without writing a file": function() {
            var s = editor({missing:true});
            check(s.NetworkName == "Synchronet" && s.ServerName && s.YLines[0]);
            check(!file_exists(s.Config_Filename));
        },
        "uppercase INI files preserve macros and an explicit empty services trust list": function() {
            var s = editor({suffix:".INI"});
            check(s.ServerName == "SYSTEM_QWKID.example.test" && s.NetworkName == "TestNet");
            check(!s.ULines.length);
            check(s.Write_Config_File(s.Config_Filename));
            s.Read_Config_File();
            check(!s.ULines.length, "Saving an empty trust list added trusted services");
            check(s.ServerName == "SYSTEM_QWKID.example.test");
        },
        "information edits validate the network name and preserve cleared text": function() {
            var s = editor();
            menu(s, "Info_Menu", [
                {title:"Server Information", pick:1}, {title:"Server Information", pick:2},
                {title:"Server Information", pick:3}, {title:"Server Information", pick:4},
                {title:"Server Information", pick:5}, {title:"Server Information", pick:5},
                {title:"Server Information", pick:-1}
            ], ["", "", "", "", "Invalid Network", "OtherNet"]);
            check(s.NetworkName == "OtherNet" && s.messages.length == 1);
            check(s.Write_Config_File(s.Config_Filename)); s.Read_Config_File();
            check(s.ServerDesc === "" && s.Admin1 === "" && s.Admin2 === "" && s.Admin3 === "",
                "Cleared text changed on reload: " + JSON.stringify([s.ServerDesc, s.Admin1, s.Admin2, s.Admin3]));
            check(s.NetworkName == "OtherNet");
        },
        "blank values outside Info retain their existing defaults": function() {
            var s = editor({config:editor_config + "\n[TLS]\nCAFile=\n[BBSServices]\nNickServ=\nRequireTLS=\n"
                + "[EmbeddedServices]\nEnabled=\nHost=\n[FileServ]\nTimeout=\nTLS=\n"});
            check(!s.TLS_Config.ca_file && !s.EmbeddedServices_Config.nickserv && !s.EmbeddedServices_Config.bbs_require_tls);
            check(!s.EmbeddedServices_Config.enabled && s.EmbeddedServices_Config.host == "services.synchro.net");
            check(s.EmbeddedServices_Config.files.timeout == 60 && s.EmbeddedServices_Config.files.tls == "optional");
        },
        "default allow rules contain a real ILine and optional fields survive reload": function() {
            var s = editor();
            menu(s, "Allow_Menu", [
                {title:"No default allow rule found. Create a default one?", pick:0},
                {title:"Allow Rules", pick:-1}
            ]);
            check(s.ILines.length == 1 && s.ILines[0].hostmask == "*@*");
            menu(s, "Edit_Allow", [
                {title:"Editing Allow Rule", pick:2}, {title:"Editing Allow Rule", pick:3},
                {title:"Editing Allow Rule", pick:-1}
            ], ["private-test-password", "6697"], 0);
            check(s.Write_Config_File(s.Config_Filename)); s.Read_Config_File();
            check(s.ILines[0].password == "private-test-password" && s.ILines[0].port == 6697);
        },
        "ban reason edits update the reason": function() {
            var s = editor(); s.KLines.push(new s.KLine("*@example.test", "Old", "K"));
            menu(s, "Edit_Ban", [{title:"Editing Ban", pick:1}, {title:"Editing Ban", pick:-1}], ["New reason"], 0);
            check(s.KLines[0].reason == "New reason" && s.KLines[0].password === undefined);
        },
        "new operators have parsed permission flags": function() {
            var s = editor();
            menu(s, "Operator_Menu", [
                {title:"IRC Operators", pick:s.MSK_INS},
                {title:"Editing IRC Operator", pick:-1}, {title:"IRC Operators", pick:-1}
            ]);
            check(typeof s.OLines[0].flags == "number" && (s.OLines[0].flags & s.OLINE_CAN_REHASH));
        },
        "class zero and classes in use cannot be deleted": function() {
            var s = editor(); s.ILines.push(new s.ILine("*@*", null, "*@*", 0, 1));
            menu(s, "Class_Menu", [
                {title:"IRC Classes", pick:s.MSK_DEL}, {title:"IRC Classes", pick:s.MSK_DEL | 1},
                {title:"IRC Classes", pick:s.MSK_DEL | 2}, {title:"IRC Classes", pick:-1}
            ]);
            check(s.YLines[0] && s.YLines[1] && !s.YLines[30]);
        },
        "numbers reject partial input and out-of-range values; Escape preserves values": function() {
            var s = editor(); s.inputs = ["123bad", "0", "65536", "6697"];
            check(s.Edit_Integer("Port", 6667, 1, 65535) == 6697 && !s.inputs.length);
            s.inputs = [undefined]; check(s.Edit_Integer("Port", 6667, 1, 65535) == 6667);
            s.steps = [{title:"Enabled?", pick:-1}]; check(s.Yes_No("Enabled?", true) === true);
            s.steps = [{title:"Enabled?", pick:-1}]; check(s.Yes_No("Enabled?", false) === false);
        },
        "ports compare loaded numeric strings correctly and retain explicit TLS": function() {
            var s = editor(); s.Default_Port = "6667"; s.PLines = ["6697"];
            menu(s, "Port_Menu", [
                {title:"TCP/IP Ports", pick:s.MSK_INS},
                {title:"TCP/IP Ports", pick:s.MSK_INS}, {title:"Use TLS on this port?", pick:0},
                {title:"TCP/IP Ports", pick:-1}
            ], ["6697", "7000"]);
            check(s.PLines.length == 2 && s.TLS_Ports[7000]);
            check(s.messages.some(function(m) { return /already in the list/.test(m); }));
        },
        "server edits match rules by name and rename the associated hub": function() {
            var s = editor();
            s.CLines = [new s.CLine("a.test", "out-a", "a.test", 6667, 30), new s.CLine("b.test", "out-b", "b.test", 6667, 30)];
            s.NLines = [new s.NLine("b.test", "in-b", "b.test", 0, 30), new s.NLine("a.test", "in-a", "a.test", 0, 30)];
            s.HLines = [new s.HLine("*", "a.test")];
            menu(s, "Edit_Server", [
                {title:"Editing Server", pick:3}, {title:"Editing Server", pick:4},
                {title:"Editing Server", pick:8}, {title:"Require TLS for this server?", pick:0},
                {title:"Editing Server", pick:-1}
            ], ["changed-a", "renamed.test", "private-root.pem"], "A.TEST");
            check(s.Find_NLine("b.test").password == "in-b");
            check(s.Find_NLine("renamed.test").password == "changed-a");
            check(s.Find_CLine("renamed.test").tls && s.Find_NLine("renamed.test").tls);
            check(s.HLine_Exists("renamed.test") && !s.HLine_Exists("a.test"));
            check(s.Write_Config_File(s.Config_Filename)); s.Read_Config_File();
            check(s.Find_CLine("renamed.test").tls && s.Find_NLine("renamed.test").password == "changed-a");
        },
        "inbound-only rules appear in the server list and deletion removes hub authority": function() {
            var s = editor(); s.CLines = [];
            s.NLines = [new s.NLine("incoming.test", "test", "incoming.test", 0, 30)];
            s.HLines = [new s.HLine("*", "incoming.test")];
            menu(s, "Server_Menu", [
                {title:"Servers", pick:"incoming.test"}, {title:"Editing Server", pick:-1},
                {title:"Servers", pick:s.MSK_DEL}, {title:"Servers", pick:-1}
            ]);
            check(!s.CLines.length && !s.NLines.length && !s.HLines.length);
        },
        "current services, FileServ, clock and WebIRC settings survive editing and reload": function() {
            var s = editor();
            edit_settings(s, "BBS_Services_Menu", "Local BBS Services", [
                [0,"NickServ?",0], [1,"MemoServ?",0], [2,"FileServ?",0], [3,"Require TLS?",0]
            ]);
            edit_settings(s, "Embedded_Services_Menu", "Embedded Services Authority", [
                [0,"Enable hosting?",0], [1,"services.example.test"], [2,"/private/services.sqlite"],
                [3,"Require TLS?",0], [4,"12"], [5,"30"], [6,"90"]
            ]);
            edit_settings(s, "FileServ_Access_Menu", "FileServ Access and Channels", [
                [0,"Main,BBS Software"], [1,"Staff*"], [2,"*"], [3,"*_private"],
                [4,"Main"], [5,"main_public"], [6,"2"], [7,"#files,#bbs"]
            ]);
            menu(s, "FileServ_DCC_Menu", [
                {title:"FileServ DCC", pick:0}, {title:"FileServ DCC", pick:1},
                {title:"FileServ DCC", pick:6}, {title:"TLS policy", pick:"required"},
                {title:"FileServ DCC", pick:7}, {title:"FileServ DCC", pick:8}, {title:"FileServ DCC", pick:-1}
            ], ["files.example.test", "bind.example.test", "40000", "40015", "/private/jsexec"]);
            edit_settings(s, "FileServ_Limits_Menu", "FileServ Limits", [[0,"8"], [5,"120"]]);
            edit_settings(s, "Network_Menu", "Network Policies", [[0,"30"]]);
            menu(s, "WebIRC_Menu", [
                {title:"WebIRC Gateways", pick:s.MSK_INS}, {title:"WebIRC Gateway", pick:-1},
                {title:"WebIRC Gateways", pick:-1}
            ], ["gateway.example.test", "test-shared-secret"]);
            var services = JSON.stringify(s.EmbeddedServices_Config);
            check(s.Write_Config_File(s.Config_Filename)); s.Read_Config_File();
            check(JSON.stringify(s.EmbeddedServices_Config) == services);
            check(s.Clock_MaxSkew == 30 && s.WLines[0].hostname == "gateway.example.test");
        },
        "DCC port range cancellation is atomic and zero resets both endpoints": function() {
            var s = editor(), c = s.EmbeddedServices_Config.files;
            c.port_first = 40000; c.port_last = 40015;
            s.inputs = ["41000", undefined]; s.FileServ_Ports();
            check(c.port_first == 40000 && c.port_last == 40015);
            s.inputs = [undefined]; s.FileServ_Ports();
            check(c.port_first == 40000 && c.port_last == 40015);
            s.inputs = ["0"]; s.FileServ_Ports(); check(!c.port_first && !c.port_last);
        },
        "Save and Quit exits after one successful save": function() {
            var s = editor({steps:[
                {title:"Main Menu", pick:"Save and Quit"},
                {title:/^Overwrite /, pick:0}
            ]});
            check(s.Main_Menu_Exit && s.messages.indexOf("Configuration saved.") >= 0);
        },
        "cancelled and failed saves leave the editor open": function() {
            var s = editor({steps:[
                {title:"Main Menu", pick:"Save and Quit"}, {title:/^Overwrite /, pick:-1},
                {title:"Main Menu", pick:"Save and Quit", before:function(s) {
                    s.Write_Config_File = function() { return false; };
                }},
                {title:/^Overwrite /, pick:0}, {title:"Main Menu", pick:"Quit Immediately (No Save)"}
            ]});
            check(s.messages.some(function(message) { return /^Failed to write /.test(message); }));
        },
        "a failed rehash request does not report success": function() {
            var s = editor(); s.file_touch = function() { return false; };
            check(!s.Attempt_Live_Rehash());
            check(s.messages.length == 1 && /^Touch failed/.test(s.messages[0]));
        },
        "Save As changes the destination used by the following save": function() {
            var destination = editor_path("-saved.ini");
            var s = editor({steps:[
                {title:"Main Menu", pick:"Save As"}, {title:"Main Menu", pick:"Save and Quit"},
                {title:/^Overwrite /, pick:0, before:function(s) { check(s.Config_Filename == destination); }}
            ], inputs:[destination]});
            check(file_exists(destination) && s.Config_Filename == destination);
        },
        "legacy CONF input is converted to INI without expanding system macros": function() {
            var s = editor({suffix:".CONF", config:"M:SYSTEM_QWKID.test:127.0.0.1:Legacy:6667\n"});
            check(/\.ini$/.test(s.Config_Filename));
            editor_files.push(s.Config_Filename);
            check(s.ServerName == "SYSTEM_QWKID.test");
            check(s.Write_Config_File(s.Config_Filename)); s.Read_Config_File();
            check(s.ServerName == "SYSTEM_QWKID.test");
        }
    });
} finally {
    editor_files.forEach(function(path) { file_remove(path); });
}
