namespace ZZT {
  var WORLD_FILE_HEADER_SIZE_BYTES: number = 512;
  var BOARD_NAME_MAX_LEN: number = 50;
  var BOARD_MESSAGE_MAX_LEN: number = 58;
  var WORLD_NAME_MAX_LEN: number = 20;
  var FLAG_NAME_MAX_LEN: number = 20;
  var BOARD_INFO_PADDING_LEN: number = 16;
  var WORLD_INFO_PADDING_LEN: number = 14;
  var HIGH_SCORE_NAME_MAX_LEN: number = 50;
  var HIGH_SCORE_ENTRY_BYTES: number = HIGH_SCORE_NAME_MAX_LEN + 3;
  var HIGH_SCORE_JSON_VERSION: number = 1;
  var DEFAULT_HIGH_SCORE_JSON_REL_PATH: string = "data/highscores.json";
  var DEFAULT_SAVE_ROOT_REL_PATH: string = "zzt_files/saves";
  var HIGH_SCORE_DISPLAY_NAME_MAX_LEN: number = 34;

  var TileBorder: Tile = createTile(E_NORMAL, 0x0e);
  var TileBoardEdge: Tile = createTile(E_BOARD_EDGE, 0x00);
  var ShowInputDebugOverlay: boolean = false;
  var NeighborDeltaX: number[] = [0, 0, -1, 1];
  var NeighborDeltaY: number[] = [-1, 1, 0, 0];
  var ActiveHighScoreEntries: SharedHighScoreEntry[] = [];

  interface ByteCursor {
    bytes: number[];
    offset: number;
  }

  interface SharedHighScoreEntry {
    player: string;
    bbs: string;
    score: number;
    recordedAt: string;
  }

  interface SharedHighScoreWorldRecord {
    updatedAt: string;
    entries: SharedHighScoreEntry[];
  }

  interface SharedHighScoreStore {
    version: number;
    worlds: { [worldKey: string]: SharedHighScoreWorldRecord };
  }

  export interface Direction {
    DeltaX: number;
    DeltaY: number;
  }

  function randomInt(maxExclusive: number): number {
    if (maxExclusive <= 0) {
      return 0;
    }
    return Math.floor(Math.random() * maxExclusive);
  }

  function currentHsecs(): number {
    var nowMs: number = new Date().getTime();
    return Math.floor(nowMs / 10) % 6000;
  }

  function hasTimeElapsed(counter: number, duration: number): { Elapsed: boolean; Counter: number } {
    var now: number = currentHsecs();
    var diff: number = (now - counter + 6000) % 6000;

    if (diff >= duration) {
      return {
        Elapsed: true,
        Counter: now
      };
    }

    return {
      Elapsed: false,
      Counter: counter
    };
  }

  export function WorldHasTimeElapsed(duration: number): boolean {
    var elapsedCheck: { Elapsed: boolean; Counter: number } = hasTimeElapsed(World.Info.BoardTimeHsec, duration);
    World.Info.BoardTimeHsec = elapsedCheck.Counter;
    return elapsedCheck.Elapsed;
  }

  function sanitizeBoardId(boardId: number): number {
    if (boardId < 0) {
      return 0;
    }
    if (boardId > MAX_BOARD) {
      return MAX_BOARD;
    }
    return boardId;
  }

  function toUint8(value: number): number {
    return value & 0xff;
  }

  function toUint16(value: number): number {
    return value & 0xffff;
  }

  function toInt16(value: number): number {
    var v: number = toUint16(value);
    if (v >= 0x8000) {
      return v - 0x10000;
    }
    return v;
  }

  function appendBytes(target: number[], value: number[]): void {
    var i: number;
    for (i = 0; i < value.length; i += 1) {
      target.push(toUint8(value[i]));
    }
  }

  function pushUInt8(out: number[], value: number): void {
    out.push(toUint8(value));
  }

  function pushUInt16(out: number[], value: number): void {
    var v: number = toUint16(value);
    out.push(v & 0xff);
    out.push((v >> 8) & 0xff);
  }

  function pushInt16(out: number[], value: number): void {
    pushUInt16(out, toUint16(value));
  }

  function readUInt8(cursor: ByteCursor): number {
    if (cursor.offset >= cursor.bytes.length) {
      cursor.offset += 1;
      return 0;
    }
    var value: number = cursor.bytes[cursor.offset] & 0xff;
    cursor.offset += 1;
    return value;
  }

  function readUInt16(cursor: ByteCursor): number {
    var lo: number = readUInt8(cursor);
    var hi: number = readUInt8(cursor);
    return lo | (hi << 8);
  }

  function readInt16(cursor: ByteCursor): number {
    return toInt16(readUInt16(cursor));
  }

  function readBytes(cursor: ByteCursor, count: number): number[] {
    var out: number[] = [];
    var i: number;

    for (i = 0; i < count; i += 1) {
      out.push(readUInt8(cursor));
    }

    return out;
  }

  function byteToChar(value: number): string {
    return String.fromCharCode(toUint8(value));
  }

  function bytesToString(bytes: number[]): string {
    var chars: string[] = [];
    var i: number;
    for (i = 0; i < bytes.length; i += 1) {
      chars.push(byteToChar(bytes[i]));
    }
    return chars.join("");
  }

  function stringToBytes(value: string): number[] {
    var bytes: number[] = [];
    var i: number;

    for (i = 0; i < value.length; i += 1) {
      bytes.push(value.charCodeAt(i) & 0xff);
    }

    return bytes;
  }

  function writePascalString(out: number[], value: string, maxLen: number): void {
    var bytes: number[] = stringToBytes(value);
    var i: number;
    var writtenLen: number = bytes.length;

    if (writtenLen > maxLen) {
      writtenLen = maxLen;
    }

    pushUInt8(out, writtenLen);

    for (i = 0; i < writtenLen; i += 1) {
      pushUInt8(out, bytes[i]);
    }

    for (i = writtenLen; i < maxLen; i += 1) {
      pushUInt8(out, 0);
    }
  }

  function readPascalString(cursor: ByteCursor, maxLen: number): string {
    var len: number = readUInt8(cursor);
    var payload: number[] = readBytes(cursor, maxLen);

    if (len > maxLen) {
      len = maxLen;
    }

    return bytesToString(payload.slice(0, len));
  }

  function writePadding(out: number[], padding: number[], expectedLen: number): void {
    var i: number;

    for (i = 0; i < expectedLen; i += 1) {
      if (i < padding.length) {
        pushUInt8(out, padding[i]);
      } else {
        pushUInt8(out, 0);
      }
    }
  }

  function readPadding(cursor: ByteCursor, len: number): number[] {
    return readBytes(cursor, len);
  }

  function resetHighScoreList(): void {
    var i: number;
    for (i = 1; i <= HIGH_SCORE_COUNT; i += 1) {
      HighScoreList[i].Name = "";
      HighScoreList[i].Score = -1;
    }
  }

  function normalizeScoreText(value: string, maxLen: number): string {
    var cleaned: string = String(value || "").replace(/[\x00-\x1f\x7f]/g, " ");
    if (cleaned.length > maxLen) {
      cleaned = cleaned.slice(0, maxLen);
    }
    return cleaned;
  }

  function trimSpaces(value: string): string {
    return String(value || "").replace(/^\s+/, "").replace(/\s+$/, "");
  }

  function toSafePathPart(value: string, fallback: string): string {
    var safe: string = String(value || "").toLowerCase().replace(/[^a-z0-9._-]+/g, "-");
    safe = safe.replace(/^-+/, "").replace(/-+$/, "");
    if (safe.length <= 0) {
      return fallback;
    }
    return safe;
  }

  function normalizePathCase(path: string): string {
    return normalizeSlashes(path).toLowerCase();
  }

  function isAbsoluteFilePath(path: string): boolean {
    var p: string = String(path || "");
    if (p.length <= 0) {
      return false;
    }
    if (p.charAt(0) === "/" || p.charAt(0) === "\\") {
      return true;
    }
    return p.indexOf(":") >= 0;
  }

  function pathJoin(base: string, rel: string): string {
    var b: string = String(base || "");
    if (b.length > 0) {
      var tail: string = b.charAt(b.length - 1);
      if (tail !== "/" && tail !== "\\") {
        b += "/";
      }
    }
    return b + rel;
  }

  function pathDirname(path: string): string {
    var normalized: string = normalizeSlashes(path);
    var slashPos: number = normalized.lastIndexOf("/");
    if (slashPos < 0) {
      return "";
    }
    return normalized.slice(0, slashPos + 1);
  }

  function ensureDirectory(path: string): boolean {
    if (path.length <= 0) {
      return false;
    }
    if (typeof file_isdir === "function" && file_isdir(path)) {
      return true;
    }
    if (typeof mkpath !== "function") {
      return false;
    }
    try {
      if (!!mkpath(path)) {
        return true;
      }
    } catch (_err) {
      // Ignore and continue with final check.
    }
    return typeof file_isdir === "function" && file_isdir(path);
  }

  function ensureParentDirectory(filePath: string): boolean {
    var parent: string = pathDirname(filePath);
    if (parent.length <= 0) {
      return true;
    }
    return ensureDirectory(parent);
  }

  function currentIsoTimestamp(): string {
    return new Date().toISOString();
  }

  function getCurrentUserName(): string {
    if (typeof user !== "undefined") {
      if (typeof user.alias === "string" && trimSpaces(user.alias).length > 0) {
        return normalizeScoreText(trimSpaces(user.alias), HIGH_SCORE_NAME_MAX_LEN);
      }
      if (typeof user.name === "string" && trimSpaces(user.name).length > 0) {
        return normalizeScoreText(trimSpaces(user.name), HIGH_SCORE_NAME_MAX_LEN);
      }
    }
    return "Player";
  }

  function getCurrentBbsName(): string {
    if (trimSpaces(HighScoreBbsName).length > 0) {
      return normalizeScoreText(trimSpaces(HighScoreBbsName), HIGH_SCORE_NAME_MAX_LEN);
    }
    if (typeof system !== "undefined") {
      if (typeof system.name === "string" && trimSpaces(system.name).length > 0) {
        return normalizeScoreText(trimSpaces(system.name), HIGH_SCORE_NAME_MAX_LEN);
      }
      if (typeof system.qwk_id === "string" && trimSpaces(system.qwk_id).length > 0) {
        return normalizeScoreText(trimSpaces(system.qwk_id), HIGH_SCORE_NAME_MAX_LEN);
      }
      if (typeof system.inet_addr === "string" && trimSpaces(system.inet_addr).length > 0) {
        return normalizeScoreText(trimSpaces(system.inet_addr), HIGH_SCORE_NAME_MAX_LEN);
      }
    }
    return "";
  }

  function buildHighScoreDisplayName(player: string, bbs: string): string {
    var playerClean: string = normalizeScoreText(player, HIGH_SCORE_NAME_MAX_LEN);
    var bbsClean: string = normalizeScoreText(bbs, HIGH_SCORE_NAME_MAX_LEN);
    if (bbsClean.length > 0) {
      return normalizeScoreText(playerClean + " @ " + bbsClean, HIGH_SCORE_NAME_MAX_LEN);
    }
    return playerClean;
  }

  function formatHighScoreDisplayName(entry: SharedHighScoreEntry): string {
    var fullName: string = buildHighScoreDisplayName(entry.player, entry.bbs);
    if (fullName.length > HIGH_SCORE_DISPLAY_NAME_MAX_LEN) {
      return fullName.slice(0, HIGH_SCORE_DISPLAY_NAME_MAX_LEN);
    }
    return fullName;
  }

  function normalizeHighScoreEntry(entry: SharedHighScoreEntry): SharedHighScoreEntry | null {
    var scoreValue: number = Math.floor(entry.score);
    var playerName: string = normalizeScoreText(entry.player, HIGH_SCORE_NAME_MAX_LEN);
    var bbsName: string = normalizeScoreText(entry.bbs, HIGH_SCORE_NAME_MAX_LEN);

    if (playerName.length <= 0) {
      playerName = "Player";
    }
    if (!isFinite(scoreValue) || scoreValue < 0) {
      return null;
    }

    return {
      player: playerName,
      bbs: bbsName,
      score: scoreValue,
      recordedAt: trimSpaces(entry.recordedAt)
    };
  }

  function setActiveHighScoreEntries(entries: SharedHighScoreEntry[]): void {
    var normalized: SharedHighScoreEntry[] = [];
    var i: number;
    var candidate: SharedHighScoreEntry | null;

    for (i = 0; i < entries.length; i += 1) {
      candidate = normalizeHighScoreEntry(entries[i]);
      if (candidate !== null) {
        normalized.push(candidate);
      }
    }

    normalized.sort(function (a: SharedHighScoreEntry, b: SharedHighScoreEntry): number {
      if (a.score > b.score) {
        return -1;
      }
      if (a.score < b.score) {
        return 1;
      }
      if (upperCase(a.player) < upperCase(b.player)) {
        return -1;
      }
      if (upperCase(a.player) > upperCase(b.player)) {
        return 1;
      }
      return 0;
    });

    if (normalized.length > HIGH_SCORE_COUNT) {
      normalized = normalized.slice(0, HIGH_SCORE_COUNT);
    }

    ActiveHighScoreEntries = normalized;
    resetHighScoreList();
    for (i = 0; i < normalized.length; i += 1) {
      HighScoreList[i + 1].Score = normalized[i].score;
      HighScoreList[i + 1].Name = formatHighScoreDisplayName(normalized[i]);
    }
  }

  function getActiveHighScoreEntriesFromList(): SharedHighScoreEntry[] {
    var entries: SharedHighScoreEntry[] = [];
    var i: number;
    var displayName: string;
    var sepPos: number;
    var player: string;
    var bbs: string;

    for (i = 1; i <= HIGH_SCORE_COUNT; i += 1) {
      if (HighScoreList[i].Score < 0) {
        continue;
      }
      displayName = normalizeScoreText(HighScoreList[i].Name, HIGH_SCORE_NAME_MAX_LEN);
      sepPos = displayName.indexOf(" @ ");
      if (sepPos > 0) {
        player = displayName.slice(0, sepPos);
        bbs = displayName.slice(sepPos + 3);
      } else {
        player = displayName;
        bbs = "";
      }

      entries.push({
        player: player,
        bbs: bbs,
        score: HighScoreList[i].Score,
        recordedAt: ""
      });
    }

    return entries;
  }

  function parseHighScoreList(bytes: number[]): boolean {
    var i: number;
    var cursor: ByteCursor;
    var name: string;
    var entries: SharedHighScoreEntry[] = [];

    if (bytes.length < (HIGH_SCORE_COUNT * HIGH_SCORE_ENTRY_BYTES)) {
      return false;
    }

    cursor = {
      bytes: bytes,
      offset: 0
    };

    for (i = 1; i <= HIGH_SCORE_COUNT; i += 1) {
      name = readPascalString(cursor, HIGH_SCORE_NAME_MAX_LEN);
      entries.push({
        player: normalizeScoreText(name, HIGH_SCORE_NAME_MAX_LEN),
        bbs: "",
        score: readInt16(cursor),
        recordedAt: ""
      });
    }

    setActiveHighScoreEntries(entries);
    return true;
  }

  function serializeHighScoreList(): number[] {
    var out: number[] = [];
    var i: number;
    var entries: SharedHighScoreEntry[] = ActiveHighScoreEntries;

    if (entries.length <= 0) {
      entries = getActiveHighScoreEntriesFromList();
    }

    for (i = 1; i <= HIGH_SCORE_COUNT; i += 1) {
      if (i - 1 < entries.length) {
        writePascalString(out, formatHighScoreDisplayName(entries[i - 1]), HIGH_SCORE_NAME_MAX_LEN);
        pushInt16(out, entries[i - 1].score);
      } else {
        writePascalString(out, "", HIGH_SCORE_NAME_MAX_LEN);
        pushInt16(out, -1);
      }
    }

    return out;
  }

