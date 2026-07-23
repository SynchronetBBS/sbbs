// Yankee Trader control panel for SyncTERM.
// Install as a connected Wren script and press Alt-Y.

import "syncterm" for BBS, Clipboard, Conn, Hook, Input, Key, Screen, Timer,
                       WON, WONError
import "ui_app" for App
import "ui_button" for Button
import "ui_date_picker" for CalendarDate, DatePicker
import "ui_form" for Form
import "ui_input" for SelectOnFocusInput
import "ui_logview" for LogView
import "ui_pane" for Pane
import "ui_popup" for Alert, Confirm, Popup, Prompt
import "ui_picker" for ListPicker
import "ui_radio" for RadioGroup
import "ui_widget" for Rect
import "menu_ui" for MenuUi
import "yankee_trader_calc" for YTCalc
import "yankee_trader_state" for YTState
import "yankee_trader_tools" for YTCommands, YTText, YTUniverse

class YankeeTrader {
  static initialize_() {
    __routePending = false
    __fighterHome = null
    __c10Active = false
    __c10AwaitingResult = false
    __c10Source = 1
    __c10Start = 0
    __c10Stop = 0
    __c10Current = 0
    __c10Next = 0
    __c10Completed = 0
    __c10StartedAt = 0
    __c10LastSentAt = 0
    __c10LastSeconds = 0
    __c10TotalSeconds = 0
    __c10Paths = {}
    __c10CapturedTarget = 0
    __c10Missed = 0
    __liveNotice = null
    __immediateNotices = true
    __benchmark = null
    __benchmarkStarted = 0
    __benchmarkCommands = []
    __benchmarkRemaining = 0
    __t3WaitingToStart = false
    __commandRunActive = false
    __commandRunName = null
    __commandRunBatches = []
    __commandRunIndex = 0
    __commandRunStartedAt = 0
    __commandRunSends = true
    __detectActive = false
    __detectPhase = null
    __detectLastSentAt = 0
    __detectInfoText = ""
    __detectVersionText = ""
    __detectHelpText = ""
    __detectComputerText = ""
    __detectLow = 1
    __detectHigh = 100000
    __detectProbe = 0
    __detectProbeCount = 0
  }

  static run() {
    Fiber.new {
      var action = null
      Screen.modalRun(Fn.new { action = mainLoop_() })
      Input.setupMouseEvents()
      if (action != null) action.call()
    }.call()
  }

  static mainLoop_() {
    var initialGame = YTState.activeGameName
    if (__liveNotice != null) {
      alert_("Live Tool", __liveNotice)
      __liveNotice = null
    }
    if (YTState.lastError != null) alert_("State Recovery", YTState.lastError)
    var selected = 0
    while (true) {
      var picked = pickWithSelection_(
          "Yankee Trader: %(YTState.activeGameName)",
          help_, [
        "Replacement C;3: scan every traveled sector",
        "Drop one fighter in every adjacent sector",
        "Calculators",
        "Universe and log tools",
        "Dashboard",
        "Live tools",
        "F1-F12 key bindings",
        "Profile and data",
        "Strategy reference",
        "Switch game: %(YTState.activeGameName)",
        "Records and notes"
      ], selected)
      if (picked < 0) return null
      selected = picked
      if (picked == 0) {
        var target = integer_("Route Scan", "Target sector", 1, 1,
            YTState.data["profile"]["maxSector"])
        if (target != null) return Fn.new { startRoute_(target) }
      }
      if (picked == 1) {
        var home = integer_("Adjacent Fighters", "Current sector", 1, 1,
            YTState.data["profile"]["maxSector"])
        if (home != null) return Fn.new { startFighterScan_(home) }
      }
      if (picked == 2) calculators_()
      if (picked == 3) {
        var action = universe_()
        if (action != null) return action
      }
      if (picked == 4) dashboard_()
      if (picked == 5) {
        var action = liveMenu_()
        if (action != null) return action
      }
      if (picked == 6) macros_()
      if (picked == 7) {
        var action = profile_()
        if (action != null) return action
      }
      if (picked == 8) reference_()
      if (picked == 9) switchGame_()
      if (picked == 10) {
        var action = records_()
        if (action != null) return action
      }
    }
  }

  static dashboard_() {
    var p = YTState.data["profile"]
    var s = YTState.summary
    var b = YTState.data["benchmarks"]
    var benchmarks = benchmarkSummary_(b)
    var m = c10Status_
    var saved = YTState.data["lastAnalysis"]
    var savedMap = "none"
    if (saved != null) {
      var savedSource = "CSV import"
      if (saved["source"] != null) savedSource = "source %(saved["source"])"
      var savedState = saved["complete"] == false ? "partial" : "complete"
      savedMap = "%(saved["pathCount"]) paths, %(savedSource), %(savedState)"
    }
    var mapper = "idle"
    if (m["total"] > 0) {
      if (m["active"]) {
        mapper = "running %(m["completed"])/%(m["total"]) from " +
            "%(m["source"]), target %(m["current"]), captured " +
            "%(m["captured"])\nLast response: " +
            "%(oneDecimal_(m["lastSeconds"])) seconds  Average: " +
            "%(oneDecimal_(m["averageSeconds"])) seconds  ETA: " +
            "%(oneDecimal_(m["etaSeconds"])) seconds"
      } else {
        mapper = "last sweep %(m["completed"])/%(m["total"]) in " +
            "%(oneDecimal_(m["totalSeconds"])) seconds (" +
            "%(oneDecimal_(m["averageSeconds"])) seconds/path)"
      }
    }
    var commandStatus = commandRunStatus_
    var commandRunner = "idle"
    if (commandStatus["total"] > 0) {
      if (commandStatus["active"]) {
        commandRunner = "%(commandStatus["name"]): batch " +
            "%(commandStatus["current"])/%(commandStatus["total"])"
      } else {
        commandRunner = "last completed %(commandStatus["name"]): " +
            "%(commandStatus["completed"])/%(commandStatus["total"]) batches"
      }
    }
    alert_("Dashboard",
        "Game: %(YTState.activeGameName) of %(YTState.games.count)\n" +
        "Version: %(p["version"])\n" +
        "Universe: 1-%(p["maxSector"])\n\n" +
        "Mapped sectors: %(s["sectors"])\n" +
        "Planets: %(s["planets"])/%(p["maxPlanets"])\n" +
        "Port pairs: %(s["portPairs"])  Port limit: %(p["maxPorts"])\n" +
        "Tracked locations: %(s["locations"])\n" +
        "Saved C;10 map: %(savedMap)\n" +
        "C;10 mapper: %(mapper)\n" +
        "Command runner: %(commandRunner)\n\n" +
        "Missiles: %(enabled_(p["missiles"]))  " +
        "Plasma: %(enabled_(p["plasmaBolts"]))\n" +
        "Xannor: %(enabled_(p["xannor"]))  " +
        "Mercenaries: %(enabled_(p["mercenaries"]))\n" +
        "Robots: %(enabled_(p["sensorRobots"]))  " +
        "Danger scanner: %(enabled_(p["dangerScanner"]))\n" +
        "Spies: %(enabled_(p["spies"]))  " +
        "Avoid slots: %(p["sectorAvoidSlots"])  " +
        "Turns/day: %(p["turnsPerDay"])\n\n" +
        "Benchmarks (seconds): %(benchmarks)")
  }

  static liveMenu_() {
    var picked = pick_("Live Tools: %(YTState.activeGameName)", liveHelp_, [
      "Run no-turn C;10 universe sweep",
      "Benchmark T1: news and score",
      "Benchmark T2: trading-pair report",
      "Benchmark T3: 20 path searches",
      "Cancel pending automation"
    ])
    if (picked < 0) return null
    if (picked == 0) {
      var max = YTState.data["profile"]["maxSector"]
      var source = integer_("C;10 Sweep", "Viewpoint/source sector", 1, 1,
          max)
      if (source == null) return null
      var start = integer_("C;10 Sweep", "First target sector", 1, 1, max)
      if (start == null) return null
      var stop = integer_("C;10 Sweep", "Last target sector", max, start, max)
      if (stop == null) return null
      if (!confirm_("C;10 Sweep",
          "Run and save %(stop - start + 1) no-turn path searches from " +
          "sector %(source)? Progress is checkpointed every 100 paths.")) return null
      return Fn.new { startC10_(source, start, stop) }
    }
    if (picked == 1) return Fn.new { startT1_() }
    if (picked == 2) return Fn.new { startT2_() }
    if (picked == 3) return Fn.new { startT3_() }
    if (picked == 4) {
      cancelAutomation_()
      alert_("Live Tools", "Pending Yankee Trader automation was cancelled.")
    }
    return null
  }

  static startRoute_(target) {
    cancelAutomation_()
    __routePending = true
    send_("c;3;%(target)\r")
  }

  static startFighterScan_(home) {
    cancelAutomation_()
    __fighterHome = home
    send_("s\r")
  }

  static startC10_(source, start, stop) {
    cancelAutomation_()
    __c10Active = true
    __c10AwaitingResult = false
    __c10Source = source
    __c10Start = start
    __c10Stop = stop
    __c10Current = start
    __c10Next = start + 1
    __c10Completed = 0
    __c10StartedAt = 0
    __c10LastSentAt = 0
    __c10LastSeconds = 0
    __c10TotalSeconds = 0
    __c10Paths = {}
    __c10CapturedTarget = 0
    __c10Missed = 0
    var error = saveC10_(false)
    if (error != null) {
      __c10Active = false
      showLiveNotice_("C;10 sweep was not started because its map could " +
          "not be saved: %(error)")
      return
    }
    // Enter the Computer separately. Its first prompt starts the first query;
    // each later Computer prompt conclusively finishes the preceding query.
    __c10StartedAt = 0
    __c10LastSentAt = 0
    send_("c\r")
  }

  static startCommandRun_(name, batches) {
    cancelAutomation_()
    __commandRunActive = true
    __commandRunName = name
    __commandRunBatches = batches
    __commandRunIndex = 0
    __commandRunStartedAt = Timer.now
    sendCommandRunBatch_()
  }

  static sendCommandRunBatch_() {
    if (!__commandRunActive || __commandRunIndex >=
        __commandRunBatches.count) return null
    if (!__commandRunSends) return null
    var command = __commandRunBatches[__commandRunIndex]
    var error = send_(monitoredBatchText_(command))
    if (error != null) {
      var name = __commandRunName
      var batch = __commandRunIndex + 1
      __commandRunActive = false
      showLiveNotice_("%(name) stopped while sending batch %(batch): %(error)")
    }
    return null
  }

  static monitoredBatchText_(command) {
    var separator = command.endsWith(";") ? "" : ";"
    return command + separator + "?\r"
  }

  static onCommandRunHelp_(match) {
    if (!__commandRunActive) return null
    __commandRunIndex = __commandRunIndex + 1
    if (__commandRunIndex < __commandRunBatches.count) {
      sendCommandRunBatch_()
      return null
    }
    var name = __commandRunName
    var count = __commandRunBatches.count
    var elapsed = Timer.now - __commandRunStartedAt
    __commandRunActive = false
    showLiveNotice_("%(name) completed %(count) monitored batches in " +
        "%(oneDecimal_(elapsed)) seconds.")
    return null
  }

  static onHelp_(match) {
    onDetectHelp_(match)
    onCommandRunHelp_(match)
    return null
  }

  static startT1_() {
    cancelAutomation_()
    __benchmark = "t1"
    __benchmarkStarted = Timer.now
    __benchmarkRemaining = 3
    send_("z;y;ns;z;y;ns;z;y;ns\r")
  }

  static startT2_() {
    cancelAutomation_()
    __benchmark = "t2"
    __benchmarkStarted = Timer.now
    __benchmarkRemaining = 4
    send_("c;16;1\r")
  }

  static startT3_() {
    cancelAutomation_()
    var max = YTState.data["profile"]["maxSector"]
    for (i in 1..20) {
      var sector = ((i * 137) % max) + 1
      __benchmarkCommands.add("10;1;%(sector)\r")
    }
    __benchmark = "t3"
    __t3WaitingToStart = true
    send_("c\r")
  }

