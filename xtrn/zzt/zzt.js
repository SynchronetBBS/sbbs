"use strict";
var ZZT;
(function (ZZT) {
    function safeJoin(base, file) {
        if (!base) {
            return file;
        }
        var end = base.charAt(base.length - 1);
        if (end === "/" || end === "\\") {
            return base + file;
        }
        return base + "/" + file;
    }
    function makeFallbackWriter() {
        return function writeFallback(message) {
            if (typeof console !== "undefined" && typeof console.writeln === "function") {
                console.writeln(message);
                return;
            }
            if (typeof console !== "undefined" && typeof console.log === "function") {
                console.log(message);
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
    function byteToCharCode(byte) {
        return String.fromCharCode(byte & 0xff);
    }
    function getSyncFile(path) {
        var f;
        if (typeof File === "undefined") {
            return null;
        }
        try {
            f = new File(path);
        }
        catch (_err) {
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
    ZZT.runtime = {
        getExecDir: function getExecDir() {
            if (typeof js !== "undefined" && typeof js.exec_dir === "string" && js.exec_dir.length > 0) {
                return js.exec_dir;
            }
            return ".";
        },
        getArgv: function getArgv() {
            if (typeof argv === "undefined" || argv === null) {
                return [];
            }
            var result = [];
            for (var i = 0; i < argv.length; i += 1) {
                result.push(String(argv[i]));
            }
            return result;
        },
        readTextFileLines: function readTextFileLines(path) {
            var lines = [];
            var f;
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
            }
            finally {
                f.close();
            }
            return lines;
        },
        readBinaryFile: function readBinaryFile(path) {
            var f = getSyncFile(path);
            var bytes = [];
            if (f === null) {
                return null;
            }
            if (!f.exists || !f.open("rb")) {
                return null;
            }
            try {
                while (true) {
                    var chunk = f.read(4096);
                    var i;
                    if (chunk === null || typeof chunk === "undefined" || chunk.length === 0) {
                        break;
                    }
                    for (i = 0; i < chunk.length; i += 1) {
                        bytes.push(chunk.charCodeAt(i) & 0xff);
                    }
                }
            }
            finally {
                f.close();
            }
            return bytes;
        },
        writeBinaryFile: function writeBinaryFile(path, bytes) {
            var f = getSyncFile(path);
            var offset;
            var end;
            var i;
            var chunkChars;
            var chunkSize = 4096;
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
            }
            finally {
                f.close();
            }
            return true;
        },
        writeLine: function writeLine(message) {
            fallbackWriteLine(message);
        },
        clearScreen: function clearScreen() {
            if (typeof console !== "undefined" &&
                typeof console.clear === "function" &&
                typeof console.writeln === "function") {
                console.clear();
            }
        },
        gotoXY: function gotoXY(x, y) {
            if (typeof console !== "undefined" && typeof console.gotoxy === "function") {
                console.gotoxy(x, y);
            }
        },
        isTerminated: function isTerminated() {
            return typeof js !== "undefined" && js.terminated === true;
        }
    };
    function execPath(name) {
        return safeJoin(ZZT.runtime.getExecDir(), name);
    }
    ZZT.execPath = execPath;
})(ZZT || (ZZT = {}));
var ZZT;
(function (ZZT) {
    ZZT.MAX_STAT = 150;
    ZZT.MAX_ELEMENT = 53;
    ZZT.MAX_BOARD = 100;
    ZZT.MAX_FLAG = 10;
    ZZT.BOARD_WIDTH = 60;
    ZZT.BOARD_HEIGHT = 25;
    ZZT.WORLD_FILE_HEADER_SIZE = 512;
    ZZT.HIGH_SCORE_COUNT = 30;
    ZZT.TORCH_DURATION = 200;
    ZZT.TORCH_DX = 8;
    ZZT.TORCH_DY = 5;
    ZZT.TORCH_DIST_SQR = 50;
    ZZT.E_EMPTY = 0;
    ZZT.E_BOARD_EDGE = 1;
    ZZT.E_MESSAGE_TIMER = 2;
    ZZT.E_MONITOR = 3;
    ZZT.E_PLAYER = 4;
    ZZT.E_AMMO = 5;
    ZZT.E_TORCH = 6;
    ZZT.E_GEM = 7;
    ZZT.E_KEY = 8;
    ZZT.E_DOOR = 9;
    ZZT.E_SCROLL = 10;
    ZZT.E_PASSAGE = 11;
    ZZT.E_DUPLICATOR = 12;
    ZZT.E_BOMB = 13;
    ZZT.E_ENERGIZER = 14;
    ZZT.E_STAR = 15;
    ZZT.E_CONVEYOR_CW = 16;
    ZZT.E_CONVEYOR_CCW = 17;
    ZZT.E_BULLET = 18;
    ZZT.E_WATER = 19;
    ZZT.E_FOREST = 20;
    ZZT.E_SOLID = 21;
    ZZT.E_NORMAL = 22;
    ZZT.E_BREAKABLE = 23;
    ZZT.E_BOULDER = 24;
    ZZT.E_SLIDER_NS = 25;
    ZZT.E_SLIDER_EW = 26;
    ZZT.E_FAKE = 27;
    ZZT.E_INVISIBLE = 28;
    ZZT.E_BLINK_WALL = 29;
    ZZT.E_TRANSPORTER = 30;
    ZZT.E_LINE = 31;
    ZZT.E_RICOCHET = 32;
    ZZT.E_BLINK_RAY_EW = 33;
    ZZT.E_BEAR = 34;
    ZZT.E_RUFFIAN = 35;
    ZZT.E_OBJECT = 36;
    ZZT.E_SLIME = 37;
    ZZT.E_SHARK = 38;
    ZZT.E_SPINNING_GUN = 39;
    ZZT.E_PUSHER = 40;
    ZZT.E_LION = 41;
    ZZT.E_TIGER = 42;
    ZZT.E_BLINK_RAY_NS = 43;
    ZZT.E_CENTIPEDE_HEAD = 44;
    ZZT.E_CENTIPEDE_SEGMENT = 45;
    ZZT.E_TEXT_BLUE = 47;
    ZZT.E_TEXT_GREEN = 48;
    ZZT.E_TEXT_CYAN = 49;
    ZZT.E_TEXT_RED = 50;
    ZZT.E_TEXT_PURPLE = 51;
    ZZT.E_TEXT_YELLOW = 52;
    ZZT.E_TEXT_WHITE = 53;
    ZZT.E_TEXT_MIN = ZZT.E_TEXT_BLUE;
    ZZT.CATEGORY_ITEM = 1;
    ZZT.CATEGORY_CREATURE = 2;
    ZZT.CATEGORY_TERRAIN = 3;
    ZZT.COLOR_SPECIAL_MIN = 0xf0;
    ZZT.COLOR_CHOICE_ON_BLACK = 0xff;
    ZZT.COLOR_WHITE_ON_CHOICE = 0xfe;
    ZZT.COLOR_CHOICE_ON_CHOICE = 0xfd;
    ZZT.SHOT_SOURCE_PLAYER = 0;
    ZZT.SHOT_SOURCE_ENEMY = 1;
    function createKeysArray() {
        var keys = [];
        var i;
        for (i = 0; i <= 7; i += 1) {
            keys.push(false);
        }
        return keys;
    }
    function createFlagsArray() {
        var flags = [];
        var i;
        for (i = 0; i <= ZZT.MAX_FLAG; i += 1) {
            flags.push("");
        }
        return flags;
    }
    function createTile(element, color) {
        return {
            Element: element,
            Color: color
        };
    }
    ZZT.createTile = createTile;
    function cloneTile(tile) {
        return {
            Element: tile.Element,
            Color: tile.Color
        };
    }
    ZZT.cloneTile = cloneTile;
    function createTileGrid() {
        var columns = [];
        var ix;
        var iy;
        for (ix = 0; ix <= ZZT.BOARD_WIDTH + 1; ix += 1) {
            var column = [];
            for (iy = 0; iy <= ZZT.BOARD_HEIGHT + 1; iy += 1) {
                column.push(createTile(ZZT.E_EMPTY, 0));
            }
            columns.push(column);
        }
        return columns;
    }
    function createDefaultStat() {
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
            Under: createTile(ZZT.E_EMPTY, 0),
            Data: null,
            DataPos: 0,
            DataLen: 0
        };
    }
    ZZT.createDefaultStat = createDefaultStat;
    function createStatsArray() {
        var stats = [];
        var i;
        for (i = 0; i <= ZZT.MAX_STAT + 1; i += 1) {
            stats.push(createDefaultStat());
        }
        return stats;
    }
    function createElementDefDefault() {
        return {
            DrawProc: null,
            TickProc: null,
            TouchProc: null,
            Character: " ",
            Color: ZZT.COLOR_CHOICE_ON_BLACK,
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
    ZZT.createElementDefDefault = createElementDefDefault;
    function createElementDefsArray() {
        var defs = [];
        var i;
        for (i = 0; i <= ZZT.MAX_ELEMENT; i += 1) {
            defs.push(createElementDefDefault());
        }
        return defs;
    }
    function createBoardInfo() {
        var unknownPadding = [];
        var i;
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
    ZZT.createBoardInfo = createBoardInfo;
    function createBoard() {
        return {
            Name: "",
            Tiles: createTileGrid(),
            StatCount: 0,
            Stats: createStatsArray(),
            Info: createBoardInfo()
        };
    }
    ZZT.createBoard = createBoard;
    function createWorldInfo() {
        var unknownPadding = [];
        var i;
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
    ZZT.createWorldInfo = createWorldInfo;
    function createWorld() {
        var boardData = [];
        var boardLen = [];
        var editorSettings = [];
        var i;
        for (i = 0; i <= ZZT.MAX_BOARD; i += 1) {
            boardData.push(null);
            boardLen.push(0);
        }
        for (i = 0; i <= ZZT.MAX_ELEMENT; i += 1) {
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
    ZZT.createWorld = createWorld;
    function createHighScoreList() {
        var list = [];
        var i;
        list.push({
            Name: "",
            Score: -1
        });
        for (i = 1; i <= ZZT.HIGH_SCORE_COUNT; i += 1) {
            list.push({
                Name: "",
                Score: -1
            });
        }
        return list;
    }
    function cloneStat(stat) {
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
    ZZT.cloneStat = cloneStat;
    function cloneBoardInfo(info) {
        var unknownPadding = [];
        var i;
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
    ZZT.cloneBoardInfo = cloneBoardInfo;
    function cloneBoard(board) {
        var cloned = createBoard();
        var ix;
        var iy;
        var i;
        cloned.Name = board.Name;
        cloned.StatCount = board.StatCount;
        cloned.Info = cloneBoardInfo(board.Info);
        for (ix = 0; ix <= ZZT.BOARD_WIDTH + 1; ix += 1) {
            for (iy = 0; iy <= ZZT.BOARD_HEIGHT + 1; iy += 1) {
                cloned.Tiles[ix][iy] = cloneTile(board.Tiles[ix][iy]);
            }
        }
        for (i = 0; i <= ZZT.MAX_STAT + 1; i += 1) {
            cloned.Stats[i] = cloneStat(board.Stats[i]);
        }
        return cloned;
    }
    ZZT.cloneBoard = cloneBoard;
    function cloneWorldInfo(info) {
        var keys = [];
        var flags = [];
        var unknownPadding = [];
        var i;
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
    ZZT.cloneWorldInfo = cloneWorldInfo;
    ZZT.PlayerDirX = 0;
    ZZT.PlayerDirY = 0;
    ZZT.TransitionTable = [];
    ZZT.TransitionTableSize = 0;
    ZZT.LoadedGameFileName = "";
    ZZT.SavedGameFileName = "SAVED";
    ZZT.SavedBoardFileName = "TEMP";
    ZZT.StartupWorldFileName = "TOWN";
    ZZT.Board = createBoard();
    ZZT.World = createWorld();
    ZZT.MessageAmmoNotShown = true;
    ZZT.MessageOutOfAmmoNotShown = true;
    ZZT.MessageNoShootingNotShown = true;
    ZZT.MessageTorchNotShown = true;
    ZZT.MessageOutOfTorchesNotShown = true;
    ZZT.MessageRoomNotDarkNotShown = true;
    ZZT.MessageHintTorchNotShown = true;
    ZZT.MessageForestNotShown = true;
    ZZT.MessageFakeNotShown = true;
    ZZT.MessageGemNotShown = true;
    ZZT.MessageEnergizerNotShown = true;
    ZZT.GameTitleExitRequested = false;
    ZZT.GamePlayExitRequested = false;
    ZZT.GameStateElement = ZZT.E_MONITOR;
    ZZT.ReturnBoardId = 0;
    ZZT.TickSpeed = 4;
    ZZT.TickTimeDuration = 0;
    ZZT.CurrentTick = 0;
    ZZT.CurrentStatTicked = 0;
    ZZT.GamePaused = false;
    ZZT.TickTimeCounter = 0;
    ZZT.ElementDefs = createElementDefsArray();
    ZZT.EditorPatternCount = 0;
    ZZT.EditorPatterns = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    ZZT.ForceDarknessOff = false;
    ZZT.InitialTextAttr = 0;
    ZZT.OopChar = "";
    ZZT.OopWord = "";
    ZZT.OopValue = -1;
    ZZT.DebugEnabled = false;
    ZZT.SoundTimeCheckCounter = 0;
    ZZT.UseSystemTimeForElapsed = true;
    ZZT.ResetConfig = false;
    ZZT.ParsingConfigFile = false;
    ZZT.EditorEnabled = true;
    ZZT.JustStarted = false;
    ZZT.ConfigRegistration = "";
    ZZT.ConfigWorldFile = "";
    ZZT.HighScoreJsonPath = "";
    ZZT.HighScoreBbsName = "";
    ZZT.SaveRootPath = "";
    ZZT.GameVersion = "3.2";
    ZZT.HighScoreList = createHighScoreList();
    ZZT.ResourceDataFileName = "ZZT.DAT";
    ZZT.WorldFileDescCount = 0;
    ZZT.WorldFileDescKeys = [];
    ZZT.WorldFileDescValues = [];
    function setWorldFileDescriptions(keys, values) {
        ZZT.WorldFileDescKeys = keys.slice(0);
        ZZT.WorldFileDescValues = values.slice(0);
        ZZT.WorldFileDescCount = keys.length;
    }
    ZZT.setWorldFileDescriptions = setWorldFileDescriptions;
    function trimWorldExtension(worldName) {
        var upper = worldName.toUpperCase();
        if (upper.length > 4 && upper.slice(upper.length - 4) === ".ZZT") {
            return worldName.slice(0, worldName.length - 4);
        }
        return worldName;
    }
    ZZT.trimWorldExtension = trimWorldExtension;
})(ZZT || (ZZT = {}));
var ZZT;
(function (ZZT) {
    var OOP_COLOR_NAMES = ["", "Blue", "Green", "Cyan", "Red", "Purple", "Yellow", "White"];
    var OOP_NUL = String.fromCharCode(0);
    var OOP_CR = "\r";
    function oopUpper(input) {
        return input.toUpperCase();
    }
    function oopRandomInt(maxExclusive) {
        if (maxExclusive <= 0) {
            return 0;
        }
        return Math.floor(Math.random() * maxExclusive);
    }
    function oopRepeatChar(ch, count) {
        var out = "";
        var i;
        for (i = 0; i < count; i += 1) {
            out += ch;
        }
        return out;
    }
    function oopDataForStat(statId) {
        var stat;
        if (statId < 0 || statId > ZZT.Board.StatCount) {
            return "";
        }
        stat = ZZT.Board.Stats[statId];
        if (stat.Data === null) {
            return "";
        }
        return stat.Data;
    }
    function oopEnsureDataLength(data, len) {
        if (len <= data.length) {
            return data;
        }
        return data + oopRepeatChar(OOP_NUL, len - data.length);
    }
    function oopReadAt(statId, position) {
        var stat;
        var data;
        if (statId < 0 || statId > ZZT.Board.StatCount) {
            return OOP_NUL;
        }
        stat = ZZT.Board.Stats[statId];
        if (position < 0 || position >= stat.DataLen) {
            return OOP_NUL;
        }
        data = oopDataForStat(statId);
        if (position >= data.length) {
            return OOP_NUL;
        }
        return data.charAt(position);
    }
    function oopSetSharedData(oldData, newData) {
        var i;
        for (i = 0; i <= ZZT.Board.StatCount; i += 1) {
            if (ZZT.Board.Stats[i].Data === oldData) {
                ZZT.Board.Stats[i].Data = newData;
                ZZT.Board.Stats[i].DataLen = newData.length;
            }
        }
    }
    function oopSetDataChar(statId, dataPos, ch) {
        var stat;
        var oldData;
        var data;
        var nextData;
        if (statId < 0 || statId > ZZT.Board.StatCount) {
            return;
        }
        stat = ZZT.Board.Stats[statId];
        if (dataPos < 0 || dataPos >= stat.DataLen) {
            return;
        }
        oldData = stat.Data;
        data = oopEnsureDataLength(oopDataForStat(statId), stat.DataLen);
        nextData = data.slice(0, dataPos) + ch + data.slice(dataPos + 1);
        oopSetSharedData(oldData, nextData);
    }
    function oopError(statId, message) {
        if (statId >= 0 && statId <= ZZT.Board.StatCount) {
            ZZT.DisplayMessage(180, "ERR: " + message);
            ZZT.SoundQueue(5, "\x50\x0A");
            ZZT.Board.Stats[statId].DataPos = -1;
        }
    }
    function oopReadChar(statId, position) {
        if (position >= 0 && position < ZZT.Board.Stats[statId].DataLen) {
            ZZT.OopChar = oopReadAt(statId, position);
            position += 1;
        }
        else {
            ZZT.OopChar = OOP_NUL;
        }
        return position;
    }
    function oopReadWord(statId, position) {
        ZZT.OopWord = "";
        do {
            position = oopReadChar(statId, position);
        } while (ZZT.OopChar === " ");
        ZZT.OopChar = oopUpper(ZZT.OopChar);
        if (!(ZZT.OopChar >= "0" && ZZT.OopChar <= "9")) {
            while ((ZZT.OopChar >= "A" && ZZT.OopChar <= "Z") ||
                ZZT.OopChar === ":" ||
                ZZT.OopChar === "_" ||
                (ZZT.OopChar >= "0" && ZZT.OopChar <= "9")) {
                ZZT.OopWord += ZZT.OopChar;
                position = oopReadChar(statId, position);
                ZZT.OopChar = oopUpper(ZZT.OopChar);
            }
        }
        if (position > 0) {
            position -= 1;
        }
        return position;
    }
    function oopReadValue(statId, position) {
        var textValue = "";
        var parsed;
        do {
            position = oopReadChar(statId, position);
        } while (ZZT.OopChar === " ");
        ZZT.OopChar = oopUpper(ZZT.OopChar);
        while (ZZT.OopChar >= "0" && ZZT.OopChar <= "9") {
            textValue += ZZT.OopChar;
            position = oopReadChar(statId, position);
            ZZT.OopChar = oopUpper(ZZT.OopChar);
        }
        if (position > 0) {
            position -= 1;
        }
        if (textValue.length > 0) {
            parsed = parseInt(textValue, 10);
            if (isNaN(parsed)) {
                ZZT.OopValue = -1;
            }
            else {
                ZZT.OopValue = parsed;
            }
        }
        else {
            ZZT.OopValue = -1;
        }
        return position;
    }
    function oopSkipLine(statId, position) {
        do {
            position = oopReadChar(statId, position);
        } while (ZZT.OopChar !== OOP_NUL && ZZT.OopChar !== OOP_CR);
        return position;
    }
    function oopParseDirection(statId, position) {
        var result = {
            Position: position,
            DeltaX: 0,
            DeltaY: 0,
            Success: true
        };
        var parsed;
        var basicDir;
        if (ZZT.OopWord === "N" || ZZT.OopWord === "NORTH") {
            result.DeltaX = 0;
            result.DeltaY = -1;
        }
        else if (ZZT.OopWord === "S" || ZZT.OopWord === "SOUTH") {
            result.DeltaX = 0;
            result.DeltaY = 1;
        }
        else if (ZZT.OopWord === "E" || ZZT.OopWord === "EAST") {
            result.DeltaX = 1;
            result.DeltaY = 0;
        }
        else if (ZZT.OopWord === "W" || ZZT.OopWord === "WEST") {
            result.DeltaX = -1;
            result.DeltaY = 0;
        }
        else if (ZZT.OopWord === "I" || ZZT.OopWord === "IDLE") {
            result.DeltaX = 0;
            result.DeltaY = 0;
        }
        else if (ZZT.OopWord === "SEEK") {
            basicDir = ZZT.CalcDirectionSeek(ZZT.Board.Stats[statId].X, ZZT.Board.Stats[statId].Y);
            result.DeltaX = basicDir.DeltaX;
            result.DeltaY = basicDir.DeltaY;
        }
        else if (ZZT.OopWord === "FLOW") {
            result.DeltaX = ZZT.Board.Stats[statId].StepX;
            result.DeltaY = ZZT.Board.Stats[statId].StepY;
        }
        else if (ZZT.OopWord === "RND") {
            basicDir = ZZT.CalcDirectionRnd();
            result.DeltaX = basicDir.DeltaX;
            result.DeltaY = basicDir.DeltaY;
        }
        else if (ZZT.OopWord === "RNDNS") {
            result.DeltaX = 0;
            result.DeltaY = oopRandomInt(2) * 2 - 1;
        }
        else if (ZZT.OopWord === "RNDNE") {
            result.DeltaX = oopRandomInt(2);
            if (result.DeltaX === 0) {
                result.DeltaY = -1;
            }
            else {
                result.DeltaY = 0;
            }
        }
        else if (ZZT.OopWord === "CW") {
            result.Position = oopReadWord(statId, result.Position);
            parsed = oopParseDirection(statId, result.Position);
            result.Position = parsed.Position;
            result.DeltaX = -parsed.DeltaY;
            result.DeltaY = parsed.DeltaX;
            result.Success = parsed.Success;
        }
        else if (ZZT.OopWord === "CCW") {
            result.Position = oopReadWord(statId, result.Position);
            parsed = oopParseDirection(statId, result.Position);
            result.Position = parsed.Position;
            result.DeltaX = parsed.DeltaY;
            result.DeltaY = -parsed.DeltaX;
            result.Success = parsed.Success;
        }
        else if (ZZT.OopWord === "RNDP") {
            result.Position = oopReadWord(statId, result.Position);
            parsed = oopParseDirection(statId, result.Position);
            result.Position = parsed.Position;
            result.Success = parsed.Success;
            result.DeltaX = parsed.DeltaY;
            result.DeltaY = parsed.DeltaX;
            if (oopRandomInt(2) === 0) {
                result.DeltaX = -result.DeltaX;
            }
            else {
                result.DeltaY = -result.DeltaY;
            }
        }
        else if (ZZT.OopWord === "OPP") {
            result.Position = oopReadWord(statId, result.Position);
            parsed = oopParseDirection(statId, result.Position);
            result.Position = parsed.Position;
            result.DeltaX = -parsed.DeltaX;
            result.DeltaY = -parsed.DeltaY;
            result.Success = parsed.Success;
        }
        else {
            result.Success = false;
            result.DeltaX = 0;
            result.DeltaY = 0;
        }
        return result;
    }
    function oopReadDirection(statId, position) {
        var result;
        position = oopReadWord(statId, position);
        result = oopParseDirection(statId, position);
        if (!result.Success) {
            oopError(statId, "Bad direction");
        }
        return result;
    }
    function oopFindString(statId, lookup) {
        var pos;
        var i;
        var nextCh;
        var isMatch;
        for (pos = 0; pos <= ZZT.Board.Stats[statId].DataLen; pos += 1) {
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
            if ((nextCh >= "A" && nextCh <= "Z") ||
                (nextCh >= "0" && nextCh <= "9") ||
                nextCh === "_") {
                continue;
            }
            return pos;
        }
        return -1;
    }
    function oopIterateStat(statId, iStat, lookup) {
        var found = false;
        var pos;
        iStat += 1;
        if (lookup === "ALL") {
            found = iStat <= ZZT.Board.StatCount;
        }
        else if (lookup === "OTHERS") {
            if (iStat <= ZZT.Board.StatCount) {
                if (iStat !== statId) {
                    found = true;
                }
                else {
                    iStat += 1;
                    found = iStat <= ZZT.Board.StatCount;
                }
            }
        }
        else if (lookup === "SELF") {
            if (statId > 0 && iStat <= statId) {
                iStat = statId;
                found = true;
            }
        }
        else {
            while (iStat <= ZZT.Board.StatCount && !found) {
                if (ZZT.Board.Stats[iStat].Data !== null) {
                    pos = 0;
                    pos = oopReadChar(iStat, pos);
                    if (ZZT.OopChar === "@") {
                        pos = oopReadWord(iStat, pos);
                        if (ZZT.OopWord === lookup) {
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
    function oopFindLabel(statId, sendLabel, state, labelPrefix) {
        var targetSplitPos = sendLabel.indexOf(":");
        var targetLookup;
        var objectMessage;
        var iter;
        if (targetSplitPos < 0) {
            if (state.StatId >= statId) {
                return false;
            }
            state.StatId = statId;
            objectMessage = sendLabel;
            if (oopUpper(objectMessage) === "RESTART") {
                state.DataPos = 0;
            }
            else {
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
    function WorldGetFlagPosition(name) {
        var i;
        var lookup = oopUpper(name);
        for (i = 1; i <= ZZT.MAX_FLAG; i += 1) {
            if (oopUpper(ZZT.World.Info.Flags[i]) === lookup) {
                return i;
            }
        }
        return -1;
    }
    ZZT.WorldGetFlagPosition = WorldGetFlagPosition;
    function WorldSetFlag(name) {
        var i;
        if (WorldGetFlagPosition(name) >= 0) {
            return;
        }
        i = 1;
        while (i < ZZT.MAX_FLAG && ZZT.World.Info.Flags[i].length > 0) {
            i += 1;
        }
        if (i <= ZZT.MAX_FLAG) {
            ZZT.World.Info.Flags[i] = name;
        }
    }
    ZZT.WorldSetFlag = WorldSetFlag;
    function WorldClearFlag(name) {
        var pos = WorldGetFlagPosition(name);
        if (pos >= 0) {
            ZZT.World.Info.Flags[pos] = "";
        }
    }
    ZZT.WorldClearFlag = WorldClearFlag;
    function oopStringToWord(input) {
        var output = "";
        var i;
        var ch;
        for (i = 0; i < input.length; i += 1) {
            ch = input.charAt(i);
            if ((ch >= "A" && ch <= "Z") || (ch >= "0" && ch <= "9")) {
                output += ch;
            }
            else if (ch >= "a" && ch <= "z") {
                output += oopUpper(ch);
            }
        }
        return output;
    }
    function oopParseTile(statId, position) {
        var tile = ZZT.createTile(ZZT.E_EMPTY, 0);
        var i;
        position = oopReadWord(statId, position);
        for (i = 1; i <= 7; i += 1) {
            if (ZZT.OopWord === oopStringToWord(OOP_COLOR_NAMES[i])) {
                tile.Color = i + 8;
                position = oopReadWord(statId, position);
                break;
            }
        }
        for (i = 0; i <= ZZT.MAX_ELEMENT; i += 1) {
            if (ZZT.OopWord === oopStringToWord(ZZT.ElementDefs[i].Name)) {
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
    function oopGetColorForTileMatch(tile) {
        if (ZZT.ElementDefs[tile.Element].Color < ZZT.COLOR_SPECIAL_MIN) {
            return ZZT.ElementDefs[tile.Element].Color & 0x07;
        }
        if (ZZT.ElementDefs[tile.Element].Color === ZZT.COLOR_WHITE_ON_CHOICE) {
            return ((tile.Color >> 4) & 0x0f) + 8;
        }
        return tile.Color & 0x0f;
    }
    function oopFindTileOnBoard(startX, startY, tile) {
        var x = startX;
        var y = startY;
        while (true) {
            x += 1;
            if (x > ZZT.BOARD_WIDTH) {
                x = 1;
                y += 1;
                if (y > ZZT.BOARD_HEIGHT) {
                    return {
                        X: x,
                        Y: y,
                        Found: false
                    };
                }
            }
            if (ZZT.Board.Tiles[x][y].Element !== tile.Element) {
                continue;
            }
            if (tile.Color === 0 || oopGetColorForTileMatch(ZZT.Board.Tiles[x][y]) === tile.Color) {
                return {
                    X: x,
                    Y: y,
                    Found: true
                };
            }
        }
    }
    function oopPlaceTile(x, y, tile) {
        var color = tile.Color;
        var tileDef = ZZT.ElementDefs[tile.Element];
        if (ZZT.Board.Tiles[x][y].Element === ZZT.E_PLAYER) {
            return;
        }
        if (tileDef.Color < ZZT.COLOR_SPECIAL_MIN) {
            color = tileDef.Color;
        }
        else {
            if (color === 0) {
                color = ZZT.Board.Tiles[x][y].Color;
            }
            if (color === 0) {
                color = 0x0f;
            }
            if (tileDef.Color === ZZT.COLOR_WHITE_ON_CHOICE) {
                color = ((color - 8) * 0x10) + 0x0f;
            }
        }
        if (ZZT.Board.Tiles[x][y].Element === tile.Element) {
            ZZT.Board.Tiles[x][y].Color = color;
        }
        else {
            ZZT.BoardDamageTile(x, y);
            if (ZZT.ElementDefs[tile.Element].Cycle >= 0) {
                ZZT.AddStat(x, y, tile.Element, color, ZZT.ElementDefs[tile.Element].Cycle, ZZT.createDefaultStat());
            }
            else {
                ZZT.Board.Tiles[x][y].Element = tile.Element;
                ZZT.Board.Tiles[x][y].Color = color;
            }
        }
        ZZT.BoardDrawTile(x, y);
    }
    function oopCheckCondition(statId, position) {
        var stat = ZZT.Board.Stats[statId];
        var dir;
        var parseTile;
        var findTile;
        var result;
        if (ZZT.OopWord === "NOT") {
            position = oopReadWord(statId, position);
            var negated = oopCheckCondition(statId, position);
            return {
                Position: negated.Position,
                Result: !negated.Result
            };
        }
        if (ZZT.OopWord === "ALLIGNED") {
            result = stat.X === ZZT.Board.Stats[0].X || stat.Y === ZZT.Board.Stats[0].Y;
            return {
                Position: position,
                Result: result
            };
        }
        if (ZZT.OopWord === "CONTACT") {
            result = ((stat.X - ZZT.Board.Stats[0].X) * (stat.X - ZZT.Board.Stats[0].X) +
                (stat.Y - ZZT.Board.Stats[0].Y) * (stat.Y - ZZT.Board.Stats[0].Y)) === 1;
            return {
                Position: position,
                Result: result
            };
        }
        if (ZZT.OopWord === "BLOCKED") {
            dir = oopReadDirection(statId, position);
            position = dir.Position;
            result = !ZZT.ElementDefs[ZZT.Board.Tiles[stat.X + dir.DeltaX][stat.Y + dir.DeltaY].Element].Walkable;
            return {
                Position: position,
                Result: result
            };
        }
        if (ZZT.OopWord === "ENERGIZED") {
            return {
                Position: position,
                Result: ZZT.World.Info.EnergizerTicks > 0
            };
        }
        if (ZZT.OopWord === "ANY") {
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
            Result: WorldGetFlagPosition(ZZT.OopWord) >= 0
        };
    }
    function oopReadLineToEnd(statId, position) {
        var line = "";
        position = oopReadChar(statId, position);
        while (ZZT.OopChar !== OOP_NUL && ZZT.OopChar !== OOP_CR) {
            line += ZZT.OopChar;
            position = oopReadChar(statId, position);
        }
        return {
            Position: position,
            Line: line
        };
    }
    function OopSend(statId, sendLabel, ignoreLock) {
        var iDataPos;
        var iStat;
        var state;
        var respectSelfLock;
        var sentToSelf = false;
        if (statId < 0) {
            statId = -statId;
            respectSelfLock = true;
        }
        else {
            respectSelfLock = false;
        }
        state = {
            StatId: 0,
            DataPos: -1
        };
        while (oopFindLabel(statId, sendLabel, state, OOP_CR + ":")) {
            iStat = state.StatId;
            iDataPos = state.DataPos;
            if (iStat < 0 || iStat > ZZT.Board.StatCount) {
                continue;
            }
            if (ZZT.Board.Stats[iStat].P2 === 0 ||
                ignoreLock ||
                ((statId === iStat) && !respectSelfLock)) {
                if (iStat === statId) {
                    sentToSelf = true;
                }
                ZZT.Board.Stats[iStat].DataPos = iDataPos;
            }
        }
        return sentToSelf;
    }
    ZZT.OopSend = OopSend;
    function oopAppendDisplayLine(state, line) {
        ZZT.TextWindowAppend(state, line);
    }
    function oopReadNameLine(statId) {
        var data = oopDataForStat(statId);
        var endLine;
        if (data.length <= 0 || data.charAt(0) !== "@") {
            return "";
        }
        endLine = data.indexOf(OOP_CR);
        if (endLine < 0) {
            endLine = data.length;
        }
        return data.slice(1, endLine);
    }
    function oopFindCounterValue(name) {
        if (name === "HEALTH") {
            return ZZT.World.Info.Health;
        }
        if (name === "AMMO") {
            return ZZT.World.Info.Ammo;
        }
        if (name === "GEMS") {
            return ZZT.World.Info.Gems;
        }
        if (name === "TORCHES") {
            return ZZT.World.Info.Torches;
        }
        if (name === "SCORE") {
            return ZZT.World.Info.Score;
        }
        if (name === "TIME") {
            return ZZT.World.Info.BoardTimeSec;
        }
        return null;
    }
    function oopSetCounterValue(name, value) {
        if (name === "HEALTH") {
            ZZT.World.Info.Health = value;
        }
        else if (name === "AMMO") {
            ZZT.World.Info.Ammo = value;
        }
        else if (name === "GEMS") {
            ZZT.World.Info.Gems = value;
        }
        else if (name === "TORCHES") {
            ZZT.World.Info.Torches = value;
        }
        else if (name === "SCORE") {
            ZZT.World.Info.Score = value;
        }
        else if (name === "TIME") {
            ZZT.World.Info.BoardTimeSec = value;
        }
    }
    function OopExecute(statId, position, name) {
        var restartParsing = true;
        while (restartParsing) {
            var textWindow = {
                Selectable: false,
                LineCount: 0,
                LinePos: 1,
                Lines: [],
                Hyperlink: "",
                Title: "",
                LoadedFilename: "",
                ScreenCopy: []
            };
            var stopRunning = false;
            var repeatInsNextTick = false;
            var replaceStat = false;
            var endOfProgram = false;
            var replaceTile = ZZT.createTile(ZZT.E_EMPTY, 0x0f);
            var lastPosition = position;
            var lineFinished;
            var textLine;
            var lineData;
            var lineFirstChar;
            var readDir;
            var conditionResult;
            var parseTile;
            var parseTile2;
            var stat;
            var tx;
            var ty;
            var ix;
            var iy;
            var scan;
            var commandLoop;
            var insCount = 0;
            var currentValue;
            var targetCounter = "";
            var deltaValue;
            var labelState;
            var objectName;
            var bindStat;
            restartParsing = false;
            ZZT.TextWindowInitState(textWindow);
            while (!endOfProgram && !stopRunning && !repeatInsNextTick && !replaceStat && insCount <= 32) {
                lineFinished = true;
                lastPosition = position;
                position = oopReadChar(statId, position);
                while (ZZT.OopChar === ":") {
                    position = oopSkipLine(statId, position);
                    position = oopReadChar(statId, position);
                }
                if (ZZT.OopChar === "'" || ZZT.OopChar === "@") {
                    position = oopSkipLine(statId, position);
                    continue;
                }
                if (ZZT.OopChar === "/" || ZZT.OopChar === "?") {
                    if (ZZT.OopChar === "/") {
                        repeatInsNextTick = true;
                    }
                    position = oopReadWord(statId, position);
                    readDir = oopParseDirection(statId, position);
                    position = readDir.Position;
                    if (readDir.Success) {
                        stat = ZZT.Board.Stats[statId];
                        if (readDir.DeltaX !== 0 || readDir.DeltaY !== 0) {
                            if (!ZZT.ElementDefs[ZZT.Board.Tiles[stat.X + readDir.DeltaX][stat.Y + readDir.DeltaY].Element].Walkable) {
                                ZZT.ElementPushablePush(stat.X + readDir.DeltaX, stat.Y + readDir.DeltaY, readDir.DeltaX, readDir.DeltaY);
                            }
                            if (ZZT.ElementDefs[ZZT.Board.Tiles[stat.X + readDir.DeltaX][stat.Y + readDir.DeltaY].Element].Walkable) {
                                ZZT.MoveStat(statId, stat.X + readDir.DeltaX, stat.Y + readDir.DeltaY);
                                repeatInsNextTick = false;
                            }
                        }
                        else {
                            repeatInsNextTick = false;
                        }
                        position = oopReadChar(statId, position);
                        if (ZZT.OopChar !== OOP_CR) {
                            position -= 1;
                        }
                        stopRunning = true;
                    }
                    else {
                        oopError(statId, "Bad direction");
                    }
                    continue;
                }
                if (ZZT.OopChar === "#") {
                    commandLoop = true;
                    while (commandLoop) {
                        commandLoop = false;
                        position = oopReadWord(statId, position);
                        if (ZZT.OopWord === "THEN") {
                            position = oopReadWord(statId, position);
                        }
                        if (ZZT.OopWord.length === 0) {
                            break;
                        }
                        insCount += 1;
                        if (ZZT.OopWord === "GO") {
                            readDir = oopReadDirection(statId, position);
                            position = readDir.Position;
                            stat = ZZT.Board.Stats[statId];
                            if (!ZZT.ElementDefs[ZZT.Board.Tiles[stat.X + readDir.DeltaX][stat.Y + readDir.DeltaY].Element].Walkable) {
                                ZZT.ElementPushablePush(stat.X + readDir.DeltaX, stat.Y + readDir.DeltaY, readDir.DeltaX, readDir.DeltaY);
                            }
                            if (ZZT.ElementDefs[ZZT.Board.Tiles[stat.X + readDir.DeltaX][stat.Y + readDir.DeltaY].Element].Walkable) {
                                ZZT.MoveStat(statId, stat.X + readDir.DeltaX, stat.Y + readDir.DeltaY);
                            }
                            else {
                                repeatInsNextTick = true;
                            }
                            stopRunning = true;
                        }
                        else if (ZZT.OopWord === "TRY") {
                            readDir = oopReadDirection(statId, position);
                            position = readDir.Position;
                            stat = ZZT.Board.Stats[statId];
                            if (!ZZT.ElementDefs[ZZT.Board.Tiles[stat.X + readDir.DeltaX][stat.Y + readDir.DeltaY].Element].Walkable) {
                                ZZT.ElementPushablePush(stat.X + readDir.DeltaX, stat.Y + readDir.DeltaY, readDir.DeltaX, readDir.DeltaY);
                            }
                            if (ZZT.ElementDefs[ZZT.Board.Tiles[stat.X + readDir.DeltaX][stat.Y + readDir.DeltaY].Element].Walkable) {
                                ZZT.MoveStat(statId, stat.X + readDir.DeltaX, stat.Y + readDir.DeltaY);
                                stopRunning = true;
                            }
                            else {
                                commandLoop = true;
                            }
                        }
                        else if (ZZT.OopWord === "WALK") {
                            readDir = oopReadDirection(statId, position);
                            position = readDir.Position;
                            ZZT.Board.Stats[statId].StepX = readDir.DeltaX;
                            ZZT.Board.Stats[statId].StepY = readDir.DeltaY;
                        }
                        else if (ZZT.OopWord === "SET") {
                            position = oopReadWord(statId, position);
                            WorldSetFlag(ZZT.OopWord);
                        }
                        else if (ZZT.OopWord === "CLEAR") {
                            position = oopReadWord(statId, position);
                            WorldClearFlag(ZZT.OopWord);
                        }
                        else if (ZZT.OopWord === "IF") {
                            position = oopReadWord(statId, position);
                            conditionResult = oopCheckCondition(statId, position);
                            position = conditionResult.Position;
                            if (conditionResult.Result) {
                                commandLoop = true;
                            }
                        }
                        else if (ZZT.OopWord === "SHOOT") {
                            readDir = oopReadDirection(statId, position);
                            position = readDir.Position;
                            stat = ZZT.Board.Stats[statId];
                            if (ZZT.BoardShoot(ZZT.E_BULLET, stat.X, stat.Y, readDir.DeltaX, readDir.DeltaY, ZZT.SHOT_SOURCE_ENEMY)) {
                                ZZT.SoundQueue(2, "\x30\x01\x26\x01");
                            }
                            stopRunning = true;
                        }
                        else if (ZZT.OopWord === "THROWSTAR") {
                            readDir = oopReadDirection(statId, position);
                            position = readDir.Position;
                            stat = ZZT.Board.Stats[statId];
                            ZZT.BoardShoot(ZZT.E_STAR, stat.X, stat.Y, readDir.DeltaX, readDir.DeltaY, ZZT.SHOT_SOURCE_ENEMY);
                            stopRunning = true;
                        }
                        else if (ZZT.OopWord === "GIVE" || ZZT.OopWord === "TAKE") {
                            deltaValue = 1;
                            if (ZZT.OopWord === "TAKE") {
                                deltaValue = -1;
                            }
                            position = oopReadWord(statId, position);
                            targetCounter = ZZT.OopWord;
                            currentValue = oopFindCounterValue(targetCounter);
                            if (currentValue !== null) {
                                position = oopReadValue(statId, position);
                                if (ZZT.OopValue > 0) {
                                    if ((currentValue + (ZZT.OopValue * deltaValue)) >= 0) {
                                        oopSetCounterValue(targetCounter, currentValue + (ZZT.OopValue * deltaValue));
                                    }
                                    else {
                                        commandLoop = true;
                                    }
                                }
                            }
                            ZZT.GameUpdateSidebar();
                        }
                        else if (ZZT.OopWord === "END") {
                            position = -1;
                            ZZT.OopChar = OOP_NUL;
                        }
                        else if (ZZT.OopWord === "ENDGAME") {
                            ZZT.World.Info.Health = 0;
                        }
                        else if (ZZT.OopWord === "IDLE") {
                            stopRunning = true;
                        }
                        else if (ZZT.OopWord === "RESTART") {
                            position = 0;
                            lineFinished = false;
                        }
                        else if (ZZT.OopWord === "ZAP") {
                            position = oopReadWord(statId, position);
                            labelState = {
                                StatId: 0,
                                DataPos: -1
                            };
                            while (oopFindLabel(statId, ZZT.OopWord, labelState, OOP_CR + ":")) {
                                oopSetDataChar(labelState.StatId, labelState.DataPos + 1, "'");
                            }
                        }
                        else if (ZZT.OopWord === "RESTORE") {
                            position = oopReadWord(statId, position);
                            labelState = {
                                StatId: 0,
                                DataPos: -1
                            };
                            while (oopFindLabel(statId, ZZT.OopWord, labelState, OOP_CR + "'")) {
                                oopSetDataChar(labelState.StatId, labelState.DataPos + 1, ":");
                            }
                        }
                        else if (ZZT.OopWord === "LOCK") {
                            ZZT.Board.Stats[statId].P2 = 1;
                        }
                        else if (ZZT.OopWord === "UNLOCK") {
                            ZZT.Board.Stats[statId].P2 = 0;
                        }
                        else if (ZZT.OopWord === "SEND") {
                            position = oopReadWord(statId, position);
                            if (OopSend(statId, ZZT.OopWord, false)) {
                                lineFinished = false;
                                if (statId >= 0 && statId <= ZZT.Board.StatCount) {
                                    position = ZZT.Board.Stats[statId].DataPos;
                                }
                            }
                        }
                        else if (ZZT.OopWord === "BECOME") {
                            parseTile = oopParseTile(statId, position);
                            position = parseTile.Position;
                            if (parseTile.Success) {
                                replaceStat = true;
                                replaceTile = parseTile.Tile;
                            }
                            else {
                                oopError(statId, "Bad #BECOME");
                            }
                        }
                        else if (ZZT.OopWord === "PUT") {
                            readDir = oopReadDirection(statId, position);
                            position = readDir.Position;
                            parseTile = oopParseTile(statId, position);
                            position = parseTile.Position;
                            stat = ZZT.Board.Stats[statId];
                            if ((readDir.DeltaX === 0 && readDir.DeltaY === 0) || !parseTile.Success) {
                                oopError(statId, "Bad #PUT");
                            }
                            else if ((stat.X + readDir.DeltaX) > 0 &&
                                (stat.X + readDir.DeltaX) <= ZZT.BOARD_WIDTH &&
                                (stat.Y + readDir.DeltaY) > 0 &&
                                (stat.Y + readDir.DeltaY) <= ZZT.BOARD_HEIGHT) {
                                tx = stat.X + readDir.DeltaX;
                                ty = stat.Y + readDir.DeltaY;
                                if (!ZZT.ElementDefs[ZZT.Board.Tiles[tx][ty].Element].Walkable) {
                                    ZZT.ElementPushablePush(tx, ty, readDir.DeltaX, readDir.DeltaY);
                                }
                                oopPlaceTile(tx, ty, parseTile.Tile);
                            }
                        }
                        else if (ZZT.OopWord === "CHANGE") {
                            parseTile = oopParseTile(statId, position);
                            position = parseTile.Position;
                            parseTile2 = oopParseTile(statId, position);
                            position = parseTile2.Position;
                            if (!parseTile.Success || !parseTile2.Success) {
                                oopError(statId, "Bad #CHANGE");
                            }
                            else {
                                if (parseTile2.Tile.Color === 0 && ZZT.ElementDefs[parseTile2.Tile.Element].Color < ZZT.COLOR_SPECIAL_MIN) {
                                    parseTile2.Tile.Color = ZZT.ElementDefs[parseTile2.Tile.Element].Color;
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
                        }
                        else if (ZZT.OopWord === "PLAY") {
                            lineData = oopReadLineToEnd(statId, position);
                            position = lineData.Position;
                            textLine = ZZT.SoundParse(lineData.Line);
                            if (textLine.length > 0) {
                                ZZT.SoundQueue(-1, textLine);
                            }
                            lineFinished = false;
                        }
                        else if (ZZT.OopWord === "CYCLE") {
                            position = oopReadValue(statId, position);
                            if (ZZT.OopValue > 0) {
                                ZZT.Board.Stats[statId].Cycle = ZZT.OopValue;
                            }
                        }
                        else if (ZZT.OopWord === "CHAR") {
                            position = oopReadValue(statId, position);
                            if (ZZT.OopValue > 0 && ZZT.OopValue <= 255) {
                                ZZT.Board.Stats[statId].P1 = ZZT.OopValue;
                                ZZT.BoardDrawTile(ZZT.Board.Stats[statId].X, ZZT.Board.Stats[statId].Y);
                            }
                        }
                        else if (ZZT.OopWord === "DIE") {
                            replaceStat = true;
                            replaceTile = ZZT.createTile(ZZT.E_EMPTY, 0x0f);
                        }
                        else if (ZZT.OopWord === "BIND") {
                            position = oopReadWord(statId, position);
                            bindStat = oopIterateStat(statId, 0, ZZT.OopWord);
                            if (bindStat.Found) {
                                ZZT.Board.Stats[statId].Data = ZZT.Board.Stats[bindStat.StatId].Data;
                                ZZT.Board.Stats[statId].DataLen = ZZT.Board.Stats[bindStat.StatId].DataLen;
                                position = 0;
                            }
                        }
                        else {
                            textLine = ZZT.OopWord;
                            if (OopSend(statId, textLine, false)) {
                                lineFinished = false;
                                if (statId >= 0 && statId <= ZZT.Board.StatCount) {
                                    position = ZZT.Board.Stats[statId].DataPos;
                                }
                            }
                            else if (textLine.indexOf(":") < 0) {
                                oopError(statId, "Bad command " + textLine);
                            }
                        }
                    }
                    if (lineFinished) {
                        position = oopSkipLine(statId, position);
                    }
                    continue;
                }
                if (ZZT.OopChar === OOP_CR) {
                    if (textWindow.LineCount > 0) {
                        ZZT.TextWindowAppend(textWindow, "");
                    }
                }
                else if (ZZT.OopChar === OOP_NUL) {
                    endOfProgram = true;
                }
                else {
                    lineFirstChar = ZZT.OopChar;
                    lineData = oopReadLineToEnd(statId, position);
                    position = lineData.Position;
                    oopAppendDisplayLine(textWindow, lineFirstChar + lineData.Line);
                }
            }
            if (repeatInsNextTick) {
                position = lastPosition;
            }
            if (ZZT.OopChar === OOP_NUL) {
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
                ZZT.TextWindowDrawOpen(textWindow);
                ZZT.TextWindowSelect(textWindow, true, false);
                ZZT.TextWindowDrawClose(textWindow);
                if (textWindow.Hyperlink.length > 0) {
                    if (OopSend(statId, textWindow.Hyperlink, false)) {
                        if (statId >= 0 && statId <= ZZT.Board.StatCount) {
                            position = ZZT.Board.Stats[statId].DataPos;
                        }
                        restartParsing = true;
                    }
                }
                ZZT.TextWindowFree(textWindow);
            }
            else if (textWindow.LineCount === 1) {
                ZZT.DisplayMessage(200, textWindow.Lines[0]);
                ZZT.TextWindowFree(textWindow);
            }
            if (replaceStat && statId >= 0 && statId <= ZZT.Board.StatCount) {
                ix = ZZT.Board.Stats[statId].X;
                iy = ZZT.Board.Stats[statId].Y;
                ZZT.DamageStat(statId);
                oopPlaceTile(ix, iy, replaceTile);
            }
        }
        return position;
    }
    ZZT.OopExecute = OopExecute;
})(ZZT || (ZZT = {}));
var ZZT;
(function (ZZT) {
    var COLOR_NAMES = ["", "Blue", "Green", "Cyan", "Red", "Purple", "Yellow", "White"];
    var DIAGONAL_DELTA_X = [-1, 0, 1, 1, 1, 0, -1, -1];
    var DIAGONAL_DELTA_Y = [1, 1, 1, 0, -1, -1, -1, 0];
    var NEIGHBOR_DELTA_X = [0, 0, -1, 1];
    var NEIGHBOR_DELTA_Y = [-1, 1, 0, 0];
    var TRANSPORTER_NS_CHARS = "^~^-v_v-";
    var TRANSPORTER_EW_CHARS = "(<(" + String.fromCharCode(179) + ")>)" + String.fromCharCode(179);
    function randomInt(maxExclusive) {
        if (maxExclusive <= 0) {
            return 0;
        }
        return Math.floor(Math.random() * maxExclusive);
    }
    function clamp(value, min, max) {
        if (value < min) {
            return min;
        }
        if (value > max) {
            return max;
        }
        return value;
    }
    function elementDefaultTick(_statId) {
    }
    function elementDefaultTouch(_x, _y, _sourceStatId, _context) {
    }
    function elementDefaultDraw(_x, _y) {
        return "?".charCodeAt(0);
    }
    function elementMessageTimerTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        if (stat.X !== 0) {
            return;
        }
        if (stat.P2 > 0) {
            if (ZZT.Board.Info.Message.length > 0) {
                ZZT.VideoWriteText(Math.floor((60 - ZZT.Board.Info.Message.length) / 2), 24, 0x09 + (stat.P2 % 7), " " + ZZT.Board.Info.Message + " ");
            }
            stat.P2 -= 1;
        }
        if (stat.P2 <= 0) {
            ZZT.RemoveStat(statId);
            ZZT.CurrentStatTicked -= 1;
            ZZT.BoardDrawBorder();
            ZZT.Board.Info.Message = "";
        }
    }
    function elementDamagingTouch(x, y, sourceStatId, _context) {
        ZZT.BoardAttack(sourceStatId, x, y);
    }
    function elementLionTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var dir;
        var tx;
        var ty;
        var destElement;
        if (stat.P1 < randomInt(10)) {
            dir = ZZT.CalcDirectionRnd();
        }
        else {
            dir = ZZT.CalcDirectionSeek(stat.X, stat.Y);
        }
        tx = stat.X + dir.DeltaX;
        ty = stat.Y + dir.DeltaY;
        destElement = ZZT.Board.Tiles[tx][ty].Element;
        if (ZZT.ElementDefs[destElement].Walkable) {
            ZZT.MoveStat(statId, tx, ty);
        }
        else if (destElement === ZZT.E_PLAYER) {
            ZZT.BoardAttack(statId, tx, ty);
        }
    }
    function elementTigerTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var shot = false;
        var bulletElement = stat.P2 >= 0x80 ? ZZT.E_STAR : ZZT.E_BULLET;
        if ((randomInt(10) * 3) <= (stat.P2 % 0x80)) {
            if (ZZT.Difference(stat.X, ZZT.Board.Stats[0].X) <= 2) {
                shot = ZZT.BoardShoot(bulletElement, stat.X, stat.Y, 0, ZZT.Signum(ZZT.Board.Stats[0].Y - stat.Y), ZZT.SHOT_SOURCE_ENEMY);
            }
            if (!shot && ZZT.Difference(stat.Y, ZZT.Board.Stats[0].Y) <= 2) {
                ZZT.BoardShoot(bulletElement, stat.X, stat.Y, ZZT.Signum(ZZT.Board.Stats[0].X - stat.X), 0, ZZT.SHOT_SOURCE_ENEMY);
            }
        }
        elementLionTick(statId);
    }
    function elementRuffianTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var dir;
        var tx;
        var ty;
        var destElement;
        if (stat.StepX === 0 && stat.StepY === 0) {
            if ((stat.P2 + 8) <= randomInt(17)) {
                if (stat.P1 >= randomInt(9)) {
                    dir = ZZT.CalcDirectionSeek(stat.X, stat.Y);
                }
                else {
                    dir = ZZT.CalcDirectionRnd();
                }
                stat.StepX = dir.DeltaX;
                stat.StepY = dir.DeltaY;
            }
            return;
        }
        if ((stat.Y === ZZT.Board.Stats[0].Y || stat.X === ZZT.Board.Stats[0].X) && randomInt(9) <= stat.P1) {
            dir = ZZT.CalcDirectionSeek(stat.X, stat.Y);
            stat.StepX = dir.DeltaX;
            stat.StepY = dir.DeltaY;
        }
        tx = stat.X + stat.StepX;
        ty = stat.Y + stat.StepY;
        destElement = ZZT.Board.Tiles[tx][ty].Element;
        if (destElement === ZZT.E_PLAYER) {
            ZZT.BoardAttack(statId, tx, ty);
        }
        else if (ZZT.ElementDefs[destElement].Walkable) {
            ZZT.MoveStat(statId, tx, ty);
            if ((stat.P2 + 8) <= randomInt(17)) {
                stat.StepX = 0;
                stat.StepY = 0;
            }
        }
        else {
            stat.StepX = 0;
            stat.StepY = 0;
        }
    }
    function elementBearTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var deltaX = 0;
        var deltaY = 0;
        var tx;
        var ty;
        var destElement;
        if (stat.X !== ZZT.Board.Stats[0].X && ZZT.Difference(stat.Y, ZZT.Board.Stats[0].Y) <= (8 - stat.P1)) {
            deltaX = ZZT.Signum(ZZT.Board.Stats[0].X - stat.X);
        }
        else if (ZZT.Difference(stat.X, ZZT.Board.Stats[0].X) <= (8 - stat.P1)) {
            deltaY = ZZT.Signum(ZZT.Board.Stats[0].Y - stat.Y);
        }
        tx = stat.X + deltaX;
        ty = stat.Y + deltaY;
        destElement = ZZT.Board.Tiles[tx][ty].Element;
        if (ZZT.ElementDefs[destElement].Walkable) {
            ZZT.MoveStat(statId, tx, ty);
        }
        else if (destElement === ZZT.E_PLAYER || destElement === ZZT.E_BREAKABLE) {
            ZZT.BoardAttack(statId, tx, ty);
        }
    }
    function elementSharkTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var dir;
        var tx;
        var ty;
        var destElement;
        if (stat.P1 < randomInt(10)) {
            dir = ZZT.CalcDirectionRnd();
        }
        else {
            dir = ZZT.CalcDirectionSeek(stat.X, stat.Y);
        }
        tx = stat.X + dir.DeltaX;
        ty = stat.Y + dir.DeltaY;
        destElement = ZZT.Board.Tiles[tx][ty].Element;
        if (destElement === ZZT.E_WATER) {
            ZZT.MoveStat(statId, tx, ty);
        }
        else if (destElement === ZZT.E_PLAYER) {
            ZZT.BoardAttack(statId, tx, ty);
        }
    }
    function elementBulletTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var tx = stat.X + stat.StepX;
        var ty = stat.Y + stat.StepY;
        var element = ZZT.Board.Tiles[tx][ty].Element;
        var targetStatId = -1;
        if (ZZT.ElementDefs[element].Walkable || element === ZZT.E_WATER) {
            ZZT.MoveStat(statId, tx, ty);
            return;
        }
        if (element === ZZT.E_BREAKABLE || (ZZT.ElementDefs[element].Destructible && (element === ZZT.E_PLAYER || stat.P1 === ZZT.SHOT_SOURCE_PLAYER))) {
            ZZT.BoardAttack(statId, tx, ty);
            return;
        }
        if (element === ZZT.E_OBJECT || element === ZZT.E_SCROLL) {
            targetStatId = ZZT.GetStatIdAt(tx, ty);
        }
        ZZT.RemoveStat(statId);
        if (targetStatId > 0) {
            ZZT.OopSend(-targetStatId, "SHOT", false);
        }
    }
    function elementSpinningGunDraw(_x, _y) {
        switch (ZZT.CurrentTick % 8) {
            case 0:
            case 1:
                return 24;
            case 2:
            case 3:
                return 26;
            case 4:
            case 5:
                return 25;
            default:
                return 27;
        }
    }
    function elementSpinningGunTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var bulletElement = stat.P2 >= 0x80 ? ZZT.E_STAR : ZZT.E_BULLET;
        var dir;
        var shot = false;
        if (randomInt(9) >= (stat.P2 % 0x80)) {
            return;
        }
        if (randomInt(9) <= stat.P1) {
            if (ZZT.Difference(stat.X, ZZT.Board.Stats[0].X) <= 2) {
                shot = ZZT.BoardShoot(bulletElement, stat.X, stat.Y, 0, ZZT.Signum(ZZT.Board.Stats[0].Y - stat.Y), ZZT.SHOT_SOURCE_ENEMY);
            }
            if (!shot && ZZT.Difference(stat.Y, ZZT.Board.Stats[0].Y) <= 2) {
                ZZT.BoardShoot(bulletElement, stat.X, stat.Y, ZZT.Signum(ZZT.Board.Stats[0].X - stat.X), 0, ZZT.SHOT_SOURCE_ENEMY);
            }
        }
        else {
            dir = ZZT.CalcDirectionRnd();
            ZZT.BoardShoot(bulletElement, stat.X, stat.Y, dir.DeltaX, dir.DeltaY, ZZT.SHOT_SOURCE_ENEMY);
        }
    }
    function elementLineDraw(x, y) {
        var lineChars = String.fromCharCode(249) +
            String.fromCharCode(208) +
            String.fromCharCode(210) +
            String.fromCharCode(186) +
            String.fromCharCode(181) +
            String.fromCharCode(188) +
            String.fromCharCode(187) +
            String.fromCharCode(185) +
            String.fromCharCode(198) +
            String.fromCharCode(200) +
            String.fromCharCode(201) +
            String.fromCharCode(204) +
            String.fromCharCode(205) +
            String.fromCharCode(202) +
            String.fromCharCode(203) +
            String.fromCharCode(206);
        var i;
        var v = 1;
        var shift = 1;
        var neighbor;
        for (i = 0; i < 4; i += 1) {
            neighbor = ZZT.Board.Tiles[x + NEIGHBOR_DELTA_X[i]][y + NEIGHBOR_DELTA_Y[i]].Element;
            if (neighbor === ZZT.E_LINE || neighbor === ZZT.E_BOARD_EDGE) {
                v += shift;
            }
            shift <<= 1;
        }
        return lineChars.charCodeAt(clamp(v - 1, 0, lineChars.length - 1));
    }
    function elementConveyorTickGeneric(x, y, direction) {
        var i;
        var iStat;
        var ix;
        var iy;
        var canMove;
        var tiles = [];
        var iMin;
        var iMax;
        var tmpTile;
        var elem;
        if (direction === 1) {
            iMin = 0;
            iMax = 8;
        }
        else {
            iMin = 7;
            iMax = -1;
        }
        canMove = true;
        i = iMin;
        while (i !== iMax) {
            tiles[i] = ZZT.cloneTile(ZZT.Board.Tiles[x + DIAGONAL_DELTA_X[i]][y + DIAGONAL_DELTA_Y[i]]);
            elem = tiles[i].Element;
            if (elem === ZZT.E_EMPTY) {
                canMove = true;
            }
            else if (!ZZT.ElementDefs[elem].Pushable) {
                canMove = false;
            }
            i += direction;
        }
        i = iMin;
        while (i !== iMax) {
            elem = tiles[i].Element;
            if (canMove) {
                if (ZZT.ElementDefs[elem].Pushable) {
                    ix = x + DIAGONAL_DELTA_X[(i - direction + 8) % 8];
                    iy = y + DIAGONAL_DELTA_Y[(i - direction + 8) % 8];
                    if (ZZT.ElementDefs[elem].Cycle > -1) {
                        tmpTile = ZZT.cloneTile(ZZT.Board.Tiles[x + DIAGONAL_DELTA_X[i]][y + DIAGONAL_DELTA_Y[i]]);
                        iStat = ZZT.GetStatIdAt(x + DIAGONAL_DELTA_X[i], y + DIAGONAL_DELTA_Y[i]);
                        ZZT.Board.Tiles[x + DIAGONAL_DELTA_X[i]][y + DIAGONAL_DELTA_Y[i]] = ZZT.cloneTile(tiles[i]);
                        ZZT.Board.Tiles[ix][iy].Element = ZZT.E_EMPTY;
                        if (iStat >= 0) {
                            ZZT.MoveStat(iStat, ix, iy);
                        }
                        ZZT.Board.Tiles[x + DIAGONAL_DELTA_X[i]][y + DIAGONAL_DELTA_Y[i]] = tmpTile;
                    }
                    else {
                        ZZT.Board.Tiles[ix][iy] = ZZT.cloneTile(tiles[i]);
                        ZZT.BoardDrawTile(ix, iy);
                    }
                    if (!ZZT.ElementDefs[tiles[(i + direction + 8) % 8].Element].Pushable) {
                        ZZT.Board.Tiles[x + DIAGONAL_DELTA_X[i]][y + DIAGONAL_DELTA_Y[i]].Element = ZZT.E_EMPTY;
                        ZZT.BoardDrawTile(x + DIAGONAL_DELTA_X[i], y + DIAGONAL_DELTA_Y[i]);
                    }
                }
                else {
                    canMove = false;
                }
            }
            else if (elem === ZZT.E_EMPTY) {
                canMove = true;
            }
            else if (!ZZT.ElementDefs[elem].Pushable) {
                canMove = false;
            }
            i += direction;
        }
    }
    function elementConveyorCWDraw(_x, _y) {
        switch (Math.floor(ZZT.CurrentTick / ZZT.ElementDefs[ZZT.E_CONVEYOR_CW].Cycle) % 4) {
            case 0:
                return 179;
            case 1:
                return 47;
            case 2:
                return 196;
            default:
                return 92;
        }
    }
    function elementConveyorCWTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        ZZT.BoardDrawTile(stat.X, stat.Y);
        elementConveyorTickGeneric(stat.X, stat.Y, 1);
    }
    function elementConveyorCCWDraw(_x, _y) {
        switch (Math.floor(ZZT.CurrentTick / ZZT.ElementDefs[ZZT.E_CONVEYOR_CCW].Cycle) % 4) {
            case 3:
                return 179;
            case 2:
                return 47;
            case 1:
                return 196;
            default:
                return 92;
        }
    }
    function elementConveyorCCWTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        ZZT.BoardDrawTile(stat.X, stat.Y);
        elementConveyorTickGeneric(stat.X, stat.Y, -1);
    }
    function elementTransporterMove(x, y, deltaX, deltaY) {
        var statId = ZZT.GetStatIdAt(x + deltaX, y + deltaY);
        var ix;
        var iy;
        var newX = -1;
        var newY = -1;
        var finishSearch = false;
        var isValidDest = true;
        var elem;
        var scanStatId;
        var transStat;
        if (statId < 0) {
            return;
        }
        transStat = ZZT.Board.Stats[statId];
        if (deltaX !== transStat.StepX || deltaY !== transStat.StepY) {
            return;
        }
        ix = transStat.X;
        iy = transStat.Y;
        while (!finishSearch) {
            ix += deltaX;
            iy += deltaY;
            elem = ZZT.Board.Tiles[ix][iy].Element;
            if (elem === ZZT.E_BOARD_EDGE) {
                finishSearch = true;
            }
            else if (isValidDest) {
                isValidDest = false;
                if (!ZZT.ElementDefs[elem].Walkable) {
                    ZZT.ElementPushablePush(ix, iy, deltaX, deltaY);
                    elem = ZZT.Board.Tiles[ix][iy].Element;
                }
                if (ZZT.ElementDefs[elem].Walkable) {
                    finishSearch = true;
                    newX = ix;
                    newY = iy;
                }
                else {
                    newX = -1;
                }
            }
            if (elem === ZZT.E_TRANSPORTER) {
                scanStatId = ZZT.GetStatIdAt(ix, iy);
                if (scanStatId >= 0) {
                    if (ZZT.Board.Stats[scanStatId].StepX === -deltaX && ZZT.Board.Stats[scanStatId].StepY === -deltaY) {
                        isValidDest = true;
                    }
                }
            }
        }
        if (newX !== -1) {
            ZZT.ElementMove(transStat.X - deltaX, transStat.Y - deltaY, newX, newY);
            ZZT.SoundQueue(3, "\x30\x01\x42\x01\x34\x01\x46\x01\x38\x01\x4A\x01\x40\x01\x52\x01");
        }
    }
    function elementTransporterTouch(x, y, _sourceStatId, context) {
        elementTransporterMove(x - context.DeltaX, y - context.DeltaY, context.DeltaX, context.DeltaY);
        context.DeltaX = 0;
        context.DeltaY = 0;
    }
    function elementTransporterTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        ZZT.BoardDrawTile(stat.X, stat.Y);
    }
    function elementTransporterDraw(x, y) {
        var statId = ZZT.GetStatIdAt(x, y);
        var stat;
        var idx;
        if (statId < 0) {
            return 197;
        }
        stat = ZZT.Board.Stats[statId];
        if (stat.StepX === 0) {
            idx = stat.StepY * 2 + 3 + (Math.floor(ZZT.CurrentTick / Math.max(1, stat.Cycle)) % 4);
            return TRANSPORTER_NS_CHARS.charCodeAt(clamp(idx - 1, 0, TRANSPORTER_NS_CHARS.length - 1));
        }
        idx = stat.StepX * 2 + 3 + (Math.floor(ZZT.CurrentTick / Math.max(1, stat.Cycle)) % 4);
        return TRANSPORTER_EW_CHARS.charCodeAt(clamp(idx - 1, 0, TRANSPORTER_EW_CHARS.length - 1));
    }
    function elementPusherDraw(x, y) {
        var statId = ZZT.GetStatIdAt(x, y);
        var stat;
        if (statId < 0) {
            return 16;
        }
        stat = ZZT.Board.Stats[statId];
        if (stat.StepX === 1) {
            return 16;
        }
        if (stat.StepX === -1) {
            return 17;
        }
        if (stat.StepY === -1) {
            return 30;
        }
        return 31;
    }
    function elementPusherTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var startX = stat.X;
        var startY = stat.Y;
        var chainId;
        if (!ZZT.ElementDefs[ZZT.Board.Tiles[stat.X + stat.StepX][stat.Y + stat.StepY].Element].Walkable) {
            ZZT.ElementPushablePush(stat.X + stat.StepX, stat.Y + stat.StepY, stat.StepX, stat.StepY);
        }
        statId = ZZT.GetStatIdAt(startX, startY);
        if (statId < 0) {
            return;
        }
        stat = ZZT.Board.Stats[statId];
        if (ZZT.ElementDefs[ZZT.Board.Tiles[stat.X + stat.StepX][stat.Y + stat.StepY].Element].Walkable) {
            ZZT.MoveStat(statId, stat.X + stat.StepX, stat.Y + stat.StepY);
            ZZT.SoundQueue(2, "\x15\x01");
            chainId = ZZT.GetStatIdAt(stat.X - (stat.StepX * 2), stat.Y - (stat.StepY * 2));
            if (chainId >= 0 &&
                ZZT.Board.Tiles[ZZT.Board.Stats[chainId].X][ZZT.Board.Stats[chainId].Y].Element === ZZT.E_PUSHER &&
                ZZT.Board.Stats[chainId].StepX === stat.StepX &&
                ZZT.Board.Stats[chainId].StepY === stat.StepY) {
                elementPusherTick(chainId);
            }
        }
    }
    function elementSlimeTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var dir;
        var color;
        var changedTiles = 0;
        var startX;
        var startY;
        var nx;
        var ny;
        var newStat;
        if (stat.P1 < stat.P2) {
            stat.P1 += 1;
            return;
        }
        color = ZZT.Board.Tiles[stat.X][stat.Y].Color;
        stat.P1 = 0;
        startX = stat.X;
        startY = stat.Y;
        for (dir = 0; dir < 4; dir += 1) {
            nx = startX + NEIGHBOR_DELTA_X[dir];
            ny = startY + NEIGHBOR_DELTA_Y[dir];
            if (ZZT.ElementDefs[ZZT.Board.Tiles[nx][ny].Element].Walkable) {
                if (changedTiles === 0) {
                    ZZT.MoveStat(statId, nx, ny);
                    ZZT.Board.Tiles[startX][startY].Color = color;
                    ZZT.Board.Tiles[startX][startY].Element = ZZT.E_BREAKABLE;
                    ZZT.BoardDrawTile(startX, startY);
                }
                else {
                    newStat = ZZT.createDefaultStat();
                    newStat.P2 = stat.P2;
                    ZZT.AddStat(nx, ny, ZZT.E_SLIME, color, ZZT.ElementDefs[ZZT.E_SLIME].Cycle, newStat);
                    ZZT.Board.Stats[ZZT.Board.StatCount].P2 = stat.P2;
                }
                changedTiles += 1;
            }
        }
        if (changedTiles === 0) {
            ZZT.RemoveStat(statId);
            ZZT.Board.Tiles[startX][startY].Element = ZZT.E_BREAKABLE;
            ZZT.Board.Tiles[startX][startY].Color = color;
            ZZT.BoardDrawTile(startX, startY);
        }
    }
    function elementSlimeTouch(x, y, _sourceStatId, _context) {
        var color = ZZT.Board.Tiles[x][y].Color;
        var statId = ZZT.GetStatIdAt(x, y);
        if (statId >= 0) {
            ZZT.DamageStat(statId);
        }
        ZZT.Board.Tiles[x][y].Element = ZZT.E_BREAKABLE;
        ZZT.Board.Tiles[x][y].Color = color;
        ZZT.BoardDrawTile(x, y);
        ZZT.SoundQueue(2, "\x20\x01\x23\x01");
    }
    function elementBlinkWallDraw(_x, _y) {
        return 206;
    }
    function elementBlinkWallTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var ix;
        var iy;
        var rayElement;
        var elem;
        if (stat.P3 === 0) {
            stat.P3 = stat.P1 + 1;
        }
        if (stat.P3 !== 1) {
            stat.P3 -= 1;
            return;
        }
        ix = stat.X + stat.StepX;
        iy = stat.Y + stat.StepY;
        rayElement = stat.StepX !== 0 ? ZZT.E_BLINK_RAY_EW : ZZT.E_BLINK_RAY_NS;
        if (ZZT.Board.Tiles[ix][iy].Element === rayElement && ZZT.Board.Tiles[ix][iy].Color === ZZT.Board.Tiles[stat.X][stat.Y].Color) {
            while (ZZT.Board.Tiles[ix][iy].Element === rayElement && ZZT.Board.Tiles[ix][iy].Color === ZZT.Board.Tiles[stat.X][stat.Y].Color) {
                ZZT.Board.Tiles[ix][iy].Element = ZZT.E_EMPTY;
                ZZT.BoardDrawTile(ix, iy);
                ix += stat.StepX;
                iy += stat.StepY;
            }
            stat.P3 = (stat.P2 * 2) + 1;
            return;
        }
        while (true) {
            elem = ZZT.Board.Tiles[ix][iy].Element;
            if (elem === ZZT.E_BOARD_EDGE) {
                break;
            }
            if (elem === ZZT.E_PLAYER) {
                ZZT.BoardDamageTile(ix, iy);
            }
            if (!ZZT.ElementDefs[ZZT.Board.Tiles[ix][iy].Element].Walkable && ZZT.Board.Tiles[ix][iy].Element !== ZZT.E_EMPTY) {
                if (ZZT.ElementDefs[ZZT.Board.Tiles[ix][iy].Element].Destructible) {
                    ZZT.BoardDamageTile(ix, iy);
                }
                if (ZZT.Board.Tiles[ix][iy].Element !== ZZT.E_EMPTY) {
                    break;
                }
            }
            ZZT.Board.Tiles[ix][iy].Element = rayElement;
            ZZT.Board.Tiles[ix][iy].Color = ZZT.Board.Tiles[stat.X][stat.Y].Color;
            ZZT.BoardDrawTile(ix, iy);
            ix += stat.StepX;
            iy += stat.StepY;
        }
        stat.P3 = (stat.P2 * 2) + 1;
    }
    function elementDuplicatorDraw(x, y) {
        var statId = ZZT.GetStatIdAt(x, y);
        var p1;
        if (statId < 0) {
            return 250;
        }
        p1 = ZZT.Board.Stats[statId].P1;
        if (p1 === 1) {
            return 250;
        }
        if (p1 === 2) {
            return 249;
        }
        if (p1 === 3) {
            return 248;
        }
        if (p1 === 4) {
            return 111;
        }
        if (p1 === 5) {
            return 79;
        }
        return 250;
    }
    function elementDuplicatorTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var srcX;
        var srcY;
        var dstX;
        var dstY;
        var sourceStatId;
        var sourceStat;
        if (stat.P1 <= 4) {
            stat.P1 += 1;
            ZZT.BoardDrawTile(stat.X, stat.Y);
            stat.Cycle = (9 - stat.P2) * 3;
            return;
        }
        stat.P1 = 0;
        srcX = stat.X + stat.StepX;
        srcY = stat.Y + stat.StepY;
        dstX = stat.X - stat.StepX;
        dstY = stat.Y - stat.StepY;
        if (ZZT.Board.Tiles[dstX][dstY].Element !== ZZT.E_EMPTY) {
            ZZT.ElementPushablePush(dstX, dstY, -stat.StepX, -stat.StepY);
        }
        if (ZZT.Board.Tiles[dstX][dstY].Element === ZZT.E_EMPTY) {
            sourceStatId = ZZT.GetStatIdAt(srcX, srcY);
            if (sourceStatId > 0) {
                sourceStat = ZZT.cloneStat(ZZT.Board.Stats[sourceStatId]);
                ZZT.AddStat(dstX, dstY, ZZT.Board.Tiles[srcX][srcY].Element, ZZT.Board.Tiles[srcX][srcY].Color, ZZT.Board.Stats[sourceStatId].Cycle, sourceStat);
                ZZT.BoardDrawTile(dstX, dstY);
            }
            else if (sourceStatId !== 0) {
                ZZT.Board.Tiles[dstX][dstY] = ZZT.cloneTile(ZZT.Board.Tiles[srcX][srcY]);
                ZZT.BoardDrawTile(dstX, dstY);
            }
        }
        stat.Cycle = (9 - stat.P2) * 3;
        ZZT.BoardDrawTile(stat.X, stat.Y);
    }
    function elementObjectTick(statId) {
        var stat;
        var tx;
        var ty;
        if (statId < 0 || statId > ZZT.Board.StatCount) {
            return;
        }
        stat = ZZT.Board.Stats[statId];
        if (stat.DataPos >= 0) {
            stat.DataPos = ZZT.OopExecute(statId, stat.DataPos, "Interaction");
        }
        if (statId < 0 || statId > ZZT.Board.StatCount) {
            return;
        }
        stat = ZZT.Board.Stats[statId];
        if (stat.X < 0 || stat.X > ZZT.BOARD_WIDTH + 1 || stat.Y < 0 || stat.Y > ZZT.BOARD_HEIGHT + 1) {
            return;
        }
        if (ZZT.Board.Tiles[stat.X][stat.Y].Element !== ZZT.E_OBJECT) {
            return;
        }
        if (stat.StepX !== 0 || stat.StepY !== 0) {
            tx = stat.X + stat.StepX;
            ty = stat.Y + stat.StepY;
            if (tx < 0 || tx > ZZT.BOARD_WIDTH + 1 || ty < 0 || ty > ZZT.BOARD_HEIGHT + 1) {
                ZZT.OopSend(-statId, "THUD", false);
                return;
            }
            if (ZZT.ElementDefs[ZZT.Board.Tiles[tx][ty].Element].Walkable) {
                ZZT.MoveStat(statId, tx, ty);
            }
            else {
                ZZT.OopSend(-statId, "THUD", false);
            }
        }
    }
    function elementObjectDraw(x, y) {
        var statId = ZZT.GetStatIdAt(x, y);
        if (statId < 0) {
            return 2;
        }
        return ZZT.Board.Stats[statId].P1;
    }
    function elementObjectTouch(x, y, _sourceStatId, _context) {
        var statId = ZZT.GetStatIdAt(x, y);
        if (statId >= 0) {
            ZZT.OopSend(-statId, "TOUCH", false);
        }
    }
    function elementCentipedeHeadTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var oldX = stat.X;
        var oldY = stat.Y;
        var dir;
        var tx;
        var ty;
        var tryDirs = [];
        var i;
        var prevX;
        var prevY;
        var followerId;
        var follower;
        var nextX;
        var nextY;
        if (stat.X === ZZT.Board.Stats[0].X && randomInt(10) < stat.P1) {
            stat.StepY = ZZT.Signum(ZZT.Board.Stats[0].Y - stat.Y);
            stat.StepX = 0;
        }
        else if (stat.Y === ZZT.Board.Stats[0].Y && randomInt(10) < stat.P1) {
            stat.StepX = ZZT.Signum(ZZT.Board.Stats[0].X - stat.X);
            stat.StepY = 0;
        }
        else if ((randomInt(10) * 4) < stat.P2 || (stat.StepX === 0 && stat.StepY === 0)) {
            dir = ZZT.CalcDirectionRnd();
            stat.StepX = dir.DeltaX;
            stat.StepY = dir.DeltaY;
        }
        tryDirs.push({ DeltaX: stat.StepX, DeltaY: stat.StepY });
        tryDirs.push({ DeltaX: stat.StepY, DeltaY: -stat.StepX });
        tryDirs.push({ DeltaX: -stat.StepY, DeltaY: stat.StepX });
        tryDirs.push({ DeltaX: -stat.StepX, DeltaY: -stat.StepY });
        for (i = 0; i < tryDirs.length; i += 1) {
            tx = stat.X + tryDirs[i].DeltaX;
            ty = stat.Y + tryDirs[i].DeltaY;
            if (ZZT.ElementDefs[ZZT.Board.Tiles[tx][ty].Element].Walkable || ZZT.Board.Tiles[tx][ty].Element === ZZT.E_PLAYER) {
                stat.StepX = tryDirs[i].DeltaX;
                stat.StepY = tryDirs[i].DeltaY;
                break;
            }
        }
        tx = stat.X + stat.StepX;
        ty = stat.Y + stat.StepY;
        if (!(ZZT.ElementDefs[ZZT.Board.Tiles[tx][ty].Element].Walkable || ZZT.Board.Tiles[tx][ty].Element === ZZT.E_PLAYER)) {
            stat.StepX = 0;
            stat.StepY = 0;
            return;
        }
        if (ZZT.Board.Tiles[tx][ty].Element === ZZT.E_PLAYER) {
            ZZT.BoardAttack(statId, tx, ty);
            return;
        }
        ZZT.MoveStat(statId, tx, ty);
        prevX = oldX;
        prevY = oldY;
        followerId = stat.Follower;
        while (followerId > 0) {
            follower = ZZT.Board.Stats[followerId];
            nextX = follower.X;
            nextY = follower.Y;
            follower.StepX = prevX - follower.X;
            follower.StepY = prevY - follower.Y;
            ZZT.MoveStat(followerId, prevX, prevY);
            prevX = nextX;
            prevY = nextY;
            followerId = follower.Follower;
        }
    }
    function elementCentipedeSegmentTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        if (stat.Leader < 0) {
            if (stat.Leader < -1) {
                ZZT.Board.Tiles[stat.X][stat.Y].Element = ZZT.E_CENTIPEDE_HEAD;
            }
            else {
                stat.Leader -= 1;
            }
        }
    }
    function elementStarDraw(x, y) {
        var tile = ZZT.Board.Tiles[x][y];
        var chars = String.fromCharCode(179) + "/" + String.fromCharCode(196) + "\\";
        tile.Color += 1;
        if (tile.Color > 15) {
            tile.Color = 9;
        }
        return chars.charCodeAt(ZZT.CurrentTick % 4);
    }
    function elementStarTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var dir;
        var tx;
        var ty;
        var destElement;
        stat.P2 -= 1;
        if (stat.P2 <= 0) {
            ZZT.RemoveStat(statId);
            return;
        }
        if ((stat.P2 % 2) !== 0) {
            ZZT.BoardDrawTile(stat.X, stat.Y);
            return;
        }
        dir = ZZT.CalcDirectionSeek(stat.X, stat.Y);
        stat.StepX = dir.DeltaX;
        stat.StepY = dir.DeltaY;
        tx = stat.X + stat.StepX;
        ty = stat.Y + stat.StepY;
        destElement = ZZT.Board.Tiles[tx][ty].Element;
        if (destElement === ZZT.E_PLAYER || destElement === ZZT.E_BREAKABLE) {
            ZZT.BoardAttack(statId, tx, ty);
            return;
        }
        if (!ZZT.ElementDefs[destElement].Walkable) {
            ZZT.ElementPushablePush(tx, ty, stat.StepX, stat.StepY);
        }
        if (ZZT.ElementDefs[ZZT.Board.Tiles[tx][ty].Element].Walkable || ZZT.Board.Tiles[tx][ty].Element === ZZT.E_WATER) {
            ZZT.MoveStat(statId, tx, ty);
        }
    }
    function elementKeyTouch(x, y, _sourceStatId, _context) {
        var color = ZZT.Board.Tiles[x][y].Color;
        var key = color % 8;
        if (key === 0) {
            key = Math.floor((color / 16) % 16);
            if (key >= 8) {
                key = 0;
            }
        }
        if (key < 1 || key > 7) {
            return;
        }
        if (ZZT.World.Info.Keys[key]) {
            ZZT.DisplayMessage(160, "You already have a " + COLOR_NAMES[key] + " key!");
            ZZT.SoundQueue(2, "\x30\x02\x20\x02");
            return;
        }
        ZZT.World.Info.Keys[key] = true;
        ZZT.Board.Tiles[x][y].Element = ZZT.E_EMPTY;
        ZZT.GameUpdateSidebar();
        ZZT.DisplayMessage(160, "You now have the " + COLOR_NAMES[key] + " key.");
        ZZT.SoundQueue(2, "\x40\x01\x44\x01\x47\x01\x40\x01\x44\x01\x47\x01\x40\x01\x44\x01\x47\x01\x50\x02");
    }
    function elementAmmoTouch(x, y, _sourceStatId, _context) {
        ZZT.World.Info.Ammo += 5;
        ZZT.Board.Tiles[x][y].Element = ZZT.E_EMPTY;
        ZZT.GameUpdateSidebar();
        ZZT.SoundQueue(2, "\x30\x01\x31\x01\x32\x01");
        if (ZZT.MessageAmmoNotShown) {
            ZZT.MessageAmmoNotShown = false;
            ZZT.DisplayMessage(180, "Ammunition - 5 shots per container.");
        }
    }
    function elementGemTouch(x, y, _sourceStatId, _context) {
        ZZT.World.Info.Gems += 1;
        ZZT.World.Info.Health += 1;
        ZZT.World.Info.Score += 10;
        ZZT.Board.Tiles[x][y].Element = ZZT.E_EMPTY;
        ZZT.GameUpdateSidebar();
        ZZT.SoundQueue(2, "\x40\x01\x37\x01\x34\x01\x30\x01");
        if (ZZT.MessageGemNotShown) {
            ZZT.MessageGemNotShown = false;
            ZZT.DisplayMessage(180, "Gems give you Health!");
        }
    }
    function elementPassageTouch(x, y, _sourceStatId, context) {
        ZZT.BoardPassageTeleport(x, y);
        context.DeltaX = 0;
        context.DeltaY = 0;
    }
    function elementDoorTouch(x, y, _sourceStatId, _context) {
        var color = ZZT.Board.Tiles[x][y].Color;
        var key = Math.floor((color / 16) % 8);
        if (key === 0) {
            key = color % 16;
            if (key >= 8) {
                key = 0;
            }
        }
        if (key < 1 || key > 7) {
            return;
        }
        if (ZZT.World.Info.Keys[key]) {
            ZZT.Board.Tiles[x][y].Element = ZZT.E_EMPTY;
            ZZT.World.Info.Keys[key] = false;
            ZZT.GameUpdateSidebar();
            ZZT.DisplayMessage(180, "The " + COLOR_NAMES[key] + " door is now open.");
            ZZT.SoundQueue(3, "\x30\x01\x37\x01\x3B\x01\x30\x01\x37\x01\x3B\x01\x40\x04");
        }
        else {
            ZZT.DisplayMessage(180, "The " + COLOR_NAMES[key] + " door is locked!");
            ZZT.SoundQueue(3, "\x17\x01\x10\x01");
        }
    }
    function elementScrollTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var tile = ZZT.Board.Tiles[stat.X][stat.Y];
        tile.Color += 1;
        if (tile.Color > 0x0f) {
            tile.Color = 0x09;
        }
        ZZT.BoardDrawTile(stat.X, stat.Y);
    }
    function elementScrollTouch(x, y, _sourceStatId, _context) {
        var statId = ZZT.GetStatIdAt(x, y);
        ZZT.SoundQueue(2, ZZT.SoundParse("c-c+d-d+e-e+f-f+g-g"));
        if (statId >= 0) {
            ZZT.Board.Stats[statId].DataPos = 0;
            ZZT.Board.Stats[statId].DataPos = ZZT.OopExecute(statId, ZZT.Board.Stats[statId].DataPos, "Scroll");
        }
        statId = ZZT.GetStatIdAt(x, y);
        while (statId > 0) {
            ZZT.RemoveStat(statId);
            statId = ZZT.GetStatIdAt(x, y);
        }
        if (ZZT.Board.Tiles[x][y].Element === ZZT.E_SCROLL) {
            ZZT.Board.Tiles[x][y].Element = ZZT.E_EMPTY;
            ZZT.Board.Tiles[x][y].Color = 0;
        }
        // Match DOS behavior expectations: touched scroll should not persist visually.
        ZZT.BoardDrawTile(x, y);
    }
    function elementTorchTouch(x, y, _sourceStatId, _context) {
        ZZT.World.Info.Torches += 1;
        ZZT.Board.Tiles[x][y].Element = ZZT.E_EMPTY;
        ZZT.GameUpdateSidebar();
        if (ZZT.MessageTorchNotShown) {
            ZZT.MessageTorchNotShown = false;
            ZZT.DisplayMessage(180, "Torch - used for lighting in the underground.");
        }
        ZZT.SoundQueue(3, "\x30\x01\x39\x01\x34\x02");
    }
    function elementEnergizerTouch(x, y, _sourceStatId, _context) {
        ZZT.SoundQueue(9, "\x20\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03" +
            "\x30\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03" +
            "\x30\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03" +
            "\x30\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03" +
            "\x30\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03" +
            "\x30\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03" +
            "\x30\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03");
        ZZT.Board.Tiles[x][y].Element = ZZT.E_EMPTY;
        ZZT.World.Info.EnergizerTicks = 75;
        ZZT.GameUpdateSidebar();
        if (ZZT.MessageEnergizerNotShown) {
            ZZT.MessageEnergizerNotShown = false;
            ZZT.DisplayMessage(180, "Energizer - You are invincible");
        }
        ZZT.OopSend(0, "ALL:ENERGIZE", false);
    }
    function elementForestTouch(x, y, _sourceStatId, _context) {
        ZZT.Board.Tiles[x][y].Element = ZZT.E_EMPTY;
        ZZT.BoardDrawTile(x, y);
        ZZT.SoundQueue(3, "\x39\x01");
        if (ZZT.MessageForestNotShown) {
            ZZT.MessageForestNotShown = false;
            ZZT.DisplayMessage(180, "A path is cleared through the forest.");
        }
    }
    function elementFakeTouch(_x, _y, _sourceStatId, _context) {
        if (ZZT.MessageFakeNotShown) {
            ZZT.MessageFakeNotShown = false;
            ZZT.DisplayMessage(120, "A fake wall - secret passage!");
        }
    }
    function elementInvisibleTouch(x, y, _sourceStatId, _context) {
        ZZT.Board.Tiles[x][y].Element = ZZT.E_NORMAL;
        ZZT.BoardDrawTile(x, y);
        ZZT.SoundQueue(3, "\x12\x01\x10\x01");
        ZZT.DisplayMessage(120, "You are blocked by an invisible wall.");
    }
    function elementWaterTouch(_x, _y, _sourceStatId, _context) {
        ZZT.SoundQueue(3, "\x40\x01\x50\x01");
        ZZT.DisplayMessage(100, "Your way is blocked by water.");
    }
    function elementPushableTouch(x, y, _sourceStatId, context) {
        ZZT.ElementPushablePush(x, y, context.DeltaX, context.DeltaY);
        ZZT.SoundQueue(2, "\x15\x01");
    }
    function DrawPlayerSurroundings(x, y, bombPhase) {
        var ix;
        var iy;
        var statId;
        for (ix = (x - ZZT.TORCH_DX) - 1; ix <= (x + ZZT.TORCH_DX) + 1; ix += 1) {
            if (ix < 1 || ix > ZZT.BOARD_WIDTH) {
                continue;
            }
            for (iy = (y - ZZT.TORCH_DY) - 1; iy <= (y + ZZT.TORCH_DY) + 1; iy += 1) {
                if (iy < 1 || iy > ZZT.BOARD_HEIGHT) {
                    continue;
                }
                if (bombPhase > 0 && (((ix - x) * (ix - x)) + ((iy - y) * (iy - y) * 2) < ZZT.TORCH_DIST_SQR)) {
                    if (bombPhase === 1) {
                        if (ZZT.ElementDefs[ZZT.Board.Tiles[ix][iy].Element].ParamTextName.length > 0) {
                            statId = ZZT.GetStatIdAt(ix, iy);
                            if (statId > 0) {
                                ZZT.OopSend(-statId, "BOMBED", false);
                            }
                        }
                        if (ZZT.ElementDefs[ZZT.Board.Tiles[ix][iy].Element].Destructible || ZZT.Board.Tiles[ix][iy].Element === ZZT.E_STAR) {
                            ZZT.BoardDamageTile(ix, iy);
                        }
                        if (ZZT.Board.Tiles[ix][iy].Element === ZZT.E_EMPTY || ZZT.Board.Tiles[ix][iy].Element === ZZT.E_BREAKABLE) {
                            ZZT.Board.Tiles[ix][iy].Element = ZZT.E_BREAKABLE;
                            ZZT.Board.Tiles[ix][iy].Color = 0x09 + randomInt(7);
                        }
                    }
                    else if (bombPhase === 2) {
                        if (ZZT.Board.Tiles[ix][iy].Element === ZZT.E_BREAKABLE) {
                            ZZT.Board.Tiles[ix][iy].Element = ZZT.E_EMPTY;
                        }
                    }
                }
                ZZT.BoardDrawTile(ix, iy);
            }
        }
    }
    ZZT.DrawPlayerSurroundings = DrawPlayerSurroundings;
    function elementBombDraw(x, y) {
        var statId = ZZT.GetStatIdAt(x, y);
        if (statId < 0) {
            return 11;
        }
        if (ZZT.Board.Stats[statId].P1 <= 1) {
            return 11;
        }
        return 48 + ZZT.Board.Stats[statId].P1;
    }
    function elementBombTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var oldX;
        var oldY;
        if (stat.P1 <= 0) {
            return;
        }
        stat.P1 -= 1;
        ZZT.BoardDrawTile(stat.X, stat.Y);
        if (stat.P1 === 1) {
            ZZT.SoundQueue(1, "\x60\x01\x50\x01\x40\x01\x30\x01\x20\x01\x10\x01");
            DrawPlayerSurroundings(stat.X, stat.Y, 1);
        }
        else if (stat.P1 === 0) {
            oldX = stat.X;
            oldY = stat.Y;
            ZZT.RemoveStat(statId);
            DrawPlayerSurroundings(oldX, oldY, 2);
        }
        else {
            if ((stat.P1 % 2) === 0) {
                ZZT.SoundQueue(1, "\xF8\x01");
            }
            else {
                ZZT.SoundQueue(1, "\xF5\x01");
            }
        }
    }
    function elementBombTouch(x, y, _sourceStatId, context) {
        var statId = ZZT.GetStatIdAt(x, y);
        if (statId < 0) {
            return;
        }
        if (ZZT.Board.Stats[statId].P1 === 0) {
            ZZT.Board.Stats[statId].P1 = 9;
            ZZT.DisplayMessage(160, "Bomb activated!");
            ZZT.SoundQueue(4, "\x30\x01\x35\x01\x40\x01\x45\x01\x50\x01");
            ZZT.BoardDrawTile(x, y);
            return;
        }
        ZZT.ElementPushablePush(x, y, context.DeltaX, context.DeltaY);
    }
    function elementBoardEdgeTouch(_x, _y, sourceStatId, context) {
        var neighborId = 3;
        var boardId;
        var entryX = ZZT.Board.Stats[0].X;
        var entryY = ZZT.Board.Stats[0].Y;
        var touchContext;
        var entryElement;
        if (context.DeltaY === -1) {
            neighborId = 0;
            entryY = ZZT.BOARD_HEIGHT;
        }
        else if (context.DeltaY === 1) {
            neighborId = 1;
            entryY = 1;
        }
        else if (context.DeltaX === -1) {
            neighborId = 2;
            entryX = ZZT.BOARD_WIDTH;
        }
        else {
            neighborId = 3;
            entryX = 1;
        }
        if (ZZT.Board.Info.NeighborBoards[neighborId] === 0) {
            return;
        }
        boardId = ZZT.World.Info.CurrentBoard;
        ZZT.BoardChange(ZZT.Board.Info.NeighborBoards[neighborId]);
        entryElement = ZZT.Board.Tiles[entryX][entryY].Element;
        touchContext = {
            DeltaX: context.DeltaX,
            DeltaY: context.DeltaY
        };
        if (entryElement !== ZZT.E_PLAYER) {
            var entryTouch = ZZT.ElementDefs[entryElement].TouchProc;
            if (entryTouch !== null) {
                entryTouch(entryX, entryY, sourceStatId, touchContext);
            }
        }
        if (ZZT.ElementDefs[ZZT.Board.Tiles[entryX][entryY].Element].Walkable || ZZT.Board.Tiles[entryX][entryY].Element === ZZT.E_PLAYER) {
            if (ZZT.Board.Tiles[entryX][entryY].Element !== ZZT.E_PLAYER) {
                ZZT.MovePlayerStat(entryX, entryY);
            }
            ZZT.TransitionDrawBoardChange();
            context.DeltaX = 0;
            context.DeltaY = 0;
            ZZT.BoardEnter();
        }
        else {
            ZZT.BoardChange(boardId);
        }
    }
    function elementPlayerTick(statId) {
        var stat = ZZT.Board.Stats[statId];
        var touchContext;
        var desiredDeltaX;
        var desiredDeltaY;
        var shootX;
        var shootY;
        var bulletCount = 0;
        var i;
        var key = ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0) : String.fromCharCode(0);
        var fireRequested;
        var torchRequested;
        var fireDirX;
        var fireDirY;
        if (ZZT.World.Info.EnergizerTicks > 0) {
            if (ZZT.ElementDefs[ZZT.E_PLAYER].Character === String.fromCharCode(2)) {
                ZZT.ElementDefs[ZZT.E_PLAYER].Character = String.fromCharCode(1);
            }
            else {
                ZZT.ElementDefs[ZZT.E_PLAYER].Character = String.fromCharCode(2);
            }
            if ((ZZT.CurrentTick % 2) !== 0) {
                ZZT.Board.Tiles[stat.X][stat.Y].Color = 0x0f;
            }
            else {
                ZZT.Board.Tiles[stat.X][stat.Y].Color = (((ZZT.CurrentTick % 7) + 1) * 16) + 0x0f;
            }
            ZZT.BoardDrawTile(stat.X, stat.Y);
        }
        else if (ZZT.Board.Tiles[stat.X][stat.Y].Color !== 0x1f ||
            ZZT.ElementDefs[ZZT.E_PLAYER].Character !== String.fromCharCode(2)) {
            ZZT.Board.Tiles[stat.X][stat.Y].Color = 0x1f;
            ZZT.ElementDefs[ZZT.E_PLAYER].Character = String.fromCharCode(2);
            ZZT.BoardDrawTile(stat.X, stat.Y);
        }
        if (ZZT.World.Info.Health <= 0) {
            ZZT.InputDeltaX = 0;
            ZZT.InputDeltaY = 0;
            ZZT.InputShiftPressed = false;
            ZZT.InputFireDirX = 0;
            ZZT.InputFireDirY = 0;
            if (ZZT.GetStatIdAt(0, 0) === -1) {
                ZZT.DisplayMessage(32000, " Game over  -  Press ESCAPE");
            }
            ZZT.TickTimeDuration = 0;
            ZZT.SoundBlockQueueing = true;
        }
        fireDirX = ZZT.InputFireDirX;
        fireDirY = ZZT.InputFireDirY;
        fireRequested = ZZT.InputFirePressed || ZZT.InputShiftPressed || key === " " || fireDirX !== 0 || fireDirY !== 0;
        if (fireRequested) {
            // Space/shift fire should survive high-frequency input polling until consumed by player tick.
            ZZT.InputFirePressed = false;
            ZZT.InputFireDirX = 0;
            ZZT.InputFireDirY = 0;
            if (fireDirX !== 0 || fireDirY !== 0) {
                ZZT.PlayerDirX = fireDirX;
                ZZT.PlayerDirY = fireDirY;
            }
            else if (ZZT.InputShiftPressed && (ZZT.InputDeltaX !== 0 || ZZT.InputDeltaY !== 0)) {
                ZZT.PlayerDirX = ZZT.InputDeltaX;
                ZZT.PlayerDirY = ZZT.InputDeltaY;
            }
            shootX = ZZT.PlayerDirX;
            shootY = ZZT.PlayerDirY;
            if (shootX !== 0 || shootY !== 0) {
                if (ZZT.Board.Info.MaxShots <= 0) {
                    if (ZZT.MessageNoShootingNotShown) {
                        ZZT.MessageNoShootingNotShown = false;
                        ZZT.DisplayMessage(160, "Can't shoot in this place!");
                    }
                }
                else if (ZZT.World.Info.Ammo <= 0) {
                    if (ZZT.MessageOutOfAmmoNotShown) {
                        ZZT.MessageOutOfAmmoNotShown = false;
                        ZZT.DisplayMessage(160, "You don't have any ammo!");
                    }
                }
                else {
                    for (i = 0; i <= ZZT.Board.StatCount; i += 1) {
                        if (ZZT.Board.Tiles[ZZT.Board.Stats[i].X][ZZT.Board.Stats[i].Y].Element === ZZT.E_BULLET && ZZT.Board.Stats[i].P1 === ZZT.SHOT_SOURCE_PLAYER) {
                            bulletCount += 1;
                        }
                    }
                    if (bulletCount < ZZT.Board.Info.MaxShots && ZZT.BoardShoot(ZZT.E_BULLET, stat.X, stat.Y, shootX, shootY, ZZT.SHOT_SOURCE_PLAYER)) {
                        ZZT.World.Info.Ammo -= 1;
                        ZZT.GameUpdateSidebar();
                        ZZT.SoundQueue(2, "\x40\x01\x30\x01\x20\x01");
                        ZZT.InputDeltaX = 0;
                        ZZT.InputDeltaY = 0;
                    }
                }
            }
        }
        else if (ZZT.InputDeltaX !== 0 || ZZT.InputDeltaY !== 0) {
            desiredDeltaX = ZZT.InputDeltaX;
            desiredDeltaY = ZZT.InputDeltaY;
            ZZT.PlayerDirX = desiredDeltaX;
            ZZT.PlayerDirY = desiredDeltaY;
            touchContext = {
                DeltaX: desiredDeltaX,
                DeltaY: desiredDeltaY
            };
            var destTouch = ZZT.ElementDefs[ZZT.Board.Tiles[stat.X + desiredDeltaX][stat.Y + desiredDeltaY].Element].TouchProc;
            if (destTouch !== null) {
                destTouch(stat.X + desiredDeltaX, stat.Y + desiredDeltaY, 0, touchContext);
            }
            ZZT.InputDeltaX = touchContext.DeltaX;
            ZZT.InputDeltaY = touchContext.DeltaY;
            if ((ZZT.InputDeltaX !== 0 || ZZT.InputDeltaY !== 0) &&
                ZZT.ElementDefs[ZZT.Board.Tiles[stat.X + ZZT.InputDeltaX][stat.Y + ZZT.InputDeltaY].Element].Walkable) {
                ZZT.MovePlayerStat(stat.X + ZZT.InputDeltaX, stat.Y + ZZT.InputDeltaY);
            }
        }
        key = key.toUpperCase();
        torchRequested = ZZT.InputTorchPressed || key === "T";
        if (torchRequested) {
            ZZT.InputTorchPressed = false;
        }
        if (torchRequested && ZZT.World.Info.TorchTicks <= 0) {
            if (ZZT.World.Info.Torches > 0) {
                if (ZZT.Board.Info.IsDark) {
                    ZZT.World.Info.Torches -= 1;
                    ZZT.World.Info.TorchTicks = ZZT.TORCH_DURATION;
                    DrawPlayerSurroundings(stat.X, stat.Y, 0);
                    ZZT.GameUpdateSidebar();
                }
                else if (ZZT.MessageRoomNotDarkNotShown) {
                    ZZT.MessageRoomNotDarkNotShown = false;
                    ZZT.DisplayMessage(160, "Don't need torch - room is not dark!");
                }
            }
            else if (ZZT.MessageOutOfTorchesNotShown) {
                ZZT.MessageOutOfTorchesNotShown = false;
                ZZT.DisplayMessage(160, "You don't have any torches!");
            }
        }
        else if (key === ZZT.KEY_ESCAPE || key === "Q") {
            ZZT.GamePromptEndPlay();
        }
        else if (key === "S") {
            ZZT.GameWorldSave("Save game:", ZZT.SavedGameFileName, ".SAV");
        }
        else if (key === "P") {
            if (ZZT.World.Info.Health > 0) {
                ZZT.GamePaused = true;
            }
        }
        else if (key === "B") {
            ZZT.SoundEnabled = !ZZT.SoundEnabled;
            ZZT.SoundClearQueue();
            if (ZZT.SoundEnabled) {
                ZZT.SoundWorldMusicOnBoardChanged(ZZT.Board.Name);
            }
            ZZT.GameUpdateSidebar();
            ZZT.InputKeyPressed = " ";
        }
        else if (key === "H") {
            ZZT.TextWindowDisplayFile("GAME.HLP", "Playing ZZT");
        }
        else if (key === "F") {
            ZZT.TextWindowDisplayFile("ORDER.HLP", "Order form");
        }
        else if (key === "?") {
            ZZT.GameDebugPrompt();
            ZZT.InputKeyPressed = String.fromCharCode(0);
        }
        if (ZZT.World.Info.TorchTicks > 0) {
            ZZT.World.Info.TorchTicks -= 1;
            if (ZZT.World.Info.TorchTicks <= 0) {
                DrawPlayerSurroundings(stat.X, stat.Y, 0);
                ZZT.SoundQueue(3, "\x30\x01\x20\x01\x10\x01");
            }
            if ((ZZT.World.Info.TorchTicks % 40) === 0 || ZZT.World.Info.TorchTicks === 0) {
                ZZT.GameUpdateSidebar();
            }
        }
        if (ZZT.World.Info.EnergizerTicks > 0) {
            ZZT.World.Info.EnergizerTicks -= 1;
            if (ZZT.World.Info.EnergizerTicks === 10) {
                ZZT.SoundQueue(9, "\x20\x03\x1A\x03\x17\x03\x16\x03\x15\x03\x13\x03\x10\x03");
            }
            else if (ZZT.World.Info.EnergizerTicks <= 0) {
                ZZT.Board.Tiles[stat.X][stat.Y].Color = ZZT.ElementDefs[ZZT.E_PLAYER].Color;
                ZZT.BoardDrawTile(stat.X, stat.Y);
            }
        }
        if (ZZT.Board.Info.TimeLimitSec > 0 && ZZT.World.Info.Health > 0) {
            if (ZZT.WorldHasTimeElapsed(100)) {
                ZZT.World.Info.BoardTimeSec += 1;
                if ((ZZT.Board.Info.TimeLimitSec - 10) === ZZT.World.Info.BoardTimeSec) {
                    ZZT.DisplayMessage(160, "Running out of time!");
                    ZZT.SoundQueue(3, "\x40\x06\x45\x06\x40\x06\x35\x06\x40\x06\x45\x06\x40\x0A");
                }
                else if (ZZT.World.Info.BoardTimeSec > ZZT.Board.Info.TimeLimitSec) {
                    ZZT.DamageStat(0);
                }
                ZZT.GameUpdateSidebar();
            }
        }
    }
    function elementMonitorTick(_statId) {
        var key = ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0).toUpperCase() : String.fromCharCode(0);
        if (key === ZZT.KEY_ESCAPE || key === "A" || key === "E" || key === "H" || key === "N" ||
            key === "P" || key === "Q" || key === "R" || key === "S" || key === "W" || key === "|") {
            ZZT.GamePlayExitRequested = true;
        }
    }
    function initElementDefs() {
        var i;
        for (i = 0; i <= ZZT.MAX_ELEMENT; i += 1) {
            ZZT.ElementDefs[i] = ZZT.createElementDefDefault();
            ZZT.ElementDefs[i].TickProc = elementDefaultTick;
            ZZT.ElementDefs[i].DrawProc = elementDefaultDraw;
            ZZT.ElementDefs[i].TouchProc = elementDefaultTouch;
        }
        ZZT.ElementDefs[ZZT.E_EMPTY].Character = " ";
        ZZT.ElementDefs[ZZT.E_EMPTY].Color = 0x70;
        ZZT.ElementDefs[ZZT.E_EMPTY].Pushable = true;
        ZZT.ElementDefs[ZZT.E_EMPTY].Walkable = true;
        ZZT.ElementDefs[ZZT.E_EMPTY].Name = "Empty";
        ZZT.ElementDefs[ZZT.E_BOARD_EDGE].Character = " ";
        ZZT.ElementDefs[ZZT.E_BOARD_EDGE].Color = 0x00;
        ZZT.ElementDefs[ZZT.E_BOARD_EDGE].TouchProc = elementBoardEdgeTouch;
        ZZT.ElementDefs[ZZT.E_BOARD_EDGE].Name = "Board edge";
        ZZT.ElementDefs[ZZT.E_MONITOR].Character = " ";
        ZZT.ElementDefs[ZZT.E_MONITOR].Color = 0x07;
        ZZT.ElementDefs[ZZT.E_MONITOR].Cycle = 1;
        ZZT.ElementDefs[ZZT.E_MONITOR].TickProc = elementMonitorTick;
        ZZT.ElementDefs[ZZT.E_MONITOR].Name = "Monitor";
        ZZT.ElementDefs[ZZT.E_MESSAGE_TIMER].Cycle = 1;
        ZZT.ElementDefs[ZZT.E_MESSAGE_TIMER].TickProc = elementMessageTimerTick;
        ZZT.ElementDefs[ZZT.E_PLAYER].Character = String.fromCharCode(2);
        ZZT.ElementDefs[ZZT.E_PLAYER].Color = 0x1f;
        ZZT.ElementDefs[ZZT.E_PLAYER].Destructible = true;
        ZZT.ElementDefs[ZZT.E_PLAYER].Pushable = true;
        ZZT.ElementDefs[ZZT.E_PLAYER].VisibleInDark = true;
        ZZT.ElementDefs[ZZT.E_PLAYER].Cycle = 1;
        ZZT.ElementDefs[ZZT.E_PLAYER].TickProc = elementPlayerTick;
        ZZT.ElementDefs[ZZT.E_PLAYER].EditorCategory = ZZT.CATEGORY_ITEM;
        ZZT.ElementDefs[ZZT.E_PLAYER].EditorShortcut = "Z";
        ZZT.ElementDefs[ZZT.E_PLAYER].Name = "Player";
        ZZT.ElementDefs[ZZT.E_PLAYER].CategoryName = "Items:";
        ZZT.ElementDefs[ZZT.E_WATER].Character = String.fromCharCode(176);
        ZZT.ElementDefs[ZZT.E_WATER].Color = 0xf9;
        ZZT.ElementDefs[ZZT.E_WATER].PlaceableOnTop = true;
        ZZT.ElementDefs[ZZT.E_WATER].TouchProc = elementWaterTouch;
        ZZT.ElementDefs[ZZT.E_WATER].EditorCategory = ZZT.CATEGORY_TERRAIN;
        ZZT.ElementDefs[ZZT.E_WATER].EditorShortcut = "W";
        ZZT.ElementDefs[ZZT.E_WATER].Name = "Water";
        ZZT.ElementDefs[ZZT.E_WATER].CategoryName = "Terrains:";
        ZZT.ElementDefs[ZZT.E_FOREST].Character = String.fromCharCode(176);
        ZZT.ElementDefs[ZZT.E_FOREST].Color = 0x20;
        ZZT.ElementDefs[ZZT.E_FOREST].TouchProc = elementForestTouch;
        ZZT.ElementDefs[ZZT.E_FOREST].EditorCategory = ZZT.CATEGORY_TERRAIN;
        ZZT.ElementDefs[ZZT.E_FOREST].EditorShortcut = "F";
        ZZT.ElementDefs[ZZT.E_FOREST].Name = "Forest";
        ZZT.ElementDefs[ZZT.E_SOLID].Character = String.fromCharCode(219);
        ZZT.ElementDefs[ZZT.E_SOLID].EditorCategory = ZZT.CATEGORY_TERRAIN;
        ZZT.ElementDefs[ZZT.E_SOLID].CategoryName = "Walls:";
        ZZT.ElementDefs[ZZT.E_SOLID].EditorShortcut = "S";
        ZZT.ElementDefs[ZZT.E_SOLID].Name = "Solid";
        ZZT.ElementDefs[ZZT.E_NORMAL].Character = String.fromCharCode(178);
        ZZT.ElementDefs[ZZT.E_NORMAL].EditorCategory = ZZT.CATEGORY_TERRAIN;
        ZZT.ElementDefs[ZZT.E_NORMAL].EditorShortcut = "N";
        ZZT.ElementDefs[ZZT.E_NORMAL].Name = "Normal";
        ZZT.ElementDefs[ZZT.E_BREAKABLE].Character = String.fromCharCode(177);
        ZZT.ElementDefs[ZZT.E_BREAKABLE].EditorCategory = ZZT.CATEGORY_TERRAIN;
        ZZT.ElementDefs[ZZT.E_BREAKABLE].EditorShortcut = "B";
        ZZT.ElementDefs[ZZT.E_BREAKABLE].Name = "Breakable";
        ZZT.ElementDefs[ZZT.E_LINE].Character = String.fromCharCode(206);
        ZZT.ElementDefs[ZZT.E_LINE].Name = "Line";
        ZZT.ElementDefs[ZZT.E_BULLET].Character = String.fromCharCode(248);
        ZZT.ElementDefs[ZZT.E_BULLET].Color = 0x0f;
        ZZT.ElementDefs[ZZT.E_BULLET].Destructible = true;
        ZZT.ElementDefs[ZZT.E_BULLET].Cycle = 1;
        ZZT.ElementDefs[ZZT.E_BULLET].TickProc = elementBulletTick;
        ZZT.ElementDefs[ZZT.E_BULLET].TouchProc = elementDamagingTouch;
        ZZT.ElementDefs[ZZT.E_BULLET].Name = "Bullet";
        ZZT.ElementDefs[ZZT.E_STAR].Character = "S";
        ZZT.ElementDefs[ZZT.E_STAR].Color = 0x0f;
        ZZT.ElementDefs[ZZT.E_STAR].Cycle = 1;
        ZZT.ElementDefs[ZZT.E_STAR].TickProc = elementStarTick;
        ZZT.ElementDefs[ZZT.E_STAR].TouchProc = elementDamagingTouch;
        ZZT.ElementDefs[ZZT.E_STAR].HasDrawProc = true;
        ZZT.ElementDefs[ZZT.E_STAR].DrawProc = elementStarDraw;
        ZZT.ElementDefs[ZZT.E_STAR].Name = "Star";
        ZZT.ElementDefs[ZZT.E_DUPLICATOR].Character = String.fromCharCode(250);
        ZZT.ElementDefs[ZZT.E_DUPLICATOR].Color = 0x0f;
        ZZT.ElementDefs[ZZT.E_DUPLICATOR].Cycle = 2;
        ZZT.ElementDefs[ZZT.E_DUPLICATOR].TickProc = elementDuplicatorTick;
        ZZT.ElementDefs[ZZT.E_DUPLICATOR].HasDrawProc = true;
        ZZT.ElementDefs[ZZT.E_DUPLICATOR].DrawProc = elementDuplicatorDraw;
        ZZT.ElementDefs[ZZT.E_DUPLICATOR].EditorCategory = ZZT.CATEGORY_ITEM;
        ZZT.ElementDefs[ZZT.E_DUPLICATOR].EditorShortcut = "U";
        ZZT.ElementDefs[ZZT.E_DUPLICATOR].Name = "Duplicator";
        ZZT.ElementDefs[ZZT.E_DUPLICATOR].ParamDirName = "Source direction?";
        ZZT.ElementDefs[ZZT.E_DUPLICATOR].Param2Name = "Duplication rate?;SF";
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CW].Character = "/";
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CW].Cycle = 3;
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CW].HasDrawProc = true;
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CW].TickProc = elementConveyorCWTick;
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CW].DrawProc = elementConveyorCWDraw;
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CW].EditorCategory = ZZT.CATEGORY_ITEM;
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CW].EditorShortcut = "1";
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CW].Name = "Clockwise";
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CW].CategoryName = "Conveyors:";
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CCW].Character = "\\";
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CCW].Cycle = 2;
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CCW].HasDrawProc = true;
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CCW].TickProc = elementConveyorCCWTick;
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CCW].DrawProc = elementConveyorCCWDraw;
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CCW].EditorCategory = ZZT.CATEGORY_ITEM;
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CCW].EditorShortcut = "2";
        ZZT.ElementDefs[ZZT.E_CONVEYOR_CCW].Name = "Counter";
        ZZT.ElementDefs[ZZT.E_SPINNING_GUN].Character = String.fromCharCode(24);
        ZZT.ElementDefs[ZZT.E_SPINNING_GUN].Cycle = 2;
        ZZT.ElementDefs[ZZT.E_SPINNING_GUN].TickProc = elementSpinningGunTick;
        ZZT.ElementDefs[ZZT.E_SPINNING_GUN].HasDrawProc = true;
        ZZT.ElementDefs[ZZT.E_SPINNING_GUN].DrawProc = elementSpinningGunDraw;
        ZZT.ElementDefs[ZZT.E_SPINNING_GUN].EditorCategory = ZZT.CATEGORY_CREATURE;
        ZZT.ElementDefs[ZZT.E_SPINNING_GUN].EditorShortcut = "G";
        ZZT.ElementDefs[ZZT.E_SPINNING_GUN].Name = "Spinning gun";
        ZZT.ElementDefs[ZZT.E_SPINNING_GUN].Param1Name = "Intelligence?";
        ZZT.ElementDefs[ZZT.E_SPINNING_GUN].Param2Name = "Firing rate?";
        ZZT.ElementDefs[ZZT.E_SPINNING_GUN].ParamBulletTypeName = "Firing type?";
        ZZT.ElementDefs[ZZT.E_LION].Character = String.fromCharCode(234);
        ZZT.ElementDefs[ZZT.E_LION].Color = 0x0c;
        ZZT.ElementDefs[ZZT.E_LION].Destructible = true;
        ZZT.ElementDefs[ZZT.E_LION].Pushable = true;
        ZZT.ElementDefs[ZZT.E_LION].Cycle = 2;
        ZZT.ElementDefs[ZZT.E_LION].TickProc = elementLionTick;
        ZZT.ElementDefs[ZZT.E_LION].TouchProc = elementDamagingTouch;
        ZZT.ElementDefs[ZZT.E_LION].ScoreValue = 1;
        ZZT.ElementDefs[ZZT.E_LION].EditorCategory = ZZT.CATEGORY_CREATURE;
        ZZT.ElementDefs[ZZT.E_LION].EditorShortcut = "L";
        ZZT.ElementDefs[ZZT.E_LION].Name = "Lion";
        ZZT.ElementDefs[ZZT.E_LION].CategoryName = "Beasts:";
        ZZT.ElementDefs[ZZT.E_LION].Param1Name = "Intelligence?";
        ZZT.ElementDefs[ZZT.E_TIGER].Character = String.fromCharCode(227);
        ZZT.ElementDefs[ZZT.E_TIGER].Color = 0x0b;
        ZZT.ElementDefs[ZZT.E_TIGER].Destructible = true;
        ZZT.ElementDefs[ZZT.E_TIGER].Pushable = true;
        ZZT.ElementDefs[ZZT.E_TIGER].Cycle = 2;
        ZZT.ElementDefs[ZZT.E_TIGER].TickProc = elementTigerTick;
        ZZT.ElementDefs[ZZT.E_TIGER].TouchProc = elementDamagingTouch;
        ZZT.ElementDefs[ZZT.E_TIGER].ScoreValue = 2;
        ZZT.ElementDefs[ZZT.E_TIGER].EditorCategory = ZZT.CATEGORY_CREATURE;
        ZZT.ElementDefs[ZZT.E_TIGER].EditorShortcut = "T";
        ZZT.ElementDefs[ZZT.E_TIGER].Name = "Tiger";
        ZZT.ElementDefs[ZZT.E_TIGER].Param1Name = "Intelligence?";
        ZZT.ElementDefs[ZZT.E_TIGER].Param2Name = "Firing rate?";
        ZZT.ElementDefs[ZZT.E_TIGER].ParamBulletTypeName = "Firing type?";
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_HEAD].Character = String.fromCharCode(233);
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_HEAD].Destructible = true;
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_HEAD].Cycle = 2;
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_HEAD].TickProc = elementCentipedeHeadTick;
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_HEAD].TouchProc = elementDamagingTouch;
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_HEAD].ScoreValue = 1;
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_HEAD].EditorCategory = ZZT.CATEGORY_CREATURE;
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_HEAD].EditorShortcut = "H";
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_HEAD].Name = "Head";
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_HEAD].CategoryName = "Centipedes";
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_HEAD].Param1Name = "Intelligence?";
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_HEAD].Param2Name = "Deviance?";
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_SEGMENT].Character = "O";
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_SEGMENT].Destructible = true;
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_SEGMENT].Cycle = 2;
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_SEGMENT].TickProc = elementCentipedeSegmentTick;
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_SEGMENT].TouchProc = elementDamagingTouch;
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_SEGMENT].ScoreValue = 3;
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_SEGMENT].EditorCategory = ZZT.CATEGORY_CREATURE;
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_SEGMENT].EditorShortcut = "S";
        ZZT.ElementDefs[ZZT.E_CENTIPEDE_SEGMENT].Name = "Segment";
        ZZT.ElementDefs[ZZT.E_BEAR].Character = String.fromCharCode(153);
        ZZT.ElementDefs[ZZT.E_BEAR].Color = 0x06;
        ZZT.ElementDefs[ZZT.E_BEAR].Destructible = true;
        ZZT.ElementDefs[ZZT.E_BEAR].Pushable = true;
        ZZT.ElementDefs[ZZT.E_BEAR].Cycle = 3;
        ZZT.ElementDefs[ZZT.E_BEAR].TickProc = elementBearTick;
        ZZT.ElementDefs[ZZT.E_BEAR].TouchProc = elementDamagingTouch;
        ZZT.ElementDefs[ZZT.E_BEAR].ScoreValue = 1;
        ZZT.ElementDefs[ZZT.E_BEAR].EditorCategory = ZZT.CATEGORY_CREATURE;
        ZZT.ElementDefs[ZZT.E_BEAR].EditorShortcut = "B";
        ZZT.ElementDefs[ZZT.E_BEAR].Name = "Bear";
        ZZT.ElementDefs[ZZT.E_BEAR].CategoryName = "Creatures:";
        ZZT.ElementDefs[ZZT.E_BEAR].Param1Name = "Sensitivity?";
        ZZT.ElementDefs[ZZT.E_RUFFIAN].Character = String.fromCharCode(5);
        ZZT.ElementDefs[ZZT.E_RUFFIAN].Color = 0x0d;
        ZZT.ElementDefs[ZZT.E_RUFFIAN].Destructible = true;
        ZZT.ElementDefs[ZZT.E_RUFFIAN].Pushable = true;
        ZZT.ElementDefs[ZZT.E_RUFFIAN].Cycle = 1;
        ZZT.ElementDefs[ZZT.E_RUFFIAN].TickProc = elementRuffianTick;
        ZZT.ElementDefs[ZZT.E_RUFFIAN].TouchProc = elementDamagingTouch;
        ZZT.ElementDefs[ZZT.E_RUFFIAN].ScoreValue = 2;
        ZZT.ElementDefs[ZZT.E_RUFFIAN].EditorCategory = ZZT.CATEGORY_CREATURE;
        ZZT.ElementDefs[ZZT.E_RUFFIAN].EditorShortcut = "R";
        ZZT.ElementDefs[ZZT.E_RUFFIAN].Name = "Ruffian";
        ZZT.ElementDefs[ZZT.E_RUFFIAN].Param1Name = "Intelligence?";
        ZZT.ElementDefs[ZZT.E_RUFFIAN].Param2Name = "Resting time?";
        ZZT.ElementDefs[ZZT.E_SHARK].Character = "^";
        ZZT.ElementDefs[ZZT.E_SHARK].Color = 0x07;
        ZZT.ElementDefs[ZZT.E_SHARK].Cycle = 3;
        ZZT.ElementDefs[ZZT.E_SHARK].TickProc = elementSharkTick;
        ZZT.ElementDefs[ZZT.E_SHARK].EditorCategory = ZZT.CATEGORY_CREATURE;
        ZZT.ElementDefs[ZZT.E_SHARK].EditorShortcut = "Y";
        ZZT.ElementDefs[ZZT.E_SHARK].Name = "Shark";
        ZZT.ElementDefs[ZZT.E_SHARK].Param1Name = "Intelligence?";
        ZZT.ElementDefs[ZZT.E_SLIME].Character = "*";
        ZZT.ElementDefs[ZZT.E_SLIME].Color = ZZT.COLOR_CHOICE_ON_BLACK;
        ZZT.ElementDefs[ZZT.E_SLIME].Cycle = 3;
        ZZT.ElementDefs[ZZT.E_SLIME].TickProc = elementSlimeTick;
        ZZT.ElementDefs[ZZT.E_SLIME].TouchProc = elementSlimeTouch;
        ZZT.ElementDefs[ZZT.E_SLIME].EditorCategory = ZZT.CATEGORY_CREATURE;
        ZZT.ElementDefs[ZZT.E_SLIME].EditorShortcut = "V";
        ZZT.ElementDefs[ZZT.E_SLIME].Name = "Slime";
        ZZT.ElementDefs[ZZT.E_SLIME].Param2Name = "Movement speed?;FS";
        ZZT.ElementDefs[ZZT.E_AMMO].Character = String.fromCharCode(132);
        ZZT.ElementDefs[ZZT.E_AMMO].Color = 0x03;
        ZZT.ElementDefs[ZZT.E_AMMO].Pushable = true;
        ZZT.ElementDefs[ZZT.E_AMMO].TouchProc = elementAmmoTouch;
        ZZT.ElementDefs[ZZT.E_AMMO].EditorCategory = ZZT.CATEGORY_ITEM;
        ZZT.ElementDefs[ZZT.E_AMMO].EditorShortcut = "A";
        ZZT.ElementDefs[ZZT.E_AMMO].Name = "Ammo";
        ZZT.ElementDefs[ZZT.E_TORCH].Character = String.fromCharCode(157);
        ZZT.ElementDefs[ZZT.E_TORCH].Color = 0x06;
        ZZT.ElementDefs[ZZT.E_TORCH].VisibleInDark = true;
        ZZT.ElementDefs[ZZT.E_TORCH].TouchProc = elementTorchTouch;
        ZZT.ElementDefs[ZZT.E_TORCH].EditorCategory = ZZT.CATEGORY_ITEM;
        ZZT.ElementDefs[ZZT.E_TORCH].EditorShortcut = "T";
        ZZT.ElementDefs[ZZT.E_TORCH].Name = "Torch";
        ZZT.ElementDefs[ZZT.E_GEM].Character = String.fromCharCode(4);
        ZZT.ElementDefs[ZZT.E_GEM].Pushable = true;
        ZZT.ElementDefs[ZZT.E_GEM].Destructible = true;
        ZZT.ElementDefs[ZZT.E_GEM].TouchProc = elementGemTouch;
        ZZT.ElementDefs[ZZT.E_GEM].EditorCategory = ZZT.CATEGORY_ITEM;
        ZZT.ElementDefs[ZZT.E_GEM].EditorShortcut = "G";
        ZZT.ElementDefs[ZZT.E_GEM].Name = "Gem";
        ZZT.ElementDefs[ZZT.E_KEY].Character = String.fromCharCode(12);
        ZZT.ElementDefs[ZZT.E_KEY].Pushable = true;
        ZZT.ElementDefs[ZZT.E_KEY].TouchProc = elementKeyTouch;
        ZZT.ElementDefs[ZZT.E_KEY].EditorCategory = ZZT.CATEGORY_ITEM;
        ZZT.ElementDefs[ZZT.E_KEY].EditorShortcut = "K";
        ZZT.ElementDefs[ZZT.E_KEY].Name = "Key";
        ZZT.ElementDefs[ZZT.E_DOOR].Character = String.fromCharCode(10);
        ZZT.ElementDefs[ZZT.E_DOOR].Color = ZZT.COLOR_WHITE_ON_CHOICE;
        ZZT.ElementDefs[ZZT.E_DOOR].TouchProc = elementDoorTouch;
        ZZT.ElementDefs[ZZT.E_DOOR].EditorCategory = ZZT.CATEGORY_ITEM;
        ZZT.ElementDefs[ZZT.E_DOOR].EditorShortcut = "D";
        ZZT.ElementDefs[ZZT.E_DOOR].Name = "Door";
        ZZT.ElementDefs[ZZT.E_SCROLL].Character = String.fromCharCode(232);
        ZZT.ElementDefs[ZZT.E_SCROLL].Color = 0x0f;
        ZZT.ElementDefs[ZZT.E_SCROLL].Pushable = true;
        ZZT.ElementDefs[ZZT.E_SCROLL].Cycle = 1;
        ZZT.ElementDefs[ZZT.E_SCROLL].TouchProc = elementScrollTouch;
        ZZT.ElementDefs[ZZT.E_SCROLL].TickProc = elementScrollTick;
        ZZT.ElementDefs[ZZT.E_SCROLL].EditorCategory = ZZT.CATEGORY_ITEM;
        ZZT.ElementDefs[ZZT.E_SCROLL].EditorShortcut = "S";
        ZZT.ElementDefs[ZZT.E_SCROLL].Name = "Scroll";
        ZZT.ElementDefs[ZZT.E_SCROLL].ParamTextName = "Edit text of scroll";
        ZZT.ElementDefs[ZZT.E_OBJECT].Character = String.fromCharCode(2);
        ZZT.ElementDefs[ZZT.E_OBJECT].Cycle = 3;
        ZZT.ElementDefs[ZZT.E_OBJECT].HasDrawProc = true;
        ZZT.ElementDefs[ZZT.E_OBJECT].DrawProc = elementObjectDraw;
        ZZT.ElementDefs[ZZT.E_OBJECT].TickProc = elementObjectTick;
        ZZT.ElementDefs[ZZT.E_OBJECT].TouchProc = elementObjectTouch;
        ZZT.ElementDefs[ZZT.E_OBJECT].EditorCategory = ZZT.CATEGORY_CREATURE;
        ZZT.ElementDefs[ZZT.E_OBJECT].EditorShortcut = "O";
        ZZT.ElementDefs[ZZT.E_OBJECT].Name = "Object";
        ZZT.ElementDefs[ZZT.E_OBJECT].Param1Name = "Character?";
        ZZT.ElementDefs[ZZT.E_OBJECT].ParamTextName = "Edit Program";
        ZZT.ElementDefs[ZZT.E_PASSAGE].Character = String.fromCharCode(240);
        ZZT.ElementDefs[ZZT.E_PASSAGE].Color = ZZT.COLOR_WHITE_ON_CHOICE;
        ZZT.ElementDefs[ZZT.E_PASSAGE].Cycle = 0;
        ZZT.ElementDefs[ZZT.E_PASSAGE].VisibleInDark = true;
        ZZT.ElementDefs[ZZT.E_PASSAGE].TouchProc = elementPassageTouch;
        ZZT.ElementDefs[ZZT.E_PASSAGE].EditorCategory = ZZT.CATEGORY_ITEM;
        ZZT.ElementDefs[ZZT.E_PASSAGE].EditorShortcut = "P";
        ZZT.ElementDefs[ZZT.E_PASSAGE].Name = "Passage";
        ZZT.ElementDefs[ZZT.E_PASSAGE].ParamBoardName = "Room thru passage?";
        ZZT.ElementDefs[ZZT.E_BOMB].Character = String.fromCharCode(11);
        ZZT.ElementDefs[ZZT.E_BOMB].Pushable = true;
        ZZT.ElementDefs[ZZT.E_BOMB].Cycle = 6;
        ZZT.ElementDefs[ZZT.E_BOMB].HasDrawProc = true;
        ZZT.ElementDefs[ZZT.E_BOMB].DrawProc = elementBombDraw;
        ZZT.ElementDefs[ZZT.E_BOMB].TickProc = elementBombTick;
        ZZT.ElementDefs[ZZT.E_BOMB].TouchProc = elementBombTouch;
        ZZT.ElementDefs[ZZT.E_BOMB].EditorCategory = ZZT.CATEGORY_ITEM;
        ZZT.ElementDefs[ZZT.E_BOMB].EditorShortcut = "B";
        ZZT.ElementDefs[ZZT.E_BOMB].Name = "Bomb";
        ZZT.ElementDefs[ZZT.E_ENERGIZER].Character = String.fromCharCode(127);
        ZZT.ElementDefs[ZZT.E_ENERGIZER].Color = 0x05;
        ZZT.ElementDefs[ZZT.E_ENERGIZER].TouchProc = elementEnergizerTouch;
        ZZT.ElementDefs[ZZT.E_ENERGIZER].EditorCategory = ZZT.CATEGORY_ITEM;
        ZZT.ElementDefs[ZZT.E_ENERGIZER].EditorShortcut = "E";
        ZZT.ElementDefs[ZZT.E_ENERGIZER].Name = "Energizer";
        ZZT.ElementDefs[ZZT.E_FAKE].Character = String.fromCharCode(178);
        ZZT.ElementDefs[ZZT.E_FAKE].PlaceableOnTop = true;
        ZZT.ElementDefs[ZZT.E_FAKE].Walkable = true;
        ZZT.ElementDefs[ZZT.E_FAKE].TouchProc = elementFakeTouch;
        ZZT.ElementDefs[ZZT.E_FAKE].EditorCategory = ZZT.CATEGORY_TERRAIN;
        ZZT.ElementDefs[ZZT.E_FAKE].EditorShortcut = "A";
        ZZT.ElementDefs[ZZT.E_FAKE].Name = "Fake";
        ZZT.ElementDefs[ZZT.E_INVISIBLE].Character = " ";
        ZZT.ElementDefs[ZZT.E_INVISIBLE].TouchProc = elementInvisibleTouch;
        ZZT.ElementDefs[ZZT.E_INVISIBLE].EditorCategory = ZZT.CATEGORY_TERRAIN;
        ZZT.ElementDefs[ZZT.E_INVISIBLE].EditorShortcut = "I";
        ZZT.ElementDefs[ZZT.E_INVISIBLE].Name = "Invisible";
        ZZT.ElementDefs[ZZT.E_BLINK_WALL].Character = String.fromCharCode(206);
        ZZT.ElementDefs[ZZT.E_BLINK_WALL].Cycle = 1;
        ZZT.ElementDefs[ZZT.E_BLINK_WALL].TickProc = elementBlinkWallTick;
        ZZT.ElementDefs[ZZT.E_BLINK_WALL].HasDrawProc = true;
        ZZT.ElementDefs[ZZT.E_BLINK_WALL].DrawProc = elementBlinkWallDraw;
        ZZT.ElementDefs[ZZT.E_BLINK_WALL].EditorCategory = ZZT.CATEGORY_TERRAIN;
        ZZT.ElementDefs[ZZT.E_BLINK_WALL].EditorShortcut = "L";
        ZZT.ElementDefs[ZZT.E_BLINK_WALL].Name = "Blink wall";
        ZZT.ElementDefs[ZZT.E_BLINK_WALL].Param1Name = "Starting time";
        ZZT.ElementDefs[ZZT.E_BLINK_WALL].Param2Name = "Period";
        ZZT.ElementDefs[ZZT.E_BLINK_WALL].ParamDirName = "Wall direction";
        ZZT.ElementDefs[ZZT.E_BLINK_RAY_EW].Character = String.fromCharCode(205);
        ZZT.ElementDefs[ZZT.E_BLINK_RAY_NS].Character = String.fromCharCode(186);
        ZZT.ElementDefs[ZZT.E_RICOCHET].Character = "*";
        ZZT.ElementDefs[ZZT.E_RICOCHET].Color = 0x0a;
        ZZT.ElementDefs[ZZT.E_RICOCHET].EditorCategory = ZZT.CATEGORY_TERRAIN;
        ZZT.ElementDefs[ZZT.E_RICOCHET].EditorShortcut = "R";
        ZZT.ElementDefs[ZZT.E_RICOCHET].Name = "Ricochet";
        ZZT.ElementDefs[ZZT.E_BOULDER].Character = String.fromCharCode(254);
        ZZT.ElementDefs[ZZT.E_BOULDER].Pushable = true;
        ZZT.ElementDefs[ZZT.E_BOULDER].TouchProc = elementPushableTouch;
        ZZT.ElementDefs[ZZT.E_BOULDER].EditorCategory = ZZT.CATEGORY_TERRAIN;
        ZZT.ElementDefs[ZZT.E_BOULDER].EditorShortcut = "O";
        ZZT.ElementDefs[ZZT.E_BOULDER].Name = "Boulder";
        ZZT.ElementDefs[ZZT.E_SLIDER_NS].Character = String.fromCharCode(18);
        ZZT.ElementDefs[ZZT.E_SLIDER_NS].TouchProc = elementPushableTouch;
        ZZT.ElementDefs[ZZT.E_SLIDER_NS].EditorCategory = ZZT.CATEGORY_TERRAIN;
        ZZT.ElementDefs[ZZT.E_SLIDER_NS].EditorShortcut = "1";
        ZZT.ElementDefs[ZZT.E_SLIDER_NS].Name = "Slider (NS)";
        ZZT.ElementDefs[ZZT.E_SLIDER_EW].Character = String.fromCharCode(29);
        ZZT.ElementDefs[ZZT.E_SLIDER_EW].TouchProc = elementPushableTouch;
        ZZT.ElementDefs[ZZT.E_SLIDER_EW].EditorCategory = ZZT.CATEGORY_TERRAIN;
        ZZT.ElementDefs[ZZT.E_SLIDER_EW].EditorShortcut = "2";
        ZZT.ElementDefs[ZZT.E_SLIDER_EW].Name = "Slider (EW)";
        ZZT.ElementDefs[ZZT.E_TRANSPORTER].Character = String.fromCharCode(197);
        ZZT.ElementDefs[ZZT.E_TRANSPORTER].TouchProc = elementTransporterTouch;
        ZZT.ElementDefs[ZZT.E_TRANSPORTER].HasDrawProc = true;
        ZZT.ElementDefs[ZZT.E_TRANSPORTER].DrawProc = elementTransporterDraw;
        ZZT.ElementDefs[ZZT.E_TRANSPORTER].Cycle = 2;
        ZZT.ElementDefs[ZZT.E_TRANSPORTER].TickProc = elementTransporterTick;
        ZZT.ElementDefs[ZZT.E_TRANSPORTER].EditorCategory = ZZT.CATEGORY_TERRAIN;
        ZZT.ElementDefs[ZZT.E_TRANSPORTER].EditorShortcut = "T";
        ZZT.ElementDefs[ZZT.E_TRANSPORTER].Name = "Transporter";
        ZZT.ElementDefs[ZZT.E_TRANSPORTER].ParamDirName = "Direction?";
        ZZT.ElementDefs[ZZT.E_PUSHER].Character = String.fromCharCode(16);
        ZZT.ElementDefs[ZZT.E_PUSHER].Color = ZZT.COLOR_CHOICE_ON_BLACK;
        ZZT.ElementDefs[ZZT.E_PUSHER].HasDrawProc = true;
        ZZT.ElementDefs[ZZT.E_PUSHER].DrawProc = elementPusherDraw;
        ZZT.ElementDefs[ZZT.E_PUSHER].Cycle = 4;
        ZZT.ElementDefs[ZZT.E_PUSHER].TickProc = elementPusherTick;
        ZZT.ElementDefs[ZZT.E_PUSHER].EditorCategory = ZZT.CATEGORY_CREATURE;
        ZZT.ElementDefs[ZZT.E_PUSHER].EditorShortcut = "P";
        ZZT.ElementDefs[ZZT.E_PUSHER].Name = "Pusher";
        ZZT.ElementDefs[ZZT.E_PUSHER].ParamDirName = "Push direction?";
        ZZT.ElementDefs[ZZT.E_LINE].HasDrawProc = true;
        ZZT.ElementDefs[ZZT.E_LINE].DrawProc = elementLineDraw;
        ZZT.ElementDefs[ZZT.E_TEXT_BLUE].Character = " ";
        ZZT.ElementDefs[ZZT.E_TEXT_GREEN].Character = " ";
        ZZT.ElementDefs[ZZT.E_TEXT_CYAN].Character = " ";
        ZZT.ElementDefs[ZZT.E_TEXT_RED].Character = " ";
        ZZT.ElementDefs[ZZT.E_TEXT_PURPLE].Character = " ";
        ZZT.ElementDefs[ZZT.E_TEXT_YELLOW].Character = " ";
        ZZT.ElementDefs[ZZT.E_TEXT_WHITE].Character = " ";
        ZZT.EditorPatternCount = 5;
        ZZT.EditorPatterns[1] = ZZT.E_SOLID;
        ZZT.EditorPatterns[2] = ZZT.E_NORMAL;
        ZZT.EditorPatterns[3] = ZZT.E_BREAKABLE;
        ZZT.EditorPatterns[4] = ZZT.E_EMPTY;
        ZZT.EditorPatterns[5] = ZZT.E_LINE;
    }
    function ResetMessageNotShownFlags() {
        ZZT.MessageAmmoNotShown = true;
        ZZT.MessageOutOfAmmoNotShown = true;
        ZZT.MessageNoShootingNotShown = true;
        ZZT.MessageTorchNotShown = true;
        ZZT.MessageOutOfTorchesNotShown = true;
        ZZT.MessageRoomNotDarkNotShown = true;
        ZZT.MessageHintTorchNotShown = true;
        ZZT.MessageForestNotShown = true;
        ZZT.MessageFakeNotShown = true;
        ZZT.MessageGemNotShown = true;
        ZZT.MessageEnergizerNotShown = true;
    }
    ZZT.ResetMessageNotShownFlags = ResetMessageNotShownFlags;
    function InitElementsEditor() {
        initElementDefs();
        ZZT.ElementDefs[ZZT.E_INVISIBLE].Character = String.fromCharCode(176);
        ZZT.ElementDefs[ZZT.E_INVISIBLE].Color = ZZT.COLOR_CHOICE_ON_BLACK;
        ZZT.ForceDarknessOff = true;
    }
    ZZT.InitElementsEditor = InitElementsEditor;
    function InitElementsGame() {
        initElementDefs();
        ZZT.ForceDarknessOff = false;
    }
    ZZT.InitElementsGame = InitElementsGame;
    function InitEditorStatSettings() {
        var i;
        ZZT.PlayerDirX = 0;
        ZZT.PlayerDirY = 0;
        for (i = 0; i <= ZZT.MAX_ELEMENT; i += 1) {
            ZZT.World.EditorStatSettings[i] = {
                P1: 4,
                P2: 4,
                P3: 0,
                StepX: 0,
                StepY: -1
            };
        }
        ZZT.World.EditorStatSettings[ZZT.E_OBJECT].P1 = 1;
        ZZT.World.EditorStatSettings[ZZT.E_BEAR].P1 = 8;
    }
    ZZT.InitEditorStatSettings = InitEditorStatSettings;
})(ZZT || (ZZT = {}));
var ZZT;
(function (ZZT) {
    var SOUND_TICK_MS = 55;
    var BRIDGE_PREFIX = "FLWEB/1 ";
    var BRIDGE_RESPONSE_PREFIX = "FLWEB/1R ";
    var ZZT_SOUND_ID = "zzt-main";
    var ZZT_MUSIC_ID = "zzt-bgm";
    var SHARED_MUSIC_REL_ROOT = "zzt_worlds";
    ZZT.SoundEnabled = true;
    ZZT.SoundBlockQueueing = false;
    ZZT.SoundCurrentPriority = -1;
    ZZT.SoundDurationMultiplier = 1;
    ZZT.SoundDurationCounter = 1;
    ZZT.SoundBuffer = "";
    ZZT.SoundBufferPos = 0;
    ZZT.SoundIsPlaying = false;
    ZZT.AnsiMusicMode = "AUTO";
    ZZT.AnsiMusicIntroducer = "|";
    ZZT.AnsiMusicForeground = false;
    var SoundFreqTable = [];
    var SoundTickLastMs = 0;
    var SoundBridgeState = 0; // 0 unknown, 1 available, -1 unavailable
    var SoundOutputActive = false;
    var AnsiMusicActive = false;
    var WorldMusicTracks = [];
    var WorldMusicCurrentAssetPath = "";
    function nowMs() {
        return new Date().getTime();
    }
    function toByteCode(ch) {
        if (ch.length <= 0) {
            return 0;
        }
        return ch.charCodeAt(0) & 0xff;
    }
    function buildAsciiSafeJson(value) {
        return JSON.stringify(value).replace(/[^\x20-\x7e]/g, function (ch) {
            var code = ch.charCodeAt(0).toString(16).toUpperCase();
            while (code.length < 4) {
                code = "0" + code;
            }
            return "\\u" + code;
        });
    }
    function writeBridgeRaw(text) {
        if (typeof console === "undefined") {
            return false;
        }
        try {
            if (typeof console.write === "function") {
                console.write(text);
                return true;
            }
            if (typeof console.print === "function") {
                console.print(text);
                return true;
            }
        }
        catch (_err) {
            return false;
        }
        return false;
    }
    function soundTrimSpaces(value) {
        return String(value || "").replace(/^\s+/, "").replace(/\s+$/, "");
    }
    function normalizeAnsiMusicModeValue(mode) {
        var upper = soundTrimSpaces(mode).toUpperCase();
        if (upper === "ON" || upper === "TRUE" || upper === "YES" || upper === "1") {
            return "ON";
        }
        if (upper === "AUTO" || upper === "CTERM" || upper === "SYNCTERM") {
            return "AUTO";
        }
        return "OFF";
    }
    function normalizeAnsiMusicIntroducerValue(introducer) {
        var upper = soundTrimSpaces(introducer).toUpperCase();
        if (upper === "N" || upper === "CSI_N" || upper === "BANANSI" || upper === "BANSI") {
            return "N";
        }
        if (upper === "M" || upper === "CSI_M" || upper === "DL") {
            return "M";
        }
        if (upper === "|" || upper === "PIPE" || upper === "BAR" || upper === "CSI_PIPE") {
            return "|";
        }
        return "|";
    }
    function normalizeAnsiMusicForegroundValue(value) {
        var upper = soundTrimSpaces(value).toUpperCase();
        if (upper === "ON" || upper === "TRUE" || upper === "YES" || upper === "1" ||
            upper === "FOREGROUND" || upper === "FG" || upper === "SYNC") {
            return true;
        }
        return false;
    }
    function detectAnsiMusicSupportForTerminal() {
        var ctermVersion;
        if (typeof console === "undefined" ||
            (typeof console.write !== "function" && typeof console.print !== "function")) {
            return false;
        }
        if (ZZT.AnsiMusicMode === "ON") {
            return true;
        }
        if (ZZT.AnsiMusicMode !== "AUTO") {
            return false;
        }
        ctermVersion = (typeof console.cterm_version === "number" ? console.cterm_version : -1);
        return ctermVersion >= 0;
    }
    function ansiMusicLengthFromDuration(durationCode) {
        var denom;
        if (durationCode <= 0) {
            return 32;
        }
        denom = Math.floor((32 + Math.floor(durationCode / 2)) / durationCode);
        if (denom < 1) {
            denom = 1;
        }
        if (denom > 64) {
            denom = 64;
        }
        return denom;
    }
    function ansiMusicToneToken(tone) {
        switch (tone) {
            case 0:
                return "C";
            case 1:
                return "C#";
            case 2:
                return "D";
            case 3:
                return "D#";
            case 4:
                return "E";
            case 5:
                return "F";
            case 6:
                return "F#";
            case 7:
                return "G";
            case 8:
                return "G#";
            case 9:
                return "A";
            case 10:
                return "A#";
            case 11:
                return "B";
            default:
                return "";
        }
    }
    function ansiMusicDrumToNoteCode(drumId) {
        var noteCodes = [0x20, 0x23, 0x27, 0x30, 0x34, 0x37, 0x40, 0x44, 0x47, 0x50];
        var index = drumId % noteCodes.length;
        if (index < 0) {
            index += noteCodes.length;
        }
        return noteCodes[index];
    }
    function buildAnsiMusicFromPattern(pattern) {
        var tokens = [];
        var i = 0;
        var noteCode;
        var durationCode;
        var noteLength = 32;
        var currentLength = 32;
        var octave = 3;
        var currentOctave = 3;
        var tone;
        var token;
        if (pattern.length <= 1) {
            return "";
        }
        tokens.push("T120");
        tokens.push("O3");
        tokens.push("L32");
        while ((i + 1) < pattern.length) {
            noteCode = toByteCode(pattern.charAt(i));
            durationCode = toByteCode(pattern.charAt(i + 1));
            i += 2;
            noteLength = ansiMusicLengthFromDuration(durationCode);
            if (noteLength !== currentLength) {
                tokens.push("L" + String(noteLength));
                currentLength = noteLength;
            }
            if (noteCode === 0) {
                tokens.push("P");
                continue;
            }
            if (noteCode >= 240) {
                noteCode = ansiMusicDrumToNoteCode(noteCode - 240);
            }
            tone = noteCode % 16;
            if (tone < 0 || tone > 11) {
                tokens.push("P");
                continue;
            }
            octave = Math.floor(noteCode / 16);
            if (octave < 0) {
                octave = 0;
            }
            if (octave > 6) {
                octave = 6;
            }
            if (octave !== currentOctave) {
                tokens.push("O" + String(octave));
                currentOctave = octave;
            }
            token = ansiMusicToneToken(tone);
            if (token.length <= 0) {
                tokens.push("P");
            }
            else {
                tokens.push(token);
            }
        }
        if (tokens.length <= 3) {
            return "";
        }
        return tokens.join("");
    }
    function emitAnsiMusicFromPattern(pattern) {
        var prefix;
        var intro;
        var music;
        if (!AnsiMusicActive || !ZZT.SoundEnabled) {
            return;
        }
        music = buildAnsiMusicFromPattern(pattern);
        if (music.length <= 0) {
            return;
        }
        intro = normalizeAnsiMusicIntroducerValue(ZZT.AnsiMusicIntroducer);
        prefix = "";
        if (intro === "|" || intro === "M") {
            prefix = (ZZT.AnsiMusicForeground ? "F" : "B");
        }
        writeBridgeRaw("\x1b[" + intro + prefix + music + "\x0E");
    }
    function emitBridgePacket(action, payload) {
        var packet = {};
        var key;
        if (typeof console === "undefined") {
            return false;
        }
        packet.action = action;
        for (key in payload) {
            if (Object.prototype.hasOwnProperty.call(payload, key)) {
                packet[key] = payload[key];
            }
        }
        return writeBridgeRaw("\x1b_" + BRIDGE_PREFIX + buildAsciiSafeJson(packet) + "\x1b\\");
    }
    function soundBridgeProbe(timeoutMs) {
        var nonce;
        var deadline;
        var buffer;
        var ch;
        var start;
        var end;
        var payload;
        var data;
        var flushDeadline;
        if (typeof console === "undefined" ||
            (typeof console.write !== "function" && typeof console.print !== "function") ||
            typeof console.inkey !== "function") {
            return false;
        }
        nonce = String(nowMs()).slice(-8) + String(Math.floor(Math.random() * 10000));
        while (console.inkey(0, 0) !== "") {
            // Flush stale input before probing.
        }
        if (!emitBridgePacket("bridge.probe", { nonce: nonce })) {
            return false;
        }
        buffer = "";
        deadline = nowMs() + timeoutMs;
        while (nowMs() < deadline) {
            ch = console.inkey(0, 50);
            if (ch.length <= 0) {
                continue;
            }
            buffer += ch;
            start = buffer.indexOf("\x1b_");
            if (start < 0) {
                if (buffer.length > 512) {
                    buffer = buffer.slice(buffer.length - 256);
                }
                continue;
            }
            end = buffer.indexOf("\x1b\\", start + 2);
            if (end < 0) {
                continue;
            }
            payload = buffer.slice(start + 2, end);
            buffer = buffer.slice(end + 2);
            if (payload.indexOf(BRIDGE_RESPONSE_PREFIX) !== 0) {
                continue;
            }
            data = null;
            try {
                data = JSON.parse(payload.slice(BRIDGE_RESPONSE_PREFIX.length));
            }
            catch (_parseErr) {
                data = null;
            }
            if (data !== null &&
                data.action === "bridge.ack" &&
                data.nonce === nonce) {
                flushDeadline = nowMs() + 100;
                while (nowMs() < flushDeadline) {
                    if (console.inkey(0, 10) === "") {
                        break;
                    }
                }
                return true;
            }
        }
        return false;
    }
    function ensureBridgeDetected() {
        if (SoundBridgeState > 0) {
            return true;
        }
        if (SoundBridgeState < 0) {
            return false;
        }
        SoundBridgeState = soundBridgeProbe(450) ? 1 : -1;
        return SoundBridgeState > 0;
    }
    function emitSoundStop() {
        if (!SoundOutputActive) {
            return;
        }
        if (ensureBridgeDetected()) {
            emitBridgePacket("audio.zzt.stop", {
                id: ZZT_SOUND_ID
            });
        }
        SoundOutputActive = false;
    }
    function emitSoundTone(freqHz, durationTicks) {
        var durationMs;
        if (freqHz <= 0) {
            emitSoundStop();
            return;
        }
        if (!ensureBridgeDetected()) {
            return;
        }
        durationMs = Math.floor(durationTicks * SOUND_TICK_MS);
        if (durationMs < 20) {
            durationMs = 20;
        }
        emitBridgePacket("audio.zzt.note", {
            id: ZZT_SOUND_ID,
            frequencyHz: freqHz,
            durationMs: durationMs,
            volume: 0.18,
            waveform: "square"
        });
        SoundOutputActive = true;
    }
    function emitSoundDrum(drumId, durationTicks) {
        var durationMs;
        if (!ensureBridgeDetected()) {
            return;
        }
        durationMs = Math.floor(durationTicks * SOUND_TICK_MS);
        if (durationMs < 20) {
            durationMs = 20;
        }
        emitBridgePacket("audio.zzt.drum", {
            id: ZZT_SOUND_ID,
            drumId: drumId,
            durationMs: durationMs,
            volume: 0.25
        });
        SoundOutputActive = true;
    }
    function normalizePath(path) {
        return String(path || "").split("\\").join("/");
    }
    function stripFileExtension(path) {
        var dot = path.lastIndexOf(".");
        if (dot <= 0) {
            return path;
        }
        return path.slice(0, dot);
    }
    function pathDirname(path) {
        var normalized = normalizePath(path);
        var slashPos = normalized.lastIndexOf("/");
        if (slashPos < 0) {
            return "";
        }
        return normalized.slice(0, slashPos + 1);
    }
    function pathBasename(path) {
        var normalized = normalizePath(path);
        var slashPos = normalized.lastIndexOf("/");
        if (slashPos < 0) {
            return normalized;
        }
        return normalized.slice(slashPos + 1);
    }
    function splitTokens(text) {
        var cleaned = String(text || "").toLowerCase().replace(/[^a-z0-9]+/g, " ");
        var parts = cleaned.split(/\s+/);
        var out = [];
        var i;
        for (i = 0; i < parts.length; i += 1) {
            if (parts[i].length > 0) {
                out.push(parts[i]);
            }
        }
        return out;
    }
    function sanitizePathSegment(value) {
        var out = String(value || "").toLowerCase().replace(/[^a-z0-9._-]+/g, "-");
        out = out.replace(/^-+/, "").replace(/-+$/, "");
        if (out.length <= 0) {
            return "world";
        }
        return out;
    }
    function fileExists(path) {
        var f;
        if (typeof File === "undefined") {
            return false;
        }
        try {
            f = new File(path);
            return !!f.exists;
        }
        catch (_err) {
            return false;
        }
    }
    function pathJoin(base, rel) {
        var b = String(base || "");
        if (b.length > 0) {
            var tail = b.charAt(b.length - 1);
            if (tail !== "/" && tail !== "\\") {
                b += "/";
            }
        }
        return b + rel;
    }
    function ensureDirectory(path) {
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
            if (typeof file_isdir === "function" && file_isdir(path)) {
                return true;
            }
            return false;
        }
        catch (_err) {
            return false;
        }
    }
    function copyBinaryFile(sourcePath, destinationPath) {
        var source;
        var destination;
        var chunk;
        if (typeof File === "undefined") {
            return false;
        }
        try {
            source = new File(sourcePath);
            destination = new File(destinationPath);
        }
        catch (_err) {
            return false;
        }
        if (!source.open("rb")) {
            return false;
        }
        if (!destination.open("wb")) {
            source.close();
            return false;
        }
        try {
            while (true) {
                chunk = source.read(8192);
                if (chunk === null || typeof chunk === "undefined" || chunk.length <= 0) {
                    break;
                }
                destination.write(chunk);
            }
        }
        finally {
            source.close();
            destination.close();
        }
        return true;
    }
    function stageWorldMusicFile(sourcePath, worldBucket) {
        var modsRoot;
        var relativeAssetPath;
        var destinationDir;
        var destinationPath;
        var sourceSize = -1;
        var destinationSize = -2;
        var copied;
        var fileName = pathBasename(sourcePath);
        if (typeof system === "undefined" ||
            typeof system.mods_dir !== "string" ||
            system.mods_dir.length <= 0) {
            return "";
        }
        if (fileName.length <= 0) {
            return "";
        }
        modsRoot = system.mods_dir;
        relativeAssetPath = SHARED_MUSIC_REL_ROOT + "/" + worldBucket + "/" + fileName;
        destinationDir = pathJoin(modsRoot, "flweb/assets/" + SHARED_MUSIC_REL_ROOT + "/" + worldBucket + "/");
        destinationPath = pathJoin(destinationDir, fileName);
        if (!ensureDirectory(destinationDir)) {
            return "";
        }
        if (typeof file_size === "function") {
            try {
                sourceSize = file_size(sourcePath);
                destinationSize = file_size(destinationPath);
            }
            catch (_err) {
                sourceSize = -1;
                destinationSize = -2;
            }
        }
        if (fileExists(destinationPath) && sourceSize >= 0 && sourceSize === destinationSize) {
            return relativeAssetPath;
        }
        copied = false;
        if (typeof file_copy === "function") {
            try {
                copied = !!file_copy(sourcePath, destinationPath);
            }
            catch (_err) {
                copied = false;
            }
        }
        if (!copied) {
            copied = copyBinaryFile(sourcePath, destinationPath);
        }
        if (!copied) {
            return "";
        }
        return relativeAssetPath;
    }
    function listWorldPackMp3Files(worldFilePath) {
        var worldDir = pathDirname(worldFilePath);
        var listed = [];
        var seen = {};
        var files = [];
        var i;
        var normalized;
        var upper;
        if (worldDir.length <= 0 || typeof directory !== "function") {
            return files;
        }
        listed = directory(pathJoin(worldDir, "*"));
        for (i = 0; i < listed.length; i += 1) {
            normalized = normalizePath(listed[i]);
            if (typeof file_isdir === "function" && file_isdir(normalized)) {
                continue;
            }
            upper = normalized.toUpperCase();
            if (upper.length < 4 || upper.slice(upper.length - 4) !== ".MP3") {
                continue;
            }
            if (seen[upper] === true) {
                continue;
            }
            seen[upper] = true;
            files.push(normalized);
        }
        files.sort(function (a, b) {
            var aUpper = a.toUpperCase();
            var bUpper = b.toUpperCase();
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
    function scoreTrackForBoard(track, boardName) {
        var boardKey = stripFileExtension(pathBasename(boardName)).toLowerCase();
        var boardTokens = splitTokens(boardKey);
        var trackTokens = track.Tokens;
        var score = 0;
        var i;
        var j;
        if (boardKey.length > 0) {
            if (boardKey === track.TitleKey) {
                score += 25;
            }
            else if (boardKey.indexOf(track.TitleKey) >= 0 || track.TitleKey.indexOf(boardKey) >= 0) {
                score += 12;
            }
        }
        for (i = 0; i < boardTokens.length; i += 1) {
            if (boardTokens[i].length <= 1) {
                continue;
            }
            for (j = 0; j < trackTokens.length; j += 1) {
                if (boardTokens[i] === trackTokens[j]) {
                    score += 3;
                }
            }
        }
        return score;
    }
    function chooseWorldMusicTrack(boardName) {
        var i;
        var score;
        var bestScore = -1;
        var best = null;
        if (WorldMusicTracks.length <= 0) {
            return null;
        }
        if (boardName.length <= 0) {
            return WorldMusicTracks[0];
        }
        for (i = 0; i < WorldMusicTracks.length; i += 1) {
            score = scoreTrackForBoard(WorldMusicTracks[i], boardName);
            if (score > bestScore) {
                bestScore = score;
                best = WorldMusicTracks[i];
            }
        }
        if (best === null) {
            return WorldMusicTracks[0];
        }
        if (bestScore <= 0) {
            return WorldMusicTracks[0];
        }
        return best;
    }
    function SoundWorldMusicConfigureFromWorldFile(worldFilePath) {
        var sources = [];
        var i;
        var worldBase;
        var worldParent;
        var worldBucket;
        var assetPath;
        var trackTitle;
        SoundWorldMusicStop();
        WorldMusicTracks = [];
        if (worldFilePath.length <= 0) {
            return;
        }
        sources = listWorldPackMp3Files(worldFilePath);
        if (sources.length <= 0) {
            return;
        }
        worldBase = sanitizePathSegment(stripFileExtension(pathBasename(worldFilePath)));
        worldParent = sanitizePathSegment(pathBasename(pathDirname(worldFilePath)));
        if (worldParent.length > 0 && worldParent !== "world") {
            worldBucket = worldParent + "-" + worldBase;
        }
        else {
            worldBucket = worldBase;
        }
        for (i = 0; i < sources.length; i += 1) {
            assetPath = stageWorldMusicFile(sources[i], worldBucket);
            if (assetPath.length <= 0) {
                continue;
            }
            trackTitle = stripFileExtension(pathBasename(sources[i])).toLowerCase();
            WorldMusicTracks.push({
                AssetPath: assetPath,
                TitleKey: trackTitle,
                Tokens: splitTokens(trackTitle)
            });
        }
    }
    ZZT.SoundWorldMusicConfigureFromWorldFile = SoundWorldMusicConfigureFromWorldFile;
    function SoundWorldMusicStop() {
        if (SoundBridgeState > 0) {
            emitBridgePacket("audio.stop", {
                id: ZZT_MUSIC_ID
            });
        }
        WorldMusicCurrentAssetPath = "";
    }
    ZZT.SoundWorldMusicStop = SoundWorldMusicStop;
    function SoundWorldMusicOnBoardChanged(boardName) {
        var selected;
        if (!ZZT.SoundEnabled) {
            return;
        }
        selected = chooseWorldMusicTrack(boardName);
        if (selected === null) {
            return;
        }
        if (selected.AssetPath === WorldMusicCurrentAssetPath) {
            return;
        }
        if (!ensureBridgeDetected()) {
            return;
        }
        if (emitBridgePacket("audio.play", {
            id: ZZT_MUSIC_ID,
            asset: {
                scope: "shared",
                path: selected.AssetPath
            },
            volume: 0.55,
            loop: true
        })) {
            WorldMusicCurrentAssetPath = selected.AssetPath;
        }
    }
    ZZT.SoundWorldMusicOnBoardChanged = SoundWorldMusicOnBoardChanged;
    function SoundWorldMusicHasTracks() {
        return WorldMusicTracks.length > 0;
    }
    ZZT.SoundWorldMusicHasTracks = SoundWorldMusicHasTracks;
    function initFreqTable() {
        var octave;
        var note;
        var freqC1 = 32.0;
        var noteStep = Math.exp(Math.log(2.0) / 12.0);
        var noteBase;
        var i;
        SoundFreqTable = [];
        for (i = 0; i <= 255; i += 1) {
            SoundFreqTable.push(0);
        }
        for (octave = 1; octave <= 15; octave += 1) {
            noteBase = Math.exp(octave * Math.log(2.0)) * freqC1;
            for (note = 0; note <= 11; note += 1) {
                SoundFreqTable[(octave * 16) + note] = Math.floor(noteBase);
                noteBase *= noteStep;
            }
        }
    }
    function soundTimerTick() {
        var noteCode;
        var durationCode;
        var duration;
        var freqHz;
        if (!ZZT.SoundEnabled) {
            if (ZZT.SoundIsPlaying) {
                ZZT.SoundIsPlaying = false;
            }
            emitSoundStop();
            return;
        }
        if (!ZZT.SoundIsPlaying) {
            return;
        }
        ZZT.SoundDurationCounter -= 1;
        if (ZZT.SoundDurationCounter > 0) {
            return;
        }
        emitSoundStop();
        if (ZZT.SoundBufferPos >= ZZT.SoundBuffer.length) {
            ZZT.SoundIsPlaying = false;
            return;
        }
        noteCode = toByteCode(ZZT.SoundBuffer.charAt(ZZT.SoundBufferPos));
        ZZT.SoundBufferPos += 1;
        if (ZZT.SoundBufferPos >= ZZT.SoundBuffer.length) {
            ZZT.SoundIsPlaying = false;
            return;
        }
        durationCode = toByteCode(ZZT.SoundBuffer.charAt(ZZT.SoundBufferPos));
        ZZT.SoundBufferPos += 1;
        duration = ZZT.SoundDurationMultiplier * durationCode;
        if (duration <= 0) {
            duration = 1;
        }
        ZZT.SoundDurationCounter = duration;
        if (noteCode === 0) {
            return;
        }
        if (noteCode < 240) {
            freqHz = SoundFreqTable[noteCode];
            if (freqHz > 0) {
                emitSoundTone(freqHz, duration);
            }
            return;
        }
        emitSoundDrum(noteCode - 240, duration);
    }
    function SoundInit() {
        initFreqTable();
        ZZT.SoundEnabled = true;
        ZZT.SoundBlockQueueing = false;
        ZZT.SoundCurrentPriority = -1;
        ZZT.SoundDurationMultiplier = 1;
        ZZT.SoundDurationCounter = 1;
        ZZT.SoundBuffer = "";
        ZZT.SoundBufferPos = 0;
        ZZT.SoundIsPlaying = false;
        SoundOutputActive = false;
        SoundTickLastMs = nowMs();
        SoundBridgeState = 0;
        ZZT.AnsiMusicMode = normalizeAnsiMusicModeValue(ZZT.AnsiMusicMode);
        ZZT.AnsiMusicIntroducer = normalizeAnsiMusicIntroducerValue(ZZT.AnsiMusicIntroducer);
        ZZT.AnsiMusicForeground = normalizeAnsiMusicForegroundValue(ZZT.AnsiMusicForeground ? "ON" : "OFF");
        AnsiMusicActive = detectAnsiMusicSupportForTerminal();
        WorldMusicTracks = [];
        WorldMusicCurrentAssetPath = "";
        // Probe once up front to avoid probing during gameplay input handling.
        ensureBridgeDetected();
    }
    ZZT.SoundInit = SoundInit;
    function SoundUpdate() {
        var now = nowMs();
        var elapsed;
        var ticks;
        var i;
        if (SoundTickLastMs <= 0) {
            SoundTickLastMs = now;
            return;
        }
        if (now < SoundTickLastMs) {
            SoundTickLastMs = now;
            return;
        }
        elapsed = now - SoundTickLastMs;
        if (elapsed < SOUND_TICK_MS) {
            return;
        }
        ticks = Math.floor(elapsed / SOUND_TICK_MS);
        if (ticks > 12) {
            ticks = 12;
        }
        SoundTickLastMs += ticks * SOUND_TICK_MS;
        for (i = 0; i < ticks; i += 1) {
            soundTimerTick();
        }
    }
    ZZT.SoundUpdate = SoundUpdate;
    function SoundUninstall() {
        SoundWorldMusicStop();
        emitSoundStop();
        SoundBridgeState = -1;
    }
    ZZT.SoundUninstall = SoundUninstall;
    function SoundClearQueue() {
        ZZT.SoundBuffer = "";
        ZZT.SoundBufferPos = 0;
        ZZT.SoundIsPlaying = false;
        ZZT.SoundCurrentPriority = -1;
        ZZT.SoundDurationCounter = 1;
        emitSoundStop();
        SoundWorldMusicStop();
    }
    ZZT.SoundClearQueue = SoundClearQueue;
    function SoundQueue(priority, pattern) {
        if (ZZT.SoundBlockQueueing) {
            return;
        }
        if (!ZZT.SoundIsPlaying ||
            (((priority >= ZZT.SoundCurrentPriority) && (ZZT.SoundCurrentPriority !== -1)) || (priority === -1))) {
            if (priority >= 0 || !ZZT.SoundIsPlaying) {
                ZZT.SoundCurrentPriority = priority;
                ZZT.SoundBuffer = pattern;
                ZZT.SoundBufferPos = 0;
                ZZT.SoundDurationCounter = 1;
            }
            else {
                ZZT.SoundBuffer = ZZT.SoundBuffer.slice(ZZT.SoundBufferPos);
                ZZT.SoundBufferPos = 0;
                if ((ZZT.SoundBuffer.length + pattern.length) < 255) {
                    ZZT.SoundBuffer += pattern;
                }
            }
            ZZT.SoundIsPlaying = true;
            emitAnsiMusicFromPattern(pattern);
        }
    }
    ZZT.SoundQueue = SoundQueue;
    function SoundParse(input) {
        var noteOctave = 3;
        var noteDuration = 1;
        var output = "";
        var i = 0;
        var tone;
        var ch;
        function appendNote(noteTone) {
            output += String.fromCharCode(((noteOctave * 16) + noteTone) & 0xff);
            output += String.fromCharCode(noteDuration & 0xff);
        }
        while (i < input.length) {
            ch = input.charAt(i).toUpperCase();
            tone = -1;
            if (ch === "T") {
                noteDuration = 1;
                i += 1;
            }
            else if (ch === "S") {
                noteDuration = 2;
                i += 1;
            }
            else if (ch === "I") {
                noteDuration = 4;
                i += 1;
            }
            else if (ch === "Q") {
                noteDuration = 8;
                i += 1;
            }
            else if (ch === "H") {
                noteDuration = 16;
                i += 1;
            }
            else if (ch === "W") {
                noteDuration = 32;
                i += 1;
            }
            else if (ch === ".") {
                noteDuration = Math.floor((noteDuration * 3) / 2);
                i += 1;
            }
            else if (ch === "3") {
                noteDuration = Math.floor(noteDuration / 3);
                i += 1;
            }
            else if (ch === "+") {
                if (noteOctave < 6) {
                    noteOctave += 1;
                }
                i += 1;
            }
            else if (ch === "-") {
                if (noteOctave > 1) {
                    noteOctave -= 1;
                }
                i += 1;
            }
            else if (ch >= "A" && ch <= "G") {
                if (ch === "C") {
                    tone = 0;
                }
                else if (ch === "D") {
                    tone = 2;
                }
                else if (ch === "E") {
                    tone = 4;
                }
                else if (ch === "F") {
                    tone = 5;
                }
                else if (ch === "G") {
                    tone = 7;
                }
                else if (ch === "A") {
                    tone = 9;
                }
                else {
                    tone = 11;
                }
                i += 1;
                if (i < input.length) {
                    ch = input.charAt(i).toUpperCase();
                    if (ch === "!") {
                        tone -= 1;
                        i += 1;
                    }
                    else if (ch === "#") {
                        tone += 1;
                        i += 1;
                    }
                }
                appendNote(tone);
            }
            else if (ch === "X") {
                output += String.fromCharCode(0);
                output += String.fromCharCode(noteDuration & 0xff);
                i += 1;
            }
            else if (ch >= "0" && ch <= "9") {
                output += String.fromCharCode((ch.charCodeAt(0) + 0xf0 - "0".charCodeAt(0)) & 0xff);
                output += String.fromCharCode(noteDuration & 0xff);
                i += 1;
            }
            else {
                i += 1;
            }
        }
        return output;
    }
    ZZT.SoundParse = SoundParse;
})(ZZT || (ZZT = {}));
var ZZT;
(function (ZZT) {
    var K_NOECHO = 1 << 17;
    var K_NOSPIN = 1 << 21;
    var K_EXTKEYS = 1 << 30;
    ZZT.KEY_BACKSPACE = String.fromCharCode(8);
    ZZT.KEY_TAB = String.fromCharCode(9);
    ZZT.KEY_ENTER = String.fromCharCode(13);
    ZZT.KEY_CTRL_Y = String.fromCharCode(25);
    ZZT.KEY_ESCAPE = String.fromCharCode(27);
    ZZT.KEY_ALT_P = String.fromCharCode(153);
    ZZT.KEY_F1 = String.fromCharCode(187);
    ZZT.KEY_F2 = String.fromCharCode(188);
    ZZT.KEY_F3 = String.fromCharCode(189);
    ZZT.KEY_F4 = String.fromCharCode(190);
    ZZT.KEY_F5 = String.fromCharCode(191);
    ZZT.KEY_F6 = String.fromCharCode(192);
    ZZT.KEY_F7 = String.fromCharCode(193);
    ZZT.KEY_F8 = String.fromCharCode(194);
    ZZT.KEY_F9 = String.fromCharCode(195);
    ZZT.KEY_F10 = String.fromCharCode(196);
    // Synchronet key_defs.js control-code values.
    ZZT.KEY_UP = String.fromCharCode(30);
    ZZT.KEY_PAGE_UP = String.fromCharCode(16);
    ZZT.KEY_LEFT = String.fromCharCode(29);
    ZZT.KEY_RIGHT = String.fromCharCode(6);
    ZZT.KEY_DOWN = String.fromCharCode(10);
    ZZT.KEY_PAGE_DOWN = String.fromCharCode(14);
    ZZT.KEY_INSERT = String.fromCharCode(22);
    ZZT.KEY_DELETE = String.fromCharCode(127);
    ZZT.KEY_HOME = String.fromCharCode(2);
    ZZT.KEY_END = String.fromCharCode(5);
    ZZT.InputDeltaX = 0;
    ZZT.InputDeltaY = 0;
    ZZT.InputShiftPressed = false;
    ZZT.InputShiftAccepted = false;
    ZZT.InputJoystickEnabled = false;
    ZZT.InputMouseEnabled = false;
    ZZT.InputKeyPressed = String.fromCharCode(0);
    ZZT.InputFirePressed = false;
    ZZT.InputFireDirX = 0;
    ZZT.InputFireDirY = 0;
    ZZT.InputTorchPressed = false;
    ZZT.InputMouseX = 0;
    ZZT.InputMouseY = 0;
    ZZT.InputMouseActivationX = 0;
    ZZT.InputMouseActivationY = 0;
    ZZT.InputMouseButtonX = 0;
    ZZT.InputMouseButtonY = 0;
    ZZT.InputJoystickMoved = false;
    ZZT.JoystickXInitial = 0;
    ZZT.JoystickYInitial = 0;
    ZZT.InputLastDeltaX = 0;
    ZZT.InputLastDeltaY = 0;
    var InputOldCtrlKeyPassthru = null;
    var InputRawBuffer = "";
    function normalizeRaw(raw) {
        if (raw === null || typeof raw === "undefined") {
            return "";
        }
        return String(raw);
    }
    function inputReadMode() {
        return K_NOECHO | K_NOSPIN | K_EXTKEYS;
    }
    function decodeExtendedKey(raw) {
        var nul = String.fromCharCode(0);
        var esc = ZZT.KEY_ESCAPE;
        var second;
        var finalChar;
        var numberPart;
        var semicolonPos;
        var tildeCode;
        if (raw.length === 0) {
            return nul;
        }
        if (raw.length === 1) {
            return raw;
        }
        if (raw === esc + "[A") {
            return ZZT.KEY_UP;
        }
        if (raw === esc + "[B") {
            return ZZT.KEY_DOWN;
        }
        if (raw === esc + "[C") {
            return ZZT.KEY_RIGHT;
        }
        if (raw === esc + "[D") {
            return ZZT.KEY_LEFT;
        }
        if (raw === esc + "[H" || raw === esc + "[1~") {
            return ZZT.KEY_HOME;
        }
        if (raw === esc + "[F" || raw === esc + "[4~") {
            return ZZT.KEY_END;
        }
        if (raw === esc + "[2~") {
            return ZZT.KEY_INSERT;
        }
        if (raw === esc + "[3~") {
            return ZZT.KEY_DELETE;
        }
        if (raw === esc + "[5~") {
            return ZZT.KEY_PAGE_UP;
        }
        if (raw === esc + "[6~") {
            return ZZT.KEY_PAGE_DOWN;
        }
        if (raw === esc + "OA") {
            return ZZT.KEY_UP;
        }
        if (raw === esc + "OB") {
            return ZZT.KEY_DOWN;
        }
        if (raw === esc + "OC") {
            return ZZT.KEY_RIGHT;
        }
        if (raw === esc + "OD") {
            return ZZT.KEY_LEFT;
        }
        if (raw === esc + "OH") {
            return ZZT.KEY_HOME;
        }
        if (raw === esc + "OF") {
            return ZZT.KEY_END;
        }
        if (raw === esc + "[P" || raw === esc + "OP" || raw === esc + "[11~" || raw === esc + "[[A") {
            return ZZT.KEY_F1;
        }
        if (raw === esc + "[Q" || raw === esc + "OQ" || raw === esc + "[12~" || raw === esc + "[[B") {
            return ZZT.KEY_F2;
        }
        if (raw === esc + "[R" || raw === esc + "OR" || raw === esc + "[13~" || raw === esc + "[[C") {
            return ZZT.KEY_F3;
        }
        if (raw === esc + "[S" || raw === esc + "OS" || raw === esc + "[14~" || raw === esc + "[[D") {
            return ZZT.KEY_F4;
        }
        if (raw === esc + "[15~") {
            return ZZT.KEY_F5;
        }
        if (raw === esc + "[17~") {
            return ZZT.KEY_F6;
        }
        if (raw === esc + "[18~") {
            return ZZT.KEY_F7;
        }
        if (raw === esc + "[19~") {
            return ZZT.KEY_F8;
        }
        if (raw === esc + "[20~") {
            return ZZT.KEY_F9;
        }
        if (raw === esc + "[21~") {
            return ZZT.KEY_F10;
        }
        if (raw.length >= 3 && raw.charAt(0) === esc && raw.charAt(1) === "[") {
            finalChar = raw.charAt(raw.length - 1);
            if (finalChar === "A") {
                return ZZT.KEY_UP;
            }
            if (finalChar === "B") {
                return ZZT.KEY_DOWN;
            }
            if (finalChar === "C") {
                return ZZT.KEY_RIGHT;
            }
            if (finalChar === "D") {
                return ZZT.KEY_LEFT;
            }
            if (finalChar === "H") {
                return ZZT.KEY_HOME;
            }
            if (finalChar === "F") {
                return ZZT.KEY_END;
            }
            if (finalChar === "~") {
                numberPart = raw.slice(2, raw.length - 1);
                semicolonPos = numberPart.indexOf(";");
                if (semicolonPos >= 0) {
                    numberPart = numberPart.slice(0, semicolonPos);
                }
                tildeCode = parseInt(numberPart, 10);
                if (tildeCode === 1) {
                    return ZZT.KEY_HOME;
                }
                if (tildeCode === 2) {
                    return ZZT.KEY_INSERT;
                }
                if (tildeCode === 3) {
                    return ZZT.KEY_DELETE;
                }
                if (tildeCode === 4) {
                    return ZZT.KEY_END;
                }
                if (tildeCode === 5) {
                    return ZZT.KEY_PAGE_UP;
                }
                if (tildeCode === 6) {
                    return ZZT.KEY_PAGE_DOWN;
                }
                if (tildeCode === 11) {
                    return ZZT.KEY_F1;
                }
                if (tildeCode === 12) {
                    return ZZT.KEY_F2;
                }
                if (tildeCode === 13) {
                    return ZZT.KEY_F3;
                }
                if (tildeCode === 14) {
                    return ZZT.KEY_F4;
                }
                if (tildeCode === 15) {
                    return ZZT.KEY_F5;
                }
                if (tildeCode === 17) {
                    return ZZT.KEY_F6;
                }
                if (tildeCode === 18) {
                    return ZZT.KEY_F7;
                }
                if (tildeCode === 19) {
                    return ZZT.KEY_F8;
                }
                if (tildeCode === 20) {
                    return ZZT.KEY_F9;
                }
                if (tildeCode === 21) {
                    return ZZT.KEY_F10;
                }
            }
        }
        if (raw.length >= 3 && raw.charAt(0) === esc && raw.charAt(1) === "O") {
            finalChar = raw.charAt(raw.length - 1);
            if (finalChar === "A") {
                return ZZT.KEY_UP;
            }
            if (finalChar === "B") {
                return ZZT.KEY_DOWN;
            }
            if (finalChar === "C") {
                return ZZT.KEY_RIGHT;
            }
            if (finalChar === "D") {
                return ZZT.KEY_LEFT;
            }
            if (finalChar === "H") {
                return ZZT.KEY_HOME;
            }
            if (finalChar === "F") {
                return ZZT.KEY_END;
            }
            if (finalChar === "P") {
                return ZZT.KEY_F1;
            }
            if (finalChar === "Q") {
                return ZZT.KEY_F2;
            }
            if (finalChar === "R") {
                return ZZT.KEY_F3;
            }
            if (finalChar === "S") {
                return ZZT.KEY_F4;
            }
        }
        // DOS/BIOS-style extended key returns (NUL + scan code)
        if (raw.charAt(0) === nul && raw.length >= 2) {
            second = raw.charAt(1);
            if ((second.charCodeAt(0) & 0xff) === 25) {
                return ZZT.KEY_ALT_P;
            }
            if (second === "H") {
                return ZZT.KEY_UP;
            }
            if (second === "P") {
                return ZZT.KEY_DOWN;
            }
            if (second === "K") {
                return ZZT.KEY_LEFT;
            }
            if (second === "M") {
                return ZZT.KEY_RIGHT;
            }
            if (second === "G") {
                return ZZT.KEY_HOME;
            }
            if (second === "O") {
                return ZZT.KEY_END;
            }
            if (second === "R") {
                return ZZT.KEY_INSERT;
            }
            if (second === "S") {
                return ZZT.KEY_DELETE;
            }
            if (second === "I") {
                return ZZT.KEY_PAGE_UP;
            }
            if (second === "Q") {
                return ZZT.KEY_PAGE_DOWN;
            }
            if (second === ";") {
                return ZZT.KEY_F1;
            }
            if (second === "<") {
                return ZZT.KEY_F2;
            }
            if (second === "=") {
                return ZZT.KEY_F3;
            }
            if (second === ">") {
                return ZZT.KEY_F4;
            }
            if (second === "?") {
                return ZZT.KEY_F5;
            }
            if (second === "@") {
                return ZZT.KEY_F6;
            }
            if (second === "A") {
                return ZZT.KEY_F7;
            }
            if (second === "B") {
                return ZZT.KEY_F8;
            }
            if (second === "C") {
                return ZZT.KEY_F9;
            }
            if (second === "D") {
                return ZZT.KEY_F10;
            }
        }
        return raw.charAt(0);
    }
    function isAnsiTerminator(ch) {
        return (ch >= "A" && ch <= "Z") || (ch >= "a" && ch <= "z") || ch === "~";
    }
    function consumeRawBuffer(forceEscape) {
        var nul = String.fromCharCode(0);
        var esc = ZZT.KEY_ESCAPE;
        var ch;
        var i;
        var sequence;
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
    function readKeyFromConsole(waitTimeout) {
        var raw;
        var extraRaw;
        var readCount;
        var key;
        var nul = String.fromCharCode(0);
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
        if (InputRawBuffer === ZZT.KEY_ESCAPE && typeof console.inkey === "function") {
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
    function normalizeKey(raw) {
        return decodeExtendedKey(normalizeRaw(raw));
    }
    function hasInteractiveConsole() {
        return typeof console !== "undefined" && typeof console.inkey === "function";
    }
    function applyDirectionalInput(key) {
        if (key === ZZT.KEY_UP) {
            ZZT.InputDeltaX = 0;
            ZZT.InputDeltaY = -1;
        }
        else if (key === ZZT.KEY_LEFT) {
            ZZT.InputDeltaX = -1;
            ZZT.InputDeltaY = 0;
        }
        else if (key === ZZT.KEY_RIGHT) {
            ZZT.InputDeltaX = 1;
            ZZT.InputDeltaY = 0;
        }
        else if (key === ZZT.KEY_DOWN) {
            ZZT.InputDeltaX = 0;
            ZZT.InputDeltaY = 1;
        }
    }
    function captureDirectionalFireInput(key) {
        if (key === "8") {
            ZZT.InputFireDirX = 0;
            ZZT.InputFireDirY = -1;
            ZZT.InputFirePressed = true;
            return true;
        }
        if (key === "4") {
            ZZT.InputFireDirX = -1;
            ZZT.InputFireDirY = 0;
            ZZT.InputFirePressed = true;
            return true;
        }
        if (key === "6") {
            ZZT.InputFireDirX = 1;
            ZZT.InputFireDirY = 0;
            ZZT.InputFirePressed = true;
            return true;
        }
        if (key === "2") {
            ZZT.InputFireDirX = 0;
            ZZT.InputFireDirY = 1;
            ZZT.InputFirePressed = true;
            return true;
        }
        return false;
    }
    function resetTickInputState() {
        ZZT.InputDeltaX = 0;
        ZZT.InputDeltaY = 0;
        ZZT.InputShiftPressed = false;
        ZZT.InputJoystickMoved = false;
    }
    function InputInitDevices() {
        if (typeof console !== "undefined") {
            if (InputOldCtrlKeyPassthru === null && typeof console.ctrlkey_passthru !== "undefined") {
                InputOldCtrlKeyPassthru = console.ctrlkey_passthru;
            }
            // Keep control keys flowing to the game while running under Synchronet.
            console.ctrlkey_passthru = "+ACGKLOPQRTUVWXYZ_";
        }
        ZZT.InputJoystickEnabled = false;
        ZZT.InputMouseEnabled = false;
        ZZT.InputJoystickMoved = false;
        ZZT.InputShiftPressed = false;
        ZZT.InputShiftAccepted = false;
        ZZT.InputDeltaX = 0;
        ZZT.InputDeltaY = 0;
        ZZT.InputLastDeltaX = 0;
        ZZT.InputLastDeltaY = 0;
        ZZT.InputKeyPressed = String.fromCharCode(0);
        ZZT.InputFirePressed = false;
        ZZT.InputFireDirX = 0;
        ZZT.InputFireDirY = 0;
        ZZT.InputTorchPressed = false;
        InputRawBuffer = "";
    }
    ZZT.InputInitDevices = InputInitDevices;
    function InputRestoreDevices() {
        if (typeof console !== "undefined" && InputOldCtrlKeyPassthru !== null) {
            console.ctrlkey_passthru = InputOldCtrlKeyPassthru;
            InputOldCtrlKeyPassthru = null;
        }
    }
    ZZT.InputRestoreDevices = InputRestoreDevices;
    function InputUpdate() {
        var key;
        ZZT.SoundUpdate();
        resetTickInputState();
        key = String.fromCharCode(0);
        if (hasInteractiveConsole() && typeof console.inkey === "function") {
            key = readKeyFromConsole(0);
        }
        ZZT.InputKeyPressed = key;
        if (key === " ") {
            ZZT.InputFirePressed = true;
        }
        captureDirectionalFireInput(key);
        if (key.toUpperCase() === "T") {
            ZZT.InputTorchPressed = true;
        }
        applyDirectionalInput(key);
        if (ZZT.InputDeltaX !== 0 || ZZT.InputDeltaY !== 0) {
            ZZT.InputLastDeltaX = ZZT.InputDeltaX;
            ZZT.InputLastDeltaY = ZZT.InputDeltaY;
        }
    }
    ZZT.InputUpdate = InputUpdate;
    function InputReadWaitKey() {
        var key = String.fromCharCode(0);
        resetTickInputState();
        ZZT.InputShiftAccepted = false;
        if (hasInteractiveConsole()) {
            if (typeof console.getkey === "function") {
                key = normalizeKey(console.getkey(inputReadMode()));
            }
            else if (typeof console.inkey === "function") {
                while (key === String.fromCharCode(0) && !ZZT.runtime.isTerminated()) {
                    ZZT.SoundUpdate();
                    key = readKeyFromConsole(25);
                    if (key === String.fromCharCode(0) && typeof mswait === "function") {
                        mswait(10);
                    }
                }
            }
        }
        else {
            // Non-Synchronet fallback: return ESC to avoid blocking in local tests.
            key = ZZT.KEY_ESCAPE;
        }
        ZZT.InputKeyPressed = key;
        if (key === " ") {
            ZZT.InputFirePressed = true;
        }
        if (key.toUpperCase() === "T") {
            ZZT.InputTorchPressed = true;
        }
        applyDirectionalInput(key);
        if (ZZT.InputDeltaX !== 0 || ZZT.InputDeltaY !== 0) {
            ZZT.InputLastDeltaX = ZZT.InputDeltaX;
            ZZT.InputLastDeltaY = ZZT.InputDeltaY;
        }
    }
    ZZT.InputReadWaitKey = InputReadWaitKey;
    function InputConfigure() {
        return true;
    }
    ZZT.InputConfigure = InputConfigure;
})(ZZT || (ZZT = {}));
var ZZT;
(function (ZZT) {
    ZZT.PORT_CGA_PALETTE = 0x03d9;
    ZZT.VideoMonochrome = false;
    ZZT.VideoColumns = 80;
    ZZT.VideoRows = 25;
    ZZT.VideoBorderColor = 1;
    ZZT.VideoCursorVisible = true;
    var VideoShadowChars = [];
    var VideoShadowAttrs = [];
    function mapConsumedControlCode(code) {
        /* Some CP437 control-range bytes are consumed by terminal parsers
           (BEL/BS/TAB/LF/FF/CR/ESC). Substitute visually-close printable
           bytes so gameplay UI remains stable in web and ANSI clients. */
        switch (code) {
            case 7: return 249; /* bullet -> middle dot */
            case 8: return 248; /* inverse bullet -> degree */
            case 9: return 248; /* circle -> degree */
            case 10: return 248; /* dotted circle (door) -> degree */
            case 12: return 11; /* female (key) -> male symbol */
            case 13: return 14; /* single note -> double note */
            case 27: return 17; /* left arrow -> left-pointing triangle */
            default: return code;
        }
    }
    function translateCp437Text(text) {
        var i;
        var code;
        var translated = "";
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
    function hasConsoleOutput() {
        return typeof console !== "undefined" && typeof console.gotoxy === "function";
    }
    function resetVideoShadow() {
        var x;
        var y;
        VideoShadowChars = [];
        VideoShadowAttrs = [];
        for (y = 0; y < ZZT.VideoRows; y += 1) {
            VideoShadowChars[y] = [];
            VideoShadowAttrs[y] = [];
            for (x = 0; x < ZZT.VideoColumns; x += 1) {
                VideoShadowChars[y][x] = "\u0000";
                VideoShadowAttrs[y][x] = -1;
            }
        }
    }
    function ensureVideoShadow() {
        if (VideoShadowChars.length !== ZZT.VideoRows) {
            resetVideoShadow();
            return;
        }
        if (ZZT.VideoRows > 0 && VideoShadowChars[0].length !== ZZT.VideoColumns) {
            resetVideoShadow();
        }
    }
    function writeRaw(text) {
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
    function VideoConfigure() {
        ZZT.VideoMonochrome = false;
        if (typeof console !== "undefined" && typeof console.screen_columns === "number" && console.screen_columns > 0) {
            ZZT.VideoColumns = console.screen_columns;
        }
        if (typeof console !== "undefined" && typeof console.screen_rows === "number" && console.screen_rows > 0) {
            ZZT.VideoRows = console.screen_rows;
        }
        resetVideoShadow();
        return true;
    }
    ZZT.VideoConfigure = VideoConfigure;
    function VideoInstall(columns, borderColor) {
        ZZT.VideoColumns = columns;
        ZZT.VideoBorderColor = borderColor;
        resetVideoShadow();
        if (typeof console !== "undefined" && typeof console.clear === "function") {
            console.clear();
        }
        else {
            ZZT.runtime.clearScreen();
        }
    }
    ZZT.VideoInstall = VideoInstall;
    function VideoUninstall() {
        if (typeof console !== "undefined" && typeof console.attributes === "number") {
            console.attributes = 7;
        }
        resetVideoShadow();
    }
    ZZT.VideoUninstall = VideoUninstall;
    function VideoWriteText(x, y, color, text) {
        var prevAttr = 7;
        var outputText;
        var i;
        var ch;
        var screenX;
        var segmentStart;
        var segmentText;
        var currentAttr = color & 0xff;
        var maxLen;
        if (text.length <= 0) {
            return;
        }
        outputText = translateCp437Text(text);
        if (outputText.length <= 0) {
            return;
        }
        if (hasConsoleOutput() && typeof console.gotoxy === "function") {
            if (y < 0 || y >= ZZT.VideoRows) {
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
                    if (screenX < 0 || screenX >= ZZT.VideoColumns) {
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
                    if (screenX < 0 || screenX >= ZZT.VideoColumns) {
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
    ZZT.VideoWriteText = VideoWriteText;
    function VideoMove(x, y, chars, data, toVideo) {
        var i;
        var screenX;
        var ch;
        var attr;
        var moveData;
        var savedText;
        var savedAttrs;
        var prevAttr = 7;
        var activeAttr = -1;
        if (chars <= 0 || y < 0 || y >= ZZT.VideoRows || data === null || typeof data !== "object") {
            return;
        }
        ensureVideoShadow();
        moveData = data;
        if (!toVideo) {
            savedText = "";
            savedAttrs = [];
            for (i = 0; i < chars; i += 1) {
                screenX = x + i;
                if (screenX < 0 || screenX >= ZZT.VideoColumns) {
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
            if (screenX < 0 || screenX >= ZZT.VideoColumns) {
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
    ZZT.VideoMove = VideoMove;
    function VideoHideCursor() {
        ZZT.VideoCursorVisible = false;
    }
    ZZT.VideoHideCursor = VideoHideCursor;
    function VideoShowCursor() {
        ZZT.VideoCursorVisible = true;
    }
    ZZT.VideoShowCursor = VideoShowCursor;
})(ZZT || (ZZT = {}));
var ZZT;
(function (ZZT) {
    ZZT.OrderPrintId = "";
    ZZT.TextWindowInited = false;
    ZZT.TextWindowRejected = false;
    ZZT.TextWindowX = 5;
    ZZT.TextWindowY = 3;
    ZZT.TextWindowWidth = 50;
    ZZT.TextWindowHeight = 18;
    var MAX_TEXT_WINDOW_LINES = 1024;
    var MAX_RESOURCE_DATA_FILES = 24;
    var TEXT_WINDOW_ANIM_SPEED = 25;
    var ResourceDataHeaderLoaded = false;
    var ResourceDataEntryCount = 0;
    var ResourceDataNames = [];
    var ResourceDataOffsets = [];
    var TextWindowStrInnerEmpty = "";
    var TextWindowStrText = "";
    var TextWindowStrInnerLine = "";
    var TextWindowStrTop = "";
    var TextWindowStrBottom = "";
    var TextWindowStrSep = "";
    var TextWindowStrInnerSep = "";
    var TextWindowStrInnerArrows = "";
    function upperString(value) {
        return value.toUpperCase();
    }
    function clamp(value, min, max) {
        if (value < min) {
            return min;
        }
        if (value > max) {
            return max;
        }
        return value;
    }
    function getLine(state, linePos) {
        if (linePos < 1 || linePos > state.LineCount) {
            return "";
        }
        return state.Lines[linePos - 1];
    }
    function setLine(state, linePos, value) {
        if (linePos < 1 || linePos > state.LineCount) {
            return;
        }
        state.Lines[linePos - 1] = value;
    }
    function textWindowHasExtension(filename) {
        var i;
        for (i = 0; i < filename.length; i += 1) {
            if (filename.charAt(i) === ".") {
                return true;
            }
        }
        return false;
    }
    function textPathHasSeparator(path) {
        return path.indexOf("/") >= 0 || path.indexOf("\\") >= 0;
    }
    function textPathIsAbsolute(path) {
        return (path.length > 0 && (path.charAt(0) === "/" || path.charAt(0) === "\\")) || path.indexOf(":") >= 0;
    }
    function pushUniqueTextPath(paths, path) {
        var i;
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
    function getTextNameVariants(filename) {
        var variants = [];
        var dotPos = filename.lastIndexOf(".");
        var base;
        var ext;
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
    function loadTextFileLinesWithFallback(filename) {
        var searchPaths = [];
        var variants = getTextNameVariants(filename);
        var i;
        var nameVariant;
        var lines = [];
        var hasSep;
        var isAbs;
        for (i = 0; i < variants.length; i += 1) {
            nameVariant = variants[i];
            hasSep = textPathHasSeparator(nameVariant);
            isAbs = textPathIsAbsolute(nameVariant);
            if (isAbs) {
                pushUniqueTextPath(searchPaths, nameVariant);
                continue;
            }
            if (hasSep) {
                pushUniqueTextPath(searchPaths, ZZT.execPath(nameVariant));
                pushUniqueTextPath(searchPaths, ZZT.execPath("../" + nameVariant));
                pushUniqueTextPath(searchPaths, nameVariant);
            }
            else {
                pushUniqueTextPath(searchPaths, ZZT.execPath(nameVariant));
                pushUniqueTextPath(searchPaths, ZZT.execPath("DOCS/" + nameVariant));
                pushUniqueTextPath(searchPaths, ZZT.execPath("docs/" + nameVariant));
                pushUniqueTextPath(searchPaths, ZZT.execPath("reconstruction-of-zzt-master/DOCS/" + nameVariant));
                pushUniqueTextPath(searchPaths, nameVariant);
                pushUniqueTextPath(searchPaths, "DOCS/" + nameVariant);
                pushUniqueTextPath(searchPaths, "docs/" + nameVariant);
            }
        }
        for (i = 0; i < searchPaths.length; i += 1) {
            lines = ZZT.runtime.readTextFileLines(searchPaths[i]);
            if (lines.length > 0) {
                return lines;
            }
        }
        return [];
    }
    function textWindowDrawTitle(color, title) {
        ZZT.VideoWriteText(ZZT.TextWindowX + 2, ZZT.TextWindowY + 1, color, TextWindowStrInnerEmpty);
        ZZT.VideoWriteText(ZZT.TextWindowX + Math.floor((ZZT.TextWindowWidth - title.length) / 2), ZZT.TextWindowY + 1, color, title);
    }
    function textWindowPrint(_state) {
        // Printer output is not implemented in the Synchronet JS port.
    }
    function bytesToString(bytes, offset, length) {
        var end = offset + length;
        var i;
        var out = "";
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
    function readInt16LE(bytes, offset) {
        var value;
        if (offset + 1 >= bytes.length) {
            return 0;
        }
        value = (bytes[offset] & 0xff) | ((bytes[offset + 1] & 0xff) << 8);
        if (value >= 0x8000) {
            value -= 0x10000;
        }
        return value;
    }
    function readUInt32LE(bytes, offset) {
        if (offset + 3 >= bytes.length) {
            return 0;
        }
        return ((bytes[offset] & 0xff) +
            ((bytes[offset + 1] & 0xff) * 0x100) +
            ((bytes[offset + 2] & 0xff) * 0x10000) +
            ((bytes[offset + 3] & 0xff) * 0x1000000));
    }
    function readPascalString(bytes, offset, maxLen) {
        var length;
        if (offset >= bytes.length) {
            return "";
        }
        length = bytes[offset] & 0xff;
        if (length > maxLen) {
            length = maxLen;
        }
        return bytesToString(bytes, offset + 1, length);
    }
    function loadResourceDataHeader() {
        var bytes;
        var offset;
        var i;
        var count;
        var name;
        var resourcePath = ZZT.execPath(ZZT.ResourceDataFileName);
        if (ResourceDataHeaderLoaded) {
            return;
        }
        ResourceDataHeaderLoaded = true;
        ResourceDataEntryCount = -1;
        ResourceDataNames = [];
        ResourceDataOffsets = [];
        bytes = ZZT.runtime.readBinaryFile(resourcePath);
        if (bytes === null) {
            bytes = ZZT.runtime.readBinaryFile(ZZT.ResourceDataFileName);
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
    function findResourceEntryPos(filename) {
        var i;
        var upFilename = upperString(filename);
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
    function readResourceEntryLines(entryPos, includeTerminatorBlank) {
        var lines = [];
        var bytes;
        var offset;
        var lineLen;
        var line;
        var resourcePath = ZZT.execPath(ZZT.ResourceDataFileName);
        bytes = ZZT.runtime.readBinaryFile(resourcePath);
        if (bytes === null) {
            bytes = ZZT.runtime.readBinaryFile(ZZT.ResourceDataFileName);
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
    function textWindowDrawLine(state, linePos, withoutFormatting, viewingFile) {
        var lineY = ((ZZT.TextWindowY + linePos) - state.LinePos) + Math.floor(ZZT.TextWindowHeight / 2) + 1;
        var textOffset = 0;
        var textColor = 0x1e;
        var textX = ZZT.TextWindowX + 4;
        var line = getLine(state, linePos);
        var splitPos;
        var writeText;
        if (linePos === state.LinePos) {
            ZZT.VideoWriteText(ZZT.TextWindowX + 2, lineY, 0x1c, TextWindowStrInnerArrows);
        }
        else {
            ZZT.VideoWriteText(ZZT.TextWindowX + 2, lineY, 0x1e, TextWindowStrInnerEmpty);
        }
        if (linePos > 0 && linePos <= state.LineCount) {
            if (withoutFormatting) {
                ZZT.VideoWriteText(ZZT.TextWindowX + 4, lineY, 0x1e, line);
                return;
            }
            if (line.length > 0) {
                if (line.charAt(0) === "!") {
                    splitPos = line.indexOf(";");
                    if (splitPos >= 0) {
                        textOffset = splitPos + 1;
                    }
                    else {
                        textOffset = 0;
                    }
                    ZZT.VideoWriteText(textX + 2, lineY, 0x1d, String.fromCharCode(16));
                    textX += 5;
                    textColor = 0x1f;
                }
                else if (line.charAt(0) === ":") {
                    splitPos = line.indexOf(";");
                    if (splitPos >= 0) {
                        textOffset = splitPos + 1;
                    }
                    else {
                        textOffset = 0;
                    }
                    textColor = 0x1f;
                }
                else if (line.charAt(0) === "$") {
                    textOffset = 1;
                    textColor = 0x1f;
                    textX = (textX - 4) + Math.floor((ZZT.TextWindowWidth - line.length) / 2);
                }
            }
            writeText = line.slice(textOffset);
            if (writeText.length > 0) {
                ZZT.VideoWriteText(textX, lineY, textColor, writeText);
            }
        }
        else if (linePos === 0 || linePos === (state.LineCount + 1)) {
            ZZT.VideoWriteText(ZZT.TextWindowX + 2, lineY, 0x1e, TextWindowStrInnerSep);
        }
        else if (linePos === -4 && viewingFile) {
            ZZT.VideoWriteText(ZZT.TextWindowX + 2, lineY, 0x1a, "   Use            to view text,");
            ZZT.VideoWriteText(ZZT.TextWindowX + 9, lineY, 0x1f, String.fromCharCode(24) + " " + String.fromCharCode(25) + ", Enter");
        }
        else if (linePos === -3 && viewingFile) {
            ZZT.VideoWriteText(ZZT.TextWindowX + 3, lineY, 0x1a, "                 to print.");
            ZZT.VideoWriteText(ZZT.TextWindowX + 14, lineY, 0x1f, "Alt-P");
        }
    }
    function TextWindowInitState(state) {
        state.LineCount = 0;
        state.LinePos = 1;
        state.Lines = [];
        state.Hyperlink = "";
        state.LoadedFilename = "";
        state.ScreenCopy = [];
    }
    ZZT.TextWindowInitState = TextWindowInitState;
    function TextWindowDrawOpen(state) {
        var i;
        var iy;
        var copyRows = ZZT.TextWindowHeight + 1;
        if (state.ScreenCopy.length < copyRows + 1) {
            state.ScreenCopy = [];
        }
        for (iy = 1; iy <= copyRows; iy += 1) {
            state.ScreenCopy[iy] = {};
            ZZT.VideoMove(ZZT.TextWindowX, iy + ZZT.TextWindowY - 1, ZZT.TextWindowWidth, state.ScreenCopy[iy], false);
        }
        for (iy = Math.floor(ZZT.TextWindowHeight / 2); iy >= 0; iy -= 1) {
            ZZT.VideoWriteText(ZZT.TextWindowX, ZZT.TextWindowY + iy + 1, 0x0f, TextWindowStrText);
            ZZT.VideoWriteText(ZZT.TextWindowX, ZZT.TextWindowY + ZZT.TextWindowHeight - iy - 1, 0x0f, TextWindowStrText);
            ZZT.VideoWriteText(ZZT.TextWindowX, ZZT.TextWindowY + iy, 0x0f, TextWindowStrTop);
            ZZT.VideoWriteText(ZZT.TextWindowX, ZZT.TextWindowY + ZZT.TextWindowHeight - iy, 0x0f, TextWindowStrBottom);
            if (typeof mswait === "function") {
                mswait(TEXT_WINDOW_ANIM_SPEED);
            }
        }
        for (i = 1; i <= ZZT.TextWindowHeight - 1; i += 1) {
            ZZT.VideoWriteText(ZZT.TextWindowX, ZZT.TextWindowY + i, 0x0f, TextWindowStrText);
        }
        ZZT.VideoWriteText(ZZT.TextWindowX, ZZT.TextWindowY + 2, 0x0f, TextWindowStrSep);
        textWindowDrawTitle(0x1e, state.Title);
    }
    ZZT.TextWindowDrawOpen = TextWindowDrawOpen;
    function TextWindowDrawClose(state) {
        var iy;
        var bottomCopyRow;
        if (state.ScreenCopy.length <= 0) {
            ZZT.TransitionDrawToBoard();
            ZZT.GameUpdateSidebar();
            return;
        }
        for (iy = 0; iy <= Math.floor(ZZT.TextWindowHeight / 2); iy += 1) {
            ZZT.VideoWriteText(ZZT.TextWindowX, ZZT.TextWindowY + iy, 0x0f, TextWindowStrTop);
            ZZT.VideoWriteText(ZZT.TextWindowX, ZZT.TextWindowY + ZZT.TextWindowHeight - iy, 0x0f, TextWindowStrBottom);
            if (typeof mswait === "function") {
                mswait(Math.floor((TEXT_WINDOW_ANIM_SPEED * 3) / 4));
            }
            if (state.ScreenCopy[iy + 1] !== undefined) {
                ZZT.VideoMove(ZZT.TextWindowX, ZZT.TextWindowY + iy, ZZT.TextWindowWidth, state.ScreenCopy[iy + 1], true);
            }
            bottomCopyRow = (ZZT.TextWindowHeight - iy) + 1;
            if (state.ScreenCopy[bottomCopyRow] !== undefined) {
                ZZT.VideoMove(ZZT.TextWindowX, ZZT.TextWindowY + ZZT.TextWindowHeight - iy, ZZT.TextWindowWidth, state.ScreenCopy[bottomCopyRow], true);
            }
        }
    }
    ZZT.TextWindowDrawClose = TextWindowDrawClose;
    function TextWindowDraw(state, withoutFormatting, viewingFile) {
        var i;
        for (i = 0; i <= (ZZT.TextWindowHeight - 4); i += 1) {
            textWindowDrawLine(state, state.LinePos - Math.floor(ZZT.TextWindowHeight / 2) + i + 2, withoutFormatting, viewingFile);
        }
        textWindowDrawTitle(0x1e, state.Title);
    }
    ZZT.TextWindowDraw = TextWindowDraw;
    function TextWindowAppend(state, line) {
        state.Lines.push(line);
        state.LineCount = state.Lines.length;
    }
    ZZT.TextWindowAppend = TextWindowAppend;
    function TextWindowFree(state) {
        state.Lines = [];
        state.LineCount = 0;
        state.LoadedFilename = "";
    }
    ZZT.TextWindowFree = TextWindowFree;
    function TextWindowOpenFile(filename, state) {
        var loadedFilename = filename;
        var lines = [];
        var i;
        var entryPos = 0;
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
        }
        else {
            lines = readResourceEntryLines(entryPos, true);
        }
        for (i = 0; i < lines.length; i += 1) {
            TextWindowAppend(state, lines[i]);
        }
    }
    ZZT.TextWindowOpenFile = TextWindowOpenFile;
    function ResourceDataLoadLines(filename, includeTerminatorBlank) {
        var loadedFilename = filename;
        var entryPos;
        if (!textWindowHasExtension(loadedFilename)) {
            loadedFilename += ".HLP";
        }
        entryPos = findResourceEntryPos(loadedFilename);
        if (entryPos <= 0) {
            return [];
        }
        return readResourceEntryLines(entryPos, includeTerminatorBlank);
    }
    ZZT.ResourceDataLoadLines = ResourceDataLoadLines;
    function TextWindowSaveFile(filename, state) {
        var file;
        var i;
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
            }
            finally {
                file.close();
            }
        }
        catch (_err) {
            return;
        }
    }
    ZZT.TextWindowSaveFile = TextWindowSaveFile;
    function TextWindowEdit(state) {
        var newLinePos;
        var insertMode;
        var charPos;
        var i;
        var line;
        var cursorY = ZZT.TextWindowY + Math.floor(ZZT.TextWindowHeight / 2) + 1;
        var keyCode;
        var ch;
        function deleteCurrLine() {
            var idx;
            if (state.LineCount > 1) {
                for (idx = state.LinePos + 1; idx <= state.LineCount; idx += 1) {
                    state.Lines[idx - 2] = state.Lines[idx - 1];
                }
                state.Lines.pop();
                state.LineCount -= 1;
                if (state.LinePos > state.LineCount) {
                    newLinePos = state.LineCount;
                }
                else {
                    TextWindowDraw(state, true, false);
                }
            }
            else if (state.LineCount === 1) {
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
        while (!ZZT.runtime.isTerminated()) {
            ZZT.VideoWriteText(77, 14, 0x1e, insertMode ? "on " : "off");
            line = getLine(state, state.LinePos);
            if (charPos > line.length + 1) {
                charPos = line.length + 1;
            }
            if (charPos >= line.length + 1) {
                ZZT.VideoWriteText(charPos + ZZT.TextWindowX + 3, cursorY, 0x70, " ");
            }
            else {
                ZZT.VideoWriteText(charPos + ZZT.TextWindowX + 3, cursorY, 0x70, line.charAt(charPos - 1));
            }
            ZZT.InputReadWaitKey();
            newLinePos = state.LinePos;
            line = getLine(state, state.LinePos);
            ch = ZZT.InputKeyPressed;
            keyCode = ch.length > 0 ? (ch.charCodeAt(0) & 0xff) : 0;
            if (ch === ZZT.KEY_UP) {
                newLinePos -= 1;
            }
            else if (ch === ZZT.KEY_DOWN) {
                newLinePos += 1;
            }
            else if (ch === ZZT.KEY_PAGE_UP) {
                newLinePos -= ZZT.TextWindowHeight - 4;
            }
            else if (ch === ZZT.KEY_PAGE_DOWN) {
                newLinePos += ZZT.TextWindowHeight - 4;
            }
            else if (ch === ZZT.KEY_RIGHT) {
                charPos += 1;
                if (charPos > line.length + 1) {
                    charPos = 1;
                    newLinePos += 1;
                }
            }
            else if (ch === ZZT.KEY_LEFT) {
                charPos -= 1;
                if (charPos < 1) {
                    charPos = ZZT.TextWindowWidth;
                    newLinePos -= 1;
                }
            }
            else if (ch === ZZT.KEY_ENTER) {
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
            }
            else if (ch === ZZT.KEY_BACKSPACE) {
                if (charPos > 1) {
                    setLine(state, state.LinePos, line.slice(0, charPos - 2) + line.slice(charPos - 1));
                    charPos -= 1;
                }
                else if (line.length === 0) {
                    deleteCurrLine();
                    newLinePos -= 1;
                    charPos = ZZT.TextWindowWidth;
                }
            }
            else if (ch === ZZT.KEY_INSERT) {
                insertMode = !insertMode;
            }
            else if (ch === ZZT.KEY_DELETE) {
                setLine(state, state.LinePos, line.slice(0, charPos - 1) + line.slice(charPos));
            }
            else if (ch === ZZT.KEY_CTRL_Y) {
                deleteCurrLine();
            }
            else if (keyCode >= 32 && charPos < (ZZT.TextWindowWidth - 7)) {
                if (!insertMode) {
                    if (charPos > line.length) {
                        setLine(state, state.LinePos, line + ch);
                    }
                    else {
                        setLine(state, state.LinePos, line.slice(0, charPos - 1) + ch + line.slice(charPos));
                    }
                    charPos += 1;
                }
                else if (line.length < (ZZT.TextWindowWidth - 8)) {
                    setLine(state, state.LinePos, line.slice(0, charPos - 1) + ch + line.slice(charPos - 1));
                    charPos += 1;
                }
            }
            if (newLinePos < 1) {
                newLinePos = 1;
            }
            else if (newLinePos > state.LineCount) {
                newLinePos = state.LineCount;
            }
            if (newLinePos !== state.LinePos) {
                state.LinePos = newLinePos;
                TextWindowDraw(state, true, false);
            }
            else {
                textWindowDrawLine(state, state.LinePos, true, false);
            }
            if (ZZT.InputKeyPressed === ZZT.KEY_ESCAPE) {
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
    ZZT.TextWindowEdit = TextWindowEdit;
    function TextWindowSelect(state, hyperlinkAsSelect, viewingFile) {
        var newLinePos;
        var iLine;
        var line;
        var pointerStr;
        var splitPos;
        var key;
        var titleHint;
        var keyCode;
        var typeAhead = "";
        var typeAheadTickMs = 0;
        function startsWithPrefix(value, prefix) {
            if (prefix.length > value.length) {
                return false;
            }
            return value.slice(0, prefix.length) === prefix;
        }
        function normalizeTypeAheadText(value) {
            var text = value;
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
        function findTypeAheadMatch(prefix) {
            var startLine = state.LinePos + 1;
            var pass;
            var lineNo;
            var text;
            var fromLine;
            var toLine;
            if (state.LineCount <= 0 || prefix.length <= 0) {
                return 0;
            }
            for (pass = 0; pass < 2; pass += 1) {
                if (pass === 0) {
                    fromLine = startLine;
                    toLine = state.LineCount;
                }
                else {
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
        ZZT.TextWindowRejected = false;
        state.Hyperlink = "";
        TextWindowDraw(state, false, viewingFile);
        while (!ZZT.runtime.isTerminated()) {
            ZZT.InputReadWaitKey();
            key = ZZT.InputKeyPressed;
            keyCode = key.length > 0 ? (key.charCodeAt(0) & 0xff) : 0;
            newLinePos = state.LinePos;
            if (ZZT.InputDeltaY !== 0) {
                newLinePos += ZZT.InputDeltaY;
                typeAhead = "";
                typeAheadTickMs = 0;
            }
            else if (ZZT.InputShiftPressed || key === ZZT.KEY_ENTER) {
                ZZT.InputShiftAccepted = true;
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
                        ZZT.InputKeyPressed = String.fromCharCode(0);
                        ZZT.InputShiftPressed = false;
                    }
                    else if (hyperlinkAsSelect) {
                        state.Hyperlink = pointerStr;
                    }
                    else {
                        pointerStr = ":" + pointerStr;
                        for (iLine = 1; iLine <= state.LineCount; iLine += 1) {
                            line = getLine(state, iLine);
                            if (pointerStr.length <= line.length &&
                                upperString(pointerStr) === upperString(line.slice(0, pointerStr.length))) {
                                newLinePos = iLine;
                                ZZT.InputKeyPressed = String.fromCharCode(0);
                                ZZT.InputShiftPressed = false;
                                break;
                            }
                        }
                    }
                }
            }
            else {
                if (key === ZZT.KEY_PAGE_UP) {
                    newLinePos -= ZZT.TextWindowHeight - 4;
                    typeAhead = "";
                    typeAheadTickMs = 0;
                }
                else if (key === ZZT.KEY_PAGE_DOWN) {
                    newLinePos += ZZT.TextWindowHeight - 4;
                    typeAhead = "";
                    typeAheadTickMs = 0;
                }
                else if (key === ZZT.KEY_ALT_P || (viewingFile && upperString(key) === "P")) {
                    textWindowPrint(state);
                }
                else if (state.Selectable && keyCode >= 32 && keyCode <= 126) {
                    var nowMs = new Date().getTime();
                    var keyUpper = upperString(key);
                    var matchedLine;
                    if (typeAheadTickMs <= 0 || (nowMs - typeAheadTickMs) > 1200) {
                        typeAhead = keyUpper;
                    }
                    else if (typeAhead.length < 24) {
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
            }
            else {
                newLinePos = 1;
            }
            if (newLinePos !== state.LinePos) {
                state.LinePos = newLinePos;
                TextWindowDraw(state, false, viewingFile);
                line = getLine(state, state.LinePos);
                if (line.length > 0 && line.charAt(0) === "!") {
                    if (hyperlinkAsSelect) {
                        titleHint = String.fromCharCode(174) + "Press ENTER to select this" + String.fromCharCode(175);
                    }
                    else {
                        titleHint = String.fromCharCode(174) + "Press ENTER for more info" + String.fromCharCode(175);
                    }
                    textWindowDrawTitle(0x1e, titleHint);
                }
            }
            if (ZZT.InputJoystickMoved && typeof mswait === "function") {
                mswait(35);
            }
            if (ZZT.InputKeyPressed === ZZT.KEY_ESCAPE || ZZT.InputKeyPressed === ZZT.KEY_ENTER || ZZT.InputShiftPressed) {
                break;
            }
        }
        if (ZZT.InputKeyPressed === ZZT.KEY_ESCAPE) {
            ZZT.InputKeyPressed = String.fromCharCode(0);
            ZZT.TextWindowRejected = true;
        }
    }
    ZZT.TextWindowSelect = TextWindowSelect;
    function TextWindowDisplayFile(filename, title) {
        var state = {
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
    ZZT.TextWindowDisplayFile = TextWindowDisplayFile;
    function TextWindowInit(x, y, width, height) {
        var i;
        var sepPos;
        ZZT.TextWindowX = x;
        ZZT.TextWindowY = y;
        ZZT.TextWindowWidth = width;
        ZZT.TextWindowHeight = height;
        TextWindowStrInnerEmpty = "";
        TextWindowStrInnerLine = "";
        for (i = 1; i <= (ZZT.TextWindowWidth - 5); i += 1) {
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
        for (i = 1; i <= Math.floor(ZZT.TextWindowWidth / 5); i += 1) {
            sepPos = i * 5 + Math.floor((ZZT.TextWindowWidth % 5) / 2) - 1;
            if (sepPos >= 0 && sepPos < TextWindowStrInnerSep.length) {
                TextWindowStrInnerSep =
                    TextWindowStrInnerSep.slice(0, sepPos) +
                        String.fromCharCode(7) +
                        TextWindowStrInnerSep.slice(sepPos + 1);
            }
        }
        ZZT.TextWindowInited = true;
    }
    ZZT.TextWindowInit = TextWindowInit;
})(ZZT || (ZZT = {}));
var ZZT;
(function (ZZT) {
    var WORLD_FILE_HEADER_SIZE_BYTES = 512;
    var BOARD_NAME_MAX_LEN = 50;
    var BOARD_MESSAGE_MAX_LEN = 58;
    var WORLD_NAME_MAX_LEN = 20;
    var FLAG_NAME_MAX_LEN = 20;
    var BOARD_INFO_PADDING_LEN = 16;
    var WORLD_INFO_PADDING_LEN = 14;
    var HIGH_SCORE_NAME_MAX_LEN = 50;
    var HIGH_SCORE_ENTRY_BYTES = HIGH_SCORE_NAME_MAX_LEN + 3;
    var HIGH_SCORE_JSON_VERSION = 1;
    var DEFAULT_HIGH_SCORE_JSON_REL_PATH = "data/highscores.json";
    var DEFAULT_SAVE_ROOT_REL_PATH = "zzt_files/saves";
    var HIGH_SCORE_DISPLAY_NAME_MAX_LEN = 34;
    var TileBorder = ZZT.createTile(ZZT.E_NORMAL, 0x0e);
    var TileBoardEdge = ZZT.createTile(ZZT.E_BOARD_EDGE, 0x00);
    var ShowInputDebugOverlay = false;
    var NeighborDeltaX = [0, 0, -1, 1];
    var NeighborDeltaY = [-1, 1, 0, 0];
    var ActiveHighScoreEntries = [];
    function randomInt(maxExclusive) {
        if (maxExclusive <= 0) {
            return 0;
        }
        return Math.floor(Math.random() * maxExclusive);
    }
    function currentHsecs() {
        var nowMs = new Date().getTime();
        return Math.floor(nowMs / 10) % 6000;
    }
    function hasTimeElapsed(counter, duration) {
        var now = currentHsecs();
        var diff = (now - counter + 6000) % 6000;
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
    function WorldHasTimeElapsed(duration) {
        var elapsedCheck = hasTimeElapsed(ZZT.World.Info.BoardTimeHsec, duration);
        ZZT.World.Info.BoardTimeHsec = elapsedCheck.Counter;
        return elapsedCheck.Elapsed;
    }
    ZZT.WorldHasTimeElapsed = WorldHasTimeElapsed;
    function sanitizeBoardId(boardId) {
        if (boardId < 0) {
            return 0;
        }
        if (boardId > ZZT.MAX_BOARD) {
            return ZZT.MAX_BOARD;
        }
        return boardId;
    }
    function toUint8(value) {
        return value & 0xff;
    }
    function toUint16(value) {
        return value & 0xffff;
    }
    function toInt16(value) {
        var v = toUint16(value);
        if (v >= 0x8000) {
            return v - 0x10000;
        }
        return v;
    }
    function appendBytes(target, value) {
        var i;
        for (i = 0; i < value.length; i += 1) {
            target.push(toUint8(value[i]));
        }
    }
    function pushUInt8(out, value) {
        out.push(toUint8(value));
    }
    function pushUInt16(out, value) {
        var v = toUint16(value);
        out.push(v & 0xff);
        out.push((v >> 8) & 0xff);
    }
    function pushInt16(out, value) {
        pushUInt16(out, toUint16(value));
    }
    function readUInt8(cursor) {
        if (cursor.offset >= cursor.bytes.length) {
            cursor.offset += 1;
            return 0;
        }
        var value = cursor.bytes[cursor.offset] & 0xff;
        cursor.offset += 1;
        return value;
    }
    function readUInt16(cursor) {
        var lo = readUInt8(cursor);
        var hi = readUInt8(cursor);
        return lo | (hi << 8);
    }
    function readInt16(cursor) {
        return toInt16(readUInt16(cursor));
    }
    function readBytes(cursor, count) {
        var out = [];
        var i;
        for (i = 0; i < count; i += 1) {
            out.push(readUInt8(cursor));
        }
        return out;
    }
    function byteToChar(value) {
        return String.fromCharCode(toUint8(value));
    }
    function bytesToString(bytes) {
        var chars = [];
        var i;
        for (i = 0; i < bytes.length; i += 1) {
            chars.push(byteToChar(bytes[i]));
        }
        return chars.join("");
    }
    function stringToBytes(value) {
        var bytes = [];
        var i;
        for (i = 0; i < value.length; i += 1) {
            bytes.push(value.charCodeAt(i) & 0xff);
        }
        return bytes;
    }
    function writePascalString(out, value, maxLen) {
        var bytes = stringToBytes(value);
        var i;
        var writtenLen = bytes.length;
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
    function readPascalString(cursor, maxLen) {
        var len = readUInt8(cursor);
        var payload = readBytes(cursor, maxLen);
        if (len > maxLen) {
            len = maxLen;
        }
        return bytesToString(payload.slice(0, len));
    }
    function writePadding(out, padding, expectedLen) {
        var i;
        for (i = 0; i < expectedLen; i += 1) {
            if (i < padding.length) {
                pushUInt8(out, padding[i]);
            }
            else {
                pushUInt8(out, 0);
            }
        }
    }
    function readPadding(cursor, len) {
        return readBytes(cursor, len);
    }
    function resetHighScoreList() {
        var i;
        for (i = 1; i <= ZZT.HIGH_SCORE_COUNT; i += 1) {
            ZZT.HighScoreList[i].Name = "";
            ZZT.HighScoreList[i].Score = -1;
        }
    }
    function normalizeScoreText(value, maxLen) {
        var cleaned = String(value || "").replace(/[\x00-\x1f\x7f]/g, " ");
        if (cleaned.length > maxLen) {
            cleaned = cleaned.slice(0, maxLen);
        }
        return cleaned;
    }
    function trimSpaces(value) {
        return String(value || "").replace(/^\s+/, "").replace(/\s+$/, "");
    }
    function toSafePathPart(value, fallback) {
        var safe = String(value || "").toLowerCase().replace(/[^a-z0-9._-]+/g, "-");
        safe = safe.replace(/^-+/, "").replace(/-+$/, "");
        if (safe.length <= 0) {
            return fallback;
        }
        return safe;
    }
    function normalizePathCase(path) {
        return normalizeSlashes(path).toLowerCase();
    }
    function isAbsoluteFilePath(path) {
        var p = String(path || "");
        if (p.length <= 0) {
            return false;
        }
        if (p.charAt(0) === "/" || p.charAt(0) === "\\") {
            return true;
        }
        return p.indexOf(":") >= 0;
    }
    function pathJoin(base, rel) {
        var b = String(base || "");
        if (b.length > 0) {
            var tail = b.charAt(b.length - 1);
            if (tail !== "/" && tail !== "\\") {
                b += "/";
            }
        }
        return b + rel;
    }
    function pathDirname(path) {
        var normalized = normalizeSlashes(path);
        var slashPos = normalized.lastIndexOf("/");
        if (slashPos < 0) {
            return "";
        }
        return normalized.slice(0, slashPos + 1);
    }
    function ensureDirectory(path) {
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
        }
        catch (_err) {
            // Ignore and continue with final check.
        }
        return typeof file_isdir === "function" && file_isdir(path);
    }
    function ensureParentDirectory(filePath) {
        var parent = pathDirname(filePath);
        if (parent.length <= 0) {
            return true;
        }
        return ensureDirectory(parent);
    }
    function currentIsoTimestamp() {
        return new Date().toISOString();
    }
    function getCurrentUserName() {
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
    function getCurrentBbsName() {
        if (trimSpaces(ZZT.HighScoreBbsName).length > 0) {
            return normalizeScoreText(trimSpaces(ZZT.HighScoreBbsName), HIGH_SCORE_NAME_MAX_LEN);
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
    function buildHighScoreDisplayName(player, bbs) {
        var playerClean = normalizeScoreText(player, HIGH_SCORE_NAME_MAX_LEN);
        var bbsClean = normalizeScoreText(bbs, HIGH_SCORE_NAME_MAX_LEN);
        if (bbsClean.length > 0) {
            return normalizeScoreText(playerClean + " @ " + bbsClean, HIGH_SCORE_NAME_MAX_LEN);
        }
        return playerClean;
    }
    function formatHighScoreDisplayName(entry) {
        var fullName = buildHighScoreDisplayName(entry.player, entry.bbs);
        if (fullName.length > HIGH_SCORE_DISPLAY_NAME_MAX_LEN) {
            return fullName.slice(0, HIGH_SCORE_DISPLAY_NAME_MAX_LEN);
        }
        return fullName;
    }
    function normalizeHighScoreEntry(entry) {
        var scoreValue = Math.floor(entry.score);
        var playerName = normalizeScoreText(entry.player, HIGH_SCORE_NAME_MAX_LEN);
        var bbsName = normalizeScoreText(entry.bbs, HIGH_SCORE_NAME_MAX_LEN);
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
    function setActiveHighScoreEntries(entries) {
        var normalized = [];
        var i;
        var candidate;
        for (i = 0; i < entries.length; i += 1) {
            candidate = normalizeHighScoreEntry(entries[i]);
            if (candidate !== null) {
                normalized.push(candidate);
            }
        }
        normalized.sort(function (a, b) {
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
        if (normalized.length > ZZT.HIGH_SCORE_COUNT) {
            normalized = normalized.slice(0, ZZT.HIGH_SCORE_COUNT);
        }
        ActiveHighScoreEntries = normalized;
        resetHighScoreList();
        for (i = 0; i < normalized.length; i += 1) {
            ZZT.HighScoreList[i + 1].Score = normalized[i].score;
            ZZT.HighScoreList[i + 1].Name = formatHighScoreDisplayName(normalized[i]);
        }
    }
    function getActiveHighScoreEntriesFromList() {
        var entries = [];
        var i;
        var displayName;
        var sepPos;
        var player;
        var bbs;
        for (i = 1; i <= ZZT.HIGH_SCORE_COUNT; i += 1) {
            if (ZZT.HighScoreList[i].Score < 0) {
                continue;
            }
            displayName = normalizeScoreText(ZZT.HighScoreList[i].Name, HIGH_SCORE_NAME_MAX_LEN);
            sepPos = displayName.indexOf(" @ ");
            if (sepPos > 0) {
                player = displayName.slice(0, sepPos);
                bbs = displayName.slice(sepPos + 3);
            }
            else {
                player = displayName;
                bbs = "";
            }
            entries.push({
                player: player,
                bbs: bbs,
                score: ZZT.HighScoreList[i].Score,
                recordedAt: ""
            });
        }
        return entries;
    }
    function parseHighScoreList(bytes) {
        var i;
        var cursor;
        var name;
        var entries = [];
        if (bytes.length < (ZZT.HIGH_SCORE_COUNT * HIGH_SCORE_ENTRY_BYTES)) {
            return false;
        }
        cursor = {
            bytes: bytes,
            offset: 0
        };
        for (i = 1; i <= ZZT.HIGH_SCORE_COUNT; i += 1) {
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
    function serializeHighScoreList() {
        var out = [];
        var i;
        var entries = ActiveHighScoreEntries;
        if (entries.length <= 0) {
            entries = getActiveHighScoreEntriesFromList();
        }
        for (i = 1; i <= ZZT.HIGH_SCORE_COUNT; i += 1) {
            if (i - 1 < entries.length) {
                writePascalString(out, formatHighScoreDisplayName(entries[i - 1]), HIGH_SCORE_NAME_MAX_LEN);
                pushInt16(out, entries[i - 1].score);
            }
            else {
                writePascalString(out, "", HIGH_SCORE_NAME_MAX_LEN);
                pushInt16(out, -1);
            }
        }
        return out;
    }
    function pushUniquePath(paths, path) {
        var i;
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
    function getCurrentScoreWorldKey() {
        var worldId = ZZT.LoadedGameFileName;
        var worldExt = ".ZZT";
        if (worldId.length <= 0 && ZZT.World.Info.Name.length > 0) {
            worldId = ZZT.World.Info.Name;
        }
        if (worldId.length <= 0) {
            worldId = "ZZT";
        }
        if (upperCase(worldId.slice(worldId.length - 4)) === ".SAV") {
            worldId = ZZT.World.Info.Name;
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
    function resolveHighScoreJsonPath() {
        var configured = trimSpaces(ZZT.HighScoreJsonPath);
        if (configured.length <= 0) {
            return ZZT.execPath(DEFAULT_HIGH_SCORE_JSON_REL_PATH);
        }
        if (isAbsoluteFilePath(configured)) {
            return configured;
        }
        return ZZT.execPath(configured);
    }
    function readSharedHighScoreStore() {
        var path = resolveHighScoreJsonPath();
        var bytes = ZZT.runtime.readBinaryFile(path);
        var emptyStore = {
            version: HIGH_SCORE_JSON_VERSION,
            worlds: {}
        };
        var text;
        var parsed;
        var parsedObj;
        var worldsObj;
        var worldKey;
        var worldValue;
        var rawEntries;
        var entry;
        var normalizedEntries;
        var i;
        var normalizedWorldKey;
        if (bytes === null || bytes.length <= 0) {
            return emptyStore;
        }
        text = trimSpaces(bytesToString(bytes));
        if (text.length <= 0) {
            return emptyStore;
        }
        try {
            parsed = JSON.parse(text);
        }
        catch (_err) {
            return emptyStore;
        }
        if (typeof parsed !== "object" || parsed === null) {
            return emptyStore;
        }
        parsedObj = parsed;
        if (typeof parsedObj.worlds !== "object" || parsedObj.worlds === null) {
            return emptyStore;
        }
        worldsObj = parsedObj.worlds;
        for (worldKey in worldsObj) {
            if (!Object.prototype.hasOwnProperty.call(worldsObj, worldKey)) {
                continue;
            }
            worldValue = worldsObj[worldKey];
            if (typeof worldValue !== "object" || worldValue === null) {
                continue;
            }
            if (!Array.isArray(worldValue.entries)) {
                continue;
            }
            rawEntries = worldValue.entries;
            normalizedEntries = [];
            for (i = 0; i < rawEntries.length; i += 1) {
                entry = rawEntries[i];
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
    function writeSharedHighScoreStore(store) {
        var path = resolveHighScoreJsonPath();
        var payload = JSON.stringify(store, null, 2) + "\n";
        if (!ensureParentDirectory(path)) {
            return false;
        }
        return ZZT.runtime.writeBinaryFile(path, stringToBytes(payload));
    }
    function resolveSaveRootPath() {
        var configured = trimSpaces(ZZT.SaveRootPath);
        if (configured.length <= 0) {
            return ZZT.execPath(DEFAULT_SAVE_ROOT_REL_PATH);
        }
        if (isAbsoluteFilePath(configured)) {
            return configured;
        }
        return ZZT.execPath(configured);
    }
    function currentUserSaveKey() {
        var userNumber = "";
        var userName = getCurrentUserName();
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
    function currentUserSaveDir() {
        return pathJoin(resolveSaveRootPath(), currentUserSaveKey() + "/");
    }
    function BuildDefaultSaveFileName() {
        var base;
        var value;
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
    ZZT.BuildDefaultSaveFileName = BuildDefaultSaveFileName;
    function getHighScorePathCandidates() {
        var paths = [];
        var baseName = ZZT.LoadedGameFileName.length > 0 ? ZZT.LoadedGameFileName : ZZT.World.Info.Name;
        var hasPathSep = baseName.indexOf("/") >= 0 || baseName.indexOf("\\") >= 0;
        if (upperCase(baseName.slice(baseName.length - 4)) === ".SAV" && ZZT.World.Info.Name.length > 0) {
            baseName = ZZT.World.Info.Name;
            hasPathSep = false;
        }
        function addHiVariants(pathBase) {
            pushUniquePath(paths, pathBase + ".HI");
            pushUniquePath(paths, pathBase + ".hi");
        }
        if (baseName.length > 0) {
            if (hasPathSep) {
                addHiVariants(ZZT.execPath(baseName));
                addHiVariants(ZZT.execPath("../" + baseName));
                addHiVariants(baseName);
            }
            else {
                addHiVariants(ZZT.execPath("zzt_files/" + baseName));
                addHiVariants(ZZT.execPath("../zzt_files/" + baseName));
                addHiVariants(ZZT.execPath(baseName));
                addHiVariants(baseName);
            }
        }
        if (ZZT.World.Info.Name.length > 0) {
            addHiVariants(ZZT.execPath(ZZT.World.Info.Name));
            addHiVariants(ZZT.World.Info.Name);
        }
        return paths;
    }
    function writeBoardInfo(out, info) {
        var i;
        pushUInt8(out, info.MaxShots);
        pushUInt8(out, info.IsDark ? 1 : 0);
        for (i = 0; i < 4; i += 1) {
            if (i < info.NeighborBoards.length) {
                pushUInt8(out, info.NeighborBoards[i]);
            }
            else {
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
    function readBoardInfo(cursor) {
        var info = ZZT.createBoardInfo();
        var i;
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
    function writeWorldInfo(out, info) {
        var i;
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
        for (i = 1; i <= ZZT.MAX_FLAG; i += 1) {
            writePascalString(out, info.Flags[i], FLAG_NAME_MAX_LEN);
        }
        pushInt16(out, info.BoardTimeSec);
        pushInt16(out, info.BoardTimeHsec);
        pushUInt8(out, info.IsSave ? 1 : 0);
        writePadding(out, info.UnknownPadding, WORLD_INFO_PADDING_LEN);
    }
    function readWorldInfo(cursor) {
        var info = ZZT.createWorldInfo();
        var i;
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
        for (i = 1; i <= ZZT.MAX_FLAG; i += 1) {
            info.Flags[i] = readPascalString(cursor, FLAG_NAME_MAX_LEN);
        }
        info.BoardTimeSec = readInt16(cursor);
        info.BoardTimeHsec = readInt16(cursor);
        info.IsSave = readUInt8(cursor) !== 0;
        info.UnknownPadding = readPadding(cursor, WORLD_INFO_PADDING_LEN);
        return info;
    }
    function writeStatRecord(out, stat, encodedDataLen) {
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
    function readStatRecord(cursor) {
        var stat = ZZT.createDefaultStat();
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
        stat.Under = ZZT.createTile(readUInt8(cursor), readUInt8(cursor));
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
    function fixedLengthDataBytes(data, len) {
        var source = data !== null ? stringToBytes(data) : [];
        var output = [];
        var i;
        for (i = 0; i < len; i += 1) {
            if (i < source.length) {
                output.push(source[i]);
            }
            else {
                output.push(0);
            }
        }
        return output;
    }
    function applyBoardOuterEdges(board) {
        var ix;
        var iy;
        for (ix = 0; ix <= ZZT.BOARD_WIDTH + 1; ix += 1) {
            board.Tiles[ix][0] = ZZT.cloneTile(TileBoardEdge);
            board.Tiles[ix][ZZT.BOARD_HEIGHT + 1] = ZZT.cloneTile(TileBoardEdge);
        }
        for (iy = 0; iy <= ZZT.BOARD_HEIGHT + 1; iy += 1) {
            board.Tiles[0][iy] = ZZT.cloneTile(TileBoardEdge);
            board.Tiles[ZZT.BOARD_WIDTH + 1][iy] = ZZT.cloneTile(TileBoardEdge);
        }
    }
    function serializeBoard(board) {
        var out = [];
        var ix;
        var iy;
        var runCount;
        var runTile;
        var statId;
        var sharedDataStatId;
        var encodedDataLen;
        writePascalString(out, board.Name, BOARD_NAME_MAX_LEN);
        ix = 1;
        iy = 1;
        runCount = 1;
        runTile = ZZT.cloneTile(board.Tiles[ix][iy]);
        while (true) {
            ix += 1;
            if (ix > ZZT.BOARD_WIDTH) {
                ix = 1;
                iy += 1;
            }
            if (iy <= ZZT.BOARD_HEIGHT &&
                runCount < 255 &&
                board.Tiles[ix][iy].Element === runTile.Element &&
                board.Tiles[ix][iy].Color === runTile.Color) {
                runCount += 1;
            }
            else {
                pushUInt8(out, runCount);
                pushUInt8(out, runTile.Element);
                pushUInt8(out, runTile.Color);
                if (iy > ZZT.BOARD_HEIGHT) {
                    break;
                }
                runTile = ZZT.cloneTile(board.Tiles[ix][iy]);
                runCount = 1;
            }
        }
        writeBoardInfo(out, board.Info);
        pushInt16(out, board.StatCount);
        for (statId = 0; statId <= board.StatCount; statId += 1) {
            var stat = board.Stats[statId];
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
    function deserializeBoard(bytes) {
        var cursor = {
            bytes: bytes,
            offset: 0
        };
        var board = ZZT.createBoard();
        var ix;
        var iy;
        var rleCount = 0;
        var rleElement = ZZT.E_EMPTY;
        var rleColor = 0;
        var statId;
        applyBoardOuterEdges(board);
        board.Name = readPascalString(cursor, BOARD_NAME_MAX_LEN);
        ix = 1;
        iy = 1;
        while (iy <= ZZT.BOARD_HEIGHT) {
            if (rleCount <= 0) {
                rleCount = readUInt8(cursor);
                if (rleCount <= 0) {
                    // DOS ZZT compatible decode: a stored count of 0 means 256 tiles.
                    rleCount = 256;
                }
                rleElement = readUInt8(cursor);
                rleColor = readUInt8(cursor);
            }
            board.Tiles[ix][iy] = ZZT.createTile(rleElement, rleColor);
            ix += 1;
            if (ix > ZZT.BOARD_WIDTH) {
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
        if (board.StatCount > ZZT.MAX_STAT) {
            board.StatCount = ZZT.MAX_STAT;
        }
        for (statId = 0; statId <= board.StatCount; statId += 1) {
            var stat = readStatRecord(cursor);
            if (stat.DataLen > 0) {
                stat.Data = bytesToString(readBytes(cursor, stat.DataLen));
            }
            else if (stat.DataLen < 0) {
                var sourceStatId = -stat.DataLen;
                if (sourceStatId >= 0 && sourceStatId < statId) {
                    stat.Data = board.Stats[sourceStatId].Data;
                    stat.DataLen = board.Stats[sourceStatId].DataLen;
                }
                else {
                    stat.Data = null;
                    stat.DataLen = 0;
                }
            }
            else {
                stat.Data = null;
            }
            board.Stats[statId] = stat;
        }
        return board;
    }
    function fileExists(path) {
        var f;
        if (typeof File === "undefined") {
            return false;
        }
        try {
            f = new File(path);
            return f.exists;
        }
        catch (_err) {
            return false;
        }
    }
    function getExtensionVariants(extension) {
        var variants = [];
        var seen = {};
        function pushVariant(ext) {
            var id = upperCase(ext);
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
    function getFullNameVariants(filename, extension) {
        var names = [];
        var extVariants = getExtensionVariants(extension);
        var i;
        for (i = 0; i < extVariants.length; i += 1) {
            pushUniquePath(names, filename + extVariants[i]);
        }
        return names;
    }
    function resolveWorldPath(filename, extension) {
        var fullNames = getFullNameVariants(filename, extension);
        var fullName = fullNames.length > 0 ? fullNames[0] : (filename + extension);
        var i;
        var sharedPath;
        var zztFilesPrimary;
        var primary;
        var resFallback;
        var hasPathSep = filename.indexOf("/") >= 0 || filename.indexOf("\\") >= 0;
        var isAbsolutePath = isAbsoluteFilePath(filename);
        var savePath;
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
                primary = ZZT.execPath(fullNames[i]);
                if (fileExists(primary)) {
                    return primary;
                }
                sharedPath = ZZT.execPath("../" + fullNames[i]);
                if (fileExists(sharedPath)) {
                    return sharedPath;
                }
            }
            return ZZT.execPath(fullName);
        }
        if (upperCase(extension) === ".SAV") {
            for (i = 0; i < fullNames.length; i += 1) {
                savePath = pathJoin(currentUserSaveDir(), fullNames[i]);
                if (fileExists(savePath)) {
                    return savePath;
                }
                zztFilesPrimary = ZZT.execPath("zzt_files/" + fullNames[i]);
                if (fileExists(zztFilesPrimary)) {
                    return zztFilesPrimary;
                }
                primary = ZZT.execPath(fullNames[i]);
                if (fileExists(primary)) {
                    return primary;
                }
            }
            return pathJoin(currentUserSaveDir(), fullName);
        }
        for (i = 0; i < fullNames.length; i += 1) {
            zztFilesPrimary = ZZT.execPath("zzt_files/" + fullNames[i]);
            if (fileExists(zztFilesPrimary)) {
                return zztFilesPrimary;
            }
            sharedPath = ZZT.execPath("../zzt_files/" + fullNames[i]);
            if (fileExists(sharedPath)) {
                return sharedPath;
            }
            primary = ZZT.execPath(fullNames[i]);
            if (fileExists(primary)) {
                return primary;
            }
            resFallback = ZZT.execPath("reconstruction-of-zzt-master/RES/" + fullNames[i]);
            if (fileExists(resFallback)) {
                return resFallback;
            }
        }
        zztFilesPrimary = ZZT.execPath("zzt_files/" + fullName);
        return zztFilesPrimary;
    }
    function GenerateTransitionTable() {
        var ix;
        var iy;
        var swapIx;
        var temp;
        ZZT.TransitionTable = [];
        for (iy = 1; iy <= ZZT.BOARD_HEIGHT; iy += 1) {
            for (ix = 1; ix <= ZZT.BOARD_WIDTH; ix += 1) {
                ZZT.TransitionTable.push({
                    X: ix,
                    Y: iy
                });
            }
        }
        ZZT.TransitionTableSize = ZZT.TransitionTable.length;
        for (ix = 0; ix < ZZT.TransitionTableSize; ix += 1) {
            swapIx = randomInt(ZZT.TransitionTableSize);
            temp = ZZT.TransitionTable[swapIx];
            ZZT.TransitionTable[swapIx] = ZZT.TransitionTable[ix];
            ZZT.TransitionTable[ix] = temp;
        }
    }
    ZZT.GenerateTransitionTable = GenerateTransitionTable;
    function Signum(val) {
        if (val > 0) {
            return 1;
        }
        if (val < 0) {
            return -1;
        }
        return 0;
    }
    ZZT.Signum = Signum;
    function Difference(a, b) {
        if ((a - b) >= 0) {
            return a - b;
        }
        return b - a;
    }
    ZZT.Difference = Difference;
    function CalcDirectionRnd() {
        var deltaX = randomInt(3) - 1;
        var deltaY = 0;
        if (deltaX === 0) {
            deltaY = randomInt(2) * 2 - 1;
        }
        return {
            DeltaX: deltaX,
            DeltaY: deltaY
        };
    }
    ZZT.CalcDirectionRnd = CalcDirectionRnd;
    function CalcDirectionSeek(x, y) {
        var deltaX = 0;
        var deltaY = 0;
        var playerX = ZZT.Board.Stats[0].X;
        var playerY = ZZT.Board.Stats[0].Y;
        if (randomInt(2) < 1 || playerY === y) {
            deltaX = Signum(playerX - x);
        }
        if (deltaX === 0) {
            deltaY = Signum(playerY - y);
        }
        if (ZZT.World.Info.EnergizerTicks > 0) {
            deltaX = -deltaX;
            deltaY = -deltaY;
        }
        return {
            DeltaX: deltaX,
            DeltaY: deltaY
        };
    }
    ZZT.CalcDirectionSeek = CalcDirectionSeek;
    function BoardClose() {
        var boardId = sanitizeBoardId(ZZT.World.Info.CurrentBoard);
        var serialized = serializeBoard(ZZT.Board);
        ZZT.World.BoardData[boardId] = serialized;
        ZZT.World.BoardLen[boardId] = serialized.length;
        if (boardId > ZZT.World.BoardCount) {
            ZZT.World.BoardCount = boardId;
        }
    }
    ZZT.BoardClose = BoardClose;
    function BoardOpen(boardId) {
        var normalizedId = sanitizeBoardId(boardId);
        var packedBoard;
        if (normalizedId > ZZT.World.BoardCount) {
            normalizedId = ZZT.World.Info.CurrentBoard;
        }
        normalizedId = sanitizeBoardId(normalizedId);
        packedBoard = ZZT.World.BoardData[normalizedId];
        if (packedBoard !== null) {
            ZZT.Board = deserializeBoard(packedBoard);
        }
        else {
            BoardCreate();
        }
        ZZT.World.Info.CurrentBoard = normalizedId;
    }
    ZZT.BoardOpen = BoardOpen;
    function BoardChange(boardId) {
        var px = ZZT.Board.Stats[0].X;
        var py = ZZT.Board.Stats[0].Y;
        if (px >= 0 && px <= ZZT.BOARD_WIDTH + 1 && py >= 0 && py <= ZZT.BOARD_HEIGHT + 1) {
            ZZT.Board.Tiles[px][py].Element = ZZT.E_PLAYER;
            ZZT.Board.Tiles[px][py].Color = ZZT.ElementDefs[ZZT.E_PLAYER].Color;
        }
        BoardClose();
        BoardOpen(boardId);
    }
    ZZT.BoardChange = BoardChange;
    function BoardCreate() {
        var ix;
        var iy;
        var i;
        var centerX;
        var centerY;
        ZZT.Board = ZZT.createBoard();
        ZZT.Board.Name = "";
        ZZT.Board.Info.Message = "";
        ZZT.Board.Info.MaxShots = 255;
        ZZT.Board.Info.IsDark = false;
        ZZT.Board.Info.ReenterWhenZapped = false;
        ZZT.Board.Info.TimeLimitSec = 0;
        for (i = 0; i <= 3; i += 1) {
            ZZT.Board.Info.NeighborBoards[i] = 0;
        }
        applyBoardOuterEdges(ZZT.Board);
        for (ix = 1; ix <= ZZT.BOARD_WIDTH; ix += 1) {
            for (iy = 1; iy <= ZZT.BOARD_HEIGHT; iy += 1) {
                ZZT.Board.Tiles[ix][iy] = ZZT.createTile(ZZT.E_EMPTY, 0);
            }
        }
        for (ix = 1; ix <= ZZT.BOARD_WIDTH; ix += 1) {
            ZZT.Board.Tiles[ix][1] = ZZT.cloneTile(TileBorder);
            ZZT.Board.Tiles[ix][ZZT.BOARD_HEIGHT] = ZZT.cloneTile(TileBorder);
        }
        for (iy = 1; iy <= ZZT.BOARD_HEIGHT; iy += 1) {
            ZZT.Board.Tiles[1][iy] = ZZT.cloneTile(TileBorder);
            ZZT.Board.Tiles[ZZT.BOARD_WIDTH][iy] = ZZT.cloneTile(TileBorder);
        }
        centerX = Math.floor(ZZT.BOARD_WIDTH / 2);
        centerY = Math.floor(ZZT.BOARD_HEIGHT / 2);
        ZZT.Board.Tiles[centerX][centerY].Element = ZZT.E_PLAYER;
        ZZT.Board.Tiles[centerX][centerY].Color = ZZT.ElementDefs[ZZT.E_PLAYER].Color;
        ZZT.Board.StatCount = 0;
        ZZT.Board.Stats[0] = ZZT.createDefaultStat();
        ZZT.Board.Stats[0].X = centerX;
        ZZT.Board.Stats[0].Y = centerY;
        ZZT.Board.Stats[0].Cycle = 1;
        ZZT.Board.Stats[0].Under = ZZT.createTile(ZZT.E_EMPTY, 0);
        ZZT.Board.Stats[0].Data = null;
        ZZT.Board.Stats[0].DataLen = 0;
    }
    ZZT.BoardCreate = BoardCreate;
    function WorldUnload() {
        var i;
        BoardClose();
        for (i = 0; i <= ZZT.MAX_BOARD; i += 1) {
            ZZT.World.BoardData[i] = null;
            ZZT.World.BoardLen[i] = 0;
        }
        ZZT.World.BoardCount = 0;
    }
    ZZT.WorldUnload = WorldUnload;
    function WorldLoad(filename, extension, titleOnly) {
        var path = resolveWorldPath(filename, extension);
        var bytes = ZZT.runtime.readBinaryFile(path);
        var cursor;
        var boardCount;
        var boardId;
        var headerValue;
        var worldInfo;
        var boardLen;
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
        }
        else {
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
        if (boardCount > ZZT.MAX_BOARD) {
            boardCount = ZZT.MAX_BOARD;
        }
        ZZT.World = ZZT.createWorld();
        ZZT.World.BoardCount = boardCount;
        ZZT.World.Info = ZZT.cloneWorldInfo(worldInfo);
        cursor.offset = WORLD_FILE_HEADER_SIZE_BYTES;
        for (boardId = 0; boardId <= ZZT.World.BoardCount; boardId += 1) {
            boardLen = readUInt16(cursor);
            ZZT.World.BoardLen[boardId] = boardLen;
            ZZT.World.BoardData[boardId] = readBytes(cursor, boardLen);
        }
        BoardOpen(ZZT.World.Info.CurrentBoard);
        ZZT.LoadedGameFileName = filename;
        ZZT.SoundWorldMusicConfigureFromWorldFile(path);
        if (!ZZT.SoundWorldMusicHasTracks() &&
            upperCase(extension) === ".SAV" &&
            ZZT.World.Info.Name.length > 0) {
            ZZT.SoundWorldMusicConfigureFromWorldFile(resolveWorldPath(ZZT.World.Info.Name, ".ZZT"));
        }
        HighScoresLoad();
        return true;
    }
    ZZT.WorldLoad = WorldLoad;
    function WorldSave(filename, extension) {
        var path = resolveWorldPath(filename, extension);
        var currentBoardId = ZZT.World.Info.CurrentBoard;
        var header = [];
        var fileBytes = [];
        var boardId;
        var boardData;
        var boardDataMaybeNull;
        BoardClose();
        pushInt16(header, -1);
        pushInt16(header, ZZT.World.BoardCount);
        writeWorldInfo(header, ZZT.World.Info);
        while (header.length < WORLD_FILE_HEADER_SIZE_BYTES) {
            header.push(0);
        }
        if (header.length > WORLD_FILE_HEADER_SIZE_BYTES) {
            header = header.slice(0, WORLD_FILE_HEADER_SIZE_BYTES);
        }
        appendBytes(fileBytes, header);
        for (boardId = 0; boardId <= ZZT.World.BoardCount; boardId += 1) {
            boardDataMaybeNull = ZZT.World.BoardData[boardId];
            if (boardDataMaybeNull === null) {
                boardData = [];
            }
            else {
                boardData = boardDataMaybeNull;
            }
            ZZT.World.BoardLen[boardId] = boardData.length;
            pushUInt16(fileBytes, boardData.length);
            appendBytes(fileBytes, boardData);
        }
        if (!ensureParentDirectory(path)) {
            BoardOpen(currentBoardId);
            return false;
        }
        var result = ZZT.runtime.writeBinaryFile(path, fileBytes);
        BoardOpen(currentBoardId);
        return result;
    }
    ZZT.WorldSave = WorldSave;
    function WorldCreate() {
        var i;
        ZZT.InitElementsGame();
        ZZT.World = ZZT.createWorld();
        ZZT.World.BoardCount = 0;
        ZZT.World.BoardLen[0] = 0;
        ZZT.InitEditorStatSettings();
        ZZT.ResetMessageNotShownFlags();
        BoardCreate();
        ZZT.World.Info.IsSave = false;
        ZZT.World.Info.CurrentBoard = 0;
        ZZT.World.Info.Ammo = 0;
        ZZT.World.Info.Gems = 0;
        ZZT.World.Info.Health = 100;
        ZZT.World.Info.EnergizerTicks = 0;
        ZZT.World.Info.Torches = 0;
        ZZT.World.Info.TorchTicks = 0;
        ZZT.World.Info.Score = 0;
        ZZT.World.Info.BoardTimeSec = 0;
        ZZT.World.Info.BoardTimeHsec = 0;
        for (i = 1; i <= 7; i += 1) {
            ZZT.World.Info.Keys[i] = false;
        }
        for (i = 1; i <= ZZT.MAX_FLAG; i += 1) {
            ZZT.World.Info.Flags[i] = "";
        }
        BoardChange(0);
        ZZT.Board.Name = "Title screen";
        ZZT.LoadedGameFileName = "";
        ZZT.World.Info.Name = "";
        ZZT.SoundWorldMusicConfigureFromWorldFile("");
    }
    ZZT.WorldCreate = WorldCreate;
    function repeatChar(ch, count) {
        var out = "";
        var i;
        for (i = 0; i < count; i += 1) {
            out += ch;
        }
        return out;
    }
    function padRight(value, width) {
        var text = value;
        if (text.length > width) {
            return text.slice(0, width);
        }
        while (text.length < width) {
            text += " ";
        }
        return text;
    }
    function padLeft(value, width) {
        var text = value;
        if (text.length > width) {
            return text.slice(text.length - width);
        }
        while (text.length < width) {
            text = " " + text;
        }
        return text;
    }
    function upperCase(value) {
        return value.toUpperCase();
    }
    function toHex2(value) {
        var hex = (value & 0xff).toString(16).toUpperCase();
        if (hex.length < 2) {
            hex = "0" + hex;
        }
        return hex;
    }
    function debugKeyLabel(key) {
        var code;
        if (key.length <= 0) {
            return "00";
        }
        code = key.charCodeAt(0) & 0xff;
        if (code >= 32 && code <= 126) {
            return key.charAt(0);
        }
        return "$" + toHex2(code);
    }
    function drawDebugStatusLine() {
        var mode = ZZT.GameStateElement === ZZT.E_MONITOR ? "MON" : "PLY";
        var keyLabel = debugKeyLabel(ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0) : "");
        var deltaLabel = String(ZZT.InputDeltaX) + "," + String(ZZT.InputDeltaY);
        var playerCycle = (ZZT.Board.Stats.length > 0 ? ZZT.Board.Stats[0].Cycle : 0);
        var text = "DBG " + mode +
            " K:" + keyLabel +
            " D:" + deltaLabel +
            " C:" + String(playerCycle) +
            " P:" + (ZZT.GamePaused ? "1" : "0") +
            " T:" + String(ZZT.CurrentTick) +
            " S:" + String(ZZT.CurrentStatTicked) + "/" + String(ZZT.Board.StatCount);
        SidebarClearLine(24);
        ZZT.VideoWriteText(60, 24, 0x1e, padRight(text, 19));
    }
    function invokeElementTouch(x, y, sourceStatId, context) {
        var element = ZZT.Board.Tiles[x][y].Element;
        var def = ZZT.ElementDefs[element];
        if (def.TouchProc !== null) {
            def.TouchProc(x, y, sourceStatId, context);
        }
    }
    function tryApplyPlayerDirectionalInput() {
        var stat;
        var context;
        var desiredDeltaX;
        var desiredDeltaY;
        if (ZZT.World.Info.Health <= 0) {
            ZZT.InputDeltaX = 0;
            ZZT.InputDeltaY = 0;
            return;
        }
        if (ZZT.InputDeltaX === 0 && ZZT.InputDeltaY === 0) {
            return;
        }
        if (ZZT.Board.StatCount < 0) {
            return;
        }
        desiredDeltaX = ZZT.InputDeltaX;
        desiredDeltaY = ZZT.InputDeltaY;
        ZZT.PlayerDirX = desiredDeltaX;
        ZZT.PlayerDirY = desiredDeltaY;
        stat = ZZT.Board.Stats[0];
        context = {
            DeltaX: desiredDeltaX,
            DeltaY: desiredDeltaY
        };
        invokeElementTouch(stat.X + context.DeltaX, stat.Y + context.DeltaY, 0, context);
        if ((context.DeltaX !== 0 || context.DeltaY !== 0) &&
            ZZT.ElementDefs[ZZT.Board.Tiles[stat.X + context.DeltaX][stat.Y + context.DeltaY].Element].Walkable) {
            MovePlayerStat(stat.X + context.DeltaX, stat.Y + context.DeltaY);
        }
        // Consume one directional input event so movement doesn't repeat uncontrollably.
        ZZT.InputDeltaX = 0;
        ZZT.InputDeltaY = 0;
    }
    function getElementDrawChar(x, y) {
        var tile = ZZT.Board.Tiles[x][y];
        var def = ZZT.ElementDefs[tile.Element];
        var drawValue;
        if (def.HasDrawProc && def.DrawProc !== null) {
            drawValue = def.DrawProc(x, y);
            return String.fromCharCode(drawValue & 0xff);
        }
        if (tile.Element < ZZT.E_TEXT_MIN) {
            if (def.Character.length > 0) {
                return def.Character.charAt(0);
            }
            return "?";
        }
        return String.fromCharCode(tile.Color & 0xff);
    }
    function getTileDrawColor(x, y) {
        var tile = ZZT.Board.Tiles[x][y];
        if (tile.Element === ZZT.E_EMPTY) {
            return 0x0f;
        }
        if (tile.Element < ZZT.E_TEXT_MIN) {
            return tile.Color;
        }
        if (tile.Element === ZZT.E_TEXT_WHITE) {
            return 0x0f;
        }
        return (((tile.Element - ZZT.E_TEXT_MIN) + 1) * 16) + 0x0f;
    }
    function BoardDrawTile(x, y) {
        var tile = ZZT.Board.Tiles[x][y];
        var visibleInDark = ZZT.ElementDefs[tile.Element].VisibleInDark;
        var litByTorch = ZZT.World.Info.TorchTicks > 0 &&
            ((ZZT.Board.Stats[0].X - x) * (ZZT.Board.Stats[0].X - x) + (ZZT.Board.Stats[0].Y - y) * (ZZT.Board.Stats[0].Y - y) * 2) < ZZT.TORCH_DIST_SQR;
        if (ZZT.Board.Info.IsDark && !ZZT.ForceDarknessOff && !visibleInDark && !litByTorch) {
            ZZT.VideoWriteText(x - 1, y - 1, 0x07, String.fromCharCode(176));
            return;
        }
        ZZT.VideoWriteText(x - 1, y - 1, getTileDrawColor(x, y), getElementDrawChar(x, y));
    }
    ZZT.BoardDrawTile = BoardDrawTile;
    function BoardDrawBorder() {
        var ix;
        var iy;
        for (ix = 1; ix <= ZZT.BOARD_WIDTH; ix += 1) {
            BoardDrawTile(ix, 1);
            BoardDrawTile(ix, ZZT.BOARD_HEIGHT);
        }
        for (iy = 1; iy <= ZZT.BOARD_HEIGHT; iy += 1) {
            BoardDrawTile(1, iy);
            BoardDrawTile(ZZT.BOARD_WIDTH, iy);
        }
    }
    ZZT.BoardDrawBorder = BoardDrawBorder;
    function TransitionDrawToBoard() {
        var ix;
        var iy;
        BoardDrawBorder();
        for (iy = 1; iy <= ZZT.BOARD_HEIGHT; iy += 1) {
            for (ix = 1; ix <= ZZT.BOARD_WIDTH; ix += 1) {
                BoardDrawTile(ix, iy);
            }
        }
    }
    ZZT.TransitionDrawToBoard = TransitionDrawToBoard;
    function TransitionDrawToFill(chr, color) {
        var ix;
        var iy;
        for (iy = 0; iy < ZZT.BOARD_HEIGHT; iy += 1) {
            for (ix = 0; ix < ZZT.BOARD_WIDTH; ix += 1) {
                ZZT.VideoWriteText(ix, iy, color, chr);
            }
        }
    }
    ZZT.TransitionDrawToFill = TransitionDrawToFill;
    function TransitionDrawBoardChange() {
        TransitionDrawToFill(String.fromCharCode(219), 0x05);
        TransitionDrawToBoard();
    }
    ZZT.TransitionDrawBoardChange = TransitionDrawBoardChange;
    function SidebarClearLine(y) {
        ZZT.VideoWriteText(60, y, 0x11, "                   ");
    }
    ZZT.SidebarClearLine = SidebarClearLine;
    function SidebarClear() {
        var i;
        for (i = 3; i <= 24; i += 1) {
            SidebarClearLine(i);
        }
    }
    ZZT.SidebarClear = SidebarClear;
    var PROMPT_NUMERIC = 0;
    var PROMPT_ALPHANUM = 1;
    var PROMPT_ANY = 2;
    function PromptString(x, y, arrowColor, color, width, mode, buffer) {
        var current = buffer;
        var oldBuffer = buffer;
        var firstKeyPress = true;
        var i;
        var key;
        var keyUpper;
        while (true) {
            for (i = 0; i < width; i += 1) {
                ZZT.VideoWriteText(x + i, y, color, " ");
                ZZT.VideoWriteText(x + i, y - 1, arrowColor, " ");
            }
            ZZT.VideoWriteText(x + width, y - 1, arrowColor, " ");
            ZZT.VideoWriteText(x + current.length, y - 1, ((Math.floor(arrowColor / 16) * 16) + 0x0f), String.fromCharCode(31));
            ZZT.VideoWriteText(x, y, color, current);
            ZZT.InputReadWaitKey();
            key = ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0) : String.fromCharCode(0);
            keyUpper = upperCase(key);
            if (current.length < width && key.charCodeAt(0) >= 32 && key.charCodeAt(0) < 128) {
                if (firstKeyPress) {
                    current = "";
                }
                if (mode === PROMPT_NUMERIC) {
                    if (key >= "0" && key <= "9") {
                        current += key;
                    }
                }
                else if (mode === PROMPT_ANY) {
                    current += key;
                }
                else if (mode === PROMPT_ALPHANUM) {
                    if ((keyUpper >= "A" && keyUpper <= "Z") || (key >= "0" && key <= "9") || key === "-") {
                        current += keyUpper;
                    }
                }
            }
            else if (key === ZZT.KEY_LEFT || key === ZZT.KEY_BACKSPACE) {
                if (current.length > 0) {
                    current = current.slice(0, current.length - 1);
                }
            }
            firstKeyPress = false;
            if (key === ZZT.KEY_ENTER || key === ZZT.KEY_ESCAPE) {
                if (key === ZZT.KEY_ESCAPE) {
                    return oldBuffer;
                }
                return current;
            }
        }
    }
    function SidebarPromptString(prompt, extension, filename, promptMode) {
        var value = filename;
        SidebarClearLine(3);
        SidebarClearLine(4);
        SidebarClearLine(5);
        ZZT.VideoWriteText(75 - prompt.length, 3, 0x1f, prompt);
        ZZT.VideoWriteText(63, 5, 0x0f, "        " + extension);
        value = PromptString(63, 5, 0x1e, 0x0f, 8, promptMode, value);
        SidebarClearLine(3);
        SidebarClearLine(4);
        SidebarClearLine(5);
        return value;
    }
    function PopupPromptString(question, buffer) {
        var value = buffer;
        SidebarClearLine(3);
        SidebarClearLine(4);
        SidebarClearLine(5);
        ZZT.VideoWriteText(63, 4, 0x1f, padRight(question, 17));
        value = PromptString(63, 5, 0x1e, 0x0f, 17, PROMPT_ANY, value);
        SidebarClearLine(3);
        SidebarClearLine(4);
        SidebarClearLine(5);
        return value;
    }
    function SidebarPromptYesNo(message, defaultReturn) {
        var key;
        SidebarClearLine(3);
        SidebarClearLine(4);
        SidebarClearLine(5);
        ZZT.VideoWriteText(63, 5, 0x1f, message);
        ZZT.VideoWriteText(63 + message.length, 5, 0x9e, "_");
        while (true) {
            ZZT.InputReadWaitKey();
            key = upperCase(ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0) : String.fromCharCode(0));
            if (key === ZZT.KEY_ESCAPE || key === ZZT.KEY_ENTER || key === "N" || key === "Q" || key === "Y") {
                break;
            }
        }
        if (key === "Y") {
            defaultReturn = true;
        }
        else if (key === "N" || key === ZZT.KEY_ESCAPE) {
            defaultReturn = false;
        }
        SidebarClearLine(5);
        return defaultReturn;
    }
    ZZT.SidebarPromptYesNo = SidebarPromptYesNo;
    function HighScoresLoad() {
        var worldKey = getCurrentScoreWorldKey();
        var store = readSharedHighScoreStore();
        var record = store.worlds[worldKey];
        var paths = getHighScorePathCandidates();
        var i;
        var bytes;
        ActiveHighScoreEntries = [];
        resetHighScoreList();
        if (typeof record !== "undefined") {
            setActiveHighScoreEntries(record.entries);
            if (ActiveHighScoreEntries.length > 0) {
                return;
            }
        }
        for (i = 0; i < paths.length; i += 1) {
            bytes = ZZT.runtime.readBinaryFile(paths[i]);
            if (bytes !== null && parseHighScoreList(bytes)) {
                if (ActiveHighScoreEntries.length > 0) {
                    HighScoresSave();
                }
                return;
            }
        }
    }
    ZZT.HighScoresLoad = HighScoresLoad;
    function HighScoresSave() {
        var worldKey = getCurrentScoreWorldKey();
        var store = readSharedHighScoreStore();
        var entries = ActiveHighScoreEntries.length > 0 ? ActiveHighScoreEntries.slice(0) : getActiveHighScoreEntriesFromList();
        var paths = getHighScorePathCandidates();
        var targetPath = paths.length > 0 ? paths[0] : ZZT.execPath("ZZT.HI");
        if (entries.length > ZZT.HIGH_SCORE_COUNT) {
            entries = entries.slice(0, ZZT.HIGH_SCORE_COUNT);
        }
        setActiveHighScoreEntries(entries);
        entries = ActiveHighScoreEntries.slice(0);
        if (entries.length > 0) {
            store.worlds[worldKey] = {
                updatedAt: currentIsoTimestamp(),
                entries: entries
            };
        }
        else if (typeof store.worlds[worldKey] !== "undefined") {
            delete store.worlds[worldKey];
        }
        writeSharedHighScoreStore(store);
        ensureParentDirectory(targetPath);
        ZZT.runtime.writeBinaryFile(targetPath, serializeHighScoreList());
    }
    ZZT.HighScoresSave = HighScoresSave;
    function highScoresInitTextWindow(state) {
        var i;
        var scoreStr;
        var displayName;
        ZZT.TextWindowInitState(state);
        ZZT.TextWindowAppend(state, "Score  Name");
        ZZT.TextWindowAppend(state, "-----  ----------------------------------");
        for (i = 1; i <= ZZT.HIGH_SCORE_COUNT; i += 1) {
            if (ZZT.HighScoreList[i].Score >= 0) {
                displayName = ZZT.HighScoreList[i].Name;
                if (displayName.length <= 0) {
                    displayName = "Unknown Player";
                }
                scoreStr = padLeft(String(ZZT.HighScoreList[i].Score), 5);
                ZZT.TextWindowAppend(state, scoreStr + "  " + displayName);
            }
        }
    }
    function HighScoresDisplay(linePos) {
        var state = {
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
        }
        else if (state.LinePos > state.LineCount) {
            state.LinePos = state.LineCount;
        }
        if (state.LineCount > 2) {
            state.Title = "High scores for " + (ZZT.World.Info.Name.length > 0 ? ZZT.World.Info.Name : splitBaseName(getCurrentScoreWorldKey()));
            ZZT.TextWindowDrawOpen(state);
            ZZT.TextWindowSelect(state, false, true);
            ZZT.TextWindowDrawClose(state);
        }
        ZZT.TextWindowFree(state);
    }
    ZZT.HighScoresDisplay = HighScoresDisplay;
    function HighScoresAdd(score) {
        var entries = ActiveHighScoreEntries.slice(0);
        var listPos = 0;
        var entry;
        var boardTitle = ZZT.World.Info.Name.length > 0 ? ZZT.World.Info.Name : splitBaseName(getCurrentScoreWorldKey());
        var textWindow = {
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
        if (listPos >= ZZT.HIGH_SCORE_COUNT || score <= 0) {
            return;
        }
        entry = {
            player: getCurrentUserName(),
            bbs: getCurrentBbsName(),
            score: score,
            recordedAt: currentIsoTimestamp()
        };
        entries.splice(listPos, 0, entry);
        if (entries.length > ZZT.HIGH_SCORE_COUNT) {
            entries = entries.slice(0, ZZT.HIGH_SCORE_COUNT);
        }
        setActiveHighScoreEntries(entries);
        HighScoresSave();
        highScoresInitTextWindow(textWindow);
        textWindow.LinePos = listPos + 1;
        textWindow.Title = "New high score for " + boardTitle;
        ZZT.TextWindowDrawOpen(textWindow);
        ZZT.TextWindowSelect(textWindow, false, false);
        ZZT.TextWindowDrawClose(textWindow);
        TransitionDrawToBoard();
        ZZT.TextWindowFree(textWindow);
    }
    ZZT.HighScoresAdd = HighScoresAdd;
    function showTitleNotice(title, lines) {
        var state = {
            Selectable: false,
            LineCount: 0,
            LinePos: 1,
            Lines: [],
            Hyperlink: "",
            Title: title,
            LoadedFilename: "",
            ScreenCopy: []
        };
        var i;
        ZZT.TextWindowInitState(state);
        for (i = 0; i < lines.length; i += 1) {
            ZZT.TextWindowAppend(state, lines[i]);
        }
        if (state.LineCount > 0) {
            ZZT.TextWindowDrawOpen(state);
            ZZT.TextWindowSelect(state, false, false);
            ZZT.TextWindowDrawClose(state);
        }
        ZZT.TextWindowFree(state);
    }
    function EditorLoop() {
        var DRAW_MODE_OFF = 0;
        var DRAW_MODE_ON = 1;
        var DRAW_MODE_TEXT = 2;
        var wasModified = false;
        var editorExitRequested = false;
        var drawMode = DRAW_MODE_OFF;
        var cursorX = 30;
        var cursorY = 12;
        var cursorPattern = 1;
        var cursorColor = 0x0e;
        var colorNames = ["", "Blue", "Green", "Cyan", "Red", "Purple", "Yellow", "White"];
        var copiedTile = ZZT.createTile(ZZT.E_EMPTY, 0x0f);
        var copiedStat = ZZT.createDefaultStat();
        var copiedHasStat = false;
        var copiedX = 30;
        var copiedY = 12;
        var keyRaw;
        var key;
        var nulKey = String.fromCharCode(0);
        var i;
        var canModify;
        var tileAtCursor;
        var cursorBlinker = 0;
        var useBlockingInputFallback = !(typeof console !== "undefined" && typeof console.inkey === "function");
        function editorDrawTileAndNeighborsAt(x, y) {
            var n;
            var nx;
            var ny;
            BoardDrawTile(x, y);
            for (n = 0; n < 4; n += 1) {
                nx = x + NeighborDeltaX[n];
                ny = y + NeighborDeltaY[n];
                if (nx >= 1 && nx <= ZZT.BOARD_WIDTH && ny >= 1 && ny <= ZZT.BOARD_HEIGHT) {
                    BoardDrawTile(nx, ny);
                }
            }
        }
        function editorElementMenuColor(element) {
            if (ZZT.ElementDefs[element].Color === ZZT.COLOR_CHOICE_ON_BLACK) {
                return (cursorColor % 0x10) + 0x10;
            }
            if (ZZT.ElementDefs[element].Color === ZZT.COLOR_WHITE_ON_CHOICE) {
                return (cursorColor * 0x10) - 0x71;
            }
            if (ZZT.ElementDefs[element].Color === ZZT.COLOR_CHOICE_ON_CHOICE) {
                return ((cursorColor - 8) * 0x11) + 8;
            }
            if ((ZZT.ElementDefs[element].Color & 0x70) === 0) {
                return (ZZT.ElementDefs[element].Color % 0x10) + 0x10;
            }
            return ZZT.ElementDefs[element].Color;
        }
        function editorElementPlacementColor(element) {
            if (ZZT.ElementDefs[element].Color === ZZT.COLOR_CHOICE_ON_BLACK) {
                return cursorColor;
            }
            if (ZZT.ElementDefs[element].Color === ZZT.COLOR_WHITE_ON_CHOICE) {
                return (cursorColor * 0x10) - 0x71;
            }
            if (ZZT.ElementDefs[element].Color === ZZT.COLOR_CHOICE_ON_CHOICE) {
                return ((cursorColor - 8) * 0x11) + 8;
            }
            return ZZT.ElementDefs[element].Color;
        }
        function editorDrawSidebar() {
            var iColor;
            var iPattern;
            var copiedCharCode;
            function getCopiedPreviewCharCode() {
                var drawProc;
                if (copiedTile.Element >= 0 && copiedTile.Element <= ZZT.MAX_ELEMENT) {
                    if (ZZT.ElementDefs[copiedTile.Element].HasDrawProc) {
                        drawProc = ZZT.ElementDefs[copiedTile.Element].DrawProc;
                        if (drawProc !== null) {
                            return drawProc(copiedX, copiedY) & 0xff;
                        }
                    }
                    if (ZZT.ElementDefs[copiedTile.Element].Character.length > 0) {
                        return ZZT.ElementDefs[copiedTile.Element].Character.charCodeAt(0) & 0xff;
                    }
                }
                return " ".charCodeAt(0);
            }
            SidebarClear();
            SidebarClearLine(1);
            ZZT.VideoWriteText(61, 0, 0x1f, "     - - - -       ");
            ZZT.VideoWriteText(62, 1, 0x70, "  ZZT Editor   ");
            ZZT.VideoWriteText(61, 2, 0x1f, "     - - - -       ");
            ZZT.VideoWriteText(61, 4, 0x70, " L ");
            ZZT.VideoWriteText(64, 4, 0x1f, " Load");
            ZZT.VideoWriteText(61, 5, 0x30, " S ");
            ZZT.VideoWriteText(64, 5, 0x1f, " Save");
            ZZT.VideoWriteText(70, 4, 0x70, " H ");
            ZZT.VideoWriteText(73, 4, 0x1e, " Help");
            ZZT.VideoWriteText(70, 5, 0x30, " Q ");
            ZZT.VideoWriteText(73, 5, 0x1f, " Quit");
            ZZT.VideoWriteText(61, 7, 0x70, " B ");
            ZZT.VideoWriteText(65, 7, 0x1f, " Switch boards");
            ZZT.VideoWriteText(61, 8, 0x30, " I ");
            ZZT.VideoWriteText(65, 8, 0x1f, " Board Info");
            ZZT.VideoWriteText(61, 10, 0x70, "  f1   ");
            ZZT.VideoWriteText(68, 10, 0x1f, " Item");
            ZZT.VideoWriteText(61, 11, 0x30, "  f2   ");
            ZZT.VideoWriteText(68, 11, 0x1f, " Creature");
            ZZT.VideoWriteText(61, 12, 0x70, "  f3   ");
            ZZT.VideoWriteText(68, 12, 0x1f, " Terrain");
            ZZT.VideoWriteText(61, 13, 0x30, "  f4   ");
            ZZT.VideoWriteText(68, 13, 0x1f, " Enter text");
            ZZT.VideoWriteText(61, 15, 0x70, " Space ");
            ZZT.VideoWriteText(68, 15, 0x1f, " Plot");
            ZZT.VideoWriteText(61, 16, 0x30, "  Tab  ");
            ZZT.VideoWriteText(68, 16, 0x1f, " Draw mode");
            ZZT.VideoWriteText(61, 18, 0x70, " P ");
            ZZT.VideoWriteText(64, 18, 0x1f, " Pattern");
            ZZT.VideoWriteText(61, 19, 0x30, " C ");
            ZZT.VideoWriteText(64, 19, 0x1f, " Color:");
            for (iColor = 9; iColor <= 15; iColor += 1) {
                ZZT.VideoWriteText(61 + iColor, 22, iColor, String.fromCharCode(219));
            }
            for (iPattern = 1; iPattern <= ZZT.EditorPatternCount; iPattern += 1) {
                ZZT.VideoWriteText(61 + iPattern, 22, 0x0f, ZZT.ElementDefs[ZZT.EditorPatterns[iPattern]].Character);
            }
            copiedCharCode = getCopiedPreviewCharCode();
            ZZT.VideoWriteText(62 + ZZT.EditorPatternCount, 22, copiedTile.Color, String.fromCharCode(copiedCharCode));
            ZZT.VideoWriteText(61, 24, 0x1f, " Mode:");
        }
        function editorUpdateSidebar() {
            var modeLabel;
            var modeColor;
            var colorNameIdx = cursorColor - 8;
            var colorName = "";
            if (drawMode === DRAW_MODE_ON) {
                modeLabel = "Drawing on ";
                modeColor = 0x9e;
            }
            else if (drawMode === DRAW_MODE_TEXT) {
                modeLabel = "Text entry ";
                modeColor = 0x9e;
            }
            else {
                modeLabel = "Drawing off";
                modeColor = 0x1e;
            }
            if (colorNameIdx >= 0 && colorNameIdx < colorNames.length) {
                colorName = colorNames[colorNameIdx];
            }
            ZZT.VideoWriteText(72, 19, 0x1e, padRight(colorName, 7));
            ZZT.VideoWriteText(62, 21, 0x1f, "       ");
            ZZT.VideoWriteText(69, 21, 0x1f, "        ");
            ZZT.VideoWriteText(61 + cursorPattern, 21, 0x1f, String.fromCharCode(31));
            ZZT.VideoWriteText(61 + cursorColor, 21, 0x1f, String.fromCharCode(31));
            ZZT.VideoWriteText(68, 24, modeColor, modeLabel);
        }
        function editorSetAndCopyTile(x, y, element, color) {
            ZZT.Board.Tiles[x][y].Element = element;
            ZZT.Board.Tiles[x][y].Color = color;
            copiedTile = ZZT.cloneTile(ZZT.Board.Tiles[x][y]);
            copiedHasStat = false;
            copiedX = x;
            copiedY = y;
            editorDrawTileAndNeighborsAt(x, y);
        }
        function editorDrawRefresh() {
            TransitionDrawToBoard();
            editorDrawSidebar();
            if (ZZT.Board.Name.length > 0) {
                ZZT.VideoWriteText(Math.floor((59 - ZZT.Board.Name.length) / 2), 0, 0x70, " " + ZZT.Board.Name + " ");
            }
            else {
                ZZT.VideoWriteText(26, 0, 0x70, " Untitled ");
            }
        }
        function editorPrepareModifyTile(x, y) {
            var result;
            wasModified = true;
            result = BoardPrepareTileForPlacement(x, y);
            editorDrawTileAndNeighborsAt(x, y);
            return result;
        }
        function editorPrepareModifyStatAtCursor() {
            if (ZZT.Board.StatCount >= ZZT.MAX_STAT) {
                return false;
            }
            return editorPrepareModifyTile(cursorX, cursorY);
        }
        function editorCreateStatTemplate(element) {
            var stat = ZZT.createDefaultStat();
            if (ZZT.ElementDefs[element].Param1Name.length > 0) {
                stat.P1 = ZZT.World.EditorStatSettings[element].P1;
            }
            if (ZZT.ElementDefs[element].Param2Name.length > 0) {
                stat.P2 = ZZT.World.EditorStatSettings[element].P2;
            }
            if (ZZT.ElementDefs[element].ParamDirName.length > 0) {
                stat.StepX = ZZT.World.EditorStatSettings[element].StepX;
                stat.StepY = ZZT.World.EditorStatSettings[element].StepY;
            }
            if (ZZT.ElementDefs[element].ParamBoardName.length > 0) {
                stat.P3 = ZZT.World.EditorStatSettings[element].P3;
            }
            return stat;
        }
        function editorPlaceTile(x, y) {
            var element;
            if (cursorPattern > ZZT.EditorPatternCount && copiedHasStat) {
                if (editorPrepareModifyStatAtCursor()) {
                    AddStat(x, y, copiedTile.Element, copiedTile.Color, copiedStat.Cycle, copiedStat);
                    editorDrawTileAndNeighborsAt(x, y);
                }
                return;
            }
            if (cursorPattern <= ZZT.EditorPatternCount) {
                element = ZZT.EditorPatterns[cursorPattern];
                if (editorPrepareModifyTile(x, y)) {
                    ZZT.Board.Tiles[x][y].Element = element;
                    ZZT.Board.Tiles[x][y].Color = cursorColor;
                }
            }
            else {
                if (editorPrepareModifyTile(x, y)) {
                    ZZT.Board.Tiles[x][y] = ZZT.cloneTile(copiedTile);
                }
            }
            editorDrawTileAndNeighborsAt(x, y);
        }
        function editorSelectElementMenu(selectedCategory) {
            var iElem;
            var y = 3;
            var menuColor;
            var selectedKey;
            var selectedKeyUpper;
            var statTemplate;
            var shortcutLabel;
            ZZT.VideoWriteText(cursorX - 1, cursorY - 1, 0x0f, String.fromCharCode(197));
            for (i = 3; i <= 20; i += 1) {
                SidebarClearLine(i);
            }
            for (iElem = 0; iElem <= ZZT.MAX_ELEMENT; iElem += 1) {
                if (ZZT.ElementDefs[iElem].EditorCategory !== selectedCategory) {
                    continue;
                }
                if (ZZT.ElementDefs[iElem].CategoryName.length > 0) {
                    y += 1;
                    if (y <= 20) {
                        ZZT.VideoWriteText(65, y, 0x1e, ZZT.ElementDefs[iElem].CategoryName);
                    }
                    y += 1;
                }
                if (y > 20) {
                    break;
                }
                shortcutLabel = ZZT.ElementDefs[iElem].EditorShortcut.length > 0
                    ? ZZT.ElementDefs[iElem].EditorShortcut
                    : " ";
                ZZT.VideoWriteText(61, y, ((y % 2) << 6) + 0x30, " " + shortcutLabel + " ");
                ZZT.VideoWriteText(65, y, 0x1f, padRight(ZZT.ElementDefs[iElem].Name, 12));
                menuColor = editorElementMenuColor(iElem);
                ZZT.VideoWriteText(78, y, menuColor, ZZT.ElementDefs[iElem].Character);
                y += 1;
            }
            ZZT.InputReadWaitKey();
            selectedKey = ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0) : String.fromCharCode(0);
            selectedKeyUpper = upperCase(selectedKey);
            for (iElem = 1; iElem <= ZZT.MAX_ELEMENT; iElem += 1) {
                if (ZZT.ElementDefs[iElem].EditorCategory !== selectedCategory) {
                    continue;
                }
                if (ZZT.ElementDefs[iElem].EditorShortcut !== selectedKeyUpper) {
                    continue;
                }
                if (iElem === ZZT.E_PLAYER) {
                    if (editorPrepareModifyTile(cursorX, cursorY)) {
                        MoveStat(0, cursorX, cursorY);
                    }
                }
                else {
                    menuColor = editorElementPlacementColor(iElem);
                    if (ZZT.ElementDefs[iElem].Cycle === -1) {
                        if (editorPrepareModifyTile(cursorX, cursorY)) {
                            editorSetAndCopyTile(cursorX, cursorY, iElem, menuColor);
                        }
                    }
                    else {
                        if (editorPrepareModifyStatAtCursor()) {
                            var previousModified = wasModified;
                            statTemplate = editorCreateStatTemplate(iElem);
                            AddStat(cursorX, cursorY, iElem, menuColor, ZZT.ElementDefs[iElem].Cycle, statTemplate);
                            if (!editorEditStat(ZZT.Board.StatCount)) {
                                RemoveStat(ZZT.Board.StatCount);
                                wasModified = previousModified;
                            }
                            else {
                                editorDrawTileAndNeighborsAt(cursorX, cursorY);
                            }
                        }
                    }
                }
                break;
            }
            editorDrawSidebar();
        }
        function editorTryPlaceTextCharacter(ch) {
            var code;
            var textElement;
            if (ch.length <= 0) {
                return false;
            }
            code = ch.charCodeAt(0);
            if (code < 32 || code >= 128) {
                return false;
            }
            if (editorPrepareModifyTile(cursorX, cursorY)) {
                textElement = (cursorColor - 9) + ZZT.E_TEXT_MIN;
                if (textElement < ZZT.E_TEXT_MIN) {
                    textElement = ZZT.E_TEXT_MIN;
                }
                else if (textElement > ZZT.E_TEXT_WHITE) {
                    textElement = ZZT.E_TEXT_WHITE;
                }
                ZZT.Board.Tiles[cursorX][cursorY].Element = textElement;
                ZZT.Board.Tiles[cursorX][cursorY].Color = code & 0xff;
                editorDrawTileAndNeighborsAt(cursorX, cursorY);
                if (cursorX < ZZT.BOARD_WIDTH) {
                    cursorX += 1;
                }
            }
            return true;
        }
        function editorPromptSlider(editable, x, y, promptText, value) {
            var parsedPrompt = promptText;
            var startChar = "1";
            var endChar = "9";
            var key;
            var newValue;
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
            ZZT.VideoWriteText(x, y, (editable ? 1 : 0) + 0x1e, parsedPrompt);
            SidebarClearLine(y + 1);
            SidebarClearLine(y + 2);
            ZZT.VideoWriteText(x, y + 2, 0x1e, startChar + "....:...." + endChar);
            while (!ZZT.runtime.isTerminated()) {
                if (!editable) {
                    break;
                }
                ZZT.VideoWriteText(x + value + 1, y + 1, 0x9f, String.fromCharCode(31));
                ZZT.InputReadWaitKey();
                key = ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0) : String.fromCharCode(0);
                if (key >= "1" && key <= "9") {
                    value = key.charCodeAt(0) - "1".charCodeAt(0);
                    SidebarClearLine(y + 1);
                }
                else {
                    newValue = value + ZZT.InputDeltaX;
                    if (newValue >= 0 && newValue <= 8 && newValue !== value) {
                        value = newValue;
                        SidebarClearLine(y + 1);
                    }
                }
                if (key === ZZT.KEY_ENTER || key === ZZT.KEY_ESCAPE || ZZT.InputShiftPressed) {
                    break;
                }
            }
            ZZT.VideoWriteText(x + value + 1, y + 1, 0x1f, String.fromCharCode(31));
            return value;
        }
        function editorPromptChoice(editable, y, prompt, choiceStr, result) {
            var starts = [];
            var iChar;
            var isStart;
            var key;
            var newResult;
            var markerX;
            SidebarClearLine(y);
            SidebarClearLine(y + 1);
            SidebarClearLine(y + 2);
            ZZT.VideoWriteText(63, y, (editable ? 1 : 0) + 0x1e, prompt);
            ZZT.VideoWriteText(63, y + 2, 0x1e, choiceStr);
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
            while (!ZZT.runtime.isTerminated()) {
                markerX = 63 + starts[result];
                if (editable) {
                    ZZT.VideoWriteText(markerX, y + 1, 0x9f, String.fromCharCode(31));
                    ZZT.InputReadWaitKey();
                    key = ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0) : String.fromCharCode(0);
                    newResult = result + ZZT.InputDeltaX;
                    if (newResult >= 0 && newResult < starts.length && newResult !== result) {
                        result = newResult;
                        SidebarClearLine(y + 1);
                    }
                }
                else {
                    break;
                }
                if (key === ZZT.KEY_ENTER || key === ZZT.KEY_ESCAPE || ZZT.InputShiftPressed) {
                    break;
                }
            }
            markerX = 63 + starts[result];
            ZZT.VideoWriteText(markerX, y + 1, 0x1f, String.fromCharCode(31));
            return result;
        }
        function editorPromptDirection(editable, y, prompt, deltaX, deltaY) {
            var choice;
            if (deltaY === -1) {
                choice = 0;
            }
            else if (deltaY === 1) {
                choice = 1;
            }
            else if (deltaX === -1) {
                choice = 2;
            }
            else {
                choice = 3;
            }
            choice = editorPromptChoice(editable, y, prompt, String.fromCharCode(24) + " " + String.fromCharCode(25) + " " + String.fromCharCode(27) + " " + String.fromCharCode(26), choice);
            return {
                DeltaX: NeighborDeltaX[choice],
                DeltaY: NeighborDeltaY[choice]
            };
        }
        function editorPromptCharacter(editable, x, y, prompt, value) {
            var iChar;
            var drawCode;
            var key;
            var newValue;
            SidebarClearLine(y);
            ZZT.VideoWriteText(x, y, (editable ? 1 : 0) + 0x1e, prompt);
            SidebarClearLine(y + 1);
            ZZT.VideoWriteText(x + 5, y + 1, 0x9f, String.fromCharCode(31));
            SidebarClearLine(y + 2);
            while (!ZZT.runtime.isTerminated()) {
                for (iChar = value - 4; iChar <= value + 4; iChar += 1) {
                    drawCode = (iChar + 256) % 256;
                    ZZT.VideoWriteText(((x + iChar) - value) + 5, y + 2, 0x1e, String.fromCharCode(drawCode));
                }
                if (!editable) {
                    break;
                }
                ZZT.InputReadWaitKey();
                key = ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0) : String.fromCharCode(0);
                if (key === ZZT.KEY_TAB) {
                    newValue = (value + 9) & 0xff;
                }
                else {
                    newValue = (value + ZZT.InputDeltaX + 256) % 256;
                }
                if (newValue !== value) {
                    value = newValue;
                    SidebarClearLine(y + 2);
                }
                if (key === ZZT.KEY_ENTER || key === ZZT.KEY_ESCAPE || ZZT.InputShiftPressed) {
                    break;
                }
            }
            ZZT.VideoWriteText(x + 5, y + 1, 0x1f, String.fromCharCode(31));
            return value;
        }
        function editorPromptNumericValue(prompt, currentValue, width, maxValue) {
            var valueStr;
            var parsed;
            SidebarClearLine(3);
            SidebarClearLine(4);
            SidebarClearLine(5);
            ZZT.VideoWriteText(63, 4, 0x1f, padRight(prompt, 17));
            valueStr = PromptString(63, 5, 0x1e, 0x0f, width, PROMPT_NUMERIC, String(currentValue));
            SidebarClearLine(3);
            SidebarClearLine(4);
            SidebarClearLine(5);
            if (ZZT.InputKeyPressed === ZZT.KEY_ESCAPE || valueStr.length <= 0) {
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
        function editorCopyStatDataToTextWindow(statId, state) {
            var stat = ZZT.Board.Stats[statId];
            var data = stat.Data === null ? "" : stat.Data;
            var line = "";
            var iData;
            var ch;
            if (data.length <= 0 || stat.DataLen <= 0) {
                ZZT.TextWindowAppend(state, "");
                return;
            }
            for (iData = 0; iData < stat.DataLen && iData < data.length; iData += 1) {
                ch = data.charAt(iData);
                if (ch === "\r" || ch === "\n") {
                    ZZT.TextWindowAppend(state, line);
                    line = "";
                    if (ch === "\r" && (iData + 1) < data.length && data.charAt(iData + 1) === "\n") {
                        iData += 1;
                    }
                }
                else if (ch !== String.fromCharCode(0)) {
                    line += ch;
                }
            }
            if (line.length > 0 || state.LineCount <= 0) {
                ZZT.TextWindowAppend(state, line);
            }
        }
        function editorStoreTextWindowIntoStatData(statId, state) {
            var iLine;
            var data = "";
            var stat = ZZT.Board.Stats[statId];
            for (iLine = 1; iLine <= state.LineCount; iLine += 1) {
                data += state.Lines[iLine - 1] + "\r";
            }
            stat.Data = data;
            stat.DataLen = data.length;
            stat.DataPos = 0;
        }
        function editorEditStatText(statId, prompt) {
            var state = {
                Selectable: false,
                LineCount: 0,
                LinePos: 1,
                Lines: [],
                Hyperlink: "",
                Title: prompt,
                LoadedFilename: "",
                ScreenCopy: []
            };
            ZZT.TextWindowInitState(state);
            ZZT.TextWindowDrawOpen(state);
            editorCopyStatDataToTextWindow(statId, state);
            ZZT.TextWindowEdit(state);
            editorStoreTextWindowIntoStatData(statId, state);
            ZZT.TextWindowFree(state);
            ZZT.TextWindowDrawClose(state);
            ZZT.InputKeyPressed = String.fromCharCode(0);
            BoardDrawTile(ZZT.Board.Stats[statId].X, ZZT.Board.Stats[statId].Y);
        }
        function editorCategoryNameForElement(element) {
            var categoryName = "";
            var iElem;
            for (iElem = 0; iElem <= element; iElem += 1) {
                if (ZZT.ElementDefs[iElem].EditorCategory === ZZT.ElementDefs[element].EditorCategory &&
                    ZZT.ElementDefs[iElem].CategoryName.length > 0) {
                    categoryName = ZZT.ElementDefs[iElem].CategoryName;
                }
            }
            return categoryName;
        }
        function editorEditStat(statId) {
            var stat;
            var element;
            var promptByte;
            var dir;
            var selectedBoard;
            var iy;
            var boardName;
            function editorEditStatSettings(selected) {
                ZZT.InputKeyPressed = String.fromCharCode(0);
                iy = 9;
                if (ZZT.ElementDefs[element].Param1Name.length > 0) {
                    if (ZZT.ElementDefs[element].ParamTextName.length === 0) {
                        promptByte = stat.P1 & 0xff;
                        if (promptByte > 8) {
                            promptByte = 8;
                        }
                        promptByte = editorPromptSlider(selected, 63, iy, ZZT.ElementDefs[element].Param1Name, promptByte);
                        if (selected) {
                            stat.P1 = promptByte;
                            ZZT.World.EditorStatSettings[element].P1 = stat.P1;
                        }
                    }
                    else {
                        if (stat.P1 === 0) {
                            stat.P1 = ZZT.World.EditorStatSettings[element].P1;
                        }
                        BoardDrawTile(stat.X, stat.Y);
                        stat.P1 = editorPromptCharacter(selected, 63, iy, ZZT.ElementDefs[element].Param1Name, stat.P1 & 0xff);
                        BoardDrawTile(stat.X, stat.Y);
                        if (selected) {
                            ZZT.World.EditorStatSettings[element].P1 = stat.P1;
                        }
                    }
                    iy += 4;
                }
                if (ZZT.InputKeyPressed !== ZZT.KEY_ESCAPE && ZZT.ElementDefs[element].ParamTextName.length > 0) {
                    if (selected) {
                        editorEditStatText(statId, ZZT.ElementDefs[element].ParamTextName);
                    }
                }
                if (ZZT.InputKeyPressed !== ZZT.KEY_ESCAPE && ZZT.ElementDefs[element].Param2Name.length > 0) {
                    promptByte = stat.P2 % 0x80;
                    if (promptByte > 8) {
                        promptByte = 8;
                    }
                    promptByte = editorPromptSlider(selected, 63, iy, ZZT.ElementDefs[element].Param2Name, promptByte);
                    if (selected) {
                        stat.P2 = (stat.P2 & 0x80) + promptByte;
                        ZZT.World.EditorStatSettings[element].P2 = stat.P2;
                    }
                    iy += 4;
                }
                if (ZZT.InputKeyPressed !== ZZT.KEY_ESCAPE && ZZT.ElementDefs[element].ParamBulletTypeName.length > 0) {
                    promptByte = Math.floor(stat.P2 / 0x80);
                    promptByte = editorPromptChoice(selected, iy, ZZT.ElementDefs[element].ParamBulletTypeName, "Bullets Stars", promptByte);
                    if (selected) {
                        stat.P2 = (stat.P2 % 0x80) + (promptByte * 0x80);
                        ZZT.World.EditorStatSettings[element].P2 = stat.P2;
                    }
                    iy += 4;
                }
                if (ZZT.InputKeyPressed !== ZZT.KEY_ESCAPE && ZZT.ElementDefs[element].ParamDirName.length > 0) {
                    dir = editorPromptDirection(selected, iy, ZZT.ElementDefs[element].ParamDirName, stat.StepX, stat.StepY);
                    if (selected) {
                        stat.StepX = dir.DeltaX;
                        stat.StepY = dir.DeltaY;
                        ZZT.World.EditorStatSettings[element].StepX = stat.StepX;
                        ZZT.World.EditorStatSettings[element].StepY = stat.StepY;
                    }
                    iy += 4;
                }
                if (ZZT.InputKeyPressed !== ZZT.KEY_ESCAPE && ZZT.ElementDefs[element].ParamBoardName.length > 0) {
                    if (selected) {
                        selectedBoard = editorSelectBoard(ZZT.ElementDefs[element].ParamBoardName, stat.P3, true);
                        if (selectedBoard >= 0) {
                            stat.P3 = selectedBoard;
                            if (stat.P3 > ZZT.World.BoardCount) {
                                editorAppendBoard();
                                copiedHasStat = false;
                                copiedTile = ZZT.createTile(ZZT.E_EMPTY, 0x0f);
                                copiedX = cursorX;
                                copiedY = cursorY;
                            }
                            ZZT.World.EditorStatSettings[element].P3 = stat.P3;
                        }
                        else {
                            ZZT.InputKeyPressed = ZZT.KEY_ESCAPE;
                        }
                    }
                    else {
                        boardName = editorGetBoardName(stat.P3, true);
                        ZZT.VideoWriteText(63, iy, 0x1f, padRight("Room: " + boardName, 17));
                    }
                }
            }
            if (statId < 0 || statId > ZZT.Board.StatCount) {
                return false;
            }
            stat = ZZT.Board.Stats[statId];
            element = ZZT.Board.Tiles[stat.X][stat.Y].Element;
            SidebarClear();
            ZZT.VideoWriteText(64, 6, 0x1e, padRight(editorCategoryNameForElement(element), 15));
            ZZT.VideoWriteText(64, 7, 0x1f, padRight(ZZT.ElementDefs[element].Name, 15));
            editorEditStatSettings(false);
            editorEditStatSettings(true);
            if (ZZT.InputKeyPressed === ZZT.KEY_ESCAPE) {
                editorDrawSidebar();
                return false;
            }
            copiedHasStat = true;
            copiedStat = ZZT.cloneStat(stat);
            copiedTile = ZZT.cloneTile(ZZT.Board.Tiles[stat.X][stat.Y]);
            copiedX = stat.X;
            copiedY = stat.Y;
            wasModified = true;
            editorDrawSidebar();
            return true;
        }
        function editorOpenEditTextWindow(state) {
            SidebarClear();
            ZZT.VideoWriteText(61, 4, 0x30, " Return ");
            ZZT.VideoWriteText(64, 5, 0x1f, " Insert line");
            ZZT.VideoWriteText(61, 7, 0x70, " Ctrl-Y ");
            ZZT.VideoWriteText(64, 8, 0x1f, " Delete line");
            ZZT.VideoWriteText(61, 10, 0x30, " Cursor keys ");
            ZZT.VideoWriteText(64, 11, 0x1f, " Move cursor");
            ZZT.VideoWriteText(61, 13, 0x70, " Insert ");
            ZZT.VideoWriteText(64, 14, 0x1f, " Insert mode");
            ZZT.VideoWriteText(61, 16, 0x30, " Delete ");
            ZZT.VideoWriteText(64, 17, 0x1f, " Delete char");
            ZZT.VideoWriteText(61, 19, 0x70, " Escape ");
            ZZT.VideoWriteText(64, 20, 0x1f, " Exit editor");
            ZZT.TextWindowEdit(state);
        }
        function editorEditHelpFile() {
            var filename = "";
            var textWindow = {
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
            if (ZZT.InputKeyPressed === ZZT.KEY_ESCAPE || filename.length <= 0) {
                return;
            }
            ZZT.TextWindowOpenFile("*" + filename + ".HLP", textWindow);
            textWindow.Title = "Editing " + filename;
            ZZT.TextWindowDrawOpen(textWindow);
            editorOpenEditTextWindow(textWindow);
            ZZT.TextWindowSaveFile(ZZT.execPath(filename + ".HLP"), textWindow);
            ZZT.TextWindowFree(textWindow);
            ZZT.TextWindowDrawClose(textWindow);
            editorDrawRefresh();
        }
        function editorTransferBoard() {
            var mode = 0;
            var path;
            var fileBytes;
            var boardData;
            var boardLen;
            var currentBoardId = ZZT.World.Info.CurrentBoard;
            mode = editorPromptChoice(true, 3, "Transfer board:", "Import Export", mode);
            if (ZZT.InputKeyPressed === ZZT.KEY_ESCAPE) {
                editorDrawSidebar();
                return;
            }
            ZZT.SavedBoardFileName = SidebarPromptString(mode === 0 ? "Import board" : "Export board", ".BRD", ZZT.SavedBoardFileName, PROMPT_ALPHANUM);
            if (ZZT.InputKeyPressed === ZZT.KEY_ESCAPE || ZZT.SavedBoardFileName.length <= 0) {
                editorDrawSidebar();
                return;
            }
            path = ZZT.execPath(ZZT.SavedBoardFileName + ".BRD");
            if (mode === 0) {
                fileBytes = ZZT.runtime.readBinaryFile(path);
                if (fileBytes === null) {
                    fileBytes = ZZT.runtime.readBinaryFile(ZZT.SavedBoardFileName + ".BRD");
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
                ZZT.World.BoardLen[currentBoardId] = boardData.length;
                ZZT.World.BoardData[currentBoardId] = boardData;
                BoardOpen(currentBoardId);
                ZZT.Board.Info.NeighborBoards[0] = 0;
                ZZT.Board.Info.NeighborBoards[1] = 0;
                ZZT.Board.Info.NeighborBoards[2] = 0;
                ZZT.Board.Info.NeighborBoards[3] = 0;
                wasModified = true;
                editorDrawRefresh();
            }
            else {
                BoardClose();
                boardData = ZZT.World.BoardData[currentBoardId] === null ? [] : ZZT.World.BoardData[currentBoardId];
                boardLen = boardData.length;
                fileBytes = [];
                pushUInt16(fileBytes, boardLen);
                appendBytes(fileBytes, boardData);
                ZZT.runtime.writeBinaryFile(path, fileBytes);
                BoardOpen(currentBoardId);
                editorDrawSidebar();
            }
        }
        function editorFloodFill(startX, startY, from) {
            var xQueue = [];
            var yQueue = [];
            var toFill = 1;
            var filled = 0;
            var x = startX;
            var y = startY;
            var n;
            var tileAt;
            var nx;
            var ny;
            xQueue[1] = startX;
            yQueue[1] = startY;
            while (toFill !== filled && toFill < 255 && !ZZT.runtime.isTerminated()) {
                tileAt = ZZT.cloneTile(ZZT.Board.Tiles[x][y]);
                editorPlaceTile(x, y);
                if (ZZT.Board.Tiles[x][y].Element !== tileAt.Element || ZZT.Board.Tiles[x][y].Color !== tileAt.Color) {
                    for (n = 0; n < 4 && toFill < 255; n += 1) {
                        nx = x + NeighborDeltaX[n];
                        ny = y + NeighborDeltaY[n];
                        if (ZZT.Board.Tiles[nx][ny].Element === from.Element &&
                            (from.Element === ZZT.E_EMPTY || ZZT.Board.Tiles[nx][ny].Color === from.Color)) {
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
        function editorAskSaveChanged() {
            ZZT.InputKeyPressed = String.fromCharCode(0);
            if (wasModified) {
                if (SidebarPromptYesNo("Save first? ", true)) {
                    if (ZZT.InputKeyPressed === ZZT.KEY_ESCAPE) {
                        return false;
                    }
                    GameWorldSave("Save world:", ZZT.LoadedGameFileName, ".ZZT");
                    if (ZZT.InputKeyPressed === ZZT.KEY_ESCAPE) {
                        return false;
                    }
                    wasModified = false;
                }
                if (ZZT.InputKeyPressed === ZZT.KEY_ESCAPE) {
                    return false;
                }
                ZZT.World.Info.Name = ZZT.LoadedGameFileName;
            }
            return true;
        }
        function editorAppendBoard() {
            var roomTitle;
            if (ZZT.World.BoardCount >= ZZT.MAX_BOARD) {
                return;
            }
            BoardClose();
            ZZT.World.BoardCount += 1;
            ZZT.World.Info.CurrentBoard = ZZT.World.BoardCount;
            ZZT.World.BoardLen[ZZT.World.Info.CurrentBoard] = 0;
            BoardCreate();
            TransitionDrawToBoard();
            roomTitle = PopupPromptString("Room's Title:", ZZT.Board.Name);
            if (roomTitle.length > 0) {
                ZZT.Board.Name = roomTitle;
            }
            TransitionDrawToBoard();
            wasModified = true;
        }
        function editorBoolToString(value) {
            if (value) {
                return "Yes";
            }
            return "No ";
        }
        function editorGetBoardName(boardId, titleScreenIsNone) {
            var boardData;
            var cursor;
            var copiedName;
            if (boardId === 0 && titleScreenIsNone) {
                return "None";
            }
            if (boardId === ZZT.World.Info.CurrentBoard) {
                return ZZT.Board.Name;
            }
            if (boardId < 0 || boardId > ZZT.World.BoardCount) {
                return "";
            }
            boardData = ZZT.World.BoardData[boardId];
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
        function editorSelectBoard(title, currentBoard, titleScreenIsNone) {
            var textWindow = {
                Selectable: true,
                LineCount: 0,
                LinePos: currentBoard + 1,
                Lines: [],
                Hyperlink: "",
                Title: title,
                LoadedFilename: "",
                ScreenCopy: []
            };
            var iBoard;
            for (iBoard = 0; iBoard <= ZZT.World.BoardCount; iBoard += 1) {
                ZZT.TextWindowAppend(textWindow, editorGetBoardName(iBoard, titleScreenIsNone));
            }
            ZZT.TextWindowAppend(textWindow, "Add new board");
            ZZT.TextWindowDrawOpen(textWindow);
            ZZT.TextWindowSelect(textWindow, false, false);
            ZZT.TextWindowDrawClose(textWindow);
            if (ZZT.TextWindowRejected) {
                ZZT.TextWindowFree(textWindow);
                return -1;
            }
            iBoard = textWindow.LinePos - 1;
            ZZT.TextWindowFree(textWindow);
            return iBoard;
        }
        function editorSwitchBoard() {
            var selectedBoard = editorSelectBoard("Switch boards", ZZT.World.Info.CurrentBoard, false);
            if (selectedBoard < 0) {
                return;
            }
            if (selectedBoard > ZZT.World.BoardCount) {
                if (SidebarPromptYesNo("Add new board? ", false)) {
                    editorAppendBoard();
                }
                return;
            }
            BoardChange(selectedBoard);
        }
        function editorEditBoardInfo() {
            var state = {
                Selectable: true,
                LineCount: 0,
                LinePos: 1,
                Lines: [],
                Hyperlink: "",
                Title: "Board Information",
                LoadedFilename: "",
                ScreenCopy: []
            };
            var iLine;
            var exitRequested = false;
            var selectedBoard;
            var prevLinePos;
            var neighborBoardLabels = [
                "       Board " + String.fromCharCode(24),
                "       Board " + String.fromCharCode(25),
                "       Board " + String.fromCharCode(27),
                "       Board " + String.fromCharCode(26)
            ];
            while (!exitRequested && !ZZT.runtime.isTerminated()) {
                prevLinePos = state.LinePos;
                ZZT.TextWindowInitState(state);
                state.Selectable = true;
                ZZT.TextWindowAppend(state, "         Title: " + ZZT.Board.Name);
                ZZT.TextWindowAppend(state, "      Can fire: " + String(ZZT.Board.Info.MaxShots) + " shots.");
                ZZT.TextWindowAppend(state, " Board is dark: " + editorBoolToString(ZZT.Board.Info.IsDark));
                for (iLine = 0; iLine < 4; iLine += 1) {
                    ZZT.TextWindowAppend(state, neighborBoardLabels[iLine] + ": " + editorGetBoardName(ZZT.Board.Info.NeighborBoards[iLine], true));
                }
                ZZT.TextWindowAppend(state, "Re-enter when zapped: " + editorBoolToString(ZZT.Board.Info.ReenterWhenZapped));
                ZZT.TextWindowAppend(state, "  Time limit, 0=None: " + String(ZZT.Board.Info.TimeLimitSec) + " sec.");
                ZZT.TextWindowAppend(state, "          Quit!");
                state.LinePos = prevLinePos;
                if (state.LinePos < 1) {
                    state.LinePos = 1;
                }
                if (state.LinePos > state.LineCount) {
                    state.LinePos = state.LineCount;
                }
                ZZT.TextWindowDrawOpen(state);
                ZZT.TextWindowSelect(state, false, false);
                ZZT.TextWindowDrawClose(state);
                if (ZZT.TextWindowRejected || ZZT.InputKeyPressed !== ZZT.KEY_ENTER) {
                    exitRequested = true;
                    continue;
                }
                if (state.LinePos >= 1 && state.LinePos <= 8) {
                    wasModified = true;
                }
                if (state.LinePos === 1) {
                    ZZT.Board.Name = PopupPromptString("New title for board:", ZZT.Board.Name);
                    exitRequested = true;
                }
                else if (state.LinePos === 2) {
                    ZZT.Board.Info.MaxShots = editorPromptNumericValue("Maximum shots?", ZZT.Board.Info.MaxShots, 3, 255);
                    editorDrawSidebar();
                }
                else if (state.LinePos === 3) {
                    ZZT.Board.Info.IsDark = !ZZT.Board.Info.IsDark;
                }
                else if (state.LinePos >= 4 && state.LinePos <= 7) {
                    selectedBoard = editorSelectBoard(neighborBoardLabels[state.LinePos - 4], ZZT.Board.Info.NeighborBoards[state.LinePos - 4], true);
                    if (selectedBoard >= 0) {
                        ZZT.Board.Info.NeighborBoards[state.LinePos - 4] = selectedBoard;
                        if (ZZT.Board.Info.NeighborBoards[state.LinePos - 4] > ZZT.World.BoardCount) {
                            editorAppendBoard();
                        }
                        exitRequested = true;
                    }
                }
                else if (state.LinePos === 8) {
                    ZZT.Board.Info.ReenterWhenZapped = !ZZT.Board.Info.ReenterWhenZapped;
                }
                else if (state.LinePos === 9) {
                    ZZT.Board.Info.TimeLimitSec = editorPromptNumericValue("Time limit?", ZZT.Board.Info.TimeLimitSec, 5, 32767);
                    editorDrawSidebar();
                }
                else if (state.LinePos === 10) {
                    exitRequested = true;
                }
            }
            ZZT.TextWindowFree(state);
        }
        if (ZZT.World.Info.IsSave || ZZT.WorldGetFlagPosition("SECRET") >= 0) {
            WorldUnload();
            WorldCreate();
        }
        ZZT.InitElementsEditor();
        ZZT.CurrentTick = 0;
        if (ZZT.World.Info.CurrentBoard !== 0) {
            BoardChange(ZZT.World.Info.CurrentBoard);
        }
        editorDrawRefresh();
        if (ZZT.World.BoardCount === 0) {
            editorAppendBoard();
            editorDrawRefresh();
        }
        ZZT.TickTimeCounter = currentHsecs();
        while (!editorExitRequested && !ZZT.runtime.isTerminated()) {
            if (drawMode === DRAW_MODE_ON) {
                editorPlaceTile(cursorX, cursorY);
            }
            if (useBlockingInputFallback) {
                ZZT.InputReadWaitKey();
            }
            else {
                ZZT.InputUpdate();
            }
            keyRaw = ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0) : nulKey;
            key = upperCase(keyRaw);
            if (keyRaw === nulKey && ZZT.InputDeltaX === 0 && ZZT.InputDeltaY === 0 && !ZZT.InputShiftPressed) {
                var elapsedCheck = hasTimeElapsed(ZZT.TickTimeCounter, 15);
                ZZT.TickTimeCounter = elapsedCheck.Counter;
                if (elapsedCheck.Elapsed) {
                    cursorBlinker = (cursorBlinker + 1) % 3;
                }
                if (cursorBlinker === 0) {
                    BoardDrawTile(cursorX, cursorY);
                }
                else {
                    ZZT.VideoWriteText(cursorX - 1, cursorY - 1, 0x0f, String.fromCharCode(197));
                }
                editorUpdateSidebar();
            }
            else {
                BoardDrawTile(cursorX, cursorY);
            }
            if (drawMode === DRAW_MODE_TEXT) {
                if (editorTryPlaceTextCharacter(keyRaw)) {
                    ZZT.InputKeyPressed = nulKey;
                    keyRaw = nulKey;
                    key = keyRaw;
                }
                else if (keyRaw === ZZT.KEY_BACKSPACE && cursorX > 1) {
                    if (editorPrepareModifyTile(cursorX - 1, cursorY)) {
                        cursorX -= 1;
                    }
                }
                else if (keyRaw === ZZT.KEY_ENTER || keyRaw === ZZT.KEY_ESCAPE) {
                    drawMode = DRAW_MODE_OFF;
                    ZZT.InputKeyPressed = nulKey;
                    keyRaw = nulKey;
                    key = keyRaw;
                }
            }
            if (ZZT.InputShiftPressed || keyRaw === " ") {
                ZZT.InputShiftAccepted = true;
                tileAtCursor = ZZT.Board.Tiles[cursorX][cursorY];
                if (tileAtCursor.Element === ZZT.E_EMPTY ||
                    (ZZT.ElementDefs[tileAtCursor.Element].PlaceableOnTop && copiedHasStat && cursorPattern > ZZT.EditorPatternCount) ||
                    ZZT.InputDeltaX !== 0 ||
                    ZZT.InputDeltaY !== 0) {
                    editorPlaceTile(cursorX, cursorY);
                }
                else {
                    canModify = editorPrepareModifyTile(cursorX, cursorY);
                    if (canModify) {
                        ZZT.Board.Tiles[cursorX][cursorY].Element = ZZT.E_EMPTY;
                    }
                }
            }
            if (ZZT.InputDeltaX !== 0 || ZZT.InputDeltaY !== 0) {
                cursorX += ZZT.InputDeltaX;
                cursorY += ZZT.InputDeltaY;
                if (cursorX < 1) {
                    cursorX = 1;
                }
                else if (cursorX > ZZT.BOARD_WIDTH) {
                    cursorX = ZZT.BOARD_WIDTH;
                }
                if (cursorY < 1) {
                    cursorY = 1;
                }
                else if (cursorY > ZZT.BOARD_HEIGHT) {
                    cursorY = ZZT.BOARD_HEIGHT;
                }
                ZZT.VideoWriteText(cursorX - 1, cursorY - 1, 0x0f, String.fromCharCode(197));
                if (keyRaw === nulKey && ZZT.InputJoystickEnabled && typeof mswait === "function") {
                    mswait(70);
                }
                ZZT.InputShiftAccepted = false;
            }
            if (keyRaw === "`") {
                editorDrawRefresh();
            }
            else if (key === "P") {
                ZZT.VideoWriteText(62, 21, 0x1f, "       ");
                cursorPattern += 1;
                if (cursorPattern > ZZT.EditorPatternCount + 1) {
                    cursorPattern = 1;
                }
            }
            else if (key === "C") {
                ZZT.VideoWriteText(72, 19, 0x1e, "       ");
                ZZT.VideoWriteText(69, 21, 0x1f, "        ");
                if ((cursorColor & 0x0f) < 0x0f) {
                    cursorColor += 1;
                }
                else {
                    cursorColor = (cursorColor & 0xf0) + 9;
                }
            }
            else if (key === "L") {
                if (editorAskSaveChanged()) {
                    if (GameWorldLoad(".ZZT")) {
                        if ((ZZT.World.Info.IsSave || ZZT.WorldGetFlagPosition("SECRET") >= 0) && !ZZT.DebugEnabled) {
                            showTitleNotice("Can not edit", [
                                ZZT.World.Info.IsSave ? "a saved game!" : ("  " + ZZT.World.Info.Name + "!")
                            ]);
                            WorldUnload();
                            WorldCreate();
                        }
                        wasModified = false;
                        editorDrawRefresh();
                    }
                }
                editorDrawSidebar();
            }
            else if (key === "S") {
                GameWorldSave("Save world:", ZZT.LoadedGameFileName, ".ZZT");
                if (ZZT.InputKeyPressed !== ZZT.KEY_ESCAPE) {
                    wasModified = false;
                }
                editorDrawSidebar();
            }
            else if (key === "N") {
                if (SidebarPromptYesNo("Make new world? ", false) && editorAskSaveChanged()) {
                    WorldUnload();
                    WorldCreate();
                    wasModified = false;
                    editorDrawRefresh();
                }
                editorDrawSidebar();
            }
            else if (key === "Z") {
                if (SidebarPromptYesNo("Clear board? ", false)) {
                    for (i = ZZT.Board.StatCount; i >= 1; i -= 1) {
                        RemoveStat(i);
                    }
                    BoardCreate();
                    wasModified = true;
                    editorDrawRefresh();
                }
                else {
                    editorDrawSidebar();
                }
            }
            else if (key === "B") {
                editorSwitchBoard();
                editorDrawRefresh();
                editorDrawSidebar();
            }
            else if (key === "I") {
                editorEditBoardInfo();
                editorDrawRefresh();
                editorDrawSidebar();
            }
            else if (key === "H") {
                ZZT.TextWindowDisplayFile("editor.hlp", "World editor help");
                editorDrawRefresh();
                editorDrawSidebar();
            }
            else if (key === "X") {
                editorFloodFill(cursorX, cursorY, ZZT.cloneTile(ZZT.Board.Tiles[cursorX][cursorY]));
            }
            else if (key === "!") {
                editorEditHelpFile();
                editorDrawSidebar();
            }
            else if (key === "T") {
                editorTransferBoard();
            }
            else if (key === "|" || keyRaw === "?") {
                GameDebugPrompt();
                editorDrawSidebar();
            }
            else if (keyRaw === ZZT.KEY_TAB) {
                if (drawMode === DRAW_MODE_OFF) {
                    drawMode = DRAW_MODE_ON;
                }
                else {
                    drawMode = DRAW_MODE_OFF;
                }
            }
            else if (keyRaw === ZZT.KEY_F1) {
                editorSelectElementMenu(ZZT.CATEGORY_ITEM);
            }
            else if (keyRaw === ZZT.KEY_F2) {
                editorSelectElementMenu(ZZT.CATEGORY_CREATURE);
            }
            else if (keyRaw === ZZT.KEY_F3) {
                editorSelectElementMenu(ZZT.CATEGORY_TERRAIN);
            }
            else if (keyRaw === ZZT.KEY_F4) {
                if (drawMode !== DRAW_MODE_TEXT) {
                    drawMode = DRAW_MODE_TEXT;
                }
                else {
                    drawMode = DRAW_MODE_OFF;
                }
            }
            else if (keyRaw === ZZT.KEY_ENTER) {
                var statIdAtCursor = GetStatIdAt(cursorX, cursorY);
                if (statIdAtCursor >= 0) {
                    editorEditStat(statIdAtCursor);
                    editorDrawSidebar();
                }
                else {
                    copiedHasStat = false;
                    copiedTile = ZZT.cloneTile(ZZT.Board.Tiles[cursorX][cursorY]);
                    copiedX = cursorX;
                    copiedY = cursorY;
                    cursorPattern = ZZT.EditorPatternCount + 1;
                }
            }
            else if (key === "Q" || keyRaw === ZZT.KEY_ESCAPE) {
                editorExitRequested = true;
            }
            if (editorExitRequested) {
                if (!editorAskSaveChanged()) {
                    editorExitRequested = false;
                    editorDrawSidebar();
                }
            }
            if (keyRaw === nulKey && ZZT.InputDeltaX === 0 && ZZT.InputDeltaY === 0 && !ZZT.InputShiftPressed && typeof mswait === "function") {
                mswait(1);
            }
        }
        ZZT.InputKeyPressed = String.fromCharCode(0);
        ZZT.InitElementsGame();
        TransitionDrawToBoard();
    }
    ZZT.EditorLoop = EditorLoop;
    function splitBaseName(path) {
        var p = path;
        var idx;
        var dot;
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
    function stripExtension(path, extension) {
        var upperExt = upperCase(extension);
        if (upperExt.length > 0 && path.length >= upperExt.length) {
            if (upperCase(path.slice(path.length - upperExt.length)) === upperExt) {
                return path.slice(0, path.length - upperExt.length);
            }
        }
        return path;
    }
    function normalizeSlashes(path) {
        return path.split("\\").join("/");
    }
    function toWorldIdentifier(path, extension) {
        var normalized = normalizeSlashes(path);
        var normalizedUpper = upperCase(normalized);
        var marker = "ZZT_FILES/";
        var markerPos = normalizedUpper.indexOf(marker);
        var relativeWithExt;
        if (upperCase(extension) === ".ZZT" && markerPos >= 0) {
            relativeWithExt = normalized.slice(markerPos);
            return stripExtension(relativeWithExt, extension);
        }
        return splitBaseName(path);
    }
    function ensureTrailingSlash(path) {
        var normalized = normalizeSlashes(path);
        if (normalized.length > 0 && normalized.charAt(normalized.length - 1) !== "/") {
            normalized += "/";
        }
        return normalized;
    }
    function splitLeafName(path) {
        var p = normalizeSlashes(path);
        var slashPos = p.lastIndexOf("/");
        if (slashPos >= 0) {
            return p.slice(slashPos + 1);
        }
        return p;
    }
    function trimPathSeparators(path) {
        var p = normalizeSlashes(path);
        while (p.length > 0 && p.charAt(0) === "/") {
            p = p.slice(1);
        }
        while (p.length > 0 && p.charAt(p.length - 1) === "/") {
            p = p.slice(0, p.length - 1);
        }
        return p;
    }
    function trimTrailingSlash(path) {
        var p = normalizeSlashes(path);
        while (p.length > 0 && p.charAt(p.length - 1) === "/") {
            p = p.slice(0, p.length - 1);
        }
        return p;
    }
    function worldParentRelativeDir(relativeDir) {
        var normalized = trimTrailingSlash(relativeDir);
        var slashPos = normalized.lastIndexOf("/");
        if (slashPos < 0) {
            return "";
        }
        return normalized.slice(0, slashPos + 1);
    }
    function pathStartsWithIgnoreCase(path, prefix) {
        var normalizedPath = normalizeSlashes(path);
        var normalizedPrefix = normalizeSlashes(prefix);
        return upperCase(normalizedPath).indexOf(upperCase(normalizedPrefix)) === 0;
    }
    function pathLeafFromParent(parentPath, entryPath) {
        var parent = ensureTrailingSlash(parentPath);
        var entry = normalizeSlashes(entryPath);
        var child;
        var slashPos;
        if (pathStartsWithIgnoreCase(entry, parent)) {
            child = entry.slice(parent.length);
        }
        else {
            child = splitLeafName(entry);
        }
        child = trimPathSeparators(child);
        slashPos = child.indexOf("/");
        if (slashPos >= 0) {
            child = child.slice(0, slashPos);
        }
        return child;
    }
    function worldFileMatchesExtension(path, extension) {
        var normalized = normalizeSlashes(path);
        var upperExt = upperCase(extension);
        if (upperExt.length <= 0 || normalized.length < upperExt.length) {
            return false;
        }
        return upperCase(normalized.slice(normalized.length - upperExt.length)) === upperExt;
    }
    function worldBuildIdentifier(prefix, relativeWithoutExt) {
        var normalizedPrefix = normalizeSlashes(prefix);
        var normalizedRelative = trimPathSeparators(relativeWithoutExt);
        if (normalizedPrefix.length > 0 && normalizedPrefix.charAt(normalizedPrefix.length - 1) !== "/") {
            normalizedPrefix += "/";
        }
        if (normalizedPrefix.length > 0) {
            return normalizedPrefix + normalizedRelative;
        }
        return normalizedRelative;
    }
    function addWorldBrowseRoot(roots, seen, label, absolutePath, identifierPrefix) {
        var normalizedAbsPath = ensureTrailingSlash(absolutePath);
        var normalizedPrefix = ensureTrailingSlash(normalizeSlashes(identifierPrefix));
        var seenId = upperCase(normalizedAbsPath);
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
    function getWorldBrowseRoots(extension) {
        var roots = [];
        var seen = {};
        var upperExt = upperCase(extension);
        if (upperExt === ".SAV") {
            addWorldBrowseRoot(roots, seen, "My saves", currentUserSaveDir(), "");
            return roots;
        }
        addWorldBrowseRoot(roots, seen, "Local zzt_files", ZZT.execPath("zzt_files/"), "zzt_files/");
        addWorldBrowseRoot(roots, seen, "Shared zzt_files", ZZT.execPath("../zzt_files/"), "../zzt_files/");
        if (roots.length <= 0) {
            addWorldBrowseRoot(roots, seen, "Game directory", ZZT.execPath(""), "");
            addWorldBrowseRoot(roots, seen, "Legacy RES", ZZT.execPath("reconstruction-of-zzt-master/RES/"), "reconstruction-of-zzt-master/RES/");
        }
        return roots;
    }
    function listWorldDirectoryEntries(root, relativeDir, extension) {
        var entries = [];
        var dirs = [];
        var files = [];
        var absDir = pathJoin(root.AbsolutePath, relativeDir);
        var listed = [];
        var i;
        var entryPath;
        var childNameRaw;
        var childName;
        var relPath;
        var relWithoutExt;
        var seenDirs = {};
        var seenFiles = {};
        var id;
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
        dirs.sort(function (a, b) {
            var aUpper = upperCase(a.Name);
            var bUpper = upperCase(b.Name);
            if (aUpper < bUpper) {
                return -1;
            }
            if (aUpper > bUpper) {
                return 1;
            }
            return 0;
        });
        files.sort(function (a, b) {
            var aUpper = upperCase(a.Name);
            var bUpper = upperCase(b.Name);
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
    function listFilesByExtension(extension) {
        var files = [];
        var listed = [];
        var listedPrimary = [];
        var listedSecondary = [];
        var listedTertiary = [];
        var listedLegacy = [];
        var i;
        var entryName;
        var upperExt = upperCase(extension);
        var seen = {};
        var rawName;
        if (typeof directory !== "function") {
            return files;
        }
        if (upperExt === ".SAV") {
            listedPrimary = directory(pathJoin(currentUserSaveDir(), "*"));
            listedSecondary = directory(ZZT.execPath("zzt_files/*"));
            listedTertiary = directory(ZZT.execPath("*"));
            listed = listed.concat(listedPrimary);
            listed = listed.concat(listedSecondary);
            listed = listed.concat(listedTertiary);
        }
        else {
            // Preferred world drop location: /xtrn/zzt/zzt_files (+ one subdirectory level for packs).
            listedPrimary = directory(ZZT.execPath("zzt_files/*"));
            listedSecondary = directory(ZZT.execPath("zzt_files/*/*"));
            // Shared location: /xtrn/zzt_files (+ one subdirectory level for packs).
            listedTertiary = directory(ZZT.execPath("../zzt_files/*"));
            listedLegacy = directory(ZZT.execPath("../zzt_files/*/*"));
            // Legacy fallback locations kept for compatibility.
            listed = listed.concat(listedPrimary);
            listed = listed.concat(listedSecondary);
            listed = listed.concat(listedTertiary);
            listed = listed.concat(listedLegacy);
            listed = listed.concat(directory(ZZT.execPath("*")));
            listed = listed.concat(directory(ZZT.execPath("reconstruction-of-zzt-master/RES/*")));
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
        files.sort(function (a, b) {
            if (upperExt === ".SAV") {
                if (upperCase(a) < upperCase(b)) {
                    return -1;
                }
                if (upperCase(a) > upperCase(b)) {
                    return 1;
                }
                return 0;
            }
            var aUpper = upperCase(a);
            var bUpper = upperCase(b);
            var aPreferred = aUpper.indexOf("ZZT_FILES/") === 0;
            var bPreferred = bUpper.indexOf("ZZT_FILES/") === 0;
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
    function worldDisplayName(name, extension) {
        var i;
        var nameUpper = upperCase(name);
        var fileId;
        if (nameUpper.indexOf("ZZT_FILES/") === 0) {
            return name.slice("zzt_files/".length);
        }
        if (upperCase(extension) !== ".ZZT") {
            return name;
        }
        fileId = splitBaseName(name);
        for (i = 0; i < ZZT.WorldFileDescCount; i += 1) {
            if (upperCase(ZZT.WorldFileDescKeys[i]) === upperCase(fileId)) {
                return ZZT.WorldFileDescValues[i];
            }
        }
        return name;
    }
    function gameWorldLoadFlat(extension) {
        var state = {
            Selectable: true,
            LineCount: 0,
            LinePos: 1,
            Lines: [],
            Hyperlink: "",
            Title: upperCase(extension) === ".ZZT" ? "ZZT Worlds" : "Saved Games",
            LoadedFilename: "",
            ScreenCopy: []
        };
        var files = listFilesByExtension(extension);
        var i;
        var selected;
        for (i = 0; i < files.length; i += 1) {
            ZZT.TextWindowAppend(state, worldDisplayName(files[i], extension));
        }
        ZZT.TextWindowAppend(state, "Exit");
        ZZT.TextWindowDrawOpen(state);
        ZZT.TextWindowSelect(state, false, false);
        ZZT.TextWindowDrawClose(state);
        if (ZZT.TextWindowRejected || state.LinePos >= state.LineCount) {
            ZZT.TextWindowFree(state);
            return false;
        }
        selected = files[state.LinePos - 1];
        ZZT.TextWindowFree(state);
        if (WorldLoad(selected, extension, false)) {
            TransitionDrawToFill(String.fromCharCode(219), 0x44);
            return true;
        }
        return false;
    }
    function gameWorldLoadBrowse(extension) {
        var state = {
            Selectable: true,
            LineCount: 0,
            LinePos: 1,
            Lines: [],
            Hyperlink: "",
            Title: "ZZT Worlds",
            LoadedFilename: "",
            ScreenCopy: []
        };
        var roots = getWorldBrowseRoots(extension);
        var currentRootIndex = 0;
        var currentRelativeDir = "";
        var showRootList = roots.length > 1;
        var includeParent = false;
        var entries = [];
        var selectedLine;
        var selectedLineCount;
        var entryIndex;
        var selectedEntry;
        var location;
        var i;
        if (roots.length <= 0) {
            return gameWorldLoadFlat(extension);
        }
        while (!ZZT.runtime.isTerminated()) {
            ZZT.TextWindowInitState(state);
            if (showRootList) {
                state.Title = "ZZT World Folders";
                for (i = 0; i < roots.length; i += 1) {
                    ZZT.TextWindowAppend(state, roots[i].Label);
                }
                ZZT.TextWindowAppend(state, "Exit");
            }
            else {
                entries = listWorldDirectoryEntries(roots[currentRootIndex], currentRelativeDir, extension);
                includeParent = currentRelativeDir.length > 0 || roots.length > 1;
                location = roots[currentRootIndex].Label;
                if (currentRelativeDir.length > 0) {
                    location += "/" + trimTrailingSlash(currentRelativeDir);
                }
                state.Title = "ZZT Worlds: " + location;
                if (includeParent) {
                    ZZT.TextWindowAppend(state, "[..]");
                }
                for (i = 0; i < entries.length; i += 1) {
                    if (entries[i].IsDirectory) {
                        ZZT.TextWindowAppend(state, entries[i].Name + "/");
                    }
                    else {
                        ZZT.TextWindowAppend(state, worldDisplayName(entries[i].Name, extension));
                    }
                }
                ZZT.TextWindowAppend(state, "Exit");
            }
            ZZT.TextWindowDrawOpen(state);
            ZZT.TextWindowSelect(state, false, false);
            ZZT.TextWindowDrawClose(state);
            selectedLine = state.LinePos;
            selectedLineCount = state.LineCount;
            ZZT.TextWindowFree(state);
            if (ZZT.TextWindowRejected || selectedLine >= selectedLineCount) {
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
                    }
                    else {
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
        ZZT.TextWindowFree(state);
        return false;
    }
    function GameWorldLoad(extension) {
        if (upperCase(extension) === ".ZZT") {
            return gameWorldLoadBrowse(extension);
        }
        return gameWorldLoadFlat(extension);
    }
    ZZT.GameWorldLoad = GameWorldLoad;
    function GameWorldSave(prompt, filename, extension) {
        var newFilename = SidebarPromptString(prompt, extension, filename, PROMPT_ALPHANUM);
        if (ZZT.InputKeyPressed === ZZT.KEY_ESCAPE || newFilename.length <= 0) {
            return;
        }
        if (upperCase(extension) === ".SAV") {
            ZZT.SavedGameFileName = newFilename;
        }
        else if (upperCase(extension) === ".ZZT") {
            ZZT.World.Info.Name = newFilename;
        }
        WorldSave(newFilename, extension);
    }
    ZZT.GameWorldSave = GameWorldSave;
    function GetStatIdAt(x, y) {
        var i;
        for (i = 0; i <= ZZT.Board.StatCount; i += 1) {
            if (ZZT.Board.Stats[i].X === x && ZZT.Board.Stats[i].Y === y) {
                return i;
            }
        }
        return -1;
    }
    ZZT.GetStatIdAt = GetStatIdAt;
    function BoardPrepareTileForPlacement(x, y) {
        var statId = GetStatIdAt(x, y);
        var result;
        if (statId > 0) {
            RemoveStat(statId);
            result = true;
        }
        else if (statId < 0) {
            if (!ZZT.ElementDefs[ZZT.Board.Tiles[x][y].Element].PlaceableOnTop) {
                ZZT.Board.Tiles[x][y].Element = ZZT.E_EMPTY;
                ZZT.Board.Tiles[x][y].Color = 0;
            }
            result = true;
        }
        else {
            // Player tile cannot be modified.
            result = false;
        }
        BoardDrawTile(x, y);
        return result;
    }
    ZZT.BoardPrepareTileForPlacement = BoardPrepareTileForPlacement;
    function AddStat(tx, ty, element, color, cycle, template) {
        var stat;
        if (ZZT.Board.StatCount >= ZZT.MAX_STAT) {
            return;
        }
        ZZT.Board.StatCount += 1;
        stat = ZZT.cloneStat(template);
        stat.X = tx;
        stat.Y = ty;
        stat.Cycle = cycle;
        stat.Under = ZZT.cloneTile(ZZT.Board.Tiles[tx][ty]);
        stat.DataPos = 0;
        ZZT.Board.Stats[ZZT.Board.StatCount] = stat;
        if (ZZT.ElementDefs[ZZT.Board.Tiles[tx][ty].Element].PlaceableOnTop) {
            ZZT.Board.Tiles[tx][ty].Color = (color & 0x0f) + (ZZT.Board.Tiles[tx][ty].Color & 0x70);
        }
        else {
            ZZT.Board.Tiles[tx][ty].Color = color;
        }
        ZZT.Board.Tiles[tx][ty].Element = element;
        if (ty > 0) {
            BoardDrawTile(tx, ty);
        }
    }
    ZZT.AddStat = AddStat;
    function RemoveStat(statId) {
        var stat;
        var i;
        if (statId < 0 || statId > ZZT.Board.StatCount) {
            return;
        }
        stat = ZZT.Board.Stats[statId];
        if (statId < ZZT.CurrentStatTicked) {
            ZZT.CurrentStatTicked -= 1;
        }
        ZZT.Board.Tiles[stat.X][stat.Y] = ZZT.cloneTile(stat.Under);
        if (stat.Y > 0) {
            BoardDrawTile(stat.X, stat.Y);
        }
        for (i = 0; i <= ZZT.Board.StatCount; i += 1) {
            if (ZZT.Board.Stats[i].Follower >= statId) {
                if (ZZT.Board.Stats[i].Follower === statId) {
                    ZZT.Board.Stats[i].Follower = -1;
                }
                else {
                    ZZT.Board.Stats[i].Follower -= 1;
                }
            }
            if (ZZT.Board.Stats[i].Leader >= statId) {
                if (ZZT.Board.Stats[i].Leader === statId) {
                    ZZT.Board.Stats[i].Leader = -1;
                }
                else {
                    ZZT.Board.Stats[i].Leader -= 1;
                }
            }
        }
        for (i = statId + 1; i <= ZZT.Board.StatCount; i += 1) {
            ZZT.Board.Stats[i - 1] = ZZT.cloneStat(ZZT.Board.Stats[i]);
        }
        ZZT.Board.Stats[ZZT.Board.StatCount] = ZZT.createDefaultStat();
        ZZT.Board.StatCount -= 1;
    }
    ZZT.RemoveStat = RemoveStat;
    function MoveStat(statId, newX, newY) {
        var stat = ZZT.Board.Stats[statId];
        var oldX = stat.X;
        var oldY = stat.Y;
        var oldUnder = ZZT.cloneTile(stat.Under);
        var ix;
        var iy;
        var oldLit;
        var newLit;
        stat.Under = ZZT.cloneTile(ZZT.Board.Tiles[newX][newY]);
        if (ZZT.Board.Tiles[oldX][oldY].Element === ZZT.E_PLAYER) {
            ZZT.Board.Tiles[newX][newY].Color = ZZT.Board.Tiles[oldX][oldY].Color;
        }
        else if (ZZT.Board.Tiles[newX][newY].Element === ZZT.E_EMPTY) {
            ZZT.Board.Tiles[newX][newY].Color = ZZT.Board.Tiles[oldX][oldY].Color & 0x0f;
        }
        else {
            ZZT.Board.Tiles[newX][newY].Color = (ZZT.Board.Tiles[oldX][oldY].Color & 0x0f) + (ZZT.Board.Tiles[newX][newY].Color & 0x70);
        }
        ZZT.Board.Tiles[newX][newY].Element = ZZT.Board.Tiles[oldX][oldY].Element;
        ZZT.Board.Tiles[oldX][oldY] = oldUnder;
        stat.X = newX;
        stat.Y = newY;
        BoardDrawTile(newX, newY);
        BoardDrawTile(oldX, oldY);
        if (statId === 0 && ZZT.Board.Info.IsDark && ZZT.World.Info.TorchTicks > 0) {
            if (((oldX - newX) * (oldX - newX) + (oldY - newY) * (oldY - newY)) === 1) {
                for (ix = newX - ZZT.TORCH_DX - 3; ix <= newX + ZZT.TORCH_DX + 3; ix += 1) {
                    if (ix < 1 || ix > ZZT.BOARD_WIDTH) {
                        continue;
                    }
                    for (iy = newY - ZZT.TORCH_DY - 3; iy <= newY + ZZT.TORCH_DY + 3; iy += 1) {
                        if (iy < 1 || iy > ZZT.BOARD_HEIGHT) {
                            continue;
                        }
                        oldLit = (((ix - oldX) * (ix - oldX)) + ((iy - oldY) * (iy - oldY) * 2)) < ZZT.TORCH_DIST_SQR;
                        newLit = (((ix - newX) * (ix - newX)) + ((iy - newY) * (iy - newY) * 2)) < ZZT.TORCH_DIST_SQR;
                        if (oldLit !== newLit) {
                            BoardDrawTile(ix, iy);
                        }
                    }
                }
            }
            else {
                ZZT.DrawPlayerSurroundings(oldX, oldY, 0);
                ZZT.DrawPlayerSurroundings(newX, newY, 0);
            }
        }
    }
    ZZT.MoveStat = MoveStat;
    function MovePlayerStat(newX, newY) {
        var stat = ZZT.Board.Stats[0];
        var oldX = stat.X;
        var oldY = stat.Y;
        var ix;
        var iy;
        var oldLit;
        var newLit;
        if (ZZT.Board.Tiles[oldX][oldY].Element === ZZT.E_PLAYER) {
            MoveStat(0, newX, newY);
            return;
        }
        // Preserve the current non-player tile (e.g. passage) and correctly
        // set what the player is standing over for subsequent MoveStat calls.
        stat.Under = ZZT.cloneTile(ZZT.Board.Tiles[newX][newY]);
        stat.X = newX;
        stat.Y = newY;
        ZZT.Board.Tiles[newX][newY].Element = ZZT.E_PLAYER;
        ZZT.Board.Tiles[newX][newY].Color = ZZT.ElementDefs[ZZT.E_PLAYER].Color;
        BoardDrawTile(oldX, oldY);
        BoardDrawTile(newX, newY);
        if (ZZT.Board.Info.IsDark && ZZT.World.Info.TorchTicks > 0) {
            if (((oldX - newX) * (oldX - newX) + (oldY - newY) * (oldY - newY)) === 1) {
                for (ix = newX - ZZT.TORCH_DX - 3; ix <= newX + ZZT.TORCH_DX + 3; ix += 1) {
                    if (ix < 1 || ix > ZZT.BOARD_WIDTH) {
                        continue;
                    }
                    for (iy = newY - ZZT.TORCH_DY - 3; iy <= newY + ZZT.TORCH_DY + 3; iy += 1) {
                        if (iy < 1 || iy > ZZT.BOARD_HEIGHT) {
                            continue;
                        }
                        oldLit = (((ix - oldX) * (ix - oldX)) + ((iy - oldY) * (iy - oldY) * 2)) < ZZT.TORCH_DIST_SQR;
                        newLit = (((ix - newX) * (ix - newX)) + ((iy - newY) * (iy - newY) * 2)) < ZZT.TORCH_DIST_SQR;
                        if (oldLit !== newLit) {
                            BoardDrawTile(ix, iy);
                        }
                    }
                }
            }
            else {
                ZZT.DrawPlayerSurroundings(oldX, oldY, 0);
                ZZT.DrawPlayerSurroundings(newX, newY, 0);
            }
        }
    }
    ZZT.MovePlayerStat = MovePlayerStat;
    function ElementMove(oldX, oldY, newX, newY) {
        var statId = GetStatIdAt(oldX, oldY);
        if (statId >= 0) {
            MoveStat(statId, newX, newY);
        }
        else {
            ZZT.Board.Tiles[newX][newY] = ZZT.cloneTile(ZZT.Board.Tiles[oldX][oldY]);
            ZZT.Board.Tiles[oldX][oldY] = ZZT.createTile(ZZT.E_EMPTY, 0);
            BoardDrawTile(newX, newY);
            BoardDrawTile(oldX, oldY);
        }
    }
    ZZT.ElementMove = ElementMove;
    function ElementPushablePush(x, y, deltaX, deltaY) {
        var tile = ZZT.Board.Tiles[x][y];
        var nextX = x + deltaX;
        var nextY = y + deltaY;
        var nextElement;
        if (!(((tile.Element === ZZT.E_SLIDER_NS) && (deltaX === 0)) ||
            ((tile.Element === ZZT.E_SLIDER_EW) && (deltaY === 0)) ||
            ZZT.ElementDefs[tile.Element].Pushable)) {
            return;
        }
        nextElement = ZZT.Board.Tiles[nextX][nextY].Element;
        if (!ZZT.ElementDefs[nextElement].Walkable && nextElement !== ZZT.E_PLAYER) {
            ElementPushablePush(nextX, nextY, deltaX, deltaY);
        }
        nextElement = ZZT.Board.Tiles[nextX][nextY].Element;
        if (!ZZT.ElementDefs[nextElement].Walkable &&
            ZZT.ElementDefs[nextElement].Destructible &&
            nextElement !== ZZT.E_PLAYER) {
            BoardDamageTile(nextX, nextY);
        }
        if (ZZT.ElementDefs[ZZT.Board.Tiles[nextX][nextY].Element].Walkable) {
            ElementMove(x, y, nextX, nextY);
        }
    }
    ZZT.ElementPushablePush = ElementPushablePush;
    function DisplayMessage(ticks, message) {
        var statId = GetStatIdAt(0, 0);
        var timerStat;
        if (statId !== -1) {
            RemoveStat(statId);
            BoardDrawBorder();
        }
        if (message.length <= 0) {
            ZZT.Board.Info.Message = "";
            SidebarClearLine(24);
            return;
        }
        timerStat = ZZT.createDefaultStat();
        AddStat(0, 0, ZZT.E_MESSAGE_TIMER, 0, 1, timerStat);
        ZZT.Board.Stats[ZZT.Board.StatCount].P2 = Math.floor(ticks / (ZZT.TickTimeDuration + 1));
        if (ZZT.Board.Stats[ZZT.Board.StatCount].P2 <= 0) {
            ZZT.Board.Stats[ZZT.Board.StatCount].P2 = ticks;
        }
        ZZT.Board.Info.Message = message;
    }
    ZZT.DisplayMessage = DisplayMessage;
    function GameUpdateSidebar() {
        var keyId;
        var torchBarId;
        var worldName;
        if (ZZT.GameStateElement === ZZT.E_PLAYER) {
            if (ZZT.Board.Info.TimeLimitSec > 0) {
                ZZT.VideoWriteText(64, 6, 0x1e, "   Time:");
                ZZT.VideoWriteText(72, 6, 0x1e, padRight(String(ZZT.Board.Info.TimeLimitSec - ZZT.World.Info.BoardTimeSec), 4));
            }
            else {
                SidebarClearLine(6);
            }
            if (ZZT.World.Info.Health < 0) {
                ZZT.World.Info.Health = 0;
            }
            ZZT.VideoWriteText(64, 7, 0x1e, " Health:");
            ZZT.VideoWriteText(64, 8, 0x1e, "   Ammo:");
            ZZT.VideoWriteText(64, 9, 0x1e, "Torches:");
            ZZT.VideoWriteText(64, 10, 0x1e, "   Gems:");
            ZZT.VideoWriteText(64, 11, 0x1e, "  Score:");
            ZZT.VideoWriteText(64, 12, 0x1e, "   Keys:");
            ZZT.VideoWriteText(72, 7, 0x1e, padRight(String(ZZT.World.Info.Health), 4));
            ZZT.VideoWriteText(72, 8, 0x1e, padRight(String(ZZT.World.Info.Ammo), 5));
            ZZT.VideoWriteText(72, 9, 0x1e, padRight(String(ZZT.World.Info.Torches), 4));
            ZZT.VideoWriteText(72, 10, 0x1e, padRight(String(ZZT.World.Info.Gems), 4));
            ZZT.VideoWriteText(72, 11, 0x1e, padRight(String(ZZT.World.Info.Score), 6));
            if (ZZT.World.Info.TorchTicks <= 0) {
                ZZT.VideoWriteText(75, 9, 0x16, "    ");
            }
            else {
                for (torchBarId = 2; torchBarId <= 5; torchBarId += 1) {
                    if (torchBarId <= Math.floor((ZZT.World.Info.TorchTicks * 5) / ZZT.TORCH_DURATION)) {
                        ZZT.VideoWriteText(73 + torchBarId, 9, 0x16, String.fromCharCode(177));
                    }
                    else {
                        ZZT.VideoWriteText(73 + torchBarId, 9, 0x16, String.fromCharCode(176));
                    }
                }
            }
            for (keyId = 1; keyId <= 7; keyId += 1) {
                if (ZZT.World.Info.Keys[keyId]) {
                    ZZT.VideoWriteText(71 + keyId, 12, 0x18 + keyId, ZZT.ElementDefs[ZZT.E_KEY].Character);
                }
                else {
                    ZZT.VideoWriteText(71 + keyId, 0x0c, 0x1f, " ");
                }
            }
            if (ZZT.SoundEnabled) {
                ZZT.VideoWriteText(65, 15, 0x1f, " Be quiet");
            }
            else {
                ZZT.VideoWriteText(65, 15, 0x1f, " Be noisy");
            }
        }
        else if (ZZT.GameStateElement === ZZT.E_MONITOR) {
            worldName = ZZT.World.Info.Name.length > 0 ? ZZT.World.Info.Name : "Untitled";
            ZZT.VideoWriteText(62, 5, 0x1b, "Pick a command:");
            ZZT.VideoWriteText(65, 7, 0x1e, " World:");
            ZZT.VideoWriteText(69, 8, 0x1f, padRight(worldName, 10));
        }
    }
    ZZT.GameUpdateSidebar = GameUpdateSidebar;
    function DamageStat(attackerStatId) {
        var stat;
        var oldX;
        var oldY;
        var destroyedElement;
        if (attackerStatId < 0 || attackerStatId > ZZT.Board.StatCount) {
            return;
        }
        stat = ZZT.Board.Stats[attackerStatId];
        if (attackerStatId === 0) {
            if (ZZT.World.Info.Health > 0) {
                ZZT.World.Info.Health -= 10;
                GameUpdateSidebar();
                DisplayMessage(120, "Ouch!");
                ZZT.Board.Tiles[stat.X][stat.Y].Color = 0x70 + (ZZT.ElementDefs[ZZT.E_PLAYER].Color % 0x10);
                if (ZZT.World.Info.Health > 0) {
                    ZZT.World.Info.BoardTimeSec = 0;
                    if (ZZT.Board.Info.ReenterWhenZapped) {
                        ZZT.SoundQueue(4, "\x20\x01\x23\x01\x27\x01\x30\x01\x10\x01");
                        ZZT.Board.Tiles[stat.X][stat.Y].Element = ZZT.E_EMPTY;
                        BoardDrawTile(stat.X, stat.Y);
                        oldX = stat.X;
                        oldY = stat.Y;
                        stat.X = ZZT.Board.Info.StartPlayerX;
                        stat.Y = ZZT.Board.Info.StartPlayerY;
                        ZZT.DrawPlayerSurroundings(oldX, oldY, 0);
                        ZZT.DrawPlayerSurroundings(stat.X, stat.Y, 0);
                        ZZT.GamePaused = true;
                    }
                    ZZT.SoundQueue(4, "\x10\x01\x20\x01\x13\x01\x23\x01");
                }
                else {
                    ZZT.SoundQueue(5, "\x20\x03\x23\x03\x27\x03\x30\x03\x27\x03\x2A\x03\x32\x03\x37\x03\x35\x03\x38\x03\x40\x03\x45\x03\x10\x0A");
                }
            }
            return;
        }
        destroyedElement = ZZT.Board.Tiles[stat.X][stat.Y].Element;
        if (destroyedElement === ZZT.E_BULLET) {
            ZZT.SoundQueue(3, "\x20\x01");
        }
        else if (destroyedElement !== ZZT.E_OBJECT) {
            ZZT.SoundQueue(3, "\x40\x01\x10\x01\x50\x01\x30\x01");
        }
        RemoveStat(attackerStatId);
        BoardDrawTile(stat.X, stat.Y);
    }
    ZZT.DamageStat = DamageStat;
    function BoardDamageTile(x, y) {
        var statId = GetStatIdAt(x, y);
        if (statId !== -1) {
            DamageStat(statId);
        }
        else {
            ZZT.Board.Tiles[x][y].Element = ZZT.E_EMPTY;
            BoardDrawTile(x, y);
        }
    }
    ZZT.BoardDamageTile = BoardDamageTile;
    function BoardAttack(attackerStatId, x, y) {
        if (attackerStatId === 0 && ZZT.World.Info.EnergizerTicks > 0) {
            ZZT.World.Info.Score += ZZT.ElementDefs[ZZT.Board.Tiles[x][y].Element].ScoreValue;
            GameUpdateSidebar();
        }
        else {
            DamageStat(attackerStatId);
        }
        if (attackerStatId > 0 && attackerStatId <= ZZT.CurrentStatTicked) {
            ZZT.CurrentStatTicked -= 1;
        }
        if (ZZT.Board.Tiles[x][y].Element === ZZT.E_PLAYER && ZZT.World.Info.EnergizerTicks > 0) {
            ZZT.World.Info.Score += ZZT.ElementDefs[ZZT.Board.Tiles[ZZT.Board.Stats[attackerStatId].X][ZZT.Board.Stats[attackerStatId].Y].Element].ScoreValue;
            GameUpdateSidebar();
        }
        else {
            BoardDamageTile(x, y);
            ZZT.SoundQueue(2, "\x10\x01");
        }
    }
    ZZT.BoardAttack = BoardAttack;
    function BoardShoot(element, tx, ty, deltaX, deltaY, source) {
        var targetElement = ZZT.Board.Tiles[tx + deltaX][ty + deltaY].Element;
        var sourceIsEnemy = source === ZZT.SHOT_SOURCE_ENEMY;
        if (ZZT.ElementDefs[targetElement].Walkable || targetElement === ZZT.E_WATER) {
            AddStat(tx + deltaX, ty + deltaY, element, ZZT.ElementDefs[element].Color, 1, ZZT.createDefaultStat());
            ZZT.Board.Stats[ZZT.Board.StatCount].P1 = source;
            ZZT.Board.Stats[ZZT.Board.StatCount].StepX = deltaX;
            ZZT.Board.Stats[ZZT.Board.StatCount].StepY = deltaY;
            ZZT.Board.Stats[ZZT.Board.StatCount].P2 = 100;
            return true;
        }
        if (targetElement === ZZT.E_BREAKABLE ||
            (ZZT.ElementDefs[targetElement].Destructible &&
                ((targetElement === ZZT.E_PLAYER) === sourceIsEnemy) &&
                ZZT.World.Info.EnergizerTicks <= 0)) {
            BoardDamageTile(tx + deltaX, ty + deltaY);
            ZZT.SoundQueue(2, "\x10\x01");
            return true;
        }
        return false;
    }
    ZZT.BoardShoot = BoardShoot;
    function BoardEnter() {
        ZZT.Board.Info.StartPlayerX = ZZT.Board.Stats[0].X;
        ZZT.Board.Info.StartPlayerY = ZZT.Board.Stats[0].Y;
        ZZT.SoundWorldMusicOnBoardChanged(ZZT.Board.Name);
        if (ZZT.Board.Info.IsDark && ZZT.MessageHintTorchNotShown) {
            DisplayMessage(180, "Room is dark - you need to light a torch!");
            ZZT.MessageHintTorchNotShown = false;
        }
        ZZT.World.Info.BoardTimeSec = 0;
        ZZT.World.Info.BoardTimeHsec = currentHsecs();
        GameUpdateSidebar();
    }
    ZZT.BoardEnter = BoardEnter;
    function BoardPassageTeleport(x, y) {
        var oldBoard = ZZT.World.Info.CurrentBoard;
        var col = ZZT.Board.Tiles[x][y].Color;
        var passageStat = GetStatIdAt(x, y);
        var fallbackPassageStat = -1;
        var ix;
        var iy;
        var sx;
        var sy;
        var newX = 0;
        var newY = 0;
        if (passageStat < 0) {
            for (ix = 1; ix <= ZZT.Board.StatCount; ix += 1) {
                sx = ZZT.Board.Stats[ix].X;
                sy = ZZT.Board.Stats[ix].Y;
                if (sx < 0 || sx > ZZT.BOARD_WIDTH + 1 || sy < 0 || sy > ZZT.BOARD_HEIGHT + 1) {
                    continue;
                }
                if (ZZT.Board.Tiles[sx][sy].Element !== ZZT.E_PASSAGE) {
                    continue;
                }
                if (sx === x && sy === y) {
                    passageStat = ix;
                    break;
                }
                if (ZZT.Board.Tiles[sx][sy].Color === col) {
                    if (fallbackPassageStat < 0) {
                        fallbackPassageStat = ix;
                    }
                    else {
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
        BoardChange(ZZT.Board.Stats[passageStat].P3);
        for (ix = 1; ix <= ZZT.BOARD_WIDTH; ix += 1) {
            for (iy = 1; iy <= ZZT.BOARD_HEIGHT; iy += 1) {
                if (ZZT.Board.Tiles[ix][iy].Element === ZZT.E_PASSAGE && ZZT.Board.Tiles[ix][iy].Color === col) {
                    newX = ix;
                    newY = iy;
                }
            }
        }
        ZZT.Board.Tiles[ZZT.Board.Stats[0].X][ZZT.Board.Stats[0].Y].Element = ZZT.E_EMPTY;
        ZZT.Board.Tiles[ZZT.Board.Stats[0].X][ZZT.Board.Stats[0].Y].Color = 0;
        if (newX !== 0) {
            ZZT.Board.Stats[0].X = newX;
            ZZT.Board.Stats[0].Y = newY;
        }
        else {
            BoardChange(oldBoard);
            return;
        }
        ZZT.GamePaused = true;
        ZZT.SoundQueue(4, "\x30\x01\x34\x01\x37\x01\x31\x01\x35\x01\x38\x01\x32\x01\x36\x01\x39\x01\x33\x01\x37\x01\x3A\x01\x34\x01\x38\x01\x40\x01");
        TransitionDrawBoardChange();
        BoardEnter();
    }
    ZZT.BoardPassageTeleport = BoardPassageTeleport;
    function GamePromptEndPlay() {
        if (ZZT.World.Info.Health <= 0) {
            ZZT.GamePlayExitRequested = true;
            BoardDrawBorder();
        }
        else {
            ZZT.GamePlayExitRequested = SidebarPromptYesNo("End this game? ", true);
        }
        ZZT.InputKeyPressed = String.fromCharCode(0);
    }
    ZZT.GamePromptEndPlay = GamePromptEndPlay;
    function GameDebugPrompt() {
        var input = "";
        var i;
        var toggle = true;
        var key;
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
                        ZZT.WorldSetFlag(input);
                    }
                    else {
                        ZZT.WorldClearFlag(input);
                    }
                }
            }
        }
        ZZT.DebugEnabled = ZZT.WorldGetFlagPosition("DEBUG") >= 0;
        if (input === "HEALTH") {
            ZZT.World.Info.Health += 50;
        }
        else if (input === "AMMO") {
            ZZT.World.Info.Ammo += 5;
        }
        else if (input === "KEYS") {
            for (i = 1; i <= 7; i += 1) {
                ZZT.World.Info.Keys[i] = true;
            }
        }
        else if (input === "TORCHES") {
            ZZT.World.Info.Torches += 3;
        }
        else if (input === "TIME") {
            ZZT.World.Info.BoardTimeSec -= 30;
        }
        else if (input === "GEMS") {
            ZZT.World.Info.Gems += 5;
        }
        else if (input === "DARK") {
            ZZT.Board.Info.IsDark = toggle;
            TransitionDrawToBoard();
        }
        else if (input === "ZAP") {
            for (i = 0; i < 4; i += 1) {
                BoardDamageTile(ZZT.Board.Stats[0].X + NeighborDeltaX[i], ZZT.Board.Stats[0].Y + NeighborDeltaY[i]);
                ZZT.Board.Tiles[ZZT.Board.Stats[0].X + NeighborDeltaX[i]][ZZT.Board.Stats[0].Y + NeighborDeltaY[i]].Element = ZZT.E_EMPTY;
                BoardDrawTile(ZZT.Board.Stats[0].X + NeighborDeltaX[i], ZZT.Board.Stats[0].Y + NeighborDeltaY[i]);
            }
        }
        SidebarClearLine(4);
        SidebarClearLine(5);
        ZZT.SoundQueue(10, "\x27\x04");
        GameUpdateSidebar();
    }
    ZZT.GameDebugPrompt = GameDebugPrompt;
    function drawGameSidebar() {
        SidebarClear();
        SidebarClearLine(0);
        SidebarClearLine(1);
        SidebarClearLine(2);
        ZZT.VideoWriteText(61, 0, 0x1f, "    - - - - -     ");
        ZZT.VideoWriteText(62, 1, 0x70, "      ZZT      ");
        ZZT.VideoWriteText(61, 2, 0x1f, "    - - - - -     ");
        if (ZZT.GameStateElement === ZZT.E_PLAYER) {
            ZZT.VideoWriteText(64, 7, 0x1e, " Health:");
            ZZT.VideoWriteText(64, 8, 0x1e, "   Ammo:");
            ZZT.VideoWriteText(64, 9, 0x1e, "Torches:");
            ZZT.VideoWriteText(64, 10, 0x1e, "   Gems:");
            ZZT.VideoWriteText(64, 11, 0x1e, "  Score:");
            ZZT.VideoWriteText(64, 12, 0x1e, "   Keys:");
            ZZT.VideoWriteText(62, 7, 0x1f, ZZT.ElementDefs[ZZT.E_PLAYER].Character);
            ZZT.VideoWriteText(62, 8, 0x1b, ZZT.ElementDefs[ZZT.E_AMMO].Character);
            ZZT.VideoWriteText(62, 9, 0x16, ZZT.ElementDefs[ZZT.E_TORCH].Character);
            ZZT.VideoWriteText(62, 10, 0x1b, ZZT.ElementDefs[ZZT.E_GEM].Character);
            ZZT.VideoWriteText(62, 12, 0x1f, ZZT.ElementDefs[ZZT.E_KEY].Character);
            ZZT.VideoWriteText(62, 14, 0x70, " T ");
            ZZT.VideoWriteText(65, 14, 0x1f, " Torch");
            ZZT.VideoWriteText(62, 15, 0x30, " B ");
            ZZT.VideoWriteText(62, 16, 0x70, " H ");
            ZZT.VideoWriteText(65, 16, 0x1f, " Help");
            ZZT.VideoWriteText(64, 18, 0x30, " Arrows ");
            ZZT.VideoWriteText(72, 18, 0x1f, " Move");
            ZZT.VideoWriteText(61, 19, 0x70, " Spacebar ");
            ZZT.VideoWriteText(72, 19, 0x1f, " Shoot");
            ZZT.VideoWriteText(62, 21, 0x70, " S ");
            ZZT.VideoWriteText(65, 21, 0x1f, " Save game");
            ZZT.VideoWriteText(62, 22, 0x30, " P ");
            ZZT.VideoWriteText(65, 22, 0x1f, " Pause");
            ZZT.VideoWriteText(62, 23, 0x70, " Q ");
            ZZT.VideoWriteText(65, 23, 0x1f, " Quit");
        }
        else {
            ZZT.VideoWriteText(62, 21, 0x70, " S ");
            ZZT.VideoWriteText(65, 21, 0x1e, " Game speed:");
            ZZT.VideoWriteText(77, 21, 0x1f, String(ZZT.TickSpeed));
            ZZT.VideoWriteText(62, 7, 0x30, " W ");
            ZZT.VideoWriteText(62, 11, 0x70, " P ");
            ZZT.VideoWriteText(62, 12, 0x30, " R ");
            ZZT.VideoWriteText(62, 13, 0x70, " Q ");
            ZZT.VideoWriteText(62, 16, 0x30, " A ");
            ZZT.VideoWriteText(62, 17, 0x70, " H ");
            ZZT.VideoWriteText(65, 11, 0x1f, " Play");
            ZZT.VideoWriteText(65, 12, 0x1e, " Restore game");
            ZZT.VideoWriteText(65, 13, 0x1e, " Quit");
            ZZT.VideoWriteText(65, 16, 0x1f, " About ZZT!");
            ZZT.VideoWriteText(65, 17, 0x1e, " High Scores");
            if (ZZT.EditorEnabled) {
                ZZT.VideoWriteText(62, 18, 0x30, " E ");
                ZZT.VideoWriteText(65, 18, 0x1e, " Board Editor");
            }
        }
    }
    function GamePlayLoop(boardChanged) {
        var stat;
        var statElement;
        var key;
        var context;
        var loopCounter = 0;
        var sleptThisLoop;
        var playerTickedThisLoop;
        var noInputFallback = !(typeof console !== "undefined" && typeof console.inkey === "function");
        drawGameSidebar();
        GameUpdateSidebar();
        ZZT.InputFirePressed = false;
        ZZT.InputTorchPressed = false;
        if (ZZT.JustStarted) {
            if (ZZT.StartupWorldFileName.length > 0) {
                SidebarClearLine(8);
                ZZT.VideoWriteText(69, 8, 0x1f, ZZT.StartupWorldFileName);
                if (!WorldLoad(ZZT.StartupWorldFileName, ".ZZT", true)) {
                    WorldCreate();
                }
            }
            ZZT.ReturnBoardId = ZZT.World.Info.CurrentBoard;
            BoardChange(0);
            ZZT.JustStarted = false;
        }
        ZZT.Board.Tiles[ZZT.Board.Stats[0].X][ZZT.Board.Stats[0].Y].Element = ZZT.GameStateElement;
        ZZT.Board.Tiles[ZZT.Board.Stats[0].X][ZZT.Board.Stats[0].Y].Color = ZZT.ElementDefs[ZZT.GameStateElement].Color;
        if (boardChanged) {
            TransitionDrawBoardChange();
        }
        if (ZZT.GameStateElement === ZZT.E_PLAYER) {
            ZZT.SoundWorldMusicOnBoardChanged(ZZT.Board.Name);
        }
        ZZT.TickTimeDuration = ZZT.TickSpeed * 2;
        ZZT.GamePlayExitRequested = false;
        ZZT.CurrentTick = randomInt(100);
        ZZT.CurrentStatTicked = ZZT.Board.StatCount + 1;
        ZZT.TickTimeCounter = currentHsecs();
        while (!ZZT.runtime.isTerminated()) {
            sleptThisLoop = false;
            playerTickedThisLoop = false;
            ZZT.SoundUpdate();
            if (ZZT.GamePlayExitRequested) {
                break;
            }
            if (ZZT.GamePaused) {
                ZZT.VideoWriteText(64, 5, 0x1f, "Pausing...");
                ZZT.InputUpdate();
                key = upperCase(ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0) : String.fromCharCode(0));
                if (key === ZZT.KEY_ESCAPE || key === "Q") {
                    GamePromptEndPlay();
                }
                if ((ZZT.InputDeltaX !== 0 || ZZT.InputDeltaY !== 0) && ZZT.World.Info.Health > 0) {
                    ZZT.PlayerDirX = ZZT.InputDeltaX;
                    ZZT.PlayerDirY = ZZT.InputDeltaY;
                    context = {
                        DeltaX: ZZT.InputDeltaX,
                        DeltaY: ZZT.InputDeltaY
                    };
                    invokeElementTouch(ZZT.Board.Stats[0].X + context.DeltaX, ZZT.Board.Stats[0].Y + context.DeltaY, 0, context);
                    if ((context.DeltaX !== 0 || context.DeltaY !== 0) &&
                        ZZT.ElementDefs[ZZT.Board.Tiles[ZZT.Board.Stats[0].X + context.DeltaX][ZZT.Board.Stats[0].Y + context.DeltaY].Element].Walkable) {
                        MovePlayerStat(ZZT.Board.Stats[0].X + context.DeltaX, ZZT.Board.Stats[0].Y + context.DeltaY);
                    }
                    if (!ZZT.GamePlayExitRequested) {
                        ZZT.GamePaused = false;
                        SidebarClearLine(5);
                        ZZT.CurrentTick = randomInt(100);
                        ZZT.CurrentStatTicked = ZZT.Board.StatCount + 1;
                        ZZT.World.Info.IsSave = true;
                    }
                }
                else if ((key === "P" || key === ZZT.KEY_ENTER) &&
                    ZZT.World.Info.Health > 0 &&
                    !ZZT.GamePlayExitRequested &&
                    ZZT.Board.Tiles[ZZT.Board.Stats[0].X][ZZT.Board.Stats[0].Y].Element === ZZT.E_PLAYER) {
                    // Synchronet UX tweak: allow explicit resume while paused.
                    ZZT.GamePaused = false;
                    SidebarClearLine(5);
                    ZZT.CurrentTick = randomInt(100);
                    ZZT.CurrentStatTicked = ZZT.Board.StatCount + 1;
                    ZZT.World.Info.IsSave = true;
                }
            }
            else {
                if (ZZT.GameStateElement === ZZT.E_PLAYER && ZZT.Board.StatCount >= 0) {
                    // Player timing should always be deterministic and responsive.
                    ZZT.Board.Stats[0].Cycle = 1;
                    if (ZZT.CurrentStatTicked <= ZZT.Board.StatCount) {
                        // Under heavy animation/stat churn, keep polling so movement keys don't starve.
                        ZZT.InputUpdate();
                    }
                }
                if (ZZT.GameStateElement === ZZT.E_MONITOR) {
                    // Title/monitor commands should remain responsive regardless of stat cycle timing.
                    ZZT.InputUpdate();
                }
                if (ZZT.CurrentStatTicked <= ZZT.Board.StatCount) {
                    stat = ZZT.Board.Stats[ZZT.CurrentStatTicked];
                    if (stat.Cycle !== 0 && stat.Cycle > 0 && (ZZT.CurrentTick % stat.Cycle) === (ZZT.CurrentStatTicked % stat.Cycle)) {
                        statElement = ZZT.Board.Tiles[stat.X][stat.Y].Element;
                        var tickProc = ZZT.ElementDefs[statElement].TickProc;
                        if (tickProc !== null) {
                            tickProc(ZZT.CurrentStatTicked);
                            if (ZZT.GameStateElement === ZZT.E_PLAYER && ZZT.CurrentStatTicked === 0) {
                                playerTickedThisLoop = true;
                            }
                        }
                    }
                    ZZT.CurrentStatTicked += 1;
                }
                if (ZZT.CurrentStatTicked > ZZT.Board.StatCount && !ZZT.GamePlayExitRequested) {
                    var elapsedCheck = hasTimeElapsed(ZZT.TickTimeCounter, ZZT.TickTimeDuration);
                    ZZT.TickTimeCounter = elapsedCheck.Counter;
                    if (elapsedCheck.Elapsed) {
                        ZZT.CurrentTick += 1;
                        if (ZZT.CurrentTick > 420) {
                            ZZT.CurrentTick = 1;
                        }
                        ZZT.CurrentStatTicked = 0;
                        ZZT.InputUpdate();
                    }
                    else if (typeof mswait === "function") {
                        mswait(1);
                        sleptThisLoop = true;
                    }
                }
            }
            key = upperCase(ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0) : String.fromCharCode(0));
            if (ZZT.GameStateElement === ZZT.E_PLAYER) {
                if (key === ZZT.KEY_ESCAPE || key === "Q") {
                    GamePromptEndPlay();
                    key = upperCase(ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0) : String.fromCharCode(0));
                }
                else if (key === "P" && ZZT.World.Info.Health > 0) {
                    ZZT.GamePaused = true;
                }
                if (ZZT.World.Info.Health > 0 && !ZZT.GamePaused && !ZZT.GamePlayExitRequested && !playerTickedThisLoop) {
                    tryApplyPlayerDirectionalInput();
                }
                else if (ZZT.World.Info.Health <= 0) {
                    ZZT.InputDeltaX = 0;
                    ZZT.InputDeltaY = 0;
                }
            }
            if (ZZT.GameStateElement === ZZT.E_MONITOR &&
                (key === ZZT.KEY_ESCAPE || key === "A" || key === "E" || key === "H" || key === "N" ||
                    key === "P" || key === "Q" || key === "R" || key === "S" || key === "W" || key === "|")) {
                ZZT.GamePlayExitRequested = true;
            }
            if (ShowInputDebugOverlay) {
                drawDebugStatusLine();
            }
            loopCounter += 1;
            if (noInputFallback && loopCounter > 8) {
                break;
            }
            if (!sleptThisLoop && typeof mswait === "function") {
                if (ZZT.GamePaused) {
                    mswait(20);
                }
            }
        }
        ZZT.SoundClearQueue();
        if (ZZT.GameStateElement === ZZT.E_PLAYER) {
            if (ZZT.World.Info.Health <= 0) {
                HighScoresAdd(ZZT.World.Info.Score);
            }
        }
        else if (ZZT.GameStateElement === ZZT.E_MONITOR) {
            SidebarClearLine(5);
        }
        ZZT.Board.Tiles[ZZT.Board.Stats[0].X][ZZT.Board.Stats[0].Y].Element = ZZT.E_PLAYER;
        ZZT.Board.Tiles[ZZT.Board.Stats[0].X][ZZT.Board.Stats[0].Y].Color = ZZT.ElementDefs[ZZT.E_PLAYER].Color;
        ZZT.SoundBlockQueueing = false;
    }
    ZZT.GamePlayLoop = GamePlayLoop;
    function GameTitleLoop() {
        var boardChanged = true;
        var startPlay;
        var key;
        var interactiveConsole = typeof console !== "undefined" && typeof console.inkey === "function";
        ZZT.GameTitleExitRequested = false;
        ZZT.JustStarted = true;
        ZZT.ReturnBoardId = 0;
        if (!interactiveConsole) {
            ZZT.runtime.writeLine("");
            ZZT.runtime.writeLine("Non-interactive runtime detected; skipping title loop.");
            return;
        }
        while (!ZZT.GameTitleExitRequested && !ZZT.runtime.isTerminated()) {
            BoardChange(0);
            while (!ZZT.runtime.isTerminated()) {
                ZZT.GameStateElement = ZZT.E_MONITOR;
                startPlay = false;
                ZZT.GamePaused = false;
                GamePlayLoop(boardChanged);
                boardChanged = false;
                key = upperCase(ZZT.InputKeyPressed.length > 0 ? ZZT.InputKeyPressed.charAt(0) : String.fromCharCode(0));
                if (key === "W") {
                    if (GameWorldLoad(".ZZT")) {
                        ZZT.ReturnBoardId = ZZT.World.Info.CurrentBoard;
                        boardChanged = true;
                    }
                }
                else if (key === "P") {
                    if (ZZT.World.Info.IsSave && !ZZT.DebugEnabled && ZZT.World.Info.Name.length > 0) {
                        startPlay = WorldLoad(ZZT.World.Info.Name, ".ZZT", false);
                        ZZT.ReturnBoardId = ZZT.World.Info.CurrentBoard;
                    }
                    else {
                        startPlay = true;
                    }
                    if (startPlay) {
                        BoardChange(ZZT.ReturnBoardId);
                        BoardEnter();
                    }
                }
                else if (key === "R") {
                    if (GameWorldLoad(".SAV")) {
                        ZZT.ReturnBoardId = ZZT.World.Info.CurrentBoard;
                        BoardChange(ZZT.ReturnBoardId);
                        startPlay = true;
                    }
                }
                else if (key === "S") {
                    ZZT.TickSpeed += 1;
                    if (ZZT.TickSpeed > 8) {
                        ZZT.TickSpeed = 1;
                    }
                }
                else if (key === "Q" || key === ZZT.KEY_ESCAPE) {
                    ZZT.GameTitleExitRequested = SidebarPromptYesNo("Quit ZZT? ", true);
                }
                else if (key === "A") {
                    ZZT.TextWindowDisplayFile("ABOUT.HLP", "About ZZT...");
                }
                else if (key === "H") {
                    HighScoresLoad();
                    HighScoresDisplay(1);
                }
                else if (key === "E") {
                    if (ZZT.EditorEnabled) {
                        EditorLoop();
                        ZZT.ReturnBoardId = ZZT.World.Info.CurrentBoard;
                        boardChanged = true;
                    }
                }
                else if (key === "|") {
                    GameDebugPrompt();
                }
                if (startPlay) {
                    ZZT.GameStateElement = ZZT.E_PLAYER;
                    ZZT.GamePaused = true;
                    GamePlayLoop(true);
                    boardChanged = true;
                }
                if (boardChanged || ZZT.GameTitleExitRequested) {
                    break;
                }
            }
        }
    }
    ZZT.GameTitleLoop = GameTitleLoop;
    function GamePrintRegisterMessage() {
        var entryName = "END" + String.fromCharCode(49 + randomInt(4)) + ".MSG";
        var lines = ZZT.ResourceDataLoadLines(entryName, false);
        var i;
        var line;
        var color = 0x0f;
        var iy = 0;
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
            }
            else {
                ZZT.VideoWriteText(0, iy, color, line);
            }
            iy += 1;
        }
        ZZT.VideoWriteText(28, 24, 0x1f, "Press any key to exit...");
        ZZT.InputReadWaitKey();
        ZZT.VideoWriteText(28, 24, 0x00, "                        ");
    }
    ZZT.GamePrintRegisterMessage = GamePrintRegisterMessage;
})(ZZT || (ZZT = {}));
var ZZT;
(function (ZZT) {
    function startsWithSlash(value) {
        return value.length > 0 && value.charAt(0) === "/";
    }
    function trimSpaces(value) {
        return String(value || "").replace(/^\s+/, "").replace(/\s+$/, "");
    }
    function normalizeIniKey(key) {
        return trimSpaces(key).replace(/[\s\-]+/g, "_").toUpperCase();
    }
    function stripOptionalQuotes(value) {
        var trimmed = trimSpaces(value);
        var quote;
        if (trimmed.length >= 2) {
            quote = trimmed.charAt(0);
            if ((quote === "\"" || quote === "'") && trimmed.charAt(trimmed.length - 1) === quote) {
                return trimmed.slice(1, trimmed.length - 1);
            }
        }
        return trimmed;
    }
    function normalizeAnsiMusicModeValue(value) {
        var upper = trimSpaces(value).toUpperCase();
        if (upper === "ON" || upper === "TRUE" || upper === "YES" || upper === "1") {
            return "ON";
        }
        if (upper === "AUTO" || upper === "CTERM" || upper === "SYNCTERM") {
            return "AUTO";
        }
        return "OFF";
    }
    function normalizeAnsiMusicIntroducerValue(value) {
        var upper = trimSpaces(value).toUpperCase();
        if (upper === "N" || upper === "CSI_N" || upper === "BANANSI" || upper === "BANSI") {
            return "N";
        }
        if (upper === "M" || upper === "CSI_M" || upper === "DL") {
            return "M";
        }
        if (upper === "|" || upper === "PIPE" || upper === "BAR" || upper === "CSI_PIPE") {
            return "|";
        }
        return "|";
    }
    function normalizeAnsiMusicForegroundValue(value) {
        var upper = trimSpaces(value).toUpperCase();
        if (upper === "ON" || upper === "TRUE" || upper === "YES" || upper === "1" ||
            upper === "FOREGROUND" || upper === "FG" || upper === "SYNC") {
            return true;
        }
        return false;
    }
    function applyIniOverrides() {
        var lines = ZZT.runtime.readTextFileLines(ZZT.execPath("zzt.ini"));
        var i;
        var line;
        var eqPos;
        var key;
        var value;
        if (lines.length <= 0) {
            lines = ZZT.runtime.readTextFileLines(ZZT.execPath("ZZT.INI"));
        }
        for (i = 0; i < lines.length; i += 1) {
            line = trimSpaces(lines[i]);
            if (line.length <= 0 || line.charAt(0) === ";" || line.charAt(0) === "#" || line.charAt(0) === "[") {
                continue;
            }
            eqPos = line.indexOf("=");
            if (eqPos <= 0) {
                continue;
            }
            key = normalizeIniKey(line.slice(0, eqPos));
            value = stripOptionalQuotes(line.slice(eqPos + 1));
            if (key === "HIGH_SCORE_JSON" || key === "HIGHSCORE_JSON") {
                ZZT.HighScoreJsonPath = value;
            }
            else if (key === "HIGH_SCORE_BBS" || key === "HIGHSCORE_BBS" || key === "BBS_NAME") {
                ZZT.HighScoreBbsName = value;
            }
            else if (key === "SERVER" && ZZT.HighScoreBbsName.length <= 0) {
                ZZT.HighScoreBbsName = value;
            }
            else if (key === "SAVE_ROOT" || key === "SAVES_ROOT") {
                ZZT.SaveRootPath = value;
            }
            else if (key === "ANSI_MUSIC" || key === "ANSI_MUSIC_MODE") {
                ZZT.AnsiMusicMode = normalizeAnsiMusicModeValue(value);
            }
            else if (key === "ANSI_MUSIC_INTRODUCER" || key === "ANSI_MUSIC_INTRO") {
                ZZT.AnsiMusicIntroducer = normalizeAnsiMusicIntroducerValue(value);
            }
            else if (key === "ANSI_MUSIC_FOREGROUND" || key === "ANSI_MUSIC_SYNC") {
                ZZT.AnsiMusicForeground = normalizeAnsiMusicForegroundValue(value);
            }
        }
    }
    function setDefaultWorldDescriptions() {
        ZZT.setWorldFileDescriptions(["TOWN", "DEMO", "CAVES", "DUNGEONS", "CITY", "BEST", "TOUR"], [
            "TOWN       The Town of ZZT",
            "DEMO       Demo of the ZZT World Editor",
            "CAVES      The Caves of ZZT",
            "DUNGEONS   The Dungeons of ZZT",
            "CITY       Underground City of ZZT",
            "BEST       The Best of ZZT",
            "TOUR       Guided Tour ZZT's Other Worlds"
        ]);
    }
    function parseArguments() {
        var args = ZZT.runtime.getArgv();
        for (var i = 0; i < args.length; i += 1) {
            var pArg = String(args[i] || "");
            if (pArg.length === 0) {
                continue;
            }
            if (startsWithSlash(pArg)) {
                var option = pArg.length > 1 ? pArg.charAt(1).toUpperCase() : "";
                if (option === "T") {
                    ZZT.SoundTimeCheckCounter = 0;
                    ZZT.UseSystemTimeForElapsed = false;
                }
                else if (option === "R") {
                    ZZT.ResetConfig = true;
                }
                else if (option === "D") {
                    ZZT.DebugEnabled = true;
                }
                continue;
            }
            ZZT.StartupWorldFileName = ZZT.trimWorldExtension(pArg);
        }
    }
    function showConfigHeader() {
        ZZT.runtime.clearScreen();
        ZZT.runtime.writeLine("");
        ZZT.runtime.writeLine("                                 <=-  ZZT  -=>");
        if (ZZT.ConfigRegistration.length === 0) {
            ZZT.runtime.writeLine("                             Shareware version 3.2");
        }
        else {
            ZZT.runtime.writeLine("                                  Version  3.2");
        }
        ZZT.runtime.writeLine("                            Created by Tim Sweeney");
        ZZT.runtime.writeLine("");
    }
    function gameConfigure() {
        ZZT.ParsingConfigFile = true;
        ZZT.EditorEnabled = true;
        ZZT.ConfigRegistration = "";
        ZZT.ConfigWorldFile = "";
        ZZT.HighScoreJsonPath = "";
        ZZT.HighScoreBbsName = "";
        ZZT.SaveRootPath = "";
        ZZT.AnsiMusicMode = "AUTO";
        ZZT.AnsiMusicIntroducer = "|";
        ZZT.AnsiMusicForeground = false;
        ZZT.GameVersion = "3.2";
        var cfgLines = ZZT.runtime.readTextFileLines(ZZT.execPath("zzt.cfg"));
        if (cfgLines.length > 0) {
            ZZT.ConfigWorldFile = cfgLines[0];
        }
        if (cfgLines.length > 1) {
            ZZT.ConfigRegistration = cfgLines[1];
        }
        if (ZZT.ConfigWorldFile.length > 0 && ZZT.ConfigWorldFile.charAt(0) === "*") {
            ZZT.EditorEnabled = false;
            ZZT.ConfigWorldFile = ZZT.ConfigWorldFile.slice(1);
        }
        if (ZZT.ConfigWorldFile.length !== 0) {
            ZZT.StartupWorldFileName = ZZT.ConfigWorldFile;
        }
        applyIniOverrides();
        ZZT.InputInitDevices();
        ZZT.ParsingConfigFile = false;
        showConfigHeader();
        if (!ZZT.InputConfigure()) {
            ZZT.GameTitleExitRequested = true;
            return;
        }
        if (!ZZT.VideoConfigure()) {
            ZZT.GameTitleExitRequested = true;
        }
    }
    function runGameMainLoop() {
        ZZT.VideoInstall(80, 1);
        ZZT.OrderPrintId = ZZT.GameVersion;
        ZZT.TextWindowInit(5, 3, 50, 18);
        ZZT.SoundInit();
        ZZT.VideoHideCursor();
        ZZT.runtime.clearScreen();
        ZZT.TickSpeed = 4;
        ZZT.SavedGameFileName = ZZT.BuildDefaultSaveFileName();
        ZZT.SavedBoardFileName = "TEMP";
        ZZT.GenerateTransitionTable();
        ZZT.WorldCreate();
        ZZT.GameTitleLoop();
    }
    function cleanupAndExitMessage() {
        ZZT.SoundUninstall();
        ZZT.SoundClearQueue();
        ZZT.InputRestoreDevices();
        ZZT.VideoUninstall();
        ZZT.runtime.clearScreen();
        if (ZZT.ConfigRegistration.length === 0) {
            ZZT.GamePrintRegisterMessage();
        }
        else {
            ZZT.runtime.writeLine("");
            ZZT.runtime.writeLine("  Registered version -- Thank you for playing ZZT.");
            ZZT.runtime.writeLine("");
        }
        ZZT.VideoShowCursor();
    }
    function main() {
        setDefaultWorldDescriptions();
        ZZT.StartupWorldFileName = "TOWN";
        ZZT.ResourceDataFileName = "ZZT.DAT";
        ZZT.ResetConfig = false;
        ZZT.GameTitleExitRequested = false;
        gameConfigure();
        parseArguments();
        if (!ZZT.GameTitleExitRequested && !ZZT.runtime.isTerminated()) {
            runGameMainLoop();
        }
        cleanupAndExitMessage();
    }
    ZZT.main = main;
})(ZZT || (ZZT = {}));
ZZT.main();
