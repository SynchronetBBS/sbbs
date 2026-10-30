/* INI spelling must not change daemon/editor policy or configuration values. */
"use strict";
load(js.exec_dir + "lib.sjs");

function check(value, message) { if (!value) throw new Error(message || "Assertion failed"); }
var case_paths = [];
var case_config = [
    "[Info]", "Servername=Case.Server.test", "Networkname=CaseNet", "Description=My Mixed Case BBS",
    "Admin1=The Sysop", "Admin2=", "Admin3=Contact@Example.test",
    "[Port:19965]", "Default=true", "TLS=true", "[Port:19966]", "TLS=false",
    "[Class:30]", "Comment=Mixed Case Class", "PingFrequency=123", "ConnectFrequency=0", "Maximum=17", "SendQ=4000000",
    "[Allow:MiXeD]", "Mask=User@*.Example.test", "Password=AllowCaseSecret", "Class=30", "Port=19965",
    "[Operator:MiXeD]", "Nick=CaseOp", "Mask=Oper@*.Example.test", "Password=OperCaseSecret", "Flags=OwkKR", "Class=30",
    "[Services:MiXeD]", "Servername=Services.Example.test",
    "[Ban:MiXeD]", "Mask=Bad@*.Example.test", "Reason=Mixed Case Ban",
    "[Restrict:MiXeD]", "Mask=ReservedNick", "Reason=Mixed Case Restriction",
    "[RBL:MiXeD]", "Hostname=DNS.Example.test", "GoodResponses=127.0.0.2,127.0.0.3", "BadResponses=127.0.0.4",
    "[WebIRC:MiXeD]", "Hostname=Gateway.Example.test", "Password=WebCaseSecret",
    "[Server:MiXeD]", "Servername=Peer.Example.test", "Hostname=Host.Example.test", "Port=19967", "TLS=true",
    "InboundPassword=InCaseSecret", "OutboundPassword=OutCaseSecret", "Class=30", "Hub=true", "Flags=q",
    "[Hub:MiXeD]", "Servername=OtherHub.Example.test",
    "[Capab]", "Enforce=true", "[Clock]", "MaxSkew=23", "[TLS]", "CAFile=/tmp/TrustedRoots.PEM",
    "[BBSServices]", "NickServ=true", "MemoServ=false", "FileServ=true", "RequireTLS=true",
    "[FileServ]", "IncludeLibs=Main,BBS Software", "ExcludeLibs=Staff*", "IncludeDirs=*", "ExcludeDirs=*_Private",
    "PublicLibs=Main", "PublicDirs=Main_public", "Guest=17", "Channels=#Files,#BBS",
    "Address=Files.Example.test", "Bind=Bind.Example.test", "Address4=192.0.2.1", "Bind4=0.0.0.0",
    "Address6=2001:db8::AbCd", "Bind6=::", "PortFirst=40000", "PortLast=40015", "TLS=required",
    "MaxDownloads=8", "MaxChats=9", "MaxQueue=80", "UserQueue=10", "PageSize=30", "Timeout=120", "JSexec=/tmp/PrivateJSexec",
    "[EmbeddedServices]", "Enabled=false", "Host=Services.Example.test", "Database=/tmp/ServicesCase.sqlite",
    "RequireTLS=true", "MaxNicks=12", "NickExpireDays=30", "ChannelExpireDays=90"
].join("\n");

function case_spelling(text, section_style, key_style) {
    return text.split("\n").map(function(line) {
        if (line.charAt(0) == "[") {
            var end = line.indexOf(":"), close = line.indexOf("]");
            if (end < 0) end = close;
            return "[" + section_style(line.substring(1, end)) + line.substring(end);
        }
        var equal = line.indexOf("=");
        return equal < 0 ? line : key_style(line.substring(0, equal)) + line.substring(equal);
    }).join("\n");
}
function mixed_case(value) {
    return value.split("").map(function(letter, i) { return i % 2 ? letter.toUpperCase() : letter.toLowerCase(); }).join("");
}
var case_styles = [function(s) { return s.toLowerCase(); }, function(s) { return s.toUpperCase(); }, mixed_case];

function case_state(editing) {
    var s = {IRCDCFG_Editor:editing, Config_Filename:"", Time_Config_Read:0,
        CLines:[], NLines:[], HLines:[], ILines:[], KLines:[], OLines:[], PLines:[],
        QLines:[], ULines:[], YLines:[], ZLines:[], WLines:[], RBL:[], Servers:{},
        ServerName:"", ServerDesc:"", Default_Port:6667, Admin1:"", Admin2:"", Admin3:"",
        Die_Password:"", Restart_Password:"", CapabEnforce:false,
        log:function() {}, Scan_For_Banned_Clients:function() {}, Reset_Autoconnect:function() {
            throw new Error("Configuration unexpectedly scheduled an outbound connection");
        }};
    load(s, "ircd/config.js");
    return s;
}
function case_load(s, text) {
    var path = IRCD_TEST_TEMP_DIR + "ircd-config-case-" + random(0x7fffffff) + ".ini";
    case_paths.push(path);
    var f = new File(path); check(f.open("w")); f.write(text); f.close();
    s.Config_Filename = path;
    s.Read_Config_File();
}
function case_snapshot(s) {
    var result = {};
    ["ServerName", "NetworkName", "ServerDesc", "Admin1", "Admin2", "Admin3", "Default_Port", "TLS_Config", "TLS_Ports",
     "CLines", "NLines", "HLines", "ILines", "KLines", "OLines", "PLines", "QLines", "ULines", "YLines", "WLines", "RBL",
     "Clock_MaxSkew", "CapabEnforce", "EmbeddedServices_Config"].forEach(function(key) { result[key] = s[key]; });
    return JSON.stringify(result);
}