  static finishBenchmark_() {
    var name = __benchmark
    var elapsed = Timer.now - __benchmarkStarted
    YTState.setBenchmark(name, elapsed)
    var message = "Benchmark %(name) finished in %(elapsed) seconds."
    __benchmark = null
    __benchmarkCommands = []
    __benchmarkRemaining = 0
    __t3WaitingToStart = false
    showLiveNotice_(message)
  }

  static cancelAutomation_() {
    if (__c10Active) {
      var error = saveC10_(false)
      if (error != null) {
        __liveNotice = "The partial C;10 map could not be saved: %(error)"
      }
    }
    __routePending = false
    __fighterHome = null
    __c10Active = false
    __c10AwaitingResult = false
    __c10Source = 1
    __c10Start = 0
    __c10Stop = 0
    __c10Current = 0
    __c10Next = 0
    __c10Completed = 0
    __c10StartedAt = 0
    __c10LastSentAt = 0
    __c10LastSeconds = 0
    __c10TotalSeconds = 0
    __c10Paths = {}
    __c10CapturedTarget = 0
    __c10Missed = 0
    __benchmark = null
    __benchmarkCommands = []
    __benchmarkRemaining = 0
    __t3WaitingToStart = false
    __commandRunActive = false
    __commandRunName = null
    __commandRunBatches = []
    __commandRunIndex = 0
    __commandRunStartedAt = 0
    __detectActive = false
    __detectPhase = null
    __detectLastSentAt = 0
    __detectInfoText = ""
    __detectVersionText = ""
    __detectHelpText = ""
    __detectComputerText = ""
    __detectLow = 1
    __detectHigh = 100000
    __detectProbe = 0
    __detectProbeCount = 0
  }

  static calculators_() {
    var selected = 0
    while (true) {
      var p = YTState.data["profile"]
      var rows = []
      if (p["plasmaBolts"]) {
        rows.add([0, "Plasma damage and safe shot"])
        rows.add([1, "Plasma bolts needed"])
        rows.add([5, "Productivity investment"])
      }
      if (p["missiles"]) rows.add([2, "Missile damage and missiles needed"])
      rows.add([3, "Planet daily production"])
      rows.add([4, "Planet bank estimate"])
      if (p["xannor"]) rows.add([6, "Xannor planning"])
      if (p["mercenaries"]) {
        rows.add([7, "Mercenary bribe and fighter surrender"])
      }
      rows.add([8, "Port-pair earnings"])
      var picked = pickWithSelection_(
          "Calculators: %(YTState.activeGameName)", calculatorHelp_,
          rows.map {|row| row[1] }.toList, selected)
      if (picked < 0) return
      selected = picked
      var action = rows[picked][0]
      if (action == 0) plasmaDamage_()
      if (action == 1) plasmaNeeded_()
      if (action == 2) missiles_()
      if (action == 3) planetProduction_()
      if (action == 4) bankEstimate_()
      if (action == 5) productivity_()
      if (action == 6) xannor_()
      if (action == 7) mercenary_()
      if (action == 8) portProfit_()
    }
  }

  static plasmaDamage_() {
    var categoryNames = [
      "Fighters", "Shields", "Mines", "Ground forces",
      "Planet productivity: one field",
      "Planet productivity: two fields",
      "Planet productivity: three fields"
    ]
    var multipliers = [50000, 25000, 250, 1000, 1000, 2000, 3000]
    calculatorForm_("Plasma Damage and Safe Shot", [
      calculatorNumber_("bolts", "Plasma bolts:", 1, 0, 1000000),
      calculatorNumber_("distance", "Distance in sectors:", 1, 1, 100),
      calculatorNumber_("remaining", "Safe-shot target amount:", 0, 0,
          999999999999999),
      calculatorChoice_("category", "Safe-shot target:", categoryNames, 0)
    ], Fn.new {|v|
      var d = YTCalc.plasmaDamageTable(v["bolts"], v["distance"])
      var safe = v["remaining"] == 0 ? 0 :
          YTCalc.safePlasmaBolts(v["remaining"],
              multipliers[v["category"]], v["distance"])
      return "Damage retained: %(YTCalc.plasmaPercent(v["distance"]))\%\n" +
          "Fighters: %(d["fighters"])\nShields: %(d["shields"])\n" +
          "Mines: %(d["mines"])\nGround forces: %(d["groundForces"])\n" +
          "Productivity (1/2/3 fields): %(d["productivity1"])/" +
          "%(d["productivity2"])/%(d["productivity3"])\n\n" +
          "Safe-shot target: %(categoryNames[v["category"]])\n" +
          "Maximum non-destructive shot: %(safe) bolts"
    }, true)
  }

  static plasmaNeeded_() {
    calculatorForm_("Plasma Bolts Needed", [
      calculatorNumber_("fighters", "Fighters:", 0, 0, 999999999999999),
      calculatorNumber_("shields", "Shields:", 0, 0, 999999999999999),
      calculatorNumber_("mines", "Mines:", 0, 0, 999999999999999),
      calculatorNumber_("forces", "Ground forces:", 0, 0, 999999999999999),
      calculatorNumber_("productivity", "Productivity maximum:", 0, 0,
          999999999999999)
    ], Fn.new {|v|
      var needed = YTCalc.plasmaBoltsNeeded(v["fighters"], v["shields"],
          v["mines"], v["forces"], v["productivity"])
      var player = YTCalc.playerPlasmaBolts(v["fighters"], v["shields"])
      return "Spreadsheet staged estimate: %(needed) bolts\n" +
          "Player fighters + shields only: " +
          "%(player) bolts"
    })
  }

  static missiles_() {
    calculatorForm_("Missile Damage and Missiles Needed", [
      calculatorNumber_("missiles", "Missiles:", 1, 0, 999999999),
      calculatorNumber_("forces", "Ground forces to remove:", 0, 0,
          999999999999999),
      calculatorNumber_("productivity", "Max productivity to remove:", 0, 0,
          999999999999999)
    ], Fn.new {|v|
      var d = YTCalc.missileDamage(v["missiles"])
      var needed = YTCalc.missilesNeeded(0, 0, 0, 0, v["forces"],
          v["productivity"])
      return "Fighter damage: %(d["fightersMin"])-%(d["fightersMax"])\n" +
          "Shields: %(d["shields"])  Mines: %(d["mines"])\n" +
          "Ground forces: %(d["groundForces"])\n" +
          "Productivity: %(d["productivityMin"])-" +
          "%(d["productivityMax"])\n\n" +
          "For the entered GF/productivity layers: " +
          "%(needed) missiles"
    })
  }

  static planetProduction_() {
    calculatorForm_("Daily Planet Production", [
      calculatorNumber_("ore", "Ore amount:", 0, 0, 999999999999999),
      calculatorNumber_("organics", "Organics amount:", 0, 0,
          999999999999999),
      calculatorNumber_("equipment", "Equipment amount:", 0, 0,
          999999999999999),
      calculatorNumber_("bank", "Banked credits:", 0, 0, 999999999999999),
      calculatorNumber_("spent", "Spent on productivity:", 0, 0,
          999999999999999),
      calculatorNumber_("forces", "Current ground forces:", 1, 1,
          999999999999999)
    ], Fn.new {|v|
      var result = YTCalc.planetProduction(v["ore"], v["organics"],
          v["equipment"], v["bank"], v["spent"], v["forces"])
      var profile = YTState.data["profile"]
      var weapons = "Mines: %(result["mines"])"
      if (profile["missiles"]) {
        weapons = "Missiles: %(result["missiles"])  " + weapons
      }
      if (profile["plasmaBolts"]) {
        weapons = weapons + "\nPlasma bolts: %(result["plasmaBolts"])"
      }
      return "Ore: %(result["ore"])  Organics: %(result["organics"])\n" +
          "Equipment: %(result["equipment"])  " +
          "Fighters: %(result["fighters"])\n" + weapons + "\n" +
          "Credits: %(result["credits"])  " +
          "Ground forces: %(result["groundForces"])\n" +
          "Credits represented: %(result["totalCredits"])"
    })
  }

  static bankEstimate_() {
    calculatorForm_("Planet Bank Estimate", [
      calculatorNumber_("forces", "Current ground forces:", 0, 0,
          999999999999999),
      calculatorNumber_("rate", "GF increase in 60 seconds:", 0, 0,
          999999999999999)
    ], Fn.new {|v|
      var e = YTCalc.planetBankEstimate(v["forces"], v["rate"])
      return "Ground forces produced per day: " +
          "%(e["groundForcesPerDay"])\n" +
          "Estimated bank: %(e["millionCredits"]) million credits"
    })
  }

  static productivity_() {
    calculatorForm_("Productivity Investment", [
      calculatorInteger_("fighters", "Current fighters per day:", 0, 0, 99999)
    ], Fn.new {|v|
      return "Credits needed to reach one plasma bolt per day: " +
          "%(YTCalc.productivityCredits(v["fighters"]))"
    })
  }

  static xannor_() {
    calculatorForm_("Xannor Planning", [
      calculatorNumber_("score", "Xannor scoreboard score:", 0, 0,
          999999999999999),
      calculatorNumber_("fighters", "Fighters in target group:", 0, 0,
          999999999999999),
      calculatorNumber_("top", "Top-player score:", 0, 0, 999999999999999)
    ], Fn.new {|v|
      return "Safe ground forces per planet: " +
          "%(YTCalc.xannorSafeForces(v["score"]))\n" +
          "Bolts leaving about 100,000 fighters: " +
          "%(YTCalc.xannorPlasmaBolts(v["fighters"]))\n" +
          "Expected Xannoron retake attack: " +
          "%(YTCalc.xannoronHoldAttack(v["top"], v["fighters"])) fighters"
    })
  }

  static mercenary_() {
    calculatorForm_("Mercenaries and Fighter Surrender", [
      calculatorNumber_("mercs", "Mercenary fighters:", 0, 0,
          999999999999999),
      calculatorNumber_("attacking", "Attacking fighters:", 0, 0,
          999999999999999),
      calculatorNumber_("defending", "Defending fighters:", 0, 0,
          999999999999999)
    ], Fn.new {|v|
      var s = YTCalc.surrender(v["attacking"], v["defending"])
      return "Mercenary bribe: " +
          "%(YTCalc.mercenaryBribe(v["mercs"])) credits\n" +
          "Attack ratio: %(s["ratio"])\n" +
          "Surrendering: %(s["surrendering"])\n" +
          "Still defending: %(s["remaining"])"
    })
  }

  static portProfit_() {
    calculatorForm_("Port-pair Earnings", [
      calculatorNumber_("holds", "Cargo holds:", 1000, 0, 999999999),
      calculatorNumber_("profit", "Profit per hold:", 0, 0, 999999999),
      calculatorNumber_("trips", "Round trips:", 1, 0, 999999999)
    ], Fn.new {|v|
      var p = YTCalc.portPairProfit(v["holds"], v["profit"], v["trips"])
      return "Credits: %(p["credits"])\nTurns: %(p["turns"])\n" +
          "A complete buy/sell round trip normally costs four turns."
    })
  }

  static calculatorNumber_(key, label, initial, minimum, maximum) {
    return {
      "kind": "number", "key": key, "label": label,
      "initial": initial, "minimum": minimum, "maximum": maximum,
      "integer": false
    }
  }

  static calculatorInteger_(key, label, initial, minimum, maximum) {
    return {
      "kind": "number", "key": key, "label": label,
      "initial": initial, "minimum": minimum, "maximum": maximum,
      "integer": true
    }
  }

  static calculatorChoice_(key, label, choices, selected) {
    return {
      "kind": "choice", "key": key, "label": label,
      "choices": choices, "selected": selected
    }
  }

  static calculatorValues_(specs, raw) {
    var values = {}
    for (spec in specs) {
      var key = spec["key"]
      var label = spec["label"]
      if (spec["kind"] == "choice") {
        var selected = raw[key]
        if (!(selected is Num) || !selected.isInteger || selected < 0 ||
            selected >= spec["choices"].count) {
          return {
            "values": null,
            "error": "%(label) requires one of the listed choices."
          }
        }
        values[key] = selected
      } else {
        var text = raw[key]
        var number = text is String ? Num.fromString(text.trim()) : null
        var invalidInteger = spec["integer"] &&
            (number == null || !number.isInteger)
        if (number == null || invalidInteger ||
            number < spec["minimum"] || number > spec["maximum"]) {
          var kind = spec["integer"] ? "an integer" : "a number"
          return {
            "values": null,
            "error": "%(label) must be %(kind) from %(spec["minimum"]) " +
                "through %(spec["maximum"])."
          }
        }
        values[key] = number
      }
    }
    return {"values": values, "error": null}
  }

