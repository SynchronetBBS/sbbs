namespace ZZT {
  export var MAX_STAT: number = 150;
  export var MAX_ELEMENT: number = 53;
  export var MAX_BOARD: number = 100;
  export var MAX_FLAG: number = 10;
  export var BOARD_WIDTH: number = 60;
  export var BOARD_HEIGHT: number = 25;
  export var WORLD_FILE_HEADER_SIZE: number = 512;
  export var HIGH_SCORE_COUNT: number = 30;
  export var TORCH_DURATION: number = 200;
  export var TORCH_DX: number = 8;
  export var TORCH_DY: number = 5;
  export var TORCH_DIST_SQR: number = 50;

  export var E_EMPTY: number = 0;
  export var E_BOARD_EDGE: number = 1;
  export var E_MESSAGE_TIMER: number = 2;
  export var E_MONITOR: number = 3;
  export var E_PLAYER: number = 4;
  export var E_AMMO: number = 5;
  export var E_TORCH: number = 6;
  export var E_GEM: number = 7;
  export var E_KEY: number = 8;
  export var E_DOOR: number = 9;
  export var E_SCROLL: number = 10;
  export var E_PASSAGE: number = 11;
  export var E_DUPLICATOR: number = 12;
  export var E_BOMB: number = 13;
  export var E_ENERGIZER: number = 14;
  export var E_STAR: number = 15;
  export var E_CONVEYOR_CW: number = 16;
  export var E_CONVEYOR_CCW: number = 17;
  export var E_BULLET: number = 18;
  export var E_WATER: number = 19;
  export var E_FOREST: number = 20;
  export var E_SOLID: number = 21;
  export var E_NORMAL: number = 22;
  export var E_BREAKABLE: number = 23;
  export var E_BOULDER: number = 24;
  export var E_SLIDER_NS: number = 25;
  export var E_SLIDER_EW: number = 26;
  export var E_FAKE: number = 27;
  export var E_INVISIBLE: number = 28;
  export var E_BLINK_WALL: number = 29;
  export var E_TRANSPORTER: number = 30;
  export var E_LINE: number = 31;
  export var E_RICOCHET: number = 32;
  export var E_BLINK_RAY_EW: number = 33;
  export var E_BEAR: number = 34;
  export var E_RUFFIAN: number = 35;
  export var E_OBJECT: number = 36;
  export var E_SLIME: number = 37;
  export var E_SHARK: number = 38;
  export var E_SPINNING_GUN: number = 39;
  export var E_PUSHER: number = 40;
  export var E_LION: number = 41;
  export var E_TIGER: number = 42;
  export var E_BLINK_RAY_NS: number = 43;
  export var E_CENTIPEDE_HEAD: number = 44;
  export var E_CENTIPEDE_SEGMENT: number = 45;
  export var E_TEXT_BLUE: number = 47;
  export var E_TEXT_GREEN: number = 48;
  export var E_TEXT_CYAN: number = 49;
  export var E_TEXT_RED: number = 50;
  export var E_TEXT_PURPLE: number = 51;
  export var E_TEXT_YELLOW: number = 52;
  export var E_TEXT_WHITE: number = 53;
  export var E_TEXT_MIN: number = E_TEXT_BLUE;

  export var CATEGORY_ITEM: number = 1;
  export var CATEGORY_CREATURE: number = 2;
  export var CATEGORY_TERRAIN: number = 3;

  export var COLOR_SPECIAL_MIN: number = 0xf0;
  export var COLOR_CHOICE_ON_BLACK: number = 0xff;
  export var COLOR_WHITE_ON_CHOICE: number = 0xfe;
  export var COLOR_CHOICE_ON_CHOICE: number = 0xfd;

  export var SHOT_SOURCE_PLAYER: number = 0;
  export var SHOT_SOURCE_ENEMY: number = 1;

  export interface Coord {
    X: number;
    Y: number;
  }

  export interface Tile {
    Element: number;
    Color: number;
  }

  export interface TouchContext {
    DeltaX: number;
    DeltaY: number;
  }

  export interface Stat {
    X: number;
    Y: number;
    StepX: number;
    StepY: number;
    Cycle: number;
    P1: number;
    P2: number;
    P3: number;
    Follower: number;
    Leader: number;
    Under: Tile;
    Data: string | null;
    DataPos: number;
    DataLen: number;
  }

  export interface BoardInfo {
    MaxShots: number;
    IsDark: boolean;
    NeighborBoards: number[];
    ReenterWhenZapped: boolean;
    Message: string;
    StartPlayerX: number;
    StartPlayerY: number;
    TimeLimitSec: number;
    UnknownPadding: number[];
  }

  export interface EditorStatSetting {
    P1: number;
    P2: number;
    P3: number;
    StepX: number;
    StepY: number;
  }

  export interface WorldInfo {
    Ammo: number;
    Gems: number;
    Keys: boolean[];
    Health: number;
    CurrentBoard: number;
    Torches: number;
    TorchTicks: number;
    EnergizerTicks: number;
    Unknown1: number;
    Score: number;
    Name: string;
    Flags: string[];
    BoardTimeSec: number;
    BoardTimeHsec: number;
    IsSave: boolean;
    UnknownPadding: number[];
  }

  export interface Board {
    Name: string;
    Tiles: Tile[][];
    StatCount: number;
    Stats: Stat[];
    Info: BoardInfo;
  }

  export interface World {
    BoardCount: number;
    BoardData: Array<number[] | null>;
    BoardLen: number[];
    Info: WorldInfo;
    EditorStatSettings: EditorStatSetting[];
  }

  export interface HighScoreEntry {
    Name: string;
    Score: number;
  }

  export interface ElementDef {
    DrawProc: ((x: number, y: number) => number) | null;
    TickProc: ((statId: number) => void) | null;
    TouchProc: ((x: number, y: number, sourceStatId: number, context: TouchContext) => void) | null;
    Character: string;
    Color: number;
    Destructible: boolean;
    Pushable: boolean;
    VisibleInDark: boolean;
    PlaceableOnTop: boolean;
    Walkable: boolean;
    HasDrawProc: boolean;
    Cycle: number;
    EditorCategory: number;
    EditorShortcut: string;
    Name: string;
    CategoryName: string;
    Param1Name: string;
    Param2Name: string;
    ParamBulletTypeName: string;
    ParamBoardName: string;
    ParamDirName: string;
    ParamTextName: string;
    ScoreValue: number;
  }

  function createKeysArray(): boolean[] {
    var keys: boolean[] = [];
    var i: number;
    for (i = 0; i <= 7; i += 1) {
      keys.push(false);
    }
    return keys;
  }

  function createFlagsArray(): string[] {
    var flags: string[] = [];
    var i: number;
    for (i = 0; i <= MAX_FLAG; i += 1) {
      flags.push("");
    }
    return flags;
  }

  export function createTile(element: number, color: number): Tile {
    return {
      Element: element,
      Color: color
    };
  }

  export function cloneTile(tile: Tile): Tile {
    return {
      Element: tile.Element,
      Color: tile.Color
    };
  }

  function createTileGrid(): Tile[][] {
    var columns: Tile[][] = [];
    var ix: number;
    var iy: number;

    for (ix = 0; ix <= BOARD_WIDTH + 1; ix += 1) {
      var column: Tile[] = [];
      for (iy = 0; iy <= BOARD_HEIGHT + 1; iy += 1) {
        column.push(createTile(E_EMPTY, 0));
      }
      columns.push(column);
    }

    return columns;
  }

  export function createDefaultStat(): Stat {
    return {
      X: 0,
      Y: 0,
      StepX: 0,
      StepY: 0,
      Cycle: 0,
      P1: 0,
      P2: 0,
      P3: 0,
      Follower: -1,
      Leader: -1,
      Under: createTile(E_EMPTY, 0),
      Data: null,
      DataPos: 0,
      DataLen: 0
    };
  }

  function createStatsArray(): Stat[] {
    var stats: Stat[] = [];
    var i: number;
    for (i = 0; i <= MAX_STAT + 1; i += 1) {
      stats.push(createDefaultStat());
    }
    return stats;
  }

  export function createElementDefDefault(): ElementDef {
    return {
      DrawProc: null,
      TickProc: null,
      TouchProc: null,
      Character: " ",
      Color: COLOR_CHOICE_ON_BLACK,
      Destructible: false,
      Pushable: false,
      VisibleInDark: false,
      PlaceableOnTop: false,
      Walkable: false,
      HasDrawProc: false,
      Cycle: -1,
      EditorCategory: 0,
      EditorShortcut: "",
      Name: "",
      CategoryName: "",
      Param1Name: "",
      Param2Name: "",
      ParamBulletTypeName: "",
      ParamBoardName: "",
      ParamDirName: "",
      ParamTextName: "",
      ScoreValue: 0
    };
  }

  function createElementDefsArray(): ElementDef[] {
    var defs: ElementDef[] = [];
    var i: number;
    for (i = 0; i <= MAX_ELEMENT; i += 1) {
      defs.push(createElementDefDefault());
    }
    return defs;
  }

  export function createBoardInfo(): BoardInfo {
    var unknownPadding: number[] = [];
    var i: number;
    for (i = 0; i < 16; i += 1) {
      unknownPadding.push(0);
    }

    return {
      MaxShots: 255,
      IsDark: false,
      NeighborBoards: [0, 0, 0, 0],
      ReenterWhenZapped: false,
      Message: "",
      StartPlayerX: 0,
      StartPlayerY: 0,
      TimeLimitSec: 0,
      UnknownPadding: unknownPadding
    };
  }

  export function createBoard(): Board {
    return {
      Name: "",
      Tiles: createTileGrid(),
      StatCount: 0,
      Stats: createStatsArray(),
      Info: createBoardInfo()
    };
  }

  export function createWorldInfo(): WorldInfo {
    var unknownPadding: number[] = [];
    var i: number;
    for (i = 0; i < 14; i += 1) {
      unknownPadding.push(0);
    }

    return {
      Ammo: 0,
      Gems: 0,
      Keys: createKeysArray(),
      Health: 100,
      CurrentBoard: 0,
      Torches: 0,
      TorchTicks: 0,
      EnergizerTicks: 0,
      Unknown1: 0,
      Score: 0,
      Name: "",
      Flags: createFlagsArray(),
      BoardTimeSec: 0,
      BoardTimeHsec: 0,
      IsSave: false,
      UnknownPadding: unknownPadding
    };
  }

  export function createWorld(): World {
    var boardData: Array<number[] | null> = [];
    var boardLen: number[] = [];
    var editorSettings: EditorStatSetting[] = [];
    var i: number;

    for (i = 0; i <= MAX_BOARD; i += 1) {
      boardData.push(null);
      boardLen.push(0);
    }

    for (i = 0; i <= MAX_ELEMENT; i += 1) {
      editorSettings.push({
        P1: 4,
        P2: 4,
        P3: 0,
        StepX: 0,
        StepY: -1
      });
    }

    return {
      BoardCount: 0,
      BoardData: boardData,
      BoardLen: boardLen,
      Info: createWorldInfo(),
      EditorStatSettings: editorSettings
    };
  }

  function createHighScoreList(): HighScoreEntry[] {
    var list: HighScoreEntry[] = [];
    var i: number;

    list.push({
      Name: "",
      Score: -1
    });

    for (i = 1; i <= HIGH_SCORE_COUNT; i += 1) {
      list.push({
        Name: "",
        Score: -1
      });
    }

    return list;
  }

  export function cloneStat(stat: Stat): Stat {
    return {
      X: stat.X,
      Y: stat.Y,
      StepX: stat.StepX,
      StepY: stat.StepY,
      Cycle: stat.Cycle,
      P1: stat.P1,
      P2: stat.P2,
      P3: stat.P3,
      Follower: stat.Follower,
      Leader: stat.Leader,
      Under: cloneTile(stat.Under),
      Data: stat.Data,
      DataPos: stat.DataPos,
      DataLen: stat.DataLen
    };
  }

  export function cloneBoardInfo(info: BoardInfo): BoardInfo {
    var unknownPadding: number[] = [];
    var i: number;
    for (i = 0; i < info.UnknownPadding.length; i += 1) {
      unknownPadding.push(info.UnknownPadding[i]);
    }

    return {
      MaxShots: info.MaxShots,
      IsDark: info.IsDark,
      NeighborBoards: [
        info.NeighborBoards[0],
        info.NeighborBoards[1],
        info.NeighborBoards[2],
        info.NeighborBoards[3]
      ],
      ReenterWhenZapped: info.ReenterWhenZapped,
      Message: info.Message,
      StartPlayerX: info.StartPlayerX,
      StartPlayerY: info.StartPlayerY,
      TimeLimitSec: info.TimeLimitSec,
      UnknownPadding: unknownPadding
    };
  }

  export function cloneBoard(board: Board): Board {
    var cloned: Board = createBoard();
    var ix: number;
    var iy: number;
    var i: number;

    cloned.Name = board.Name;
    cloned.StatCount = board.StatCount;
    cloned.Info = cloneBoardInfo(board.Info);

    for (ix = 0; ix <= BOARD_WIDTH + 1; ix += 1) {
      for (iy = 0; iy <= BOARD_HEIGHT + 1; iy += 1) {
        cloned.Tiles[ix][iy] = cloneTile(board.Tiles[ix][iy]);
      }
    }

    for (i = 0; i <= MAX_STAT + 1; i += 1) {
      cloned.Stats[i] = cloneStat(board.Stats[i]);
    }

    return cloned;
  }

  export function cloneWorldInfo(info: WorldInfo): WorldInfo {
    var keys: boolean[] = [];
    var flags: string[] = [];
    var unknownPadding: number[] = [];
    var i: number;

    for (i = 0; i < info.Keys.length; i += 1) {
      keys.push(info.Keys[i]);
    }

    for (i = 0; i < info.Flags.length; i += 1) {
      flags.push(info.Flags[i]);
    }

    for (i = 0; i < info.UnknownPadding.length; i += 1) {
      unknownPadding.push(info.UnknownPadding[i]);
    }

    return {
      Ammo: info.Ammo,
      Gems: info.Gems,
      Keys: keys,
      Health: info.Health,
      CurrentBoard: info.CurrentBoard,
      Torches: info.Torches,
      TorchTicks: info.TorchTicks,
      EnergizerTicks: info.EnergizerTicks,
      Unknown1: info.Unknown1,
      Score: info.Score,
      Name: info.Name,
      Flags: flags,
      BoardTimeSec: info.BoardTimeSec,
      BoardTimeHsec: info.BoardTimeHsec,
      IsSave: info.IsSave,
      UnknownPadding: unknownPadding
    };
  }

  export var PlayerDirX: number = 0;
  export var PlayerDirY: number = 0;

  export var TransitionTable: Coord[] = [];
  export var TransitionTableSize: number = 0;

  export var LoadedGameFileName: string = "";
  export var SavedGameFileName: string = "SAVED";
  export var SavedBoardFileName: string = "TEMP";
  export var StartupWorldFileName: string = "TOWN";

  export var Board: Board = createBoard();
  export var World: World = createWorld();

  export var MessageAmmoNotShown: boolean = true;
  export var MessageOutOfAmmoNotShown: boolean = true;
  export var MessageNoShootingNotShown: boolean = true;
  export var MessageTorchNotShown: boolean = true;
  export var MessageOutOfTorchesNotShown: boolean = true;
  export var MessageRoomNotDarkNotShown: boolean = true;
  export var MessageHintTorchNotShown: boolean = true;
  export var MessageForestNotShown: boolean = true;
  export var MessageFakeNotShown: boolean = true;
  export var MessageGemNotShown: boolean = true;
  export var MessageEnergizerNotShown: boolean = true;

  export var GameTitleExitRequested: boolean = false;
  export var GamePlayExitRequested: boolean = false;
  export var GameStateElement: number = E_MONITOR;
  export var ReturnBoardId: number = 0;

  export var TickSpeed: number = 4;
  export var TickTimeDuration: number = 0;
  export var CurrentTick: number = 0;
  export var CurrentStatTicked: number = 0;
  export var GamePaused: boolean = false;
  export var TickTimeCounter: number = 0;

  export var ElementDefs: ElementDef[] = createElementDefsArray();
  export var EditorPatternCount: number = 0;
  export var EditorPatterns: number[] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

  export var ForceDarknessOff: boolean = false;
  export var InitialTextAttr: number = 0;

  export var OopChar: string = "";
  export var OopWord: string = "";
  export var OopValue: number = -1;

  export var DebugEnabled: boolean = false;

  export var SoundTimeCheckCounter: number = 0;
  export var UseSystemTimeForElapsed: boolean = true;

  export var ResetConfig: boolean = false;
  export var ParsingConfigFile: boolean = false;
  export var EditorEnabled: boolean = true;
  export var JustStarted: boolean = false;
  export var ConfigRegistration: string = "";
  export var ConfigWorldFile: string = "";
  export var HighScoreJsonPath: string = "";
  export var HighScoreBbsName: string = "";
  export var SaveRootPath: string = "";
  export var GameVersion: string = "3.2";

  export var HighScoreList: HighScoreEntry[] = createHighScoreList();

  export var ResourceDataFileName: string = "ZZT.DAT";

  export var WorldFileDescCount: number = 0;
  export var WorldFileDescKeys: string[] = [];
  export var WorldFileDescValues: string[] = [];

  export function setWorldFileDescriptions(keys: string[], values: string[]): void {
    WorldFileDescKeys = keys.slice(0);
    WorldFileDescValues = values.slice(0);
    WorldFileDescCount = keys.length;
  }

  export function trimWorldExtension(worldName: string): string {
    var upper: string = worldName.toUpperCase();
    if (upper.length > 4 && upper.slice(upper.length - 4) === ".ZZT") {
      return worldName.slice(0, worldName.length - 4);
    }
    return worldName;
  }
}
