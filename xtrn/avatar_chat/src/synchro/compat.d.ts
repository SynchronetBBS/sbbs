declare function exit(code?: number): void;
declare function load(filename: string): any;
declare function load<T>(context: T, filename: string, symbol?: string, defaultValue?: any): any;
declare function log(level: number, text: string): void;
declare function log(text: string): void;
declare function base64_decode(data: string): string;
declare function ascii(text: string): number;
declare function mswait(milliseconds: number): void;
declare function md5_calc(text: string): string;

declare const K_NONE: number;
declare const K_NOCRLF: number;
declare const K_NOECHO: number;
declare const K_NOSPIN: number;
declare const K_EXTKEYS: number;

declare const KEY_UP: string;
declare const KEY_DOWN: string;
declare const KEY_LEFT: string;
declare const KEY_RIGHT: string;
declare const KEY_HOME: string;
declare const KEY_END: string;
declare const KEY_DEL: string;
declare const KEY_PAGEUP: string;
declare const KEY_PAGEDN: string;
declare const KEY_ESC: string;

declare const BLACK: number;
declare const BLUE: number;
declare const GREEN: number;
declare const CYAN: number;
declare const RED: number;
declare const MAGENTA: number;
declare const BROWN: number;
declare const LIGHTGRAY: number;
declare const DARKGRAY: number;
declare const LIGHTBLUE: number;
declare const LIGHTGREEN: number;
declare const LIGHTCYAN: number;
declare const LIGHTRED: number;
declare const LIGHTMAGENTA: number;
declare const YELLOW: number;
declare const WHITE: number;

declare const BG_BLACK: number;
declare const BG_BLUE: number;
declare const BG_GREEN: number;
declare const BG_CYAN: number;
declare const BG_RED: number;
declare const BG_MAGENTA: number;
declare const BG_BROWN: number;
declare const BG_LIGHTGRAY: number;

interface SynchronetConsole {
  screen_columns: number;
  screen_rows: number;
  attributes: number;
  line_counter: number;
  clear(attribute?: number | string, autopause?: boolean): void;
  home(): void;
  gotoxy(x: number, y: number): void;
  cleartoeol(attr?: number): void;
  print(text: string): void;
  writeln(text: string): void;
  inkey(mode?: number, timeout?: number): string;
}

declare const console: SynchronetConsole;

interface SynchronetJS {
  exec_dir: string;
  startup_dir: string;
  terminated: boolean;
  global: any;
}

declare const js: SynchronetJS;

interface SynchronetSystem {
  name: string;
  qwk_id: string;
  data_dir: string;
  matchuser(alias: string): number;
}

declare const system: SynchronetSystem;

interface SynchronetUser {
  number: number;
  alias: string;
  ip_address: string;
}

declare const user: SynchronetUser;

interface SynchronetBbs {
  online: number;
  command_str: string;
}

declare const bbs: SynchronetBbs;

declare class File {
  constructor(name: string);
  name: string;
  exists: boolean;
  eof: boolean;
  open(mode: string): boolean;
  close(): void;
  readln(maxlen?: number): string | null;
  iniGetValue(section: string | null, key: string, defaultValue?: any): any;
  iniSetValue(section: string | null, key: string, value: any): boolean;
  iniGetObject(section?: string | null, lowercase?: boolean, blanks?: boolean): any;
}

declare class Frame {
  constructor(x: number, y: number, width: number, height: number, attr?: number, parent?: Frame);
  attr: number;
  width: number;
  height: number;
  transparent: boolean;
  word_wrap: boolean;
  h_scroll: boolean;
  v_scroll: boolean;
  open(): void;
  close(): void;
  cycle(): boolean;
  clear(attr?: number): void;
  refresh(): void;
  gotoxy(x: number, y: number): void;
  putmsg(text: string, attr?: number): void;
  setData(x: number, y: number, ch: string, attr: number, useOffset?: boolean): void;
}

interface AvatarObject {
  disabled?: boolean;
  data?: string;
}

interface AvatarLibrary {
  defs?: {
    width: number;
    height: number;
  };
  read(usernum?: number, username?: string, netaddr?: string | null, bbsid?: string | null): AvatarObject | false | null | undefined;
  read_netuser?(username?: string, netaddr?: string | null): AvatarObject | false | null | undefined;
}

interface ChatNick {
  name: string;
  host?: string;
  ip?: string;
  qwkid?: string;
  avatar?: string;
}

interface ChatPrivateMeta {
  to: ChatNick;
}

interface ChatMessage {
  nick?: ChatNick;
  str: string;
  time: number;
  private?: ChatPrivateMeta;
}

interface ChatUserEntry {
  nick: string | ChatNick;
  system?: string;
  host?: string;
  bbs?: string;
  qwkid?: string;
}

interface ChatChannel {
  name: string;
  messages: ChatMessage[];
  users: ChatUserEntry[];
}

declare class JSONClient {
  constructor(host: string, port: number);
  updates: any[];
  connect(): boolean;
  disconnect(): boolean;
  cycle(): void;
  read(scope: string, location: string, lock?: number): any;
  write(scope: string, location: string, data: any, lock?: number): any;
  push(scope: string, location: string, data: any, lock?: number): any;
  slice(scope: string, location: string, start?: number, end?: number, lock?: number): any[];
}

declare class JSONChat {
  constructor(usernum?: number, jsonclient?: JSONClient, host?: string, port?: number);
  nick: ChatNick;
  client: JSONClient;
  channels: { [name: string]: ChatChannel };
  settings: {
    MAX_HISTORY: number;
  };
  connect(): boolean;
  disconnect(): void;
  join(target: string): void;
  part(target: string): void;
  who(target: string): ChatUserEntry[];
  cycle(): boolean;
  submit(target: string, text: string): boolean;
  getcmd(target: string, text: string): boolean;
  update(packet: any): boolean;
}
