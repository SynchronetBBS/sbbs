namespace ZZT {
  function startsWithSlash(value: string): boolean {
    return value.length > 0 && value.charAt(0) === "/";
  }

  function trimSpaces(value: string): string {
    return String(value || "").replace(/^\s+/, "").replace(/\s+$/, "");
  }

  function normalizeIniKey(key: string): string {
    return trimSpaces(key).replace(/[\s\-]+/g, "_").toUpperCase();
  }

  function stripOptionalQuotes(value: string): string {
    var trimmed: string = trimSpaces(value);
    var quote: string;

    if (trimmed.length >= 2) {
      quote = trimmed.charAt(0);
      if ((quote === "\"" || quote === "'") && trimmed.charAt(trimmed.length - 1) === quote) {
        return trimmed.slice(1, trimmed.length - 1);
      }
    }

    return trimmed;
  }

  function normalizeAnsiMusicModeValue(value: string): string {
    var upper: string = trimSpaces(value).toUpperCase();
    if (upper === "ON" || upper === "TRUE" || upper === "YES" || upper === "1") {
      return "ON";
    }
    if (upper === "AUTO" || upper === "CTERM" || upper === "SYNCTERM") {
      return "AUTO";
    }
    return "OFF";
  }

  function normalizeAnsiMusicIntroducerValue(value: string): string {
    var upper: string = trimSpaces(value).toUpperCase();
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

  function normalizeAnsiMusicForegroundValue(value: string): boolean {
    var upper: string = trimSpaces(value).toUpperCase();
    if (upper === "ON" || upper === "TRUE" || upper === "YES" || upper === "1" ||
        upper === "FOREGROUND" || upper === "FG" || upper === "SYNC") {
      return true;
    }
    return false;
  }

  function applyIniOverrides(): void {
    var lines: string[] = runtime.readTextFileLines(execPath("zzt.ini"));
    var i: number;
    var line: string;
    var eqPos: number;
    var key: string;
    var value: string;

    if (lines.length <= 0) {
      lines = runtime.readTextFileLines(execPath("ZZT.INI"));
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
        HighScoreJsonPath = value;
      } else if (key === "HIGH_SCORE_BBS" || key === "HIGHSCORE_BBS" || key === "BBS_NAME") {
        HighScoreBbsName = value;
      } else if (key === "SERVER" && HighScoreBbsName.length <= 0) {
        HighScoreBbsName = value;
      } else if (key === "SAVE_ROOT" || key === "SAVES_ROOT") {
        SaveRootPath = value;
      } else if (key === "ANSI_MUSIC" || key === "ANSI_MUSIC_MODE") {
        AnsiMusicMode = normalizeAnsiMusicModeValue(value);
      } else if (key === "ANSI_MUSIC_INTRODUCER" || key === "ANSI_MUSIC_INTRO") {
        AnsiMusicIntroducer = normalizeAnsiMusicIntroducerValue(value);
      } else if (key === "ANSI_MUSIC_FOREGROUND" || key === "ANSI_MUSIC_SYNC") {
        AnsiMusicForeground = normalizeAnsiMusicForegroundValue(value);
      }
    }
  }

  function setDefaultWorldDescriptions(): void {
    setWorldFileDescriptions(
      ["TOWN", "DEMO", "CAVES", "DUNGEONS", "CITY", "BEST", "TOUR"],
      [
        "TOWN       The Town of ZZT",
        "DEMO       Demo of the ZZT World Editor",
        "CAVES      The Caves of ZZT",
        "DUNGEONS   The Dungeons of ZZT",
        "CITY       Underground City of ZZT",
        "BEST       The Best of ZZT",
        "TOUR       Guided Tour ZZT's Other Worlds"
      ]
    );
  }

  function parseArguments(): void {
    var args = runtime.getArgv();
    for (var i = 0; i < args.length; i += 1) {
      var pArg = String(args[i] || "");
      if (pArg.length === 0) {
        continue;
      }

      if (startsWithSlash(pArg)) {
        var option = pArg.length > 1 ? pArg.charAt(1).toUpperCase() : "";
        if (option === "T") {
          SoundTimeCheckCounter = 0;
          UseSystemTimeForElapsed = false;
        } else if (option === "R") {
          ResetConfig = true;
        } else if (option === "D") {
          DebugEnabled = true;
        }
        continue;
      }

      StartupWorldFileName = trimWorldExtension(pArg);
    }
  }

  function showConfigHeader(): void {
    runtime.clearScreen();
    runtime.writeLine("");
    runtime.writeLine("                                 <=-  ZZT  -=>");
    if (ConfigRegistration.length === 0) {
      runtime.writeLine("                             Shareware version 3.2");
    } else {
      runtime.writeLine("                                  Version  3.2");
    }
    runtime.writeLine("                            Created by Tim Sweeney");
    runtime.writeLine("");
  }

  function gameConfigure(): void {
    ParsingConfigFile = true;
    EditorEnabled = true;
    ConfigRegistration = "";
    ConfigWorldFile = "";
    HighScoreJsonPath = "";
    HighScoreBbsName = "";
    SaveRootPath = "";
    AnsiMusicMode = "AUTO";
    AnsiMusicIntroducer = "|";
    AnsiMusicForeground = false;
    GameVersion = "3.2";

    var cfgLines = runtime.readTextFileLines(execPath("zzt.cfg"));
    if (cfgLines.length > 0) {
      ConfigWorldFile = cfgLines[0];
    }
    if (cfgLines.length > 1) {
      ConfigRegistration = cfgLines[1];
    }

    if (ConfigWorldFile.length > 0 && ConfigWorldFile.charAt(0) === "*") {
      EditorEnabled = false;
      ConfigWorldFile = ConfigWorldFile.slice(1);
    }
    if (ConfigWorldFile.length !== 0) {
      StartupWorldFileName = ConfigWorldFile;
    }

    applyIniOverrides();
    InputInitDevices();

    ParsingConfigFile = false;
    showConfigHeader();

    if (!InputConfigure()) {
      GameTitleExitRequested = true;
      return;
    }
    if (!VideoConfigure()) {
      GameTitleExitRequested = true;
    }
  }

  function runGameMainLoop(): void {
    VideoInstall(80, 1);
    OrderPrintId = GameVersion;
    TextWindowInit(5, 3, 50, 18);
    SoundInit();

    VideoHideCursor();
    runtime.clearScreen();

    TickSpeed = 4;
    SavedGameFileName = BuildDefaultSaveFileName();
    SavedBoardFileName = "TEMP";
    GenerateTransitionTable();
    WorldCreate();
    GameTitleLoop();
  }

  function cleanupAndExitMessage(): void {
    SoundUninstall();
    SoundClearQueue();
    InputRestoreDevices();
    VideoUninstall();
    runtime.clearScreen();

    if (ConfigRegistration.length === 0) {
      GamePrintRegisterMessage();
    } else {
      runtime.writeLine("");
      runtime.writeLine("  Registered version -- Thank you for playing ZZT.");
      runtime.writeLine("");
    }

    VideoShowCursor();
  }

  export function main(): void {
    setDefaultWorldDescriptions();

    StartupWorldFileName = "TOWN";
    ResourceDataFileName = "ZZT.DAT";
    ResetConfig = false;
    GameTitleExitRequested = false;

    gameConfigure();
    parseArguments();

    if (!GameTitleExitRequested && !runtime.isTerminated()) {
      runGameMainLoop();
    }

    cleanupAndExitMessage();
  }
}

ZZT.main();
