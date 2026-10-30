/*

 ircdcfg.js

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details:
 https://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

 Copyright 2023 Randy Sommerfeld <cyan@synchro.net>

*/

"use strict";

load("sbbsdefs.js");
load("uifcdefs.js");
load("ircd/config.js");

function Yes_No(str, current) {
    var ctx = new uifc.list.CTX;
    ctx.cur = current === false ? 1 : 0;
    var result = uifc.list(WIN_MID | WIN_ACT, str, ["Yes", "No"], ctx);
    if (result == 0) return true;
    if (result == 1) return false;
    return current;
}

function Find_Hub(str) {
    var i;

    for (i in HLines) {
        if (String(str).toLowerCase() == HLines[i].servername.toLowerCase())
            return true;
    }

    return false;
}

function Find_ULine(str) {
    var i;

    for (i in ULines) {
        if (ULines[i].toLowerCase() == str.toLowerCase())
            return true;
    }

    return false;
}

function Var_Exists_in_Array(arr, str) {
    var i;

    for (i in arr) {
        if (String(arr[i]).toLowerCase() == String(str).toLowerCase())
            return true;
    }

    return false;
}

function Edit_Existing(desc, str, maxlen, required, password) {
    while (!js.terminated) {
        var newstr = uifc.input(WIN_MID, desc, str == null ? "" : String(str),
                              maxlen || 255, K_EDIT | (password ? K_PASSWORD : 0));
        if (newstr === undefined) return str;
        if (/[\r\n]/.test(newstr) || (required && !newstr.length)) {
            uifc.msg(required ? "Enter a nonempty, single-line value." : "Enter a single-line value.");
            continue;
        }
        return newstr;
    }
    return str;
}

function Edit_Integer(desc, current, min, max, cancel_undefined) {
    while (!js.terminated) {
        var str = uifc.input(WIN_MID, desc, current === undefined ? "" : String(current),
                             String(max).length, K_EDIT);
        if (str === undefined) return cancel_undefined ? undefined : current;
        var value = Number(str);
        if (!/^\d+$/.test(str) || !isFinite(value) || value < min || value > max) {
            uifc.msg("Enter a whole number from " + min + " to " + max + ".");
            continue;
        }
        return value;
    }
    return current;
}

function Edit_Class(current) {
    var keys = Object.keys(YLines).sort(function(a, b) { return Number(a) - Number(b); });
    var ctx = new uifc.list.CTX;
    ctx.cur = Math.max(0, keys.indexOf(String(current)));
    var selected = uifc.list(WIN_MID | WIN_ACT, "IRC Class", keys.map(function(key) {
        return key + (YLines[key].comment ? " (" + YLines[key].comment + ")" : "");
    }), ctx);
    return selected < 0 ? current : Number(keys[selected]);
}

/* Shared editors keep cancellation, numeric bounds and INI values consistent. */
function Settings_Menu(title, target, fields) {
    var ctx = new uifc.list.CTX;
    while (!js.terminated) {
        var selected = uifc.list(WIN_ORG | WIN_MID | WIN_ACT, title, fields.map(function(field) {
            var value = field.display ? field.display() : target[field.key];
            if (typeof value == "boolean") value = value ? "Yes" : "No";
            return format("%22s: %s", field.label, value);
        }), ctx);
        if (selected < 0) return;
        var field = fields[selected], value = target[field.key];
        if (field.help) Help_Window(field.help);
        if (field.edit) { field.edit(); continue; }
        if (field.choices) {
            var choice_ctx = new uifc.list.CTX;
            choice_ctx.cur = Math.max(0, field.choices.indexOf(value));
            var choice = uifc.list(WIN_MID | WIN_ACT, field.label, field.choices, choice_ctx);
            if (choice >= 0) target[field.key] = field.choices[choice];
        } else if (typeof value == "boolean") {
            target[field.key] = Yes_No(field.label + "?", value);
        } else if (typeof value == "number") {
            target[field.key] = Edit_Integer(field.label, value, field.min, field.max);
        } else {
            target[field.key] = Edit_Existing(field.label, value, field.maxlen, field.required, field.password);
        }
    }
}

function Clear_All() {
    uifc.list(WIN_ORG | WIN_ACT | WIN_IMM, " ", " ", new uifc.list.CTX);
}

function Help_Window(text) {
    var oldbg = uifc.background_color;
    var oldlb = uifc.lightbar_color;
    uifc.background_color = 12;
    uifc.lightbar_color = 79;
    uifc.list(
        WIN_ACT | WIN_BOT | WIN_IMM,
        "Help",
        text,
        new uifc.list.CTX
    );
    uifc.background_color = oldbg;
    uifc.lightbar_color = oldlb;
}

