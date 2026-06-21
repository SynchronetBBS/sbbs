// SyncTERM bitmap-graphics capability probe -- groundwork for v6 picture support.
// Run on a node from the terminal:  ;exec ?/share/sbbs/xtrn/zmachine/test/gfx_probe.js
// Reports the terminal's graphics capabilities and, if supported, uploads + draws a
// 128x128 test PPM via the SyncTERM APC protocol (cache-store -> LoadPPM -> Paste).
load("sbbsdefs.js");
var cterm = load({}, "cterm_lib.js");   // sets console.cterm_version; provides query_ctda + constants

console.clear();
console.print("\x01n\x01h\x01wSyncTERM graphics capability probe\x01n\r\n\r\n");
console.print("  cterm_version        : \x01h" + console.cterm_version + "\x01n\r\n");

var pixops = false, jxl = false;
if (console.cterm_version >= cterm.cterm_version_supports_copy_buffers)
    pixops = cterm.query_ctda(cterm.cterm_device_attributes.pixelops_supported);
if (typeof cterm.supports_jpegxl == "function") jxl = cterm.supports_jpegxl();

console.print("  pixel graphics (PPM) : " + (pixops ? "\x01h\x01gYES" : "\x01h\x01rno") + "\x01n\r\n");
console.print("  JPEG-XL              : " + (jxl ? "\x01h\x01gYES" : "\x01h\x01yno") + "\x01n\r\n\r\n");

if (pixops) {
    var f = new File(js.exec_dir + "test.ppm");
    if (!f.open("rb")) console.print("\x01h\x01rcannot open test.ppm\x01n\r\n");
    else {
        var ck = console.ctrlkey_passthru;
        console.ctrlkey_passthru = true;
        console.write("\x1b_SyncTERM:C;S;zmachine/test.ppm;");   // upload to the client's file cache (base64)
        f.base64 = true;
        console.write(f.read());
        console.write("\x1b\\");
        f.close();
        console.write("\x1b_SyncTERM:C;LoadPPM;B=0;zmachine/test.ppm\x1b\\");              // decode into draw buffer 0
        console.write("\x1b_SyncTERM:P;Paste;SX=0;SY=0;SW=128;SH=128;DX=248;DY=48;B=0\x1b\\"); // blit at pixel (248,48)
        console.ctrlkey_passthru = ck;
        console.gotoxy(1, 20);
        console.print("\x01h\x01gA 128x128 gradient (white border) should have appeared above.\x01n\r\n");
    }
} else {
    console.print("\x01h\x01y[text fallback] No pixel graphics on this terminal -- a v6 game would\r\n");
    console.print("render its text windows on the character grid with [picture] placeholders.\x01n\r\n");
}
console.crlf();
console.pause();
