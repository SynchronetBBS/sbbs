/**
 * The narrow host boundary from DESIGN.md: everything above this file is
 * host-agnostic and testable; only src/host/sbbs.ts touches Synchronet
 * globals.
 */

import { SessionMeta } from '../core/dropfiles';
import { TdfFont } from '../core/tdf';

/** One entry in the bundled TheDraw font index. */
export interface FontMeta {
  name: string;
  /** Rendered height in rows (the size axis for filtering). */
  height: number;
  /** 'Outline' | 'Block' | 'Color'. */
  type: string;
}

/**
 * Access to the bundled TheDraw fonts. `list()` is cheap (reads the prebuilt
 * index); `load()` parses one font file on demand. Injected so the UI stays
 * host-agnostic and testable.
 */
export interface FontProvider {
  list(): FontMeta[];
  load(name: string): TdfFont | null;
}

export interface TerminalCaps {
  cols: number;
  rows: number;
  /** Terminal (and session) is UTF-8; document stays CP437 internally. */
  utf8: boolean;
  /** Mouse reporting was successfully enabled. */
  mouse: boolean;
}

export interface KeyEvent {
  type: 'key';
  /** Single char (possibly a Synchronet KEY_* control char), 'F1'..'F12',
   * or a CSI-u modified key: 'C-,' / 'C-.' / 'C-/'. */
  key: string;
}

export interface MouseEvent {
  type: 'mouse';
  /** 1-based terminal coordinates. */
  x: number;
  y: number;
  /** 0=left 1=middle 2=right; 64/65 are reported as wheel instead. */
  button: number;
  press: boolean;
  release: boolean;
  /** True while dragging with a button held. */
  motion: boolean;
  /** -1 wheel up, +1 wheel down, 0 none. */
  wheel: number;
}

export interface NoEvent {
  type: 'none';
}

export type InputEvent = KeyEvent | MouseEvent | NoEvent;

export interface MessageSession {
  /** Where the flattened message body must be written (%f / argv[0]). */
  bodyPath: string;
  meta: SessionMeta;
  /** Existing body text (CP437, LF-normalized) when re-editing; '' if none. */
  sourceText: string;
  /** Quote lines (CP437, no line endings) when replying; empty otherwise. */
  quoteLines: string[];
  /** Message transport is UTF-8 (from MSGINF or terminal capability). */
  utf8: boolean;
}
