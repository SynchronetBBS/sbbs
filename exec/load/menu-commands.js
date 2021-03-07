// Mapping of menuedit/menushell 'commands' to built-in or helper functions

var inBBS = js.global.bbs !== undefined;

load("sbbsdefs.js");
if (inBBS) load("menu-command-helpers.js");

var menuFile = system.data_dir + "menus.json";

// We can recategorize stuff later if needed
var Commands = {
	User: {},
	System: {},
	Messages: {},
	Externals: {}, // Placeholder, don't populate
	Files: {},
	Menus: {}, // Placeholder, don't populate
};

function _wrap(fn) {
	return function() {
		fn();
	}
}

function getMenus() {
	if (!file_exists(menuFile)) return;
	var f = new File(menuFile);
	f.open("r");
	Commands.Menus = JSON.parse(f.read());
	f.close();
}

/*	Pattern:

	Commands[area][command] = {
		Description: "This is what this does."
	}
	if (inBBS) { // Not being loaded from jsexec
		Commands[area][command].Action = some.method;
	}

	(If Action doesn't rely on the 'bbs' or 'console' objects, the inBBS()
	check can be skipped.)

	We can add other properties (default ARS, text, hotkey, etc.) later.

	Add custom functions to menu-command-helpers.js.
*/

// User & user-activity related functions

Commands.User.Config = {
	Description: "Enter the user settings configuration menu"
};

if (inBBS) {
	Commands.User.Config.Action = _wrap(bbs.user_config);
	Commands.User.Find.Action = findUser;
	Commands.User.Info.Action = _wrap(bbs.user_info);
	Commands.User.List.Action = _wrap(bbs.list_users);
	Commands.User.ListNodes.Action = _wrap(bbs.list_nodes);
	Commands.User.ListNodesActive.Action = _wrap(bbs.whos_online);
	Commands.User.Logons.Action = _wrap(bbs.list_logons);
	Commands.User.SelectShell.Action = _wrap(bbs.select_shell);
	Commands.User.SelectEditor.Action = _wrap(bbs.select_editor);
}

Commands.User.Find = {
	Description: "Find user by username/number"
};

Commands.User.Info = {
	Description: "Display current user information"
};

Commands.User.List = {
	Description: "List users"
}

Commands.User.ListNodes = {
	Description: "List all node activity"
}

Commands.User.ListNodesActive = {
	Description: "List all active nodes"
}

Commands.User.Logons = {
	Description: "Display the recent-logon list"
}

Commands.User.SelectShell = {
	Description: "Select a command shell"
}

Commands.User.SelectEditor = {
	Description: "Select an external message editor"
}

// System areas & functions that don't fit elsewhere

if (inBBS) {
	Commands.System.Chat.Action = function() { bbs.exec("?chat_sec.js"); }
	Commands.System.ExternalPrograms.Action = _wrap(bbs.xtrn_sec);
	Commands.System.Info.Action = _wrap(bbs.sys_info);
	Commands.System.LogOff.Action = _wrap(bbs.logoff);
	Commands.System.LogOffFast.Action = _wrap(bbs.logout);
	Commands.System.NodeChat.Action = _wrap(bbs.multinode_chat);
	Commands.System.NodeChatPrivate.Action = _wrap(bbs.private_chat);
	Commands.System.NodeMessage.Action = _wrap(bbs.private_message);
	Commands.System.NodeStats.Action = _wrap(bbs.node_stats);
	Commands.System.PageSysop.Action = _wrap(bbs.page_sysop);
	Commands.System.PageGuru.Action = _wrap(bbs.page_guru);
	Commands.System.Stats.Action = _wrap(bbs.sys_stats);
	Commands.System.TextSection.Action = _wrap(bbs.text_sec);
	Commands.System.TimeBank.Action = _wrap(bbs.time_bank);
	Commands.System.Version.Action = _wrap(bbs.ver);
}

Commands.System.Chat = {
	Description: "Enter the chat section"
}

Commands.System.ExternalPrograms = {
	Description: "External Programs section"
}

Commands.System.Info = {
	Description: "Display system information"
}

Commands.System.LogOff = {
	Description: "Log off"
}

Commands.System.LogOffFast = {
	Description: "Log off, fast"
}

Commands.System.NodeChat = {
	Description: "Enter multi-node chat"
}

Commands.System.NodeChatPrivate = {
	Description: "Enter private inter-node chat"
}

Commands.System.NodeMessage = {
	Description: "Inter-node private message prompt"
}

Commands.System.NodeStats = {
	Description: "Display node statistics"
}

