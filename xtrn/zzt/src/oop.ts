namespace ZZT {
  var OOP_COLOR_NAMES: string[] = ["", "Blue", "Green", "Cyan", "Red", "Purple", "Yellow", "White"];
  var OOP_NUL: string = String.fromCharCode(0);
  var OOP_CR: string = "\r";

  interface OopFindLabelState {
    StatId: number;
    DataPos: number;
  }

  interface OopDirectionResult {
    Position: number;
    DeltaX: number;
    DeltaY: number;
    Success: boolean;
  }

  interface OopTileParseResult {
    Position: number;
    Tile: Tile;
    Success: boolean;
  }

  interface OopConditionResult {
    Position: number;
    Result: boolean;
  }

  interface OopLineResult {
    Position: number;
    Line: string;
  }

  interface OopBoardFindResult {
    X: number;
    Y: number;
    Found: boolean;
  }

  interface OopStatIterateResult {
    StatId: number;
    Found: boolean;
  }

  function oopUpper(input: string): string {
    return input.toUpperCase();
  }

  function oopRandomInt(maxExclusive: number): number {
    if (maxExclusive <= 0) {
      return 0;
    }
    return Math.floor(Math.random() * maxExclusive);
  }

  function oopRepeatChar(ch: string, count: number): string {
    var out: string = "";
    var i: number;

    for (i = 0; i < count; i += 1) {
      out += ch;
    }

    return out;
  }

  function oopDataForStat(statId: number): string {
    var stat: Stat;
    if (statId < 0 || statId > Board.StatCount) {
      return "";
    }

    stat = Board.Stats[statId];
    if (stat.Data === null) {
      return "";
    }

    return stat.Data;
  }

  function oopEnsureDataLength(data: string, len: number): string {
    if (len <= data.length) {
      return data;
    }
    return data + oopRepeatChar(OOP_NUL, len - data.length);
  }

  function oopReadAt(statId: number, position: number): string {
    var stat: Stat;
    var data: string;

    if (statId < 0 || statId > Board.StatCount) {
      return OOP_NUL;
    }

    stat = Board.Stats[statId];
    if (position < 0 || position >= stat.DataLen) {
      return OOP_NUL;
    }

    data = oopDataForStat(statId);
    if (position >= data.length) {
      return OOP_NUL;
    }

    return data.charAt(position);
  }

  function oopSetSharedData(oldData: string | null, newData: string): void {
    var i: number;

    for (i = 0; i <= Board.StatCount; i += 1) {
      if (Board.Stats[i].Data === oldData) {
        Board.Stats[i].Data = newData;
        Board.Stats[i].DataLen = newData.length;
      }
    }
  }

  function oopSetDataChar(statId: number, dataPos: number, ch: string): void {
    var stat: Stat;
    var oldData: string | null;
    var data: string;
    var nextData: string;

    if (statId < 0 || statId > Board.StatCount) {
      return;
    }

    stat = Board.Stats[statId];
    if (dataPos < 0 || dataPos >= stat.DataLen) {
      return;
    }

    oldData = stat.Data;
    data = oopEnsureDataLength(oopDataForStat(statId), stat.DataLen);
    nextData = data.slice(0, dataPos) + ch + data.slice(dataPos + 1);
    oopSetSharedData(oldData, nextData);
  }

  function oopError(statId: number, message: string): void {
    if (statId >= 0 && statId <= Board.StatCount) {
      DisplayMessage(180, "ERR: " + message);
      SoundQueue(5, "\x50\x0A");
      Board.Stats[statId].DataPos = -1;
    }
  }

  function oopReadChar(statId: number, position: number): number {
    if (position >= 0 && position < Board.Stats[statId].DataLen) {
      OopChar = oopReadAt(statId, position);
      position += 1;
    } else {
      OopChar = OOP_NUL;
    }

    return position;
  }

  function oopReadWord(statId: number, position: number): number {
    OopWord = "";

    do {
      position = oopReadChar(statId, position);
    } while (OopChar === " ");

    OopChar = oopUpper(OopChar);
    if (!(OopChar >= "0" && OopChar <= "9")) {
      while (
        (OopChar >= "A" && OopChar <= "Z") ||
        OopChar === ":" ||
        OopChar === "_" ||
        (OopChar >= "0" && OopChar <= "9")
      ) {
        OopWord += OopChar;
        position = oopReadChar(statId, position);
        OopChar = oopUpper(OopChar);
      }
    }

    if (position > 0) {
      position -= 1;
    }

    return position;
  }

  function oopReadValue(statId: number, position: number): number {
    var textValue: string = "";
    var parsed: number;

    do {
      position = oopReadChar(statId, position);
    } while (OopChar === " ");

    OopChar = oopUpper(OopChar);
    while (OopChar >= "0" && OopChar <= "9") {
      textValue += OopChar;
      position = oopReadChar(statId, position);
      OopChar = oopUpper(OopChar);
    }

    if (position > 0) {
      position -= 1;
    }

    if (textValue.length > 0) {
      parsed = parseInt(textValue, 10);
      if (isNaN(parsed)) {
        OopValue = -1;
      } else {
        OopValue = parsed;
      }
    } else {
      OopValue = -1;
    }

    return position;
  }

  function oopSkipLine(statId: number, position: number): number {
    do {
      position = oopReadChar(statId, position);
    } while (OopChar !== OOP_NUL && OopChar !== OOP_CR);
    return position;
  }

  function oopParseDirection(statId: number, position: number): OopDirectionResult {
    var result: OopDirectionResult = {
      Position: position,
      DeltaX: 0,
      DeltaY: 0,
      Success: true
    };
    var parsed: OopDirectionResult;
    var basicDir: Direction;

    if (OopWord === "N" || OopWord === "NORTH") {
      result.DeltaX = 0;
      result.DeltaY = -1;
    } else if (OopWord === "S" || OopWord === "SOUTH") {
      result.DeltaX = 0;
      result.DeltaY = 1;
    } else if (OopWord === "E" || OopWord === "EAST") {
      result.DeltaX = 1;
      result.DeltaY = 0;
    } else if (OopWord === "W" || OopWord === "WEST") {
      result.DeltaX = -1;
      result.DeltaY = 0;
    } else if (OopWord === "I" || OopWord === "IDLE") {
      result.DeltaX = 0;
      result.DeltaY = 0;
    } else if (OopWord === "SEEK") {
      basicDir = CalcDirectionSeek(Board.Stats[statId].X, Board.Stats[statId].Y);
      result.DeltaX = basicDir.DeltaX;
      result.DeltaY = basicDir.DeltaY;
    } else if (OopWord === "FLOW") {
      result.DeltaX = Board.Stats[statId].StepX;
      result.DeltaY = Board.Stats[statId].StepY;
    } else if (OopWord === "RND") {
      basicDir = CalcDirectionRnd();
      result.DeltaX = basicDir.DeltaX;
      result.DeltaY = basicDir.DeltaY;
    } else if (OopWord === "RNDNS") {
      result.DeltaX = 0;
      result.DeltaY = oopRandomInt(2) * 2 - 1;
    } else if (OopWord === "RNDNE") {
      result.DeltaX = oopRandomInt(2);
      if (result.DeltaX === 0) {
        result.DeltaY = -1;
      } else {
        result.DeltaY = 0;
      }
    } else if (OopWord === "CW") {
      result.Position = oopReadWord(statId, result.Position);
      parsed = oopParseDirection(statId, result.Position);
      result.Position = parsed.Position;
      result.DeltaX = -parsed.DeltaY;
      result.DeltaY = parsed.DeltaX;
      result.Success = parsed.Success;
    } else if (OopWord === "CCW") {
      result.Position = oopReadWord(statId, result.Position);
      parsed = oopParseDirection(statId, result.Position);
      result.Position = parsed.Position;
      result.DeltaX = parsed.DeltaY;
      result.DeltaY = -parsed.DeltaX;
      result.Success = parsed.Success;
    } else if (OopWord === "RNDP") {
      result.Position = oopReadWord(statId, result.Position);
      parsed = oopParseDirection(statId, result.Position);
      result.Position = parsed.Position;
      result.Success = parsed.Success;
      result.DeltaX = parsed.DeltaY;
      result.DeltaY = parsed.DeltaX;
      if (oopRandomInt(2) === 0) {
        result.DeltaX = -result.DeltaX;
      } else {
        result.DeltaY = -result.DeltaY;
      }
    } else if (OopWord === "OPP") {
      result.Position = oopReadWord(statId, result.Position);
      parsed = oopParseDirection(statId, result.Position);
      result.Position = parsed.Position;
      result.DeltaX = -parsed.DeltaX;
      result.DeltaY = -parsed.DeltaY;
      result.Success = parsed.Success;
    } else {
      result.Success = false;
      result.DeltaX = 0;
      result.DeltaY = 0;
    }

    return result;
  }

  function oopReadDirection(statId: number, position: number): OopDirectionResult {
    var result: OopDirectionResult;

    position = oopReadWord(statId, position);
    result = oopParseDirection(statId, position);
    if (!result.Success) {
      oopError(statId, "Bad direction");
    }
    return result;
  }

  function oopFindString(statId: number, lookup: string): number {
    var pos: number;
    var i: number;
    var nextCh: string;
    var isMatch: boolean;

    for (pos = 0; pos <= Board.Stats[statId].DataLen; pos += 1) {
      isMatch = true;
      for (i = 0; i < lookup.length; i += 1) {
        if (oopUpper(lookup.charAt(i)) !== oopUpper(oopReadAt(statId, pos + i))) {
          isMatch = false;
          break;
        }
      }

      if (!isMatch) {
        continue;
      }

      nextCh = oopUpper(oopReadAt(statId, pos + lookup.length));
      if (
        (nextCh >= "A" && nextCh <= "Z") ||
        (nextCh >= "0" && nextCh <= "9") ||
        nextCh === "_"
      ) {
        continue;
      }

      return pos;
    }

    return -1;
  }

  function oopIterateStat(statId: number, iStat: number, lookup: string): OopStatIterateResult {
    var found: boolean = false;
    var pos: number;

    iStat += 1;

    if (lookup === "ALL") {
      found = iStat <= Board.StatCount;
    } else if (lookup === "OTHERS") {
      if (iStat <= Board.StatCount) {
        if (iStat !== statId) {
          found = true;
        } else {
          iStat += 1;
          found = iStat <= Board.StatCount;
        }
      }
    } else if (lookup === "SELF") {
      if (statId > 0 && iStat <= statId) {
        iStat = statId;
        found = true;
      }
    } else {
      while (iStat <= Board.StatCount && !found) {
        if (Board.Stats[iStat].Data !== null) {
          pos = 0;
          pos = oopReadChar(iStat, pos);
          if (OopChar === "@") {
            pos = oopReadWord(iStat, pos);
            if (OopWord === lookup) {
              found = true;
            }
          }
        }

        if (!found) {
          iStat += 1;
        }
      }
    }

    return {
      StatId: iStat,
      Found: found
    };
  }

  function oopFindLabel(statId: number, sendLabel: string, state: OopFindLabelState, labelPrefix: string): boolean {
    var targetSplitPos: number = sendLabel.indexOf(":");
    var targetLookup: string;
    var objectMessage: string;
    var iter: OopStatIterateResult;

    if (targetSplitPos < 0) {
      if (state.StatId >= statId) {
        return false;
      }

      state.StatId = statId;
      objectMessage = sendLabel;
      if (oopUpper(objectMessage) === "RESTART") {
        state.DataPos = 0;
      } else {
        state.DataPos = oopFindString(state.StatId, labelPrefix + objectMessage);
      }
      return state.DataPos >= 0;
    }

    targetLookup = oopUpper(sendLabel.slice(0, targetSplitPos));
    objectMessage = sendLabel.slice(targetSplitPos + 1);

    while (true) {
      iter = oopIterateStat(statId, state.StatId, targetLookup);
      state.StatId = iter.StatId;
      if (!iter.Found) {
        return false;
      }

      if (oopUpper(objectMessage) === "RESTART") {
        state.DataPos = 0;
        return true;
      }

      state.DataPos = oopFindString(state.StatId, labelPrefix + objectMessage);
      if (state.DataPos >= 0) {
        return true;
      }
    }
  }

  export function WorldGetFlagPosition(name: string): number {
    var i: number;
    var lookup: string = oopUpper(name);

    for (i = 1; i <= MAX_FLAG; i += 1) {
      if (oopUpper(World.Info.Flags[i]) === lookup) {
        return i;
      }
    }

    return -1;
  }

  export function WorldSetFlag(name: string): void {
    var i: number;

    if (WorldGetFlagPosition(name) >= 0) {
      return;
    }

    i = 1;
    while (i < MAX_FLAG && World.Info.Flags[i].length > 0) {
      i += 1;
    }

    if (i <= MAX_FLAG) {
      World.Info.Flags[i] = name;
    }
  }

  export function WorldClearFlag(name: string): void {
    var pos: number = WorldGetFlagPosition(name);
    if (pos >= 0) {
      World.Info.Flags[pos] = "";
    }
  }

  function oopStringToWord(input: string): string {
    var output: string = "";
    var i: number;
    var ch: string;

    for (i = 0; i < input.length; i += 1) {
      ch = input.charAt(i);
      if ((ch >= "A" && ch <= "Z") || (ch >= "0" && ch <= "9")) {
        output += ch;
      } else if (ch >= "a" && ch <= "z") {
        output += oopUpper(ch);
      }
    }

    return output;
  }

  function oopParseTile(statId: number, position: number): OopTileParseResult {
    var tile: Tile = createTile(E_EMPTY, 0);
    var i: number;

    position = oopReadWord(statId, position);

    for (i = 1; i <= 7; i += 1) {
      if (OopWord === oopStringToWord(OOP_COLOR_NAMES[i])) {
        tile.Color = i + 8;
        position = oopReadWord(statId, position);
        break;
      }
    }

    for (i = 0; i <= MAX_ELEMENT; i += 1) {
      if (OopWord === oopStringToWord(ElementDefs[i].Name)) {
        tile.Element = i;
        return {
          Position: position,
          Tile: tile,
          Success: true
        };
      }
    }

    return {
      Position: position,
      Tile: tile,
      Success: false
    };
  }

  function oopGetColorForTileMatch(tile: Tile): number {
    if (ElementDefs[tile.Element].Color < COLOR_SPECIAL_MIN) {
      return ElementDefs[tile.Element].Color & 0x07;
    }

    if (ElementDefs[tile.Element].Color === COLOR_WHITE_ON_CHOICE) {
      return ((tile.Color >> 4) & 0x0f) + 8;
    }

    return tile.Color & 0x0f;
  }

  function oopFindTileOnBoard(startX: number, startY: number, tile: Tile): OopBoardFindResult {
    var x: number = startX;
    var y: number = startY;

    while (true) {
      x += 1;
      if (x > BOARD_WIDTH) {
        x = 1;
        y += 1;
        if (y > BOARD_HEIGHT) {
          return {
            X: x,
            Y: y,
            Found: false
          };
        }
      }

      if (Board.Tiles[x][y].Element !== tile.Element) {
        continue;
      }

      if (tile.Color === 0 || oopGetColorForTileMatch(Board.Tiles[x][y]) === tile.Color) {
        return {
          X: x,
          Y: y,
          Found: true
        };
      }
    }
  }

  function oopPlaceTile(x: number, y: number, tile: Tile): void {
    var color: number = tile.Color;
    var tileDef: ElementDef = ElementDefs[tile.Element];

    if (Board.Tiles[x][y].Element === E_PLAYER) {
      return;
    }

    if (tileDef.Color < COLOR_SPECIAL_MIN) {
      color = tileDef.Color;
    } else {
      if (color === 0) {
        color = Board.Tiles[x][y].Color;
      }
      if (color === 0) {
        color = 0x0f;
      }
      if (tileDef.Color === COLOR_WHITE_ON_CHOICE) {
        color = ((color - 8) * 0x10) + 0x0f;
      }
    }

    if (Board.Tiles[x][y].Element === tile.Element) {
      Board.Tiles[x][y].Color = color;
    } else {
      BoardDamageTile(x, y);
      if (ElementDefs[tile.Element].Cycle >= 0) {
        AddStat(x, y, tile.Element, color, ElementDefs[tile.Element].Cycle, createDefaultStat());
      } else {
        Board.Tiles[x][y].Element = tile.Element;
        Board.Tiles[x][y].Color = color;
      }
    }

    BoardDrawTile(x, y);
  }

  function oopCheckCondition(statId: number, position: number): OopConditionResult {
    var stat: Stat = Board.Stats[statId];
    var dir: OopDirectionResult;
    var parseTile: OopTileParseResult;
    var findTile: OopBoardFindResult;
    var result: boolean;

    if (OopWord === "NOT") {
      position = oopReadWord(statId, position);
      var negated: OopConditionResult = oopCheckCondition(statId, position);
      return {
        Position: negated.Position,
        Result: !negated.Result
      };
    }

    if (OopWord === "ALLIGNED") {
      result = stat.X === Board.Stats[0].X || stat.Y === Board.Stats[0].Y;
      return {
        Position: position,
        Result: result
      };
    }

    if (OopWord === "CONTACT") {
      result = ((stat.X - Board.Stats[0].X) * (stat.X - Board.Stats[0].X) +
        (stat.Y - Board.Stats[0].Y) * (stat.Y - Board.Stats[0].Y)) === 1;
      return {
        Position: position,
        Result: result
      };
    }

    if (OopWord === "BLOCKED") {
      dir = oopReadDirection(statId, position);
      position = dir.Position;
      result = !ElementDefs[Board.Tiles[stat.X + dir.DeltaX][stat.Y + dir.DeltaY].Element].Walkable;
      return {
        Position: position,
        Result: result
      };
    }

    if (OopWord === "ENERGIZED") {
      return {
        Position: position,
        Result: World.Info.EnergizerTicks > 0
      };
    }

    if (OopWord === "ANY") {
      parseTile = oopParseTile(statId, position);
      position = parseTile.Position;
      if (!parseTile.Success) {
        oopError(statId, "Bad object kind");
        return {
          Position: position,
          Result: false
        };
      }
      findTile = oopFindTileOnBoard(0, 1, parseTile.Tile);
      return {
        Position: position,
        Result: findTile.Found
      };
    }

    return {
      Position: position,
      Result: WorldGetFlagPosition(OopWord) >= 0
    };
  }

  function oopReadLineToEnd(statId: number, position: number): OopLineResult {
    var line: string = "";

    position = oopReadChar(statId, position);
    while (OopChar !== OOP_NUL && OopChar !== OOP_CR) {
      line += OopChar;
      position = oopReadChar(statId, position);
    }

    return {
      Position: position,
      Line: line
    };
  }

  export function OopSend(statId: number, sendLabel: string, ignoreLock: boolean): boolean {
    var iDataPos: number;
    var iStat: number;
    var state: OopFindLabelState;
    var respectSelfLock: boolean;
    var sentToSelf: boolean = false;

    if (statId < 0) {
      statId = -statId;
      respectSelfLock = true;
    } else {
      respectSelfLock = false;
    }

    state = {
      StatId: 0,
      DataPos: -1
    };

    while (oopFindLabel(statId, sendLabel, state, OOP_CR + ":")) {
      iStat = state.StatId;
      iDataPos = state.DataPos;

      if (iStat < 0 || iStat > Board.StatCount) {
        continue;
      }

      if (
        Board.Stats[iStat].P2 === 0 ||
        ignoreLock ||
        ((statId === iStat) && !respectSelfLock)
      ) {
        if (iStat === statId) {
          sentToSelf = true;
        }
        Board.Stats[iStat].DataPos = iDataPos;
      }
    }

    return sentToSelf;
  }

  function oopAppendDisplayLine(state: TextWindowState, line: string): void {
    TextWindowAppend(state, line);
  }

  function oopReadNameLine(statId: number): string {
    var data: string = oopDataForStat(statId);
    var endLine: number;

    if (data.length <= 0 || data.charAt(0) !== "@") {
      return "";
    }

    endLine = data.indexOf(OOP_CR);
    if (endLine < 0) {
      endLine = data.length;
    }

    return data.slice(1, endLine);
  }

  function oopFindCounterValue(name: string): number | null {
    if (name === "HEALTH") {
      return World.Info.Health;
    }
    if (name === "AMMO") {
      return World.Info.Ammo;
    }
    if (name === "GEMS") {
      return World.Info.Gems;
    }
    if (name === "TORCHES") {
      return World.Info.Torches;
    }
    if (name === "SCORE") {
      return World.Info.Score;
    }
    if (name === "TIME") {
      return World.Info.BoardTimeSec;
    }
    return null;
  }

  function oopSetCounterValue(name: string, value: number): void {
    if (name === "HEALTH") {
      World.Info.Health = value;
    } else if (name === "AMMO") {
      World.Info.Ammo = value;
    } else if (name === "GEMS") {
      World.Info.Gems = value;
    } else if (name === "TORCHES") {
      World.Info.Torches = value;
    } else if (name === "SCORE") {
      World.Info.Score = value;
    } else if (name === "TIME") {
      World.Info.BoardTimeSec = value;
    }
  }

  export function OopExecute(statId: number, position: number, name: string): number {
    var restartParsing: boolean = true;

    while (restartParsing) {
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
      var stopRunning: boolean = false;
      var repeatInsNextTick: boolean = false;
      var replaceStat: boolean = false;
      var endOfProgram: boolean = false;
      var replaceTile: Tile = createTile(E_EMPTY, 0x0f);
      var lastPosition: number = position;
      var lineFinished: boolean;
      var textLine: string;
      var lineData: OopLineResult;
      var lineFirstChar: string;
      var readDir: OopDirectionResult;
      var conditionResult: OopConditionResult;
      var parseTile: OopTileParseResult;
      var parseTile2: OopTileParseResult;
      var stat: Stat;
      var tx: number;
      var ty: number;
      var ix: number;
      var iy: number;
      var scan: OopBoardFindResult;
      var commandLoop: boolean;
      var insCount: number = 0;
      var currentValue: number | null;
      var targetCounter: string = "";
      var deltaValue: number;
      var labelState: OopFindLabelState;
      var objectName: string;
      var bindStat: OopStatIterateResult;

      restartParsing = false;
      TextWindowInitState(textWindow);

      while (!endOfProgram && !stopRunning && !repeatInsNextTick && !replaceStat && insCount <= 32) {
        lineFinished = true;
        lastPosition = position;
        position = oopReadChar(statId, position);

        while (OopChar === ":") {
          position = oopSkipLine(statId, position);
          position = oopReadChar(statId, position);
        }

        if (OopChar === "'" || OopChar === "@") {
          position = oopSkipLine(statId, position);
          continue;
        }

        if (OopChar === "/" || OopChar === "?") {
          if (OopChar === "/") {
            repeatInsNextTick = true;
          }

          position = oopReadWord(statId, position);
          readDir = oopParseDirection(statId, position);
          position = readDir.Position;

          if (readDir.Success) {
            stat = Board.Stats[statId];
            if (readDir.DeltaX !== 0 || readDir.DeltaY !== 0) {
              if (!ElementDefs[Board.Tiles[stat.X + readDir.DeltaX][stat.Y + readDir.DeltaY].Element].Walkable) {
                ElementPushablePush(stat.X + readDir.DeltaX, stat.Y + readDir.DeltaY, readDir.DeltaX, readDir.DeltaY);
              }

              if (ElementDefs[Board.Tiles[stat.X + readDir.DeltaX][stat.Y + readDir.DeltaY].Element].Walkable) {
                MoveStat(statId, stat.X + readDir.DeltaX, stat.Y + readDir.DeltaY);
                repeatInsNextTick = false;
              }
            } else {
              repeatInsNextTick = false;
            }

            position = oopReadChar(statId, position);
            if (OopChar !== OOP_CR) {
              position -= 1;
            }

            stopRunning = true;
          } else {
            oopError(statId, "Bad direction");
          }
          continue;
        }

        if (OopChar === "#") {
          commandLoop = true;
          while (commandLoop) {
            commandLoop = false;
            position = oopReadWord(statId, position);

            if (OopWord === "THEN") {
              position = oopReadWord(statId, position);
            }

            if (OopWord.length === 0) {
              break;
            }

            insCount += 1;

            if (OopWord === "GO") {
              readDir = oopReadDirection(statId, position);
              position = readDir.Position;
              stat = Board.Stats[statId];

              if (!ElementDefs[Board.Tiles[stat.X + readDir.DeltaX][stat.Y + readDir.DeltaY].Element].Walkable) {
                ElementPushablePush(stat.X + readDir.DeltaX, stat.Y + readDir.DeltaY, readDir.DeltaX, readDir.DeltaY);
              }

              if (ElementDefs[Board.Tiles[stat.X + readDir.DeltaX][stat.Y + readDir.DeltaY].Element].Walkable) {
                MoveStat(statId, stat.X + readDir.DeltaX, stat.Y + readDir.DeltaY);
              } else {
                repeatInsNextTick = true;
              }

              stopRunning = true;
            } else if (OopWord === "TRY") {
              readDir = oopReadDirection(statId, position);
              position = readDir.Position;
              stat = Board.Stats[statId];

              if (!ElementDefs[Board.Tiles[stat.X + readDir.DeltaX][stat.Y + readDir.DeltaY].Element].Walkable) {
                ElementPushablePush(stat.X + readDir.DeltaX, stat.Y + readDir.DeltaY, readDir.DeltaX, readDir.DeltaY);
              }

              if (ElementDefs[Board.Tiles[stat.X + readDir.DeltaX][stat.Y + readDir.DeltaY].Element].Walkable) {
                MoveStat(statId, stat.X + readDir.DeltaX, stat.Y + readDir.DeltaY);
                stopRunning = true;
              } else {
                commandLoop = true;
              }
            } else if (OopWord === "WALK") {
              readDir = oopReadDirection(statId, position);
              position = readDir.Position;
              Board.Stats[statId].StepX = readDir.DeltaX;
              Board.Stats[statId].StepY = readDir.DeltaY;
            } else if (OopWord === "SET") {
              position = oopReadWord(statId, position);
              WorldSetFlag(OopWord);
            } else if (OopWord === "CLEAR") {
              position = oopReadWord(statId, position);
              WorldClearFlag(OopWord);
            } else if (OopWord === "IF") {
              position = oopReadWord(statId, position);
              conditionResult = oopCheckCondition(statId, position);
              position = conditionResult.Position;
              if (conditionResult.Result) {
                commandLoop = true;
              }
            } else if (OopWord === "SHOOT") {
              readDir = oopReadDirection(statId, position);
              position = readDir.Position;
              stat = Board.Stats[statId];
              if (BoardShoot(E_BULLET, stat.X, stat.Y, readDir.DeltaX, readDir.DeltaY, SHOT_SOURCE_ENEMY)) {
                SoundQueue(2, "\x30\x01\x26\x01");
              }
              stopRunning = true;
            } else if (OopWord === "THROWSTAR") {
              readDir = oopReadDirection(statId, position);
              position = readDir.Position;
              stat = Board.Stats[statId];
              BoardShoot(E_STAR, stat.X, stat.Y, readDir.DeltaX, readDir.DeltaY, SHOT_SOURCE_ENEMY);
              stopRunning = true;
            } else if (OopWord === "GIVE" || OopWord === "TAKE") {
              deltaValue = 1;
              if (OopWord === "TAKE") {
                deltaValue = -1;
              }

              position = oopReadWord(statId, position);
              targetCounter = OopWord;
              currentValue = oopFindCounterValue(targetCounter);

              if (currentValue !== null) {
                position = oopReadValue(statId, position);
                if (OopValue > 0) {
                  if ((currentValue + (OopValue * deltaValue)) >= 0) {
                    oopSetCounterValue(targetCounter, currentValue + (OopValue * deltaValue));
                  } else {
                    commandLoop = true;
                  }
                }
              }

              GameUpdateSidebar();
            } else if (OopWord === "END") {
              position = -1;
              OopChar = OOP_NUL;
            } else if (OopWord === "ENDGAME") {
              World.Info.Health = 0;
            } else if (OopWord === "IDLE") {
              stopRunning = true;
            } else if (OopWord === "RESTART") {
              position = 0;
              lineFinished = false;
            } else if (OopWord === "ZAP") {
              position = oopReadWord(statId, position);
              labelState = {
                StatId: 0,
                DataPos: -1
              };
              while (oopFindLabel(statId, OopWord, labelState, OOP_CR + ":")) {
                oopSetDataChar(labelState.StatId, labelState.DataPos + 1, "'");
              }
            } else if (OopWord === "RESTORE") {
              position = oopReadWord(statId, position);
              labelState = {
                StatId: 0,
                DataPos: -1
              };
              while (oopFindLabel(statId, OopWord, labelState, OOP_CR + "'")) {
                oopSetDataChar(labelState.StatId, labelState.DataPos + 1, ":");
              }
            } else if (OopWord === "LOCK") {
              Board.Stats[statId].P2 = 1;
            } else if (OopWord === "UNLOCK") {
              Board.Stats[statId].P2 = 0;
            } else if (OopWord === "SEND") {
              position = oopReadWord(statId, position);
              if (OopSend(statId, OopWord, false)) {
                lineFinished = false;
                if (statId >= 0 && statId <= Board.StatCount) {
                  position = Board.Stats[statId].DataPos;
                }
              }
            } else if (OopWord === "BECOME") {
              parseTile = oopParseTile(statId, position);
              position = parseTile.Position;
              if (parseTile.Success) {
                replaceStat = true;
                replaceTile = parseTile.Tile;
              } else {
                oopError(statId, "Bad #BECOME");
              }
            } else if (OopWord === "PUT") {
              readDir = oopReadDirection(statId, position);
              position = readDir.Position;
              parseTile = oopParseTile(statId, position);
              position = parseTile.Position;
              stat = Board.Stats[statId];

              if ((readDir.DeltaX === 0 && readDir.DeltaY === 0) || !parseTile.Success) {
                oopError(statId, "Bad #PUT");
              } else if (
                (stat.X + readDir.DeltaX) > 0 &&
                (stat.X + readDir.DeltaX) <= BOARD_WIDTH &&
                (stat.Y + readDir.DeltaY) > 0 &&
                (stat.Y + readDir.DeltaY) <= BOARD_HEIGHT
              ) {
                tx = stat.X + readDir.DeltaX;
                ty = stat.Y + readDir.DeltaY;

                if (!ElementDefs[Board.Tiles[tx][ty].Element].Walkable) {
                  ElementPushablePush(tx, ty, readDir.DeltaX, readDir.DeltaY);
                }
                oopPlaceTile(tx, ty, parseTile.Tile);
              }
            } else if (OopWord === "CHANGE") {
              parseTile = oopParseTile(statId, position);
              position = parseTile.Position;
              parseTile2 = oopParseTile(statId, position);
              position = parseTile2.Position;

              if (!parseTile.Success || !parseTile2.Success) {
                oopError(statId, "Bad #CHANGE");
              } else {
                if (parseTile2.Tile.Color === 0 && ElementDefs[parseTile2.Tile.Element].Color < COLOR_SPECIAL_MIN) {
                  parseTile2.Tile.Color = ElementDefs[parseTile2.Tile.Element].Color;
                }

                ix = 0;
                iy = 1;
                while (true) {
                  scan = oopFindTileOnBoard(ix, iy, parseTile.Tile);
                  if (!scan.Found) {
                    break;
                  }
                  oopPlaceTile(scan.X, scan.Y, parseTile2.Tile);
                  ix = scan.X;
                  iy = scan.Y;
                }
              }
            } else if (OopWord === "PLAY") {
              lineData = oopReadLineToEnd(statId, position);
              position = lineData.Position;
              textLine = SoundParse(lineData.Line);
              if (textLine.length > 0) {
                SoundQueue(-1, textLine);
              }
              lineFinished = false;
            } else if (OopWord === "CYCLE") {
              position = oopReadValue(statId, position);
              if (OopValue > 0) {
                Board.Stats[statId].Cycle = OopValue;
              }
            } else if (OopWord === "CHAR") {
              position = oopReadValue(statId, position);
              if (OopValue > 0 && OopValue <= 255) {
                Board.Stats[statId].P1 = OopValue;
                BoardDrawTile(Board.Stats[statId].X, Board.Stats[statId].Y);
              }
            } else if (OopWord === "DIE") {
              replaceStat = true;
              replaceTile = createTile(E_EMPTY, 0x0f);
            } else if (OopWord === "BIND") {
              position = oopReadWord(statId, position);
              bindStat = oopIterateStat(statId, 0, OopWord);
              if (bindStat.Found) {
                Board.Stats[statId].Data = Board.Stats[bindStat.StatId].Data;
                Board.Stats[statId].DataLen = Board.Stats[bindStat.StatId].DataLen;
                position = 0;
              }
            } else {
              textLine = OopWord;
              if (OopSend(statId, textLine, false)) {
                lineFinished = false;
                if (statId >= 0 && statId <= Board.StatCount) {
                  position = Board.Stats[statId].DataPos;
                }
              } else if (textLine.indexOf(":") < 0) {
                oopError(statId, "Bad command " + textLine);
              }
            }
          }

          if (lineFinished) {
            position = oopSkipLine(statId, position);
          }

          continue;
        }

        if (OopChar === OOP_CR) {
          if (textWindow.LineCount > 0) {
            TextWindowAppend(textWindow, "");
          }
        } else if (OopChar === OOP_NUL) {
          endOfProgram = true;
        } else {
          lineFirstChar = OopChar;
          lineData = oopReadLineToEnd(statId, position);
          position = lineData.Position;
          oopAppendDisplayLine(textWindow, lineFirstChar + lineData.Line);
        }
      }

      if (repeatInsNextTick) {
        position = lastPosition;
      }

      if (OopChar === OOP_NUL) {
        position = -1;
      }

      if (textWindow.LineCount > 1) {
        objectName = oopReadNameLine(statId);
        if (objectName.length <= 0) {
          objectName = name;
        }
        if (objectName.length <= 0) {
          objectName = "Interaction";
        }

        textWindow.Title = objectName;
        TextWindowDrawOpen(textWindow);
        TextWindowSelect(textWindow, true, false);
        TextWindowDrawClose(textWindow);

        if (textWindow.Hyperlink.length > 0) {
          if (OopSend(statId, textWindow.Hyperlink, false)) {
            if (statId >= 0 && statId <= Board.StatCount) {
              position = Board.Stats[statId].DataPos;
            }
            restartParsing = true;
          }
        }
        TextWindowFree(textWindow);
      } else if (textWindow.LineCount === 1) {
        DisplayMessage(200, textWindow.Lines[0]);
        TextWindowFree(textWindow);
      }

      if (replaceStat && statId >= 0 && statId <= Board.StatCount) {
        ix = Board.Stats[statId].X;
        iy = Board.Stats[statId].Y;
        DamageStat(statId);
        oopPlaceTile(ix, iy, replaceTile);
      }
    }

    return position;
  }
}
