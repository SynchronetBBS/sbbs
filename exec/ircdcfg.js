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

function Yes_No(str) {
    var ctx = new uifc.list.CTX;
    var exit = false;

    while (!exit) {
        switch(uifc.list(WIN_MID | WIN_ACT, str, ["Yes", "No"], ctx)) {
            case 0:
                return true;
                break;
            case 1:
                return false;
                break;
        }
    }
}

function Find_Hub(str) {
    var i;

    for (i in HLines) {
        if (str == HLines[i].servername)
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
        if (arr[i] == str)
            return true;
    }

    return false;
}

function Edit_Existing(desc, str) {
    var newstr = uifc.input(
        WIN_MID,
        desc,
        str,
        40 /* maxlen */,
        K_EDIT
    );
    if (newstr === undefined)
        return str;
    return newstr;
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
            ],
            ctx
        );
        if (result == -1)
            exit = true;
        switch(result) {
            case 0:
                Help_Window([
                    "The server name as it will appear on the IRC network.",
                    "Most of the time this should be the BBS QWK-ID + .synchro.net",
                    "Example: mybbs.synchro.net"
                ]);
                ServerName = Edit_Existing("ServerName", ServerName);
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
        }
    }
}

function Port_Menu() {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, str, ports, i;

    while (!exit) {
        ports = [ Default_Port ].concat(PLines);
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT | WIN_INS | WIN_DEL,
            "TCP/IP Ports",
            ports,
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        if (result & MSK_INS) {
            str = parseInt(uifc.input(WIN_MID, "New Port", "", 5));
            if (str === undefined || !str)
                continue;
            if (Var_Exists_in_Array(ports, str)) {
                uifc.msg(format("Port %lu already in the list.", str));
                continue;
            }
            ports.push(str);
        } else if (result & MSK_DEL) {
            result &= MSK_OFF;
            if (result == 0) {
                uifc.msg("Can't delete the default port.");
                continue;
            }
            ports.splice(result, 1);
        } else if (result == 0) {
            Help_Window([
                "The first port in the list is always the default port."
            ]);
            str = parseInt(uifc.input(WIN_MID, "Default Port", Default_Port, 5, K_EDIT));
            if (str === undefined || !str)
                continue;
            Default_Port = str;
        } else {
            str = parseInt(uifc.input(WIN_MID, "Change Port", ports[result], 5, K_EDIT));
            if (str === undefined || !str)
                continue;
            if (Var_Exists_in_Array(ports, str)) {
                uifc.msg(format("Port %lu already in the list.", str));
                continue;
            }
            ports[result] = str;
        }
        ports.shift();
        PLines = ports;
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
                ircclass.pingfreq = parseInt(Edit_Existing("Ping Frequency", ircclass.pingfreq));
                break;
            case 2:
                Help_Window([
                    "How often, in seconds, the IRCd attempts to connect to a disconnected",
                    "server.  This option has no use for clients (use 0 in that case).",
                    "'0' here for a server means it should never attempt to auto-connect."
                ]);
                ircclass.connfreq = parseInt(Edit_Existing("Connect Frequency", ircclass.connfreq));
                break;
            case 3:
                Help_Window([
                    "The maximum number of users or servers allowed in this class.",
                    "If a server or user attempts to connect when this is exceeded,",
                    "the connection is denied."
                ]);
                ircclass.maxlinks = parseInt(Edit_Existing("Maximum Links", ircclass.maxlinks));
                break;
            case 4:
                Help_Window([
                    "The maximum number of bytes held in the internal IRCd send buffer",
                    "before the user or server is forcibly disconnected for flooding.",
                ]);
                ircclass.sendq = parseInt(Edit_Existing("Maximum Send Queue", ircclass.sendq));
                break;
        }
    }
}