  static calculatorForm_(title, specs, calculate) {
    return calculatorForm_(title, specs, calculate, false)
  }

  static calculatorForm_(title, specs, calculate, sideResults) {
    return Screen.modalRun(Fn.new {
      var app = App.new()
      var pane = Pane.new()
      pane.title = title
      pane.focused = true
      pane.shadow = true
      var resultPosition = sideResults ? "to the right" : "below the fields"
      pane.helpText = "# %(title)\n\nAll inputs stay visible so values can " +
          "be compared and corrected. Word-wrapped results update in the " +
          "panel %(resultPosition) after every edit; there is no " +
          "calculation step or result popup to dismiss.\n\n" +
          "Tab / Down      next field\n" +
          "BackTab / Up    previous field\n" +
          "Typing / paste  immediately refreshes results\n" +
          "Enter           select the current number for replacement\n" +
          "Space / Enter   select a target choice\n" +
          "Esc             close the calculator"
      pane.onClose = Fn.new { app.quit() }

      var fieldRows = 0
      for (spec in specs) {
        fieldRows = fieldRows +
            (spec["kind"] == "choice" ? spec["choices"].count : 1)
      }
      var requestedHeight = sideResults ? (fieldRows + 6).max(20) : 21
      pane.bounds = Pane.modalBounds(78, requestedHeight)
      app.root.add(pane)

      var form = Form.new()
      var controls = {}
      for (spec in specs) {
        var control
        if (spec["kind"] == "choice") {
          control = RadioGroup.new()
          control.items = spec["choices"]
          control.selected = spec["selected"]
          form.addFieldH(spec["label"], control, spec["choices"].count)
        } else {
          control = SelectOnFocusInput.new()
          control.value = spec["initial"].toString
          control.maxLen = 24
          form.addField(spec["label"], control)
        }
        controls[spec["key"]] = control
      }

      var ib = pane.innerBounds
      var layout = calculatorLayout_(ib, fieldRows, sideResults)
      form.bounds = layout["form"]

      var resultPane = Pane.new()
      resultPane.title = "Results"
      resultPane.titleAsBar = false
      resultPane.frameKind = "display"
      resultPane.closeable = false
      resultPane.helpable = false
      resultPane.bounds = layout["results"]
      var resultView = LogView.new()
      resultView.capacity = 64
      resultView.bounds = resultPane.innerBounds
      resultPane.add(resultView)

      var renderResult = Fn.new {|level, message|
        resultView.clear()
        for (line in calculatorResultLines_(message, resultView.bounds.w)) {
          resultView.append(level, line)
        }
        resultView.goLive()
        return null
      }
      var refresh = Fn.new {
        var raw = {}
        for (spec in specs) {
          var control = controls[spec["key"]]
          raw[spec["key"]] = spec["kind"] == "choice" ?
              control.selected : control.value
        }
        var parsed = calculatorValues_(specs, raw)
        if (parsed["error"] != null) {
          renderResult.call(LogView.LEVEL_ERR, parsed["error"])
          return null
        }
        renderResult.call(LogView.LEVEL_INFO,
            calculate.call(parsed["values"]))
        return null
      }
      for (spec in specs) {
        if (spec["kind"] == "number") {
          bindCalculatorInput_(controls[spec["key"]], refresh)
        } else {
          controls[spec["key"]].onChange =
              Fn.new {|ignoredIndex, ignoredItem| refresh.call() }
        }
      }
      form.onCancel = Fn.new { app.quit() }
      // With no submit action Form creates only its cancel button.
      form.children[specs.count].label = "Close"
      pane.add(form)
      pane.add(resultPane)
      refresh.call()
      app.runSync()
      return null
    })
  }

  static calculatorLayout_(bounds, fieldRows, sideResults) {
    if (sideResults) {
      var gap = bounds.w > 30 ? 1 : 0
      var formWidth = 43.min((bounds.w - 18).max((bounds.w / 2).floor))
      return {
        "form": Rect.new(bounds.x, bounds.y, formWidth, bounds.h),
        "results": Rect.new(bounds.x + formWidth + gap, bounds.y,
            bounds.w - formWidth - gap, bounds.h)
      }
    }

    // Form's Close button occupies one blank separator row plus one button
    // row after the fields. Reserve the remaining height for full-width
    // results, with a blank row between the two areas when space permits.
    var formHeight = (fieldRows + 2).min((bounds.h - 3).max(1))
    var gap = bounds.h - formHeight > 3 ? 1 : 0
    return {
      "form": Rect.new(bounds.x, bounds.y, bounds.w, formHeight),
      "results": Rect.new(bounds.x, bounds.y + formHeight + gap, bounds.w,
          (bounds.h - formHeight - gap).max(1))
    }
  }

  static calculatorResultLines_(message, viewWidth) {
    // LogView reserves two columns when its scrollbar is needed. Wrapping to
    // that stable width prevents a scrollbar from forcing a second hard wrap.
    return Popup.wrap_(message, (viewWidth - 2).max(1))
  }

  static bindCalculatorInput_(control, refresh) {
    control.onChange = Fn.new {|ignored|
      refresh.call()
      return null
    }
    control.onSubmit = Fn.new {|ignored|
      // Results are already current. Enter just prepares this value for the
      // next replacement, matching spreadsheet-style iterative calculation.
      control.selectAll()
      return null
    }
  }

  static universe_() {
    var selected = 0
    while (true) {
      var rows = [
        [7, "View saved C;10 map"],
        [2, "Scan saved ports"],
        [3, "Act on saved C;10 analysis"],
        [4, "Scan a sector range"],
        [5, "Manage navigation avoids"],
        [8, "Legacy and recovery imports"]
      ]
      if (YTState.data["profile"]["mercenaries"]) {
        rows.add([6, "Surround mercenaries with fighters"])
      }
      if (commandRunStatus_["active"]) {
        rows.add([9, "Cancel running command plan"])
      }
      var picked = pickWithSelection_(
          "Universe: %(YTState.activeGameName)", universeHelp_,
          rows.map {|row| row[1] }.toList, selected)
      if (picked < 0) return
      selected = picked
      var action = rows[picked][0]
      if (action == 7) c10Map_()
      if (action == 8) legacyImports_()
      if (action == 9) {
        cancelAutomation_()
        alert_("Universe Tools", "Pending command batches were cancelled.")
      }
      if (action == 2) {
        var run = portScans_()
        if (run != null) return run
      }
      if (action == 3) {
        var run = analysisActions_()
        if (run != null) return run
      }
      if (action == 4) {
        var run = rangeActions_()
        if (run != null) return run
      }
      if (action == 5) {
        var run = avoid_()
        if (run != null) return run
      }
      if (action == 6) {
        var run = surround_()
        if (run != null) return run
      }
    }
  }

  static legacyImports_() {
    var selected = 0
    while (true) {
      var picked = pickWithSelection_(
          "Legacy Imports: %(YTState.activeGameName)",
          legacyImportHelp_, [
        "Import old 10-scan C;10 CSV from clipboard",
        "Import sensor/session log from clipboard"
      ], selected)
      if (picked < 0) return
      selected = picked
      if (picked == 0) analyze10_()
      if (picked == 1) analyzeLog_()
    }
  }

  static c10Map_() {
    var result = YTState.data["lastAnalysis"]
    if (result == null) {
      alert_("Saved C;10 Map", "No live sweep or CSV scan has been saved.")
      return
    }
    var direct = analysisList_(result, "directWarps")
    var coverage = analysisList_(result, "coverage")
    var deadEnds = sortedMapEdges_(analysisList_(result, "deadEnds"))
    var linked = sortedMapEdges_(analysisList_(result, "linked"))
    var rifts = sortedMapEdges_(analysisList_(result, "rifts"))
    var selected = 0
    while (true) {
      var picked = pickWithSelection_(
          "Saved C;10 Map: %(YTState.activeGameName)",
          mapHelp_, [
        "Map summary",
        "Direct outgoing warps (%(direct.count))",
        "Coverage/scouting sectors (%(coverage.count))",
        "Dead-end probe edges (%(deadEnds.count))",
        "Long-number-gap link probes (%(linked.count))",
        "Possible rift entrance probes (%(rifts.count))",
        "Find captured path by target sector"
      ], selected)
      if (picked < 0) return
      selected = picked
      if (picked == 0) c10Summary_(result, direct, coverage, deadEnds,
          linked, rifts)
      if (picked == 1) browseMapSectors_("Direct Outgoing Warps", result,
          direct)
      if (picked == 2) browseMapSectors_("Coverage/Scouting Sectors", result,
          coverage)
      if (picked == 3) browseMapEdges_("Dead-end Probe Edges", result, deadEnds)
      if (picked == 4) browseMapEdges_("Long-number-gap Link Probes", result,
          linked)
      if (picked == 5) browseMapEdges_("Possible Rift Entrance Probes", result,
          rifts)
      if (picked == 6) findMapPath_(result)
    }
  }

  static analysisList_(result, key) {
    var value = result[key]
    return value is List ? value : []
  }

  static sortedMapEdges_(edges) {
    var sorted = edges.map {|edge| edge }.toList
    sorted.sort {|a, b| a[0] == b[0] ? a[1] < b[1] : a[0] < b[0] }
    return sorted
  }

  static c10Summary_(result, direct, coverage, deadEnds, linked, rifts) {
    var source = "CSV import"
    if (result["source"] != null) source = result["source"].toString
    var state = result["complete"] == false ? "partial" : "complete"
    var queries = result["pathCount"]
    if (result["queryCount"] != null) queries = result["queryCount"]
    var targetRange = "not recorded"
    if (result["targetStart"] != null && result["targetStop"] != null) {
      targetRange = "%(result["targetStart"])-%(result["targetStop"])"
    }
    alert_("Saved C;10 Map",
        "State: %(state)\nSource: %(source)\nTargets: %(targetRange)\n" +
        "Queries completed: %(queries)\nPaths captured: " +
        "%(result["pathCount"])\nDirect outgoing warps: " +
        "%(direct.count == 0 ? "none recorded" : YTText.join(direct, ", "))\n" +
        "Coverage/scouting sectors: %(coverage.count)\n" +
        "Dead-end probe edges: %(deadEnds.count)\n" +
        "Long-number-gap link probes: %(linked.count)\n" +
        "Possible rift entrance probes: %(rifts.count)")
  }

  static browseMapSectors_(title, result, sectors) {
    if (sectors.count == 0) {
      alert_(title, "No matching sectors were found in this analysis.")
      return
    }
    var labels = sectors.map {|sector| "Sector %(sector)" }.toList
    var selected = 0
    while (true) {
      var picked = pickWithSelection_(title, mapHelp_, labels, selected)
      if (picked < 0) return
      selected = picked
      var sector = sectors[picked]
      var routes = YTUniverse.routesContainingSector(result, sector)
      browseMapRoutes_("%(title): sector %(sector)", routes,
          "Selected sector: %(sector)")
    }
  }

  static browseMapEdges_(title, result, edges) {
    if (edges.count == 0) {
      alert_(title, "No matching edges were found in this analysis.")
      return
    }
    var labels = []
    for (edge in edges) {
      var label = "%(edge[0]) -> %(edge[1])"
      if (edge.count >= 3) label = label + " (%(edge[2]) paths)"
      labels.add(label)
    }
    var selected = 0
    while (true) {
      var picked = pickWithSelection_(title, mapHelp_, labels, selected)
      if (picked < 0) return
      selected = picked
      var edge = edges[picked]
      var detail = "Selected edge: %(edge[0]) -> %(edge[1])"
      if (edge.count >= 3) {
        detail = detail + "\nDestination appears in %(edge[2]) captured paths."
      }
      var routes = YTUniverse.routesContainingEdge(result, edge[0], edge[1])
      browseMapRoutes_("%(title): %(edge[0]) -> %(edge[1])", routes, detail)
    }
  }