try {
    run_tests({
        "every section and key accepts lower, upper and mixed case in daemon and editor modes": function() {
            [false, true].forEach(function(editing) {
                var s = case_state(editing); case_load(s, case_config);
                check(s.NetworkName == "CaseNet" && s.TLS_Ports[19965] && s.Clock_MaxSkew == 23);
                check(s.EmbeddedServices_Config.nickserv && s.EmbeddedServices_Config.files.port_last == 40015);
                var expected = case_snapshot(s);
                case_styles.forEach(function(sections) { case_styles.forEach(function(keys) {
                    case_load(s, case_spelling(case_config, sections, keys));
                    check(case_snapshot(s) == expected, "Spelling changed policy in " + (editing ? "editor" : "daemon") + " mode");
                }); });
            });
        },
        "case folding preserves passwords, paths, text, masks and section arguments": function() {
            var s = case_state(true), argument, allow = s.ini_Allow;
            s.ini_Allow = function(arg, ini) { argument = arg; allow(arg, ini); };
            case_load(s, case_spelling(case_config, mixed_case, mixed_case));
            check(argument == "MiXeD");
            check(s.ILines[0].password == "AllowCaseSecret" && s.OLines[0].password == "OperCaseSecret");
            check(s.CLines[0].password == "OutCaseSecret" && s.NLines[0].password == "InCaseSecret");
            check(s.WLines[0].password == "WebCaseSecret");
            check(s.TLS_Config.ca_file == "/tmp/TrustedRoots.PEM");
            check(s.EmbeddedServices_Config.database == "/tmp/ServicesCase.sqlite");
            check(s.EmbeddedServices_Config.files.jsexec == "/tmp/PrivateJSexec");
            check(s.EmbeddedServices_Config.files.include_libs == "Main,BBS Software");
            check(s.ServerName == "Case.Server.test" && s.ServerDesc == "My Mixed Case BBS" && s.Admin2 === "");
        },
        "disabled sections remain disabled for every spelling": function() {
            var text = "[Info]\nEnabled=false\nServername=ignored.test\n"
                + "[BBSServices]\nDisabled=true\nNickServ=true\nRequireTLS=true\n"
                + "[EmbeddedServices]\nDisabled=true\nEnabled=true\nHost=ignored.test\n";
            case_styles.forEach(function(style) {
                var s = case_state(true); case_load(s, case_spelling(text, style, style));
                check(s.ServerName != "ignored.test");
                check(!s.EmbeddedServices_Config.nickserv && !s.EmbeddedServices_Config.bbs_require_tls);
                check(!s.EmbeddedServices_Config.enabled && s.EmbeddedServices_Config.host == "services.synchro.net");
            });
        },
        "disabled authority hosting retains a leaf's authority host and policies for every spelling": function() {
            var text = "[EmbeddedServices]\nEnabled=false\nHost=services.test\nRequireTLS=true\nMaxNicks=7\nNickExpireDays=90\nChannelExpireDays=15\n";
            case_styles.forEach(function(style) {
                var s = case_state(false); case_load(s, case_spelling(text, style, style));
                var c = s.EmbeddedServices_Config;
                check(!c.enabled && c.host == "services.test" && c.require_tls);
                check(c.max_nicks == 7 && c.nick_expire_days == 90 && c.channel_expire_days == 15);
            });
        },
        "empty and disabled services sections suppress default trusted hosts regardless of case": function() {
            ["", "Disabled=true\nServername=ignored.test", "Enabled=false\nServername=ignored.test"].forEach(function(body) {
                case_styles.forEach(function(style) {
                    var s = case_state(true); case_load(s, case_spelling("[Services:MiXeD]\n" + body, style, style));
                    check(!s.ULines.length);
                    check(s.Write_Config_File(s.Config_Filename)); s.Read_Config_File();
                    check(!s.ULines.length);
                });
            });
        },
        "mixed-case configuration round-trips through the shared writer": function() {
            var s = case_state(true); case_load(s, case_spelling(case_config, mixed_case, mixed_case));
            var expected = case_snapshot(s);
            check(s.Write_Config_File(s.Config_Filename)); s.Read_Config_File();
            check(case_snapshot(s) == expected);
        },
        "direct section handlers accept any key spelling without mutating caller objects": function() {
            var s = case_state(true), input = {nIcKsErV:true, rEqUiReTlS:true}, original = JSON.stringify(input);
            s.Clear_Config_Globals(); s.ini_BBSServices("", input);
            check(s.EmbeddedServices_Config.nickserv && s.EmbeddedServices_Config.bbs_require_tls);
            check(JSON.stringify(input) == original);
            s.ini_FileServ("", {pUbLiClIbS:"Main", jSeXeC:"/tmp/CaseJSexec"});
            check(s.EmbeddedServices_Config.files.public_libs == "Main" && s.EmbeddedServices_Config.files.jsexec == "/tmp/CaseJSexec");
        },
        "unknown section names cannot dispatch inherited JavaScript properties": function() {
            var s = case_state(true); case_load(s, "[CONSTRUCTOR]\nEnabled=true\n[ToString]\nEnabled=true\n");
            check(s.NetworkName == "Synchronet" && !s.EmbeddedServices_Config.enabled);
        }
    });
} finally {
    case_paths.forEach(function(path) { file_remove(path); });
}
