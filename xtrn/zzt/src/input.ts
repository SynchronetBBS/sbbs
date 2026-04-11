namespace ZZT {
  var K_NOECHO: number = 1 << 17;
  var K_NOSPIN: number = 1 << 21;
  var K_EXTKEYS: number = 1 << 30;

  export var KEY_BACKSPACE: string = String.fromCharCode(8);
  export var KEY_TAB: string = String.fromCharCode(9);
  export var KEY_ENTER: string = String.fromCharCode(13);
  export var KEY_CTRL_Y: string = String.fromCharCode(25);
  export var KEY_ESCAPE: string = String.fromCharCode(27);
  export var KEY_ALT_P: string = String.fromCharCode(153);
  export var KEY_F1: string = String.fromCharCode(187);
  export var KEY_F2: string = String.fromCharCode(188);
  export var KEY_F3: string = String.fromCharCode(189);
  export var KEY_F4: string = String.fromCharCode(190);
  export var KEY_F5: string = String.fromCharCode(191);
  export var KEY_F6: string = String.fromCharCode(192);
  export var KEY_F7: string = String.fromCharCode(193);
  export var KEY_F8: string = String.fromCharCode(194);
  export var KEY_F9: string = String.fromCharCode(195);
  export var KEY_F10: string = String.fromCharCode(196);

  // Synchronet key_defs.js control-code values.
  export var KEY_UP: string = String.fromCharCode(30);
  export var KEY_PAGE_UP: string = String.fromCharCode(16);
  export var KEY_LEFT: string = String.fromCharCode(29);
  export var KEY_RIGHT: string = String.fromCharCode(6);
  export var KEY_DOWN: string = String.fromCharCode(10);
  export var KEY_PAGE_DOWN: string = String.fromCharCode(14);
  export var KEY_INSERT: string = String.fromCharCode(22);
  export var KEY_DELETE: string = String.fromCharCode(127);
  export var KEY_HOME: string = String.fromCharCode(2);
  export var KEY_END: string = String.fromCharCode(5);

  export var InputDeltaX: number = 0;
  export var InputDeltaY: number = 0;
  export var InputShiftPressed: boolean = false;
  export var InputShiftAccepted: boolean = false;
  export var InputJoystickEnabled: boolean = false;
  export var InputMouseEnabled: boolean = false;
  export var InputKeyPressed: string = String.fromCharCode(0);
  export var InputFirePressed: boolean = false;
  export var InputFireDirX: number = 0;
  export var InputFireDirY: number = 0;
  export var InputTorchPressed: boolean = false;
  export var InputMouseX: number = 0;
  export var InputMouseY: number = 0;
  export var InputMouseActivationX: number = 0;
  export var InputMouseActivationY: number = 0;
  export var InputMouseButtonX: number = 0;
  export var InputMouseButtonY: number = 0;
  export var InputJoystickMoved: boolean = false;
  export var JoystickXInitial: number = 0;
  export var JoystickYInitial: number = 0;
  export var InputLastDeltaX: number = 0;
  export var InputLastDeltaY: number = 0;
  var InputOldCtrlKeyPassthru: number | string | null = null;
  var InputRawBuffer: string = "";

  function normalizeRaw(raw: string | null | undefined): string {
    if (raw === null || typeof raw === "undefined") {
      return "";
    }
    return String(raw);
  }

  function inputReadMode(): number {
    return K_NOECHO | K_NOSPIN | K_EXTKEYS;
  }

  function decodeExtendedKey(raw: string): string {
    var nul: string = String.fromCharCode(0);
    var esc: string = KEY_ESCAPE;
    var second: string;
    var finalChar: string;
    var numberPart: string;
    var semicolonPos: number;
    var tildeCode: number;

    if (raw.length === 0) {
      return nul;
    }

    if (raw.length === 1) {
      return raw;
    }

    if (raw === esc + "[A") {
      return KEY_UP;
    }
    if (raw === esc + "[B") {
      return KEY_DOWN;
    }
    if (raw === esc + "[C") {
      return KEY_RIGHT;
    }
    if (raw === esc + "[D") {
      return KEY_LEFT;
    }
    if (raw === esc + "[H" || raw === esc + "[1~") {
      return KEY_HOME;
    }
    if (raw === esc + "[F" || raw === esc + "[4~") {
      return KEY_END;
    }
    if (raw === esc + "[2~") {
      return KEY_INSERT;
    }
    if (raw === esc + "[3~") {
      return KEY_DELETE;
    }
    if (raw === esc + "[5~") {
      return KEY_PAGE_UP;
    }
    if (raw === esc + "[6~") {
      return KEY_PAGE_DOWN;
    }
    if (raw === esc + "OA") {
      return KEY_UP;
    }
    if (raw === esc + "OB") {
      return KEY_DOWN;
    }
    if (raw === esc + "OC") {
      return KEY_RIGHT;
    }
    if (raw === esc + "OD") {
      return KEY_LEFT;
    }
    if (raw === esc + "OH") {
      return KEY_HOME;
    }
    if (raw === esc + "OF") {
      return KEY_END;
    }
    if (raw === esc + "[P" || raw === esc + "OP" || raw === esc + "[11~" || raw === esc + "[[A") {
      return KEY_F1;
    }
    if (raw === esc + "[Q" || raw === esc + "OQ" || raw === esc + "[12~" || raw === esc + "[[B") {
      return KEY_F2;
    }
    if (raw === esc + "[R" || raw === esc + "OR" || raw === esc + "[13~" || raw === esc + "[[C") {
      return KEY_F3;
    }
    if (raw === esc + "[S" || raw === esc + "OS" || raw === esc + "[14~" || raw === esc + "[[D") {
      return KEY_F4;
    }
    if (raw === esc + "[15~") {
      return KEY_F5;
    }
    if (raw === esc + "[17~") {
      return KEY_F6;
    }
    if (raw === esc + "[18~") {
      return KEY_F7;
    }
    if (raw === esc + "[19~") {
      return KEY_F8;
    }
    if (raw === esc + "[20~") {
      return KEY_F9;
    }
    if (raw === esc + "[21~") {
      return KEY_F10;
    }

    if (raw.length >= 3 && raw.charAt(0) === esc && raw.charAt(1) === "[") {
      finalChar = raw.charAt(raw.length - 1);
      if (finalChar === "A") {
        return KEY_UP;
      }
      if (finalChar === "B") {
        return KEY_DOWN;
      }
      if (finalChar === "C") {
        return KEY_RIGHT;
      }
      if (finalChar === "D") {
        return KEY_LEFT;
      }
      if (finalChar === "H") {
        return KEY_HOME;
      }
      if (finalChar === "F") {
        return KEY_END;
      }
      if (finalChar === "~") {
        numberPart = raw.slice(2, raw.length - 1);
        semicolonPos = numberPart.indexOf(";");
        if (semicolonPos >= 0) {
          numberPart = numberPart.slice(0, semicolonPos);
        }
        tildeCode = parseInt(numberPart, 10);
        if (tildeCode === 1) {
          return KEY_HOME;
        }
        if (tildeCode === 2) {
          return KEY_INSERT;
        }
        if (tildeCode === 3) {
          return KEY_DELETE;
        }
        if (tildeCode === 4) {
          return KEY_END;
        }
        if (tildeCode === 5) {
          return KEY_PAGE_UP;
        }
        if (tildeCode === 6) {
          return KEY_PAGE_DOWN;
        }
        if (tildeCode === 11) {
          return KEY_F1;
        }
        if (tildeCode === 12) {
          return KEY_F2;
        }
        if (tildeCode === 13) {
          return KEY_F3;
        }
        if (tildeCode === 14) {
          return KEY_F4;
        }
        if (tildeCode === 15) {
          return KEY_F5;
        }
        if (tildeCode === 17) {
          return KEY_F6;
        }
        if (tildeCode === 18) {
          return KEY_F7;
        }
        if (tildeCode === 19) {
          return KEY_F8;
        }
        if (tildeCode === 20) {
          return KEY_F9;
        }
        if (tildeCode === 21) {
          return KEY_F10;
        }
      }
    }

    if (raw.length >= 3 && raw.charAt(0) === esc && raw.charAt(1) === "O") {
      finalChar = raw.charAt(raw.length - 1);
      if (finalChar === "A") {
        return KEY_UP;
      }
      if (finalChar === "B") {
        return KEY_DOWN;
      }
      if (finalChar === "C") {
        return KEY_RIGHT;
      }
      if (finalChar === "D") {
        return KEY_LEFT;
      }
      if (finalChar === "H") {
        return KEY_HOME;
      }
      if (finalChar === "F") {
        return KEY_END;
      }
      if (finalChar === "P") {
        return KEY_F1;
      }
      if (finalChar === "Q") {
        return KEY_F2;
      }
      if (finalChar === "R") {
        return KEY_F3;
      }
      if (finalChar === "S") {
        return KEY_F4;
      }
    }

    // DOS/BIOS-style extended key returns (NUL + scan code)
    if (raw.charAt(0) === nul && raw.length >= 2) {
      second = raw.charAt(1);
      if ((second.charCodeAt(0) & 0xff) === 25) {
        return KEY_ALT_P;
      }
      if (second === "H") {
        return KEY_UP;
      }
      if (second === "P") {
        return KEY_DOWN;
      }
      if (second === "K") {
        return KEY_LEFT;
      }
      if (second === "M") {
        return KEY_RIGHT;
      }
      if (second === "G") {
        return KEY_HOME;
      }
      if (second === "O") {
        return KEY_END;
      }
      if (second === "R") {
        return KEY_INSERT;
      }
      if (second === "S") {
        return KEY_DELETE;
      }
      if (second === "I") {
        return KEY_PAGE_UP;
      }
      if (second === "Q") {
        return KEY_PAGE_DOWN;
      }
      if (second === ";") {
        return KEY_F1;
      }
      if (second === "<") {
        return KEY_F2;
      }
      if (second === "=") {
        return KEY_F3;
      }
      if (second === ">") {
        return KEY_F4;
      }
      if (second === "?") {
        return KEY_F5;
      }
      if (second === "@") {
        return KEY_F6;
      }
      if (second === "A") {
        return KEY_F7;
      }
      if (second === "B") {
        return KEY_F8;
      }
      if (second === "C") {
        return KEY_F9;
      }
      if (second === "D") {
        return KEY_F10;
      }
    }

    return raw.charAt(0);
  }

  function isAnsiTerminator(ch: string): boolean {
    return (ch >= "A" && ch <= "Z") || (ch >= "a" && ch <= "z") || ch === "~";
  }

  function consumeRawBuffer(forceEscape: boolean): string {
    var nul: string = String.fromCharCode(0);
    var esc: string = KEY_ESCAPE;
    var ch: string;
    var i: number;
    var sequence: string;

    if (InputRawBuffer.length <= 0) {
      return nul;
    }

    ch = InputRawBuffer.charAt(0);
    if (ch === String.fromCharCode(0)) {
      if (InputRawBuffer.length < 2) {
        return nul;
      }
      sequence = InputRawBuffer.slice(0, 2);
      InputRawBuffer = InputRawBuffer.slice(2);
      return decodeExtendedKey(sequence);
    }

    if (ch === esc) {
      if (InputRawBuffer.length === 1) {
        if (!forceEscape) {
          return nul;
        }
        InputRawBuffer = InputRawBuffer.slice(1);
        return esc;
      }

      if (InputRawBuffer.charAt(1) === "[" || InputRawBuffer.charAt(1) === "O") {
        i = 2;
        while (i < InputRawBuffer.length && i <= 12) {
          if (isAnsiTerminator(InputRawBuffer.charAt(i))) {
            sequence = InputRawBuffer.slice(0, i + 1);
            InputRawBuffer = InputRawBuffer.slice(i + 1);
            return decodeExtendedKey(sequence);
          }
          i += 1;
        }

        if (forceEscape) {
          InputRawBuffer = InputRawBuffer.slice(1);
          return esc;
        }
        return nul;
      }

      InputRawBuffer = InputRawBuffer.slice(1);
      return esc;
    }

    InputRawBuffer = InputRawBuffer.slice(1);
    return ch;
  }

  function readKeyFromConsole(waitTimeout: number): string {
    var raw: string;
    var extraRaw: string;
    var readCount: number;
    var key: string;
    var nul: string = String.fromCharCode(0);

    raw = "";
    if (typeof console.inkey === "function") {
      raw = normalizeRaw(console.inkey(inputReadMode(), waitTimeout));
    }

    if (raw.length > 0) {
      InputRawBuffer += raw;
    }

    if (typeof console.inkey === "function") {
      // Drain any immediately available bytes so ANSI/control sequences
      // and rapid key repeats are decoded from a single coherent buffer.
      readCount = 0;
      while (readCount < 32) {
        extraRaw = normalizeRaw(console.inkey(inputReadMode(), 0));
        if (extraRaw.length <= 0) {
          break;
        }
        InputRawBuffer += extraRaw;
        readCount += 1;
      }
    }

    key = consumeRawBuffer(false);
    if (key !== nul) {
      return key;
    }

    if (InputRawBuffer === KEY_ESCAPE && typeof console.inkey === "function") {
      raw = normalizeRaw(console.inkey(inputReadMode(), 5));
      if (raw.length > 0) {
        InputRawBuffer += raw;
      }
      key = consumeRawBuffer(true);
      if (key !== nul) {
        return key;
      }
    }

    if (waitTimeout > 0 && InputRawBuffer.length > 0) {
      key = consumeRawBuffer(true);
      if (key !== nul) {
        return key;
      }
    }

    return nul;
  }

  function normalizeKey(raw: string | null | undefined): string {
    return decodeExtendedKey(normalizeRaw(raw));
  }

  function hasInteractiveConsole(): boolean {
    return typeof console !== "undefined" && typeof console.inkey === "function";
  }

  function applyDirectionalInput(key: string): void {
    if (key === KEY_UP) {
      InputDeltaX = 0;
      InputDeltaY = -1;
    } else if (key === KEY_LEFT) {
      InputDeltaX = -1;
      InputDeltaY = 0;
    } else if (key === KEY_RIGHT) {
      InputDeltaX = 1;
      InputDeltaY = 0;
    } else if (key === KEY_DOWN) {
      InputDeltaX = 0;
      InputDeltaY = 1;
    }
  }

  function captureDirectionalFireInput(key: string): boolean {
    if (key === "8") {
      InputFireDirX = 0;
      InputFireDirY = -1;
      InputFirePressed = true;
      return true;
    }
    if (key === "4") {
      InputFireDirX = -1;
      InputFireDirY = 0;
      InputFirePressed = true;
      return true;
    }
    if (key === "6") {
      InputFireDirX = 1;
      InputFireDirY = 0;
      InputFirePressed = true;
      return true;
    }
    if (key === "2") {
      InputFireDirX = 0;
      InputFireDirY = 1;
      InputFirePressed = true;
      return true;
    }
    return false;
  }

  function resetTickInputState(): void {
    InputDeltaX = 0;
    InputDeltaY = 0;
    InputShiftPressed = false;
    InputJoystickMoved = false;
  }

  export function InputInitDevices(): void {
    if (typeof console !== "undefined") {
      if (InputOldCtrlKeyPassthru === null && typeof console.ctrlkey_passthru !== "undefined") {
        InputOldCtrlKeyPassthru = console.ctrlkey_passthru;
      }
      // Keep control keys flowing to the game while running under Synchronet.
      console.ctrlkey_passthru = "+ACGKLOPQRTUVWXYZ_";
    }

    InputJoystickEnabled = false;
    InputMouseEnabled = false;
    InputJoystickMoved = false;
    InputShiftPressed = false;
    InputShiftAccepted = false;
    InputDeltaX = 0;
    InputDeltaY = 0;
    InputLastDeltaX = 0;
    InputLastDeltaY = 0;
    InputKeyPressed = String.fromCharCode(0);
    InputFirePressed = false;
    InputFireDirX = 0;
    InputFireDirY = 0;
    InputTorchPressed = false;
    InputRawBuffer = "";
  }

  export function InputRestoreDevices(): void {
    if (typeof console !== "undefined" && InputOldCtrlKeyPassthru !== null) {
      console.ctrlkey_passthru = InputOldCtrlKeyPassthru;
      InputOldCtrlKeyPassthru = null;
    }
  }

  export function InputUpdate(): void {
    var key: string;

    SoundUpdate();
    resetTickInputState();
    key = String.fromCharCode(0);

    if (hasInteractiveConsole() && typeof console.inkey === "function") {
      key = readKeyFromConsole(0);
    }

    InputKeyPressed = key;
    if (key === " ") {
      InputFirePressed = true;
    }
    captureDirectionalFireInput(key);
    if (key.toUpperCase() === "T") {
      InputTorchPressed = true;
    }
    applyDirectionalInput(key);

    if (InputDeltaX !== 0 || InputDeltaY !== 0) {
      InputLastDeltaX = InputDeltaX;
      InputLastDeltaY = InputDeltaY;
    }
  }

  export function InputReadWaitKey(): void {
    var key: string = String.fromCharCode(0);

    resetTickInputState();
    InputShiftAccepted = false;

    if (hasInteractiveConsole()) {
      if (typeof console.getkey === "function") {
        key = normalizeKey(console.getkey(inputReadMode()));
      } else if (typeof console.inkey === "function") {
        while (key === String.fromCharCode(0) && !runtime.isTerminated()) {
          SoundUpdate();
          key = readKeyFromConsole(25);
          if (key === String.fromCharCode(0) && typeof mswait === "function") {
            mswait(10);
          }
        }
      }
    } else {
      // Non-Synchronet fallback: return ESC to avoid blocking in local tests.
      key = KEY_ESCAPE;
    }

    InputKeyPressed = key;
    if (key === " ") {
      InputFirePressed = true;
    }
    if (key.toUpperCase() === "T") {
      InputTorchPressed = true;
    }
    applyDirectionalInput(key);

    if (InputDeltaX !== 0 || InputDeltaY !== 0) {
      InputLastDeltaX = InputDeltaX;
      InputLastDeltaY = InputDeltaY;
    }
  }

  export function InputConfigure(): boolean {
    return true;
  }
}