  function pushUniquePath(paths: string[], path: string): void {
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

  function getCurrentScoreWorldKey(): string {
    var worldId: string = LoadedGameFileName;
    var worldExt: string = ".ZZT";

    if (worldId.length <= 0 && World.Info.Name.length > 0) {
      worldId = World.Info.Name;
    }
    if (worldId.length <= 0) {
      worldId = "ZZT";
    }

    if (upperCase(worldId.slice(worldId.length - 4)) === ".SAV") {
      worldId = World.Info.Name;
      worldExt = ".ZZT";
    }

    if (worldId.length <= 0) {
      worldId = "ZZT";
    }

    worldId = toWorldIdentifier(worldId, worldExt);
    worldId = stripExtension(worldId, ".ZZT");
    worldId = stripExtension(worldId, ".zzt");
    worldId = normalizePathCase(worldId);
    if (worldId.length <= 0) {
      worldId = "zzt";
    }
    return worldId;
  }

  function resolveHighScoreJsonPath(): string {
    var configured: string = trimSpaces(HighScoreJsonPath);
    if (configured.length <= 0) {
      return execPath(DEFAULT_HIGH_SCORE_JSON_REL_PATH);
    }
    if (isAbsoluteFilePath(configured)) {
      return configured;
    }
    return execPath(configured);
  }

  function readSharedHighScoreStore(): SharedHighScoreStore {
    var path: string = resolveHighScoreJsonPath();
    var bytes: number[] | null = runtime.readBinaryFile(path);
    var emptyStore: SharedHighScoreStore = {
      version: HIGH_SCORE_JSON_VERSION,
      worlds: {}
    };
    var text: string;
    var parsed: unknown;
    var parsedObj: { version?: unknown; worlds?: unknown };
    var worldsObj: { [key: string]: unknown };
    var worldKey: string;
    var worldValue: { updatedAt?: unknown; entries?: unknown };
    var rawEntries: unknown[];
    var entry: { player?: unknown; bbs?: unknown; score?: unknown; recordedAt?: unknown };
    var normalizedEntries: SharedHighScoreEntry[];
    var i: number;
    var normalizedWorldKey: string;

    if (bytes === null || bytes.length <= 0) {
      return emptyStore;
    }

    text = trimSpaces(bytesToString(bytes));
    if (text.length <= 0) {
      return emptyStore;
    }

    try {
      parsed = JSON.parse(text);
    } catch (_err) {
      return emptyStore;
    }

    if (typeof parsed !== "object" || parsed === null) {
      return emptyStore;
    }
    parsedObj = parsed as { version?: unknown; worlds?: unknown };
    if (typeof parsedObj.worlds !== "object" || parsedObj.worlds === null) {
      return emptyStore;
    }

    worldsObj = parsedObj.worlds as { [key: string]: unknown };
    for (worldKey in worldsObj) {
      if (!Object.prototype.hasOwnProperty.call(worldsObj, worldKey)) {
        continue;
      }

      worldValue = worldsObj[worldKey] as { updatedAt?: unknown; entries?: unknown };
      if (typeof worldValue !== "object" || worldValue === null) {
        continue;
      }
      if (!Array.isArray(worldValue.entries)) {
        continue;
      }

      rawEntries = worldValue.entries as unknown[];
      normalizedEntries = [];
      for (i = 0; i < rawEntries.length; i += 1) {
        entry = rawEntries[i] as { player?: unknown; bbs?: unknown; score?: unknown; recordedAt?: unknown };
        if (typeof entry !== "object" || entry === null) {
          continue;
        }
        normalizedEntries.push({
          player: typeof entry.player === "string" ? entry.player : "",
          bbs: typeof entry.bbs === "string" ? entry.bbs : "",
          score: typeof entry.score === "number" ? entry.score : -1,
          recordedAt: typeof entry.recordedAt === "string" ? entry.recordedAt : ""
        });
      }

      normalizedWorldKey = normalizePathCase(worldKey);
      emptyStore.worlds[normalizedWorldKey] = {
        updatedAt: typeof worldValue.updatedAt === "string" ? worldValue.updatedAt : "",
        entries: normalizedEntries
      };
    }

    return emptyStore;
  }

  function writeSharedHighScoreStore(store: SharedHighScoreStore): boolean {
    var path: string = resolveHighScoreJsonPath();
    var payload: string = JSON.stringify(store, null, 2) + "\n";

    if (!ensureParentDirectory(path)) {
      return false;
    }
    return runtime.writeBinaryFile(path, stringToBytes(payload));
  }

  function resolveSaveRootPath(): string {
    var configured: string = trimSpaces(SaveRootPath);
    if (configured.length <= 0) {
      return execPath(DEFAULT_SAVE_ROOT_REL_PATH);
    }
    if (isAbsoluteFilePath(configured)) {
      return configured;
    }
    return execPath(configured);
  }

  function currentUserSaveKey(): string {
    var userNumber: string = "";
    var userName: string = getCurrentUserName();

    if (typeof user !== "undefined" &&
        typeof user.number === "number" &&
        isFinite(user.number) &&
        user.number > 0) {
      userNumber = String(Math.floor(user.number));
    }

    if (userNumber.length > 0) {
      return "u" + toSafePathPart(userNumber + "-" + userName, "user");
    }
    return "u" + toSafePathPart(userName, "user");
  }

  function currentUserSaveDir(): string {
    return pathJoin(resolveSaveRootPath(), currentUserSaveKey() + "/");
  }

  export function BuildDefaultSaveFileName(): string {
    var base: string;
    var value: string;

    if (typeof user !== "undefined" &&
        typeof user.number === "number" &&
        isFinite(user.number) &&
        user.number > 0) {
      return ("S" + String(Math.floor(user.number))).slice(0, 8);
    }

    base = getCurrentUserName().toUpperCase().replace(/[^A-Z0-9]+/g, "");
    if (base.length <= 0) {
      return "SAVED";
    }

    value = "S" + base;
    if (value.length > 8) {
      value = value.slice(0, 8);
    }
    return value;
  }

  function getHighScorePathCandidates(): string[] {
    var paths: string[] = [];
    var baseName: string = LoadedGameFileName.length > 0 ? LoadedGameFileName : World.Info.Name;
    var hasPathSep: boolean =
      baseName.indexOf("/") >= 0 || baseName.indexOf("\\") >= 0;

    if (upperCase(baseName.slice(baseName.length - 4)) === ".SAV" && World.Info.Name.length > 0) {
      baseName = World.Info.Name;
      hasPathSep = false;
    }

    function addHiVariants(pathBase: string): void {
      pushUniquePath(paths, pathBase + ".HI");
      pushUniquePath(paths, pathBase + ".hi");
    }

    if (baseName.length > 0) {
      if (hasPathSep) {
        addHiVariants(execPath(baseName));
        addHiVariants(execPath("../" + baseName));
        addHiVariants(baseName);
      } else {
        addHiVariants(execPath("zzt_files/" + baseName));
        addHiVariants(execPath("../zzt_files/" + baseName));
        addHiVariants(execPath(baseName));
        addHiVariants(baseName);
      }
    }

    if (World.Info.Name.length > 0) {
      addHiVariants(execPath(World.Info.Name));
      addHiVariants(World.Info.Name);
    }

    return paths;
  }

  function writeBoardInfo(out: number[], info: BoardInfo): void {
    var i: number;

    pushUInt8(out, info.MaxShots);
    pushUInt8(out, info.IsDark ? 1 : 0);

    for (i = 0; i < 4; i += 1) {
      if (i < info.NeighborBoards.length) {
        pushUInt8(out, info.NeighborBoards[i]);
      } else {
        pushUInt8(out, 0);
      }
    }

    pushUInt8(out, info.ReenterWhenZapped ? 1 : 0);
    writePascalString(out, info.Message, BOARD_MESSAGE_MAX_LEN);
    pushUInt8(out, info.StartPlayerX);
    pushUInt8(out, info.StartPlayerY);
    pushInt16(out, info.TimeLimitSec);
    writePadding(out, info.UnknownPadding, BOARD_INFO_PADDING_LEN);
  }

  function readBoardInfo(cursor: ByteCursor): BoardInfo {
    var info: BoardInfo = createBoardInfo();
    var i: number;

    info.MaxShots = readUInt8(cursor);
    info.IsDark = readUInt8(cursor) !== 0;

    for (i = 0; i < 4; i += 1) {
      info.NeighborBoards[i] = readUInt8(cursor);
    }

    info.ReenterWhenZapped = readUInt8(cursor) !== 0;
    info.Message = readPascalString(cursor, BOARD_MESSAGE_MAX_LEN);
    info.StartPlayerX = readUInt8(cursor);
    info.StartPlayerY = readUInt8(cursor);
    info.TimeLimitSec = readInt16(cursor);
    info.UnknownPadding = readPadding(cursor, BOARD_INFO_PADDING_LEN);

    return info;
  }

  function writeWorldInfo(out: number[], info: WorldInfo): void {
    var i: number;

    pushInt16(out, info.Ammo);
    pushInt16(out, info.Gems);

    for (i = 1; i <= 7; i += 1) {
      pushUInt8(out, info.Keys[i] ? 1 : 0);
    }

    pushInt16(out, info.Health);
    pushInt16(out, info.CurrentBoard);
    pushInt16(out, info.Torches);
    pushInt16(out, info.TorchTicks);
    pushInt16(out, info.EnergizerTicks);
    pushInt16(out, info.Unknown1);
    pushInt16(out, info.Score);

    writePascalString(out, info.Name, WORLD_NAME_MAX_LEN);
    for (i = 1; i <= MAX_FLAG; i += 1) {
      writePascalString(out, info.Flags[i], FLAG_NAME_MAX_LEN);
    }

    pushInt16(out, info.BoardTimeSec);
    pushInt16(out, info.BoardTimeHsec);
    pushUInt8(out, info.IsSave ? 1 : 0);
    writePadding(out, info.UnknownPadding, WORLD_INFO_PADDING_LEN);
  }

  function readWorldInfo(cursor: ByteCursor): WorldInfo {
    var info: WorldInfo = createWorldInfo();
    var i: number;

    info.Ammo = readInt16(cursor);
    info.Gems = readInt16(cursor);

    for (i = 1; i <= 7; i += 1) {
      info.Keys[i] = readUInt8(cursor) !== 0;
    }

    info.Health = readInt16(cursor);
    info.CurrentBoard = readInt16(cursor);
    info.Torches = readInt16(cursor);
    info.TorchTicks = readInt16(cursor);
    info.EnergizerTicks = readInt16(cursor);
    info.Unknown1 = readInt16(cursor);
    info.Score = readInt16(cursor);

    info.Name = readPascalString(cursor, WORLD_NAME_MAX_LEN);
    for (i = 1; i <= MAX_FLAG; i += 1) {
      info.Flags[i] = readPascalString(cursor, FLAG_NAME_MAX_LEN);
    }

    info.BoardTimeSec = readInt16(cursor);
    info.BoardTimeHsec = readInt16(cursor);
    info.IsSave = readUInt8(cursor) !== 0;
    info.UnknownPadding = readPadding(cursor, WORLD_INFO_PADDING_LEN);

    return info;
  }

  function writeStatRecord(out: number[], stat: Stat, encodedDataLen: number): void {
    pushUInt8(out, stat.X);
    pushUInt8(out, stat.Y);
    pushInt16(out, stat.StepX);
    pushInt16(out, stat.StepY);
    pushInt16(out, stat.Cycle);
    pushUInt8(out, stat.P1);
    pushUInt8(out, stat.P2);
    pushUInt8(out, stat.P3);
    pushInt16(out, stat.Follower);
    pushInt16(out, stat.Leader);
    pushUInt8(out, stat.Under.Element);
    pushUInt8(out, stat.Under.Color);

    // Data pointer (4 bytes in 16-bit large model)
    pushUInt16(out, 0);
    pushUInt16(out, 0);

    pushInt16(out, stat.DataPos);
    pushInt16(out, encodedDataLen);

    // unk1 pointer (4 bytes)
    pushUInt16(out, 0);
    pushUInt16(out, 0);

    // unk2 pointer (4 bytes)
    pushUInt16(out, 0);
    pushUInt16(out, 0);
  }

  function readStatRecord(cursor: ByteCursor): Stat {
    var stat: Stat = createDefaultStat();

    stat.X = readUInt8(cursor);
    stat.Y = readUInt8(cursor);
    stat.StepX = readInt16(cursor);
    stat.StepY = readInt16(cursor);
    stat.Cycle = readInt16(cursor);
    stat.P1 = readUInt8(cursor);
    stat.P2 = readUInt8(cursor);
    stat.P3 = readUInt8(cursor);
    stat.Follower = readInt16(cursor);
    stat.Leader = readInt16(cursor);
    stat.Under = createTile(readUInt8(cursor), readUInt8(cursor));

    // Data pointer (ignored)
    readUInt16(cursor);
    readUInt16(cursor);

    stat.DataPos = readInt16(cursor);
    stat.DataLen = readInt16(cursor);

    // unk1/unk2 pointers (ignored)
    readUInt16(cursor);
    readUInt16(cursor);
    readUInt16(cursor);
    readUInt16(cursor);

    return stat;
  }

  function fixedLengthDataBytes(data: string | null, len: number): number[] {
    var source: number[] = data !== null ? stringToBytes(data) : [];
    var output: number[] = [];
    var i: number;

    for (i = 0; i < len; i += 1) {
      if (i < source.length) {
        output.push(source[i]);
      } else {
        output.push(0);
      }
    }

    return output;
  }

  function applyBoardOuterEdges(board: Board): void {
    var ix: number;
    var iy: number;

    for (ix = 0; ix <= BOARD_WIDTH + 1; ix += 1) {
      board.Tiles[ix][0] = cloneTile(TileBoardEdge);
      board.Tiles[ix][BOARD_HEIGHT + 1] = cloneTile(TileBoardEdge);
    }

    for (iy = 0; iy <= BOARD_HEIGHT + 1; iy += 1) {
      board.Tiles[0][iy] = cloneTile(TileBoardEdge);
      board.Tiles[BOARD_WIDTH + 1][iy] = cloneTile(TileBoardEdge);
    }
  }

  function serializeBoard(board: Board): number[] {
    var out: number[] = [];
    var ix: number;
    var iy: number;
    var runCount: number;
    var runTile: Tile;
    var statId: number;
    var sharedDataStatId: number;
    var encodedDataLen: number;

    writePascalString(out, board.Name, BOARD_NAME_MAX_LEN);

    ix = 1;
    iy = 1;
    runCount = 1;
    runTile = cloneTile(board.Tiles[ix][iy]);

    while (true) {
      ix += 1;
      if (ix > BOARD_WIDTH) {
        ix = 1;
        iy += 1;
      }

      if (
        iy <= BOARD_HEIGHT &&
        runCount < 255 &&
        board.Tiles[ix][iy].Element === runTile.Element &&
        board.Tiles[ix][iy].Color === runTile.Color
      ) {
        runCount += 1;
      } else {
        pushUInt8(out, runCount);
        pushUInt8(out, runTile.Element);
        pushUInt8(out, runTile.Color);

        if (iy > BOARD_HEIGHT) {
          break;
        }

        runTile = cloneTile(board.Tiles[ix][iy]);
        runCount = 1;
      }
    }

    writeBoardInfo(out, board.Info);
    pushInt16(out, board.StatCount);

    for (statId = 0; statId <= board.StatCount; statId += 1) {
      var stat: Stat = board.Stats[statId];
      encodedDataLen = stat.DataLen;

      if (encodedDataLen > 0) {
        for (sharedDataStatId = 1; sharedDataStatId <= statId - 1; sharedDataStatId += 1) {
          if (board.Stats[sharedDataStatId].DataLen > 0 && board.Stats[sharedDataStatId].Data === stat.Data) {
            encodedDataLen = -sharedDataStatId;
            break;
          }
        }
      }

      writeStatRecord(out, stat, encodedDataLen);
      if (encodedDataLen > 0) {
        appendBytes(out, fixedLengthDataBytes(stat.Data, encodedDataLen));
      }
    }

    return out;
  }

  function deserializeBoard(bytes: number[]): Board {
    var cursor: ByteCursor = {
      bytes: bytes,
      offset: 0
    };
    var board: Board = createBoard();
    var ix: number;
    var iy: number;
    var rleCount: number = 0;
    var rleElement: number = E_EMPTY;
    var rleColor: number = 0;
    var statId: number;

    applyBoardOuterEdges(board);

    board.Name = readPascalString(cursor, BOARD_NAME_MAX_LEN);

    ix = 1;
    iy = 1;
    while (iy <= BOARD_HEIGHT) {
      if (rleCount <= 0) {
        rleCount = readUInt8(cursor);
        if (rleCount <= 0) {
          // DOS ZZT compatible decode: a stored count of 0 means 256 tiles.
          rleCount = 256;
        }
        rleElement = readUInt8(cursor);
        rleColor = readUInt8(cursor);
      }

      board.Tiles[ix][iy] = createTile(rleElement, rleColor);

      ix += 1;
      if (ix > BOARD_WIDTH) {
        ix = 1;
        iy += 1;
      }

      rleCount -= 1;
    }

    board.Info = readBoardInfo(cursor);
    board.StatCount = readInt16(cursor);
    if (board.StatCount < 0) {
      board.StatCount = 0;
    }
    if (board.StatCount > MAX_STAT) {
      board.StatCount = MAX_STAT;
    }

    for (statId = 0; statId <= board.StatCount; statId += 1) {
      var stat: Stat = readStatRecord(cursor);

      if (stat.DataLen > 0) {
        stat.Data = bytesToString(readBytes(cursor, stat.DataLen));
      } else if (stat.DataLen < 0) {
        var sourceStatId: number = -stat.DataLen;
        if (sourceStatId >= 0 && sourceStatId < statId) {
          stat.Data = board.Stats[sourceStatId].Data;
          stat.DataLen = board.Stats[sourceStatId].DataLen;
        } else {
          stat.Data = null;
          stat.DataLen = 0;
        }
      } else {
        stat.Data = null;
      }

      board.Stats[statId] = stat;
    }

    return board;
  }

  function fileExists(path: string): boolean {
    var f: SyncFile;
    if (typeof File === "undefined") {
      return false;
    }
    try {
      f = new File(path);
      return f.exists;
    } catch (_err) {
      return false;
    }
  }

  function getExtensionVariants(extension: string): string[] {
    var variants: string[] = [];
    var seen: { [id: string]: boolean } = {};

    function pushVariant(ext: string): void {
      var id: string = upperCase(ext);
      if (seen[id] === true) {
        return;
      }
      seen[id] = true;
      variants.push(ext);
    }

    pushVariant(extension);
    pushVariant(extension.toLowerCase());
    pushVariant(extension.toUpperCase());
    return variants;
  }

  function getFullNameVariants(filename: string, extension: string): string[] {
    var names: string[] = [];
    var extVariants: string[] = getExtensionVariants(extension);
    var i: number;

    for (i = 0; i < extVariants.length; i += 1) {
      pushUniquePath(names, filename + extVariants[i]);
    }
    return names;
  }

  function resolveWorldPath(filename: string, extension: string): string {
    var fullNames: string[] = getFullNameVariants(filename, extension);
    var fullName: string = fullNames.length > 0 ? fullNames[0] : (filename + extension);
    var i: number;
    var sharedPath: string;
    var zztFilesPrimary: string;
    var primary: string;
    var resFallback: string;
    var hasPathSep: boolean =
      filename.indexOf("/") >= 0 || filename.indexOf("\\") >= 0;
    var isAbsolutePath: boolean = isAbsoluteFilePath(filename);
    var savePath: string;

    if (hasPathSep || isAbsolutePath) {
      if (isAbsolutePath) {
        for (i = 0; i < fullNames.length; i += 1) {
          if (fileExists(fullNames[i])) {
            return fullNames[i];
          }
        }
        return fullName;
      }

      for (i = 0; i < fullNames.length; i += 1) {
        primary = execPath(fullNames[i]);
        if (fileExists(primary)) {
          return primary;
        }

        sharedPath = execPath("../" + fullNames[i]);
        if (fileExists(sharedPath)) {
          return sharedPath;
        }
      }

      return execPath(fullName);
    }

    if (upperCase(extension) === ".SAV") {
      for (i = 0; i < fullNames.length; i += 1) {
        savePath = pathJoin(currentUserSaveDir(), fullNames[i]);
        if (fileExists(savePath)) {
          return savePath;
        }

        zztFilesPrimary = execPath("zzt_files/" + fullNames[i]);
        if (fileExists(zztFilesPrimary)) {
          return zztFilesPrimary;
        }

        primary = execPath(fullNames[i]);
        if (fileExists(primary)) {
          return primary;
        }
      }
      return pathJoin(currentUserSaveDir(), fullName);
    }

    for (i = 0; i < fullNames.length; i += 1) {
      zztFilesPrimary = execPath("zzt_files/" + fullNames[i]);
      if (fileExists(zztFilesPrimary)) {
        return zztFilesPrimary;
      }

      sharedPath = execPath("../zzt_files/" + fullNames[i]);
      if (fileExists(sharedPath)) {
        return sharedPath;
      }

      primary = execPath(fullNames[i]);
      if (fileExists(primary)) {
        return primary;
      }

      resFallback = execPath("reconstruction-of-zzt-master/RES/" + fullNames[i]);
      if (fileExists(resFallback)) {
        return resFallback;
      }
    }

    zztFilesPrimary = execPath("zzt_files/" + fullName);
    return zztFilesPrimary;
  }

  export function GenerateTransitionTable(): void {
    var ix: number;
    var iy: number;
    var swapIx: number;
    var temp: Coord;

    TransitionTable = [];
    for (iy = 1; iy <= BOARD_HEIGHT; iy += 1) {
      for (ix = 1; ix <= BOARD_WIDTH; ix += 1) {
        TransitionTable.push({
          X: ix,
          Y: iy
        });
      }
    }

    TransitionTableSize = TransitionTable.length;

    for (ix = 0; ix < TransitionTableSize; ix += 1) {
      swapIx = randomInt(TransitionTableSize);
      temp = TransitionTable[swapIx];
      TransitionTable[swapIx] = TransitionTable[ix];
      TransitionTable[ix] = temp;
    }
  }

  export function Signum(val: number): number {
    if (val > 0) {
      return 1;
    }
    if (val < 0) {
      return -1;
    }
    return 0;
  }

  export function Difference(a: number, b: number): number {
    if ((a - b) >= 0) {
      return a - b;
    }
    return b - a;
  }

  export function CalcDirectionRnd(): Direction {
    var deltaX: number = randomInt(3) - 1;
    var deltaY: number = 0;

    if (deltaX === 0) {
      deltaY = randomInt(2) * 2 - 1;
    }

    return {
      DeltaX: deltaX,
      DeltaY: deltaY
    };
  }

  export function CalcDirectionSeek(x: number, y: number): Direction {
    var deltaX: number = 0;
    var deltaY: number = 0;
    var playerX: number = Board.Stats[0].X;
    var playerY: number = Board.Stats[0].Y;

    if (randomInt(2) < 1 || playerY === y) {
      deltaX = Signum(playerX - x);
    }

    if (deltaX === 0) {
      deltaY = Signum(playerY - y);
    }

    if (World.Info.EnergizerTicks > 0) {
      deltaX = -deltaX;
      deltaY = -deltaY;
    }

    return {
      DeltaX: deltaX,
      DeltaY: deltaY
    };
  }

  export function BoardClose(): void {
    var boardId: number = sanitizeBoardId(World.Info.CurrentBoard);
    var serialized: number[] = serializeBoard(Board);

    World.BoardData[boardId] = serialized;
    World.BoardLen[boardId] = serialized.length;

    if (boardId > World.BoardCount) {
      World.BoardCount = boardId;
    }
  }

  export function BoardOpen(boardId: number): void {
    var normalizedId: number = sanitizeBoardId(boardId);
    var packedBoard: number[] | null;

    if (normalizedId > World.BoardCount) {
      normalizedId = World.Info.CurrentBoard;
    }

    normalizedId = sanitizeBoardId(normalizedId);
    packedBoard = World.BoardData[normalizedId];

    if (packedBoard !== null) {
      Board = deserializeBoard(packedBoard);
    } else {
      BoardCreate();
    }

    World.Info.CurrentBoard = normalizedId;
  }

  export function BoardChange(boardId: number): void {
    var px: number = Board.Stats[0].X;
    var py: number = Board.Stats[0].Y;

    if (px >= 0 && px <= BOARD_WIDTH + 1 && py >= 0 && py <= BOARD_HEIGHT + 1) {
      Board.Tiles[px][py].Element = E_PLAYER;
      Board.Tiles[px][py].Color = ElementDefs[E_PLAYER].Color;
    }

    BoardClose();
    BoardOpen(boardId);
  }

  export function BoardCreate(): void {
    var ix: number;
    var iy: number;
    var i: number;
    var centerX: number;
    var centerY: number;

    Board = createBoard();
    Board.Name = "";
    Board.Info.Message = "";
    Board.Info.MaxShots = 255;
    Board.Info.IsDark = false;
    Board.Info.ReenterWhenZapped = false;
    Board.Info.TimeLimitSec = 0;

    for (i = 0; i <= 3; i += 1) {
      Board.Info.NeighborBoards[i] = 0;
    }

    applyBoardOuterEdges(Board);

    for (ix = 1; ix <= BOARD_WIDTH; ix += 1) {
      for (iy = 1; iy <= BOARD_HEIGHT; iy += 1) {
        Board.Tiles[ix][iy] = createTile(E_EMPTY, 0);
      }
    }

    for (ix = 1; ix <= BOARD_WIDTH; ix += 1) {
      Board.Tiles[ix][1] = cloneTile(TileBorder);
      Board.Tiles[ix][BOARD_HEIGHT] = cloneTile(TileBorder);
    }

    for (iy = 1; iy <= BOARD_HEIGHT; iy += 1) {
      Board.Tiles[1][iy] = cloneTile(TileBorder);
      Board.Tiles[BOARD_WIDTH][iy] = cloneTile(TileBorder);
    }

    centerX = Math.floor(BOARD_WIDTH / 2);
    centerY = Math.floor(BOARD_HEIGHT / 2);
    Board.Tiles[centerX][centerY].Element = E_PLAYER;
    Board.Tiles[centerX][centerY].Color = ElementDefs[E_PLAYER].Color;

    Board.StatCount = 0;
    Board.Stats[0] = createDefaultStat();
    Board.Stats[0].X = centerX;
    Board.Stats[0].Y = centerY;
    Board.Stats[0].Cycle = 1;
    Board.Stats[0].Under = createTile(E_EMPTY, 0);
    Board.Stats[0].Data = null;
    Board.Stats[0].DataLen = 0;
  }

  export function WorldUnload(): void {
    var i: number;

    BoardClose();
    for (i = 0; i <= MAX_BOARD; i += 1) {
      World.BoardData[i] = null;
      World.BoardLen[i] = 0;
    }
    World.BoardCount = 0;
  }

  export function WorldLoad(filename: string, extension: string, titleOnly: boolean): boolean {
    var path: string = resolveWorldPath(filename, extension);
    var bytes: number[] | null = runtime.readBinaryFile(path);
    var cursor: ByteCursor;
    var boardCount: number;
    var boardId: number;
    var headerValue: number;
    var worldInfo: WorldInfo;
    var boardLen: number;

    if (bytes === null || bytes.length < 4) {
      return false;
    }

    cursor = {
      bytes: bytes,
      offset: 0
    };

    headerValue = readInt16(cursor);
    if (headerValue < 0) {
      if (headerValue !== -1) {
        return false;
      }
      boardCount = readInt16(cursor);
    } else {
      boardCount = headerValue;
    }

    worldInfo = readWorldInfo(cursor);

    if (titleOnly) {
      boardCount = 0;
      worldInfo.CurrentBoard = 0;
      worldInfo.IsSave = true;
    }

    if (boardCount < 0) {
      boardCount = 0;
    }
    if (boardCount > MAX_BOARD) {
      boardCount = MAX_BOARD;
    }

    World = createWorld();
    World.BoardCount = boardCount;
    World.Info = cloneWorldInfo(worldInfo);
    cursor.offset = WORLD_FILE_HEADER_SIZE_BYTES;

    for (boardId = 0; boardId <= World.BoardCount; boardId += 1) {
      boardLen = readUInt16(cursor);
      World.BoardLen[boardId] = boardLen;
      World.BoardData[boardId] = readBytes(cursor, boardLen);
    }

    BoardOpen(World.Info.CurrentBoard);
    LoadedGameFileName = filename;
    SoundWorldMusicConfigureFromWorldFile(path);
    if (!SoundWorldMusicHasTracks() &&
        upperCase(extension) === ".SAV" &&
        World.Info.Name.length > 0) {
      SoundWorldMusicConfigureFromWorldFile(resolveWorldPath(World.Info.Name, ".ZZT"));
    }
    HighScoresLoad();
    return true;
  }

  export function WorldSave(filename: string, extension: string): boolean {
    var path: string = resolveWorldPath(filename, extension);
    var currentBoardId: number = World.Info.CurrentBoard;
    var header: number[] = [];
    var fileBytes: number[] = [];
    var boardId: number;
    var boardData: number[];
    var boardDataMaybeNull: number[] | null;

    BoardClose();

    pushInt16(header, -1);
    pushInt16(header, World.BoardCount);
    writeWorldInfo(header, World.Info);

    while (header.length < WORLD_FILE_HEADER_SIZE_BYTES) {
      header.push(0);
    }
    if (header.length > WORLD_FILE_HEADER_SIZE_BYTES) {
      header = header.slice(0, WORLD_FILE_HEADER_SIZE_BYTES);
    }

    appendBytes(fileBytes, header);

    for (boardId = 0; boardId <= World.BoardCount; boardId += 1) {
      boardDataMaybeNull = World.BoardData[boardId];
      if (boardDataMaybeNull === null) {
        boardData = [];
      } else {
        boardData = boardDataMaybeNull;
      }

      World.BoardLen[boardId] = boardData.length;
      pushUInt16(fileBytes, boardData.length);
      appendBytes(fileBytes, boardData);
    }

    if (!ensureParentDirectory(path)) {
      BoardOpen(currentBoardId);
      return false;
    }

    var result: boolean = runtime.writeBinaryFile(path, fileBytes);
    BoardOpen(currentBoardId);
    return result;
  }

  export function WorldCreate(): void {
    var i: number;

    InitElementsGame();
    World = createWorld();
    World.BoardCount = 0;
    World.BoardLen[0] = 0;

    InitEditorStatSettings();
    ResetMessageNotShownFlags();
    BoardCreate();

    World.Info.IsSave = false;
    World.Info.CurrentBoard = 0;
    World.Info.Ammo = 0;
    World.Info.Gems = 0;
    World.Info.Health = 100;
    World.Info.EnergizerTicks = 0;
    World.Info.Torches = 0;
    World.Info.TorchTicks = 0;
    World.Info.Score = 0;
    World.Info.BoardTimeSec = 0;
    World.Info.BoardTimeHsec = 0;

    for (i = 1; i <= 7; i += 1) {
      World.Info.Keys[i] = false;
    }
    for (i = 1; i <= MAX_FLAG; i += 1) {
      World.Info.Flags[i] = "";
    }

    BoardChange(0);
    Board.Name = "Title screen";
    LoadedGameFileName = "";
    World.Info.Name = "";
    SoundWorldMusicConfigureFromWorldFile("");
  }

  function repeatChar(ch: string, count: number): string {
    var out: string = "";
    var i: number;
    for (i = 0; i < count; i += 1) {
      out += ch;
    }
    return out;
  }

  function padRight(value: string, width: number): string {
    var text: string = value;
    if (text.length > width) {
      return text.slice(0, width);
    }
    while (text.length < width) {
      text += " ";
    }
    return text;
  }

  function padLeft(value: string, width: number): string {
    var text: string = value;
    if (text.length > width) {
      return text.slice(text.length - width);
    }
    while (text.length < width) {
      text = " " + text;
    }
    return text;
  }

  function upperCase(value: string): string {
    return value.toUpperCase();
  }

  function toHex2(value: number): string {
    var hex: string = (value & 0xff).toString(16).toUpperCase();
    if (hex.length < 2) {
      hex = "0" + hex;
    }
    return hex;
  }

  function debugKeyLabel(key: string): string {
    var code: number;
    if (key.length <= 0) {
      return "00";
    }
    code = key.charCodeAt(0) & 0xff;
    if (code >= 32 && code <= 126) {
      return key.charAt(0);
    }
    return "$" + toHex2(code);
  }

  function drawDebugStatusLine(): void {
    var mode: string = GameStateElement === E_MONITOR ? "MON" : "PLY";
    var keyLabel: string = debugKeyLabel(InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0) : "");
    var deltaLabel: string = String(InputDeltaX) + "," + String(InputDeltaY);
    var playerCycle: number = (Board.Stats.length > 0 ? Board.Stats[0].Cycle : 0);
    var text: string =
      "DBG " + mode +
      " K:" + keyLabel +
      " D:" + deltaLabel +
      " C:" + String(playerCycle) +
      " P:" + (GamePaused ? "1" : "0") +
      " T:" + String(CurrentTick) +
      " S:" + String(CurrentStatTicked) + "/" + String(Board.StatCount);

    SidebarClearLine(24);
    VideoWriteText(60, 24, 0x1e, padRight(text, 19));
  }

