// viewport.js — v6 scaled-viewport geometry. Pure (no I/O); unit-tested in test/viewport.js.
// ES5 / SpiderMonkey 1.8.5.

// JSZM_v6Viewport(termPx, termCols, fontW, termRows, fontH, nativeW, nativeH)
//   -> { scale, widthPx, cols, offsetPx, offsetCols }
// Arthur is authored for a fixed ~640x400 screen (2x the 320x200 native art) and lays out its windows
// in that space REGARDLESS of the reported screen size -- proven live: at a reported 320x200 (scale 1)
// it still set window-0 to 612px wide at y=201. So a scaled-DOWN screen (scale 1, 40 cols) doesn't make
// Arthur lay out smaller; it just garbles, since Arthur's 640-space coords no longer match the door's
// frame. We therefore pick S purely from the WIDTH (largest integer S<=2 that fits): S=2 on a standard
// 80-col/640px terminal -> Arthur's full authored frame. We do NOT step S down for a short terminal
// (e.g. SyncTERM 80x43 = 336px canvas): keep S=2 and let the bottom few text rows clip (handled by the
// blit clip), which is far better than the broken scale-1 layout. termRows/fontH are unused now (kept
// for signature stability); the height is reported by the caller as the real terminal canvas.
function JSZM_v6Viewport(termPx, termCols, fontW, termRows, fontH, nativeW, nativeH) {
  var fw = fontW > 0 ? fontW : 1;
  var nw = nativeW > 0 ? nativeW : 320;
  var s = Math.floor(termPx / nw);
  if (s < 1) s = 1; if (s > 2) s = 2;                      // cap at 2x (pixel art); width-driven only
  var widthPx = s * nw; if (widthPx > termPx) widthPx = termPx;
  var cols = Math.floor(widthPx / fw); if (cols > termCols) cols = termCols; if (cols < 1) cols = 1;
  var offsetCols = Math.floor((termCols - cols) / 2); if (offsetCols < 0) offsetCols = 0;
  return { scale: s, widthPx: widthPx, cols: cols, offsetPx: offsetCols * fw, offsetCols: offsetCols };
}
