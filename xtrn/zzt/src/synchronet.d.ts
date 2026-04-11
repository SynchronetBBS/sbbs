declare var argv: string[] | undefined;
declare var js: {
  exec_dir?: string;
  terminated?: boolean;
  global?: unknown;
};
declare var system: {
  name?: string;
  qwk_id?: string;
  inet_addr?: string;
  text_dir?: string;
  exec_dir?: string;
  mods_dir?: string;
};

declare var console: {
  print?: (value?: unknown) => void;
  writeln?: (value?: unknown) => void;
  write?: (value?: unknown) => void;
  putmsg?: (value?: unknown) => void;
  gotoxy?: (x: number, y: number) => void;
  clear?: () => void;
  home?: () => void;
  cleartoeol?: () => void;
  right?: (count?: number) => void;
  left?: (count?: number) => void;
  up?: (count?: number) => void;
  down?: (count?: number) => void;
  attributes?: number;
  screen_columns?: number;
  screen_rows?: number;
  line_counter?: number;
  status?: number;
  ctrlkey_passthru?: number | string;
  aborted?: boolean;
  cterm_version?: number;
  inkey?: (mode?: number, timeout?: number) => string;
  getkey?: (mode?: number) => string;
  getkeys?: (keys: string, max?: number, mode?: number) => string;
  getstr?: (str?: string, maxLen?: number, mode?: number) => string;
  ungetstr?: (value: string) => void;
};

declare var bbs: {
  sys_status?: number;
  online?: boolean | number;
  hangup?: () => void;
};

declare var user: {
  alias?: string;
  name?: string;
  number?: number;
};

declare function print(value?: unknown): void;
declare function writeln(value?: unknown): void;
declare function load(path: string): unknown;
declare function load(scope: boolean, path: string, arg?: unknown): unknown;
declare function require(path: string, symbol?: string): unknown;
declare function directory(pattern: string, flags?: number): string[];
declare function file_isdir(path: string): boolean;
declare function file_getname(path: string): string;
declare function file_copy(source: string, destination: string): boolean;
declare function file_size(path: string): number;
declare function mkpath(path: string): boolean;
declare function mswait(ms: number): number;
declare function sleep(ms: number): void;
declare function yield(waitForIo?: boolean): void;

interface SyncFile {
  readonly exists: boolean;
  readonly eof?: boolean;
  readonly length?: number;
  readonly error?: number;
  position?: number;
  open(mode: string, shareable?: boolean): boolean;
  readln(maxLen?: number): string | null;
  read(count?: number): string | null;
  readBin(bytes?: number): number;
  write(value: string): boolean | number | void;
  writeBin(value: number, bytes?: number): boolean | number | void;
  close(): void;
}

declare var File: {
  new (path: string): SyncFile;
};
