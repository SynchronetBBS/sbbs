var inBBS = function() {
	return (typeof js.global.bbs == "undefined") ? false : true;
}

load("sbbsdefs.js");
if(inBBS)
	load("menu-command-helpers.js");

var menuFile = system.data_dir + "menus.json";

// We can recategorize stuff later if needed
var Commands = {
	'User' : {},
	'System' : {},
	'Messages' : {},
	'Externals' : {}, // Placeholder, don't populate
	'Files' : {},
	'Menus' : {} // Placeholder, don't populate
};

/*	Pattern:

	Commands[area][command] = {
		'Description' : "This is what this does."
	}
	if(inBBS()) // Not being loaded from jsexec
		Commands[area][command].Action = some.method;

	(If Action doesn't rely on the 'bbs' or 'console' objects, the inBBS()
	check can be skipped.)

	We can add other properties (default ARS, text, hotkey, etc.) later.

	Add custom functions to menu-command-helpers.js.
*/

// User & user-activity related functions

Commands.User.Config = {
	'Description' : "Enter the user settings configuration menu"
};
if(inBBS())
	Commands.User.Config.Action = bbs.user_config;

Commands.User.Find = {
	'Description' : "Find user by username/number"
};
if(inBBS())
	Commands.User.Find.Action = findUser;

Commands.User.Info = {
	'Description' : "Display current user information"
};
if(inBBS())
	Commands.User.Info.Action = bbs.user_info;

Commands.User.List = {
	'Description' : "List users"
}
if(inBBS())
	Commands.User.List.Action = bbs.list_users;

Commands.User.ListNodes = {
	'Description' : "List all node activity"
}
if(inBBS())
	Commands.User.ListNodes.Action = bbs.list_nodes;

Commands.User.ListNodesActive = {
	'Description' : "List all active nodes"
}
if(inBBS())
	Commands.User.ListNodesActive.Action = bbs.whos_online;

Commands.User.Logons = {
	'Description' : "Display the recent-logon list"
}
if(inBBS())
	Commands.User.Logons.Action = bbs.list_logons;

Commands.User.SelectShell = {
	'Description' : "Select a command shell"
}
if(inBBS())
	Commands.User.SelectShell.Action = bbs.select_shell;

Commands.User.SelectEditor = {
	'Description' : "Select an external message editor"
}
if(inBBS())
	Commands.User.SelectEditor.Action = bbs.select_editor;

// System areas & functions that don't fit elsewhere

Commands.System.Chat = {
	'Description' : "Enter the chat section"
}
if(inBBS())
	Commands.System.Chat.Action = function() { bbs.exec("?chat_sec.js"); }

Commands.System.ExternalPrograms = {
	'Description' : "External Programs section"
}
if(inBBS())
	Commands.System.ExternalPrograms.Action = bbs.xtrn_sec;

Commands.System.Info = {
	'Description' : "Display system information"
}
if(inBBS())
	Commands.System.Info.Action = bbs.sys_info;

Commands.System.LogOff = {
	'Description' : "Log off"
}
if(inBBS())
	Commands.System.LogOff.Action = bbs.logoff;

Commands.System.LogOffFast = {
	'Description' : "Log off, fast"
}
if(inBBS())
	Commands.System.LogOffFast.Action = bbs.logout;

Commands.System.NodeChat = {
	'Description' : "Enter multi-node chat"
}
if(inBBS())
	Commands.System.NodeChat.Action = bbs.multinode_chat;

Commands.System.NodeChatPrivate = {
	'Description' : "Enter private inter-node chat"
}
if(inBBS())
	Commands.System.NodeChatPrivate.Action = bbs.private_chat;

Commands.System.NodeMessage = {
	'Description' : "Inter-node private message prompt"
}
if(inBBS())
	Commands.System.NodeMessage.Action = bbs.private_message;

Commands.System.NodeStats = {
	'Description' : "Display node statistics"
}
if(inBBS())
	Commands.System.NodeStats.Action = bbs.node_stats;

Commands.System.PageSysop = {
	'Description' : "Page the sysop for chat"
}
if(inBBS())
	Commands.System.PageSysop.Action = bbs.page_sysop;

Commands.System.PageGuru = {
	'Description' : "Page the annoying guru for chat"
}
if(inBBS())
	Commands.System.PageGuru.Action = bbs.page_guru;

Commands.System.Stats = {
	'Description' : "Display system statistics"
}
if(inBBS())
	Commands.System.Stats.Action = bbs.sys_stats;

Commands.System.TextSection = {
	'Description' : "Text files section"
}
if(inBBS())
	Commands.System.TextSection.Action = bbs.text_sec;

Commands.System.TimeBank = {
	'Description' : "Time Bank"
}
if(inBBS())
	Commands.System.TimeBank.Action = bbs.time_bank;

