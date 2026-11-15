// Message scan configuration and new-scan pointer configuration
// (what bbs.cfg_msg_scan() and bbs.cfg_msg_ptrs() run)
// for Synchronet v3.22+
// Set SCFG->System->Loadable Modules->Scan Config to "scancfg"

// Usage: scancfg cfg <SCAN_CFG_NEW | SCAN_CFG_TOYOU>
//        scancfg ptrs

// Customizable via modopts/scancfg options:
// - selected_item_prefix, unselected_item_prefix: what marks the default item
//   of a list and what takes its place before the other items
// - selected_item_attr: attributes of the default item's highlighted name
// - lightbar: true for selected_item_attr to highlight the whole item
// - more_fmt: the indicator shown when a list doesn't fit on the screen
// Without the first three, no default item is marked and the navigation keys
// only page through a list that doesn't fit on the screen.

require("sbbsdefs.js", "SCAN_CFG_NEW");
require("userdefs.js", "UREST_QWK_NODE");
require("select_item.js", "select_item");

var options = load("modopts.js", "scancfg", {});

var marked = options.selected_item_prefix !== undefined
	|| options.unselected_item_prefix !== undefined
	|| Boolean(options.selected_item_attr);

var PTR_NONE = 0xffffffff;	// a pointer past any message: nothing is new

function item_prefix(is_default)
{
	var prefix = is_default ? options.selected_item_prefix : options.unselected_item_prefix;
	return prefix === undefined ? "" : prefix;
}

function prefix_width()
{
	return Math.max(strip_ctrl(item_prefix(true)).length, strip_ctrl(item_prefix(false)).length);
}

// The name field of a list item format: a plain %s (width: 0) or one of a
// fixed width (e.g. %-40.40s). The name of the default item is highlighted
// only in such a field, so that a customized format is never misaligned.
function name_field(fmt)
{
	var m = fmt.match(/%(?:-(\d+)\.(\d+))?s/);
	if(!m || m[1] != m[2])
		return null;
	return { spec: m[0], width: m[1] ? parseInt(m[1], 10) : 0 };
}

function item_str(fmt, num, name, state, widest, is_default)
{
	var field = (is_default && options.selected_item_attr) ? name_field(fmt) : null;
	if(field) {
		name = options.selected_item_attr + format("%-*s", widest, name.substr(0, widest)) + "\x01n";
		if(field.width > widest)
			name += format("%*s", field.width - widest, "");
		fmt = fmt.replace(field.spec, "%s");
	}
	// select_item() ends the line
	return format(fmt, num, name, state, "").replace(/[\r\n]+$/, "");
}

// Display a numbered list of message groups or sub-boards and prompt the user
// to choose from it. Returns what select_item() does: the index of the item
// chosen, -1 if the user quit, -2 for Enter when no item is the default, or
// one of the lst.keys command keys, lst.current then being the default item.
// 'lst' describes the list:
// - fmt: the item format (number, name, state)
// - header: what's displayed above the items
// - prompt: the prompt
// - state: optional function(index) returning an item's state (a string)
// - keys: optional command keys
// - current: the default item
function select_from(list, lst)
{
	var field = name_field(lst.fmt);
	var pw = marked ? prefix_width() : 0;
	var bar = Boolean(options.lightbar && options.selected_item_attr);
	var has_default = marked && Boolean(pw || bar || (options.selected_item_attr && field));
	var name_max = (field && field.width) ? field.width
		: Math.max(console.screen_columns - pw - 6, 1);
	var widest = 0;
	for(var i = 0; i < list.length; i++)
		widest = Math.max(widest, Math.min(list[i].description.length, name_max));

	function justify(i)	// right-justifies the item numbers
	{
		return (i < 9 ? " " : "") + (i < 99 ? " " : "");
	}

	function state(i)
	{
		return lst.state ? lst.state(i) : "";
	}

	function plain_item(i)	// without its own colors
	{
		return justify(i) + strip_ctrl(item_str(lst.fmt, i + 1, list[i].description, state(i), widest, false));
	}

	// A lightbar is the width of the widest item, of those without a state
	// (which is the item's last, aligned, column)
	var widest_item = 0;
	for(i = 0; bar && !lst.state && i < list.length; i++)
		widest_item = Math.max(widest_item, plain_item(i).length);

	var sel = {
		count: list.length,
		current: has_default ? Math.min(lst.current, list.length - 1) : -1,
		no_default: !has_default,
		keys: lst.keys,
		header: lst.header,
		prompt: function() { return lst.prompt; },
		print_item: function(i, is_default, redraw) {
			if(pw)
				console.print(item_prefix(is_default));
			if(!redraw)
				console.add_hotspot((i + 1) * 10 <= list.length ? (i + 1) + "\r" : i + 1);
			if(bar && is_default)
				console.print(options.selected_item_attr
					+ format("%-*s", widest_item, plain_item(i)) + "\x01n");
			else
				console.print(justify(i)
					+ item_str(lst.fmt, i + 1, list[i].description, state(i), widest, is_default));
		},
		search: function(i) { return [list[i].description, list[i].name, list[i].code || ""]; },
		more_fmt: options.more_fmt,
		more_indent: pw + 1
	};
	var result = select_item(sel);
	if(typeof result == 'string')
		lst.current = sel.current;
	return result;
}