function Info_Menu() {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, str;

    while (!exit) {
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT,
            "Server Information",
            [
                format("%15s: %-40s", "ServerName", ServerName),
                format("%15s: %-40s", "Description", ServerDesc),
                format("%15s: %-40s", "/ADMIN Line 1", Admin1),
                format("%15s: %-40s", "/ADMIN Line 2", Admin2),
                format("%15s: %-40s", "/ADMIN Line 3", Admin3),
                format("%15s: %-40s", "Networkname", NetworkName),
            ],
            ctx
        );
        if (result == -1)
            exit = true;
        switch(result) {
            case 0:
                Help_Window([
                    "The server name as it will appear on the IRC network.",
                    "Choose a unique name agreed with the network's administrators.",
                    "SYSTEM_HOST_NAME and SYSTEM_QWKID macros remain portable when saved."
                ]);
                ServerName = Edit_Existing("ServerName", ServerName, 255, true);
                break;
            case 1:
                Help_Window([
                    "A short description of your IRC server.",
                    "Cosmetic only, appears in /WHOIS, /LINKS, et al.",
                    "Example: Visit my BBS at https://example.com/"
                ]);
                ServerDesc = Edit_Existing("Description", ServerDesc);
                break;
            case 2:
                Help_Window([
                    "The first line of the output of the /ADMIN command.",
                    "Cosmetic only.",
                    "Example: Your sysop today is sysop@mybbs.synchro.net"
                ]);
                Admin1 = Edit_Existing("/ADMIN Line 1", Admin1);
                break;
            case 3:
                Help_Window([
                    "The second line of the output of the /ADMIN command.",
                    "Cosmetic only.",
                    "Example: This BBS is experimental, let us know what you think!"
                ]);
                Admin2 = Edit_Existing("/ADMIN Line 2", Admin2);
                break;
            case 4:
                Help_Window([
                    "The third line of the output of the /ADMIN command.",
                    "Cosmetic only.",
                    "Example: Proudly running Gentoo Linux."
                ]);
                Admin3 = Edit_Existing("/ADMIN Line 3", Admin3);
                break;
            case 5:
                Help_Window([
                    "The IRC network name in the welcome and ISUPPORT replies.",
                    "Defaults to Synchronet. Use 1-64 printable ASCII characters",
                    "without spaces or backslashes; use the same name on each server."
                ]);
                str = uifc.input(WIN_MID, "Networkname", NetworkName, 64, K_EDIT);
                if (str !== undefined) {
                    if (!str) NetworkName = "Synchronet";
                    else if (/^[\x21-\x7e]{1,64}$/.test(str) && str.indexOf("\\") < 0) NetworkName = str;
                    else uifc.msg("Use 1-64 printable ASCII characters without spaces or backslashes.");
                }
                break;
        }
    }
}

function Port_Menu() {
    var ctx = new uifc.list.CTX;
    while (!js.terminated) {
        var ports = [Number(Default_Port)].concat(PLines.map(Number));
        var result = uifc.list(WIN_ORG | WIN_MID | WIN_ACT | WIN_INS | WIN_DEL,
            "TCP/IP Ports", ports.map(function(port, index) {
                return port + (index == 0 ? " (default)" : "")
                    + ((TLS_Ports[port] || port == 6697 || port == 994) ? " (TLS)" : "");
            }), ctx);
        if (result < 0) return;
        var index = result & MSK_OFF;
        if (result & MSK_DEL) {
            if (index == 0) { uifc.msg("Can't delete the default port."); continue; }
            delete TLS_Ports[ports[index]];
            ports.splice(index, 1);
        } else {
            var inserting = !!(result & MSK_INS);
            var old_port = inserting ? undefined : ports[index];
            var port = Edit_Integer(inserting ? "New Port" : "TCP/IP Port", old_port, 1, 65535);
            if (port === undefined) continue;
            if (port != old_port && ports.indexOf(port) >= 0) {
                uifc.msg("Port " + port + " already in the list."); continue;
            }
            var tls = port == 6697 || port == 994;
            if (!tls) tls = Yes_No("Use TLS on this port?",
                !!TLS_Ports[old_port] || old_port == 6697 || old_port == 994);
            if (old_port !== undefined && old_port != port) delete TLS_Ports[old_port];
            if (tls) TLS_Ports[port] = true;
            else delete TLS_Ports[port];
            if (inserting) ports.push(port);
            else ports[index] = port;
        }
        Default_Port = ports.shift();
        PLines = ports;
    }
}

function TLS_Menu() {
    Help_Window([
        "PEM bundle of trusted CA certificates for outbound TLS server links.",
        "Relative paths use the Synchronet control directory.",
        "Listening ports use Synchronet's existing ssl.cert certificate and key."
    ]);
    var path = uifc.input(WIN_MID, "TLS CA bundle (PEM)", TLS_Config.ca_file, 255, K_EDIT);
    if (path !== undefined) TLS_Config.ca_file = path;
}

function Network_Menu() {
    var settings = {skew:Clock_MaxSkew, enforce:CapabEnforce};
    Settings_Menu("Network Policies", settings, [
        {key:"skew", label:"Maximum clock skew", min:1, max:2147483647, help:[
            "Maximum permitted server clock difference, in seconds. Default: 30.",
            "Clock-capable links are checked at connection and periodically.",
            "Use the same policy on all hubs."
        ]},
        {key:"enforce", label:"Enforce capabilities", help:[
            "Preserve the legacy [Capab] Enforce setting.",
            "The current daemon does not use this flag to reject peers."
        ]}
    ]);
    Clock_MaxSkew = settings.skew;
    CapabEnforce = settings.enforce;
}

function BBS_Services_Menu() {
    Help_Window([
        "Local BBS services are optional and disabled by default.",
        "NickServ authenticates BBS accounts; MemoServ accesses BBS telegrams.",
        "FileServ exports BBS file catalogs according to its own access policy.",
        "These settings do not enable the central nickname/channel authority."
    ]);
    Settings_Menu("Local BBS Services", EmbeddedServices_Config, [
        {key:"nickserv", label:"NickServ"},
        {key:"memoserv", label:"MemoServ"},
        {key:"fileserv", label:"FileServ"},
        {key:"bbs_require_tls", label:"Require TLS", help:[
            "Require TLS on every hop from the client to the destination services",
            "for authentication, account changes and private account/memo commands.",
            "This also requires a TLS client connection for BBS PASS authentication.",
            "FileServ DCC encryption is configured separately."
        ]}
    ]);
}