  function invokeElementTouch(x: number, y: number, sourceStatId: number, context: TouchContext): void {
    var element: number = Board.Tiles[x][y].Element;
    var def: ElementDef = ElementDefs[element];
    if (def.TouchProc !== null) {
      def.TouchProc(x, y, sourceStatId, context);
    }
  }

  function tryApplyPlayerDirectionalInput(): void {
    var stat: Stat;
    var context: TouchContext;
    var desiredDeltaX: number;
    var desiredDeltaY: number;

    if (World.Info.Health <= 0) {
      InputDeltaX = 0;
      InputDeltaY = 0;
      return;
    }

    if (InputDeltaX === 0 && InputDeltaY === 0) {
      return;
    }
    if (Board.StatCount < 0) {
      return;
    }

    desiredDeltaX = InputDeltaX;
    desiredDeltaY = InputDeltaY;

    PlayerDirX = desiredDeltaX;
    PlayerDirY = desiredDeltaY;

    stat = Board.Stats[0];
    context = {
      DeltaX: desiredDeltaX,
      DeltaY: desiredDeltaY
    };

    invokeElementTouch(stat.X + context.DeltaX, stat.Y + context.DeltaY, 0, context);
    if ((context.DeltaX !== 0 || context.DeltaY !== 0) &&
        ElementDefs[Board.Tiles[stat.X + context.DeltaX][stat.Y + context.DeltaY].Element].Walkable) {
      MovePlayerStat(stat.X + context.DeltaX, stat.Y + context.DeltaY);
    }

    // Consume one directional input event so movement doesn't repeat uncontrollably.
    InputDeltaX = 0;
    InputDeltaY = 0;
  }

  function getElementDrawChar(x: number, y: number): string {
    var tile: Tile = Board.Tiles[x][y];
    var def: ElementDef = ElementDefs[tile.Element];
    var drawValue: number;

    if (def.HasDrawProc && def.DrawProc !== null) {
      drawValue = def.DrawProc(x, y);
      return String.fromCharCode(drawValue & 0xff);
    }

    if (tile.Element < E_TEXT_MIN) {
      if (def.Character.length > 0) {
        return def.Character.charAt(0);
      }
      return "?";
    }

    return String.fromCharCode(tile.Color & 0xff);
  }

  function getTileDrawColor(x: number, y: number): number {
    var tile: Tile = Board.Tiles[x][y];

    if (tile.Element === E_EMPTY) {
      return 0x0f;
    }

    if (tile.Element < E_TEXT_MIN) {
      return tile.Color;
    }

    if (tile.Element === E_TEXT_WHITE) {
      return 0x0f;
    }

    return (((tile.Element - E_TEXT_MIN) + 1) * 16) + 0x0f;
  }

  export function BoardDrawTile(x: number, y: number): void {
    var tile: Tile = Board.Tiles[x][y];
    var visibleInDark: boolean = ElementDefs[tile.Element].VisibleInDark;
    var litByTorch: boolean = World.Info.TorchTicks > 0 &&
      ((Board.Stats[0].X - x) * (Board.Stats[0].X - x) + (Board.Stats[0].Y - y) * (Board.Stats[0].Y - y) * 2) < TORCH_DIST_SQR;

    if (Board.Info.IsDark && !ForceDarknessOff && !visibleInDark && !litByTorch) {
      VideoWriteText(x - 1, y - 1, 0x07, String.fromCharCode(176));
      return;
    }

    VideoWriteText(x - 1, y - 1, getTileDrawColor(x, y), getElementDrawChar(x, y));
  }

  export function BoardDrawBorder(): void {
    var ix: number;
    var iy: number;

    for (ix = 1; ix <= BOARD_WIDTH; ix += 1) {
      BoardDrawTile(ix, 1);
      BoardDrawTile(ix, BOARD_HEIGHT);
    }

    for (iy = 1; iy <= BOARD_HEIGHT; iy += 1) {
      BoardDrawTile(1, iy);
      BoardDrawTile(BOARD_WIDTH, iy);
    }
  }

  export function TransitionDrawToBoard(): void {
    var ix: number;
    var iy: number;

    BoardDrawBorder();
    for (iy = 1; iy <= BOARD_HEIGHT; iy += 1) {
      for (ix = 1; ix <= BOARD_WIDTH; ix += 1) {
        BoardDrawTile(ix, iy);
      }
    }
  }

  export function TransitionDrawToFill(chr: string, color: number): void {
    var ix: number;
    var iy: number;
    for (iy = 0; iy < BOARD_HEIGHT; iy += 1) {
      for (ix = 0; ix < BOARD_WIDTH; ix += 1) {
        VideoWriteText(ix, iy, color, chr);
      }
    }
  }

  export function TransitionDrawBoardChange(): void {
    TransitionDrawToFill(String.fromCharCode(219), 0x05);
    TransitionDrawToBoard();
  }

  export function SidebarClearLine(y: number): void {
    VideoWriteText(60, y, 0x11, "                   ");
  }

  export function SidebarClear(): void {
    var i: number;
    for (i = 3; i <= 24; i += 1) {
      SidebarClearLine(i);
    }
  }

  var PROMPT_NUMERIC: number = 0;
  var PROMPT_ALPHANUM: number = 1;
  var PROMPT_ANY: number = 2;

