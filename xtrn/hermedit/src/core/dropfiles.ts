/**
 * Pure parsers/builders for Synchronet's editor drop files. File I/O happens
 * in the host adapter; these functions see only line arrays, so vitest can
 * pin the exact formats from SYNCHRONET_CONTRACT.md.
 */

export interface SessionMeta {
  from: string;
  to: string;
  subject: string;
  /** Message area name, 'NetMail', or 'Electronic Mail' (MSGINF only). */
  area: string;
  privateMsg: boolean;
  /** 'CP437' or 'UTF-8'. EDITOR.INF does not declare one -> ''. */
  charset: string;
  /** Which drop file supplied the metadata. */
  source: 'msginf' | 'editor.inf' | 'none';
}

export function emptyMeta(): SessionMeta {
  return { from: '', to: '', subject: '', area: '', privateMsg: false, charset: '', source: 'none' };
}

/**
 * WWIV-style EDITOR.INF: six CRLF lines —
 * subject / to / user number / from (or Anonymous) / alias / security level.
 */
export function parseEditorInf(lines: string[]): SessionMeta {
  var m = emptyMeta();
  m.source = 'editor.inf';
  m.subject = lines.length > 0 ? String(lines[0]) : '';
  m.to = lines.length > 1 ? String(lines[1]) : '';
  m.from = lines.length > 3 ? String(lines[3]) : '';
  return m;
}

/**
 * QuickBBS-style MSGINF: eight CRLF lines —
 * from / to / subject / msg number / area / private YES|NO /
 * tagline file (Synchronet extension) / charset (UTF-8 or CP437).
 */
export function parseMsgInf(lines: string[]): SessionMeta {
  var m = emptyMeta();
  m.source = 'msginf';
  m.from = lines.length > 0 ? String(lines[0]) : '';
  m.to = lines.length > 1 ? String(lines[1]) : '';
  m.subject = lines.length > 2 ? String(lines[2]) : '';
  m.area = lines.length > 4 ? String(lines[4]) : '';
  m.privateMsg = lines.length > 5 && String(lines[5]).toUpperCase() === 'YES';
  if (lines.length > 7) {
    var cs = String(lines[7]).toUpperCase();
    if (cs === 'UTF-8' || cs === 'CP437') m.charset = cs;
  }
  return m;
}

/**
 * RESULT.ED contents for a successful save: status placeholder, edited
 * subject, editor identification. CRLF endings, written by the host adapter.
 */
export function buildResultEd(subject: string, editorIdent: string): string {
  return '0\r\n' + subject + '\r\n' + editorIdent + '\r\n';
}