function Embedded_Services_Menu() {
    Help_Window([
        "Central NickServ and ChanServ are disabled by default.",
        "Set the same authority Host on every BBS using embedded services.",
        "An enabled instance hosts it only when Host resolves to a listening address.",
        "Trust that Host in IRC Services/Super Servers and coordinate any cutover",
        "from external services; their registrations are not imported."
    ]);
    Settings_Menu("Embedded Services Authority", EmbeddedServices_Config, [
        {key:"enabled", label:"Enable hosting"},
        {key:"host", label:"Authority host", required:true},
        {key:"database", label:"SQLite database", maxlen:1024, help:[
            "Registration database on the authority host.",
            "Blank uses ircd-services.sqlite in the Synchronet data directory."
        ]},
        {key:"require_tls", label:"Require TLS", help:[
            "Require TLS on every hop to the authority for authentication,",
            "registration changes and private account/channel commands.",
            "HELP, public INFO and LOGOUT remain available without TLS."
        ]},
        {key:"max_nicks", label:"Nicknames per account", min:1, max:2147483647},
        {key:"nick_expire_days", label:"Nickname expiry days", min:0, max:2147483647, help:[
            "Drop nickname registrations after this many unused days; 0 disables expiry.",
            "Use counts while the authenticated owner is using that nickname."
        ]},
        {key:"channel_expire_days", label:"Channel expiry days", min:0, max:2147483647, help:[
            "Drop channel registrations after this many unused days; 0 disables expiry.",
            "Use counts while an authenticated founder or access-list member is present."
        ]}
    ]);
}

function FileServ_Access_Menu() {
    Help_Window([
        "Comma-separated, case-insensitive wildcard lists; exclusions win.",
        "Library masks match short names (Main), directory masks match full codes.",
        "Anonymous requests require public selections and an eligible guest account.",
        "Native BBS access requirements still apply; public does not bypass ARS."
    ]);
    Settings_Menu("FileServ Access and Channels", EmbeddedServices_Config.files, [
        {key:"include_libs", label:"Include libraries", maxlen:1024},
        {key:"exclude_libs", label:"Exclude libraries", maxlen:1024},
        {key:"include_dirs", label:"Include directories", maxlen:1024},
        {key:"exclude_dirs", label:"Exclude directories", maxlen:1024},
        {key:"public_libs", label:"Public libraries", maxlen:1024},
        {key:"public_dirs", label:"Public directories", maxlen:1024},
        {key:"guest", label:"Guest account number", min:0, max:1000000, help:[
            "0 selects the first active BBS account with the G restriction.",
            "A positive number selects that account, which must also be G-restricted."
        ]},
        {key:"channels", label:"Channels", maxlen:1024, help:[
            "Comma-separated channels FileServ should join, for example #files,#bbs.",
            "Blank enables private commands only. Bots reply using NOTICE."
        ]}
    ]);
}

function FileServ_Ports() {
    var c = EmbeddedServices_Config.files;
    var first = Edit_Integer("First DCC port (0 for automatic)", c.port_first, 0, 65535, true);
    if (first === undefined) return;
    if (!first) { c.port_first = c.port_last = 0; return; }
    var last = Edit_Integer("Last DCC port", Math.max(first, c.port_last), first, Math.min(65535, first + 1024), true);
    if (last === undefined) return;
    c.port_first = first;
    c.port_last = last;
}

function FileServ_DCC_Menu() {
    var c = EmbeddedServices_Config.files;
    Help_Window([
        "Address is advertised to clients; Bind selects local listening addresses.",
        "Both accept a hostname resolving to IPv4 and IPv6 or a numeric address.",
        "Address4/6 and Bind4/6 override their corresponding address family.",
        "Forward the selected DCC ports through any firewall or NAT."
    ]);
    Settings_Menu("FileServ DCC", c, [
        {key:"address", label:"Advertised address"},
        {key:"bind", label:"Bind address"},
        {key:"address4", label:"Advertised IPv4"},
        {key:"bind4", label:"Bind IPv4"},
        {key:"address6", label:"Advertised IPv6"},
        {key:"bind6", label:"Bind IPv6"},
        {key:"tls", label:"TLS policy", choices:["off", "optional", "required"], help:[
            "off: ordinary DCC only. optional: TLS by default, PLAIN may be requested.",
            "required: encrypted DCC only. Clients must support SCHAT/SSEND.",
            "TLS uses the BBS ssl.cert certificate and key."
        ]},
        {label:"DCC ports", display:function() {
            return c.port_first ? c.port_first + "-" + c.port_last : "Automatic";
        }, edit:FileServ_Ports},
        {key:"jsexec", label:"JSexec executable", maxlen:1024, help:[
            "Optional full path to jsexec for DCC workers.",
            "Blank uses the executable in the Synchronet exec directory."
        ]}
    ]);
}

function FileServ_Limits_Menu() {
    Settings_Menu("FileServ Limits", EmbeddedServices_Config.files, [
        {key:"max_downloads", label:"Concurrent downloads", min:1, max:256},
        {key:"max_chats", label:"Concurrent chats", min:1, max:256},
        {key:"max_queue", label:"Total queue entries", min:1, max:256},
        {key:"user_queue", label:"Queue entries per user", min:1, max:256},
        {key:"page_size", label:"Results per page", min:1, max:256},
        {key:"timeout", label:"DCC timeout (seconds)", min:1, max:3600}
    ]);
}