  function PromptString(x: number, y: number, arrowColor: number, color: number, width: number, mode: number, buffer: string): string {
    var current: string = buffer;
    var oldBuffer: string = buffer;
    var firstKeyPress: boolean = true;
    var i: number;
    var key: string;
    var keyUpper: string;

    while (true) {
      for (i = 0; i < width; i += 1) {
        VideoWriteText(x + i, y, color, " ");
        VideoWriteText(x + i, y - 1, arrowColor, " ");
      }
      VideoWriteText(x + width, y - 1, arrowColor, " ");
      VideoWriteText(x + current.length, y - 1, ((Math.floor(arrowColor / 16) * 16) + 0x0f), String.fromCharCode(31));
      VideoWriteText(x, y, color, current);

      InputReadWaitKey();
      key = InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0) : String.fromCharCode(0);
      keyUpper = upperCase(key);

      if (current.length < width && key.charCodeAt(0) >= 32 && key.charCodeAt(0) < 128) {
        if (firstKeyPress) {
          current = "";
        }
        if (mode === PROMPT_NUMERIC) {
          if (key >= "0" && key <= "9") {
            current += key;
          }
        } else if (mode === PROMPT_ANY) {
          current += key;
        } else if (mode === PROMPT_ALPHANUM) {
          if ((keyUpper >= "A" && keyUpper <= "Z") || (key >= "0" && key <= "9") || key === "-") {
            current += keyUpper;
          }
        }
      } else if (key === KEY_LEFT || key === KEY_BACKSPACE) {
        if (current.length > 0) {
          current = current.slice(0, current.length - 1);
        }
      }

      firstKeyPress = false;
      if (key === KEY_ENTER || key === KEY_ESCAPE) {
        if (key === KEY_ESCAPE) {
          return oldBuffer;
        }
        return current;
      }
    }
  }

  function SidebarPromptString(prompt: string, extension: string, filename: string, promptMode: number): string {
    var value: string = filename;
    SidebarClearLine(3);
    SidebarClearLine(4);
    SidebarClearLine(5);
    VideoWriteText(75 - prompt.length, 3, 0x1f, prompt);
    VideoWriteText(63, 5, 0x0f, "        " + extension);
    value = PromptString(63, 5, 0x1e, 0x0f, 8, promptMode, value);
    SidebarClearLine(3);
    SidebarClearLine(4);
    SidebarClearLine(5);
    return value;
  }

  function PopupPromptString(question: string, buffer: string): string {
    var value: string = buffer;
    SidebarClearLine(3);
    SidebarClearLine(4);
    SidebarClearLine(5);
    VideoWriteText(63, 4, 0x1f, padRight(question, 17));
    value = PromptString(63, 5, 0x1e, 0x0f, 17, PROMPT_ANY, value);
    SidebarClearLine(3);
    SidebarClearLine(4);
    SidebarClearLine(5);

    return value;
  }

  export function SidebarPromptYesNo(message: string, defaultReturn: boolean): boolean {
    var key: string;

    SidebarClearLine(3);
    SidebarClearLine(4);
    SidebarClearLine(5);
    VideoWriteText(63, 5, 0x1f, message);
    VideoWriteText(63 + message.length, 5, 0x9e, "_");

    while (true) {
      InputReadWaitKey();
      key = upperCase(InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0) : String.fromCharCode(0));
      if (key === KEY_ESCAPE || key === KEY_ENTER || key === "N" || key === "Q" || key === "Y") {
        break;
      }
    }

    if (key === "Y") {
      defaultReturn = true;
    } else if (key === "N" || key === KEY_ESCAPE) {
      defaultReturn = false;
    }

    SidebarClearLine(5);
    return defaultReturn;
  }

  export function HighScoresLoad(): void {
    var worldKey: string = getCurrentScoreWorldKey();
    var store: SharedHighScoreStore = readSharedHighScoreStore();
    var record: SharedHighScoreWorldRecord | undefined = store.worlds[worldKey];
    var paths: string[] = getHighScorePathCandidates();
    var i: number;
    var bytes: number[] | null;

    ActiveHighScoreEntries = [];
    resetHighScoreList();

    if (typeof record !== "undefined") {
      setActiveHighScoreEntries(record.entries);
      if (ActiveHighScoreEntries.length > 0) {
        return;
      }
    }

    for (i = 0; i < paths.length; i += 1) {
      bytes = runtime.readBinaryFile(paths[i]);
      if (bytes !== null && parseHighScoreList(bytes)) {
        if (ActiveHighScoreEntries.length > 0) {
          HighScoresSave();
        }
        return;
      }
    }
  }

  export function HighScoresSave(): void {
    var worldKey: string = getCurrentScoreWorldKey();
    var store: SharedHighScoreStore = readSharedHighScoreStore();
    var entries: SharedHighScoreEntry[] =
      ActiveHighScoreEntries.length > 0 ? ActiveHighScoreEntries.slice(0) : getActiveHighScoreEntriesFromList();
    var paths: string[] = getHighScorePathCandidates();
    var targetPath: string = paths.length > 0 ? paths[0] : execPath("ZZT.HI");

    if (entries.length > HIGH_SCORE_COUNT) {
      entries = entries.slice(0, HIGH_SCORE_COUNT);
    }
    setActiveHighScoreEntries(entries);
    entries = ActiveHighScoreEntries.slice(0);

    if (entries.length > 0) {
      store.worlds[worldKey] = {
        updatedAt: currentIsoTimestamp(),
        entries: entries
      };
    } else if (typeof store.worlds[worldKey] !== "undefined") {
      delete store.worlds[worldKey];
    }

    writeSharedHighScoreStore(store);
    ensureParentDirectory(targetPath);
    runtime.writeBinaryFile(targetPath, serializeHighScoreList());
  }

  function highScoresInitTextWindow(state: TextWindowState): void {
    var i: number;
    var scoreStr: string;
    var displayName: string;

    TextWindowInitState(state);
    TextWindowAppend(state, "Score  Name");
    TextWindowAppend(state, "-----  ----------------------------------");

    for (i = 1; i <= HIGH_SCORE_COUNT; i += 1) {
      if (HighScoreList[i].Score >= 0) {
        displayName = HighScoreList[i].Name;
        if (displayName.length <= 0) {
          displayName = "Unknown Player";
        }
        scoreStr = padLeft(String(HighScoreList[i].Score), 5);
        TextWindowAppend(state, scoreStr + "  " + displayName);
      }
    }
  }

  export function HighScoresDisplay(linePos: number): void {
    var state: TextWindowState = {
      Selectable: false,
      LineCount: 0,
      LinePos: 1,
      Lines: [],
      Hyperlink: "",
      Title: "",
      LoadedFilename: "",
      ScreenCopy: []
    };

    highScoresInitTextWindow(state);
    state.LinePos = linePos;
    if (state.LinePos < 1) {
      state.LinePos = 1;
    } else if (state.LinePos > state.LineCount) {
      state.LinePos = state.LineCount;
    }

    if (state.LineCount > 2) {
      state.Title = "High scores for " + (World.Info.Name.length > 0 ? World.Info.Name : splitBaseName(getCurrentScoreWorldKey()));
      TextWindowDrawOpen(state);
      TextWindowSelect(state, false, true);
      TextWindowDrawClose(state);
    }

    TextWindowFree(state);
  }

  export function HighScoresAdd(score: number): void {
    var entries: SharedHighScoreEntry[] = ActiveHighScoreEntries.slice(0);
    var listPos: number = 0;
    var entry: SharedHighScoreEntry;
    var boardTitle: string = World.Info.Name.length > 0 ? World.Info.Name : splitBaseName(getCurrentScoreWorldKey());
    var textWindow: TextWindowState = {
      Selectable: false,
      LineCount: 0,
      LinePos: 1,
      Lines: [],
      Hyperlink: "",
      Title: "",
      LoadedFilename: "",
      ScreenCopy: []
    };

    while (listPos < entries.length && score < entries[listPos].score) {
      listPos += 1;
    }

    if (listPos >= HIGH_SCORE_COUNT || score <= 0) {
      return;
    }

    entry = {
      player: getCurrentUserName(),
      bbs: getCurrentBbsName(),
      score: score,
      recordedAt: currentIsoTimestamp()
    };
    entries.splice(listPos, 0, entry);
    if (entries.length > HIGH_SCORE_COUNT) {
      entries = entries.slice(0, HIGH_SCORE_COUNT);
    }
    setActiveHighScoreEntries(entries);
    HighScoresSave();

    highScoresInitTextWindow(textWindow);
    textWindow.LinePos = listPos + 1;
    textWindow.Title = "New high score for " + boardTitle;
    TextWindowDrawOpen(textWindow);
    TextWindowSelect(textWindow, false, false);
    TextWindowDrawClose(textWindow);
    TransitionDrawToBoard();
    TextWindowFree(textWindow);
  }

  function showTitleNotice(title: string, lines: string[]): void {
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
    var i: number;

    TextWindowInitState(state);
    for (i = 0; i < lines.length; i += 1) {
      TextWindowAppend(state, lines[i]);
    }

    if (state.LineCount > 0) {
      TextWindowDrawOpen(state);
      TextWindowSelect(state, false, false);
      TextWindowDrawClose(state);
    }

    TextWindowFree(state);
  }

  export function EditorLoop(): void {
    var DRAW_MODE_OFF: number = 0;
    var DRAW_MODE_ON: number = 1;
    var DRAW_MODE_TEXT: number = 2;
    var wasModified: boolean = false;
    var editorExitRequested: boolean = false;
    var drawMode: number = DRAW_MODE_OFF;
    var cursorX: number = 30;
    var cursorY: number = 12;
    var cursorPattern: number = 1;
    var cursorColor: number = 0x0e;
    var colorNames: string[] = ["", "Blue", "Green", "Cyan", "Red", "Purple", "Yellow", "White"];
    var copiedTile: Tile = createTile(E_EMPTY, 0x0f);
    var copiedStat: Stat = createDefaultStat();
    var copiedHasStat: boolean = false;
    var copiedX: number = 30;
    var copiedY: number = 12;
    var keyRaw: string;
    var key: string;
    var nulKey: string = String.fromCharCode(0);
    var i: number;
    var canModify: boolean;
    var tileAtCursor: Tile;
    var cursorBlinker: number = 0;
    var useBlockingInputFallback: boolean =
      !(typeof console !== "undefined" && typeof console.inkey === "function");

    function editorDrawTileAndNeighborsAt(x: number, y: number): void {
      var n: number;
      var nx: number;
      var ny: number;
      BoardDrawTile(x, y);
      for (n = 0; n < 4; n += 1) {
        nx = x + NeighborDeltaX[n];
        ny = y + NeighborDeltaY[n];
        if (nx >= 1 && nx <= BOARD_WIDTH && ny >= 1 && ny <= BOARD_HEIGHT) {
          BoardDrawTile(nx, ny);
        }
      }
    }

    function editorElementMenuColor(element: number): number {
      if (ElementDefs[element].Color === COLOR_CHOICE_ON_BLACK) {
        return (cursorColor % 0x10) + 0x10;
      }
      if (ElementDefs[element].Color === COLOR_WHITE_ON_CHOICE) {
        return (cursorColor * 0x10) - 0x71;
      }
      if (ElementDefs[element].Color === COLOR_CHOICE_ON_CHOICE) {
        return ((cursorColor - 8) * 0x11) + 8;
      }
      if ((ElementDefs[element].Color & 0x70) === 0) {
        return (ElementDefs[element].Color % 0x10) + 0x10;
      }
      return ElementDefs[element].Color;
    }

    function editorElementPlacementColor(element: number): number {
      if (ElementDefs[element].Color === COLOR_CHOICE_ON_BLACK) {
        return cursorColor;
      }
      if (ElementDefs[element].Color === COLOR_WHITE_ON_CHOICE) {
        return (cursorColor * 0x10) - 0x71;
      }
      if (ElementDefs[element].Color === COLOR_CHOICE_ON_CHOICE) {
        return ((cursorColor - 8) * 0x11) + 8;
      }
      return ElementDefs[element].Color;
    }

    function editorDrawSidebar(): void {
      var iColor: number;
      var iPattern: number;
      var copiedCharCode: number;

      function getCopiedPreviewCharCode(): number {
        var drawProc: ((x: number, y: number) => number) | null;
        if (copiedTile.Element >= 0 && copiedTile.Element <= MAX_ELEMENT) {
          if (ElementDefs[copiedTile.Element].HasDrawProc) {
            drawProc = ElementDefs[copiedTile.Element].DrawProc;
            if (drawProc !== null) {
              return drawProc(copiedX, copiedY) & 0xff;
            }
          }
          if (ElementDefs[copiedTile.Element].Character.length > 0) {
            return ElementDefs[copiedTile.Element].Character.charCodeAt(0) & 0xff;
          }
        }
        return " ".charCodeAt(0);
      }

      SidebarClear();
      SidebarClearLine(1);
      VideoWriteText(61, 0, 0x1f, "     - - - -       ");
      VideoWriteText(62, 1, 0x70, "  ZZT Editor   ");
      VideoWriteText(61, 2, 0x1f, "     - - - -       ");
      VideoWriteText(61, 4, 0x70, " L ");
      VideoWriteText(64, 4, 0x1f, " Load");
      VideoWriteText(61, 5, 0x30, " S ");
      VideoWriteText(64, 5, 0x1f, " Save");
      VideoWriteText(70, 4, 0x70, " H ");
      VideoWriteText(73, 4, 0x1e, " Help");
      VideoWriteText(70, 5, 0x30, " Q ");
      VideoWriteText(73, 5, 0x1f, " Quit");
      VideoWriteText(61, 7, 0x70, " B ");
      VideoWriteText(65, 7, 0x1f, " Switch boards");
      VideoWriteText(61, 8, 0x30, " I ");
      VideoWriteText(65, 8, 0x1f, " Board Info");
      VideoWriteText(61, 10, 0x70, "  f1   ");
      VideoWriteText(68, 10, 0x1f, " Item");
      VideoWriteText(61, 11, 0x30, "  f2   ");
      VideoWriteText(68, 11, 0x1f, " Creature");
      VideoWriteText(61, 12, 0x70, "  f3   ");
      VideoWriteText(68, 12, 0x1f, " Terrain");
      VideoWriteText(61, 13, 0x30, "  f4   ");
      VideoWriteText(68, 13, 0x1f, " Enter text");
      VideoWriteText(61, 15, 0x70, " Space ");
      VideoWriteText(68, 15, 0x1f, " Plot");
      VideoWriteText(61, 16, 0x30, "  Tab  ");
      VideoWriteText(68, 16, 0x1f, " Draw mode");
      VideoWriteText(61, 18, 0x70, " P ");
      VideoWriteText(64, 18, 0x1f, " Pattern");
      VideoWriteText(61, 19, 0x30, " C ");
      VideoWriteText(64, 19, 0x1f, " Color:");

      for (iColor = 9; iColor <= 15; iColor += 1) {
        VideoWriteText(61 + iColor, 22, iColor, String.fromCharCode(219));
      }

      for (iPattern = 1; iPattern <= EditorPatternCount; iPattern += 1) {
        VideoWriteText(61 + iPattern, 22, 0x0f, ElementDefs[EditorPatterns[iPattern]].Character);
      }

      copiedCharCode = getCopiedPreviewCharCode();
      VideoWriteText(62 + EditorPatternCount, 22, copiedTile.Color, String.fromCharCode(copiedCharCode));
      VideoWriteText(61, 24, 0x1f, " Mode:");
    }

    function editorUpdateSidebar(): void {
      var modeLabel: string;
      var modeColor: number;
      var colorNameIdx: number = cursorColor - 8;
      var colorName: string = "";

      if (drawMode === DRAW_MODE_ON) {
        modeLabel = "Drawing on ";
        modeColor = 0x9e;
      } else if (drawMode === DRAW_MODE_TEXT) {
        modeLabel = "Text entry ";
        modeColor = 0x9e;
      } else {
        modeLabel = "Drawing off";
        modeColor = 0x1e;
      }

      if (colorNameIdx >= 0 && colorNameIdx < colorNames.length) {
        colorName = colorNames[colorNameIdx];
      }
      VideoWriteText(72, 19, 0x1e, padRight(colorName, 7));
      VideoWriteText(62, 21, 0x1f, "       ");
      VideoWriteText(69, 21, 0x1f, "        ");
      VideoWriteText(61 + cursorPattern, 21, 0x1f, String.fromCharCode(31));
      VideoWriteText(61 + cursorColor, 21, 0x1f, String.fromCharCode(31));
      VideoWriteText(68, 24, modeColor, modeLabel);
    }

    function editorSetAndCopyTile(x: number, y: number, element: number, color: number): void {
      Board.Tiles[x][y].Element = element;
      Board.Tiles[x][y].Color = color;
      copiedTile = cloneTile(Board.Tiles[x][y]);
      copiedHasStat = false;
      copiedX = x;
      copiedY = y;
      editorDrawTileAndNeighborsAt(x, y);
    }

    function editorDrawRefresh(): void {
      TransitionDrawToBoard();
      editorDrawSidebar();
      if (Board.Name.length > 0) {
        VideoWriteText(Math.floor((59 - Board.Name.length) / 2), 0, 0x70, " " + Board.Name + " ");
      } else {
        VideoWriteText(26, 0, 0x70, " Untitled ");
      }
    }

    function editorPrepareModifyTile(x: number, y: number): boolean {
      var result: boolean;
      wasModified = true;
      result = BoardPrepareTileForPlacement(x, y);
      editorDrawTileAndNeighborsAt(x, y);
      return result;
    }

    function editorPrepareModifyStatAtCursor(): boolean {
      if (Board.StatCount >= MAX_STAT) {
        return false;
      }
      return editorPrepareModifyTile(cursorX, cursorY);
    }

    function editorCreateStatTemplate(element: number): Stat {
      var stat: Stat = createDefaultStat();

      if (ElementDefs[element].Param1Name.length > 0) {
        stat.P1 = World.EditorStatSettings[element].P1;
      }
      if (ElementDefs[element].Param2Name.length > 0) {
        stat.P2 = World.EditorStatSettings[element].P2;
      }
      if (ElementDefs[element].ParamDirName.length > 0) {
        stat.StepX = World.EditorStatSettings[element].StepX;
        stat.StepY = World.EditorStatSettings[element].StepY;
      }
      if (ElementDefs[element].ParamBoardName.length > 0) {
        stat.P3 = World.EditorStatSettings[element].P3;
      }

      return stat;
    }

    function editorPlaceTile(x: number, y: number): void {
      var element: number;
      if (cursorPattern > EditorPatternCount && copiedHasStat) {
        if (editorPrepareModifyStatAtCursor()) {
          AddStat(x, y, copiedTile.Element, copiedTile.Color, copiedStat.Cycle, copiedStat);
          editorDrawTileAndNeighborsAt(x, y);
        }
        return;
      }

      if (cursorPattern <= EditorPatternCount) {
        element = EditorPatterns[cursorPattern];
        if (editorPrepareModifyTile(x, y)) {
          Board.Tiles[x][y].Element = element;
          Board.Tiles[x][y].Color = cursorColor;
        }
      } else {
        if (editorPrepareModifyTile(x, y)) {
          Board.Tiles[x][y] = cloneTile(copiedTile);
        }
      }
      editorDrawTileAndNeighborsAt(x, y);
    }

    function editorSelectElementMenu(selectedCategory: number): void {
      var iElem: number;
      var y: number = 3;
      var menuColor: number;
      var selectedKey: string;
      var selectedKeyUpper: string;
      var statTemplate: Stat;
      var shortcutLabel: string;

      VideoWriteText(cursorX - 1, cursorY - 1, 0x0f, String.fromCharCode(197));
      for (i = 3; i <= 20; i += 1) {
        SidebarClearLine(i);
      }

      for (iElem = 0; iElem <= MAX_ELEMENT; iElem += 1) {
        if (ElementDefs[iElem].EditorCategory !== selectedCategory) {
          continue;
        }

        if (ElementDefs[iElem].CategoryName.length > 0) {
          y += 1;
          if (y <= 20) {
            VideoWriteText(65, y, 0x1e, ElementDefs[iElem].CategoryName);
          }
          y += 1;
        }

        if (y > 20) {
          break;
        }

        shortcutLabel = ElementDefs[iElem].EditorShortcut.length > 0
          ? ElementDefs[iElem].EditorShortcut
          : " ";

        VideoWriteText(61, y, ((y % 2) << 6) + 0x30, " " + shortcutLabel + " ");
        VideoWriteText(65, y, 0x1f, padRight(ElementDefs[iElem].Name, 12));
        menuColor = editorElementMenuColor(iElem);
        VideoWriteText(78, y, menuColor, ElementDefs[iElem].Character);
        y += 1;
      }

      InputReadWaitKey();
      selectedKey = InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0) : String.fromCharCode(0);
      selectedKeyUpper = upperCase(selectedKey);

      for (iElem = 1; iElem <= MAX_ELEMENT; iElem += 1) {
        if (ElementDefs[iElem].EditorCategory !== selectedCategory) {
          continue;
        }
        if (ElementDefs[iElem].EditorShortcut !== selectedKeyUpper) {
          continue;
        }

        if (iElem === E_PLAYER) {
          if (editorPrepareModifyTile(cursorX, cursorY)) {
            MoveStat(0, cursorX, cursorY);
          }
        } else {
          menuColor = editorElementPlacementColor(iElem);
          if (ElementDefs[iElem].Cycle === -1) {
            if (editorPrepareModifyTile(cursorX, cursorY)) {
              editorSetAndCopyTile(cursorX, cursorY, iElem, menuColor);
            }
          } else {
            if (editorPrepareModifyStatAtCursor()) {
              var previousModified: boolean = wasModified;
              statTemplate = editorCreateStatTemplate(iElem);
              AddStat(cursorX, cursorY, iElem, menuColor, ElementDefs[iElem].Cycle, statTemplate);
              if (!editorEditStat(Board.StatCount)) {
                RemoveStat(Board.StatCount);
                wasModified = previousModified;
              } else {
                editorDrawTileAndNeighborsAt(cursorX, cursorY);
              }
            }
          }
        }

        break;
      }
      editorDrawSidebar();
    }

    function editorTryPlaceTextCharacter(ch: string): boolean {
      var code: number;
      var textElement: number;

      if (ch.length <= 0) {
        return false;
      }

      code = ch.charCodeAt(0);
      if (code < 32 || code >= 128) {
        return false;
      }

      if (editorPrepareModifyTile(cursorX, cursorY)) {
        textElement = (cursorColor - 9) + E_TEXT_MIN;
        if (textElement < E_TEXT_MIN) {
          textElement = E_TEXT_MIN;
        } else if (textElement > E_TEXT_WHITE) {
          textElement = E_TEXT_WHITE;
        }
        Board.Tiles[cursorX][cursorY].Element = textElement;
        Board.Tiles[cursorX][cursorY].Color = code & 0xff;
        editorDrawTileAndNeighborsAt(cursorX, cursorY);
        if (cursorX < BOARD_WIDTH) {
          cursorX += 1;
        }
      }
      return true;
    }

    function editorPromptSlider(
      editable: boolean,
      x: number,
      y: number,
      promptText: string,
      value: number
    ): number {
      var parsedPrompt: string = promptText;
      var startChar: string = "1";
      var endChar: string = "9";
      var key: string;
      var newValue: number;

      if (parsedPrompt.length >= 3 && parsedPrompt.charAt(parsedPrompt.length - 3) === ";") {
        startChar = parsedPrompt.charAt(parsedPrompt.length - 2);
        endChar = parsedPrompt.charAt(parsedPrompt.length - 1);
        parsedPrompt = parsedPrompt.slice(0, parsedPrompt.length - 3);
      }

      if (value < 0) {
        value = 0;
      }
      if (value > 8) {
        value = 8;
      }

      SidebarClearLine(y);
      VideoWriteText(x, y, (editable ? 1 : 0) + 0x1e, parsedPrompt);
      SidebarClearLine(y + 1);
      SidebarClearLine(y + 2);
      VideoWriteText(x, y + 2, 0x1e, startChar + "....:...." + endChar);

      while (!runtime.isTerminated()) {
        if (!editable) {
          break;
        }

        VideoWriteText(x + value + 1, y + 1, 0x9f, String.fromCharCode(31));
        InputReadWaitKey();
        key = InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0) : String.fromCharCode(0);

        if (key >= "1" && key <= "9") {
          value = key.charCodeAt(0) - "1".charCodeAt(0);
          SidebarClearLine(y + 1);
        } else {
          newValue = value + InputDeltaX;
          if (newValue >= 0 && newValue <= 8 && newValue !== value) {
            value = newValue;
            SidebarClearLine(y + 1);
          }
        }

        if (key === KEY_ENTER || key === KEY_ESCAPE || InputShiftPressed) {
          break;
        }
      }

      VideoWriteText(x + value + 1, y + 1, 0x1f, String.fromCharCode(31));
      return value;
    }

    function editorPromptChoice(editable: boolean, y: number, prompt: string, choiceStr: string, result: number): number {
      var starts: number[] = [];
      var iChar: number;
      var isStart: boolean;
      var key: string;
      var newResult: number;
      var markerX: number;

      SidebarClearLine(y);
      SidebarClearLine(y + 1);
      SidebarClearLine(y + 2);
      VideoWriteText(63, y, (editable ? 1 : 0) + 0x1e, prompt);
      VideoWriteText(63, y + 2, 0x1e, choiceStr);

      for (iChar = 0; iChar < choiceStr.length; iChar += 1) {
        isStart = choiceStr.charAt(iChar) !== " " && (iChar === 0 || choiceStr.charAt(iChar - 1) === " ");
        if (isStart) {
          starts.push(iChar);
        }
      }

      if (starts.length <= 0) {
        return 0;
      }

      if (result < 0) {
        result = 0;
      }
      if (result >= starts.length) {
        result = starts.length - 1;
      }

      while (!runtime.isTerminated()) {
        markerX = 63 + starts[result];
        if (editable) {
          VideoWriteText(markerX, y + 1, 0x9f, String.fromCharCode(31));
          InputReadWaitKey();
          key = InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0) : String.fromCharCode(0);
          newResult = result + InputDeltaX;
          if (newResult >= 0 && newResult < starts.length && newResult !== result) {
            result = newResult;
            SidebarClearLine(y + 1);
          }
        } else {
          break;
        }

        if (key === KEY_ENTER || key === KEY_ESCAPE || InputShiftPressed) {
          break;
        }
      }

      markerX = 63 + starts[result];
      VideoWriteText(markerX, y + 1, 0x1f, String.fromCharCode(31));
      return result;
    }

    function editorPromptDirection(
      editable: boolean,
      y: number,
      prompt: string,
      deltaX: number,
      deltaY: number
    ): Direction {
      var choice: number;

      if (deltaY === -1) {
        choice = 0;
      } else if (deltaY === 1) {
        choice = 1;
      } else if (deltaX === -1) {
        choice = 2;
      } else {
        choice = 3;
      }

      choice = editorPromptChoice(
        editable,
        y,
        prompt,
        String.fromCharCode(24) + " " + String.fromCharCode(25) + " " + String.fromCharCode(27) + " " + String.fromCharCode(26),
        choice
      );

      return {
        DeltaX: NeighborDeltaX[choice],
        DeltaY: NeighborDeltaY[choice]
      };
    }

    function editorPromptCharacter(editable: boolean, x: number, y: number, prompt: string, value: number): number {
      var iChar: number;
      var drawCode: number;
      var key: string;
      var newValue: number;

      SidebarClearLine(y);
      VideoWriteText(x, y, (editable ? 1 : 0) + 0x1e, prompt);
      SidebarClearLine(y + 1);
      VideoWriteText(x + 5, y + 1, 0x9f, String.fromCharCode(31));
      SidebarClearLine(y + 2);

      while (!runtime.isTerminated()) {
        for (iChar = value - 4; iChar <= value + 4; iChar += 1) {
          drawCode = (iChar + 256) % 256;
          VideoWriteText(((x + iChar) - value) + 5, y + 2, 0x1e, String.fromCharCode(drawCode));
        }

        if (!editable) {
          break;
        }

        InputReadWaitKey();
        key = InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0) : String.fromCharCode(0);
        if (key === KEY_TAB) {
          newValue = (value + 9) & 0xff;
        } else {
          newValue = (value + InputDeltaX + 256) % 256;
        }

        if (newValue !== value) {
          value = newValue;
          SidebarClearLine(y + 2);
        }

        if (key === KEY_ENTER || key === KEY_ESCAPE || InputShiftPressed) {
          break;
        }
      }

      VideoWriteText(x + 5, y + 1, 0x1f, String.fromCharCode(31));
      return value;
    }

    function editorPromptNumericValue(prompt: string, currentValue: number, width: number, maxValue: number): number {
      var valueStr: string;
      var parsed: number;

      SidebarClearLine(3);
      SidebarClearLine(4);
      SidebarClearLine(5);
      VideoWriteText(63, 4, 0x1f, padRight(prompt, 17));
      valueStr = PromptString(63, 5, 0x1e, 0x0f, width, PROMPT_NUMERIC, String(currentValue));
      SidebarClearLine(3);
      SidebarClearLine(4);
      SidebarClearLine(5);

      if (InputKeyPressed === KEY_ESCAPE || valueStr.length <= 0) {
        return currentValue;
      }

      parsed = parseInt(valueStr, 10);
      if (isNaN(parsed)) {
        return currentValue;
      }
      if (parsed < 0) {
        parsed = 0;
      }
      if (parsed > maxValue) {
        parsed = maxValue;
      }
      return parsed;
    }

    function editorCopyStatDataToTextWindow(statId: number, state: TextWindowState): void {
      var stat: Stat = Board.Stats[statId];
      var data: string = stat.Data === null ? "" : stat.Data;
      var line: string = "";
      var iData: number;
      var ch: string;

      if (data.length <= 0 || stat.DataLen <= 0) {
        TextWindowAppend(state, "");
        return;
      }

      for (iData = 0; iData < stat.DataLen && iData < data.length; iData += 1) {
        ch = data.charAt(iData);
        if (ch === "\r" || ch === "\n") {
          TextWindowAppend(state, line);
          line = "";
          if (ch === "\r" && (iData + 1) < data.length && data.charAt(iData + 1) === "\n") {
            iData += 1;
          }
        } else if (ch !== String.fromCharCode(0)) {
          line += ch;
        }
      }

      if (line.length > 0 || state.LineCount <= 0) {
        TextWindowAppend(state, line);
      }
    }

    function editorStoreTextWindowIntoStatData(statId: number, state: TextWindowState): void {
      var iLine: number;
      var data: string = "";
      var stat: Stat = Board.Stats[statId];

      for (iLine = 1; iLine <= state.LineCount; iLine += 1) {
        data += state.Lines[iLine - 1] + "\r";
      }

      stat.Data = data;
      stat.DataLen = data.length;
      stat.DataPos = 0;
    }

    function editorEditStatText(statId: number, prompt: string): void {
      var state: TextWindowState = {
        Selectable: false,
        LineCount: 0,
        LinePos: 1,
        Lines: [],
        Hyperlink: "",
        Title: prompt,
        LoadedFilename: "",
        ScreenCopy: []
      };

      TextWindowInitState(state);
      TextWindowDrawOpen(state);
      editorCopyStatDataToTextWindow(statId, state);
      TextWindowEdit(state);
      editorStoreTextWindowIntoStatData(statId, state);
      TextWindowFree(state);
      TextWindowDrawClose(state);
      InputKeyPressed = String.fromCharCode(0);
      BoardDrawTile(Board.Stats[statId].X, Board.Stats[statId].Y);
    }

    function editorCategoryNameForElement(element: number): string {
      var categoryName: string = "";
      var iElem: number;
      for (iElem = 0; iElem <= element; iElem += 1) {
        if (ElementDefs[iElem].EditorCategory === ElementDefs[element].EditorCategory &&
            ElementDefs[iElem].CategoryName.length > 0) {
          categoryName = ElementDefs[iElem].CategoryName;
        }
      }
      return categoryName;
    }

    function editorEditStat(statId: number): boolean {
      var stat: Stat;
      var element: number;
      var promptByte: number;
      var dir: Direction;
      var selectedBoard: number;
      var iy: number;
      var boardName: string;

      function editorEditStatSettings(selected: boolean): void {
        InputKeyPressed = String.fromCharCode(0);
        iy = 9;

        if (ElementDefs[element].Param1Name.length > 0) {
          if (ElementDefs[element].ParamTextName.length === 0) {
            promptByte = stat.P1 & 0xff;
            if (promptByte > 8) {
              promptByte = 8;
            }
            promptByte = editorPromptSlider(selected, 63, iy, ElementDefs[element].Param1Name, promptByte);
            if (selected) {
              stat.P1 = promptByte;
              World.EditorStatSettings[element].P1 = stat.P1;
            }
          } else {
            if (stat.P1 === 0) {
              stat.P1 = World.EditorStatSettings[element].P1;
            }
            BoardDrawTile(stat.X, stat.Y);
            stat.P1 = editorPromptCharacter(selected, 63, iy, ElementDefs[element].Param1Name, stat.P1 & 0xff);
            BoardDrawTile(stat.X, stat.Y);
            if (selected) {
              World.EditorStatSettings[element].P1 = stat.P1;
            }
          }
          iy += 4;
        }

        if (InputKeyPressed !== KEY_ESCAPE && ElementDefs[element].ParamTextName.length > 0) {
          if (selected) {
            editorEditStatText(statId, ElementDefs[element].ParamTextName);
          }
        }

        if (InputKeyPressed !== KEY_ESCAPE && ElementDefs[element].Param2Name.length > 0) {
          promptByte = stat.P2 % 0x80;
          if (promptByte > 8) {
            promptByte = 8;
          }
          promptByte = editorPromptSlider(selected, 63, iy, ElementDefs[element].Param2Name, promptByte);
          if (selected) {
            stat.P2 = (stat.P2 & 0x80) + promptByte;
            World.EditorStatSettings[element].P2 = stat.P2;
          }
          iy += 4;
        }

        if (InputKeyPressed !== KEY_ESCAPE && ElementDefs[element].ParamBulletTypeName.length > 0) {
          promptByte = Math.floor(stat.P2 / 0x80);
          promptByte = editorPromptChoice(selected, iy, ElementDefs[element].ParamBulletTypeName, "Bullets Stars", promptByte);
          if (selected) {
            stat.P2 = (stat.P2 % 0x80) + (promptByte * 0x80);
            World.EditorStatSettings[element].P2 = stat.P2;
          }
          iy += 4;
        }

        if (InputKeyPressed !== KEY_ESCAPE && ElementDefs[element].ParamDirName.length > 0) {
          dir = editorPromptDirection(selected, iy, ElementDefs[element].ParamDirName, stat.StepX, stat.StepY);
          if (selected) {
            stat.StepX = dir.DeltaX;
            stat.StepY = dir.DeltaY;
            World.EditorStatSettings[element].StepX = stat.StepX;
            World.EditorStatSettings[element].StepY = stat.StepY;
          }
          iy += 4;
        }

        if (InputKeyPressed !== KEY_ESCAPE && ElementDefs[element].ParamBoardName.length > 0) {
          if (selected) {
            selectedBoard = editorSelectBoard(ElementDefs[element].ParamBoardName, stat.P3, true);
            if (selectedBoard >= 0) {
              stat.P3 = selectedBoard;
              if (stat.P3 > World.BoardCount) {
                editorAppendBoard();
                copiedHasStat = false;
                copiedTile = createTile(E_EMPTY, 0x0f);
                copiedX = cursorX;
                copiedY = cursorY;
              }
              World.EditorStatSettings[element].P3 = stat.P3;
            } else {
              InputKeyPressed = KEY_ESCAPE;
            }
          } else {
            boardName = editorGetBoardName(stat.P3, true);
            VideoWriteText(63, iy, 0x1f, padRight("Room: " + boardName, 17));
          }
        }
      }

      if (statId < 0 || statId > Board.StatCount) {
        return false;
      }

      stat = Board.Stats[statId];
      element = Board.Tiles[stat.X][stat.Y].Element;

      SidebarClear();
      VideoWriteText(64, 6, 0x1e, padRight(editorCategoryNameForElement(element), 15));
      VideoWriteText(64, 7, 0x1f, padRight(ElementDefs[element].Name, 15));
      editorEditStatSettings(false);
      editorEditStatSettings(true);

      if (InputKeyPressed === KEY_ESCAPE) {
        editorDrawSidebar();
        return false;
      }

      copiedHasStat = true;
      copiedStat = cloneStat(stat);
      copiedTile = cloneTile(Board.Tiles[stat.X][stat.Y]);
      copiedX = stat.X;
      copiedY = stat.Y;
      wasModified = true;
      editorDrawSidebar();
      return true;
    }

    function editorOpenEditTextWindow(state: TextWindowState): void {
      SidebarClear();
      VideoWriteText(61, 4, 0x30, " Return ");
      VideoWriteText(64, 5, 0x1f, " Insert line");
      VideoWriteText(61, 7, 0x70, " Ctrl-Y ");
      VideoWriteText(64, 8, 0x1f, " Delete line");
      VideoWriteText(61, 10, 0x30, " Cursor keys ");
      VideoWriteText(64, 11, 0x1f, " Move cursor");
      VideoWriteText(61, 13, 0x70, " Insert ");
      VideoWriteText(64, 14, 0x1f, " Insert mode");
      VideoWriteText(61, 16, 0x30, " Delete ");
      VideoWriteText(64, 17, 0x1f, " Delete char");
      VideoWriteText(61, 19, 0x70, " Escape ");
      VideoWriteText(64, 20, 0x1f, " Exit editor");
      TextWindowEdit(state);
    }

    function editorEditHelpFile(): void {
      var filename: string = "";
      var textWindow: TextWindowState = {
        Selectable: false,
        LineCount: 0,
        LinePos: 1,
        Lines: [],
        Hyperlink: "",
        Title: "",
        LoadedFilename: "",
        ScreenCopy: []
      };

      filename = SidebarPromptString("File to edit", ".HLP", filename, PROMPT_ALPHANUM);
      if (InputKeyPressed === KEY_ESCAPE || filename.length <= 0) {
        return;
      }

      TextWindowOpenFile("*" + filename + ".HLP", textWindow);
      textWindow.Title = "Editing " + filename;
      TextWindowDrawOpen(textWindow);
      editorOpenEditTextWindow(textWindow);
      TextWindowSaveFile(execPath(filename + ".HLP"), textWindow);
      TextWindowFree(textWindow);
      TextWindowDrawClose(textWindow);
      editorDrawRefresh();
    }

    function editorTransferBoard(): void {
      var mode: number = 0;
      var path: string;
      var fileBytes: number[] | null;
      var boardData: number[];
      var boardLen: number;
      var currentBoardId: number = World.Info.CurrentBoard;

      mode = editorPromptChoice(true, 3, "Transfer board:", "Import Export", mode);
      if (InputKeyPressed === KEY_ESCAPE) {
        editorDrawSidebar();
        return;
      }

      SavedBoardFileName = SidebarPromptString(
        mode === 0 ? "Import board" : "Export board",
        ".BRD",
        SavedBoardFileName,
        PROMPT_ALPHANUM
      );
      if (InputKeyPressed === KEY_ESCAPE || SavedBoardFileName.length <= 0) {
        editorDrawSidebar();
        return;
      }

      path = execPath(SavedBoardFileName + ".BRD");

      if (mode === 0) {
        fileBytes = runtime.readBinaryFile(path);
        if (fileBytes === null) {
          fileBytes = runtime.readBinaryFile(SavedBoardFileName + ".BRD");
        }
        if (fileBytes === null || fileBytes.length < 2) {
          editorDrawSidebar();
          return;
        }

        boardLen = (fileBytes[0] & 0xff) + ((fileBytes[1] & 0xff) << 8);
        if (boardLen < 0) {
          boardLen = 0;
        }
        if (boardLen > fileBytes.length - 2) {
          boardLen = fileBytes.length - 2;
        }
        boardData = fileBytes.slice(2, 2 + boardLen);

        BoardClose();
        World.BoardLen[currentBoardId] = boardData.length;
        World.BoardData[currentBoardId] = boardData;
        BoardOpen(currentBoardId);
        Board.Info.NeighborBoards[0] = 0;
        Board.Info.NeighborBoards[1] = 0;
        Board.Info.NeighborBoards[2] = 0;
        Board.Info.NeighborBoards[3] = 0;
        wasModified = true;
        editorDrawRefresh();
      } else {
        BoardClose();
        boardData = World.BoardData[currentBoardId] === null ? [] : (World.BoardData[currentBoardId] as number[]);
        boardLen = boardData.length;
        fileBytes = [];
        pushUInt16(fileBytes, boardLen);
        appendBytes(fileBytes, boardData);
        runtime.writeBinaryFile(path, fileBytes);
        BoardOpen(currentBoardId);
        editorDrawSidebar();
      }
    }

    function editorFloodFill(startX: number, startY: number, from: Tile): void {
      var xQueue: number[] = [];
      var yQueue: number[] = [];
      var toFill: number = 1;
      var filled: number = 0;
      var x: number = startX;
      var y: number = startY;
      var n: number;
      var tileAt: Tile;
      var nx: number;
      var ny: number;

      xQueue[1] = startX;
      yQueue[1] = startY;

      while (toFill !== filled && toFill < 255 && !runtime.isTerminated()) {
        tileAt = cloneTile(Board.Tiles[x][y]);
        editorPlaceTile(x, y);
        if (Board.Tiles[x][y].Element !== tileAt.Element || Board.Tiles[x][y].Color !== tileAt.Color) {
          for (n = 0; n < 4 && toFill < 255; n += 1) {
            nx = x + NeighborDeltaX[n];
            ny = y + NeighborDeltaY[n];
            if (Board.Tiles[nx][ny].Element === from.Element &&
                (from.Element === E_EMPTY || Board.Tiles[nx][ny].Color === from.Color)) {
              toFill += 1;
              xQueue[toFill] = nx;
              yQueue[toFill] = ny;
            }
          }
        }
        filled += 1;
        if (filled <= toFill) {
          x = xQueue[filled];
          y = yQueue[filled];
        }
      }
    }

    function editorAskSaveChanged(): boolean {
      InputKeyPressed = String.fromCharCode(0);
      if (wasModified) {
        if (SidebarPromptYesNo("Save first? ", true)) {
          if (InputKeyPressed === KEY_ESCAPE) {
            return false;
          }
          GameWorldSave("Save world:", LoadedGameFileName, ".ZZT");
          if (InputKeyPressed === KEY_ESCAPE) {
            return false;
          }
          wasModified = false;
        }
        if (InputKeyPressed === KEY_ESCAPE) {
          return false;
        }
        World.Info.Name = LoadedGameFileName;
      }
      return true;
    }

    function editorAppendBoard(): void {
      var roomTitle: string;
      if (World.BoardCount >= MAX_BOARD) {
        return;
      }

      BoardClose();
      World.BoardCount += 1;
      World.Info.CurrentBoard = World.BoardCount;
      World.BoardLen[World.Info.CurrentBoard] = 0;
      BoardCreate();

      TransitionDrawToBoard();
      roomTitle = PopupPromptString("Room's Title:", Board.Name);
      if (roomTitle.length > 0) {
        Board.Name = roomTitle;
      }
      TransitionDrawToBoard();
      wasModified = true;
    }

    function editorBoolToString(value: boolean): string {
      if (value) {
        return "Yes";
      }
      return "No ";
    }

    function editorGetBoardName(boardId: number, titleScreenIsNone: boolean): string {
      var boardData: number[] | null;
      var cursor: ByteCursor;
      var copiedName: string;

      if (boardId === 0 && titleScreenIsNone) {
        return "None";
      }

      if (boardId === World.Info.CurrentBoard) {
        return Board.Name;
      }

      if (boardId < 0 || boardId > World.BoardCount) {
        return "";
      }

      boardData = World.BoardData[boardId];
      if (boardData === null || boardData.length <= 0) {
        return "";
      }

      cursor = {
        bytes: boardData,
        offset: 0
      };
      copiedName = readPascalString(cursor, BOARD_NAME_MAX_LEN);
      return copiedName;
    }

    function editorSelectBoard(title: string, currentBoard: number, titleScreenIsNone: boolean): number {
      var textWindow: TextWindowState = {
        Selectable: true,
        LineCount: 0,
        LinePos: currentBoard + 1,
        Lines: [],
        Hyperlink: "",
        Title: title,
        LoadedFilename: "",
        ScreenCopy: []
      };
      var iBoard: number;

      for (iBoard = 0; iBoard <= World.BoardCount; iBoard += 1) {
        TextWindowAppend(textWindow, editorGetBoardName(iBoard, titleScreenIsNone));
      }
      TextWindowAppend(textWindow, "Add new board");

      TextWindowDrawOpen(textWindow);
      TextWindowSelect(textWindow, false, false);
      TextWindowDrawClose(textWindow);

      if (TextWindowRejected) {
        TextWindowFree(textWindow);
        return -1;
      }

      iBoard = textWindow.LinePos - 1;
      TextWindowFree(textWindow);
      return iBoard;
    }

    function editorSwitchBoard(): void {
      var selectedBoard: number = editorSelectBoard("Switch boards", World.Info.CurrentBoard, false);

      if (selectedBoard < 0) {
        return;
      }

      if (selectedBoard > World.BoardCount) {
        if (SidebarPromptYesNo("Add new board? ", false)) {
          editorAppendBoard();
        }
        return;
      }

      BoardChange(selectedBoard);
    }

    function editorEditBoardInfo(): void {
      var state: TextWindowState = {
        Selectable: true,
        LineCount: 0,
        LinePos: 1,
        Lines: [],
        Hyperlink: "",
        Title: "Board Information",
        LoadedFilename: "",
        ScreenCopy: []
      };
      var iLine: number;
      var exitRequested: boolean = false;
      var selectedBoard: number;
      var prevLinePos: number;
      var neighborBoardLabels: string[] = [
        "       Board " + String.fromCharCode(24),
        "       Board " + String.fromCharCode(25),
        "       Board " + String.fromCharCode(27),
        "       Board " + String.fromCharCode(26)
      ];

      while (!exitRequested && !runtime.isTerminated()) {
        prevLinePos = state.LinePos;
        TextWindowInitState(state);
        state.Selectable = true;
        TextWindowAppend(state, "         Title: " + Board.Name);
        TextWindowAppend(state, "      Can fire: " + String(Board.Info.MaxShots) + " shots.");
        TextWindowAppend(state, " Board is dark: " + editorBoolToString(Board.Info.IsDark));

        for (iLine = 0; iLine < 4; iLine += 1) {
          TextWindowAppend(
            state,
            neighborBoardLabels[iLine] + ": " + editorGetBoardName(Board.Info.NeighborBoards[iLine], true)
          );
        }

        TextWindowAppend(
          state,
          "Re-enter when zapped: " + editorBoolToString(Board.Info.ReenterWhenZapped)
        );
        TextWindowAppend(
          state,
          "  Time limit, 0=None: " + String(Board.Info.TimeLimitSec) + " sec."
        );
        TextWindowAppend(state, "          Quit!");

        state.LinePos = prevLinePos;
        if (state.LinePos < 1) {
          state.LinePos = 1;
        }
        if (state.LinePos > state.LineCount) {
          state.LinePos = state.LineCount;
        }

        TextWindowDrawOpen(state);
        TextWindowSelect(state, false, false);
        TextWindowDrawClose(state);

        if (TextWindowRejected || InputKeyPressed !== KEY_ENTER) {
          exitRequested = true;
          continue;
        }

        if (state.LinePos >= 1 && state.LinePos <= 8) {
          wasModified = true;
        }

        if (state.LinePos === 1) {
          Board.Name = PopupPromptString("New title for board:", Board.Name);
          exitRequested = true;
        } else if (state.LinePos === 2) {
          Board.Info.MaxShots = editorPromptNumericValue("Maximum shots?", Board.Info.MaxShots, 3, 255);
          editorDrawSidebar();
        } else if (state.LinePos === 3) {
          Board.Info.IsDark = !Board.Info.IsDark;
        } else if (state.LinePos >= 4 && state.LinePos <= 7) {
          selectedBoard = editorSelectBoard(
            neighborBoardLabels[state.LinePos - 4],
            Board.Info.NeighborBoards[state.LinePos - 4],
            true
          );
          if (selectedBoard >= 0) {
            Board.Info.NeighborBoards[state.LinePos - 4] = selectedBoard;
            if (Board.Info.NeighborBoards[state.LinePos - 4] > World.BoardCount) {
              editorAppendBoard();
            }
            exitRequested = true;
          }
        } else if (state.LinePos === 8) {
          Board.Info.ReenterWhenZapped = !Board.Info.ReenterWhenZapped;
        } else if (state.LinePos === 9) {
          Board.Info.TimeLimitSec = editorPromptNumericValue(
            "Time limit?",
            Board.Info.TimeLimitSec,
            5,
            32767
          );
          editorDrawSidebar();
        } else if (state.LinePos === 10) {
          exitRequested = true;
        }
      }

      TextWindowFree(state);
    }

    if (World.Info.IsSave || WorldGetFlagPosition("SECRET") >= 0) {
      WorldUnload();
      WorldCreate();
    }

    InitElementsEditor();
    CurrentTick = 0;

    if (World.Info.CurrentBoard !== 0) {
      BoardChange(World.Info.CurrentBoard);
    }

    editorDrawRefresh();
    if (World.BoardCount === 0) {
      editorAppendBoard();
      editorDrawRefresh();
    }

    TickTimeCounter = currentHsecs();
    while (!editorExitRequested && !runtime.isTerminated()) {
      if (drawMode === DRAW_MODE_ON) {
        editorPlaceTile(cursorX, cursorY);
      }

      if (useBlockingInputFallback) {
        InputReadWaitKey();
      } else {
        InputUpdate();
      }

      keyRaw = InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0) : nulKey;
      key = upperCase(keyRaw);

      if (keyRaw === nulKey && InputDeltaX === 0 && InputDeltaY === 0 && !InputShiftPressed) {
        var elapsedCheck: { Elapsed: boolean; Counter: number } = hasTimeElapsed(TickTimeCounter, 15);
        TickTimeCounter = elapsedCheck.Counter;
        if (elapsedCheck.Elapsed) {
          cursorBlinker = (cursorBlinker + 1) % 3;
        }

        if (cursorBlinker === 0) {
          BoardDrawTile(cursorX, cursorY);
        } else {
          VideoWriteText(cursorX - 1, cursorY - 1, 0x0f, String.fromCharCode(197));
        }
        editorUpdateSidebar();
      } else {
        BoardDrawTile(cursorX, cursorY);
      }

      if (drawMode === DRAW_MODE_TEXT) {
        if (editorTryPlaceTextCharacter(keyRaw)) {
          InputKeyPressed = nulKey;
          keyRaw = nulKey;
          key = keyRaw;
        } else if (keyRaw === KEY_BACKSPACE && cursorX > 1) {
          if (editorPrepareModifyTile(cursorX - 1, cursorY)) {
            cursorX -= 1;
          }
        } else if (keyRaw === KEY_ENTER || keyRaw === KEY_ESCAPE) {
          drawMode = DRAW_MODE_OFF;
          InputKeyPressed = nulKey;
          keyRaw = nulKey;
          key = keyRaw;
        }
      }

      if (InputShiftPressed || keyRaw === " ") {
        InputShiftAccepted = true;
        tileAtCursor = Board.Tiles[cursorX][cursorY];
        if (tileAtCursor.Element === E_EMPTY ||
          (ElementDefs[tileAtCursor.Element].PlaceableOnTop && copiedHasStat && cursorPattern > EditorPatternCount) ||
          InputDeltaX !== 0 ||
          InputDeltaY !== 0) {
          editorPlaceTile(cursorX, cursorY);
        } else {
          canModify = editorPrepareModifyTile(cursorX, cursorY);
          if (canModify) {
            Board.Tiles[cursorX][cursorY].Element = E_EMPTY;
          }
        }
      }

      if (InputDeltaX !== 0 || InputDeltaY !== 0) {
        cursorX += InputDeltaX;
        cursorY += InputDeltaY;
        if (cursorX < 1) {
          cursorX = 1;
        } else if (cursorX > BOARD_WIDTH) {
          cursorX = BOARD_WIDTH;
        }
        if (cursorY < 1) {
          cursorY = 1;
        } else if (cursorY > BOARD_HEIGHT) {
          cursorY = BOARD_HEIGHT;
        }
        VideoWriteText(cursorX - 1, cursorY - 1, 0x0f, String.fromCharCode(197));
        if (keyRaw === nulKey && InputJoystickEnabled && typeof mswait === "function") {
          mswait(70);
        }
        InputShiftAccepted = false;
      }

      if (keyRaw === "`") {
        editorDrawRefresh();
      } else if (key === "P") {
        VideoWriteText(62, 21, 0x1f, "       ");
        cursorPattern += 1;
        if (cursorPattern > EditorPatternCount + 1) {
          cursorPattern = 1;
        }
      } else if (key === "C") {
        VideoWriteText(72, 19, 0x1e, "       ");
        VideoWriteText(69, 21, 0x1f, "        ");
        if ((cursorColor & 0x0f) < 0x0f) {
          cursorColor += 1;
        } else {
          cursorColor = (cursorColor & 0xf0) + 9;
        }
      } else if (key === "L") {
        if (editorAskSaveChanged()) {
          if (GameWorldLoad(".ZZT")) {
            if ((World.Info.IsSave || WorldGetFlagPosition("SECRET") >= 0) && !DebugEnabled) {
              showTitleNotice("Can not edit", [
                World.Info.IsSave ? "a saved game!" : ("  " + World.Info.Name + "!")
              ]);
              WorldUnload();
              WorldCreate();
            }
            wasModified = false;
            editorDrawRefresh();
          }
        }
        editorDrawSidebar();
      } else if (key === "S") {
        GameWorldSave("Save world:", LoadedGameFileName, ".ZZT");
        if (InputKeyPressed !== KEY_ESCAPE) {
          wasModified = false;
        }
        editorDrawSidebar();
      } else if (key === "N") {
        if (SidebarPromptYesNo("Make new world? ", false) && editorAskSaveChanged()) {
          WorldUnload();
          WorldCreate();
          wasModified = false;
          editorDrawRefresh();
        }
        editorDrawSidebar();
      } else if (key === "Z") {
        if (SidebarPromptYesNo("Clear board? ", false)) {
          for (i = Board.StatCount; i >= 1; i -= 1) {
            RemoveStat(i);
          }
          BoardCreate();
          wasModified = true;
          editorDrawRefresh();
        } else {
          editorDrawSidebar();
        }
      } else if (key === "B") {
        editorSwitchBoard();
        editorDrawRefresh();
        editorDrawSidebar();
      } else if (key === "I") {
        editorEditBoardInfo();
        editorDrawRefresh();
        editorDrawSidebar();
      } else if (key === "H") {
        TextWindowDisplayFile("editor.hlp", "World editor help");
        editorDrawRefresh();
        editorDrawSidebar();
      } else if (key === "X") {
        editorFloodFill(cursorX, cursorY, cloneTile(Board.Tiles[cursorX][cursorY]));
      } else if (key === "!") {
        editorEditHelpFile();
        editorDrawSidebar();
      } else if (key === "T") {
        editorTransferBoard();
      } else if (key === "|" || keyRaw === "?") {
        GameDebugPrompt();
        editorDrawSidebar();
      } else if (keyRaw === KEY_TAB) {
        if (drawMode === DRAW_MODE_OFF) {
          drawMode = DRAW_MODE_ON;
        } else {
          drawMode = DRAW_MODE_OFF;
        }
      } else if (keyRaw === KEY_F1) {
        editorSelectElementMenu(CATEGORY_ITEM);
      } else if (keyRaw === KEY_F2) {
        editorSelectElementMenu(CATEGORY_CREATURE);
      } else if (keyRaw === KEY_F3) {
        editorSelectElementMenu(CATEGORY_TERRAIN);
      } else if (keyRaw === KEY_F4) {
        if (drawMode !== DRAW_MODE_TEXT) {
          drawMode = DRAW_MODE_TEXT;
        } else {
          drawMode = DRAW_MODE_OFF;
        }
      } else if (keyRaw === KEY_ENTER) {
        var statIdAtCursor: number = GetStatIdAt(cursorX, cursorY);
        if (statIdAtCursor >= 0) {
          editorEditStat(statIdAtCursor);
          editorDrawSidebar();
        } else {
          copiedHasStat = false;
          copiedTile = cloneTile(Board.Tiles[cursorX][cursorY]);
          copiedX = cursorX;
          copiedY = cursorY;
          cursorPattern = EditorPatternCount + 1;
        }
      } else if (key === "Q" || keyRaw === KEY_ESCAPE) {
        editorExitRequested = true;
      }

      if (editorExitRequested) {
        if (!editorAskSaveChanged()) {
          editorExitRequested = false;
          editorDrawSidebar();
        }
      }

      if (keyRaw === nulKey && InputDeltaX === 0 && InputDeltaY === 0 && !InputShiftPressed && typeof mswait === "function") {
        mswait(1);
      }
    }

    InputKeyPressed = String.fromCharCode(0);
    InitElementsGame();
    TransitionDrawToBoard();
  }

  function splitBaseName(path: string): string {
    var p: string = path;
    var idx: number;
    var dot: number;

    idx = p.lastIndexOf("/");
    if (idx >= 0) {
      p = p.slice(idx + 1);
    }
    idx = p.lastIndexOf("\\");
    if (idx >= 0) {
      p = p.slice(idx + 1);
    }

    dot = p.lastIndexOf(".");
    if (dot > 0) {
      p = p.slice(0, dot);
    }
    return p;
  }

  function stripExtension(path: string, extension: string): string {
    var upperExt: string = upperCase(extension);
    if (upperExt.length > 0 && path.length >= upperExt.length) {
      if (upperCase(path.slice(path.length - upperExt.length)) === upperExt) {
        return path.slice(0, path.length - upperExt.length);
      }
    }
    return path;
  }

  function normalizeSlashes(path: string): string {
    return path.split("\\").join("/");
  }

  function toWorldIdentifier(path: string, extension: string): string {
    var normalized: string = normalizeSlashes(path);
    var normalizedUpper: string = upperCase(normalized);
    var marker: string = "ZZT_FILES/";
    var markerPos: number = normalizedUpper.indexOf(marker);
    var relativeWithExt: string;

    if (upperCase(extension) === ".ZZT" && markerPos >= 0) {
      relativeWithExt = normalized.slice(markerPos);
      return stripExtension(relativeWithExt, extension);
    }

    return splitBaseName(path);
  }

  interface WorldBrowseRoot {
    Label: string;
    AbsolutePath: string;
    IdentifierPrefix: string;
  }

  interface WorldBrowseEntry {
    IsDirectory: boolean;
    Name: string;
    RelativePath: string;
    Identifier: string;
  }

  function ensureTrailingSlash(path: string): string {
    var normalized: string = normalizeSlashes(path);
    if (normalized.length > 0 && normalized.charAt(normalized.length - 1) !== "/") {
      normalized += "/";
    }
    return normalized;
  }

  function splitLeafName(path: string): string {
    var p: string = normalizeSlashes(path);
    var slashPos: number = p.lastIndexOf("/");

    if (slashPos >= 0) {
      return p.slice(slashPos + 1);
    }
    return p;
  }

  function trimPathSeparators(path: string): string {
    var p: string = normalizeSlashes(path);

    while (p.length > 0 && p.charAt(0) === "/") {
      p = p.slice(1);
    }
    while (p.length > 0 && p.charAt(p.length - 1) === "/") {
      p = p.slice(0, p.length - 1);
    }
    return p;
  }

  function trimTrailingSlash(path: string): string {
    var p: string = normalizeSlashes(path);
    while (p.length > 0 && p.charAt(p.length - 1) === "/") {
      p = p.slice(0, p.length - 1);
    }
    return p;
  }

  function worldParentRelativeDir(relativeDir: string): string {
    var normalized: string = trimTrailingSlash(relativeDir);
    var slashPos: number = normalized.lastIndexOf("/");

    if (slashPos < 0) {
      return "";
    }
    return normalized.slice(0, slashPos + 1);
  }

  function pathStartsWithIgnoreCase(path: string, prefix: string): boolean {
    var normalizedPath: string = normalizeSlashes(path);
    var normalizedPrefix: string = normalizeSlashes(prefix);
    return upperCase(normalizedPath).indexOf(upperCase(normalizedPrefix)) === 0;
  }

  function pathLeafFromParent(parentPath: string, entryPath: string): string {
    var parent: string = ensureTrailingSlash(parentPath);
    var entry: string = normalizeSlashes(entryPath);
    var child: string;
    var slashPos: number;

    if (pathStartsWithIgnoreCase(entry, parent)) {
      child = entry.slice(parent.length);
    } else {
      child = splitLeafName(entry);
    }

    child = trimPathSeparators(child);
    slashPos = child.indexOf("/");
    if (slashPos >= 0) {
      child = child.slice(0, slashPos);
    }
    return child;
  }

  function worldFileMatchesExtension(path: string, extension: string): boolean {
    var normalized: string = normalizeSlashes(path);
    var upperExt: string = upperCase(extension);

    if (upperExt.length <= 0 || normalized.length < upperExt.length) {
      return false;
    }
    return upperCase(normalized.slice(normalized.length - upperExt.length)) === upperExt;
  }

  function worldBuildIdentifier(prefix: string, relativeWithoutExt: string): string {
    var normalizedPrefix: string = normalizeSlashes(prefix);
    var normalizedRelative: string = trimPathSeparators(relativeWithoutExt);

    if (normalizedPrefix.length > 0 && normalizedPrefix.charAt(normalizedPrefix.length - 1) !== "/") {
      normalizedPrefix += "/";
    }

    if (normalizedPrefix.length > 0) {
      return normalizedPrefix + normalizedRelative;
    }
    return normalizedRelative;
  }

  function addWorldBrowseRoot(
    roots: WorldBrowseRoot[],
    seen: { [id: string]: boolean },
    label: string,
    absolutePath: string,
    identifierPrefix: string
  ): void {
    var normalizedAbsPath: string = ensureTrailingSlash(absolutePath);
    var normalizedPrefix: string = ensureTrailingSlash(normalizeSlashes(identifierPrefix));
    var seenId: string = upperCase(normalizedAbsPath);

    if (normalizedAbsPath.length <= 0) {
      return;
    }
    if (seen[seenId] === true) {
      return;
    }
    if (typeof file_isdir === "function" && !file_isdir(normalizedAbsPath)) {
      return;
    }

    seen[seenId] = true;
    roots.push({
      Label: label,
      AbsolutePath: normalizedAbsPath,
      IdentifierPrefix: normalizedPrefix
    });
  }

  function getWorldBrowseRoots(extension: string): WorldBrowseRoot[] {
    var roots: WorldBrowseRoot[] = [];
    var seen: { [id: string]: boolean } = {};
    var upperExt: string = upperCase(extension);

    if (upperExt === ".SAV") {
      addWorldBrowseRoot(roots, seen, "My saves", currentUserSaveDir(), "");
      return roots;
    }

    addWorldBrowseRoot(roots, seen, "Local zzt_files", execPath("zzt_files/"), "zzt_files/");
    addWorldBrowseRoot(roots, seen, "Shared zzt_files", execPath("../zzt_files/"), "../zzt_files/");

    if (roots.length <= 0) {
      addWorldBrowseRoot(roots, seen, "Game directory", execPath(""), "");
      addWorldBrowseRoot(
        roots,
        seen,
        "Legacy RES",
        execPath("reconstruction-of-zzt-master/RES/"),
        "reconstruction-of-zzt-master/RES/"
      );
    }

    return roots;
  }

  function listWorldDirectoryEntries(root: WorldBrowseRoot, relativeDir: string, extension: string): WorldBrowseEntry[] {
    var entries: WorldBrowseEntry[] = [];
    var dirs: WorldBrowseEntry[] = [];
    var files: WorldBrowseEntry[] = [];
    var absDir: string = pathJoin(root.AbsolutePath, relativeDir);
    var listed: string[] = [];
    var i: number;
    var entryPath: string;
    var childNameRaw: string;
    var childName: string;
    var relPath: string;
    var relWithoutExt: string;
    var seenDirs: { [id: string]: boolean } = {};
    var seenFiles: { [id: string]: boolean } = {};
    var id: string;

    if (typeof directory !== "function") {
      return entries;
    }

    listed = directory(pathJoin(absDir, "*"));
    for (i = 0; i < listed.length; i += 1) {
      entryPath = normalizeSlashes(listed[i]);
      childNameRaw = pathLeafFromParent(absDir, entryPath);
      childName = trimPathSeparators(childNameRaw);
      if (childName.length <= 0 || childName === "." || childName === "..") {
        continue;
      }

      if (typeof file_isdir === "function" && file_isdir(entryPath)) {
        id = upperCase(childName);
        if (seenDirs[id] === true) {
          continue;
        }
        seenDirs[id] = true;
        dirs.push({
          IsDirectory: true,
          Name: childName,
          RelativePath: relativeDir + childName + "/",
          Identifier: ""
        });
        continue;
      }

      if (!worldFileMatchesExtension(entryPath, extension)) {
        continue;
      }

      relPath = relativeDir + childName;
      relWithoutExt = stripExtension(relPath, extension);
      if (relWithoutExt.length <= 0) {
        continue;
      }

      id = upperCase(relWithoutExt);
      if (seenFiles[id] === true) {
        continue;
      }
      seenFiles[id] = true;
      files.push({
        IsDirectory: false,
        Name: splitBaseName(childName),
        RelativePath: relWithoutExt,
        Identifier: worldBuildIdentifier(root.IdentifierPrefix, relWithoutExt)
      });
    }

    dirs.sort(function (a: WorldBrowseEntry, b: WorldBrowseEntry): number {
      var aUpper: string = upperCase(a.Name);
      var bUpper: string = upperCase(b.Name);
      if (aUpper < bUpper) {
        return -1;
      }
      if (aUpper > bUpper) {
        return 1;
      }
      return 0;
    });

    files.sort(function (a: WorldBrowseEntry, b: WorldBrowseEntry): number {
      var aUpper: string = upperCase(a.Name);
      var bUpper: string = upperCase(b.Name);
      if (aUpper < bUpper) {
        return -1;
      }
      if (aUpper > bUpper) {
        return 1;
      }
      return 0;
    });

    entries = entries.concat(dirs);
    entries = entries.concat(files);
    return entries;
  }

  function listFilesByExtension(extension: string): string[] {
    var files: string[] = [];
    var listed: string[] = [];
    var listedPrimary: string[] = [];
    var listedSecondary: string[] = [];
    var listedTertiary: string[] = [];
    var listedLegacy: string[] = [];
    var i: number;
    var entryName: string;
    var upperExt: string = upperCase(extension);
    var seen: { [id: string]: boolean } = {};
    var rawName: string;

    if (typeof directory !== "function") {
      return files;
    }

    if (upperExt === ".SAV") {
      listedPrimary = directory(pathJoin(currentUserSaveDir(), "*"));
      listedSecondary = directory(execPath("zzt_files/*"));
      listedTertiary = directory(execPath("*"));
      listed = listed.concat(listedPrimary);
      listed = listed.concat(listedSecondary);
      listed = listed.concat(listedTertiary);
    } else {
      // Preferred world drop location: /xtrn/zzt/zzt_files (+ one subdirectory level for packs).
      listedPrimary = directory(execPath("zzt_files/*"));
      listedSecondary = directory(execPath("zzt_files/*/*"));

      // Shared location: /xtrn/zzt_files (+ one subdirectory level for packs).
      listedTertiary = directory(execPath("../zzt_files/*"));
      listedLegacy = directory(execPath("../zzt_files/*/*"));

      // Legacy fallback locations kept for compatibility.
      listed = listed.concat(listedPrimary);
      listed = listed.concat(listedSecondary);
      listed = listed.concat(listedTertiary);
      listed = listed.concat(listedLegacy);
      listed = listed.concat(directory(execPath("*")));
      listed = listed.concat(directory(execPath("reconstruction-of-zzt-master/RES/*")));
    }

    for (i = 0; i < listed.length; i += 1) {
      if (typeof file_isdir === "function" && file_isdir(listed[i])) {
        continue;
      }

      entryName = toWorldIdentifier(listed[i], extension);
      if (entryName.length <= 0) {
        continue;
      }

      if (upperExt.length > 0) {
        rawName = listed[i];
        if (upperCase(rawName.slice(rawName.length - upperExt.length)) !== upperExt) {
          continue;
        }
      }

      if (seen[upperCase(entryName)] === true) {
        continue;
      }
      seen[upperCase(entryName)] = true;
      files.push(entryName);
    }

    files.sort(function (a: string, b: string): number {
      if (upperExt === ".SAV") {
        if (upperCase(a) < upperCase(b)) {
          return -1;
        }
        if (upperCase(a) > upperCase(b)) {
          return 1;
        }
        return 0;
      }

      var aUpper: string = upperCase(a);
      var bUpper: string = upperCase(b);
      var aPreferred: boolean = aUpper.indexOf("ZZT_FILES/") === 0;
      var bPreferred: boolean = bUpper.indexOf("ZZT_FILES/") === 0;

      if (aPreferred && !bPreferred) {
        return -1;
      }
      if (!aPreferred && bPreferred) {
        return 1;
      }
      if (aUpper < bUpper) {
        return -1;
      }
      if (aUpper > bUpper) {
        return 1;
      }
      return 0;
    });

    return files;
  }

  function worldDisplayName(name: string, extension: string): string {
    var i: number;
    var nameUpper: string = upperCase(name);
    var fileId: string;

    if (nameUpper.indexOf("ZZT_FILES/") === 0) {
      return name.slice("zzt_files/".length);
    }

    if (upperCase(extension) !== ".ZZT") {
      return name;
    }

    fileId = splitBaseName(name);
    for (i = 0; i < WorldFileDescCount; i += 1) {
      if (upperCase(WorldFileDescKeys[i]) === upperCase(fileId)) {
        return WorldFileDescValues[i];
      }
    }
    return name;
  }

  function gameWorldLoadFlat(extension: string): boolean {
    var state: TextWindowState = {
      Selectable: true,
      LineCount: 0,
      LinePos: 1,
      Lines: [],
      Hyperlink: "",
      Title: upperCase(extension) === ".ZZT" ? "ZZT Worlds" : "Saved Games",
      LoadedFilename: "",
      ScreenCopy: []
    };
    var files: string[] = listFilesByExtension(extension);
    var i: number;
    var selected: string;

    for (i = 0; i < files.length; i += 1) {
      TextWindowAppend(state, worldDisplayName(files[i], extension));
    }
    TextWindowAppend(state, "Exit");

    TextWindowDrawOpen(state);
    TextWindowSelect(state, false, false);
    TextWindowDrawClose(state);

    if (TextWindowRejected || state.LinePos >= state.LineCount) {
      TextWindowFree(state);
      return false;
    }

    selected = files[state.LinePos - 1];
    TextWindowFree(state);

    if (WorldLoad(selected, extension, false)) {
      TransitionDrawToFill(String.fromCharCode(219), 0x44);
      return true;
    }
    return false;
  }

  function gameWorldLoadBrowse(extension: string): boolean {
    var state: TextWindowState = {
      Selectable: true,
      LineCount: 0,
      LinePos: 1,
      Lines: [],
      Hyperlink: "",
      Title: "ZZT Worlds",
      LoadedFilename: "",
      ScreenCopy: []
    };
    var roots: WorldBrowseRoot[] = getWorldBrowseRoots(extension);
    var currentRootIndex: number = 0;
    var currentRelativeDir: string = "";
    var showRootList: boolean = roots.length > 1;
    var includeParent: boolean = false;
    var entries: WorldBrowseEntry[] = [];
    var selectedLine: number;
    var selectedLineCount: number;
    var entryIndex: number;
    var selectedEntry: WorldBrowseEntry;
    var location: string;
    var i: number;

    if (roots.length <= 0) {
      return gameWorldLoadFlat(extension);
    }

    while (!runtime.isTerminated()) {
      TextWindowInitState(state);

      if (showRootList) {
        state.Title = "ZZT World Folders";
        for (i = 0; i < roots.length; i += 1) {
          TextWindowAppend(state, roots[i].Label);
        }
        TextWindowAppend(state, "Exit");
      } else {
        entries = listWorldDirectoryEntries(roots[currentRootIndex], currentRelativeDir, extension);
        includeParent = currentRelativeDir.length > 0 || roots.length > 1;
        location = roots[currentRootIndex].Label;
        if (currentRelativeDir.length > 0) {
          location += "/" + trimTrailingSlash(currentRelativeDir);
        }
        state.Title = "ZZT Worlds: " + location;

        if (includeParent) {
          TextWindowAppend(state, "[..]");
        }

        for (i = 0; i < entries.length; i += 1) {
          if (entries[i].IsDirectory) {
            TextWindowAppend(state, entries[i].Name + "/");
          } else {
            TextWindowAppend(state, worldDisplayName(entries[i].Name, extension));
          }
        }

        TextWindowAppend(state, "Exit");
      }

      TextWindowDrawOpen(state);
      TextWindowSelect(state, false, false);
      TextWindowDrawClose(state);

      selectedLine = state.LinePos;
      selectedLineCount = state.LineCount;
      TextWindowFree(state);

      if (TextWindowRejected || selectedLine >= selectedLineCount) {
        return false;
      }

      if (showRootList) {
        if (selectedLine >= roots.length + 1) {
          return false;
        }
        currentRootIndex = selectedLine - 1;
        currentRelativeDir = "";
        showRootList = false;
        continue;
      }

      entryIndex = selectedLine - 1;

      if (includeParent) {
        if (entryIndex === 0) {
          if (currentRelativeDir.length > 0) {
            currentRelativeDir = worldParentRelativeDir(currentRelativeDir);
          } else {
            showRootList = roots.length > 1;
          }
          continue;
        }
        entryIndex -= 1;
      }

      if (entryIndex >= entries.length) {
        return false;
      }

      selectedEntry = entries[entryIndex];
      if (selectedEntry.IsDirectory) {
        currentRelativeDir = selectedEntry.RelativePath;
        continue;
      }

      if (WorldLoad(selectedEntry.Identifier, extension, false)) {
        TransitionDrawToFill(String.fromCharCode(219), 0x44);
        return true;
      }
      return false;
    }

    TextWindowFree(state);
    return false;
  }

  export function GameWorldLoad(extension: string): boolean {
    if (upperCase(extension) === ".ZZT") {
      return gameWorldLoadBrowse(extension);
    }
    return gameWorldLoadFlat(extension);
  }

  export function GameWorldSave(prompt: string, filename: string, extension: string): void {
    var newFilename: string = SidebarPromptString(prompt, extension, filename, PROMPT_ALPHANUM);
    if (InputKeyPressed === KEY_ESCAPE || newFilename.length <= 0) {
      return;
    }

    if (upperCase(extension) === ".SAV") {
      SavedGameFileName = newFilename;
    } else if (upperCase(extension) === ".ZZT") {
      World.Info.Name = newFilename;
    }

    WorldSave(newFilename, extension);
  }

  export function GetStatIdAt(x: number, y: number): number {
    var i: number;
    for (i = 0; i <= Board.StatCount; i += 1) {
      if (Board.Stats[i].X === x && Board.Stats[i].Y === y) {
        return i;
      }
    }
    return -1;
  }

  export function BoardPrepareTileForPlacement(x: number, y: number): boolean {
    var statId: number = GetStatIdAt(x, y);
    var result: boolean;

    if (statId > 0) {
      RemoveStat(statId);
      result = true;
    } else if (statId < 0) {
      if (!ElementDefs[Board.Tiles[x][y].Element].PlaceableOnTop) {
        Board.Tiles[x][y].Element = E_EMPTY;
        Board.Tiles[x][y].Color = 0;
      }
      result = true;
    } else {
      // Player tile cannot be modified.
      result = false;
    }

    BoardDrawTile(x, y);
    return result;
  }

  export function AddStat(tx: number, ty: number, element: number, color: number, cycle: number, template: Stat): void {
    var stat: Stat;

    if (Board.StatCount >= MAX_STAT) {
      return;
    }

    Board.StatCount += 1;
    stat = cloneStat(template);
    stat.X = tx;
    stat.Y = ty;
    stat.Cycle = cycle;
    stat.Under = cloneTile(Board.Tiles[tx][ty]);
    stat.DataPos = 0;
    Board.Stats[Board.StatCount] = stat;

    if (ElementDefs[Board.Tiles[tx][ty].Element].PlaceableOnTop) {
      Board.Tiles[tx][ty].Color = (color & 0x0f) + (Board.Tiles[tx][ty].Color & 0x70);
    } else {
      Board.Tiles[tx][ty].Color = color;
    }
    Board.Tiles[tx][ty].Element = element;
    if (ty > 0) {
      BoardDrawTile(tx, ty);
    }
  }

  export function RemoveStat(statId: number): void {
    var stat: Stat;
    var i: number;

    if (statId < 0 || statId > Board.StatCount) {
      return;
    }

    stat = Board.Stats[statId];

    if (statId < CurrentStatTicked) {
      CurrentStatTicked -= 1;
    }

    Board.Tiles[stat.X][stat.Y] = cloneTile(stat.Under);
    if (stat.Y > 0) {
      BoardDrawTile(stat.X, stat.Y);
    }

    for (i = 0; i <= Board.StatCount; i += 1) {
      if (Board.Stats[i].Follower >= statId) {
        if (Board.Stats[i].Follower === statId) {
          Board.Stats[i].Follower = -1;
        } else {
          Board.Stats[i].Follower -= 1;
        }
      }
      if (Board.Stats[i].Leader >= statId) {
        if (Board.Stats[i].Leader === statId) {
          Board.Stats[i].Leader = -1;
        } else {
          Board.Stats[i].Leader -= 1;
        }
      }
    }

    for (i = statId + 1; i <= Board.StatCount; i += 1) {
      Board.Stats[i - 1] = cloneStat(Board.Stats[i]);
    }

    Board.Stats[Board.StatCount] = createDefaultStat();
    Board.StatCount -= 1;
  }

  export function MoveStat(statId: number, newX: number, newY: number): void {
    var stat: Stat = Board.Stats[statId];
    var oldX: number = stat.X;
    var oldY: number = stat.Y;
    var oldUnder: Tile = cloneTile(stat.Under);
    var ix: number;
    var iy: number;
    var oldLit: boolean;
    var newLit: boolean;

    stat.Under = cloneTile(Board.Tiles[newX][newY]);

    if (Board.Tiles[oldX][oldY].Element === E_PLAYER) {
      Board.Tiles[newX][newY].Color = Board.Tiles[oldX][oldY].Color;
    } else if (Board.Tiles[newX][newY].Element === E_EMPTY) {
      Board.Tiles[newX][newY].Color = Board.Tiles[oldX][oldY].Color & 0x0f;
    } else {
      Board.Tiles[newX][newY].Color = (Board.Tiles[oldX][oldY].Color & 0x0f) + (Board.Tiles[newX][newY].Color & 0x70);
    }

    Board.Tiles[newX][newY].Element = Board.Tiles[oldX][oldY].Element;
    Board.Tiles[oldX][oldY] = oldUnder;

    stat.X = newX;
    stat.Y = newY;

    BoardDrawTile(newX, newY);
    BoardDrawTile(oldX, oldY);

    if (statId === 0 && Board.Info.IsDark && World.Info.TorchTicks > 0) {
      if (((oldX - newX) * (oldX - newX) + (oldY - newY) * (oldY - newY)) === 1) {
        for (ix = newX - TORCH_DX - 3; ix <= newX + TORCH_DX + 3; ix += 1) {
          if (ix < 1 || ix > BOARD_WIDTH) {
            continue;
          }
          for (iy = newY - TORCH_DY - 3; iy <= newY + TORCH_DY + 3; iy += 1) {
            if (iy < 1 || iy > BOARD_HEIGHT) {
              continue;
            }
            oldLit = (((ix - oldX) * (ix - oldX)) + ((iy - oldY) * (iy - oldY) * 2)) < TORCH_DIST_SQR;
            newLit = (((ix - newX) * (ix - newX)) + ((iy - newY) * (iy - newY) * 2)) < TORCH_DIST_SQR;
            if (oldLit !== newLit) {
              BoardDrawTile(ix, iy);
            }
          }
        }
      } else {
        DrawPlayerSurroundings(oldX, oldY, 0);
        DrawPlayerSurroundings(newX, newY, 0);
      }
    }
  }

  export function MovePlayerStat(newX: number, newY: number): void {
    var stat: Stat = Board.Stats[0];
    var oldX: number = stat.X;
    var oldY: number = stat.Y;
    var ix: number;
    var iy: number;
    var oldLit: boolean;
    var newLit: boolean;

    if (Board.Tiles[oldX][oldY].Element === E_PLAYER) {
      MoveStat(0, newX, newY);
      return;
    }

    // Preserve the current non-player tile (e.g. passage) and correctly
    // set what the player is standing over for subsequent MoveStat calls.
    stat.Under = cloneTile(Board.Tiles[newX][newY]);
    stat.X = newX;
    stat.Y = newY;
    Board.Tiles[newX][newY].Element = E_PLAYER;
    Board.Tiles[newX][newY].Color = ElementDefs[E_PLAYER].Color;

    BoardDrawTile(oldX, oldY);
    BoardDrawTile(newX, newY);

    if (Board.Info.IsDark && World.Info.TorchTicks > 0) {
      if (((oldX - newX) * (oldX - newX) + (oldY - newY) * (oldY - newY)) === 1) {
        for (ix = newX - TORCH_DX - 3; ix <= newX + TORCH_DX + 3; ix += 1) {
          if (ix < 1 || ix > BOARD_WIDTH) {
            continue;
          }
          for (iy = newY - TORCH_DY - 3; iy <= newY + TORCH_DY + 3; iy += 1) {
            if (iy < 1 || iy > BOARD_HEIGHT) {
              continue;
            }
            oldLit = (((ix - oldX) * (ix - oldX)) + ((iy - oldY) * (iy - oldY) * 2)) < TORCH_DIST_SQR;
            newLit = (((ix - newX) * (ix - newX)) + ((iy - newY) * (iy - newY) * 2)) < TORCH_DIST_SQR;
            if (oldLit !== newLit) {
              BoardDrawTile(ix, iy);
            }
          }
        }
      } else {
        DrawPlayerSurroundings(oldX, oldY, 0);
        DrawPlayerSurroundings(newX, newY, 0);
      }
    }
  }

  export function ElementMove(oldX: number, oldY: number, newX: number, newY: number): void {
    var statId: number = GetStatIdAt(oldX, oldY);
    if (statId >= 0) {
      MoveStat(statId, newX, newY);
    } else {
      Board.Tiles[newX][newY] = cloneTile(Board.Tiles[oldX][oldY]);
      Board.Tiles[oldX][oldY] = createTile(E_EMPTY, 0);
      BoardDrawTile(newX, newY);
      BoardDrawTile(oldX, oldY);
    }
  }

  export function ElementPushablePush(x: number, y: number, deltaX: number, deltaY: number): void {
    var tile: Tile = Board.Tiles[x][y];
    var nextX: number = x + deltaX;
    var nextY: number = y + deltaY;
    var nextElement: number;

    if (!(((tile.Element === E_SLIDER_NS) && (deltaX === 0)) ||
          ((tile.Element === E_SLIDER_EW) && (deltaY === 0)) ||
          ElementDefs[tile.Element].Pushable)) {
      return;
    }

    nextElement = Board.Tiles[nextX][nextY].Element;
    if (!ElementDefs[nextElement].Walkable && nextElement !== E_PLAYER) {
      ElementPushablePush(nextX, nextY, deltaX, deltaY);
    }

    nextElement = Board.Tiles[nextX][nextY].Element;
    if (!ElementDefs[nextElement].Walkable &&
        ElementDefs[nextElement].Destructible &&
        nextElement !== E_PLAYER) {
      BoardDamageTile(nextX, nextY);
    }

    if (ElementDefs[Board.Tiles[nextX][nextY].Element].Walkable) {
      ElementMove(x, y, nextX, nextY);
    }
  }

  export function DisplayMessage(ticks: number, message: string): void {
    var statId: number = GetStatIdAt(0, 0);
    var timerStat: Stat;
    if (statId !== -1) {
      RemoveStat(statId);
      BoardDrawBorder();
    }

    if (message.length <= 0) {
      Board.Info.Message = "";
      SidebarClearLine(24);
      return;
    }

    timerStat = createDefaultStat();
    AddStat(0, 0, E_MESSAGE_TIMER, 0, 1, timerStat);
    Board.Stats[Board.StatCount].P2 = Math.floor(ticks / (TickTimeDuration + 1));
    if (Board.Stats[Board.StatCount].P2 <= 0) {
      Board.Stats[Board.StatCount].P2 = ticks;
    }
    Board.Info.Message = message;
  }

  export function GameUpdateSidebar(): void {
    var keyId: number;
    var torchBarId: number;
    var worldName: string;

    if (GameStateElement === E_PLAYER) {
      if (Board.Info.TimeLimitSec > 0) {
        VideoWriteText(64, 6, 0x1e, "   Time:");
        VideoWriteText(72, 6, 0x1e, padRight(String(Board.Info.TimeLimitSec - World.Info.BoardTimeSec), 4));
      } else {
        SidebarClearLine(6);
      }

      if (World.Info.Health < 0) {
        World.Info.Health = 0;
      }

      VideoWriteText(64, 7, 0x1e, " Health:");
      VideoWriteText(64, 8, 0x1e, "   Ammo:");
      VideoWriteText(64, 9, 0x1e, "Torches:");
      VideoWriteText(64, 10, 0x1e, "   Gems:");
      VideoWriteText(64, 11, 0x1e, "  Score:");
      VideoWriteText(64, 12, 0x1e, "   Keys:");

      VideoWriteText(72, 7, 0x1e, padRight(String(World.Info.Health), 4));
      VideoWriteText(72, 8, 0x1e, padRight(String(World.Info.Ammo), 5));
      VideoWriteText(72, 9, 0x1e, padRight(String(World.Info.Torches), 4));
      VideoWriteText(72, 10, 0x1e, padRight(String(World.Info.Gems), 4));
      VideoWriteText(72, 11, 0x1e, padRight(String(World.Info.Score), 6));

      if (World.Info.TorchTicks <= 0) {
        VideoWriteText(75, 9, 0x16, "    ");
      } else {
        for (torchBarId = 2; torchBarId <= 5; torchBarId += 1) {
          if (torchBarId <= Math.floor((World.Info.TorchTicks * 5) / TORCH_DURATION)) {
            VideoWriteText(73 + torchBarId, 9, 0x16, String.fromCharCode(177));
          } else {
            VideoWriteText(73 + torchBarId, 9, 0x16, String.fromCharCode(176));
          }
        }
      }

      for (keyId = 1; keyId <= 7; keyId += 1) {
        if (World.Info.Keys[keyId]) {
          VideoWriteText(71 + keyId, 12, 0x18 + keyId, ElementDefs[E_KEY].Character);
        } else {
          VideoWriteText(71 + keyId, 0x0c, 0x1f, " ");
        }
      }

      if (SoundEnabled) {
        VideoWriteText(65, 15, 0x1f, " Be quiet");
      } else {
        VideoWriteText(65, 15, 0x1f, " Be noisy");
      }
    } else if (GameStateElement === E_MONITOR) {
      worldName = World.Info.Name.length > 0 ? World.Info.Name : "Untitled";
      VideoWriteText(62, 5, 0x1b, "Pick a command:");
      VideoWriteText(65, 7, 0x1e, " World:");
      VideoWriteText(69, 8, 0x1f, padRight(worldName, 10));
    }
  }

  export function DamageStat(attackerStatId: number): void {
    var stat: Stat;
    var oldX: number;
    var oldY: number;
    var destroyedElement: number;

    if (attackerStatId < 0 || attackerStatId > Board.StatCount) {
      return;
    }

    stat = Board.Stats[attackerStatId];
    if (attackerStatId === 0) {
      if (World.Info.Health > 0) {
        World.Info.Health -= 10;
        GameUpdateSidebar();
        DisplayMessage(120, "Ouch!");
        Board.Tiles[stat.X][stat.Y].Color = 0x70 + (ElementDefs[E_PLAYER].Color % 0x10);

        if (World.Info.Health > 0) {
          World.Info.BoardTimeSec = 0;
          if (Board.Info.ReenterWhenZapped) {
            SoundQueue(4, "\x20\x01\x23\x01\x27\x01\x30\x01\x10\x01");

            Board.Tiles[stat.X][stat.Y].Element = E_EMPTY;
            BoardDrawTile(stat.X, stat.Y);
            oldX = stat.X;
            oldY = stat.Y;
            stat.X = Board.Info.StartPlayerX;
            stat.Y = Board.Info.StartPlayerY;
            DrawPlayerSurroundings(oldX, oldY, 0);
            DrawPlayerSurroundings(stat.X, stat.Y, 0);
            GamePaused = true;
          }
          SoundQueue(4, "\x10\x01\x20\x01\x13\x01\x23\x01");
        } else {
          SoundQueue(5, "\x20\x03\x23\x03\x27\x03\x30\x03\x27\x03\x2A\x03\x32\x03\x37\x03\x35\x03\x38\x03\x40\x03\x45\x03\x10\x0A");
        }
      }
      return;
    }

    destroyedElement = Board.Tiles[stat.X][stat.Y].Element;
    if (destroyedElement === E_BULLET) {
      SoundQueue(3, "\x20\x01");
    } else if (destroyedElement !== E_OBJECT) {
      SoundQueue(3, "\x40\x01\x10\x01\x50\x01\x30\x01");
    }

    RemoveStat(attackerStatId);
    BoardDrawTile(stat.X, stat.Y);
  }

  export function BoardDamageTile(x: number, y: number): void {
    var statId: number = GetStatIdAt(x, y);
    if (statId !== -1) {
      DamageStat(statId);
    } else {
      Board.Tiles[x][y].Element = E_EMPTY;
      BoardDrawTile(x, y);
    }
  }

  export function BoardAttack(attackerStatId: number, x: number, y: number): void {
    if (attackerStatId === 0 && World.Info.EnergizerTicks > 0) {
      World.Info.Score += ElementDefs[Board.Tiles[x][y].Element].ScoreValue;
      GameUpdateSidebar();
    } else {
      DamageStat(attackerStatId);
    }

    if (attackerStatId > 0 && attackerStatId <= CurrentStatTicked) {
      CurrentStatTicked -= 1;
    }

    if (Board.Tiles[x][y].Element === E_PLAYER && World.Info.EnergizerTicks > 0) {
      World.Info.Score += ElementDefs[Board.Tiles[Board.Stats[attackerStatId].X][Board.Stats[attackerStatId].Y].Element].ScoreValue;
      GameUpdateSidebar();
    } else {
      BoardDamageTile(x, y);
      SoundQueue(2, "\x10\x01");
    }
  }

  export function BoardShoot(element: number, tx: number, ty: number, deltaX: number, deltaY: number, source: number): boolean {
    var targetElement: number = Board.Tiles[tx + deltaX][ty + deltaY].Element;
    var sourceIsEnemy: boolean = source === SHOT_SOURCE_ENEMY;

    if (ElementDefs[targetElement].Walkable || targetElement === E_WATER) {
      AddStat(tx + deltaX, ty + deltaY, element, ElementDefs[element].Color, 1, createDefaultStat());
      Board.Stats[Board.StatCount].P1 = source;
      Board.Stats[Board.StatCount].StepX = deltaX;
      Board.Stats[Board.StatCount].StepY = deltaY;
      Board.Stats[Board.StatCount].P2 = 100;
      return true;
    }

    if (targetElement === E_BREAKABLE ||
        (ElementDefs[targetElement].Destructible &&
         ((targetElement === E_PLAYER) === sourceIsEnemy) &&
         World.Info.EnergizerTicks <= 0)) {
      BoardDamageTile(tx + deltaX, ty + deltaY);
      SoundQueue(2, "\x10\x01");
      return true;
    }

    return false;
  }

  export function BoardEnter(): void {
    Board.Info.StartPlayerX = Board.Stats[0].X;
    Board.Info.StartPlayerY = Board.Stats[0].Y;
    SoundWorldMusicOnBoardChanged(Board.Name);
    if (Board.Info.IsDark && MessageHintTorchNotShown) {
      DisplayMessage(180, "Room is dark - you need to light a torch!");
      MessageHintTorchNotShown = false;
    }
    World.Info.BoardTimeSec = 0;
    World.Info.BoardTimeHsec = currentHsecs();
    GameUpdateSidebar();
  }

  export function BoardPassageTeleport(x: number, y: number): void {
    var oldBoard: number = World.Info.CurrentBoard;
    var col: number = Board.Tiles[x][y].Color;
    var passageStat: number = GetStatIdAt(x, y);
    var fallbackPassageStat: number = -1;
    var ix: number;
    var iy: number;
    var sx: number;
    var sy: number;
    var newX: number = 0;
    var newY: number = 0;

    if (passageStat < 0) {
      for (ix = 1; ix <= Board.StatCount; ix += 1) {
        sx = Board.Stats[ix].X;
        sy = Board.Stats[ix].Y;
        if (sx < 0 || sx > BOARD_WIDTH + 1 || sy < 0 || sy > BOARD_HEIGHT + 1) {
          continue;
        }
        if (Board.Tiles[sx][sy].Element !== E_PASSAGE) {
          continue;
        }
        if (sx === x && sy === y) {
          passageStat = ix;
          break;
        }
        if (Board.Tiles[sx][sy].Color === col) {
          if (fallbackPassageStat < 0) {
            fallbackPassageStat = ix;
          } else {
            fallbackPassageStat = -2;
          }
        }
      }

      if (passageStat < 0 && fallbackPassageStat >= 0) {
        passageStat = fallbackPassageStat;
      }
      if (passageStat < 0) {
        return;
      }
    }

    BoardChange(Board.Stats[passageStat].P3);

    for (ix = 1; ix <= BOARD_WIDTH; ix += 1) {
      for (iy = 1; iy <= BOARD_HEIGHT; iy += 1) {
        if (Board.Tiles[ix][iy].Element === E_PASSAGE && Board.Tiles[ix][iy].Color === col) {
          newX = ix;
          newY = iy;
        }
      }
    }

    Board.Tiles[Board.Stats[0].X][Board.Stats[0].Y].Element = E_EMPTY;
    Board.Tiles[Board.Stats[0].X][Board.Stats[0].Y].Color = 0;

    if (newX !== 0) {
      Board.Stats[0].X = newX;
      Board.Stats[0].Y = newY;
    } else {
      BoardChange(oldBoard);
      return;
    }

    GamePaused = true;
    SoundQueue(4, "\x30\x01\x34\x01\x37\x01\x31\x01\x35\x01\x38\x01\x32\x01\x36\x01\x39\x01\x33\x01\x37\x01\x3A\x01\x34\x01\x38\x01\x40\x01");
    TransitionDrawBoardChange();
    BoardEnter();
  }

  export function GamePromptEndPlay(): void {
    if (World.Info.Health <= 0) {
      GamePlayExitRequested = true;
      BoardDrawBorder();
    } else {
      GamePlayExitRequested = SidebarPromptYesNo("End this game? ", true);
    }
    InputKeyPressed = String.fromCharCode(0);
  }

  export function GameDebugPrompt(): void {
    var input: string = "";
    var i: number;
    var toggle: boolean = true;
    var key: string;

    SidebarClearLine(4);
    SidebarClearLine(5);

    input = PromptString(63, 5, 0x1e, 0x0f, 11, PROMPT_ANY, input);
    input = upperCase(input);

    if (input.length > 0) {
      key = input.charAt(0);
      if (key === "+" || key === "-") {
        toggle = key === "+";
        input = input.slice(1);
        if (input.length > 0) {
          if (toggle) {
            WorldSetFlag(input);
          } else {
            WorldClearFlag(input);
          }
        }
      }
    }

    DebugEnabled = WorldGetFlagPosition("DEBUG") >= 0;

    if (input === "HEALTH") {
      World.Info.Health += 50;
    } else if (input === "AMMO") {
      World.Info.Ammo += 5;
    } else if (input === "KEYS") {
      for (i = 1; i <= 7; i += 1) {
        World.Info.Keys[i] = true;
      }
    } else if (input === "TORCHES") {
      World.Info.Torches += 3;
    } else if (input === "TIME") {
      World.Info.BoardTimeSec -= 30;
    } else if (input === "GEMS") {
      World.Info.Gems += 5;
    } else if (input === "DARK") {
      Board.Info.IsDark = toggle;
      TransitionDrawToBoard();
    } else if (input === "ZAP") {
      for (i = 0; i < 4; i += 1) {
        BoardDamageTile(Board.Stats[0].X + NeighborDeltaX[i], Board.Stats[0].Y + NeighborDeltaY[i]);
        Board.Tiles[Board.Stats[0].X + NeighborDeltaX[i]][Board.Stats[0].Y + NeighborDeltaY[i]].Element = E_EMPTY;
        BoardDrawTile(Board.Stats[0].X + NeighborDeltaX[i], Board.Stats[0].Y + NeighborDeltaY[i]);
      }
    }

    SidebarClearLine(4);
    SidebarClearLine(5);
    SoundQueue(10, "\x27\x04");
    GameUpdateSidebar();
  }

  function drawGameSidebar(): void {
    SidebarClear();
    SidebarClearLine(0);
    SidebarClearLine(1);
    SidebarClearLine(2);
    VideoWriteText(61, 0, 0x1f, "    - - - - -     ");
    VideoWriteText(62, 1, 0x70, "      ZZT      ");
    VideoWriteText(61, 2, 0x1f, "    - - - - -     ");

    if (GameStateElement === E_PLAYER) {
      VideoWriteText(64, 7, 0x1e, " Health:");
      VideoWriteText(64, 8, 0x1e, "   Ammo:");
      VideoWriteText(64, 9, 0x1e, "Torches:");
      VideoWriteText(64, 10, 0x1e, "   Gems:");
      VideoWriteText(64, 11, 0x1e, "  Score:");
      VideoWriteText(64, 12, 0x1e, "   Keys:");
      VideoWriteText(62, 7, 0x1f, ElementDefs[E_PLAYER].Character);
      VideoWriteText(62, 8, 0x1b, ElementDefs[E_AMMO].Character);
      VideoWriteText(62, 9, 0x16, ElementDefs[E_TORCH].Character);
      VideoWriteText(62, 10, 0x1b, ElementDefs[E_GEM].Character);
      VideoWriteText(62, 12, 0x1f, ElementDefs[E_KEY].Character);
      VideoWriteText(62, 14, 0x70, " T ");
      VideoWriteText(65, 14, 0x1f, " Torch");
      VideoWriteText(62, 15, 0x30, " B ");
      VideoWriteText(62, 16, 0x70, " H ");
      VideoWriteText(65, 16, 0x1f, " Help");
      VideoWriteText(64, 18, 0x30, " Arrows ");
      VideoWriteText(72, 18, 0x1f, " Move");
      VideoWriteText(61, 19, 0x70, " Spacebar ");
      VideoWriteText(72, 19, 0x1f, " Shoot");
      VideoWriteText(62, 21, 0x70, " S ");
      VideoWriteText(65, 21, 0x1f, " Save game");
      VideoWriteText(62, 22, 0x30, " P ");
      VideoWriteText(65, 22, 0x1f, " Pause");
      VideoWriteText(62, 23, 0x70, " Q ");
      VideoWriteText(65, 23, 0x1f, " Quit");
    } else {
      VideoWriteText(62, 21, 0x70, " S ");
      VideoWriteText(65, 21, 0x1e, " Game speed:");
      VideoWriteText(77, 21, 0x1f, String(TickSpeed));
      VideoWriteText(62, 7, 0x30, " W ");
      VideoWriteText(62, 11, 0x70, " P ");
      VideoWriteText(62, 12, 0x30, " R ");
      VideoWriteText(62, 13, 0x70, " Q ");
      VideoWriteText(62, 16, 0x30, " A ");
      VideoWriteText(62, 17, 0x70, " H ");
      VideoWriteText(65, 11, 0x1f, " Play");
      VideoWriteText(65, 12, 0x1e, " Restore game");
      VideoWriteText(65, 13, 0x1e, " Quit");
      VideoWriteText(65, 16, 0x1f, " About ZZT!");
      VideoWriteText(65, 17, 0x1e, " High Scores");
      if (EditorEnabled) {
        VideoWriteText(62, 18, 0x30, " E ");
        VideoWriteText(65, 18, 0x1e, " Board Editor");
      }
    }
  }

  export function GamePlayLoop(boardChanged: boolean): void {
    var stat: Stat;
    var statElement: number;
    var key: string;
    var context: TouchContext;
    var loopCounter: number = 0;
    var sleptThisLoop: boolean;
    var playerTickedThisLoop: boolean;
    var noInputFallback: boolean =
      !(typeof console !== "undefined" && typeof console.inkey === "function");

    drawGameSidebar();
    GameUpdateSidebar();
    InputFirePressed = false;
    InputTorchPressed = false;

    if (JustStarted) {
      if (StartupWorldFileName.length > 0) {
        SidebarClearLine(8);
        VideoWriteText(69, 8, 0x1f, StartupWorldFileName);
        if (!WorldLoad(StartupWorldFileName, ".ZZT", true)) {
          WorldCreate();
        }
      }
      ReturnBoardId = World.Info.CurrentBoard;
      BoardChange(0);
      JustStarted = false;
    }

    Board.Tiles[Board.Stats[0].X][Board.Stats[0].Y].Element = GameStateElement;
    Board.Tiles[Board.Stats[0].X][Board.Stats[0].Y].Color = ElementDefs[GameStateElement].Color;

    if (boardChanged) {
      TransitionDrawBoardChange();
    }

    if (GameStateElement === E_PLAYER) {
      SoundWorldMusicOnBoardChanged(Board.Name);
    }

    TickTimeDuration = TickSpeed * 2;
    GamePlayExitRequested = false;
    CurrentTick = randomInt(100);
    CurrentStatTicked = Board.StatCount + 1;
    TickTimeCounter = currentHsecs();

    while (!runtime.isTerminated()) {
      sleptThisLoop = false;
      playerTickedThisLoop = false;
      SoundUpdate();

      if (GamePlayExitRequested) {
        break;
      }

      if (GamePaused) {
        VideoWriteText(64, 5, 0x1f, "Pausing...");
        InputUpdate();
        key = upperCase(InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0) : String.fromCharCode(0));
        if (key === KEY_ESCAPE || key === "Q") {
          GamePromptEndPlay();
        }

        if ((InputDeltaX !== 0 || InputDeltaY !== 0) && World.Info.Health > 0) {
          PlayerDirX = InputDeltaX;
          PlayerDirY = InputDeltaY;
          context = {
            DeltaX: InputDeltaX,
            DeltaY: InputDeltaY
          };

          invokeElementTouch(Board.Stats[0].X + context.DeltaX, Board.Stats[0].Y + context.DeltaY, 0, context);
          if ((context.DeltaX !== 0 || context.DeltaY !== 0) &&
              ElementDefs[Board.Tiles[Board.Stats[0].X + context.DeltaX][Board.Stats[0].Y + context.DeltaY].Element].Walkable) {
            MovePlayerStat(Board.Stats[0].X + context.DeltaX, Board.Stats[0].Y + context.DeltaY);
          }

          if (!GamePlayExitRequested) {
            GamePaused = false;
            SidebarClearLine(5);
            CurrentTick = randomInt(100);
            CurrentStatTicked = Board.StatCount + 1;
            World.Info.IsSave = true;
          }
        } else if (
          (key === "P" || key === KEY_ENTER) &&
          World.Info.Health > 0 &&
          !GamePlayExitRequested &&
          Board.Tiles[Board.Stats[0].X][Board.Stats[0].Y].Element === E_PLAYER
        ) {
          // Synchronet UX tweak: allow explicit resume while paused.
          GamePaused = false;
          SidebarClearLine(5);
          CurrentTick = randomInt(100);
          CurrentStatTicked = Board.StatCount + 1;
          World.Info.IsSave = true;
        }
      } else {
        if (GameStateElement === E_PLAYER && Board.StatCount >= 0) {
          // Player timing should always be deterministic and responsive.
          Board.Stats[0].Cycle = 1;

          if (CurrentStatTicked <= Board.StatCount) {
            // Under heavy animation/stat churn, keep polling so movement keys don't starve.
            InputUpdate();
          }
        }

        if (GameStateElement === E_MONITOR) {
          // Title/monitor commands should remain responsive regardless of stat cycle timing.
          InputUpdate();
        }

        if (CurrentStatTicked <= Board.StatCount) {
          stat = Board.Stats[CurrentStatTicked];
          if (stat.Cycle !== 0 && stat.Cycle > 0 && (CurrentTick % stat.Cycle) === (CurrentStatTicked % stat.Cycle)) {
            statElement = Board.Tiles[stat.X][stat.Y].Element;
            var tickProc: ((statId: number) => void) | null = ElementDefs[statElement].TickProc;
            if (tickProc !== null) {
              tickProc(CurrentStatTicked);
              if (GameStateElement === E_PLAYER && CurrentStatTicked === 0) {
                playerTickedThisLoop = true;
              }
            }
          }
          CurrentStatTicked += 1;
        }

        if (CurrentStatTicked > Board.StatCount && !GamePlayExitRequested) {
          var elapsedCheck: { Elapsed: boolean; Counter: number } = hasTimeElapsed(TickTimeCounter, TickTimeDuration);
          TickTimeCounter = elapsedCheck.Counter;
          if (elapsedCheck.Elapsed) {
            CurrentTick += 1;
            if (CurrentTick > 420) {
              CurrentTick = 1;
            }
            CurrentStatTicked = 0;
            InputUpdate();
          } else if (typeof mswait === "function") {
            mswait(1);
            sleptThisLoop = true;
          }
        }
      }

      key = upperCase(InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0) : String.fromCharCode(0));
      if (GameStateElement === E_PLAYER) {
        if (key === KEY_ESCAPE || key === "Q") {
          GamePromptEndPlay();
          key = upperCase(InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0) : String.fromCharCode(0));
        } else if (key === "P" && World.Info.Health > 0) {
          GamePaused = true;
        }

        if (World.Info.Health > 0 && !GamePaused && !GamePlayExitRequested && !playerTickedThisLoop) {
          tryApplyPlayerDirectionalInput();
        } else if (World.Info.Health <= 0) {
          InputDeltaX = 0;
          InputDeltaY = 0;
        }
      }

      if (GameStateElement === E_MONITOR &&
          (key === KEY_ESCAPE || key === "A" || key === "E" || key === "H" || key === "N" ||
            key === "P" || key === "Q" || key === "R" || key === "S" || key === "W" || key === "|")) {
        GamePlayExitRequested = true;
      }

      if (ShowInputDebugOverlay) {
        drawDebugStatusLine();
      }

      loopCounter += 1;
      if (noInputFallback && loopCounter > 8) {
        break;
      }

      if (!sleptThisLoop && typeof mswait === "function") {
        if (GamePaused) {
          mswait(20);
        }
      }
    }

    SoundClearQueue();

    if (GameStateElement === E_PLAYER) {
      if (World.Info.Health <= 0) {
        HighScoresAdd(World.Info.Score);
      }
    } else if (GameStateElement === E_MONITOR) {
      SidebarClearLine(5);
    }

    Board.Tiles[Board.Stats[0].X][Board.Stats[0].Y].Element = E_PLAYER;
    Board.Tiles[Board.Stats[0].X][Board.Stats[0].Y].Color = ElementDefs[E_PLAYER].Color;
    SoundBlockQueueing = false;
  }

  export function GameTitleLoop(): void {
    var boardChanged: boolean = true;
    var startPlay: boolean;
    var key: string;
    var interactiveConsole: boolean =
      typeof console !== "undefined" && typeof console.inkey === "function";

    GameTitleExitRequested = false;
    JustStarted = true;
    ReturnBoardId = 0;

    if (!interactiveConsole) {
      runtime.writeLine("");
      runtime.writeLine("Non-interactive runtime detected; skipping title loop.");
      return;
    }

    while (!GameTitleExitRequested && !runtime.isTerminated()) {
      BoardChange(0);

      while (!runtime.isTerminated()) {
        GameStateElement = E_MONITOR;
        startPlay = false;
        GamePaused = false;

        GamePlayLoop(boardChanged);
        boardChanged = false;

        key = upperCase(InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0) : String.fromCharCode(0));

        if (key === "W") {
          if (GameWorldLoad(".ZZT")) {
            ReturnBoardId = World.Info.CurrentBoard;
            boardChanged = true;
          }
        } else if (key === "P") {
          if (World.Info.IsSave && !DebugEnabled && World.Info.Name.length > 0) {
            startPlay = WorldLoad(World.Info.Name, ".ZZT", false);
            ReturnBoardId = World.Info.CurrentBoard;
          } else {
            startPlay = true;
          }

          if (startPlay) {
            BoardChange(ReturnBoardId);
            BoardEnter();
          }
        } else if (key === "R") {
          if (GameWorldLoad(".SAV")) {
            ReturnBoardId = World.Info.CurrentBoard;
            BoardChange(ReturnBoardId);
            startPlay = true;
          }
        } else if (key === "S") {
          TickSpeed += 1;
          if (TickSpeed > 8) {
            TickSpeed = 1;
          }
        } else if (key === "Q" || key === KEY_ESCAPE) {
          GameTitleExitRequested = SidebarPromptYesNo("Quit ZZT? ", true);
        } else if (key === "A") {
          TextWindowDisplayFile("ABOUT.HLP", "About ZZT...");
        } else if (key === "H") {
          HighScoresLoad();
          HighScoresDisplay(1);
        } else if (key === "E") {
          if (EditorEnabled) {
            EditorLoop();
            ReturnBoardId = World.Info.CurrentBoard;
            boardChanged = true;
          }
        } else if (key === "|") {
          GameDebugPrompt();
        }

        if (startPlay) {
          GameStateElement = E_PLAYER;
          GamePaused = true;
          GamePlayLoop(true);
          boardChanged = true;
        }

        if (boardChanged || GameTitleExitRequested) {
          break;
        }
      }
    }
  }

  export function GamePrintRegisterMessage(): void {
    var entryName: string = "END" + String.fromCharCode(49 + randomInt(4)) + ".MSG";
    var lines: string[] = ResourceDataLoadLines(entryName, false);
    var i: number;
    var line: string;
    var color: number = 0x0f;
    var iy: number = 0;

    if (lines.length <= 0) {
      return;
    }

    for (i = 0; i < lines.length; i += 1) {
      line = lines[i];
      if (line.length <= 0) {
        color -= 1;
        if (color < 0) {
          color = 0;
        }
      } else {
        VideoWriteText(0, iy, color, line);
      }
      iy += 1;
    }

    VideoWriteText(28, 24, 0x1f, "Press any key to exit...");
    InputReadWaitKey();
    VideoWriteText(28, 24, 0x00, "                        ");
  }
}
