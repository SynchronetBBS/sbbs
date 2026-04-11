load('sbbsdefs.js');
var settings = load('modopts.js', 'web') || { web_directory: '../webv4' };

load(settings.web_directory + '/lib/init.js');
load(settings.web_lib + 'auth.js');

var CHUNK_SIZE = 4096;
var SHARED_ROOT = fullpath(backslash(system.mods_dir + 'flweb/assets/'));
var ALLOWED_EXT = {
    mp3: 'audio/mpeg',
    ogg: 'audio/ogg',
    wav: 'audio/wav',
    m4a: 'audio/mp4',
    aac: 'audio/aac',
    flac: 'audio/flac',
    opus: 'audio/ogg',
    mid: 'audio/midi',
    midi: 'audio/midi',
    webm: 'audio/webm',
    png: 'image/png',
    jpg: 'image/jpeg',
    jpeg: 'image/jpeg',
    gif: 'image/gif',
    webp: 'image/webp',
    json: 'application/json'
};

function fail(status, message) {
    http_reply.status = status;
    http_reply.header['Content-Type'] = 'text/plain; charset=utf-8';
    write(message + '\r\n');
    exit();
}

function ensureTrailingSlash(path) {
    if (path.charAt(path.length - 1) !== '/' && path.charAt(path.length - 1) !== '\\') {
        return path + '/';
    }
    return path;
}

function stripTrailingSlashes(path) {
    return String(path || '').replace(/[\/\\]+$/, '');
}

function sanitizePath(path) {
    var clean = String(path || '').replace(/\\/g, '/');
    if (!clean || clean.charAt(0) === '/') return null;
    if (clean.indexOf('..') !== -1) return null;
    if (/\/\//.test(clean)) return null;
    if (!/^[A-Za-z0-9._\-\/ ]+$/.test(clean)) return null;
    return clean;
}

function isAbsolutePath(path) {
    var value = String(path || '');
    return /^[A-Za-z]:[\\/]/.test(value) || /^[\\/]/.test(value);
}

function resolveStartupDir(path) {
    var value = String(path || '');
    if (!value) return '';
    if (isAbsolutePath(value)) {
        return ensureTrailingSlash(fullpath(backslash(value)));
    }
    return ensureTrailingSlash(fullpath(backslash(system.exec_dir + value)));
}

function getBaseDir(scope, code) {
    var prog;
    var key;
    var lookupCode;
    if (scope === 'shared') {
        return SHARED_ROOT;
    }
    if (scope === 'xtrn') {
        if (!code) return null;
        lookupCode = String(code);
        prog = xtrn_area.prog[lookupCode] || xtrn_area.prog[lookupCode.toLowerCase()] || xtrn_area.prog[lookupCode.toUpperCase()];
        if (!prog) {
            for (key in xtrn_area.prog) {
                if (!xtrn_area.prog.hasOwnProperty(key)) continue;
                if (!xtrn_area.prog[key] || !xtrn_area.prog[key].code) continue;
                if (String(xtrn_area.prog[key].code).toLowerCase() === lookupCode.toLowerCase()) {
                    prog = xtrn_area.prog[key];
                    break;
                }
            }
        }
        if (!prog || !prog.startup_dir) return null;
        return resolveStartupDir(prog.startup_dir + '/assets/web/');
    }
    return null;
}

function extname(path) {
    var idx = path.lastIndexOf('.');
    if (idx === -1) return '';
    return path.slice(idx + 1).toLowerCase();
}

if (user.number < 1) fail('403 Forbidden', 'Authentication required');
if (!http_request.query.scope || !http_request.query.path) fail('400 Bad Request', 'Missing scope/path');

var scope = String(http_request.query.scope[0] || '');
var relPath = sanitizePath(http_request.query.path[0]);
var code = http_request.query.code ? String(http_request.query.code[0] || '') : '';
var baseDir = getBaseDir(scope, code);
var targetPath;
var ext;
var mime;
var f;
var rangeHeader;
var rangeMatch;
var startByte = 0;
var endByte = -1;
var totalSize;
var chunkStart;
var chunkEnd;

if (!relPath) fail('400 Bad Request', 'Invalid path');
if (!baseDir) fail('404 Not Found', 'Invalid asset scope');

baseDir = ensureTrailingSlash(baseDir);
targetPath = stripTrailingSlashes(baseDir + relPath);

if (targetPath.indexOf(baseDir) !== 0) fail('403 Forbidden', 'Path denied');
if (!file_exists(targetPath)) fail('404 Not Found', 'Asset not found');

ext = extname(targetPath);
mime = ALLOWED_EXT[ext];
if (!mime) fail('403 Forbidden', 'Extension not allowed');

http_reply.header['Content-Type'] = mime;
http_reply.header['Content-Disposition'] = 'inline';
http_reply.header['Accept-Ranges'] = 'bytes';
http_reply.header['Cache-Control'] = 'private, max-age=3600';

f = new File(targetPath);
if (!f.open('rb')) fail('500 Internal Server Error', 'Unable to open asset');

totalSize = f.length;
rangeHeader = http_request.header['range'] || http_request.header['Range'] || '';

if (rangeHeader) {
    rangeMatch = /^bytes=(\d*)-(\d*)$/i.exec(String(rangeHeader));
    if (!rangeMatch) {
        f.close();
        fail('416 Requested Range Not Satisfiable', 'Invalid range');
    }
    if (rangeMatch[1] === '' && rangeMatch[2] === '') {
        f.close();
        fail('416 Requested Range Not Satisfiable', 'Invalid range');
    }

    if (rangeMatch[1] !== '') {
        startByte = parseInt(rangeMatch[1], 10);
        endByte = rangeMatch[2] !== '' ? parseInt(rangeMatch[2], 10) : (totalSize - 1);
    } else {
        endByte = totalSize - 1;
        startByte = totalSize - parseInt(rangeMatch[2], 10);
    }

    if (isNaN(startByte) || isNaN(endByte) || startByte < 0 || endByte < startByte || startByte >= totalSize) {
        f.close();
        fail('416 Requested Range Not Satisfiable', 'Invalid range');
    }
    if (endByte >= totalSize) endByte = totalSize - 1;

    http_reply.status = '206 Partial Content';
    http_reply.header['Content-Range'] = 'bytes ' + startByte + '-' + endByte + '/' + totalSize;
    http_reply.header['Content-Length'] = (endByte - startByte + 1);
    f.position = startByte;
    chunkStart = startByte;
    chunkEnd = endByte;
} else {
    http_reply.header['Content-Length'] = totalSize;
    chunkStart = 0;
    chunkEnd = totalSize - 1;
}

for (var n = chunkStart; n <= chunkEnd; n += CHUNK_SIZE) {
    var remaining = (chunkEnd + 1) - f.position;
    write(f.read(remaining > CHUNK_SIZE ? CHUNK_SIZE : remaining));
    yield(false);
}

f.close();
