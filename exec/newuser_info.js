// New User Information/help module

require("sbbsdefs.js", "P_NOABORT");
require("text.js", "UseExternalEditorQ");
require("gettext.js", "gettext");

bbs.menu("../sbbs", P_NOABORT | P_NOERROR);
bbs.menu("../system", P_NOABORT | P_NOERROR);
bbs.menu("../newuser", P_NOABORT | P_NOERROR);

if (Object.getOwnPropertyNames(xtrn_area.editor).length && (system.newuser_questions & UQ_XEDIT) && bbs.text(UseExternalEditorQ)) {
	if (console.yesno(bbs.text(UseExternalEditorQ))) {
		if (!bbs.select_editor()) {
			console.print(gettext("Sorry, no external editors are available to you"))
			console.newline();
		}
	} else
		user.xedit = 0;
}

if ((system.newuser_questions & UQ_CMDSHELL) && bbs.text(CommandShellHeading)) {
	bbs.select_shell();
}