  static browseMapRoutes_(title, routes, detail) {
    if (routes.count == 0) {
      alert_(title, "%(detail)\n\nNo containing captured route was found.")
      return
    }
    if (routes.count == 1) {
      showMapPath_(title, routes[0][0], routes[0][1], detail)
      return
    }
    var labels = mapRouteLabels_(routes)
    var selected = 0
    while (true) {
      var picked = pickWithSelection_(title,
          "Select a captured target route containing this map feature.",
          labels, selected)
      if (picked < 0) return
      selected = picked
      showMapPath_(title, routes[picked][0], routes[picked][1], detail)
    }
  }

  static mapRouteLabels_(routes) {
    return routes.map {|route|
      var jumps = route[1].count - 1
      return "Target %(route[0]): %(jumps) %(jumps == 1 ? "jump" : "jumps")"
    }.toList
  }

  static findMapPath_(result) {
    var target = integer_("Find Captured Path", "Target sector", 1, 1,
        YTState.data["profile"]["maxSector"])
    if (target == null) return
    var path = YTUniverse.pathTo(result, target)
    if (path == null) {
      alert_("Find Captured Path",
          "No captured path is stored for target sector %(target).")
      return
    }
    showMapPath_("Captured Path", target, path, null)
  }

  static showMapPath_(title, target, path, detail) {
    var message = ""
    if (detail != null) message = detail + "\n\n"
    message = message + "Stored target: %(target)\n" +
        "Route endpoint: %(path[-1])\nJumps: %(path.count - 1)\n\n" +
        YTText.join(path, " -> ")
    alert_(title, message)
  }

  static analyze10_() {
    var text = Clipboard.text
    if (text == null || text.count == 0) {
      alert_("C;10 Analysis", "The clipboard is empty.")
      return
    }
    var start = integer_("C;10 Analysis", "First CSV row is target sector", 8,
        1, YTState.data["profile"]["maxSector"])
    if (start == null) return
    var result = YTUniverse.analyze10Scan(text, start,
        YTState.data["profile"]["maxSector"])
    var error = YTState.setAnalysis(result)
    if (error != null) {
      alert_("C;10 Analysis", "The CSV scan could not be saved: %(error)")
      return
    }
    alert_("C;10 Analysis",
        "Paths: %(result["pathCount"])\n" +
        "Unique coverage sectors: %(result["coverage"].count)\n" +
        "Dead-end probes: %(result["deadEnds"].count)\n" +
        "Long links: %(result["linked"].count)\n" +
        "Possible 20-120-sector rifts: %(result["rifts"].count)")
  }

  static analyzeLog_() {
    var text = Clipboard.text
    if (text == null || text.count == 0) {
      alert_("Session Log", "The clipboard is empty.")
      return
    }
    var graph = YTUniverse.parseSensorLog(text)
    var dead = YTUniverse.deadEnds(graph)
    var error = YTState.mergeGraph(graph)
    if (error != null) {
      alert_("Session Log", "The imported map could not be saved: %(error)")
      return
    }
    alert_("Session Log",
        "Mapped %(graph.count) sector blocks and found %(dead.count) direct " +
        "dead ends. The map was merged into this game's saved universe.")
  }

  static portScans_() {
    var sectors = []
    for (key in YTState.data["sectors"].keys) {
      var row = YTState.data["sectors"][key]
      if (row["port"] == true) sectors.add(row["sector"])
    }
    sectors = YTText.uniqueSorted(sectors)
    if (sectors.count == 0) {
      alert_("Port Scans", "No ports are present in the saved sensor map. " +
          "Run sensor scans or import a session capture first.")
      return
    }
    var per = integer_("Port Scans", "Sectors per command batch", 100, 1, 500)
    if (per == null) return
    var commands = YTCommands.portScans(sectors, per)
    return reviewCommandPlan_("Saved Port Reports",
        "Request a C;2 port report for every port in the saved sensor map.",
        "Read-only computer reports are expected to use no turns.", commands,
        sectors.count)
  }

  static analysisActions_() {
    var result = YTState.data["lastAnalysis"]
    if (result == null) {
      alert_("Analysis Actions", "Run a live C;10 sweep or import a CSV " +
          "scan first.")
      return
    }
    var source = pick_("Analysis Targets", mapHelp_, [
      "Coverage/scouting sectors", "Dead-end probe destinations",
      "Long-number-gap destinations", "Possible rift destinations"
    ])
    if (source < 0) return
    var sectors
    if (source == 0) sectors = result["coverage"]
    if (source == 1) sectors = result["deadEnds"].map {|r| r[1] }.toList
    if (source == 2) sectors = result["linked"].map {|r| r[1] }.toList
    if (source == 3) sectors = result["rifts"].map {|r| r[1] }.toList
    sectors = YTText.uniqueSorted(sectors)
    if (sectors.count == 0) {
      alert_("Analysis Actions", "The selected analysis category is empty.")
      return
    }
    return sectorActions_(sectors)
  }

  static rangeActions_() {
    var max = YTState.data["profile"]["maxSector"]
    var start = integer_("Sector Range", "First sector", 1, 1, max)
    if (start == null) return
    var stop = integer_("Sector Range", "Last sector", max, start, max)
    if (stop == null) return
    var sectors = []
    for (n in start..stop) sectors.add(n)
    return sectorActions_(sectors)
  }

  static sectorActions_(sectors) {
    var p = YTState.data["profile"]
    var rows = []
    if (p["missiles"]) rows.add([0, "Launch one missile at each sector"])
    rows.add([1, "Request planet reports"])
    if (p["sensorRobots"]) rows.add([2, "Send sensor robots"])
    rows.add([3, "Travel to and sensor-scan each sector"])
    rows.add([4, "Request port reports"])
    var picked = pick_("Action for %(sectors.count) Sectors",
        commandActionHelp_,
        rows.map {|row| row[1] }.toList)
    if (picked < 0) return
    var kind = rows[picked][0]
    var per = integer_("Command Batches", "Sectors per batch", 100, 1, 500)
    if (per == null) return
    var commands
    if (kind == 0) commands = YTCommands.missiles(sectors, per)
    if (kind == 1) commands = YTCommands.planetScans(sectors, per)
    if (kind == 2) commands = YTCommands.robots(sectors, per)
    if (kind == 3) commands = YTCommands.travelAndScan(sectors, per)
    if (kind == 4) commands = YTCommands.portScans(sectors, per)
    var title
    var description
    var warning
    if (kind == 0) {
      title = "Missile Scouting"
      description = "Launch one live cruise missile at each selected sector."
      warning = "Destructive: missiles can hit players, fighters, or planets " +
          "and may provoke retaliation. Resources are consumed."
    }
    if (kind == 1) {
      title = "Planet Reports"
      description = "Request a C;9 planet report for each selected sector."
      warning = "Read-only computer reports are expected to use no turns."
    }
    if (kind == 2) {
      title = "Sensor Robots"
      description = "Send a sensor robot command to each selected sector."
      warning = "May consume robots or turns according to this game's rules."
    }
    if (kind == 3) {
      title = "Travel and Sensor Scan"
      description = "Autopilot to every selected sector and run sensors."
      warning = "Consumes travel turns and can encounter mines, fighters, " +
          "black holes, or other hazards."
    }
    if (kind == 4) {
      title = "Port Reports"
      description = "Request a C;2 port report for each selected sector."
      warning = "Read-only computer reports are expected to use no turns."
    }
    return reviewCommandPlan_(title, description, warning, commands,
        sectors.count)
  }

  static avoid_() {
    var slots = YTState.data["profile"]["sectorAvoidSlots"]
    var picked = pick_("Navigation Avoids", commandActionHelp_, [
      "Set avoid sectors",
      "Clear all %(slots) avoid slots"
    ])
    if (picked < 0) return
    if (picked == 1) {
      return reviewCommandPlan_("Clear Navigation Avoids",
          "Clear every configured computer avoid slot.",
          "Changes autopilot and missile routing but is expected to use no turns.",
          [YTCommands.clearAvoid(slots)], slots)
    }
    var value = prompt_("Navigation Avoids", "Comma-separated sectors", "")
    if (value == null) return
    var sectors = YTText.uniqueSorted(YTText.numbers(value))
    if (sectors.count == 0) {
      alert_("Navigation Avoids", "Enter at least one sector.")
      return
    }
    if (sectors.count > slots) sectors = sectors[0...slots]
    return reviewCommandPlan_("Set Navigation Avoids",
        "Replace the first %(sectors.count) computer avoid slots.",
        "Changes autopilot and missile routing but is expected to use no turns.",
        [YTCommands.avoid(sectors, slots) + ";1"], sectors.count)
  }

  static surround_() {
    var center = integer_("Mercenary Surround", "Mercenary sector", 1, 1,
        YTState.data["profile"]["maxSector"])
    if (center == null) return
    var value = prompt_("Mercenary Surround", "Adjacent sectors", "")
    if (value == null) return
    var fighters = integer_("Mercenary Surround", "Fighters per sector", 100,
        100, 999999999)
    if (fighters == null) return
    var adjacent = YTText.uniqueSorted(YTText.numbers(value))
    if (adjacent.count == 0) {
      alert_("Mercenary Surround", "Enter at least one adjacent sector.")
      return
    }
    var command = YTCommands.surroundMercenaries(center, adjacent, fighters)
    return reviewCommandPlan_("Mercenary Surround",
        "Avoid sector %(center), visit %(adjacent.count) adjacent sectors, " +
        "and drop %(fighters) fighters in each.",
        "Consumes travel turns and %(fighters * adjacent.count) fighters. " +
        "Autopilot can encounter hazards; the avoid list is changed.",
        [command], adjacent.count)
  }

  static reviewCommandPlan_(title, description, warning, commands, itemCount) {
    if (commands.count == 0) {
      alert_(title, "No commands were generated.")
      return null
    }
    var help = "# %(title)\n\n%(description)\n\nTargets/actions: " +
        "%(itemCount)\nBatches: %(commands.count)\n\nWarning: %(warning)\n\n" +
        "Run now closes the panel and sends one batch at a time from the Main " +
        "Command prompt. A harmless Help request marks the end of each batch " +
        "before the next is sent. Copy is retained as an optional fallback."
    var selected = 0
    while (true) {
      var picked = pickWithSelection_("%(title): Review", help, [
        "Run now from Main Command",
        "Preview first batch",
        "Copy batches to clipboard",
        "Cancel"
      ], selected)
      if (picked < 0 || picked == 3) return null
      selected = picked
      if (picked == 0) {
        if (confirm_(title,
            "%(description)\n\n%(warning)\n\nRun %(commands.count) monitored " +
            "batches now? The terminal must currently be at Main Command.")) {
          return Fn.new { startCommandRun_(title, commands) }
        }
      }
      if (picked == 1) {
        alert_("%(title): First Batch", commandPreview_(commands[0]))
      }
      if (picked == 2) {
        Clipboard.text = YTText.join(commands, "\n")
        alert_(title, "Copied %(commands.count) reviewed command batches.")
      }
    }
  }

  static commandPreview_(command) {
    if (command.count <= 1200) return command
    return command[0...1200] + "\n... preview truncated ..."
  }

  static records_() {
    var selected = 0
    while (true) {
      var picked = pickWithSelection_(
          "Records: %(YTState.activeGameName)",
          recordsHelp_, [
        "Add or update sector note",
        "Add planet record",
        "Add profitable port pair",
        "Add revealed or special location",
        "Browse sector notes",
        "Browse planets",
        "Browse port pairs",
        "Browse tracked locations",
        "Run planet/fighter reconciliation reports"
      ], selected)
      if (picked < 0) return
      selected = picked
      if (picked == 0) addSector_()
      if (picked == 1) addPlanet_()
      if (picked == 2) addPair_()
      if (picked == 3) addLocation_()
      if (picked == 4) browseSectors_()
      if (picked == 5) browsePlanets_()
      if (picked == 6) browsePairs_()
      if (picked == 7) browseLocations_()
      if (picked == 8) {
        var run = reviewCommandPlan_("Reconciliation Reports",
            "Request the computer's planet list (C;11) and fighter list (C;13).",
            "Read-only computer reports are expected to use no turns.",
            ["c;11;1;c;13;1"], 2)
        if (run != null) return run
      }
    }
  }

