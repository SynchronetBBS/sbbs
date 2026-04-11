namespace ZZT {
  export var PORT_CGA_PALETTE: number = 0x03d9;
  export var VideoMonochrome: boolean = false;
  export var VideoColumns: number = 80;
  export var VideoRows: number = 25;
  export var VideoBorderColor: number = 1;
  export var VideoCursorVisible: boolean = true;
  var VideoShadowChars: string[][] = [];
  var VideoShadowAttrs: number[][] = [];

  function mapConsumedControlCode(code: number): number {
    /* Some CP437 control-range bytes are consumed by terminal parsers
       (BEL/BS/TAB/LF/FF/CR/ESC). Substitute visually-close printable
       bytes so gameplay UI remains stable in web and ANSI clients. */
    switch (code) {
      case 7:  return 249; /* bullet -> middle dot */
      case 8:  return 248; /* inverse bullet -> degree */
      case 9:  return 248; /* circle -> degree */
      case 10: return 248; /* dotted circle (door) -> degree */
      case 12: return 11;  /* female (key) -> male symbol */
      case 13: return 14;  /* single note -> double note */
      case 27: return 17;  /* left arrow -> left-pointing triangle */
      default: return code;
    }
  }

  function translateCp437Text(text: string): string {
    var i: number;
    var code: number;
    var translated: string = "";

    for (i = 0; i < text.length; i += 1) {
      code = text.charCodeAt(i);
      if (code > 255) {
        translated += text.charAt(i);
        continue;
      }
      translated += String.fromCharCode(mapConsumedControlCode(code & 0xff));
    }

    return translated;
  }

  function hasConsoleOutput(): boolean {
    return typeof console !== "undefined" && typeof console.gotoxy === "function";
  }

  function resetVideoShadow(): void {
    var x: number;
    var y: number;

    VideoShadowChars = [];
    VideoShadowAttrs = [];

    for (y = 0; y < VideoRows; y += 1) {
      VideoShadowChars[y] = [];
      VideoShadowAttrs[y] = [];
      for (x = 0; x < VideoColumns; x += 1) {
        VideoShadowChars[y][x] = "\u0000";
        VideoShadowAttrs[y][x] = -1;
      }
    }
  }

  function ensureVideoShadow(): void {
    if (VideoShadowChars.length !== VideoRows) {
      resetVideoShadow();
      return;
    }
    if (VideoRows > 0 && VideoShadowChars[0].length !== VideoColumns) {
      resetVideoShadow();
    }
  }

  function writeRaw(text: string): void {
    if (typeof console !== "undefined" && typeof console.write === "function") {
      console.write(text);
      return;
    }
    if (typeof console !== "undefined" && typeof console.putmsg === "function") {
      console.putmsg(text);
      return;
    }
    if (typeof print === "function") {
      print(text);
    }
  }

  export function VideoConfigure(): boolean {
    VideoMonochrome = false;
    if (typeof console !== "undefined" && typeof console.screen_columns === "number" && console.screen_columns > 0) {
      VideoColumns = console.screen_columns;
    }
    if (typeof console !== "undefined" && typeof console.screen_rows === "number" && console.screen_rows > 0) {
      VideoRows = console.screen_rows;
    }
    resetVideoShadow();
    return true;
  }

  export function VideoInstall(columns: number, borderColor: number): void {
    VideoColumns = columns;
    VideoBorderColor = borderColor;
    resetVideoShadow();
    if (typeof console !== "undefined" && typeof console.clear === "function") {
      console.clear();
    } else {
      runtime.clearScreen();
    }
  }

  export function VideoUninstall(): void {
    if (typeof console !== "undefined" && typeof console.attributes === "number") {
      console.attributes = 7;
    }
    resetVideoShadow();
  }

  export function VideoWriteText(x: number, y: number, color: number, text: string): void {
    var prevAttr: number = 7;
    var outputText: string;
    var i: number;
    var ch: string;
    var screenX: number;
    var segmentStart: number;
    var segmentText: string;
    var currentAttr: number = color & 0xff;
    var maxLen: number;

    if (text.length <= 0) {
      return;
    }
    outputText = translateCp437Text(text);
    if (outputText.length <= 0) {
      return;
    }

    if (hasConsoleOutput() && typeof console.gotoxy === "function") {
      if (y < 0 || y >= VideoRows) {
        return;
      }
      ensureVideoShadow();

      if (typeof console.attributes === "number") {
        prevAttr = console.attributes;
        console.attributes = currentAttr;
      }

      maxLen = outputText.length;
      i = 0;
      while (i < maxLen) {
        while (i < maxLen) {
          screenX = x + i;
          if (screenX < 0 || screenX >= VideoColumns) {
            i += 1;
            continue;
          }

          ch = outputText.charAt(i);
          if (VideoShadowAttrs[y][screenX] === currentAttr && VideoShadowChars[y][screenX] === ch) {
            i += 1;
            continue;
          }

          break;
        }

        if (i >= maxLen) {
          break;
        }

        segmentStart = i;
        segmentText = "";
        while (i < maxLen) {
          screenX = x + i;
          if (screenX < 0 || screenX >= VideoColumns) {
            break;
          }

          ch = outputText.charAt(i);
          if (VideoShadowAttrs[y][screenX] === currentAttr && VideoShadowChars[y][screenX] === ch) {
            break;
          }

          VideoShadowAttrs[y][screenX] = currentAttr;
          VideoShadowChars[y][screenX] = ch;
          segmentText += ch;
          i += 1;
        }

        if (segmentText.length > 0) {
          console.gotoxy(x + segmentStart + 1, y + 1);
          writeRaw(segmentText);
        }
      }

      if (typeof console.attributes === "number") {
        console.attributes = prevAttr;
      }
      return;
    }
    // Non-interactive fallback (e.g. Node local tests): avoid flooding output.
  }

  export function VideoMove(x: number, y: number, chars: number, data: unknown, toVideo: boolean): void {
    var i: number;
    var screenX: number;
    var ch: string;
    var attr: number;
    var moveData: { Text?: string; Attrs?: number[] };
    var savedText: string;
    var savedAttrs: number[];
    var prevAttr: number = 7;
    var activeAttr: number = -1;

    if (chars <= 0 || y < 0 || y >= VideoRows || data === null || typeof data !== "object") {
      return;
    }

    ensureVideoShadow();
    moveData = data as { Text?: string; Attrs?: number[] };

    if (!toVideo) {
      savedText = "";
      savedAttrs = [];
      for (i = 0; i < chars; i += 1) {
        screenX = x + i;
        if (screenX < 0 || screenX >= VideoColumns) {
          savedText += " ";
          savedAttrs.push(0x07);
          continue;
        }

        ch = VideoShadowChars[y][screenX];
        attr = VideoShadowAttrs[y][screenX];
        if (ch === "\u0000") {
          ch = " ";
        }
        if (attr < 0) {
          attr = 0x07;
        }

        savedText += ch;
        savedAttrs.push(attr & 0xff);
      }
      moveData.Text = savedText;
      moveData.Attrs = savedAttrs;
      return;
    }

    savedText = typeof moveData.Text === "string" ? moveData.Text : "";
    savedAttrs = moveData.Attrs !== undefined ? moveData.Attrs : [];

    if (hasConsoleOutput() && typeof console.gotoxy === "function" && typeof console.attributes === "number") {
      prevAttr = console.attributes;
    }

    for (i = 0; i < chars; i += 1) {
      screenX = x + i;
      if (screenX < 0 || screenX >= VideoColumns) {
        continue;
      }

      ch = i < savedText.length ? savedText.charAt(i) : " ";
      attr = i < savedAttrs.length ? (savedAttrs[i] & 0xff) : 0x07;
      if (ch.length <= 0) {
        ch = " ";
      }

      if (VideoShadowChars[y][screenX] === ch && VideoShadowAttrs[y][screenX] === attr) {
        continue;
      }

      VideoShadowChars[y][screenX] = ch;
      VideoShadowAttrs[y][screenX] = attr;

      if (hasConsoleOutput() && typeof console.gotoxy === "function") {
        if (typeof console.attributes === "number" && activeAttr !== attr) {
          console.attributes = attr;
          activeAttr = attr;
        }
        console.gotoxy(screenX + 1, y + 1);
        writeRaw(ch);
      }
    }

    if (hasConsoleOutput() && typeof console.attributes === "number") {
      console.attributes = prevAttr;
    }
  }

  export function VideoHideCursor(): void {
    VideoCursorVisible = false;
  }

  export function VideoShowCursor(): void {
    VideoCursorVisible = true;
  }
}
