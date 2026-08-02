/**
 * Pure geometry for the Draw-mode shape tools. Every function returns plain
 * coordinate/cell lists — no Synchronet globals, no document mutation — so the
 * controller decides glyph/attr and applies them, and vitest can pin the math.
 */

export interface Point {
  x: number;
  y: number;
}

export interface PlotCell {
  x: number;
  y: number;
  ch: number;
}

/** CP437 single- and double-line box-drawing sets. */
export var BOX_SINGLE = { tl: 0xda, tr: 0xbf, bl: 0xc0, br: 0xd9, h: 0xc4, v: 0xb3 };
export var BOX_DOUBLE = { tl: 0xc9, tr: 0xbb, bl: 0xc8, br: 0xbc, h: 0xcd, v: 0xba };

/** Bresenham line from (x0,y0) to (x1,y1), inclusive of both ends. */
export function linePoints(x0: number, y0: number, x1: number, y1: number): Point[] {
  var pts: Point[] = [];
  var dx = Math.abs(x1 - x0);
  var dy = Math.abs(y1 - y0);
  var sx = x0 < x1 ? 1 : -1;
  var sy = y0 < y1 ? 1 : -1;
  var err = dx - dy;
  var x = x0;
  var y = y0;
  for (;;) {
    pts.push({ x: x, y: y });
    if (x === x1 && y === y1) break;
    var e2 = 2 * err;
    if (e2 > -dy) { err -= dy; x += sx; }
    if (e2 < dx) { err += dx; y += sy; }
  }
  return pts;
}

/**
 * The glyph a straight line should use: horizontal and vertical runs get the
 * proper CP437 box-drawing edge; anything diagonal falls back to `fallback`
 * (the current brush glyph).
 */
export function lineGlyph(x0: number, y0: number, x1: number, y1: number, fallback: number, dbl?: boolean): number {
  var g = dbl ? BOX_DOUBLE : BOX_SINGLE;
  if (y0 === y1 && x0 !== x1) return g.h;
  if (x0 === x1 && y0 !== y1) return g.v;
  return fallback;
}

/**
 * Border cells of the rectangle spanned by the two corners (given in any
 * order), drawn with CP437 box-drawing glyphs (corners, edges). Degenerate
 * rectangles collapse to a line or a single cell.
 */
export function boxCells(ax: number, ay: number, bx: number, by: number, dbl?: boolean): PlotCell[] {
  var x0 = Math.min(ax, bx);
  var x1 = Math.max(ax, bx);
  var y0 = Math.min(ay, by);
  var y1 = Math.max(ay, by);
  var g = dbl ? BOX_DOUBLE : BOX_SINGLE;
  var cells: PlotCell[] = [];
  if (x0 === x1 && y0 === y1) { cells.push({ x: x0, y: y0, ch: g.v }); return cells; }
  if (y0 === y1) { for (var xh = x0; xh <= x1; xh++) cells.push({ x: xh, y: y0, ch: g.h }); return cells; }
  if (x0 === x1) { for (var yv = y0; yv <= y1; yv++) cells.push({ x: x0, y: yv, ch: g.v }); return cells; }
  cells.push({ x: x0, y: y0, ch: g.tl });
  cells.push({ x: x1, y: y0, ch: g.tr });
  cells.push({ x: x0, y: y1, ch: g.bl });
  cells.push({ x: x1, y: y1, ch: g.br });
  for (var x = x0 + 1; x < x1; x++) {
    cells.push({ x: x, y: y0, ch: g.h });
    cells.push({ x: x, y: y1, ch: g.h });
  }
  for (var y = y0 + 1; y < y1; y++) {
    cells.push({ x: x0, y: y, ch: g.v });
    cells.push({ x: x1, y: y, ch: g.v });
  }
  return cells;
}

/** CP437 half-block glyphs. */
export var HALF = { top: 0xdf, bottom: 0xdc, left: 0xdd, right: 0xde, full: 0xdb };

/**
 * Line drawn at half-cell vertical resolution: Bresenham runs on a grid with
 * two subpixels per cell row, and each touched cell renders ▀ / ▄ / █ for
 * top / bottom / both halves. Endpoints anchor to the top half of their cell.
 */