  static addSector_() {
    var sector = integer_("Sector Note", "Sector", 1, 1,
        YTState.data["profile"]["maxSector"])
    if (sector == null) return
    var kinds = ["dead end", "unused hideout", "dangerous", "revealed",
        "black hole", "Xannor", "mercenary", "enemy", "friendly", "other"]
    var kind = pick_("Sector Kind", null, kinds)
    if (kind < 0) return
    var date = date_("Sector Note", "Date or last visit")
    if (date == null) return
    var note = prompt_("Sector Note", "Note", "")
    if (note == null) return
    reportSave_(YTState.putSector(sector, kinds[kind], note, date))
  }

  static addPlanet_() {
    var limit = YTState.data["profile"]["maxPlanets"]
    if (YTState.data["planets"].count >= limit) {
      alert_("Planet Record", "This game's limit of %(limit) planets is reached.")
      return
    }
    var sector = integer_("Planet Record", "Sector", 1, 1,
        YTState.data["profile"]["maxSector"])
    if (sector == null) return
    var name = prompt_("Planet Record", "Planet name", "New Planet")
    if (name == null) return
    var fighters = number_("Planet Record", "Defensive fighters", 0, 0,
        999999999999999)
    if (fighters == null) return
    var credits = number_("Planet Record", "Credits in millions", 0, 0,
        999999999999999)
    if (credits == null) return
    var forces = number_("Planet Record", "Ground forces", 0, 0,
        999999999999999)
    if (forces == null) return
    var date = date_("Planet Record", "Last visit or revealed date")
    if (date == null) return
    var note = prompt_("Planet Record", "Route and notes", "")
    if (note == null) return
    reportSave_(YTState.addPlanet(sector, name, fighters, credits, forces, date,
        note))
  }

  static addPair_() {
    var max = YTState.data["profile"]["maxSector"]
    var first = integer_("Port Pair", "First sector", 1, 1, max)
    if (first == null) return
    var second = integer_("Port Pair", "Second sector", 1, 1, max)
    if (second == null) return
    var profit = number_("Port Pair", "Profit per cargo hold", 0, 0, 999999999)
    if (profit == null) return
    var note = prompt_("Port Pair", "Goods and notes", "")
    if (note == null) return
    reportSave_(YTState.addPortPair(first, second, profit, note))
  }

  static addLocation_() {
    var sector = integer_("Tracked Location", "Sector", 1, 1,
        YTState.data["profile"]["maxSector"])
    if (sector == null) return
    var kinds = ["revealed planet", "enemy planet", "friendly planet",
        "Xannor group", "Xannor HQ", "Mercenary Base", "mercenaries",
        "fighter group", "black hole", "top player", "other"]
    var kind = pick_("Location Kind", null, kinds)
    if (kind < 0) return
    var owner = prompt_("Tracked Location", "Owner or label", "")
    if (owner == null) return
    var date = date_("Tracked Location", "Date")
    if (date == null) return
    var note = prompt_("Tracked Location", "Note", "")
    if (note == null) return
    reportSave_(YTState.addLocation(sector, kinds[kind], owner, date, note))
  }

  static browseSectors_() {
    var rows = []
    for (key in YTState.data["sectors"].keys) rows.add(YTState.data["sectors"][key])
    rows.sort {|a, b| a["sector"] < b["sector"] }
    if (rows.count == 0) return alert_("Sector Notes", "No sector notes yet.")
    var labels = rows.map {|r|
      return "%(r["sector"])  %(r["kind"] == null ? "mapped" : r["kind"])"
    }.toList
    var picked = pick_("Sector Notes", null, labels)
    if (picked < 0) return
    var r = rows[picked]
    alert_("Sector %(r["sector"])",
        "Kind: %(r["kind"])\nPort: %(r["port"])\n" +
        "Warps: %(r["warps"] == null ? "" : YTText.join(r["warps"], ", "))\n" +
        "Date: %(r["date"])\n%(r["note"])")
  }

  static browsePlanets_() { browseList_("Planets", YTState.data["planets"],
      Fn.new {|r| "%(r["sector"])  %(r["name"])" }, Fn.new {|r|
        "Sector: %(r["sector"])\nFighters: %(r["fighters"])\n" +
        "Credits: %(r["creditsMillions"]) million\n" +
        "Ground forces: %(r["groundForces"])\nDate: %(r["date"])\n%(r["note"])"
      }) }

  static browsePairs_() { browseList_("Port Pairs", YTState.data["portPairs"],
      Fn.new {|r| "%(r["first"])-%(r["second"])  %(r["profitPerHold"])/hold" },
      Fn.new {|r| "Sectors: %(r["first"])-%(r["second"])\n" +
          "Profit per hold: %(r["profitPerHold"])\n%(r["note"])" }) }

  static browseLocations_() { browseList_("Tracked Locations",
      YTState.data["locations"],
      Fn.new {|r| "%(r["sector"])  %(r["kind"])  %(r["owner"])" },
      Fn.new {|r| "Sector: %(r["sector"])\nType: %(r["kind"])\n" +
          "Owner: %(r["owner"])\nDate: %(r["date"])\n%(r["note"])" }) }

  static browseList_(title, rows, label, detail) {
    if (rows.count == 0) return alert_(title, "No records yet.")
    var labels = rows.map {|r| label.call(r) }.toList
    var picked = pick_(title, null, labels)
    if (picked >= 0) alert_(title, detail.call(rows[picked]))
  }

  static macros_() {
    var macros = YTCommands.defaultMacros(
        YTState.data["profile"]["macroRepeats"])
    var labels = macros.map {|m| "%(m[0])  %(m[1])" }.toList
    var picked = pick_("F1-F12 Bindings: %(YTState.activeGameName)", macroHelp_,
        labels)
    if (picked < 0) return null
    var macro = macros[picked]
    alert_("%(macro[0]) — %(macro[1])",
        "This key is active while connected to this BBS.\n\nCommand:\n" +
        "%(macro[3])")
    return null
  }

  static onMacroKey_(index) {
    var macros = YTCommands.defaultMacros(
        YTState.data["profile"]["macroRepeats"])
    if (index < 0 || index >= macros.count) return false
    if (__routePending || __fighterHome != null || __c10Active ||
        __benchmark != null || __detectActive) {
      showLiveNotice_("Function-key macro %(macros[index][0]) was ignored " +
          "while live automation was active.")
      return true
    }
    send_(macros[index][2] + "\r")
    return true
  }

  static switchGame_() {
    var picked = pickWithSelection_("Select Yankee Trader Game",
        gameHelp_, YTState.gameNames, YTState.activeGameIndex)
    if (picked >= 0 && picked != YTState.activeGameIndex) {
      cancelAutomation_()
      reportSave_(YTState.selectGame(picked))
    }
  }

  static addGame_() {
    var name = prompt_("Add Game", "Game name", "Game %(YTState.games.count + 1)")
    if (name == null) return
    var version = pick_("Initial Game Type", null,
        presetNames_)
    if (version < 0) return
    reportSave_(YTState.addGame(name, presetVersion_(version)))
  }

  static profile_() {
    var selected = 0
    while (true) {
      var p = YTState.data["profile"]
      var picked = pickWithSelection_(
          "Games: %(YTState.activeGameName)", dataHelp_, [
        "Switch game (%(YTState.games.count) configured)",
        "Add another game",
        "Rename active game",
        "Delete active game",
        "Detect game settings from the connected door",
        "Apply version defaults: %(p["version"])",
        "Universe size: %(p["maxSector"])",
        "Turns per day: %(p["turnsPerDay"])",
        "Maximum planets: %(p["maxPlanets"])",
        "Maximum ports: %(p["maxPorts"])",
        "Sector avoid slots: %(p["sectorAvoidSlots"])",
        "Macro repeat count: %(p["macroRepeats"])",
        "Missiles: %(enabled_(p["missiles"]))",
        "Plasma bolts: %(enabled_(p["plasmaBolts"]))",
        "Xannor: %(enabled_(p["xannor"]))",
        "Mercenaries: %(enabled_(p["mercenaries"]))",
        "Sensor robots: %(enabled_(p["sensorRobots"]))",
        "Danger scanner: %(enabled_(p["dangerScanner"]))",
        "Spies: %(enabled_(p["spies"]))",
        "Copy all BBS games as WON",
        "Replace all BBS games from clipboard WON"
      ], selected)
      if (picked < 0) return
      selected = picked
      if (picked == 0) switchGame_()
      if (picked == 1) addGame_()
      if (picked == 2) {
        var name = prompt_("Rename Game", "Game name", YTState.activeGameName)
        if (name != null) reportSave_(YTState.renameActiveGame(name))
      }
      if (picked == 3) {
        var deleting = YTState.activeGameName
        if (confirm_("Delete Game",
            "Delete %(deleting) and all of its maps, records, and settings?")) {
          reportSave_(YTState.deleteActiveGame())
        }
      }
      if (picked == 4) {
        if (confirm_("Detect Game Settings",
            "Start this from Yankee Trader's Main Command prompt. The " +
            "detector reads only zero-turn screens and uses C;2 port-report " +
            "validity to binary-search the sector ceiling. No settings are " +
            "changed until you review them. Start now?")) {
          return Fn.new { startDetect_() }
        }
      }
      if (picked == 5) {
        var version = pick_("Apply Version Defaults", null,
            presetNames_)
        if (version >= 0 && confirm_("Apply Version Defaults",
            "Reset this game's feature flags, sector limit, turns, and macro " +
            "count? Existing maps and records are retained.")) {
          reportSave_(YTState.setVersionDefaults(presetVersion_(version)))
        }
      }
      if (picked == 6) {
        var max = integer_("Universe Size", "Maximum sector", p["maxSector"],
            100, 99999)
        if (max != null) reportSave_(YTState.setProfileOption("maxSector", max))
      }
      if (picked == 7) {
        var turns = integer_("Turns Per Day", "Turns", p["turnsPerDay"],
            1, 999999)
        if (turns != null) reportSave_(YTState.setProfileOption("turnsPerDay",
            turns))
      }
      if (picked == 8) {
        var planets = integer_("Maximum Planets", "Planet limit", p["maxPlanets"],
            1, 10000)
        if (planets != null) reportSave_(YTState.setProfileOption("maxPlanets",
            planets))
      }
      if (picked == 9) {
        var ports = integer_("Maximum Ports", "Port limit", p["maxPorts"],
            1, 10000)
        if (ports != null) reportSave_(YTState.setProfileOption("maxPorts",
            ports))
      }
      if (picked == 10) {
        var slots = integer_("Sector Avoids", "Available slots",
            p["sectorAvoidSlots"], 1, 100)
        if (slots != null) reportSave_(YTState.setProfileOption(
            "sectorAvoidSlots", slots))
      }
      if (picked == 11) {
        var repeats = integer_("Macro Repeats", "Repeat count",
            p["macroRepeats"], 1, 1000)
        if (repeats != null) reportSave_(YTState.setProfileOption("macroRepeats",
            repeats))
      }
      if (picked == 12) reportSave_(YTState.setProfileOption("missiles",
          !p["missiles"]))
      if (picked == 13) reportSave_(YTState.setProfileOption("plasmaBolts",
          !p["plasmaBolts"]))
      if (picked == 14) reportSave_(YTState.setProfileOption("xannor",
          !p["xannor"]))
      if (picked == 15) reportSave_(YTState.setProfileOption("mercenaries",
          !p["mercenaries"]))
      if (picked == 16) reportSave_(YTState.setProfileOption("sensorRobots",
          !p["sensorRobots"]))
      if (picked == 17) reportSave_(YTState.setProfileOption("dangerScanner",
          !p["dangerScanner"]))
      if (picked == 18) reportSave_(YTState.setProfileOption("spies",
          !p["spies"]))
      if (picked == 19) {
        Clipboard.text = YTState.exportText
        alert_("State Export", "All games for this BBS are on the clipboard.")
      }
      if (picked == 20) importState_()
    }
  }