function FileServ_Menu() {
    var ctx = new uifc.list.CTX;
    var menus = [FileServ_Access_Menu, FileServ_DCC_Menu, FileServ_Limits_Menu];
    while (!js.terminated) {
        var result = uifc.list(WIN_ORG | WIN_MID | WIN_ACT, "FileServ Settings",
            ["Access and Channels", "DCC Addresses, Ports and TLS", "Transfer Limits"], ctx);
        if (result < 0) return;
        menus[result]();
    }
}

function WebIRC_Menu() {
    var ctx = new uifc.list.CTX;
    Help_Window([
        "Trust only gateways that may supply a client's real hostname/address.",
        "Hostname is the permitted gateway mask; Password is its shared secret."
    ]);
    while (!js.terminated) {
        var result = uifc.list(WIN_ORG | WIN_MID | WIN_ACT | WIN_INS | WIN_DEL,
            "WebIRC Gateways", WLines.map(function(line) { return line.hostname; }), ctx);
        if (result < 0) return;
        var index = result & MSK_OFF;
        if (result & MSK_DEL) { WLines.splice(index, 1); continue; }
        if (result & MSK_INS) {
            var host = uifc.input(WIN_MID, "New WebIRC gateway hostname", "", 255);
            if (!host) continue;
            var password = uifc.input(WIN_MID, "WebIRC password", "", 255, K_PASSWORD);
            if (!password) continue;
            WLines.push(new WLine(host, password));
            index = WLines.length - 1;
        }
        Settings_Menu("WebIRC Gateway", WLines[index], [
            {key:"hostname", label:"Hostname", required:true},
            {key:"password", label:"Password", required:true, password:true,
                display:function() { return "********"; }}
        ]);
    }
}

function Edit_IRC_Class(ircclass) {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, str, i;

    while (!exit) {
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT,
            "Editing IRC Class",
            [
                format("%20s: %-40s", "Comment", ircclass.comment ? ircclass.comment : ""),
                format("%20s: %-40lu", "Ping Frequency", ircclass.pingfreq),
                format("%20s: %-40lu", "Connect Frequency", ircclass.connfreq),
                format("%20s: %-40lu", "Maximum Links", ircclass.maxlinks),
                format("%20s: %-40lu", "Maximum Send Queue", ircclass.sendq),
            ],
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        switch(result) {
            case 0:
                Help_Window([
                    "This comment is purely cosmetic.",
                    "It's saved in the config but not used in the IRCd itself."
                ]);
                ircclass.comment = Edit_Existing("Comment", ircclass.comment);
                break;
            case 1:
                Help_Window([
                    "How often, in seconds, the IRCd sends a PING to idle clients/servers.",
                    "Note that this defines both the idle time and the response time, so",
                    "if you put '60' here, the real timeout will be 120 seconds."
                ]);
                ircclass.pingfreq = Edit_Integer("Ping Frequency", ircclass.pingfreq, 1, 999);
                break;
            case 2:
                Help_Window([
                    "How often, in seconds, the IRCd attempts to connect to a disconnected",
                    "server.  This option has no use for clients (use 0 in that case).",
                    "'0' here for a server means it should never attempt to auto-connect."
                ]);
                ircclass.connfreq = Edit_Integer("Connect Frequency", ircclass.connfreq, 0, 9999);
                break;
            case 3:
                Help_Window([
                    "The maximum number of users or servers allowed in this class.",
                    "If a server or user attempts to connect when this is exceeded,",
                    "the connection is denied."
                ]);
                ircclass.maxlinks = Edit_Integer("Maximum Links", ircclass.maxlinks, 0, 99999);
                break;
            case 4:
                Help_Window([
                    "The maximum number of bytes held in the internal IRCd send buffer",
                    "before the user or server is forcibly disconnected for flooding.",
                ]);
                ircclass.sendq = Edit_Integer("Maximum Send Queue", ircclass.sendq, 2048, 999999999);
                break;
        }
    }
}

function Class_Menu() {
    var ctx = new uifc.list.CTX;
    YLines[0].comment = "Hardcoded fallback, not configurable.";
    while (!js.terminated) {
        var keys = Object.keys(YLines).sort(function(a, b) { return Number(a) - Number(b); });
        var result = uifc.list(WIN_ORG | WIN_MID | WIN_ACT | WIN_INS | WIN_DEL,
            "IRC Classes", keys.map(function(key) {
                return key + (YLines[key].comment ? " (" + YLines[key].comment + ")" : "");
            }), ctx);
        if (result < 0) return;
        if (result & MSK_INS) {
            var id = Edit_Integer("New Class Number", undefined, 1, 65535);
            if (id === undefined) continue;
            if (YLines[id]) { uifc.msg("Class " + id + " already exists."); continue; }
            YLines[id] = new YLine(120, 600, 100, 1000000);
            Edit_IRC_Class(YLines[id]);
            continue;
        }
        var key = keys[result & MSK_OFF];
        if (Number(key) == 0) {
            uifc.msg("Class 0 is a hardcoded fallback and can't be changed or deleted.");
            continue;
        }
        if (result & MSK_DEL) {
            var used = ILines.concat(OLines, CLines, NLines).some(function(line) {
                return Number(line.ircclass) == Number(key);
            });
            if (used) { uifc.msg("This class is still assigned to an allow rule, operator or server."); continue; }
            delete YLines[key];
        } else Edit_IRC_Class(YLines[key]);
    }
}