Commands.System.PageSysop = {
	Description: "Page the sysop for chat"
}

Commands.System.PageGuru = {
	Description: "Page the annoying guru for chat"
}

Commands.System.Stats = {
	Description: "Display system statistics"
}

Commands.System.TextSection = {
	Description: "Text files section"
}

Commands.System.TimeBank = {
	Description: "Time Bank"
}

Commands.System.Version = {
	Description: "Display software version information"
}

// Message area related functions

if (inBBS) {
	Commands.Messages.BulkMail.Action = _wrap(bbs.bulk_mail);
	Commands.Messages.Find.Action = findMessages;
	Commands.Messages.Post.Action = _wrap(bbs.post_msg);
	Commands.Messages.MailRead.Action = _wrap(bbs.read_mail);
	Commands.Messages.QWKSection.Action = _wrap(bbs.qwk_sec);
	Commands.Messages.Read.Action = _wrap(bbs.scan_msgs);
	Commands.Messages.Scan.Action = scanSubs;
	Commands.Messages.ScanConfig.Action = _wrap(bbs.cfg_msg_scan);
	Commands.Messages.ScanPointers.Action = _wrap(bbs.cfg_msg_ptrs);
	Commands.Messages.ScanPointersReinit.Action = _wrap(bbs.reinit_msg_ptrs);
	Commands.Messages.SelectGroup.Action = selectMessageGroup;
	Commands.Messages.SelectArea.Action = selectMessageArea;
	Commands.Messages.SelectGroupAndArea.Action = selectGroupAndArea;
	Commands.Messages.SendMail.Action = sendMail;
	Commands.Messages.SendNetMail.Action = sendNetMail;
	Commands.Messages.SubInfo.Action = _wrap(bbs.sub_info);
}

Commands.Messages.BulkMail = {
	Description: "Send bulk private mail"
}

Commands.Messages.Find = {
	Description: "Search message groups/areas/subs"
}

Commands.Messages.Post = {
	Description: "Post a message in the current area"
}

Commands.Messages.MailRead = {
	Description: "Read private mail / email"
}

Commands.Messages.QWKSection = {
	Description: "Enter the QWK transfer/configuration section"
}

Commands.Messages.Read = {
	Description: "Read messages in the current area"
}

Commands.Messages.Scan = {
	Description: "Scan for new messages"
}

Commands.Messages.ScanConfig = {
	Description: "New message scan configuration"
}

Commands.Messages.ScanPointers = {
	Description: "Configure message scan pointer values"
}

Commands.Messages.ScanPointersReinit = {
	Description: "Re-initialize new message scan pointer values"
}

Commands.Messages.SelectGroup = {
	Description: "Select a message group"
}

Commands.Messages.SelectArea = {
	Description: "Select a message area"
}

Commands.Messages.SelectGroupAndArea = {
	Description: "Select a message group and area"
}

Commands.Messages.SendMail = {
	Description: "Send local private mail"
}

Commands.Messages.SendNetMail = {
	Description: "Send netmail/email"
}

Commands.Messages.SubInfo = {
	Description: "Display message sub-board information"
}

// File area related functions

if (inBBS) {
	Commands.Files.BatchMenu.Action = _wrap(bbs.batch_menu);
	Commands.Files.BatchDownload.Action = _wrap(bbs.batch_download);
	Commands.Files.DirInfo.Action = _wrap(bbs.dir_info);
	Commands.Files.List.Action = _wrap(bbs.list_files);
	Commands.Files.ListExtended.Action = _wrap(bbs.list_file_info);
	Commands.Files.Scan.Action = _wrap(bbs.scan_dirs);
	Commands.Files.TempXfer.Action = _wrap(bbs.temp_xfer);
	Commands.Files.Upload.Action = _wrap(bbs.upload_file);
	Commands.Files.XferPolicy.Action = _wrap(bbs.xfer_policy);
}

Commands.Files.BatchMenu = {
	Description: "Enter the batch file transfer menu"
}

Commands.Files.BatchDownload = {
	Description: "Start a batch download"
}

Commands.Files.DirInfo = {
	Description: "Display file directory information"
}

Commands.Files.List = {
	Description: "List all files in the current directory"
}

Commands.Files.ListExtended = {
	Description: "List all files in current directory (extended info)"
}

Commands.Files.Scan = {
	Description: "Scan directories for files"
}

Commands.Files.TempXfer = {
	Description: "Enter the temporary file transfer menu"
}

Commands.Files.Upload = {
	Description: "Upload a file to the current directory"
}

Commands.Files.XferPolicy = {
	Description: "Display the file transfer policy"
}

getMenus();