  static startDetect_() {
    cancelAutomation_()
    __detectActive = true
    __detectPhase = "info"
    __detectInfoText = ""
    __detectVersionText = ""
    __detectHelpText = ""
    __detectComputerText = ""
    __detectLow = 1
    // Profiles accept sectors through 99,999. Keep a known-invalid exclusive
    // upper bound so a ceiling of 99,999 is still detectable.
    __detectHigh = 100000
    __detectProbe = 0
    __detectProbeCount = 0
    sendDetect_("i\r")
  }

  static onDetectInfo_(match) {
    if (!__detectActive || __detectPhase != "info") return null
    __detectInfoText = matchText_(match)
    __detectPhase = "version"
    sendDetect_("v\r")
    return null
  }

  static onDetectVersion_(match) {
    if (!__detectActive || __detectPhase != "version") return null
    __detectVersionText = matchText_(match)
    __detectPhase = "help"
    sendDetect_(YTCommands.mainHelpCommand)
    return null
  }

  static onDetectHelp_(match) {
    if (!__detectActive || __detectPhase != "help") return null
    __detectHelpText = matchText_(match)
    __detectPhase = "computer"
    sendDetect_("c\r")
    return null
  }

  static detectHelpPattern_ { "<Help>(.+?)Main Command" }

  static onDetectComputer_(match) {
    if (!__detectActive || __detectPhase != "computer") return null
    __detectComputerText = matchText_(match)
    // The general Computer-prompt hook sees this same prompt immediately
    // after this more-specific capture hook. Let it start the first probe so
    // later Computer prompts can unambiguously mean that a probe was valid.
    __detectPhase = "sector-ready"
    return null
  }

  static onDetectInvalidSector_(match) {
    if (!__detectActive || __detectPhase != "sector" ||
        __detectProbe == 0) return null
    __detectHigh = __detectProbe
    // Yankee Trader remains inside C;2 after an invalid sector, so answer the
    // repeated sector prompt directly instead of sending another C;2 command.
    nextDetectSectorProbe_(true)
    return null
  }

  static nextDetectSectorProbe_(atSectorPrompt) {
    if (__detectHigh - __detectLow <= 1) {
      if (atSectorPrompt) {
        // Submit a known-valid sector to leave the repeated C;2 input prompt.
        // The resulting Computer prompt will perform the actual exit.
        __detectPhase = "sector-exit"
        sendDetect_("%(__detectLow)\r")
      } else {
        __detectPhase = "exit"
        sendDetect_("1\r")
      }
      return
    }
    __detectProbe = YTUniverse.nextBinarySectorProbe(__detectLow, __detectHigh)
    __detectProbeCount = __detectProbeCount + 1
    if (atSectorPrompt) {
      sendDetect_("%(__detectProbe)\r")
    } else {
      sendDetect_("2;%(__detectProbe)\r")
    }
  }

  static finishDetect_() {
    if (!__detectActive || __detectPhase != "exit") return null
    var detected = YTUniverse.detectGameSettings(__detectInfoText,
        __detectVersionText, __detectHelpText, __detectComputerText,
        __detectLow)
    detected["sectorProbeCount"] = __detectProbeCount
    __detectActive = false
    __detectPhase = null
    Fiber.new { reviewDetected_(detected) }.call()
    return null
  }

  static reviewDetected_(detected) {
    var version = detected["version"]
    if (version == null) {
      hookAlert_("Detect Game Settings",
          "The Version screen was captured, but its Yankee Trader version " +
          "was not recognized. No settings were changed.")
      return
    }

    var current = YTState.data["profile"]
    var useDefaults = hookConfirm_("Version-Based Settings",
        "Detected %(version). Some limits and features are not exposed by " +
        "a safe player command. Use the documented %(version) defaults for " +
        "those inferred settings? Choose No to retain their current values.")
    var proposed = {}
    var basis = useDefaults ? YTState.defaultProfile(version) : current
    for (key in basis.keys) proposed[key] = basis[key]
    proposed["version"] = version

    var sources = {}
    for (key in proposed.keys) {
      sources[key] = useDefaults ? "inferred from %(version)" :
          "retained from profile"
    }
    sources["version"] = "detected from Version screen"

    var direct = ["maxSector", "macroRepeats", "missiles", "plasmaBolts",
        "sensorRobots", "spies"]
    for (key in direct) {
      if (detected[key] != null) {
        proposed[key] = detected[key]
        sources[key] = key == "maxSector" ?
            "detected by C;2 binary search" : "detected from command screens"
      }
    }

    var shownTurns = detected["currentTurns"]
    var turnsSource = null
    if (shownTurns != null) {
      var changed = hookConfirm_("Verify Daily Turns",
          "The Info screen showed %(shownTurns) turns. Have you taken any " +
          "turns, or spent or gained turns in another way, since today's " +
          "reset? Choose No only if %(shownTurns) is still the untouched " +
          "daily allocation.")
      if (!changed && shownTurns >= 1) {
        proposed["turnsPerDay"] = shownTurns
        turnsSource = "verified untouched Info value"
      }
    }
    if (turnsSource == null) {
      var turns = hookInteger_("Verify Daily Turns",
          "Configured turns per day", current["turnsPerDay"], 1, 999999)
      if (turns == null) {
        hookAlert_("Detect Game Settings",
            "Detection review cancelled; no settings changed.")
        return
      }
      proposed["turnsPerDay"] = turns
      turnsSource = "entered by user after state check"
    }
    sources["turnsPerDay"] = turnsSource

    var banner = detected["banner"] == null ? version : detected["banner"]
    var inference = useDefaults ?
        "Unexposed fields use %(version) defaults." :
        "Unexposed fields retain the current profile."
    var summary = "Build: %(banner)\n" +
        "Universe: 1-%(proposed["maxSector"]) (binary search, " +
        "%(detected["sectorProbeCount"]) probes)\n" +
        "Command repeats: %(proposed["macroRepeats"])\n" +
        "Turns/day: %(proposed["turnsPerDay"]) (%(turnsSource))\n" +
        "Planet/port limits: %(proposed["maxPlanets"])/" +
        "%(proposed["maxPorts"])\nAvoid slots: " +
        "%(proposed["sectorAvoidSlots"])\n\n" +
        "Missiles: %(enabled_(proposed["missiles"]))  Plasma: " +
        "%(enabled_(proposed["plasmaBolts"]))\n" +
        "Robots: %(enabled_(proposed["sensorRobots"]))  Danger scanner: " +
        "%(enabled_(proposed["dangerScanner"]))  Spies: " +
        "%(enabled_(proposed["spies"]))\n" +
        "Xannor: %(enabled_(proposed["xannor"]))  Mercenaries: " +
        "%(enabled_(proposed["mercenaries"]))\n\n%(inference)"
    if (!hookConfirm_("Apply Detected Settings", summary +
        "\n\nApply these settings to %(YTState.activeGameName)?")) {
      hookAlert_("Detect Game Settings", "No settings were changed.")
      return
    }

    var detection = {
      "banner": banner,
      "sectorProbeCount": detected["sectorProbeCount"],
      "sources": sources
    }
    var error = YTState.applyDetectedProfile(proposed, detection)
    if (error == null) {
      hookAlert_("Detect Game Settings",
          "Detected settings were saved for %(YTState.activeGameName).")
    } else {
      hookAlert_("Save Error", error)
    }
  }

  static onDetectContinue_(match) {
    if (__detectActive && (__detectPhase == "info" ||
        __detectPhase == "version" || __detectPhase == "help")) {
      sendDetect_("\r")
    }
    return null
  }

  static onDetectTimer_() {
    if (__detectActive && Timer.now - __detectLastSentAt > 30) {
      var phase = __detectPhase
      __detectActive = false
      __detectPhase = null
      showLiveNotice_("Game-setting detection timed out while reading the " +
          "%(phase) screen. No settings were changed. Return to Main Command " +
          "and try again.")
    }
    return null
  }

  static sendDetect_(text) {
    __detectLastSentAt = Timer.now
    send_(text)
  }

  static matchText_(match) {
    if (match is List && match.count > 0) return match[0]
    return ""
  }

  static detectStatus_ {
    return {
      "active": __detectActive,
      "phase": __detectPhase,
      "low": __detectLow,
      "high": __detectHigh,
      "probe": __detectProbe,
      "probes": __detectProbeCount
    }
  }

  static presetNames_ { [
    "Stock YT 3.6 / 3,000 sectors",
    "YT 3.6G mod / 2,004 sectors",
    "YT 3.2 / 1,000 sectors"
  ] }

  static presetVersion_(index) {
    if (index == 1) return "3.6G mod"
    if (index == 2) return "3.2"
    return "modern"
  }

  static importState_() {
    var text = Clipboard.text
    if (text == null || text.count == 0) return alert_("State Import",
        "The clipboard is empty.")
    var parsed = WON.deserialize(text)
    if (parsed is WONError) return alert_("State Import", parsed.toString)
    if (!confirm_("State Import",
        "Replace this BBS's Yankee Trader records with the clipboard data?")) return
    var error = YTState.replace(parsed)
    if (error == null) {
      alert_("State Import", "State imported successfully.")
    } else {
      alert_("State Import", error)
    }
  }

  static reference_() {
    var p = YTState.data["profile"]
    var topics = [
      ["This game's rules", "%(YTState.activeGameName): sectors 1-%(p["maxSector"]), " +
        "%(p["turnsPerDay"]) turns/day, %(p["maxPlanets"]) planets, " +
        "%(p["maxPorts"]) ports, %(p["sectorAvoidSlots"]) avoid slots, and " +
        "command repeats up to %(p["macroRepeats"]). Feature flags in Profile " +
        "and Data override the %(p["version"]) starting preset."],
      ["Daily play", "Early game: explore, record dead ends and port pairs, " +
        "build planets, and read both newspapers. Late game: protect secrecy, " +
        "cycle productive planets, map with missiles, and plan Xannor turn refills."],
      ["Money and ports", "Track two-way profitable port pairs. With 1,000 holds, " +
        "profit is 1,000 times the per-hold spread per four-turn round trip. " +
        "Live or imported sensor data records ports; the saved-map tool builds " +
        "no-turn C;2 checks for them."],
      ["Planets", "Prefer separate dead ends, keep detailed fighters/credits/GF " +
        "and visit dates, and move or empty revealed planets. Banked credits drive " +
        "daily weapons and forces. Stock 3.2 allowed 75 planets and stock 3.6 " +
        "allowed 100; record a modified game's actual limit in its settings."],
      ["Warfare", "Plasma damage is quadratic and falls two percentage points per " +
        "sector after the first. It hits fighters, shields, mines, and planet GF " +
        "and productivity. Missiles can reveal locations and partly bypass fighters."],
      ["Xannor", "Keep roughly Xannor score divided by 50,000 ground forces on each " +
        "planet. Fighter kills return five turns per 1,000 Xannor; reduce large " +
        "groups with plasma but leave about 100,000 for a full turn refill."],
      ["Mercenaries", "A bribe costs 2.5 credits per fighter. To absorb a huge " +
        "group, exclude its sector and put at least 100 defensive fighters in every " +
        "adjacent sector near the end of the day."],
      ["Mapping", "Use travel logs or the no-turn C;10 sweep. Native analysis finds " +
        "direct dead ends, unique coverage candidates, long links, and possible " +
        "single-entrance rifts, then exports missile/planet/robot scripts."],
      ["Scanners and spies", "YT 3.2 uses sensor robots. Stock 3.6 removes them, " +
        "adds the 250,000-credit Danger Scanner that stops before hazards, and " +
        "adds up to three 333,000-credit spies that roam and report enemy forces " +
        "or planets. Modified games may enable a different combination."],
      ["Advanced", "The control panel covers route scanning, adjacent fighter " +
        "breadcrumbs, Xannor HQ retaliation backtracing support, Mercenary Base safe " +
        "plasma shots, Group 20 notes, avoid lists, and black-hole missile scripts."],
      ["Operational caution", "Mines and attacks are random; online multiplayer has " +
        "known stale-location behavior. Missile, plasma, surrender, mercenary, and " +
        "retaliation events can reveal locations. Keep captures and verify outputs."]
    ]
    var picked = pick_("Strategy Reference", null,
        topics.map {|t| t[0] }.toList)
    if (picked >= 0) alert_(topics[picked][0], topics[picked][1])
  }

