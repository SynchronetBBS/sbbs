load('sbbsdefs.js');

var DEFAULTS = {
	lockScreenSize: false,
	columns: 80,
	rows: 25,
	minColumns: 80,
	maxColumns: 160,
	minRows: 24,
	maxRows: 60,
	enableTouchHandlers: true,
	enableWheelArrows: true,
	enableRenderSpeedPatch: true,
	enableParentAudioUnlockBridge: true,
	enableCanvasScale: true
};

function parseBool(value, fallback)
{
	if (typeof value === 'boolean')
		return value;
	if (typeof value === 'number')
		return value !== 0;
	if (value === undefined || value === null)
		return fallback;

	var str = String(value).toLowerCase();
	if (str === '1' || str === 'true' || str === 'yes' || str === 'on')
		return true;
	if (str === '0' || str === 'false' || str === 'no' || str === 'off')
		return false;
	return fallback;
}

function parseIntSafe(value, fallback, min, max)
{
	var n = parseInt(value, 10);
	if (!isFinite(n) || isNaN(n))
		n = fallback;
	if (isFinite(min) && n < min)
		n = min;
	if (isFinite(max) && n > max)
		n = max;
	return n;
}

function normalizeOptions(raw)
{
	var src = raw || {};
	var out = {};

	out.lockScreenSize = parseBool(src.lockScreenSize, DEFAULTS.lockScreenSize);
	out.columns = parseIntSafe(src.columns, DEFAULTS.columns, 40, 240);
	out.rows = parseIntSafe(src.rows, DEFAULTS.rows, 15, 120);
	out.minColumns = parseIntSafe(src.minColumns, DEFAULTS.minColumns, 40, 240);
	out.maxColumns = parseIntSafe(src.maxColumns, DEFAULTS.maxColumns, 40, 240);
	out.minRows = parseIntSafe(src.minRows, DEFAULTS.minRows, 15, 120);
	out.maxRows = parseIntSafe(src.maxRows, DEFAULTS.maxRows, 15, 120);

	out.enableTouchHandlers = parseBool(src.enableTouchHandlers, DEFAULTS.enableTouchHandlers);
	out.enableWheelArrows = parseBool(src.enableWheelArrows, DEFAULTS.enableWheelArrows);
	out.enableRenderSpeedPatch = parseBool(src.enableRenderSpeedPatch, DEFAULTS.enableRenderSpeedPatch);
	out.enableParentAudioUnlockBridge = parseBool(
		src.enableParentAudioUnlockBridge,
		DEFAULTS.enableParentAudioUnlockBridge
	);
	out.enableCanvasScale = parseBool(src.enableCanvasScale, DEFAULTS.enableCanvasScale);

	if (out.minColumns > out.maxColumns)
		out.minColumns = out.maxColumns;
	if (out.minRows > out.maxRows)
		out.minRows = out.maxRows;
	if (out.columns < out.minColumns)
		out.columns = out.minColumns;
	if (out.columns > out.maxColumns)
		out.columns = out.maxColumns;
	if (out.rows < out.minRows)
		out.rows = out.minRows;
	if (out.rows > out.maxRows)
		out.rows = out.maxRows;

	return out;
}

function getIniPath()
{
	var candidates = [
		fullpath(js.exec_dir + '../terminal-ui.ini'),
		fullpath(js.exec_dir + '../TERMINAL-UI.INI')
	];
	for (var i = 0; i < candidates.length; i += 1) {
		candidates[i] = String(candidates[i]).replace(/[\\\/]+$/, '');
		if (file_exists(candidates[i]))
			return candidates[i];
	}
	return candidates[0];
}

function readIniOptions(path)
{
	var file = new File(path);
	var section = 'terminal';
	var raw = {};
	var explicitLock;
	var allowResize;

	if (!file.open('r'))
		return null;

	explicitLock = file.iniGetValue(section, 'lock_screen_size', undefined);
	allowResize = file.iniGetValue(section, 'allow_resize', undefined);

	if (explicitLock === undefined || explicitLock === null) {
		raw.lockScreenSize = allowResize === undefined || allowResize === null
			? DEFAULTS.lockScreenSize
			: !parseBool(allowResize, true);
	} else {
		raw.lockScreenSize = parseBool(explicitLock, DEFAULTS.lockScreenSize);
	}

	raw.columns = file.iniGetValue(section, 'columns', DEFAULTS.columns);
	raw.rows = file.iniGetValue(section, 'rows', DEFAULTS.rows);
	raw.minColumns = file.iniGetValue(section, 'min_columns', DEFAULTS.minColumns);
	raw.maxColumns = file.iniGetValue(section, 'max_columns', DEFAULTS.maxColumns);
	raw.minRows = file.iniGetValue(section, 'min_rows', DEFAULTS.minRows);
	raw.maxRows = file.iniGetValue(section, 'max_rows', DEFAULTS.maxRows);
	raw.enableTouchHandlers = file.iniGetValue(section, 'enable_touch_handlers', DEFAULTS.enableTouchHandlers);
	raw.enableWheelArrows = file.iniGetValue(section, 'enable_mouse_wheel_arrows', DEFAULTS.enableWheelArrows);
	raw.enableRenderSpeedPatch = file.iniGetValue(section, 'enable_render_speed_patch', DEFAULTS.enableRenderSpeedPatch);
	raw.enableParentAudioUnlockBridge = file.iniGetValue(section, 'enable_parent_audio_unlock_bridge', DEFAULTS.enableParentAudioUnlockBridge);
	raw.enableCanvasScale = file.iniGetValue(section, 'enable_canvas_scale', DEFAULTS.enableCanvasScale);

	file.close();
	return normalizeOptions(raw);
}

var iniPath = getIniPath();
var options = normalizeOptions(DEFAULTS);
var loaded = false;

try {
	var iniOpts = readIniOptions(iniPath);
	if (iniOpts) {
		options = iniOpts;
		loaded = true;
	}
} catch (err) {
	loaded = false;
}

http_reply.header['Content-Type'] = 'application/json; charset=utf-8';
http_reply.header['Cache-Control'] = 'no-store, no-cache, must-revalidate';
http_reply.header['Pragma'] = 'no-cache';
http_reply.header['Expires'] = '0';
write(JSON.stringify({
	ok: true,
	loaded: loaded,
	path: iniPath,
	options: options
}));