function Class_Menu() {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, str, opts, i;

    YLines[0].comment = "Hardcoded fallback, not configurable.";

    while (!exit) {
        opts = [];
        for (i in YLines) {
            opts.push(format("%lu%s", i, YLines[i].comment ? " (" + YLines[i].comment + ")" : ""));
        }
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT | WIN_INS | WIN_DEL,
            "IRC Classes",
            opts,
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        if (result & MSK_INS) {
            Help_Window([
                "IRC Class numbers are arbitrary and have no programmatic significance.",
                "They're usually organized in groups by the Sysop / Operator.",
                "i.e., below 10 is for regular users, above 20 is for servers, etc."
            ]);
            str = parseInt(uifc.input(WIN_MID, "New Class Number", "", 3));
            if (str === undefined || !str)
                continue;
            if (YLines[str]) {
                uifc.msg(format("Class %lu already in the list.", str));
                continue;
            }
            YLines[str] = new YLine(120,600,100,1000000);
            Edit_IRC_Class(YLines[str]);
        } else if (result & MSK_DEL) {
            result &= MSK_OFF;
            delete YLines[opts[result].split(" ")[0]];
        } else {
            if (result == 0) {
                uifc.msg("Class 0 is a hardcoded fallback and can't be configured.");
                continue;
            }
            Edit_IRC_Class(YLines[opts[result].split(" ")[0]]);
        }
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
                iline.hostmask = Edit_Existing("Mask", iline.hostmask);
                iline.ipmask = iline.hostmask;
                break;
            case 1:
                Help_Window([
                    "IRC Class that will be assigned to users connecting that",
                    "match the allow rule."
                ]);
                iline.ircclass = parseInt(Edit_Existing("IRC Class", iline.ircclass));
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
            ILines.push("*@*",null,"*@*",null,1);
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
            ILines.push(new ILine("*@*",null,"*@*",null,1));
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
                format("%20s: %-40s", "Password", oline.password),
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
                oline.password = Edit_Existing("Password", oline.password);
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
                oline.ircclass = parseInt(Edit_Existing("IRC Class", oline.ircclass));
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
            OLines.push(new OLine("user@*.example.com", "*", "Nickname", "o", 10));
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
                kline.hostmask = Edit_Existing("Hostmask", kline.hostmask);
                break;
            case 1:
                Help_Window([
                    "This string is displayed to the banned user when they're refused",
                    "access."
                ]);
                kline.password = Edit_Existing("Reason", kline.reason);
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

function Edit_Server(cl) {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var cline = CLines[cl];
    var nline = NLines[cl];
    var result, opts, i, hub, found_hub;

    while (!exit) {
        hub = Find_Hub(cline.servername);
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT,
            "Editing Server",
            [
                format("%20s: %-40s", "TCP/IP Address", cline.host),
                format("%20s: %-40lu", "TCP/IP Port", cline.port),
                format("%20s: %-40s", "Outbound Password", cline.password),
                format("%20s: %-40s", "Inbound Password", nline.password),
                format("%20s: %-40s", "ServerName", cline.servername),
                format("%20s: %-40lu", "IRC Class", cline.ircclass),
                format("%20s: %-40s", "Flags", NLine_Flags_String(nline.flags)),
                format("%20s: %-40s", "Hub", hub)
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
                    "TCP/IP port where the server is reachable."
                ]);
                cline.port = parseInt(Edit_Existing("TCP/IP Port", cline.port));
                break;
            case 2:
                Help_Window([
                    "The password sent TO the server when it connects.",
                    "*NOTE* This password is not encrypted."
                ]);
                cline.password = Edit_Existing("Outbound Password", cline.password);
                break;
            case 3:
                Help_Window([
                    "The password expected to be received FROM the server when it connects.",
                    "*NOTE* This password is not encrypted."
                ]);
                nline.password = Edit_Existing("Inbound Password", nline.password);
                break;
            case 4:
                Help_Window([
                    "The server name as it appears on the IRC network.",
                    "*NOTE* that this can be different from the TCP/IP address.",
                    "For most sysops, this will be qwkid.synchro.net"
                ]);
                cline.servername = Edit_Existing("ServerName", cline.servername);
                nline.servername = cline.servername;
                break;
            case 5:
                Help_Window([
                    "The IRC class that this server is a member of."
                ]);
                cline.ircclass = parseInt(Edit_Existing("IRC Class", cline.ircclass));
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
                hub = Yes_No("Is this server a hub?");
                if (!hub) {
                    for (i in HLines) {
                        if (HLines[i].servername == cline.servername)
                            HLines.splice(i, 1);
                    }
                } else if (hub && !Find_Hub(cline.servername)) {
                    HLines.push(new HLine("*", cline.servername));
                }
        }
    }
}

