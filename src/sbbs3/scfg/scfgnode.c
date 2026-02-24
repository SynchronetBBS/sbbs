/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "scfg.h"
#include "sbbs_ini.h"

static char* node_path_help =
	"`Node Directory:`\n"
	"\n"
	"This is the path to this node's private directory where its separate\n"
	"configuration and data files are stored.\n"
	"\n"
	"The drive and directory of this path can be set to any valid directory\n"
	"that can be accessed by `ALL` nodes of the BBS.\n"
;

void adjust_last_node()
{
	const char* section = "bbs";
	const char* key = "LastNode";


	FILE*       fp = iniOpenFile(cfg.filename, /* modify */ false);
	str_list_t  ini = iniReadFile(fp);
	iniCloseFile(fp);
	uint        last_node = iniGetUInteger(ini, section, key, cfg.sys_nodes);
	char        prompt[128];
	SAFEPRINTF(prompt, "Update Terminal Server 'LastNode' value to %u", cfg.sys_nodes);
	if (last_node < cfg.sys_nodes && uifc.confirm(prompt)) {
		fp = iniOpenFile(cfg.filename, /* modify */ true);
		if (fp == NULL)
			uifc.msgf("Error %d opening %s", errno, cfg.filename);
		else {
			iniSetUInteger(&ini, section, key, cfg.sys_nodes, NULL);
			iniWriteFile(fp, ini);
			iniCloseFile(fp);
		}
	}
	iniFreeStringList(ini);
}

void node_menu()
{
	char       str[81], savnode = 0;
	char       cfg_filename[MAX_PATH + 1];
	int        i, j;
	static int node_menu_dflt, node_bar;

	SAFECOPY(cfg_filename, cfg.filename);
	while (1) {
		SAFECOPY(cfg.filename, cfg_filename);
		for (i = 0; i < cfg.sys_nodes; i++)
			snprintf(opt[i], MAX_OPLN, "Node %d", i + 1);
		opt[i][0] = 0;
		j = WIN_ORG | WIN_ACT | WIN_INSACT | WIN_DELACT;
		if (cfg.sys_nodes > 1)
			j |= WIN_DEL | WIN_COPY;
		if (cfg.sys_nodes < MAX_NODES)
			j |= WIN_INS;
		if (savnode)
			j |= WIN_PASTE;
		uifc.helpbuf =
			"`Nodes:`\n"
			"\n"
			"This is the list of configured terminal server nodes. A node is required\n"
			"for each supported simultaneous 'caller'.\n"
			"\n"
			"To add a node, hit ~ INS ~.\n"
			"\n"
			"To delete a node, hit ~ DEL ~.\n"
			"\n"
			"To configure a node, select it using the arrow keys and hit ~ ENTER ~.\n"
			"\n"
			"To copy a node's configuration to another node, first select the source\n"
			"node with the arrow keys and hit ~ Ctrl-C ~. Then select the destination\n"
			"node and hit ~ Ctrl-V ~.\n"
		;

		i = uifc.list(j, 0, 0, 13, &node_menu_dflt, &node_bar, "Nodes", opt);
		if (i == -1) {
			if (savnode) {
				free_node_cfg(&cfg);
				savnode = 0;
			}
			return;
		}
		int msk = i & MSK_ON;
		if (msk == MSK_DEL) {
			sprintf(str, "Delete Node %d", cfg.sys_nodes);
			i = 1;
			uifc.helpbuf =
				"`Delete Node:`\n"
				"\n"
				"If you are positive you want to delete this node, select `Yes`. Otherwise,\n"
				"select `No` or hit ~ ESC ~.\n"
			;
			i = uifc.list(WIN_MID, 0, 0, 0, &i, 0, str, uifcYesNoOpts);
			if (!i) {
				--cfg.sys_nodes;
				cfg.new_install = new_install;
				save_main_cfg(&cfg);
				adjust_last_node();
				refresh_cfg(&cfg);
			}
			continue;
		}
		if (msk == MSK_INS && cfg.sys_nodes < MAX_NODES) {
			SAFECOPY(cfg.node_dir, cfg.node_path[cfg.sys_nodes - 1]);
			i = cfg.sys_nodes + 1;
			load_node_cfg(&cfg, error, sizeof(error));
			if (i == 1) {
				SAFEPRINTF(str, "../node%d/", i);
			} else {
				char* p;
				SAFECOPY(str, cfg.node_path[0]);
				if ((p = strchr(str, '1')) != NULL)
					sprintf(p, "%d/", i);
				else
					SAFEPRINTF(str, "../node%d/", i);
			}
			sprintf(tmp, "Node %d Directory", i);
			uifc.helpbuf = node_path_help;
			j = uifc.input(WIN_MID | WIN_SAV, 0, 0, tmp, str, 50, K_EDIT);
			uifc.changes = FALSE;
			if (j < 2)
				continue;
			truncsp(str);
			SAFECOPY(cfg.node_path[i - 1], str);
			md(str);
			cfg.node_num = ++cfg.sys_nodes;
			SAFECOPY(cfg.node_phone, "N/A");
			cfg.new_install = new_install;
			save_node_cfg(&cfg);
			save_main_cfg(&cfg);
			free_node_cfg(&cfg);
			adjust_last_node();
			refresh_cfg(&cfg);
			continue;
		}
		if (msk == MSK_COPY) {
			if (savnode)
				free_node_cfg(&cfg);
			i &= MSK_OFF;
			SAFECOPY(cfg.node_dir, cfg.node_path[i]);
			load_node_cfg(&cfg, error, sizeof(error));
			savnode = 1;
			continue;
		}
		if (msk == MSK_PASTE) {
			i &= MSK_OFF;
			SAFECOPY(cfg.node_dir, cfg.node_path[i]);
			cfg.node_num = i + 1;
			save_node_cfg(&cfg);
			refresh_cfg(&cfg);
			continue;
		}

		if (savnode) {
			free_node_cfg(&cfg);
			savnode = 0;
		}
		SAFECOPY(cfg.node_dir, cfg.node_path[i]);
		prep_dir(cfg.ctrl_dir, cfg.node_dir, sizeof(cfg.node_dir));

		load_node_cfg(&cfg, error, sizeof(error));
		cfg.node_num = i + 1;
		node_cfg();

		free_node_cfg(&cfg);
	}
}