function open_base(sub)
{
	var msgbase = new MsgBase(sub.code);
	if(!msgbase.open())
		return null;
	return msgbase;
}

// The last index record, its position in the index being its "index" property
function last_index(msgbase)
{
	var total = msgbase.total_msgs;
	while(total > 0) {
		var idx = msgbase.get_msg_index(/* by_offset: */true, total - 1, /* include_votes: */true);
		if(idx) {
			idx.index = total - 1;
			return idx;
		}
		total--;
	}
	return null;
}

// The time of the message a sub-board's new-scan pointer is at (the last
// message read), 0 when it's before the first message
function ptr_time(sub)
{
	var ptr = sub.scan_ptr;
	if(!ptr)
		return 0;
	var msgbase = open_base(sub);
	if(!msgbase)
		return 0;
	var t = 0;
	var last = last_index(msgbase);
	var first = last ? msgbase.get_msg_index(true, 0, true) : null;
	if(first && first.number <= ptr) {
		if(last.number <= ptr)
			t = last.time;
		else {
			var bot = 0;
			var top = last.index;
			t = first.time;
			while(bot <= top) {
				var mid = Math.floor((bot + top) / 2);
				var idx = msgbase.get_msg_index(true, mid, true);
				if(!idx)
					break;
				if(idx.number <= ptr) {
					t = idx.time;
					bot = mid + 1;
				} else
					top = mid - 1;
			}
		}
	}
	msgbase.close();
	return t;
}

// The new-scan pointer that makes the messages posted at or after a time new
function ptr_by_time(sub, t)
{
	if(!t)
		return 0;
	var msgbase = open_base(sub);
	if(!msgbase)
		return 0;
	var ptr = PTR_NONE;
	var last = last_index(msgbase);
	if(last && last.time >= t) {
		var match = last;
		var bot = 0;
		var top = last.index;
		while(bot <= top) {
			var mid = Math.floor((bot + top) / 2);
			var idx = msgbase.get_msg_index(true, mid, true);
			if(!idx)
				break;
			if(idx.time < t) {
				bot = mid + 1;
				continue;
			}
			match = idx;
			if(idx.time > t && mid > 0) {
				top = mid - 1;
				continue;
			}
			break;
		}
		if(match.number > 0)
			ptr = match.number - 1;
	}
	msgbase.close();
	return ptr;
}

// The new-scan pointer that makes the last 'count' messages new
function ptr_from_end(sub, count)
{
	var msgbase = open_base(sub);
	if(!msgbase)
		return 0;
	var last = msgbase.last_msg;
	msgbase.close();
	return count > last ? 0 : last - count;
}

// Prompt for where to set the new-scan pointer of a list of sub-boards (a
// number of messages from the end, a date or the last message) and set them.
// 'dflt_time' is the date offered when there's more than one sub-board.
function set_ptrs(subs, dflt_time)
{
	var keys = bbs.text(bbs.text.DateLastKeys).toUpperCase();
	var single = (subs.length == 1);
	var loading = bbs.text(bbs.text.LoadingMsgPtrs);
	var i;

	console.cmd_keys = keys;
	console.mnemonics(bbs.text(bbs.text.SetMsgPtrPrompt));
	var key = console.getkeys(keys + console.quit_key, 9999);
	if(key === 0 || key === console.quit_key || console.aborted) {
		console.aborted = false;
		return;
	}
	if(key === keys.charAt(0)) {	// Date
		var t = single ? (ptr_time(subs[0]) || time()) : dflt_time;
		t = bbs.get_newscantime(t);
		if(t === null || console.aborted) {
			console.aborted = false;
			return;
		}
		if(single)
			console.print(loading);
		for(i = 0; i < subs.length && bbs.online; i++) {
			if(!single)
				console.progress(loading, i, subs.length);
			subs[i].scan_ptr = ptr_by_time(subs[i], t);
		}
		if(!single)
			console.progress(loading, subs.length, subs.length);
		return;
	}
	// The last message: Enter or the key for it
	var count = (typeof key == 'number') ? key : 0;
	var end = (key === keys.charAt(1)) || !single;
	for(i = 0; i < subs.length && bbs.online; i++) {
		if(!single)
			console.progress(loading, i, subs.length);
		if(count == 0 && end)
			subs[i].scan_ptr = PTR_NONE;
		else
			subs[i].scan_ptr = ptr_from_end(subs[i], count);
	}
	if(!single)
		console.progress(loading, subs.length, subs.length);
}