Commands.System.Version = {
	'Description' : "Display software version information"
}
if(inBBS())
	Commands.System.Version.Action = bbs.ver;

// Message area related functions

Commands.Messages.BulkMail = {
	'Description' : "Send bulk private mail"
}
if(inBBS())
	Commands.Messages.BulkMail.Action = bbs.bulk_mail;

Commands.Messages.Find = {
	'Description' : "Search message groups/areas/subs"
}
if(inBBS())
	Commands.Messages.Find.Action = findMessages;

Commands.Messages.Post = {
	'Description' : "Post a message in the current area"
}
if(inBBS())
	Commands.Messages.Post.Action = bbs.post_msg;

Commands.Messages.MailRead = {
	'Description' : "Read private mail / email"
}
if(inBBS())
	Commands.Messages.MailRead.Action = bbs.read_mail;

Commands.Messages.QWKSection = {
	'Description' : "Enter the QWK transfer/configuration section"
}
if(inBBS())
	Commands.Messages.QWKSection.Action = bbs.qwk_sec;

Commands.Messages.Read = {
	'Description' : "Read messages in the current area"
}
if(inBBS())
	Commands.Messages.Read.Action = bbs.scan_msgs;

Commands.Messages.Scan = {
	'Description' : "Scan for new messages"
}
if(inBBS())
	Commands.Messages.Scan.Action = scanSubs;

Commands.Messages.ScanConfig = {
	'Description' : "New message scan configuration"
}
if(inBBS())
	Commands.Messages.ScanConfig.Action = bbs.cfg_msg_scan;

Commands.Messages.ScanPointers = {
	'Description' : "Configure message scan pointer values"
}
if(inBBS())
	Commands.Messages.ScanPointers.Action = bbs.cfg_msg_ptrs;

Commands.Messages.ScanPointersReinit = {
	'Description' : "Re-initialize new message scan pointer values"
}
if(inBBS())
	Commands.Messages.ScanPointersReinit.Action = bbs.reinit_msg_ptrs;

Commands.Messages.SelectGroup = {
	'Description' : "Select a message group"
}
if(inBBS())
	Commands.Messages.SelectGroup.Action = selectMessageGroup;

Commands.Messages.SelectArea = {
	'Description' : "Select a message area"
}
if(inBBS())
	Commands.Messages.SelectArea.Action = selectMessageArea;

Commands.Messages.SelectGroupAndArea = {
	'Description' : "Select a message group and area"
}
if(inBBS())
	Commands.Messages.SelectGroupAndArea.Action = selectGroupAndArea;

Commands.Messages.SendMail = {
	'Description' : "Send local private mail"
}
if(inBBS())
	Commands.Messages.SendMail.Action = sendMail;

Commands.Messages.SendNetMail = {
	'Description' : "Send netmail/email"
}
if(inBBS())
	Commands.Messages.SendNetMail.Action = sendNetMail;

Commands.Messages.SubInfo = {
	'Description' : "Display message sub-board information"
}
if(inBBS())
	Commands.Messages.SubInfo.Action = bbs.sub_info;

// File area related functions

Commands.Files.BatchMenu = {
	'Description' : "Enter the batch file transfer menu"
}
if(inBBS())
	Commands.Files.BatchMenu.Action = bbs.batch_menu;

Commands.Files.BatchDownload = {
	'Description' : "Start a batch download"
}
if(inBBS())
	Commands.Files.BatchDownload.Action = bbs.batch_download;

Commands.Files.DirInfo = {
	'Description' : "Display file directory information"
}
if(inBBS())
	Commands.Files.DirInfo.Action = bbs.dir_info;

Commands.Files.List = {
	'Description' : "List all files in the current directory"
}
if(inBBS())
	Commands.Files.List.Action = bbs.list_files;

Commands.Files.ListExtended = {
	'Description' : "List all files in current directory (extended info)"
}
if(inBBS())
	Commands.Files.ListExtended.Action = bbs.list_file_info;

Commands.Files.Scan = {
	'Description' : "Scan directories for files"
}
if(inBBS())
	Commands.Files.Scan.Action = bbs.scan_dirs;

Commands.Files.TempXfer = {
	'Description' : "Enter the temporary file transfer menu"
}
if(inBBS())
	Commands.Files.TempXfer.Action = bbs.temp_xfer;

Commands.Files.Upload = {
	'Description' : "Upload a file to the current directory"
}
if(inBBS())
	Commands.Files.Upload.Action = bbs.upload_file;

Commands.Files.XferPolicy = {
	'Description' : "Display the file transfer policy"
}
if(inBBS())
	Commands.Files.XferPolicy.Action = bbs.xfer_policy;

var getMenus = function() {
	if(!file_exists(menuFile))
		return;
	var f = new File(menuFile);
	f.open("r");
	Commands.Menus = JSON.parse(f.read());
	f.close();
}

getMenus();