  static onRoute_(match) {
    if (!__routePending) return null
    __routePending = false
    var route = YTUniverse.parseRouteBlock(match[0],
        YTState.data["profile"]["maxSector"])
    if (route.count > 1) route.removeAt(0)
    if (route.count == 0) {
      send_("n\r")
      showLiveNotice_("The C;3 route was not recognized; no movement was sent.")
      return null
    }
    send_("n;1;s;" + YTCommands.scanRoute(route) + "\r")
    showLiveNotice_("Route scan queued for %(route.count) sectors.")
    return null
  }

  static onSensor_(match) {
    if (match == null) return null
    var graph = YTUniverse.parseSensorLog(match[0])
    var saveError = null
    if (graph.count > 0) saveError = YTState.mergeGraph(graph)
    if (__fighterHome == null) {
      if (saveError != null) {
        showLiveNotice_("The live sensor map could not be saved: %(saveError)")
      }
      return null
    }
    var home = __fighterHome
    __fighterHome = null
    var adjacent = []
    for (key in graph.keys) {
      var sector = Num.fromString(key)
      if (sector != home) adjacent.add(sector)
    }
    adjacent.sort {|a, b| a < b }
    if (adjacent.count == 0) {
      showLiveNotice_("No adjacent sectors were recognized; no movement was sent.")
      return null
    }
    var command = ""
    for (sector in adjacent) {
      if (command.count > 0) command = command + ";"
      command = command + "m;%(sector);f;1;s;m;%(home)"
    }
    send_(command + "\r")
    var message = "Fighter breadcrumbs queued in %(adjacent.count) sectors."
    if (saveError != null) {
      message = message + " Sensor-map save failed: %(saveError)"
    }
    showLiveNotice_(message)
    return null
  }

  static onPathResult_(match) {
    if (!__c10Active || !__c10AwaitingResult || match == null) return null
    var path = YTUniverse.parsePathFinder(match[0], __c10Source,
        __c10Current, YTState.data["profile"]["maxSector"])
    if (path.count > 0) {
      __c10Paths[__c10Current.toString] = path
      __c10CapturedTarget = __c10Current
    }
    return null
  }

  static sendC10Query_() {
    __c10AwaitingResult = true
    __c10CapturedTarget = 0
    __c10LastSentAt = Timer.now
    if (__c10StartedAt == 0) __c10StartedAt = __c10LastSentAt
    send_(YTCommands.c10Command(__c10Source, __c10Current))
  }

  static completeC10Query_() {
    if (!__c10Active || !__c10AwaitingResult) return null
    __c10AwaitingResult = false
    var now = Timer.now
    __c10LastSeconds = now - __c10LastSentAt
    __c10TotalSeconds = __c10TotalSeconds + __c10LastSeconds
    if (__c10CapturedTarget != __c10Current) __c10Missed = __c10Missed + 1
    __c10Completed = __c10Completed + 1
    if (__c10Next <= __c10Stop) {
      if (__c10Completed % 100 == 0) {
        var checkpointError = saveC10_(false)
        if (checkpointError != null) {
          __c10Active = false
          showLiveNotice_("C;10 stopped after %(__c10Completed) queries " +
              "because its checkpoint could not be saved: " +
              "%(checkpointError)")
          return null
        }
      }
      __c10Current = __c10Next
      __c10Next = __c10Next + 1
      sendC10Query_()
      return null
    }

    __c10Active = false
    var average = __c10TotalSeconds / __c10Completed
    var saveError = saveC10_(true)
    var message
    if (saveError == null) {
      message = "C;10 sweep completed and saved: " +
          "%(__c10Paths.count) paths from %(__c10Completed) queries in " +
          "%(oneDecimal_(__c10TotalSeconds)) seconds (" +
          "%(oneDecimal_(average)) seconds/query)."
      if (__c10Missed > 0) {
        message = message + " %(__c10Missed) responses were " +
            "not recognized and are not in the saved map."
      }
    } else {
      message = "C;10 sweep completed, but its map could not be " +
          "saved: %(saveError)"
    }
    showLiveNotice_(message)
    return null
  }

  static onMainPrompt_(match) {
    if (__detectActive && __detectPhase == "exit") return finishDetect_()
    if (__benchmark == "t1") {
      __benchmarkRemaining = __benchmarkRemaining - 1
      if (__benchmarkRemaining == 0) finishBenchmark_()
    }
    return null
  }

  static onMore_(match) {
    if (__detectActive && (__detectPhase == "info" ||
        __detectPhase == "version" || __detectPhase == "help")) {
      sendDetect_("\r")
      return null
    }
    if (__benchmark == "t2") {
      if (__benchmarkRemaining > 0) {
        __benchmarkRemaining = __benchmarkRemaining - 1
        send_("\r")
      } else {
        send_("n")
        finishBenchmark_()
      }
    }
    return null
  }

  static onComputerPrompt_(match) {
    if (__detectActive) {
      if (__detectPhase == "sector-ready") {
        __detectPhase = "sector"
        nextDetectSectorProbe_(false)
        return null
      }
      if (__detectPhase == "sector") {
        __detectLow = __detectProbe
        nextDetectSectorProbe_(false)
        return null
      }
      if (__detectPhase == "sector-exit") {
        __detectPhase = "exit"
        sendDetect_("1\r")
        return null
      }
    }
    if (__c10Active) {
      if (__c10AwaitingResult) {
        completeC10Query_()
      } else {
        sendC10Query_()
      }
      return null
    }
    if (__benchmark == "t3") {
      if (__t3WaitingToStart) {
        __t3WaitingToStart = false
        __benchmarkStarted = Timer.now
      }
      if (__benchmarkCommands.count > 0) {
        send_(__benchmarkCommands.removeAt(0))
      } else {
        // The twentieth result is complete at this prompt. Stop the benchmark
        // clock here; exiting the Computer is cleanup, not path-search time.
        send_("1\r")
        finishBenchmark_()
      }
      return null
    }
    return null
  }

  static onComputerDeactivated_(match) {
    if (__detectActive && __detectPhase == "exit") return finishDetect_()
    return null
  }

  static send_(text) {
    var error = Conn.send(text)
    if (error != null) __liveNotice = error.toString
    return error
  }

  // Each short-lived UI App must own a screen snapshot. App.run()/runSync()
  // intentionally leave their final frame visible; modalRun restores the
  // menu or terminal content that the child covered.
  static pick_(title, helpText, items) {
    return Screen.modalRun(Fn.new {
      return ListPicker.pick(title, helpText, items)
    })
  }

  static pickWithSelection_(title, helpText, items, selected) {
    return Screen.modalRun(Fn.new {
      return ListPicker.pickWithSelection(title, helpText, items, selected)
    })
  }

  static confirm_(title, message) {
    return Screen.modalRun(Fn.new {
      return MenuUi.confirmStandalone(title, message)
    })
  }

  static promptValue_(title, message, initial, maxLen) {
    return Screen.modalRun(Fn.new {
      return MenuUi.promptStandalone(title, message, initial, maxLen, false)
    })
  }

  static prompt_(title, message, initial) {
    return promptValue_(title, message, initial, 1024)
  }

  static integer_(title, message, initial, minimum, maximum) {
    while (true) {
      var value = promptValue_(title, message, initial.toString, 20)
      if (value == null) return null
      var number = Num.fromString(value.trim())
      if (number != null && number.isInteger && number >= minimum &&
          number <= maximum) return number
      alert_(title, "Enter an integer from %(minimum) through %(maximum).")
    }
  }

  static number_(title, message, initial, minimum, maximum) {
    while (true) {
      var value = promptValue_(title, message, initial.toString, 24)
      if (value == null) return null
      var number = Num.fromString(value.trim())
      if (number != null && number >= minimum && number <= maximum) return number
      alert_(title, "Enter a number from %(minimum) through %(maximum).")
    }
  }

  static alert_(title, message) {
    Screen.modalRun(Fn.new { MenuUi.alertStandalone(title, message) })
  }

  // MenuUi's standalone helpers use App.runSync(), which is appropriate for
  // the Alt-Y menu but blocks when called from an inbound-stream/timer hook.
  // A normal App.run() yields the child fiber back to the hook dispatcher,
  // then its input claim resumes that fiber when the user presses a key.
  static hookPopup_(popup) {
    var result = Screen.modalRun(Fn.new {
      var app = App.new()
      popup.onDismiss = Fn.new {|value| app.quit() }
      app.pushModal(popup)
      app.run()
      return popup.result
    })
    Input.setupMouseEvents()
    return result
  }

  static hookConfirm_(title, message) {
    var popup = Confirm.new(message)
    popup.title = title
    popup.bounds = Popup.centeredBounds_(message, 1, 24)
    return hookPopup_(popup)
  }

  static hookPromptValue_(title, message, initial, maxLen) {
    var popup = Prompt.new(message, initial)
    popup.title = title
    popup.input.maxLen = maxLen
    popup.sizeForInput(maxLen, 34)
    return hookPopup_(popup)
  }

  static hookInteger_(title, message, initial, minimum, maximum) {
    while (true) {
      var value = hookPromptValue_(title, message, initial.toString, 20)
      if (value == null) return null
      var number = Num.fromString(value.trim())
      if (number != null && number.isInteger && number >= minimum &&
          number <= maximum) return number
      hookAlert_(title,
          "Enter an integer from %(minimum) through %(maximum).")
    }
  }

  static hookAlert_(title, message) {
    var popup = Alert.new(title, message)
    popup.bounds = Popup.centeredBounds_(message, 1, 24)
    hookPopup_(popup)
  }

  // Tests can retain the old deferred behavior.
  static showLiveNotice_(message) {
    if (!__immediateNotices) {
      __liveNotice = message
      return null
    }
    __liveNotice = null
    Fiber.new { hookAlert_("Live Tool", message) }.call()
    return null
  }

  static immediateNotices=(enabled) { __immediateNotices = enabled == true }
  static commandRunSends=(enabled) { __commandRunSends = enabled == true }

  static reportSave_(error) {
    if (error != null) alert_("Save Error", error)
  }

  static saveC10_(complete) {
    var result = YTUniverse.analyze10Paths(__c10Paths,
        YTState.data["profile"]["maxSector"], __c10Source)
    result["targetStart"] = __c10Start
    result["targetStop"] = __c10Stop
    result["queryCount"] = __c10Completed
    var completedThrough = __c10Start - 1
    if (__c10Completed > 0) completedThrough = __c10Current
    result["completedThrough"] = completedThrough
    result["missedPaths"] = __c10Missed
    result["complete"] = complete
    result["elapsedSeconds"] = __c10TotalSeconds
    return YTState.setAnalysis(result)
  }

  static onDisconnect_() {
    if (__c10Active) saveC10_(false)
    return null
  }

  static c10Status_ {
    var total = 0
    if (__c10Start > 0 && __c10Stop >= __c10Start) {
      total = __c10Stop - __c10Start + 1
    }
    var average = __c10Completed == 0 ? 0 :
        __c10TotalSeconds / __c10Completed
    var remaining = total - __c10Completed
    return {
      "active": __c10Active,
      "source": __c10Source,
      "current": __c10Current,
      "captured": __c10Paths.count,
      "missed": __c10Missed,
      "completed": __c10Completed,
      "total": total,
      "lastSeconds": __c10LastSeconds,
      "totalSeconds": __c10TotalSeconds,
      "averageSeconds": average,
      "etaSeconds": average * remaining
    }
  }

  static pendingNotice_ { __liveNotice }

  static benchmarkStatus_ {
    return {
      "name": __benchmark,
      "commands": __benchmarkCommands.count,
      "waiting": __t3WaitingToStart
    }
  }

  static commandRunStatus_ {
    var total = __commandRunBatches.count
    var current = 0
    if (total > 0) current = (__commandRunIndex + 1).min(total)
    return {
      "active": __commandRunActive,
      "name": __commandRunName,
      "current": current,
      "completed": __commandRunIndex.min(total),
      "total": total
    }
  }

