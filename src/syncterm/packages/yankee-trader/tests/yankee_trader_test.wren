import "syncterm" for Cache, FileError, Key, KeyEvent, Mouse, MouseEvent, WON,
                       WONError
import "ui_date_picker" for CalendarDate, DatePicker
import "ui_input" for SelectOnFocusInput
import "ui_widget" for Rect
import "yankee_trader_calc" for YTCalc
import "yankee_trader_state" for YTState
import "yankee_trader_tools" for YTCommands, YTText, YTUniverse
import "yankee_trader" for YankeeTrader

class YankeeTraderTest {
  static check_(condition, name) {
    if (!condition) Fiber.abort("Yankee Trader test failed: %(name)")
    __passed = __passed + 1
  }

  static near_(actual, expected) { (actual - expected).abs < 0.0001 }

  static cacheText_(name) {
    if (!Cache.contains(name)) return null
    var file = Cache.list[name]
    var result = file.open()
    if (result is FileError) Fiber.abort(result.toString)
    var value = file.read()
    file.close()
    if (value is FileError) Fiber.abort(value.toString)
    return value
  }

  static binaryCeiling_(ceiling) {
    var low = 1
    var high = 100000
    var probes = 0
    var probe = YTUniverse.nextBinarySectorProbe(low, high)
    while (probe != null) {
      if (probe <= ceiling) {
        low = probe
      } else {
        high = probe
      }
      probes = probes + 1
      probe = YTUniverse.nextBinarySectorProbe(low, high)
    }
    return [low, probes]
  }