export function halfBlockLineCells(x0: number, y0: number, x1: number, y1: number): PlotCell[] {
  var pts = linePoints(x0, y0 * 2, x1, y1 * 2);
  var halves: { [key: string]: { top: boolean; bottom: boolean } } = {};
  var order: string[] = [];
  for (var i = 0; i < pts.length; i++) {
    var p = pts[i] as Point;
    var cy = p.y >> 1;
    var key = p.x + ',' + cy;
    if (halves[key] === undefined) {
      halves[key] = { top: false, bottom: false };
      order.push(key);
    }
    if ((p.y & 1) === 0) (halves[key] as { top: boolean }).top = true;
    else (halves[key] as { bottom: boolean }).bottom = true;
  }
  var out: PlotCell[] = [];
  for (var k = 0; k < order.length; k++) {
    var kk = order[k] as string;
    var parts = kk.split(',');
    var h = halves[kk] as { top: boolean; bottom: boolean };
    var ch = h.top && h.bottom ? HALF.full : (h.top ? HALF.top : HALF.bottom);
    out.push({ x: parseInt(parts[0] as string, 10), y: parseInt(parts[1] as string, 10), ch: ch });
  }
  return out;
}

/**
 * Rectangle border drawn with half blocks for a thin look: ▀ along the top
 * (corners included), ▄ along the bottom, ▌/▐ on the sides. Degenerate
 * rectangles collapse to a half-block row / column / single cell.
 */
export function halfBlockBoxCells(ax: number, ay: number, bx: number, by: number): PlotCell[] {
  var x0 = Math.min(ax, bx);
  var x1 = Math.max(ax, bx);
  var y0 = Math.min(ay, by);
  var y1 = Math.max(ay, by);
  var cells: PlotCell[] = [];
  if (y0 === y1) { for (var xh = x0; xh <= x1; xh++) cells.push({ x: xh, y: y0, ch: HALF.top }); return cells; }
  if (x0 === x1) { for (var yv = y0; yv <= y1; yv++) cells.push({ x: x0, y: yv, ch: HALF.left }); return cells; }
  for (var x = x0; x <= x1; x++) {
    cells.push({ x: x, y: y0, ch: HALF.top });
    cells.push({ x: x, y: y1, ch: HALF.bottom });
  }
  for (var y = y0 + 1; y < y1; y++) {
    cells.push({ x: x0, y: y, ch: HALF.left });
    cells.push({ x: x1, y: y, ch: HALF.right });
  }
  return cells;
}

/**
 * Every cell inside the ellipse inscribed in the two corners' bounding box
 * (outline cells included — draw the border after the fill so it wins).
 */
export function ellipseFillPoints(ax: number, ay: number, bx: number, by: number): Point[] {
  var x0 = Math.min(ax, bx);
  var x1 = Math.max(ax, bx);
  var y0 = Math.min(ay, by);
  var y1 = Math.max(ay, by);
  var rx = (x1 - x0) / 2;
  var ry = (y1 - y0) / 2;
  var cx = (x0 + x1) / 2;
  var cy = (y0 + y1) / 2;
  var pts: Point[] = [];
  if (rx === 0 || ry === 0) return pts;
  for (var y = y0; y <= y1; y++) {
    for (var x = x0; x <= x1; x++) {
      var nx = (x - cx) / rx;
      var ny = (y - cy) / ry;
      if (nx * nx + ny * ny <= 1) pts.push({ x: x, y: y });
    }
  }
  return pts;
}

/** Every cell inside (and on) the rectangle spanned by the two corners. */
export function rectFillPoints(ax: number, ay: number, bx: number, by: number): Point[] {
  var x0 = Math.min(ax, bx);
  var x1 = Math.max(ax, bx);
  var y0 = Math.min(ay, by);
  var y1 = Math.max(ay, by);
  var pts: Point[] = [];
  for (var y = y0; y <= y1; y++) {
    for (var x = x0; x <= x1; x++) pts.push({ x: x, y: y });
  }
  return pts;
}