function ptr_cfg()
{
	var grps = msg_area.grp_list;
	var grp = { current: 0 };

	while(bbs.online && !js.terminated) {
		console.cmd_keys = console.all_key;
		grp.fmt = bbs.text(bbs.text.CfgGrpLstFmt);
		grp.header = bbs.text(bbs.text.CfgGrpLstHdr);
		grp.prompt = bbs.text(bbs.text.WhichOrAll);
		grp.keys = console.all_key;
		var g = select_from(grps, grp);
		if(g === -1 || g === -2)
			break;
		if(typeof g == 'string') {	// every group
			var all = [];
			for(var i = 0; i < grps.length; i++)
				all = all.concat(grps[i].sub_list);
			set_ptrs(all, time());
			continue;
		}
		grp.current = g;
		var subs = grps[g].sub_list;
		var sub = { current: 0 };
		while(bbs.online && !js.terminated) {
			var times = [];
			var ptr_at = function(i) {
				if(times[i] === undefined)
					times[i] = ptr_time(subs[i]);
				return times[i];
			};
			console.cmd_keys = console.all_key;
			sub.fmt = bbs.text(bbs.text.SubPtrLstFmt);
			sub.header = format(bbs.text(bbs.text.CfgSubLstHdr), grps[g].description);
			sub.prompt = bbs.text(bbs.text.WhichOrAll);
			sub.keys = console.all_key;
			sub.state = function(i) {
				var t = ptr_at(i);
				return t ? system.timestr(t) : bbs.text(bbs.text.All);
			};
			var s = select_from(subs, sub);
			if(console.aborted)
				return;
			if(s === -1 || s === -2)
				break;
			if(typeof s == 'string') {	// the entire group
				var newest = 0;
				for(var j = 0; j < subs.length; j++)
					newest = Math.max(newest, ptr_at(j));
				set_ptrs(subs, newest || time());
				continue;
			}
			sub.current = s;
			set_ptrs([subs[s]]);
		}
	}
}

function scan_cfg(misc)
{
	var grps = msg_area.grp_list;
	var nscan = Boolean(misc & SCAN_CFG_NEW);
	var qwk_node = Boolean(user.security.restrictions & UREST_QWK_NODE);
	var grp = { current: 0 };

	misc &= ~SCAN_CFG_YONLY;
	while(bbs.online && !js.terminated) {
		grp.fmt = bbs.text(bbs.text.CfgGrpLstFmt);
		grp.header = bbs.text(bbs.text.CfgGrpLstHdr);
		grp.prompt = bbs.text(nscan ? bbs.text.NScanCfgWhichGrp : bbs.text.SScanCfgWhichGrp);
		var g = select_from(grps, grp);
		if(g < 0)
			break;
		grp.current = g;
		var subs = grps[g].sub_list;
		var sub = { current: 0 };
		while(bbs.online && !js.terminated) {
			console.cmd_keys = console.all_key;
			sub.fmt = bbs.text(bbs.text.CfgSubLstFmt);
			sub.header = format(bbs.text(bbs.text.CfgSubLstHdr), grps[g].description);
			sub.prompt = bbs.text(nscan ? bbs.text.NScanCfgWhichSub : bbs.text.SScanCfgWhichSub);
			sub.keys = console.all_key;
			sub.state = function(i) {
				if(!(subs[i].scan_cfg & misc))
					return bbs.text(bbs.text.Off);
				if(nscan && (subs[i].scan_cfg & SCAN_CFG_YONLY))
					return bbs.text(bbs.text.ToYouOnly);
				return bbs.text(bbs.text.On);
			};
			var s = select_from(subs, sub);
			if(console.aborted)
				return;
			if(s === -1 || s === -2)
				break;
			var you_only = 0;
			if(typeof s == 'string') {	// the entire group, set as its first sub-board isn't
				var on = subs[0].scan_cfg & misc;
				if(nscan && !on && !qwk_node && !console.noyes(bbs.text(bbs.text.MsgsToYouOnlyQ)))
					you_only = SCAN_CFG_YONLY;
				for(var j = 0; j < subs.length; j++) {
					if(on)
						subs[j].scan_cfg &= ~misc;
					else {
						if(nscan)
							subs[j].scan_cfg &= ~SCAN_CFG_YONLY;
						subs[j].scan_cfg |= misc | you_only;
					}
				}
				continue;
			}
			sub.current = s;
			if(nscan && !(subs[s].scan_cfg & misc)) {
				if(!qwk_node && !console.noyes(bbs.text(bbs.text.MsgsToYouOnlyQ)))
					subs[s].scan_cfg |= SCAN_CFG_YONLY;
				else
					subs[s].scan_cfg &= ~SCAN_CFG_YONLY;
			}
			subs[s].scan_cfg ^= misc;
		}
	}
}

if(argv[0] == "ptrs")
	ptr_cfg();
else
	scan_cfg(parseInt(argv[1], 10) || SCAN_CFG_NEW);
