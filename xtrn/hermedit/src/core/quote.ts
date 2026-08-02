/**
 * Quote pre-formatting for BBS message conventions. Pure — the controller
 * feeds it the source lines and an author, and inserts the result.
 *
 * Styles:
 *   'gt'       - classic `> ` prefix; already-quoted lines nest tightly (`>>`).
 *   'initials' - Synchronet/FidoNet style ` AB> ` using the author's initials.
 *   'none'     - raw text, no prefix (a plain insert).
 * An optional attribution header ("<author> wrote:") can lead the block.
 */

export type QuoteStyle = 'gt' | 'initials' | 'none';

/** Up to three uppercase initials from a name ("Alice B Carol" -> "ABC"). */
export function authorInitials(name: string): string {
  var parts = String(name || '').replace(/^\s+|\s+$/g, '').split(/\s+/);
  var out = '';
  for (var i = 0; i < parts.length && out.length < 3; i++) {
    if ((parts[i] as string).length > 0) out += (parts[i] as string).charAt(0).toUpperCase();
  }
  return out.length > 0 ? out : '?';
}

/** The per-line prefix for a style. */
export function quotePrefix(style: QuoteStyle, author: string): string {
  if (style === 'gt') return '> ';
  if (style === 'initials') return ' ' + authorInitials(author) + '> ';
  return '';
}

/**
 * Format source lines as a quote block. `attribution` prepends a
 * "<author> wrote:" header when the author is known.
 */
export function formatQuote(lines: string[], author: string, style: QuoteStyle, attribution: boolean): string[] {
  var out: string[] = [];
  if (attribution && author && author.length > 0) out.push(author + ' wrote:');
  var prefix = quotePrefix(style, author);
  for (var i = 0; i < lines.length; i++) {
    var line = lines[i] as string;
    // Nest an existing '>' quote tightly rather than "> > ".
    if (style === 'gt' && line.charAt(0) === '>') out.push('>' + line);
    else out.push(prefix + line);
  }
  return out;
}