void node_cfg()
{
	static int node_dflt;
	char       str[81];
	int        i;

	while (1) {
		i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Phone Number", cfg.node_phone);
		snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Login Requirements", cfg.node_arstr);
		snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Keep Node File Open", cfg.node_misc & NM_CLOSENODEDAB ? "No":"Yes");
		snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Daily Event", (cfg.node_daily_misc & EVENT_DISABLED) ? "<DISABLED>" : cfg.node_daily_cmd);
		snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Node Directory", cfg.node_path[cfg.node_num - 1]);
		snprintf(opt[i++], MAX_OPLN, "%-27.27s%s", "Text Directory", cfg.text_dir);
		opt[i][0] = 0;
		sprintf(str, "Node %d Configuration", cfg.node_num);
		uifc.helpbuf =
			"`Node Configuration:`\n"
			"\n"
			"Configuration settings specific to the selected node.\n"
		;
		switch (uifc.list(WIN_ACT | WIN_CHE | WIN_BOT | WIN_RHT, 0, 0, 60, &node_dflt, 0
		                  , str, opt)) {
			case -1:
				i = save_changes(WIN_MID | WIN_SAV);
				if (!i) {
					save_node_cfg(&cfg);
					save_main_cfg(&cfg);
					refresh_cfg(&cfg);
				}
				if (i != -1)
					return;
				break;
			case __COUNTER__:
				uifc.helpbuf =
					"`Node Phone Number:`\n"
					"\n"
					"This is the phone number to access the selected node (e.g. for `SEXPOTS`).\n"
					"This value is used for information purposes only.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 10, "Phone Number", cfg.node_phone, sizeof(cfg.node_phone) - 1, K_EDIT);
				break;
			case __COUNTER__:
				sprintf(str, "Node %u Login", cfg.node_num);
				getar(str, cfg.node_arstr);
				break;
			case __COUNTER__:
				i = cfg.node_misc & NM_CLOSENODEDAB ? 1:0;
				uifc.helpbuf =
					"`Keep Node File Open:`\n"
					"\n"
					"If you want to keep the shared node file (`ctrl/node.dab`) open,\n"
					"(for better performance and reliability) set this option to `Yes`.\n"
					"If want to keep the file closed (for `Samba` compatibility), set this\n"
					"option to `No`.\n"
				;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 10, 0, &i, 0
				              , "Keep Node File Open", uifcYesNoOpts);
				if (i == 0 && cfg.node_misc & NM_CLOSENODEDAB) {
					cfg.node_misc &= ~NM_CLOSENODEDAB;
					uifc.changes = TRUE;
				}
				else if (i == 1 && !(cfg.node_misc & NM_CLOSENODEDAB)) {
					cfg.node_misc |= NM_CLOSENODEDAB;
					uifc.changes = TRUE;
				}
				break;
			case __COUNTER__:
				edit_fixed_event("Daily", cfg.node_daily_cmd, &cfg.node_daily_misc,
				           "`Daily Event:`\n"
				           "\n"
				           "If you have an event that this node's terminal server should run every\n"
				           "day, enter the command line for that event here.\n"
				           "\n"
				           "An event can be any valid command line. If multiple programs or commands\n"
				           "are required, use a batch file or shell script.\n"
				           SCFG_CMDLINE_PREFIX_HELP
				           SCFG_CMDLINE_SPEC_HELP
				           );
				break;
			case __COUNTER__:
				uifc.helpbuf = node_path_help;
				uifc.input(WIN_MID | WIN_SAV, 0, 10, "Node Directory"
				           , cfg.node_path[cfg.node_num - 1], sizeof(cfg.node_path[0]) - 1, K_EDIT);
				break;
			case __COUNTER__:
				uifc.helpbuf =
					"`Text Directory:`\n"
					"\n"
					"This directory contains the system's menus and other important text\n"
					"files, so be sure the files and directories are copied or moved to this\n"
					"directory if you decide to change its location.\n"
				;
				uifc.input(WIN_MID | WIN_SAV, 0, 10, "Text Directory"
				           , cfg.text_dir, sizeof(cfg.text_dir) - 1, K_EDIT);
				break;
		}
	}
}