function Edit_Allow(il) {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var iline = ILines[il];
    var result;

    while (!exit) {
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT,
            "Editing Allow Rule",
            [
                format("%20s: %-40s", "Mask", iline.hostmask),
                format("%20s: %-40lu", "IRC Class", iline.ircclass),
                format("%20s: %-40s", "Password", iline.password ? "********" : ""),
                format("%20s: %-40s", "Local Port", iline.port || "Any"),
            ],
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        switch(result) {
            case 0:
                Help_Window([
                    "Hostmask in the format of user@host, glob-style wildcards supported.",
                    "Most configurations need only a single *@* allow rule."
                ]);
                var mask = Edit_Existing("Mask", iline.hostmask, 255, true);
                if (mask.indexOf("@") < 0) { uifc.msg("Use a user@host mask."); break; }
                iline.hostmask = iline.ipmask = mask;
                break;
            case 1:
                Help_Window([
                    "IRC Class that will be assigned to users connecting that",
                    "match the allow rule."
                ]);
                iline.ircclass = Edit_Class(iline.ircclass);
                break;
            case 2:
                iline.password = Edit_Existing("Password (blank for none)", iline.password || "", 255, false, true);
                break;
            case 3:
                iline.port = Edit_Integer("Local Port (0 for any)", iline.port || 0, 0, 65535);
                break;
        }
    }
}