function Server_Menu() {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, opts, i;

    while (!exit) {
        opts = [];
        for (i in CLines) {
            opts.push(CLines[i].servername);
        }
        result = uifc.list(
            WIN_ORG | WIN_MID | WIN_ACT | WIN_INS | WIN_DEL,
            "Servers",
            opts,
            ctx
        );
        if (result == -1) {
            exit = true;
            break;
        }
        if (result & MSK_INS) {
            CLines.push(new CLine("127.0.0.1", "*", "example.synchro.net", Default_Port, 10));
            NLines.push(new NLine("127.0.0.1", "*", "example.synchro.net", 0, 10));
            Edit_Server(CLines.length-1);
            continue;
        } else if (result & MSK_DEL) {
            result &= MSK_OFF;
            CLines.splice(result, 1);
            NLines.splice(result, 1);
            continue;
        } else {
            Edit_Server(result);
            continue;
        }
    }
}

function Services_Menu() {
    var ctx = new uifc.list.CTX;
    var exit = false;
    var result, i, str;

    while (!exit) {
        Help_Window([
            "These servers have special exemptions to normal IRC rules.  Only",
            "IRC services should be listed here.  Most sysops should only have",
            "two entries: services.synchro.net and stats.synchro.net"
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
            str = uifc.input(WIN_MID, "New IRC Services/Super Server Entry", "", 40);
            if ((str === undefined) || (str == -1))
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
            ULines[result] = Edit_Existing("IRC Services/Super Server Entry", ULines[result]);
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
                qline.nick = Edit_Existing("Nickmask", qline.nick);
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
            if ((str === undefined) || (str == -1))
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
            if ((str === undefined) || (str == -1))
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

    fn = uifc.input(WIN_MID, "Save Config", "", 100);

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

    uifc.msg("Configuration saved.");
    return true;
}

function Save_and_Quit() {
    if (!Yes_No("Overwrite " + Config_Filename + "?")) {
        return Save_As();
    }
    if (!Write_Config_File(Config_Filename)) {
        uifc.msg("Failed to write " + Config_Filename);
        return false;
    }
    uifc.msg("Configuration saved.");
    return true;
}

/* Global */
const IRCDCFG_Editor = true;
var Time_Config_Read = 0;
var Config_Filename;
var Screen_Mode;
var Main_Menu_Exit = false;

for (var cmdarg=0;cmdarg<argc;cmdarg++) {
    switch(argv[cmdarg].toLowerCase()) {
        case "-f":
            Config_Filename = argv[++cmdarg];
            break;
        case "-i":
            Screen_Mode = argv[++cmdarg];
            break;
    }
}

if(!uifc.init("Synchronet IRCd Configuration", Screen_Mode))
    throw("UIFC init error");

var Global_Help_CTX = new uifc.list.CTX;

Read_Config_File();

if (Config_Filename.match(/[.][Cc][Oo][Nn][Ff]$/)) {
    Config_Filename = Config_Filename.replace(/[.][Cc][Oo][Nn][Ff]$/, ".ini");
}

var ctx = new uifc.list.CTX;
while (!Main_Menu_Exit && !js.terminated) {
    var result = uifc.list(
        WIN_ORG | WIN_MID | WIN_ACT,
        "Main Menu",
        [
            "Server Information",
            "TCP/IP Ports",
            "IRC Classes",
            "Allow Rules",
            "IRC Operators",
            "Bans",
            "Servers",
            "IRC Services/Super Servers",
            "Nickname Restrictions",
            "RBL",
            " ",
            "Touch Semaphore (Rehash)",
            " ",
            "Save As",
            "Save and Quit",
            "Quit Immediately (No Save)"
        ],
        ctx
    );
    if (result == -1) {
        if (Yes_No("Save configuration before exit?"))
            Save_and_Quit();
        break;
    }
    switch (result) {
        case 0:
            Info_Menu();
            break;
        case 1:
            Port_Menu();
            break;
        case 2:
            Class_Menu();
            break;
        case 3:
            Allow_Menu();
            break;
        case 4:
            Operator_Menu();
            break;
        case 5:
            Ban_Menu();
            break;
        case 6:
            Server_Menu();
            break;
        case 7:
            Services_Menu();
            break;
        case 8:
            Restrict_Menu();
            break;
        case 9:
            RBL_Menu();
            break;
        case 10: /* Blank */
            break;
        case 11:
            Attempt_Live_Rehash();
            break;
        case 12: /* Blank */
            break;
        case 13: /* Save As */
            Save_As();
            break;
        case 14: /* Save and Quit */
            Save_and_Quit();
            break;
        case 15: /* Quit Only (No Save) */
            Main_Menu_Exit = true;
            break;
    }
}

uifc.bail();
