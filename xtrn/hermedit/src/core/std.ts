/**
 * Tiny ES5-safe helpers. The BBS runtime is SpiderMonkey 1.8.5: no
 * String.prototype.repeat, no Object.assign, no Array.prototype.find.
 */

export function repeatChar(ch: string, n: number): string {
  var s = '';
  for (var i = 0; i < n; i++) s += ch;
  return s;
}

export function padEnd(s: string, width: number, ch: string): string {
  while (s.length < width) s += ch;
  return s;
}

export function clamp(v: number, lo: number, hi: number): number {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

export function hasOwn(obj: object, key: string): boolean {
  return Object.prototype.hasOwnProperty.call(obj, key);
}

export function objectKeys(obj: { [key: string]: unknown }): string[] {
  var out: string[] = [];
  for (var k in obj) {
    if (hasOwn(obj, k)) out.push(k);
  }
  return out;
}

/** Last index in the string that is not a space or tab; -1 if all blank. */
export function lastNonBlank(s: string): number {
  for (var i = s.length - 1; i >= 0; i--) {
    var c = s.charAt(i);
    if (c !== ' ' && c !== '\t') return i;
  }
  return -1;
}
