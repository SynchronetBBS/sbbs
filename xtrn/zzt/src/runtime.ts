namespace ZZT {
  export interface Runtime {
    getExecDir(): string;
    getArgv(): string[];
    readTextFileLines(path: string): string[];
    readBinaryFile(path: string): number[] | null;
    writeBinaryFile(path: string, bytes: number[]): boolean;
    writeLine(message: string): void;
    clearScreen(): void;
    gotoXY(x: number, y: number): void;
    isTerminated(): boolean;
  }

  function safeJoin(base: string, file: string): string {
    if (!base) {
      return file;
    }
    var end = base.charAt(base.length - 1);
    if (end === "/" || end === "\\") {
      return base + file;
    }
    return base + "/" + file;
  }

  function makeFallbackWriter(): (message: string) => void {
    return function writeFallback(message: string): void {
      if (typeof console !== "undefined" && typeof console.writeln === "function") {
        console.writeln(message);
        return;
      }
      if (typeof console !== "undefined" && typeof (console as unknown as { log?: (msg: string) => void }).log === "function") {
        (console as unknown as { log: (msg: string) => void }).log(message);
        return;
      }
      if (typeof writeln === "function") {
        writeln(message);
        return;
      }
      if (typeof print === "function") {
        print(message + "\n");
      }
    };
  }

  function byteToCharCode(byte: number): string {
    return String.fromCharCode(byte & 0xff);
  }

  function getSyncFile(path: string): SyncFile | null {
    var f: SyncFile;
    if (typeof File === "undefined") {
      return null;
    }

    try {
      f = new File(path);
    } catch (_err) {
      // In non-Synchronet runtimes (for example Node.js), "File" may exist
      // with a different constructor signature.
      return null;
    }

    if (!f || typeof f.open !== "function") {
      return null;
    }
    return f;
  }

  var fallbackWriteLine = makeFallbackWriter();

  export var runtime: Runtime = {
    getExecDir: function getExecDir(): string {
      if (typeof js !== "undefined" && typeof js.exec_dir === "string" && js.exec_dir.length > 0) {
        return js.exec_dir;
      }
      return ".";
    },

    getArgv: function getArgv(): string[] {
      if (typeof argv === "undefined" || argv === null) {
        return [];
      }
      var result: string[] = [];
      for (var i = 0; i < argv.length; i += 1) {
        result.push(String(argv[i]));
      }
      return result;
    },

    readTextFileLines: function readTextFileLines(path: string): string[] {
      var lines: string[] = [];
      var f: SyncFile | null;

      f = getSyncFile(path);
      if (f === null) {
        return lines;
      }

      if (!f.exists || !f.open("r")) {
        return lines;
      }

      try {
        while (true) {
          var line = f.readln(2048);
          if (line === null || typeof line === "undefined") {
            break;
          }
          lines.push(line);
        }
      } finally {
        f.close();
      }

      return lines;
    },

    readBinaryFile: function readBinaryFile(path: string): number[] | null {
      var f: SyncFile | null = getSyncFile(path);
      var bytes: number[] = [];

      if (f === null) {
        return null;
      }
      if (!f.exists || !f.open("rb")) {
        return null;
      }

      try {
        while (true) {
          var chunk = f.read(4096);
          var i: number;
          if (chunk === null || typeof chunk === "undefined" || chunk.length === 0) {
            break;
          }
          for (i = 0; i < chunk.length; i += 1) {
            bytes.push(chunk.charCodeAt(i) & 0xff);
          }
        }
      } finally {
        f.close();
      }

      return bytes;
    },

    writeBinaryFile: function writeBinaryFile(path: string, bytes: number[]): boolean {
      var f: SyncFile | null = getSyncFile(path);
      var offset: number;
      var end: number;
      var i: number;
      var chunkChars: string[];
      var chunkSize: number = 4096;

      if (f === null) {
        return false;
      }
      if (!f.open("wb")) {
        return false;
      }

      try {
        for (offset = 0; offset < bytes.length; offset += chunkSize) {
          end = offset + chunkSize;
          if (end > bytes.length) {
            end = bytes.length;
          }

          chunkChars = [];
          for (i = offset; i < end; i += 1) {
            chunkChars.push(byteToCharCode(bytes[i]));
          }
          f.write(chunkChars.join(""));
        }
      } finally {
        f.close();
      }

      return true;
    },

    writeLine: function writeLine(message: string): void {
      fallbackWriteLine(message);
    },

    clearScreen: function clearScreen(): void {
      if (
        typeof console !== "undefined" &&
        typeof console.clear === "function" &&
        typeof console.writeln === "function"
      ) {
        console.clear();
      }
    },

    gotoXY: function gotoXY(x: number, y: number): void {
      if (typeof console !== "undefined" && typeof console.gotoxy === "function") {
        console.gotoxy(x, y);
      }
    },

    isTerminated: function isTerminated(): boolean {
      return typeof js !== "undefined" && js.terminated === true;
    }
  };

  export function execPath(name: string): string {
    return safeJoin(runtime.getExecDir(), name);
  }
}
