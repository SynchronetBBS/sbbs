namespace ZZT {
  export var OrderPrintId: string = "";
  export var TextWindowInited: boolean = false;
  export var TextWindowRejected: boolean = false;

  export interface TextWindowState {
    Selectable: boolean;
    LineCount: number;
    LinePos: number;
    Lines: string[];
    Hyperlink: string;
    Title: string;
    LoadedFilename: string;
    ScreenCopy: unknown[];
  }

  export var TextWindowX: number = 5;
  export var TextWindowY: number = 3;
  export var TextWindowWidth: number = 50;
  export var TextWindowHeight: number = 18;

  var MAX_TEXT_WINDOW_LINES: number = 1024;
  var MAX_RESOURCE_DATA_FILES: number = 24;
  var TEXT_WINDOW_ANIM_SPEED: number = 25;

  var ResourceDataHeaderLoaded: boolean = false;
  var ResourceDataEntryCount: number = 0;
  var ResourceDataNames: string[] = [];
  var ResourceDataOffsets: number[] = [];

  var TextWindowStrInnerEmpty: string = "";
  var TextWindowStrText: string = "";
  var TextWindowStrInnerLine: string = "";
  var TextWindowStrTop: string = "";
  var TextWindowStrBottom: string = "";
  var TextWindowStrSep: string = "";
  var TextWindowStrInnerSep: string = "";
  var TextWindowStrInnerArrows: string = "";

  function upperString(value: string): string {
    return value.toUpperCase();
  }

  function clamp(value: number, min: number, max: number): number {
    if (value < min) {
      return min;
    }
    if (value > max) {
      return max;
    }
    return value;
  }

  function getLine(state: TextWindowState, linePos: number): string {
    if (linePos < 1 || linePos > state.LineCount) {
      return "";
    }
    return state.Lines[linePos - 1];
  }

  function setLine(state: TextWindowState, linePos: number, value: string): void {
    if (linePos < 1 || linePos > state.LineCount) {
      return;
    }
    state.Lines[linePos - 1] = value;
  }

  function textWindowHasExtension(filename: string): boolean {
    var i: number;
    for (i = 0; i < filename.length; i += 1) {
      if (filename.charAt(i) === ".") {
        return true;
      }
    }
    return false;
  }

  function textPathHasSeparator(path: string): boolean {
    return path.indexOf("/") >= 0 || path.indexOf("\\") >= 0;
  }

  function textPathIsAbsolute(path: string): boolean {
    return (path.length > 0 && (path.charAt(0) === "/" || path.charAt(0) === "\\")) || path.indexOf(":") >= 0;
  }

  function pushUniqueTextPath(paths: string[], path: string): void {
    var i: number;
    if (path.length <= 0) {
      return;
    }
    for (i = 0; i < paths.length; i += 1) {
      if (paths[i] === path) {
        return;
      }
    }
    paths.push(path);
  }

  function getTextNameVariants(filename: string): string[] {
    var variants: string[] = [];
    var dotPos: number = filename.lastIndexOf(".");
    var base: string;
    var ext: string;

    pushUniqueTextPath(variants, filename);
    pushUniqueTextPath(variants, upperString(filename));
    pushUniqueTextPath(variants, filename.toLowerCase());

    if (dotPos > 0 && dotPos < filename.length - 1) {
      base = filename.slice(0, dotPos);
      ext = filename.slice(dotPos);
      pushUniqueTextPath(variants, base + ext.toUpperCase());
      pushUniqueTextPath(variants, base + ext.toLowerCase());
    }

    return variants;
  }

  function loadTextFileLinesWithFallback(filename: string): string[] {
    var searchPaths: string[] = [];
    var variants: string[] = getTextNameVariants(filename);
    var i: number;
    var nameVariant: string;
    var lines: string[] = [];
    var hasSep: boolean;
    var isAbs: boolean;

    for (i = 0; i < variants.length; i += 1) {
      nameVariant = variants[i];
      hasSep = textPathHasSeparator(nameVariant);
      isAbs = textPathIsAbsolute(nameVariant);

      if (isAbs) {
        pushUniqueTextPath(searchPaths, nameVariant);
        continue;
      }

      if (hasSep) {
        pushUniqueTextPath(searchPaths, execPath(nameVariant));
        pushUniqueTextPath(searchPaths, execPath("../" + nameVariant));
        pushUniqueTextPath(searchPaths, nameVariant);
      } else {
        pushUniqueTextPath(searchPaths, execPath(nameVariant));
        pushUniqueTextPath(searchPaths, execPath("DOCS/" + nameVariant));
        pushUniqueTextPath(searchPaths, execPath("docs/" + nameVariant));
        pushUniqueTextPath(searchPaths, execPath("reconstruction-of-zzt-master/DOCS/" + nameVariant));
        pushUniqueTextPath(searchPaths, nameVariant);
        pushUniqueTextPath(searchPaths, "DOCS/" + nameVariant);
        pushUniqueTextPath(searchPaths, "docs/" + nameVariant);
      }
    }

    for (i = 0; i < searchPaths.length; i += 1) {
      lines = runtime.readTextFileLines(searchPaths[i]);
      if (lines.length > 0) {
        return lines;
      }
    }

    return [];
  }

  function textWindowDrawTitle(color: number, title: string): void {
    VideoWriteText(TextWindowX + 2, TextWindowY + 1, color, TextWindowStrInnerEmpty);
    VideoWriteText(
      TextWindowX + Math.floor((TextWindowWidth - title.length) / 2),
      TextWindowY + 1,
      color,
      title
    );
  }

  function textWindowPrint(_state: TextWindowState): void {
    // Printer output is not implemented in the Synchronet JS port.
  }

  function bytesToString(bytes: number[], offset: number, length: number): string {
    var end: number = offset + length;
    var i: number;
    var out: string = "";

    if (offset < 0) {
      offset = 0;
    }
    if (end > bytes.length) {
      end = bytes.length;
    }

    for (i = offset; i < end; i += 1) {
      out += String.fromCharCode(bytes[i] & 0xff);
    }

    return out;
  }

  function readInt16LE(bytes: number[], offset: number): number {
    var value: number;
    if (offset + 1 >= bytes.length) {
      return 0;
    }
    value = (bytes[offset] & 0xff) | ((bytes[offset + 1] & 0xff) << 8);
    if (value >= 0x8000) {
      value -= 0x10000;
    }
    return value;
  }

  function readUInt32LE(bytes: number[], offset: number): number {
    if (offset + 3 >= bytes.length) {
      return 0;
    }
    return (
      (bytes[offset] & 0xff) +
      ((bytes[offset + 1] & 0xff) * 0x100) +
      ((bytes[offset + 2] & 0xff) * 0x10000) +
      ((bytes[offset + 3] & 0xff) * 0x1000000)
    );
  }

  function readPascalString(bytes: number[], offset: number, maxLen: number): string {
    var length: number;

    if (offset >= bytes.length) {
      return "";
    }
    length = bytes[offset] & 0xff;
    if (length > maxLen) {
      length = maxLen;
    }
    return bytesToString(bytes, offset + 1, length);
  }

  function loadResourceDataHeader(): void {
    var bytes: number[] | null;
    var offset: number;
    var i: number;
    var count: number;
    var name: string;
    var resourcePath: string = execPath(ResourceDataFileName);

    if (ResourceDataHeaderLoaded) {
      return;
    }
    ResourceDataHeaderLoaded = true;
    ResourceDataEntryCount = -1;
    ResourceDataNames = [];
    ResourceDataOffsets = [];

    bytes = runtime.readBinaryFile(resourcePath);
    if (bytes === null) {
      bytes = runtime.readBinaryFile(ResourceDataFileName);
    }
    if (bytes === null || bytes.length < 2) {
      return;
    }

    count = readInt16LE(bytes, 0);
    if (count < 0) {
      count = 0;
    }
    if (count > MAX_RESOURCE_DATA_FILES) {
      count = MAX_RESOURCE_DATA_FILES;
    }

    offset = 2;
    for (i = 0; i < MAX_RESOURCE_DATA_FILES; i += 1) {
      if (offset + 51 > bytes.length) {
        return;
      }
      name = readPascalString(bytes, offset, 50);
      ResourceDataNames.push(name);
      offset += 51;
    }

    for (i = 0; i < MAX_RESOURCE_DATA_FILES; i += 1) {
      if (offset + 4 > bytes.length) {
        return;
      }
      ResourceDataOffsets.push(readUInt32LE(bytes, offset));
      offset += 4;
    }

    ResourceDataEntryCount = count;
  }

  function findResourceEntryPos(filename: string): number {
    var i: number;
    var upFilename: string = upperString(filename);

    loadResourceDataHeader();
    if (ResourceDataEntryCount <= 0) {
      return 0;
    }

    for (i = 0; i < ResourceDataEntryCount; i += 1) {
      if (upperString(ResourceDataNames[i]) === upFilename) {
        return i + 1;
      }
    }
    return 0;
  }

  function readResourceEntryLines(entryPos: number, includeTerminatorBlank: boolean): string[] {
    var lines: string[] = [];
    var bytes: number[] | null;
    var offset: number;
    var lineLen: number;
    var line: string;
    var resourcePath: string = execPath(ResourceDataFileName);

    bytes = runtime.readBinaryFile(resourcePath);
    if (bytes === null) {
      bytes = runtime.readBinaryFile(ResourceDataFileName);
    }
    if (bytes === null) {
      return lines;
    }

    if (entryPos < 1 || entryPos > ResourceDataOffsets.length) {
      return lines;
    }

    offset = ResourceDataOffsets[entryPos - 1];
    if (offset < 0 || offset >= bytes.length) {
      return lines;
    }

    while (offset < bytes.length) {
      lineLen = bytes[offset] & 0xff;
      offset += 1;
      if (offset + lineLen > bytes.length) {
        break;
      }

      line = bytesToString(bytes, offset, lineLen);
      offset += lineLen;

      if (line === "@") {
        if (includeTerminatorBlank) {
          lines.push("");
        }
        break;
      }
      lines.push(line);
      if (lines.length >= MAX_TEXT_WINDOW_LINES) {
        break;
      }
    }

    return lines;
  }

  function textWindowDrawLine(state: TextWindowState, linePos: number, withoutFormatting: boolean, viewingFile: boolean): void {
    var lineY: number =
      ((TextWindowY + linePos) - state.LinePos) + Math.floor(TextWindowHeight / 2) + 1;
    var textOffset: number = 0;
    var textColor: number = 0x1e;
    var textX: number = TextWindowX + 4;
    var line: string = getLine(state, linePos);
    var splitPos: number;
    var writeText: string;

    if (linePos === state.LinePos) {
      VideoWriteText(TextWindowX + 2, lineY, 0x1c, TextWindowStrInnerArrows);
    } else {
      VideoWriteText(TextWindowX + 2, lineY, 0x1e, TextWindowStrInnerEmpty);
    }

    if (linePos > 0 && linePos <= state.LineCount) {
      if (withoutFormatting) {
        VideoWriteText(TextWindowX + 4, lineY, 0x1e, line);
        return;
      }

      if (line.length > 0) {
        if (line.charAt(0) === "!") {
          splitPos = line.indexOf(";");
          if (splitPos >= 0) {
            textOffset = splitPos + 1;
          } else {
            textOffset = 0;
          }
          VideoWriteText(textX + 2, lineY, 0x1d, String.fromCharCode(16));
          textX += 5;
          textColor = 0x1f;
        } else if (line.charAt(0) === ":") {
          splitPos = line.indexOf(";");
          if (splitPos >= 0) {
            textOffset = splitPos + 1;
          } else {
            textOffset = 0;
          }
          textColor = 0x1f;
        } else if (line.charAt(0) === "$") {
          textOffset = 1;
          textColor = 0x1f;
          textX = (textX - 4) + Math.floor((TextWindowWidth - line.length) / 2);
        }
      }

      writeText = line.slice(textOffset);
      if (writeText.length > 0) {
        VideoWriteText(textX, lineY, textColor, writeText);
      }
    } else if (linePos === 0 || linePos === (state.LineCount + 1)) {
      VideoWriteText(TextWindowX + 2, lineY, 0x1e, TextWindowStrInnerSep);
    } else if (linePos === -4 && viewingFile) {
      VideoWriteText(TextWindowX + 2, lineY, 0x1a, "   Use            to view text,");
      VideoWriteText(TextWindowX + 9, lineY, 0x1f, String.fromCharCode(24) + " " + String.fromCharCode(25) + ", Enter");
    } else if (linePos === -3 && viewingFile) {
      VideoWriteText(TextWindowX + 3, lineY, 0x1a, "                 to print.");
      VideoWriteText(TextWindowX + 14, lineY, 0x1f, "Alt-P");
    }
  }

  export function TextWindowInitState(state: TextWindowState): void {
    state.LineCount = 0;
    state.LinePos = 1;
    state.Lines = [];
    state.Hyperlink = "";
    state.LoadedFilename = "";
    state.ScreenCopy = [];
  }

  export function TextWindowDrawOpen(state: TextWindowState): void {
    var i: number;
    var iy: number;
    var copyRows: number = TextWindowHeight + 1;

    if (state.ScreenCopy.length < copyRows + 1) {
      state.ScreenCopy = [];
    }

    for (iy = 1; iy <= copyRows; iy += 1) {
      state.ScreenCopy[iy] = {};
      VideoMove(TextWindowX, iy + TextWindowY - 1, TextWindowWidth, state.ScreenCopy[iy], false);
    }

    for (iy = Math.floor(TextWindowHeight / 2); iy >= 0; iy -= 1) {
      VideoWriteText(TextWindowX, TextWindowY + iy + 1, 0x0f, TextWindowStrText);
      VideoWriteText(TextWindowX, TextWindowY + TextWindowHeight - iy - 1, 0x0f, TextWindowStrText);
      VideoWriteText(TextWindowX, TextWindowY + iy, 0x0f, TextWindowStrTop);
      VideoWriteText(TextWindowX, TextWindowY + TextWindowHeight - iy, 0x0f, TextWindowStrBottom);
      if (typeof mswait === "function") {
        mswait(TEXT_WINDOW_ANIM_SPEED);
      }
    }

    for (i = 1; i <= TextWindowHeight - 1; i += 1) {
      VideoWriteText(TextWindowX, TextWindowY + i, 0x0f, TextWindowStrText);
    }
    VideoWriteText(TextWindowX, TextWindowY + 2, 0x0f, TextWindowStrSep);
    textWindowDrawTitle(0x1e, state.Title);
  }

  export function TextWindowDrawClose(state: TextWindowState): void {
    var iy: number;
    var bottomCopyRow: number;

    if (state.ScreenCopy.length <= 0) {
      TransitionDrawToBoard();
      GameUpdateSidebar();
      return;
    }

    for (iy = 0; iy <= Math.floor(TextWindowHeight / 2); iy += 1) {
      VideoWriteText(TextWindowX, TextWindowY + iy, 0x0f, TextWindowStrTop);
      VideoWriteText(TextWindowX, TextWindowY + TextWindowHeight - iy, 0x0f, TextWindowStrBottom);
      if (typeof mswait === "function") {
        mswait(Math.floor((TEXT_WINDOW_ANIM_SPEED * 3) / 4));
      }

      if (state.ScreenCopy[iy + 1] !== undefined) {
        VideoMove(TextWindowX, TextWindowY + iy, TextWindowWidth, state.ScreenCopy[iy + 1], true);
      }

      bottomCopyRow = (TextWindowHeight - iy) + 1;
      if (state.ScreenCopy[bottomCopyRow] !== undefined) {
        VideoMove(
          TextWindowX,
          TextWindowY + TextWindowHeight - iy,
          TextWindowWidth,
          state.ScreenCopy[bottomCopyRow],
          true
        );
      }
    }
  }

  export function TextWindowDraw(state: TextWindowState, withoutFormatting: boolean, viewingFile: boolean): void {
    var i: number;

    for (i = 0; i <= (TextWindowHeight - 4); i += 1) {
      textWindowDrawLine(
        state,
        state.LinePos - Math.floor(TextWindowHeight / 2) + i + 2,
        withoutFormatting,
        viewingFile
      );
    }
    textWindowDrawTitle(0x1e, state.Title);
  }

  export function TextWindowAppend(state: TextWindowState, line: string): void {
    state.Lines.push(line);
    state.LineCount = state.Lines.length;
  }

  export function TextWindowFree(state: TextWindowState): void {
    state.Lines = [];
    state.LineCount = 0;
    state.LoadedFilename = "";
  }

  export function TextWindowOpenFile(filename: string, state: TextWindowState): void {
    var loadedFilename: string = filename;
    var lines: string[] = [];
    var i: number;
    var entryPos: number = 0;

    if (!textWindowHasExtension(loadedFilename)) {
      loadedFilename += ".HLP";
    }

    if (loadedFilename.length > 0 && loadedFilename.charAt(0) === "*") {
      loadedFilename = loadedFilename.slice(1);
      entryPos = -1;
    }

    TextWindowInitState(state);
    state.LoadedFilename = upperString(loadedFilename);

    if (entryPos === 0) {
      entryPos = findResourceEntryPos(loadedFilename);
    }

    if (entryPos <= 0) {
      lines = loadTextFileLinesWithFallback(loadedFilename);
    } else {
      lines = readResourceEntryLines(entryPos, true);
    }

    for (i = 0; i < lines.length; i += 1) {
      TextWindowAppend(state, lines[i]);
    }
  }

  export function ResourceDataLoadLines(filename: string, includeTerminatorBlank: boolean): string[] {
    var loadedFilename: string = filename;
    var entryPos: number;

    if (!textWindowHasExtension(loadedFilename)) {
      loadedFilename += ".HLP";
    }

    entryPos = findResourceEntryPos(loadedFilename);
    if (entryPos <= 0) {
      return [];
    }

    return readResourceEntryLines(entryPos, includeTerminatorBlank);
  }

  export function TextWindowSaveFile(filename: string, state: TextWindowState): void {
    var file: SyncFile;
    var i: number;

    if (typeof File === "undefined") {
      return;
    }

    try {
      file = new File(filename);
      if (!file.open("w")) {
        return;
      }
      try {
        for (i = 1; i <= state.LineCount; i += 1) {
          file.write(getLine(state, i) + "\r\n");
        }
      } finally {
        file.close();
      }
    } catch (_err) {
      return;
    }
  }

  export function TextWindowEdit(state: TextWindowState): void {
    var newLinePos: number;
    var insertMode: boolean;
    var charPos: number;
    var i: number;
    var line: string;
    var cursorY: number = TextWindowY + Math.floor(TextWindowHeight / 2) + 1;
    var keyCode: number;
    var ch: string;

    function deleteCurrLine(): void {
      var idx: number;
      if (state.LineCount > 1) {
        for (idx = state.LinePos + 1; idx <= state.LineCount; idx += 1) {
          state.Lines[idx - 2] = state.Lines[idx - 1];
        }
        state.Lines.pop();
        state.LineCount -= 1;
        if (state.LinePos > state.LineCount) {
          newLinePos = state.LineCount;
        } else {
          TextWindowDraw(state, true, false);
        }
      } else if (state.LineCount === 1) {
        state.Lines[0] = "";
      }
    }

    if (state.LineCount === 0) {
      TextWindowAppend(state, "");
    }

    insertMode = true;
    state.LinePos = 1;
    charPos = 1;
    TextWindowDraw(state, true, false);

    while (!runtime.isTerminated()) {
      VideoWriteText(77, 14, 0x1e, insertMode ? "on " : "off");
      line = getLine(state, state.LinePos);
      if (charPos > line.length + 1) {
        charPos = line.length + 1;
      }
      if (charPos >= line.length + 1) {
        VideoWriteText(charPos + TextWindowX + 3, cursorY, 0x70, " ");
      } else {
        VideoWriteText(charPos + TextWindowX + 3, cursorY, 0x70, line.charAt(charPos - 1));
      }

      InputReadWaitKey();
      newLinePos = state.LinePos;
      line = getLine(state, state.LinePos);
      ch = InputKeyPressed;
      keyCode = ch.length > 0 ? (ch.charCodeAt(0) & 0xff) : 0;

      if (ch === KEY_UP) {
        newLinePos -= 1;
      } else if (ch === KEY_DOWN) {
        newLinePos += 1;
      } else if (ch === KEY_PAGE_UP) {
        newLinePos -= TextWindowHeight - 4;
      } else if (ch === KEY_PAGE_DOWN) {
        newLinePos += TextWindowHeight - 4;
      } else if (ch === KEY_RIGHT) {
        charPos += 1;
        if (charPos > line.length + 1) {
          charPos = 1;
          newLinePos += 1;
        }
      } else if (ch === KEY_LEFT) {
        charPos -= 1;
        if (charPos < 1) {
          charPos = TextWindowWidth;
          newLinePos -= 1;
        }
      } else if (ch === KEY_ENTER) {
        if (state.LineCount < MAX_TEXT_WINDOW_LINES) {
          for (i = state.LineCount; i >= state.LinePos + 1; i -= 1) {
            state.Lines[i] = state.Lines[i - 1];
          }
          state.Lines[state.LinePos] = line.slice(charPos - 1);
          setLine(state, state.LinePos, line.slice(0, charPos - 1));
          state.LineCount += 1;
          newLinePos += 1;
          charPos = 1;
        }
      } else if (ch === KEY_BACKSPACE) {
        if (charPos > 1) {
          setLine(state, state.LinePos, line.slice(0, charPos - 2) + line.slice(charPos - 1));
          charPos -= 1;
        } else if (line.length === 0) {
          deleteCurrLine();
          newLinePos -= 1;
          charPos = TextWindowWidth;
        }
      } else if (ch === KEY_INSERT) {
        insertMode = !insertMode;
      } else if (ch === KEY_DELETE) {
        setLine(state, state.LinePos, line.slice(0, charPos - 1) + line.slice(charPos));
      } else if (ch === KEY_CTRL_Y) {
        deleteCurrLine();
      } else if (keyCode >= 32 && charPos < (TextWindowWidth - 7)) {
        if (!insertMode) {
          if (charPos > line.length) {
            setLine(state, state.LinePos, line + ch);
          } else {
            setLine(state, state.LinePos, line.slice(0, charPos - 1) + ch + line.slice(charPos));
          }
          charPos += 1;
        } else if (line.length < (TextWindowWidth - 8)) {
          setLine(state, state.LinePos, line.slice(0, charPos - 1) + ch + line.slice(charPos - 1));
          charPos += 1;
        }
      }

      if (newLinePos < 1) {
        newLinePos = 1;
      } else if (newLinePos > state.LineCount) {
        newLinePos = state.LineCount;
      }

      if (newLinePos !== state.LinePos) {
        state.LinePos = newLinePos;
        TextWindowDraw(state, true, false);
      } else {
        textWindowDrawLine(state, state.LinePos, true, false);
      }

      if (InputKeyPressed === KEY_ESCAPE) {
        break;
      }
    }

    if (state.LineCount > 0 && getLine(state, state.LineCount).length === 0) {
      state.Lines.pop();
      state.LineCount -= 1;
      if (state.LineCount <= 0) {
        TextWindowAppend(state, "");
      }
    }
  }

  export function TextWindowSelect(state: TextWindowState, hyperlinkAsSelect: boolean, viewingFile: boolean): void {
    var newLinePos: number;
    var iLine: number;
    var line: string;
    var pointerStr: string;
    var splitPos: number;
    var key: string;
    var titleHint: string;
    var keyCode: number;
    var typeAhead: string = "";
    var typeAheadTickMs: number = 0;

    function startsWithPrefix(value: string, prefix: string): boolean {
      if (prefix.length > value.length) {
        return false;
      }
      return value.slice(0, prefix.length) === prefix;
    }

    function normalizeTypeAheadText(value: string): string {
      var text: string = value;

      if (text.length > 0 && text.charAt(0) === "!") {
        text = text.slice(1);
      }

      while (text.length > 0 && (text.charAt(0) === " " || text.charAt(0) === "\t")) {
        text = text.slice(1);
      }

      if (text.length > 0 && text.charAt(text.length - 1) === "/") {
        text = text.slice(0, text.length - 1);
      }
      return upperString(text);
    }

    function findTypeAheadMatch(prefix: string): number {
      var startLine: number = state.LinePos + 1;
      var pass: number;
      var lineNo: number;
      var text: string;
      var fromLine: number;
      var toLine: number;

      if (state.LineCount <= 0 || prefix.length <= 0) {
        return 0;
      }

      for (pass = 0; pass < 2; pass += 1) {
        if (pass === 0) {
          fromLine = startLine;
          toLine = state.LineCount;
        } else {
          fromLine = 1;
          toLine = state.LinePos;
        }

        if (fromLine > toLine) {
          continue;
        }

        for (lineNo = fromLine; lineNo <= toLine; lineNo += 1) {
          text = normalizeTypeAheadText(getLine(state, lineNo));
          if (startsWithPrefix(text, prefix)) {
            return lineNo;
          }
        }
      }

      return 0;
    }

    TextWindowRejected = false;
    state.Hyperlink = "";
    TextWindowDraw(state, false, viewingFile);

    while (!runtime.isTerminated()) {
      InputReadWaitKey();
      key = InputKeyPressed;
      keyCode = key.length > 0 ? (key.charCodeAt(0) & 0xff) : 0;
      newLinePos = state.LinePos;

      if (InputDeltaY !== 0) {
        newLinePos += InputDeltaY;
        typeAhead = "";
        typeAheadTickMs = 0;
      } else if (InputShiftPressed || key === KEY_ENTER) {
        InputShiftAccepted = true;
        typeAhead = "";
        typeAheadTickMs = 0;
        line = getLine(state, state.LinePos);

        if (line.length > 0 && line.charAt(0) === "!") {
          pointerStr = line.slice(1);
          splitPos = pointerStr.indexOf(";");
          if (splitPos >= 0) {
            pointerStr = pointerStr.slice(0, splitPos);
          }

          if (pointerStr.length > 0 && pointerStr.charAt(0) === "-") {
            pointerStr = pointerStr.slice(1);
            TextWindowFree(state);
            TextWindowOpenFile(pointerStr, state);
            if (state.LineCount <= 0) {
              return;
            }

            viewingFile = true;
            newLinePos = state.LinePos;
            TextWindowDraw(state, false, viewingFile);
            InputKeyPressed = String.fromCharCode(0);
            InputShiftPressed = false;
          } else if (hyperlinkAsSelect) {
            state.Hyperlink = pointerStr;
          } else {
            pointerStr = ":" + pointerStr;
            for (iLine = 1; iLine <= state.LineCount; iLine += 1) {
              line = getLine(state, iLine);
              if (pointerStr.length <= line.length &&
                  upperString(pointerStr) === upperString(line.slice(0, pointerStr.length))) {
                newLinePos = iLine;
                InputKeyPressed = String.fromCharCode(0);
                InputShiftPressed = false;
                break;
              }
            }
          }
        }
      } else {
        if (key === KEY_PAGE_UP) {
          newLinePos -= TextWindowHeight - 4;
          typeAhead = "";
          typeAheadTickMs = 0;
        } else if (key === KEY_PAGE_DOWN) {
          newLinePos += TextWindowHeight - 4;
          typeAhead = "";
          typeAheadTickMs = 0;
        } else if (key === KEY_ALT_P || (viewingFile && upperString(key) === "P")) {
          textWindowPrint(state);
        } else if (state.Selectable && keyCode >= 32 && keyCode <= 126) {
          var nowMs: number = new Date().getTime();
          var keyUpper: string = upperString(key);
          var matchedLine: number;

          if (typeAheadTickMs <= 0 || (nowMs - typeAheadTickMs) > 1200) {
            typeAhead = keyUpper;
          } else if (typeAhead.length < 24) {
            typeAhead += keyUpper;
          }
          typeAheadTickMs = nowMs;

          matchedLine = findTypeAheadMatch(typeAhead);
          if (matchedLine === 0 && typeAhead.length > 1) {
            typeAhead = keyUpper;
            matchedLine = findTypeAheadMatch(typeAhead);
          }
          if (matchedLine > 0) {
            newLinePos = matchedLine;
          }
        }
      }

      if (state.LineCount > 0) {
        newLinePos = clamp(newLinePos, 1, state.LineCount);
      } else {
        newLinePos = 1;
      }

      if (newLinePos !== state.LinePos) {
        state.LinePos = newLinePos;
        TextWindowDraw(state, false, viewingFile);
        line = getLine(state, state.LinePos);
        if (line.length > 0 && line.charAt(0) === "!") {
          if (hyperlinkAsSelect) {
            titleHint = String.fromCharCode(174) + "Press ENTER to select this" + String.fromCharCode(175);
          } else {
            titleHint = String.fromCharCode(174) + "Press ENTER for more info" + String.fromCharCode(175);
          }
          textWindowDrawTitle(0x1e, titleHint);
        }
      }

      if (InputJoystickMoved && typeof mswait === "function") {
        mswait(35);
      }

      if (InputKeyPressed === KEY_ESCAPE || InputKeyPressed === KEY_ENTER || InputShiftPressed) {
        break;
      }
    }

    if (InputKeyPressed === KEY_ESCAPE) {
      InputKeyPressed = String.fromCharCode(0);
      TextWindowRejected = true;
    }
  }

  export function TextWindowDisplayFile(filename: string, title: string): void {
    var state: TextWindowState = {
      Selectable: false,
      LineCount: 0,
      LinePos: 1,
      Lines: [],
      Hyperlink: "",
      Title: title,
      LoadedFilename: "",
      ScreenCopy: []
    };

    TextWindowOpenFile(filename, state);
    state.Selectable = false;
    if (state.LineCount > 0) {
      TextWindowDrawOpen(state);
      TextWindowSelect(state, false, true);
      TextWindowDrawClose(state);
    }
    TextWindowFree(state);
  }

  export function TextWindowInit(x: number, y: number, width: number, height: number): void {
    var i: number;
    var sepPos: number;

    TextWindowX = x;
    TextWindowY = y;
    TextWindowWidth = width;
    TextWindowHeight = height;

    TextWindowStrInnerEmpty = "";
    TextWindowStrInnerLine = "";
    for (i = 1; i <= (TextWindowWidth - 5); i += 1) {
      TextWindowStrInnerEmpty += " ";
      TextWindowStrInnerLine += String.fromCharCode(205);
    }

    TextWindowStrTop = String.fromCharCode(198) + String.fromCharCode(209) +
      TextWindowStrInnerLine + String.fromCharCode(209) + String.fromCharCode(181);
    TextWindowStrBottom = String.fromCharCode(198) + String.fromCharCode(207) +
      TextWindowStrInnerLine + String.fromCharCode(207) + String.fromCharCode(181);
    TextWindowStrSep = " " + String.fromCharCode(198) + TextWindowStrInnerLine + String.fromCharCode(181) + " ";
    TextWindowStrText = " " + String.fromCharCode(179) + TextWindowStrInnerEmpty + String.fromCharCode(179) + " ";

    TextWindowStrInnerArrows = TextWindowStrInnerEmpty;
    if (TextWindowStrInnerArrows.length > 0) {
      TextWindowStrInnerArrows =
        String.fromCharCode(175) +
        TextWindowStrInnerArrows.slice(1, TextWindowStrInnerArrows.length - 1) +
        String.fromCharCode(174);
    }

    TextWindowStrInnerSep = TextWindowStrInnerEmpty;
    for (i = 1; i <= Math.floor(TextWindowWidth / 5); i += 1) {
      sepPos = i * 5 + Math.floor((TextWindowWidth % 5) / 2) - 1;
      if (sepPos >= 0 && sepPos < TextWindowStrInnerSep.length) {
        TextWindowStrInnerSep =
          TextWindowStrInnerSep.slice(0, sepPos) +
          String.fromCharCode(7) +
          TextWindowStrInnerSep.slice(sepPos + 1);
      }
    }

    TextWindowInited = true;
  }
}
