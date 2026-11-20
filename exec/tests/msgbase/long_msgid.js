// A message whose Message-ID is longer than the buffer get_msgid() copies
// it into (256 bytes in the MsgBase header object): the header's "id"
// property is the whole Message-ID, exactly as stored.
//
// get_msgid() copied a stored Message-ID with strncpy() and did not
// terminate the buffer when the ID was as long as the buffer or longer;
// the "id" property was built from that buffer, so it read past the end
// of it and returned the truncated ID followed by whatever bytes came
// next in memory.

mkpath(system.temp_dir);

var path = system.temp_dir + "test_msgbase_long_msgid_" + Date.now();
var EXTENSIONS = [".shd", ".sdt", ".sid", ".sha", ".sda", ".ini", ".hash"];

function cleanup()
{
	EXTENSIONS.forEach(function (ext) {
		try {
			file_remove(path + ext);
		} catch (e) { /* ignore */ }
	});
}

// A Message-ID of 'len' bytes: "<", 'x' characters, "@long.example>"
function long_id(len)
{
	var tail = "@long.example>";
	var s = "<";
	while (s.length < len - tail.length)
		s += "x";
	return s + tail;
}

var failures = [];

function check(desc, got, expect)
{
	if (got !== expect)
		failures.push(format("%s: got %s (%u bytes), expected %u bytes"
			, desc, JSON.stringify(got), String(got).length, String(expect).length));
}

try {
	var mb = new MsgBase(path, /* is_path: */ true);
	if (!mb.open())
		throw new Error("open failed: " + mb.last_error);

	// 255 fits the 256-byte buffer; 256 and 300 do not
	[100, 255, 256, 300].forEach(function (len) {
		var id = long_id(len);
		if (!mb.save_msg({ to: "Someone", from: "Tester", subject: "id " + len, id: id }, "body"))
			throw new Error("save_msg failed: " + mb.last_error);
		var num = mb.last_msg;
		check(len + "-byte id, expanded header", mb.get_msg_header(false, num).id, id);
		check(len + "-byte id, unexpanded header", mb.get_msg_header(false, num, false).id, id);
		var all = mb.get_all_msg_headers();
		var h = all[num];
		check(len + "-byte id, get_all_msg_headers", h && h.number && h.id, id);
	});
	mb.close();
} finally {
	cleanup();
}

if (failures.length > 0)
	throw new Error("long Message-ID failures:\n  " + failures.join("\n  "));
