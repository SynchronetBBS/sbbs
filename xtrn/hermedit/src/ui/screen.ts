/**
 * Diff-based screen renderer. A frame is composed into cell buffers
 * (CP437 code + attribute per cell), then flush() emits only the runs that
 * changed since the previous flush as ONE batched raw write:
 * ANSI cursor positioning + SGR + glyph bytes. No per-cell gotoxy calls, no
 * hotspot registration — coordinate hit-testing lives in widgets.ts.
 *
 * The writer callback is injected so tests can capture output.
 */

import { ansiFromAttr } from '../core/attr';
import { displayChar } from '../core/cp437';

export class Screen {
  cols: number;
  rows: number;
  utf8: boolean;
  private ch: number[] = [];
  private attr: number[] = [];
  private prevCh: number[] = [];
  private prevAttr: number[] = [];
  private prevValid = false;
  /** 1-based hardware cursor position for after the flush. */
  cursorX = 1;
  cursorY = 1;
  cursorVisible = true;
  private writer: (s: string) => void;

  constructor(cols: number, rows: number, utf8: boolean, writer: (s: string) => void) {
    this.cols = cols;
    this.rows = rows;
    this.utf8 = utf8;
    this.writer = writer;
    for (var i = 0; i < cols * rows; i++) {
      this.ch.push(0x20);
      this.attr.push(0x07);
      this.prevCh.push(0x20);
      this.prevAttr.push(0x07);
    }
  }

  /** x, y are 0-based. */
  put(x: number, y: number, code: number, attr: number): void {
    if (x < 0 || y < 0 || x >= this.cols || y >= this.rows) return;
    var i = y * this.cols + x;
    this.ch[i] = code & 0xff;
    this.attr[i] = attr & 0xff;
  }

  /** ASCII/CP437 string (one byte per char). */
  putStr(x: number, y: number, s: string, attr: number): void {
    for (var i = 0; i < s.length; i++) this.put(x + i, y, s.charCodeAt(i), attr);
  }

  fill(x: number, y: number, w: number, h: number, code: number, attr: number): void {
    for (var yy = y; yy < y + h; yy++) {
      for (var xx = x; xx < x + w; xx++) this.put(xx, yy, code, attr);
    }
  }

  hline(x: number, y: number, w: number, code: number, attr: number): void {
    for (var i = 0; i < w; i++) this.put(x + i, y, code, attr);
  }

  /** Force the next flush to repaint everything (e.g. after a modal). */
  invalidate(): void {
    this.prevValid = false;
  }

  /** Adopt a new terminal size: fresh buffers, full repaint on next flush. */
  resize(cols: number, rows: number): void {
    this.cols = cols;
    this.rows = rows;
    this.ch = [];
    this.attr = [];
    this.prevCh = [];
    this.prevAttr = [];
    for (var i = 0; i < cols * rows; i++) {
      this.ch.push(0x20);
      this.attr.push(0x07);
      this.prevCh.push(0x20);
      this.prevAttr.push(0x07);
    }
    this.prevValid = false;
  }

  flush(): void {
    var out = '';
    var lastAttr = -1;
    for (var y = 0; y < this.rows; y++) {
      var x = 0;
      while (x < this.cols) {
        var i = y * this.cols + x;
        var changed = !this.prevValid ||
          this.ch[i] !== this.prevCh[i] || this.attr[i] !== this.prevAttr[i];
        if (!changed) {
          x++;
          continue;
        }
        // Start a run: position once, then emit until cells stop changing.
        out += '\x1b[' + (y + 1) + ';' + (x + 1) + 'H';
        // Never let a run print into the terminal's last column of the last
        // row minus... (81st col wrap is prevented by layout: chrome uses all
        // columns but the final row/col cell is written last).
        while (x < this.cols) {
          var j = y * this.cols + x;
          // Never write the terminal's bottom-right cell: on many terminals
          // that triggers an automatic scroll of the whole screen.
          if (y === this.rows - 1 && x === this.cols - 1) {
            this.prevCh[j] = this.ch[j] as number;
            this.prevAttr[j] = this.attr[j] as number;
            x++;
            break;
          }
          var cChanged = !this.prevValid ||
            this.ch[j] !== this.prevCh[j] || this.attr[j] !== this.prevAttr[j];
          if (!cChanged) break;
          var a = this.attr[j] as number;
          if (a !== lastAttr) {
            out += ansiFromAttr(a);
            lastAttr = a;
          }
          out += displayChar(this.ch[j] as number, this.utf8);
          this.prevCh[j] = this.ch[j] as number;
          this.prevAttr[j] = a;
          x++;
        }
      }
    }
    out += '\x1b[' + this.cursorY + ';' + this.cursorX + 'H';
    out += this.cursorVisible ? '\x1b[?25h' : '\x1b[?25l';
    this.prevValid = true;
    if (out.length > 0) this.writer(out);
  }
}