  static benchmarkSummary_(benchmarks) {
    if (benchmarks.count == 0) return "none recorded"
    return "T1 %(oneDecimal_(benchmarks["t1"]))  " +
        "T2 %(oneDecimal_(benchmarks["t2"]))  " +
        "T3 %(oneDecimal_(benchmarks["t3"]))"
  }

  static oneDecimal_(value) {
    if (!(value is Num) || value.isNan || value.isInfinity) return "-"
    var tenths = (value * 10).round
    var sign = tenths < 0 ? "-" : ""
    var magnitude = tenths.abs
    return "%(sign)%((magnitude / 10).floor)." +
        "%((magnitude % 10).floor)"
  }

  static date_(title, message) {
    var result = null
    var today = CalendarDate.fromTimestamp(BBS.connected + Conn.elapsedSeconds)
    Screen.modalRun(Fn.new {
      var app = App.new()
      var pane = Pane.new()
      pane.title = message
      pane.focused = true
      pane.shadow = true
      pane.helpText = "# %(title): %(message)\n\nDefaults to today. Select a " +
          "day from the month calendar. Neighboring-month days can be chosen " +
          "directly.\n\nArrow keys move by a day or week.\n" +
          "Page Up / Page Down changes month.\n" +
          "Home / End selects the first or last day.\n" +
          "Enter or a double-click chooses the highlighted date."
      pane.onClose = Fn.new { app.quit() }
      pane.bounds = Pane.modalBounds(36, 14)
      app.root.add(pane)

      var calendar = DatePicker.new(today[0], today[1], today[2])
      var choose = Fn.new {
        result = calendar.value
        app.quit()
      }
      calendar.onAccept = choose

      var todayButton = Button.new("Today")
      todayButton.onPress = Fn.new {
        calendar.setDate(today[0], today[1], today[2])
      }
      var chooseButton = Button.new("Choose")
      chooseButton.onPress = choose
      var cancelButton = Button.new("Cancel")
      cancelButton.onPress = Fn.new { app.quit() }

      var inner = pane.innerBounds
      var calendarX = inner.x + ((inner.w - calendar.preferredWidth) / 2).floor
      calendar.bounds = Rect.new(calendarX, inner.y,
          calendar.preferredWidth, calendar.preferredHeight)
      var buttonWidth = todayButton.intrinsicWidth +
          chooseButton.intrinsicWidth + cancelButton.intrinsicWidth + 2
      var buttonX = inner.x + ((inner.w - buttonWidth) / 2).floor
      var buttonY = inner.bottom
      todayButton.bounds = Rect.new(buttonX, buttonY,
          todayButton.intrinsicWidth, 1)
      buttonX = buttonX + todayButton.intrinsicWidth + 1
      chooseButton.bounds = Rect.new(buttonX, buttonY,
          chooseButton.intrinsicWidth, 1)
      buttonX = buttonX + chooseButton.intrinsicWidth + 1
      cancelButton.bounds = Rect.new(buttonX, buttonY,
          cancelButton.intrinsicWidth, 1)

      pane.add(calendar)
      pane.add(todayButton)
      pane.add(chooseButton)
      pane.add(cancelButton)
      app.runSync()
    })
    return result
  }

  static enabled_(value) { value ? "on" : "off" }

  static help_ { "# Yankee Trader Control Panel\n\nAlt-Y opens this panel. " +
      "The first two entries provide quick access to the frequently used " +
      "route-scan and adjacent-fighter actions. " +
      "It combines the site's ZOC scripts, spreadsheet calculators, mapping " +
      "helpers, records, macros, and strategy notes. Each BBS may contain " +
      "multiple independent Yankee Trader games." }
  static liveHelp_ { "# Live Tools\n\nLive actions send commands to the connected " +
      "game. Start them from the expected Main or Computer command prompt. " +
      "Use Cancel to stop future scripted sends. Completion and error status " +
      "appears automatically. YT 3.6 performs pathfinding from disk, so C;10 " +
      "response time may increase during a large sweep." }
  static calculatorHelp_ { "# Calculators\n\nFormulas are converted from the " +
      "Yankee Trader Assistant spreadsheet and the website's damage tables. " +
      "Each calculator shows every input together and refreshes its result " +
      "beside the form as values are edited, making repeated what-if " +
      "calculations immediate." }
  static universeHelp_ { "# Universe Tools\n\nLive C;10 sweeps and sensor " +
      "scans are captured and saved automatically. Actions are reviewed in " +
      "the panel, then either run directly in monitored batches or optionally " +
      "copied. Start direct actions at Yankee Trader's Main Command prompt. " +
      "Cancel stops future batches but cannot retract a batch already sent. " +
      "Old external captures are isolated under Legacy and recovery imports." }
  static legacyImportHelp_ { "# Legacy and Recovery Imports\n\nThese tools are " +
      "not part of normal live operation. Use them only to recover an old " +
      "10-scan CSV or a previously captured terminal session. They read the " +
      "clipboard and merge or replace the active game's saved map analysis." }
  static commandActionHelp_ { "# In-Game Actions\n\nChoose what Yankee Trader " +
      "should do with the selected sectors. Before anything is sent, the next " +
      "screen shows the number of targets and batches, describes turn/resource " +
      "risk, and offers Run now, Preview, Copy, or Cancel. Direct runs must " +
      "start at Main Command and can be stopped with Cancel pending automation." }
  static mapHelp_ { "# Saved C;10 Map\n\nA C;10 sweep records shortest paths " +
      "from one source sector. It is not a complete adjacency map. An edge " +
      "`A -> B` means that directed step occurred in a captured path; it does " +
      "not prove `B -> A` exists.\n\n" +
      "## Map summary\n\nShows the viewpoint, queried target range, completion " +
      "state, and response count. Check this first to see whether the sweep " +
      "covered the intended range or is only a partial checkpoint.\n\n" +
      "## Direct outgoing warps\n\nDestinations reached from the viewpoint in " +
      "one jump. These are the exits C;10 exposed directly from the source and " +
      "are useful for understanding or checking that starting position.\n\n" +
      "## Coverage/scouting sectors\n\nSectors appearing in exactly one saved " +
      "shortest path. This is the original 10-scan missile/visit list: scouting " +
      "these uncommon branches is more useful than repeatedly probing sectors " +
      "shared by many routes. It is a coverage heuristic, not a claim that the " +
      "listed sector itself is a dead end.\n\n" +
      "## Dead-end probe edges\n\nThe final `A -> B` step of a saved route when " +
      "target B is also a coverage sector. Probe B from A to determine whether " +
      "it is really a dead end. The sweep alone does not confirm that B has no " +
      "other outgoing warp.\n\n" +
      "## Long-number-gap link probes\n\nA path edge whose sector numbers differ " +
      "by more than 10. The old scanner used this numerical-gap heuristic to " +
      "find possible entrances to separately numbered branches. It is not " +
      "distance and does not by itself prove an isolated region.\n\n" +
      "## Possible rift entrance probes\n\nAn edge whose destination occurs in " +
      "20 through 119 captured paths. The old scanner treated that frequency " +
      "as a clue that the edge may lead into an isolated, single-entrance group " +
      "of roughly that size. Follow-up probing is required; this list is not " +
      "proof of a rift.\n\n" +
      "## Find captured path by target sector\n\nOpens the exact saved shortest " +
      "path from the viewpoint to a queried target. Only targets successfully " +
      "captured by this sweep are available.\n\n" +
      "Selecting any listed sector or edge shows the captured target routes " +
      "that contain it, followed by the full sector sequence and jump count." }
  static recordsHelp_ { "# Records\n\nPer-BBS notes replace the site's suggested " +
      "Notepad tabs: planets, dead ends, revealed locations, intelligence, and " +
      "profitable port pairs. Date fields open a month calendar defaulted to " +
      "today and save the selection as YYYY-MM-DD." }
  static macroHelp_ { "# F1-F12 Key Bindings\n\nThese are active key bindings, " +
      "not menu actions. Close the panel and press a function key at the " +
      "appropriate Yankee Trader prompt. The site's /R repeat suffixes are " +
      "expanded natively using this game's repeat setting." }
  static dataHelp_ { "# Games and Data\n\nEach game has independent settings, " +
      "maps, notes, analyses, and benchmarks. Modern and YT 3.2 defaults are " +
      "starting points; every feature flag and limit can be changed per game. " +
      "Detect Game Settings reads safe player screens, binary-searches the " +
      "sector ceiling, and asks you to verify state-dependent values before " +
      "applying them. Each game uses separate profile, universe, and records " +
      "WON files in this BBS's SyncTERM cache." }
  static gameHelp_ { "# Select Game\n\nSwitching changes the settings and records " +
      "used by every control-panel feature. Any pending live automation is " +
      "cancelled before the switch." }
}

YankeeTrader.initialize_()

Hook.onKey(Key.altY) {|key|
  YankeeTrader.run()
  return true
}
Hook.onKey(Key.f1, Fn.new {|key| YankeeTrader.onMacroKey_(0) })
Hook.onKey(Key.f2, Fn.new {|key| YankeeTrader.onMacroKey_(1) })
Hook.onKey(Key.f3, Fn.new {|key| YankeeTrader.onMacroKey_(2) })
Hook.onKey(Key.f4, Fn.new {|key| YankeeTrader.onMacroKey_(3) })
Hook.onKey(Key.f5, Fn.new {|key| YankeeTrader.onMacroKey_(4) })
Hook.onKey(Key.f6, Fn.new {|key| YankeeTrader.onMacroKey_(5) })
Hook.onKey(Key.f7, Fn.new {|key| YankeeTrader.onMacroKey_(6) })
Hook.onKey(Key.f8, Fn.new {|key| YankeeTrader.onMacroKey_(7) })
Hook.onKey(Key.f9, Fn.new {|key| YankeeTrader.onMacroKey_(8) })
Hook.onKey(Key.f10, Fn.new {|key| YankeeTrader.onMacroKey_(9) })
Hook.onKey(Key.f11, Fn.new {|key| YankeeTrader.onMacroKey_(10) })
Hook.onKey(Key.f12, Fn.new {|key| YankeeTrader.onMacroKey_(11) })
Hook.onMatchClean("Name  (.+?)Main Command",
    Fn.new {|match| YankeeTrader.onDetectInfo_(match) })
Hook.onMatchClean("Version (.+?)Main Command",
    Fn.new {|match| YankeeTrader.onDetectVersion_(match) })
Hook.onMatchClean(YankeeTrader.detectHelpPattern_,
    Fn.new {|match| YankeeTrader.onHelp_(match) })
Hook.onMatchClean("<Computer activated>(.+?)Computer command",
    Fn.new {|match| YankeeTrader.onDetectComputer_(match) })
Hook.onMatchClean("Invalid sector number!(.+?)Enter sector number port is in",
    Fn.new {|match| YankeeTrader.onDetectInvalidSector_(match) })
Hook.onMatchClean("Hit [Enter]",
    Fn.new {|match| YankeeTrader.onDetectContinue_(match) })
Hook.onMatchClean("Course will(.+?)Enter course into autopilot",
    Fn.new {|match| YankeeTrader.onRoute_(match) })
Hook.onMatchClean("[ Sensors Activated ](.+?)[ End Sensor Scan ]",
    Fn.new {|match| YankeeTrader.onSensor_(match) })
Hook.onMatchClean("The shortest path from sector(.+?)Course will take",
    Fn.new {|match| YankeeTrader.onPathResult_(match) })
Hook.onMatchClean("Main Command ",
    Fn.new {|match| YankeeTrader.onMainPrompt_(match) })
Hook.onMatchClean("More", Fn.new {|match| YankeeTrader.onMore_(match) })
Hook.onMatchClean("Computer command",
    Fn.new {|match| YankeeTrader.onComputerPrompt_(match) })
Hook.onMatchClean("<Computer deactivated>",
    Fn.new {|match| YankeeTrader.onComputerDeactivated_(match) })
Hook.onDisconnect(Fn.new { YankeeTrader.onDisconnect_() })
Hook.every(1000) { YankeeTrader.onDetectTimer_() }