  static run() {
    __passed = 0
    // Hook-completion popups are immediate in production. Keep this headless
    // unit suite on the deferred path so simulated hooks do not await OK.
    YankeeTrader.immediateNotices = false
    check_(YankeeTrader.promptLineMatches_(
        "Time: 179:38  Main Command (?=Help)?", "main") &&
        YankeeTrader.promptLineMatches_(
            "Time: 179:38  Main Command (?=help)?   ", "main") &&
        !YankeeTrader.promptLineMatches_(
            "Time: 179:38  Computer command (?=help)?", "main") &&
        !YankeeTrader.promptLineMatches_(
            "Time: 179:38  Main Command (?=Help)? c", "main"),
        "live send guard requires an untouched Main Command cursor line")
    var cursorLine = YankeeTrader.cursorLine_
    check_(cursorLine == null || cursorLine is String,
        "live send guard can read the host cursor line")
    // State-machine tests invoke entry points without a connected door prompt.
    YankeeTrader.promptChecks = false

    check_(YTCalc.plasmaPercent(1) == 100, "plasma first sector")
    var defaults = YTState.defaultData
    check_(defaults["schema"] == 2 && defaults["games"].count == 1 &&
        defaults["games"][0]["profile"]["maxSector"] == 3000 &&
        defaults["games"][0]["planets"].count == 0 &&
        defaults["games"][0]["profileDetection"] == null,
        "multi-game state defaults")
    check_(YTState.root["games"].count == 1,
        "persistent state static fields initialize")
    check_(!YTState.exportText.contains("\n") &&
        !(WON.deserialize(YTState.exportText) is WONError),
        "persistent and exported WON uses compact round-trippable form")
    check_(YankeeTrader.onRoute_(null) == null &&
        YankeeTrader.onSensor_(null) == null &&
        YankeeTrader.onPathResult_(null) == null &&
        YankeeTrader.onHelp_(null) == null &&
        YankeeTrader.onMainPrompt_(null) == null &&
        YankeeTrader.onMore_(null) == null &&
        YankeeTrader.onComputerPrompt_(null) == null &&
        YankeeTrader.onComputerDeactivated_(null) == null &&
        YankeeTrader.onNoTurns_(null) == null,
        "connected hook static fields initialize")
    var mapper = YankeeTrader.c10Status_
    check_(!mapper["active"] && mapper["total"] == 0,
        "C10 mapper timing state initializes")
    var detector = YankeeTrader.detectStatus_
    check_(!detector["active"] && detector["low"] == 1 &&
        detector["high"] == 100000, "settings detector state initializes")
    check_(YankeeTrader.benchmarkSummary_({}) == "none recorded",
        "dashboard reports no benchmark results compactly")
    var benchmarkSummary = YankeeTrader.benchmarkSummary_({
      "t1": 1.234567890123, "t2": 20, "t3": 300.099999999999
    })
    check_(benchmarkSummary == "T1 1.2  T2 20.0  T3 300.1" &&
        benchmarkSummary.count == 25,
        "dashboard limits all benchmark values to one decimal place")
    check_(YankeeTrader.oneDecimal_(-2.26) == "-2.3" &&
        YankeeTrader.oneDecimal_(null) == "-",
        "dashboard timing formatter handles rounding and missing values")
    check_(CalendarDate.isoFromTimestamp(0) == "1970-01-01" &&
        CalendarDate.isoFromTimestamp(951782400) == "2000-02-29" &&
        CalendarDate.iso(1, 2, 3) == "0001-02-03",
        "record date formatter produces ISO dates")
    check_(CalendarDate.daysInMonth(2024, 2) == 29 &&
        CalendarDate.daysInMonth(2023, 2) == 28 &&
        CalendarDate.daysInMonth(2026, 4) == 30 &&
        CalendarDate.daysInMonth(2026, 7) == 31 &&
        CalendarDate.weekday(2026, 7, 23) == 4,
        "record date picker follows calendar and leap-year month lengths")
    var calendar = DatePicker.new(2024, 2, 29)
    calendar.moveMonths(12)
    check_(calendar.value == "2025-02-28",
        "calendar clamps leap day when changing year")
    calendar.moveDays(1)
    check_(calendar.value == "2025-03-01",
        "calendar day navigation crosses month boundaries")
    calendar.bounds = Rect.new(1, 1, 28, 8)
    var calendarSurface = calendar.draw()
    check_(calendarSurface.cellAt(9, 0).ch == "M" &&
        calendarSurface.cellAt(1, 1).ch == "S" &&
        calendarSurface.cellAt(25, 1).ch == "S",
        "calendar renders named month and weekday grid")
    calendar.handle(MouseEvent.new(Mouse.button1Click, 26, 1))
    check_(calendar.value == "2025-04-01",
        "calendar next-month header control is clickable")
    YankeeTrader.startDetect_()
    check_(YankeeTrader.detectStatus_["active"],
        "settings detector starts as live automation")
    check_(YankeeTrader.detectHelpPattern_ == "<Help>(.+?)Main Command" &&
        !YankeeTrader.detectHelpPattern_.contains("Instructions"),
        "settings detector captures question-mark Help screen")
    YankeeTrader.onDetectInfo_(["Name  Test Main Command"])
    YankeeTrader.onDetectVersion_(["Version 3.6G Main Command"])
    YankeeTrader.onDetectHelp_([
        "<Help> [ENTER] - Re-display sector Main Command"
    ])
    check_(YankeeTrader.detectStatus_["phase"] == "computer",
        "captured Help screen advances settings detection")
    YankeeTrader.onDetectComputer_([
        "<Computer activated> Computer command"
    ])
    check_(YankeeTrader.detectStatus_["phase"] == "sector-ready",
        "Computer capture defers probe to general prompt hook")
    YankeeTrader.onComputerPrompt_(["Computer command"])
    while (YankeeTrader.detectStatus_["phase"] == "sector") {
      var probe = YankeeTrader.detectStatus_["probe"]
      if (probe <= 3000) {
        YankeeTrader.onComputerPrompt_(["Computer command"])
      } else {
        YankeeTrader.onDetectInvalidSector_([
            "Invalid sector number! Range is 1 - 3000 " +
            "Enter sector number port is in"
        ])
      }
    }
    if (YankeeTrader.detectStatus_["phase"] == "sector-exit") {
      YankeeTrader.onComputerPrompt_(["Computer command"])
    }
    detector = YankeeTrader.detectStatus_
    check_(detector["phase"] == "exit" && detector["low"] == 3000 &&
        detector["high"] == 3001 && detector["probes"] <= 17,
        "binary sector detection handles repeated invalid-sector prompt")
    YankeeTrader.cancelAutomation_()
    check_(!YankeeTrader.detectStatus_["active"],
        "shared automation cancellation stops settings detector")
    YankeeTrader.commandRunSends = false
    check_(YankeeTrader.monitoredBatchText_("c;2;2;1") ==
        "c;2;2;1;?\r" &&
        YankeeTrader.monitoredBatchText_("c;11;1;") == "c;11;1;?\r",
        "command batches end with one Main Help completion marker")
    YankeeTrader.startCommandRun_("Test Reports",
        ["c;2;2;1", "c;2;9;1"])
    var commandRun = YankeeTrader.commandRunStatus_
    check_(commandRun["active"] && commandRun["current"] == 1 &&
        commandRun["completed"] == 0 && commandRun["total"] == 2,
        "reviewed command runner starts first monitored batch")
    YankeeTrader.onHelp_(["<Help> Main Command"])
    commandRun = YankeeTrader.commandRunStatus_
    check_(commandRun["active"] && commandRun["current"] == 2 &&
        commandRun["completed"] == 1,
        "command runner advances only at its Help completion marker")
    YankeeTrader.onHelp_(["<Help> Main Command"])
    commandRun = YankeeTrader.commandRunStatus_
    check_(!commandRun["active"] && commandRun["completed"] == 2 &&
        YankeeTrader.pendingNotice_.contains("completed 2 monitored batches"),
        "command runner reports completed plan")
    YankeeTrader.startCommandRun_("Travel and Sensor Scan",
        ["c;3;8;y;s", "c;3;10;y;s"])
    YankeeTrader.onNoTurns_(["Sorry but you have no turns left."])
    commandRun = YankeeTrader.commandRunStatus_
    check_(!commandRun["active"] &&
        YankeeTrader.pendingNotice_.contains("Travel and Sensor Scan stopped") &&
        YankeeTrader.pendingNotice_.contains("no later batch will be sent"),
        "out-of-turn response stops later monitored command batches")
    YankeeTrader.startRoute_(10)
    check_(YankeeTrader.activeTurnRunName_ == "Route scan",
        "route query is tracked before its movement plan is available")
    YankeeTrader.onAutopilotTurnLimit_([
        "Not enough turns left to autopilot this course!"
    ])
    check_(YankeeTrader.activeTurnRunName_ == null &&
        YankeeTrader.pendingNotice_.contains("Route scan stopped"),
        "insufficient-turn C3 response declines autopilot and clears route state")
    YankeeTrader.startRoute_(10)
    YankeeTrader.onRoute_([
        "Course will take 2 turns:\nfrom sector 1\n1, 8, 10\n" +
        "Enter course into autopilot"
    ])
    commandRun = YankeeTrader.commandRunStatus_
    check_(commandRun["active"] && commandRun["name"] == "Route scan" &&
        commandRun["total"] == 3,
        "route scanner monitors setup and each traveled sector separately")
    YankeeTrader.onNoTurns_(["Sorry but you have no turns left."])
    YankeeTrader.cancelAutomation_()
    YankeeTrader.commandRunSends = true
    var classic = YTState.defaultProfile("3.2")
    check_(classic["maxSector"] == 1000 && classic["sensorRobots"] &&
        classic["maxPlanets"] == 75 && classic["maxPorts"] == 300 &&
        classic["missiles"] && classic["xannor"] && classic["mercenaries"] &&
        !classic["plasmaBolts"] && !classic["dangerScanner"] &&
        !classic["spies"], "YT 3.2 documented defaults")
    var stock = YTState.defaultProfile("modern")
    check_(stock["version"] == "3.6" && stock["maxSector"] == 3000 &&
        stock["maxPorts"] == 1000 && stock["sectorAvoidSlots"] == 30 &&
        stock["dangerScanner"] && stock["spies"] &&
        !stock["sensorRobots"], "stock YT 3.6 defaults")
    var localMod = YTState.defaultProfile("3.6G mod")
    check_(localMod["maxSector"] == 2004 && !localMod["missiles"] &&
        localMod["plasmaBolts"], "local YT 3.6G mod defaults")
    var gameA = YTState.defaultGame("Door A")
    gameA["sectors"]["44"] = {"sector": 44}
    var gameB = YTState.defaultGame("Door B", "3.2")
    gameB["profile"]["maxSector"] = 1200
    var multi = YTState.normalize_({
      "schema": 2, "activeGame": 1, "games": [gameA, gameB]
    })
    check_(multi["activeGame"] == 1 && multi["games"][0]["sectors"].count == 1 &&
        multi["games"][1]["sectors"].count == 0 &&
        multi["games"][1]["profile"]["maxSector"] == 1200,
        "independent multi-game records and settings")
    check_(YTState.replace({"schema": 1}) != null,
        "state import rejects unsupported schemas")
    check_(YTCalc.plasmaPercent(2) == 98, "plasma dissipation")
    check_(YTCalc.plasmaPercent(51) == 0, "plasma range floor")
    check_(YTCalc.plasmaDamage(9, 3000, 1) == 243000,
        "plasma productivity damage")
    check_(YTCalc.safePlasmaBolts(250000, 3000, 1) == 9,
        "safe plasma example")
    check_(YTCalc.playerPlasmaBolts(50000, 0) == 1,
        "player plasma fighter formula")
    check_(YTCalc.playerPlasmaBolts(0, 25000) == 1,
        "player plasma shield formula")
    check_(YTCalc.xannorSafeForces(500000000) == 10000,
        "Xannor safe forces")
    check_(YTCalc.xannorPlasmaBolts(150000) == 1 &&
        YTCalc.xannorPlasmaBolts(5100000) == 10,
        "Xannor plasma table")
    check_(YTCalc.mercenaryBribe(101) == 253, "mercenary bribe ceiling")
    var surrender = YTCalc.surrender(2000, 1000)
    check_(near_(surrender["surrendering"], 230), "surrender formula")
    check_(YTCalc.productivityCredits(0) == 8333500,
        "productivity investment ceiling")
    var missile = YTCalc.missileDamage(2)
    check_(missile["fightersMin"] == 4900 && missile["groundForces"] == 25,
        "missile table")
    var bank = YTCalc.planetBankEstimate(1000, 2)
    check_(bank["groundForcesPerDay"] == 2870 &&
        near_(bank["millionCredits"], 28.7), "planet bank estimate")
    var profit = YTCalc.portPairProfit(1000, 50, 2)
    check_(profit["credits"] == 100000 && profit["turns"] == 8,
        "port pair profit")
    var calculatorSpecs = [
      YankeeTrader.calculatorNumber_("holds", "Cargo holds:", 1000, 0, 9999),
      YankeeTrader.calculatorInteger_("trips", "Round trips:", 1, 0, 20),
      YankeeTrader.calculatorChoice_("target", "Target:", ["A", "B"], 0)
    ]
    var calculatorValues = YankeeTrader.calculatorValues_(calculatorSpecs, {
      "holds": " 2500.5 ", "trips": "4", "target": 1
    })
    check_(calculatorValues["error"] == null &&
        calculatorValues["values"]["holds"] == 2500.5 &&
        calculatorValues["values"]["trips"] == 4 &&
        calculatorValues["values"]["target"] == 1,
        "calculator form parses all visible fields")
    var badInteger = YankeeTrader.calculatorValues_(calculatorSpecs, {
      "holds": "1000", "trips": "2.5", "target": 0
    })
    check_(badInteger["values"] == null &&
        badInteger["error"].contains("integer"),
        "calculator form rejects non-integer integer fields")
    var badBounds = YankeeTrader.calculatorValues_(calculatorSpecs, {
      "holds": "10000", "trips": "2", "target": 3
    })
    check_(badBounds["values"] == null &&
        badBounds["error"].contains("Cargo holds"),
        "calculator form reports the first invalid visible field")
    var liveInput = SelectOnFocusInput.new()
    liveInput.value = "12"
    liveInput.selectAll()
    var refreshes = 0
    YankeeTrader.bindCalculatorInput_(liveInput, Fn.new {
      refreshes = refreshes + 1
      return null
    })
    liveInput.handle(KeyEvent.new(0x33))
    check_(liveInput.value == "3" && refreshes == 1,
        "calculator input refreshes immediately on edit")
    liveInput.handle(KeyEvent.new(Key.enter))
    check_(liveInput.allSelected && refreshes == 1,
        "calculator Enter prepares replacement without recalculating twice")
    var wrappedResult = YankeeTrader.calculatorResultLines_(
        "Alpha beta gamma", 10)
    check_(wrappedResult.count == 3 && wrappedResult[0] == "Alpha" &&
        wrappedResult[1] == "beta" && wrappedResult[2] == "gamma",
        "calculator results wrap at word boundaries")
    var verticalLayout = YankeeTrader.calculatorLayout_(
        Rect.new(2, 3, 74, 17), 6, false)
    check_(verticalLayout["form"].w == 74 &&
        verticalLayout["results"].w == 74 &&
        verticalLayout["results"].y >
            verticalLayout["form"].y + verticalLayout["form"].h,
        "ordinary calculators place full-width results below inputs")
    var plasmaLayout = YankeeTrader.calculatorLayout_(
        Rect.new(2, 3, 74, 17), 10, true)
    check_(plasmaLayout["form"].h == 17 &&
        plasmaLayout["results"].h == 17 &&
        plasmaLayout["results"].x >
            plasmaLayout["form"].x + plasmaLayout["form"].w,
        "plasma calculator keeps results beside its tall form")

    var numbers = YTText.numbers("Sector 34 leads to 37, 35")
    check_(numbers.count == 3 && numbers[0] == 34 && numbers[2] == 35,
        "number extraction")
    check_(YTText.numberOnLine("Turns.... : 1,000\nHolds: 20", "Turns") ==
        1000, "labelled number extraction with commas")
    var detectedMod = YTUniverse.detectGameSettings(
        "[ Info ]\n Turns.... : 487\n",
        "Version 3.6g * YT * Mod 02/09/2024\n" +
        "Prices & Xannor fix, Anticloak, Spies, Missiles disabled",
        "<Instructions>\n! - Launch a Cruise Missile\n" +
        "+ - Fire Plasma Bolt\nYou may repeat any command up to 20 times\n" +
        "any number between 2 and 20.",
        "<Computer activated>\n15) Show Active Spies\n", 2004)
    check_(detectedMod["version"] == "3.6G mod" &&
        detectedMod["currentTurns"] == 487 &&
        detectedMod["maxSector"] == 2004 &&
        detectedMod["macroRepeats"] == 20 &&
        !detectedMod["missiles"] && detectedMod["plasmaBolts"] &&
        !detectedMod["sensorRobots"] && detectedMod["spies"],
        "3.6G screen setting detection")
    var detectedClassic = YTUniverse.detectGameSettings(
        "Turns.... : 500", "Version 3.2", "R - Launch Sensor Robot\n" +
        "any number between 2 and 30", "Computer commands", 1000)
    check_(detectedClassic["version"] == "3.2" &&
        detectedClassic["macroRepeats"] == 30 &&
        detectedClassic["sensorRobots"] && !detectedClassic["spies"],
        "3.2 screen setting detection")
    var modCeiling = binaryCeiling_(2004)
    var largestCeiling = binaryCeiling_(99999)
    check_(modCeiling[0] == 2004 && modCeiling[1] <= 17 &&
        largestCeiling[0] == 99999 && largestCeiling[1] <= 17,
        "C2 sector ceiling binary search")
    var graph = YTUniverse.parseSensorLog(
        "Sector: 34\nPort: A\nWarps lead to: 37, 35\n" +
        "Sector: 35\nWarps lead to: 34\n")
    check_(graph.count == 2 && graph["34"]["port"] &&
        graph["35"]["warps"][0] == 34, "sensor-log parser")
    var dead = YTUniverse.deadEnds(graph)
    check_(dead.count == 1 && dead[0][0] == 35, "dead-end extraction")
    var ports = YTUniverse.extractPortSectors(
        "Sector: 9\nSector: 2\nSector: 9\n")
    check_(ports.count == 2 && ports[0] == 2 && ports[1] == 9,
        "port sector extraction")

    var scan = YTUniverse.analyze10Scan(
        "1,2,8\n1,2,9\n1,3,10\n", 8, 20)
    check_(scan["pathCount"] == 3 && scan["coverage"].count == 4,
        "C10 path and frequency analysis")
    check_(scan["deadEnds"].count == 3,
        "C10 dead-end probes")
    var livePath = YTUniverse.parsePathFinder(
        "Working. The shortest path from sector 44 to sector 81 is:\r\n" +
        "44, 52, 81\r\nCourse will take 2 turns.\r\n" +
        "Enter start for path search?", 44, 81, 3000)
    check_(livePath.count == 3 && livePath[0] == 44 && livePath[-1] == 81,
        "live C10 path capture")
    var path3000 = YTUniverse.parsePathFinder(
        "Working. The shortest path from sector 1 to sector 3000 is:\n" +
        "1, 4, 13, 12, 20, 22, 29, 37, 46, 53, 48, 55, 58, 2930, " +
        "2939, 2935, 2945, 2955, 2957, 2963, 2970, 2978, 2986, 2995, " +
        "2989, 2994, 2987, 2997, 3000\nCourse will take 28 turns.\n" +
        "Time: 150:09 Computer command (?=help)? 10;1;3001", 1, 3000,
        3001)
    check_(path3000.count == 29 && path3000[-3] == 2987 &&
        path3000[-2] == 2997 && path3000[-1] == 3000,
        "C10 route stops before turn, status, and next-command numbers")
    var contaminated = YTUniverse.parsePathFinder(
        "The shortest path from sector 1 to sector 3000 is:\n" +
        "1, 4, 13, 2999\nCourse will take 25 turns.\n" +
        "Time: 150:09 Computer command (?=help)? 10;1;3000", 1, 3000,
        3000)
    check_(contaminated.count == 0,
        "C10 parser rejects a route missing its target before summary")
    var liveAnalysis = YTUniverse.analyze10Paths({
      "52": [44, 52],
      "81": livePath
    }, 3000, 44)
    check_(liveAnalysis["pathCount"] == 2 &&
        liveAnalysis["paths"]["81"][1] == 52 &&
        liveAnalysis["directWarps"].count == 1 &&
        liveAnalysis["directWarps"][0] == 52,
        "saved rooted C10 map and direct warps")
    var sectorRoutes = YTUniverse.routesContainingSector(liveAnalysis, 52)
    var edgeRoutes = YTUniverse.routesContainingEdge(liveAnalysis, 44, 52)
    var routeLabels = YankeeTrader.mapRouteLabels_(edgeRoutes)
    check_(sectorRoutes.count == 2 && sectorRoutes[0][0] == 52 &&
        sectorRoutes[1][0] == 81 && edgeRoutes.count == 2 &&
        routeLabels[0] == "Target 52: 1 jump" &&
        routeLabels[1] == "Target 81: 2 jumps" &&
        YTUniverse.pathTo(liveAnalysis, 81)[-1] == 81 &&
        YTUniverse.pathTo(liveAnalysis, 999) == null,
        "saved map sector, edge, and target route lookup")
    liveAnalysis["complete"] = true
    liveAnalysis["queryCount"] = 2
    check_(YTState.setAnalysis(liveAnalysis) == null &&
        YTState.data["lastAnalysis"]["paths"]["52"][0] == 44,
        "C10 path map persists in state")
    var universeFile = YTState.universeFilename_(YTState.data["id"])
    var universeBeforeProfile = cacheText_(universeFile)
    var detectedProfile = YTState.defaultProfile("modern")
    detectedProfile["maxPorts"] = 777
    check_(YTState.applyDetectedProfile(detectedProfile,
        {"sectorProbeCount": 17}) == null &&
        Cache.contains(YTState.profileFilename_(YTState.data["id"])) &&
        cacheText_(universeFile) == universeBeforeProfile,
        "profile save does not rewrite current game's universe map")
    YTState.initialize_()
    check_(YTState.data["profile"]["maxPorts"] == 777 &&
        YTState.data["lastAnalysis"]["paths"]["52"][0] == 44,
        "split profile and universe files reload independently")
    check_(YTState.universeFilename_("g1") !=
        YTState.universeFilename_("g2") &&
        YTState.recordsFilename_("g1") != YTState.recordsFilename_("g2"),
        "each game has distinct universe and records files")
    check_(YTState.addGame("Split Door", "3.2") == null,
        "second game registry entry saves")
    var splitId = YTState.data["id"]
    check_(splitId != "g1" &&
        YTState.putSector(12, "note", "second game", "2026-07-22") == null &&
        YTState.addPlanet(12, "Split Planet", 1, 2, 3, "2026-07-22",
            "record") == null,
        "second game writes its own universe and records")
    check_(YTState.selectGame(0) == null, "return to first split game")
    YTState.initialize_()
    check_(YTState.games.count == 2 &&
        YTState.data["lastAnalysis"]["paths"]["52"][0] == 44,
        "game registry reload retains first game's universe")
    check_(YTState.selectGame(1) == null &&
        YTState.data["id"] == splitId &&
        YTState.data["sectors"]["12"]["note"] == "second game" &&
        YTState.data["planets"][0]["name"] == "Split Planet",
        "second game's per-game files reload independently")
    check_(YTState.selectGame(0) == null, "restore first game after split test")
    YankeeTrader.startC10_(44, 81, 82)
    YankeeTrader.onComputerPrompt_(["Computer command"])
    YankeeTrader.onPathResult_(["Working. The shortest path from sector 44 " +
        "to sector 81 is:\r\n44, 52, 81\r\n" +
        "Course will take 2 turns."])
    YankeeTrader.onComputerPrompt_(["Computer command"])
    YankeeTrader.onPathResult_(["Working. The shortest path from sector 44 " +
        "to sector 82 is:\r\n44, 52, 82\r\n" +
        "Course will take 2 turns."])
    YankeeTrader.onComputerPrompt_(["Computer command"])
    var finishedMapper = YankeeTrader.c10Status_
    check_(!finishedMapper["active"] && finishedMapper["captured"] == 2 &&
        YTState.data["lastAnalysis"]["complete"] &&
        YTState.data["lastAnalysis"]["paths"]["81"][-1] == 81 &&
        YTState.data["lastAnalysis"]["paths"]["82"][-1] == 82 &&
        YankeeTrader.pendingNotice_.contains("completed and saved"),
        "live mapper saves each response under its matching target")
    YankeeTrader.cancelAutomation_()

    YankeeTrader.startT3_()
    var t3 = YankeeTrader.benchmarkStatus_
    check_(t3["name"] == "t3" && t3["commands"] == 20 && t3["waiting"],
        "T3 waits for initial Computer prompt")
    YankeeTrader.onComputerPrompt_(["Computer command"])
    check_(!YankeeTrader.benchmarkStatus_["waiting"] &&
        YankeeTrader.benchmarkStatus_["commands"] == 19,
        "T3 starts first search at initial Computer prompt")
    for (i in 1..20) {
      YankeeTrader.onComputerPrompt_(["Computer command"])
    }
    t3 = YankeeTrader.benchmarkStatus_
    check_(t3["name"] == null && t3["commands"] == 0 &&
        YTState.data["benchmarks"]["t3"] != null &&
        YankeeTrader.pendingNotice_.contains("Benchmark t3 finished"),
        "T3 finishes on twentieth returned Computer prompt")

    check_(YTCommands.mainHelpCommand == "?\r",
        "settings detection uses Main Command help, not full instructions")
    check_(YTCommands.portScans([2, 9, 12], 2)[0] == "c;2;2;2;9;1",
        "port scan batching")
    check_(YTCommands.missiles([19, 47], 10)[0] == "!;19;1;!;47;1",
        "missile command")
    check_(YTCommands.robots([3, 68], 10)[0] == "R;3;R;68",
        "YT 3.2 robot command")
    check_(YTCommands.c10Command(8) == "10;1;8\r",
        "constant-time C10 command generation")
    check_(YTCommands.c10Command(44, 81) == "10;44;81\r",
        "arbitrary-viewpoint C10 command")
    check_(YTCommands.avoid([34, 2524]) == "c;7;1;34;7;2;2524",
        "avoid command")
    check_(YTCommands.avoid([3, 4, 5], 2) == "c;7;1;3;7;2;4",
        "profile-sized avoid command")
    check_(YTCommands.clearAvoid(30).contains("7;30;;1"),
        "clear all documented avoid slots")
    var breadcrumbs = YTCommands.fighterBreadcrumbs(1, [8, 10])
    check_(breadcrumbs.count == 6 &&
        breadcrumbs[0] == "m;8" && breadcrumbs[1] == "f;1;s" &&
        breadcrumbs[2] == "m;1" && breadcrumbs[3] == "m;10",
        "fighter placement does not queue a drop behind an unverified move")
    var surround = YTCommands.surroundMercenaryBatches(44, [43, 45], 100)
    check_(surround.count == 5 && surround[0].endsWith(";1") &&
        surround[1] == "c;3;43;y" && surround[2] == "f;100" &&
        surround[3] == "c;3;45;y" && surround[4] == "f;100",
        "mercenary surround verifies each move before dropping fighters")
    var macros = YTCommands.defaultMacros(2)
    check_(!macros[4][2].contains("/R") &&
        macros[4][2].contains(";L;3;;P"), "native macro repetition")
    check_(!macros[3][2].contains("\r"),
        "function-key macro gets one send terminator")
    check_(macros[4][1].contains("equipment") &&
        macros[5][2].startsWith("L;2;;P;;;0") &&
        macros[6][2].startsWith("L;1;;P;;;0") &&
        macros[8][1].contains("equipment") &&
        macros[10][1].contains("ore"),
        "website F5-F11 cargo bindings")
    check_(macros[7][2] == "L;T;C;P;;L;T;C;P;;" &&
        macros[8][2] ==
            "P;;;;;L;T;C;3;P;;;;;L;T;C;3;",
        "macro repeats preserve prompt semicolons")
    var route = YTUniverse.parseRouteBlock(
        "Course will take 4 turns:\nfrom sector 1\n1, 8, 10\n" +
        "Enter course into autopilot", 3000)
    check_(route.count == 3 && route[2] == 10, "C3 route parser")

    System.print("=== YANKEE TRADER: %(__passed) tests, 0 fail ===")
  }
}

YankeeTraderTest.run()
