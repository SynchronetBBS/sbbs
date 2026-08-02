/**
 * HERMedIT — Synchronet external message editor entry point.
 *
 * Registered in ctrl/xtrn.ini as:
 *   cmd=?/sbbs/xtrn/future_edit/HERMedIT.js %f
 *
 * Lifecycle (SYNCHRONET_CONTRACT.md): read drop files and %f, run the
 * editor, then either write the body + RESULT.ED and exit 0, or exit 1
 * without presenting a changed body as success. Terminal state is restored
 * on save, abort, exception, and disconnect.
 */

import { loadSession, saveMessage, initTerminal, restoreTerminal, terminalSize, createSbbsFontProvider, EDITOR_IDENT } from './host/sbbs';
import { readInput } from './host/input';
import { Screen } from './ui/screen';
import { Controller } from './ui/controller';

load('sbbsdefs.js');

function isAlive(): boolean {
  return Boolean(bbs.online) && !js.terminated;
}

function main(): number {
  if (typeof bbs === 'undefined' || typeof console === 'undefined') {
    // Not running under the terminal server (e.g. jsexec): nothing to edit.
    return 1;
  }

  var caps = initTerminal();
  // Log the build and the size Synchronet reports for this session — the
  // two facts needed to diagnose "which editor am I running and why does
  // the layout look like this".
  log(LOG_INFO, EDITOR_IDENT + ' build ' + (typeof BUILD_STAMP === 'string' ? BUILD_STAMP : 'dev')
    + ' starting; terminal ' + caps.cols + 'x' + caps.rows
    + (caps.utf8 ? ' utf8' : ' cp437') + (caps.mouse ? ' mouse' : ''));
  var session = loadSession();
  var exitCode = 1;
  try {
    var scr = new Screen(caps.cols, caps.rows, caps.utf8, function (s) {
      console.write(s);
    });
    // Take over the screen: reset attributes, clear, hide wrap artifacts.
    console.write('\x1b[0m\x1b[2J\x1b[H');

    var controller = new Controller(session, caps, scr, readInput, isAlive, createSbbsFontProvider(), terminalSize);
    var result = controller.run();

    if (result.action === 'save') {
      if (saveMessage(session, result.bodyCp437, result.subject)) {
        exitCode = 0;
      } else {
        exitCode = 1;
      }
    } else {
      // Disconnect mid-edit: preserve the typed body for the host's
      // recovery handling, but still report failure (FSEditor precedent).
      if (!isAlive() && result.bodyCp437.length > 0) {
        saveMessage(session, result.bodyCp437, result.subject);
      }
      exitCode = 1;
    }
  } catch (e) {
    log(LOG_ERR, 'future_edit: ' + String(e));
    exitCode = 1;
  }
  restoreTerminal();
  console.write('\x1b[0m\x1b[2J\x1b[H');
  return exitCode;
}

var code = main();
log(LOG_INFO, EDITOR_IDENT + ' exiting with code ' + code);
exit(code);