function Allow_Menu() {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, opts, i;

    while (!exit) {
        Clear_All();
        if (ILines.length < 1) {
            if (!Yes_No("No default allow rule found. Create a default one?"))
                break;
            ILines.push(new ILine("*@*", null, "*@*", 0, YLines[1] ? 1 : 0));
        }
        opts = [];
        for (i in ILines) {
            opts.push(format("%s (Class %lu)", ILines[i].hostmask, ILines[i].ircclass));
        }
        Help_Window([
            "Most configurations will only need a single *@* allow rule here.",
        ]);
        result = uifc.list(
            WIN_MID | WIN_ACT | WIN_INS | WIN_DEL,
            "Allow Rules",
            opts,
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        if (result & MSK_INS) {
            ILines.push(new ILine("*@*",null,"*@*",0,YLines[1] ? 1 : 0));
            Edit_Allow(ILines.length-1);
            continue;
        } else if (result & MSK_DEL) {
            result &= MSK_OFF;
            ILines.splice(result, 1);
        } else {
            Edit_Allow(result);
        }
    }
}

function Edit_Operator(ol) {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, opts, i;
    var oline = OLines[ol];

    while (!exit) {
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT,
            "Editing IRC Operator",
            [
                format("%20s: %-40s", "Nickname", oline.nick),
                format("%20s: %-40s", "Hostmask", oline.hostmask),
                format("%20s: %-40s", "Password", oline.password ? "********" : ""),
                format("%20s: %-40s", "Flags", OLine_Flags_String(oline.flags)),
                format("%20s: %-40lu", "IRC Class", oline.ircclass),
            ],
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        switch(result) {
            case 0:
                Help_Window([
                    "This is only used as the first argument of the /OPER command.",
                    "The user's nickname doesn't need to match this for the user to",
                    "become an IRC operator."
                ]);
                oline.nick = Edit_Existing("Nickname", oline.nick);
                break;
            case 1:
                Help_Window([
                    "The user must match this hostmask (glob-style wildcards supported)",
                    "in order to use the /OPER command."
                ]);
                oline.hostmask = Edit_Existing("Hostmask", oline.hostmask);
                break;
            case 2:
                Help_Window([
                    "The password required to use the /OPER command.  Set this to an",
                    "asterisk if using the 'S' (system password) flag.",
                    "BE AWARE that this password is not encrypted."
                ]);
                oline.password = Edit_Existing("Password", oline.password, 255, false, true);
                break;
            case 3:
                Help_Window([
                    "Flags are permissions that IRC Operators have on the network.",
                    "See docs/ircd.txt for a full list of flags and their function.",
                    "For most sysops, simply the 'o' flag should be sufficient."
                ]);
                oline.flags = parse_oline_flags(Edit_Existing("Flags", OLine_Flags_String(oline.flags)));
                break;
            case 4:
                Help_Window([
                    "When the /OPER command is successful, the user will inherit",
                    "this IRC Class."
                ]);
                oline.ircclass = Edit_Class(oline.ircclass);
                break;
        }
    }
}

function Operator_Menu() {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, opts, i;

    while (!exit) {
        opts = [];
        for (i in OLines) {
            opts.push(format("%s (%s)", OLines[i].nick, OLines[i].hostmask));
        }
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT | WIN_INS | WIN_DEL,
            "IRC Operators",
            opts,
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        if (result & MSK_INS) {
            OLines.push(new OLine("user@*.example.com", "*", "Nickname", parse_oline_flags("o"), YLines[10] ? 10 : 0));
            Edit_Operator(OLines.length-1);
            continue;
        } else if (result & MSK_DEL) {
            result &= MSK_OFF;
            OLines.splice(result, 1);
            continue;
        } else {
            Edit_Operator(result);
            continue;
        }
    }
}

function Edit_Ban(kl) {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, opts, i;
    var kline = KLines[kl];

    while (!exit) {
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT,
            "Editing Ban",
            [
                format("%20s: %-40s", "Hostmask", kline.hostmask),
                format("%20s: %-40s", "Reason", kline.reason),
            ],
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        switch(result) {
            case 0:
                Help_Window([
                    "If either the connecting user's hostmask or IP mask match this",
                    "(glob-style wildcards supported), then the user is refused access.",
                    "Example: *@*.example.com"
                ]);
                kline.hostmask = Edit_Existing("Hostmask", kline.hostmask, 255, true);
                break;
            case 1:
                Help_Window([
                    "This string is displayed to the banned user when they're refused",
                    "access."
                ]);
                kline.reason = Edit_Existing("Reason", kline.reason);
                break;
        }
    }
}

function Ban_Menu() {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, opts, i;

    while (!exit) {
        opts = [];
        for (i in KLines) {
            opts.push(format("%s (%s)", KLines[i].hostmask, KLines[i].reason.slice(0,40)));
        }
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT | WIN_INS | WIN_DEL,
            "Bans",
            opts,
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        if (result & MSK_INS) {
            KLines.push(new KLine("user@*.example.com", "No reason configured", "K"));
            Edit_Ban(KLines.length-1);
            continue;
        } else if (result & MSK_DEL) {
            result &= MSK_OFF;
            KLines.splice(result, 1);
            continue;
        } else {
            Edit_Ban(result);
            continue;
        }
    }
}

function Edit_Server(servername) {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var existing_c = Find_CLine(servername), existing_n = Find_NLine(servername);
    var source = existing_c || existing_n;
    var cline = existing_c || new CLine(source.host, source.password, source.servername, 0, source.ircclass, source.tls);
    var nline = existing_n || new NLine(source.host, source.password, source.servername, 0, source.ircclass, source.tls);
    var original = JSON.stringify([cline, nline]);
    var result, opts, i, hub, found_hub;

    while (!exit) {
        hub = Find_Hub(cline.servername);
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT,
            "Editing Server",
            [
                format("%20s: %-40s", "TCP/IP Address", cline.host),
                format("%20s: %-40lu", "TCP/IP Port", cline.port),
                format("%20s: %-40s", "Outbound Password", cline.password ? "********" : ""),
                format("%20s: %-40s", "Inbound Password", nline.password ? "********" : ""),
                format("%20s: %-40s", "ServerName", cline.servername),
                format("%20s: %-40lu", "IRC Class", cline.ircclass),
                format("%20s: %-40s", "Flags", NLine_Flags_String(nline.flags)),
                format("%20s: %-40s", "Hub", hub),
                format("%20s: %-40s", "Require TLS", !!cline.tls || !!nline.tls || cline.port == 6697 || cline.port == 994)
            ],
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        switch(result) {
            case 0:
                Help_Window([
                    "The IP address or DNS hostname for this server.",
                    "*NOTE* that while often this is the same as 'Server Name', it's",
                    "not always the case."
                ]);
                cline.host = Edit_Existing("TCP/IP Address", cline.host);
                nline.host = cline.host;
                break;
            case 1:
                Help_Window([
                    "TCP/IP port where the server is reachable. Use 0 for inbound-only links."
                ]);
                cline.port = Edit_Integer("TCP/IP Port (0 for inbound only)", cline.port, 0, 65535);
                if (cline.port == 6697 || cline.port == 994) cline.tls = nline.tls = true;
                break;
            case 2:
                Help_Window([
                    "The password sent TO the server when it connects.",
                    "Enable Require TLS to encrypt and validate this connection."
                ]);
                cline.password = Edit_Existing("Outbound Password", cline.password, 255, true, true);
                break;
            case 3:
                Help_Window([
                    "The password expected to be received FROM the server when it connects.",
                    "Enable Require TLS to reject plaintext connections from this server."
                ]);
                nline.password = Edit_Existing("Inbound Password", nline.password, 255, true, true);
                break;
            case 4:
                Help_Window([
                    "The server name as it appears on the IRC network.",
                    "*NOTE* that this can be different from the TCP/IP address.",
                    "Use the exact server name agreed with the peer administrator."
                ]);
                var name = Edit_Existing("ServerName", cline.servername, 255, true);
                var other_c = Find_CLine(name), other_n = Find_NLine(name);
                if ((other_c && other_c != existing_c) || (other_n && other_n != existing_n)) {
                    uifc.msg("That server is already configured."); break;
                }
                HLines.forEach(function(line) {
                    if (line.servername.toLowerCase() == cline.servername.toLowerCase()) line.servername = name;
                });
                cline.servername = nline.servername = name;
                break;
            case 5:
                Help_Window([
                    "The IRC class that this server is a member of."
                ]);
                cline.ircclass = Edit_Class(cline.ircclass);
                nline.ircclass = cline.ircclass;
                break;
            case 6:
                Help_Window([
                    "Flags affect certain server behaviours.",
                    "See docs/ircd.txt for a full list of flags and their function.",
                    "Most sysops will leave this blank (no flags)"
                ]);
                nline.flags = parse_nline_flags(Edit_Existing("Flags", NLine_Flags_String(nline.flags)));
                break;
            case 7:
                Help_Window([
                    "Hubs are servers allowed to have other servers connected to them.",
                    "Most sysops should answer 'yes' here.  The only reason to answer 'no'",
                    "is if you're running a hub yourself and this server is a leaf."
                ]);
                hub = Yes_No("Is this server a hub?", hub);
                if (!hub) {
                    HLines = HLines.filter(function(line) {
                        return line.servername.toLowerCase() != cline.servername.toLowerCase();
                    });
                } else if (hub && !Find_Hub(cline.servername)) {
                    HLines.push(new HLine("*", cline.servername));
                }
                break;
            case 8:
                Help_Window([
                    "Use the peer's dedicated TLS port and a hostname in its certificate.",
                    "The certificate chain must validate against the configured TLS CA bundle.",
                    "Plaintext connections claiming this server name will be refused."
                ]);
                cline.tls = Yes_No("Require TLS for this server?", !!cline.tls || !!nline.tls) || cline.port == 6697 || cline.port == 994;
                nline.tls = cline.tls;
                if (cline.tls && !TLS_Config.ca_file)
                    TLS_Menu();
        }
    }
    if (JSON.stringify([cline, nline]) != original) {
        if (!existing_c) CLines.push(cline);
        if (!existing_n) NLines.push(nline);
    }
}

function Server_Menu() {
    var ctx = new uifc.list.CTX;
    while (!js.terminated) {
        var names = [];
        CLines.concat(NLines).forEach(function(line) {
            if (!Var_Exists_in_Array(names, line.servername)) names.push(line.servername);
        });
        var result = uifc.list(WIN_ORG | WIN_MID | WIN_ACT | WIN_INS | WIN_DEL,
                               "Servers", names, ctx);
        if (result < 0) return;
        if (result & MSK_INS) {
            var count = 1, name = "peer.example.net";
            while (Find_CLine(name) || Find_NLine(name)) name = "peer" + (++count) + ".example.net";
            var ircclass = YLines[30] ? 30 : 0;
            CLines.push(new CLine("127.0.0.1", "*", name, Number(Default_Port), ircclass));
            NLines.push(new NLine("127.0.0.1", "*", name, 0, ircclass));
            Edit_Server(name);
        } else if (result & MSK_DEL) {
            var name = names[result & MSK_OFF].toLowerCase();
            CLines = CLines.filter(function(line) { return line.servername.toLowerCase() != name; });
            NLines = NLines.filter(function(line) { return line.servername.toLowerCase() != name; });
            HLines = HLines.filter(function(line) { return line.servername.toLowerCase() != name; });
        } else Edit_Server(names[result]);
    }
}

function Services_Menu() {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, i, str;

    while (!exit) {
        Help_Window([
            "These servers have special exemptions to normal IRC rules.",
            "List trusted services hosts only. This does not enable embedded services.",
            "Use the hosts for your network; irc.synchro.net uses external services.",
            "Embedded services also need the same authority Host set on every BBS."
        ]);
        result = uifc.list(
            WIN_MID | WIN_ACT | WIN_INS | WIN_DEL,
            "IRC Services/Super Servers",
            ULines,
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        if (result & MSK_INS) {
            str = uifc.input(WIN_MID, "New IRC Services/Super Server Entry", "", 255);
            if (str === undefined || !str.length)
                continue;
            if (Find_ULine(str)) {
                uifc.msg("Entry already exists.");
                continue;
            }
            ULines.push(str);
            continue;
        } else if (result & MSK_DEL) {
            result &= MSK_OFF;
            ULines.splice(result, 1);
            continue;
        } else {
            var host = Edit_Existing("IRC Services/Super Server Entry", ULines[result], 255, true);
            if (Find_ULine(host) && host.toLowerCase() != ULines[result].toLowerCase())
                uifc.msg("Entry already exists.");
            else ULines[result] = host;
            continue;
        }
    }
}

function Edit_Restrict(ql) {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, opts, i;
    var qline = QLines[ql];

    while (!exit) {
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT,
            "Editing Nickname Restriction",
            [
                format("%20s: %-40s", "Nickmask", qline.nick),
                format("%20s: %-40s", "Reason", qline.reason),
            ],
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        switch(result) {
            case 0:
                Help_Window([
                    "The restricted nickname.  Glob-style wildcards are supported here.",
                    "Most sysops should restrict the following nicks:",
                    "*Serv, Sysop, IRCOp*, and Global."
                ]);
                qline.nick = Edit_Existing("Nickmask", qline.nick, 255, true);
                break;
            case 1:
                Help_Window([
                    "This string is displayed to the user when they attempt to use a",
                    "restricted nickname."
                ]);
                qline.reason = Edit_Existing("Reason", qline.reason);
                break;
        }
    }
}

function Restrict_Menu() {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, opts, i;

    while (!exit) {
        opts = [];
        for (i in QLines) {
            opts.push(QLines[i].nick);
        }
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT | WIN_INS | WIN_DEL,
            "Nickname Restrictions",
            opts,
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        if (result & MSK_INS) {
            Help_Window([
                "Enter the nickname you wish to restrict.",
                "Glob-style wildcards are allowed here."
            ]);
            str = uifc.input(WIN_MID, "New Nickname Restriction", "", 40);
            if (str === undefined || !str.length)
                continue;
            if (Var_Exists_in_Array(opts, str.toLowerCase())) {
                uifc.msg("Entry already exists.");
                continue;
            }
            QLines.push(new QLine(str,"No reason provided."));
            Edit_Restrict(QLines.length-1);
            continue;
        } else if (result & MSK_DEL) {
            result &= MSK_OFF;
            QLines.splice(result, 1);
            continue;
        } else {
            Edit_Restrict(result);
            continue;
        }
    }
}

function Edit_RBL(r) {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, opts, i, str;
    var rbl = RBL[r];

    while (!exit) {
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT,
            "Editing RBL",
            [
                format("%15s: %-50s", "Hostname", rbl.hostname),
                format(
                    "%15s: %-50s",
                    "Good Responses",
                    rbl.good ? rbl.good.join(",") : "(no whitelist responses configured)"
                ),
                format(
                    "%15s: %-50s",
                    "Bad Responses",
                    rbl.bad ? rbl.bad.join(",") : "(any response is considered a bad response)"
                )
            ],
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        switch(result) {
            case 0:
                Help_Window([
                    "The DNS hostname used to perform the RBL lookup."
                ]);
                rbl.hostname = Edit_Existing("Hostname", rbl.hostname);
                break;
            case 1:
                Help_Window([
                    "A comma-separated list of 'good' DNS responses.  These are whitelist",
                    "responses that allow a user to connect.  Leave blank for no whitelist.",
                    "Example: 127.0.0.9,127.0.0.99"
                ]);
                str = uifc.input(
                    WIN_MID,
                    "Good Responses",
                    rbl.good ? rbl.good.join(",") : "",
                    80,
                    K_EDIT
                );
                if (str === undefined || str == -1)
                    break;
                if (str == "") {
                    rbl.good = "";
                    break;
                }
                rbl.good = str.split(",");
                break;
            case 2:
                Help_Window([
                    "A comma-separated list of 'bad' DNS responses.  Users with RBL lookups",
                    "with a 'bad' response won't be allowed to connect.  Leave blank to treat",
                    "any response as a bad response.  Example: 127.0.0.1,127.0.0.8"
                ]);
                str = uifc.input(
                    WIN_MID,
                    "Bad Responses",
                    rbl.bad ? rbl.bad.join(",") : "",
                    80,
                    K_EDIT
                );
                if (str === undefined || str == -1)
                    break;
                if (str == "") {
                    rbl.bad = "";
                    break;
                }
                rbl.bad = str.split(",");
                break;
        }
    }
}

function RBL_Menu() {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, opts, i, str;

    while (!exit) {
        opts = [];
        for (i in RBL) {
            opts.push(RBL[i].hostname);
        }
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT | WIN_INS | WIN_DEL,
            "RBL",
            opts,
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        if (result & MSK_INS) {
            str = uifc.input(WIN_MID, "New RBL Hostname", "", 40);
            if (str === undefined || !str.length)
                continue;
            if (Var_Exists_in_Array(opts, str.toLowerCase())) {
                uifc.msg("Entry already exists.");
                continue;
            }
            RBL.push(new RBL_Config_Object(str,null,null));
            Edit_RBL(RBL.length-1);
            continue;
        } else if (result & MSK_DEL) {
            result &= MSK_OFF;
            RBL.splice(result, 1);
            continue;
        } else {
            Edit_RBL(result);
            continue;
        }
    }
}

function Attempt_Live_Rehash() {
    var semaphore = system.ctrl_dir + "ircd.rehash";
    if (!file_touch(semaphore)) {
        uifc.msg("Touch failed for " + semaphore);
        return false;
    }
    uifc.msg("Semaphore " + semaphore + " touched.");
    return true;
}

function Save_As() {
    var fn;

    Help_Window([
        "A bare filename without a path will save to ctrl/",
        "Otherwise provide a full path."
    ]);

    fn = uifc.input(WIN_MID, "Save Config", Config_Filename, 1024, K_EDIT);

    if (!fn || fn === undefined || fn == -1) {
        uifc.msg("Write aborted.");
        return false;
    }

    if(!(fn.indexOf('/')>=0 || fn.indexOf('\\')>=0))
        fn = system.ctrl_dir + fn;

    if (!Write_Config_File(fn)) {
        uifc.msg("Failed to write.");
        return false;
    }

    Config_Filename = fn;
    uifc.msg("Configuration saved.");
    return true;
}

function Save_and_Quit() {
    var overwrite = Yes_No("Overwrite " + Config_Filename + "?");
    if (overwrite === undefined) return false;
    if (!overwrite) return Save_As();
    if (!Write_Config_File(Config_Filename)) {
        uifc.msg("Failed to write " + Config_Filename);
        return false;
    }
    uifc.msg("Configuration saved.");
    return true;
}

/* Global */
var IRCDCFG_Editor = true;
var Time_Config_Read = 0;
var Config_Filename;
var Screen_Mode;
var Main_Menu_Exit = false;

for (var cmdarg = 0; cmdarg < argc; cmdarg++) {
    var option = argv[cmdarg].toLowerCase();
    if ((option != "-f" && option != "-i") || cmdarg + 1 >= argc)
        throw new Error("Usage: ircdcfg.js [-f config-file] [-i screen-mode]");
    if (option == "-f") Config_Filename = argv[++cmdarg];
    else Screen_Mode = argv[++cmdarg];
}

if (!uifc.init("Synchronet IRCd Configuration", Screen_Mode))
    throw new Error("UIFC init error");

try {
    Read_Config_File();
    Config_Filename = Config_Filename.replace(/\.conf$/i, ".ini");
    var ctx = new uifc.list.CTX;
    var menus = [
        ["Server Information", Info_Menu],
        ["TCP/IP Ports", Port_Menu],
        ["IRC Classes", Class_Menu],
        ["Allow Rules", Allow_Menu],
        ["IRC Operators", Operator_Menu],
        ["Bans", Ban_Menu],
        ["Servers", Server_Menu],
        ["IRC Services/Super Servers", Services_Menu],
        ["Nickname Restrictions", Restrict_Menu],
        ["RBL", RBL_Menu],
        ["TLS Settings", TLS_Menu],
        ["Network Policies", Network_Menu],
        ["WebIRC Gateways", WebIRC_Menu],
        ["Local BBS Services", BBS_Services_Menu],
        ["Embedded Services Authority", Embedded_Services_Menu],
        ["FileServ Settings", FileServ_Menu],
        ["Touch Semaphore (Rehash)", Attempt_Live_Rehash],
        ["Save As", Save_As],
        ["Save and Quit", function() { Main_Menu_Exit = Save_and_Quit(); }],
        ["Quit Immediately (No Save)", function() { Main_Menu_Exit = true; }]
    ];
    while (!Main_Menu_Exit && !js.terminated) {
        var result = uifc.list(WIN_ORG | WIN_MID | WIN_ACT, "Main Menu",
            menus.map(function(menu) { return menu[0]; }), ctx);
        if (result < 0) {
            var save = Yes_No("Save configuration before exit?");
            if (save === false) break;
            if (save === true && Save_and_Quit()) break;
        } else menus[result][1]();
    }
} finally {
    uifc.bail();
}