/**
 * Ellipse outline inscribed in the bounding box of the two corners. Sampled
 * parametrically and de-duplicated — precise enough for chunky textmode art
 * and free of the fractional-center headaches of integer midpoint on an even
 * bounding box. A zero-size box yields a single point.
 */
export function ellipsePoints(ax: number, ay: number, bx: number, by: number): Point[] {
  var x0 = Math.min(ax, bx);
  var x1 = Math.max(ax, bx);
  var y0 = Math.min(ay, by);
  var y1 = Math.max(ay, by);
  var rx = (x1 - x0) / 2;
  var ry = (y1 - y0) / 2;
  var cx = (x0 + x1) / 2;
  var cy = (y0 + y1) / 2;
  if (rx === 0 && ry === 0) return [{ x: x0, y: y0 }];
  var seen: { [key: string]: boolean } = {};
  var pts: Point[] = [];
  var steps = Math.max(8, Math.round((rx + ry) * 4));
  for (var i = 0; i < steps; i++) {
    var t = (i / steps) * Math.PI * 2;
    var x = Math.round(cx + rx * Math.cos(t));
    var y = Math.round(cy + ry * Math.sin(t));
    var key = x + ',' + y;
    if (!seen[key]) {
      seen[key] = true;
      pts.push({ x: x, y: y });
    }
  }
  return pts;
}

export interface ShapeMouse {
  /** 0 left, 1 middle, 2 right. */
  button: number;
  press: boolean;
  release: boolean;
  motion: boolean;
}

export type ShapeAction = 'none' | 'set-anchor' | 'preview' | 'commit' | 'cancel' | 'eyedrop';

/**
 * Decide what a two-point tool (line/box/circle) should do for one mouse
 * event, given whether a start point is already set and whether the event is
 * on that same start cell.
 *
 * The subtlety this encodes: SGR drag (motion) reports arrive with
 * `press === true` — they end in 'M' like a button-down — so a real
 * button-down must be `press && !motion`, and motion must be handled before
 * the button-down branch. Otherwise every drag tick reads as a fresh press
 * and resets the anchor to the cursor, committing a zero-size shape.
 *
 * Works for both drag-release and click-move-click, so terminals without
 * reliable button-up reports still commit (on the second click).
 */
export function shapeToolAction(ev: ShapeMouse, hasAnchor: boolean, atAnchorCell: boolean): ShapeAction {
  var down = ev.press && !ev.motion;
  if (down && ev.button === 1) return 'eyedrop';
  if (down && ev.button === 2) return 'cancel';
  if (ev.release) return hasAnchor && !atAnchorCell ? 'commit' : 'none';
  if (ev.motion) return hasAnchor ? 'preview' : 'none';
  if (down && ev.button === 0) return hasAnchor ? 'commit' : 'set-anchor';
  return 'none';
}

/**
 * 4-way flood fill from (startX, startY), bounded to [0,width) x [0,height).
 * `sample(x, y)` returns an identity string for a cell; contiguous cells whose
 * identity equals the start cell's are returned. `maxCells` (default 4000)
 * caps runaway fills of a large open area.
 */
export function floodFill(
  startX: number,
  startY: number,
  width: number,
  height: number,
  sample: (x: number, y: number) => string,
  maxCells?: number
): Point[] {
  var cap = maxCells === undefined ? 4000 : maxCells;
  if (startX < 0 || startY < 0 || startX >= width || startY >= height) return [];
  var target = sample(startX, startY);
  var out: Point[] = [];
  var seen: { [key: string]: boolean } = {};
  var stack: Point[] = [{ x: startX, y: startY }];
  while (stack.length > 0 && out.length < cap) {
    var p = stack.pop() as Point;
    if (p.x < 0 || p.y < 0 || p.x >= width || p.y >= height) continue;
    var k = p.x + ',' + p.y;
    if (seen[k]) continue;
    seen[k] = true;
    if (sample(p.x, p.y) !== target) continue;
    out.push({ x: p.x, y: p.y });
    stack.push({ x: p.x + 1, y: p.y });
    stack.push({ x: p.x - 1, y: p.y });
    stack.push({ x: p.x, y: p.y + 1 });
    stack.push({ x: p.x, y: p.y - 1 });
  }
  return out;
}
