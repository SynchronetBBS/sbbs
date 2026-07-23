// Per-BBS registry plus per-game persistent state for Yankee Trader.

import "syncterm" for Cache, FileError, WON, WONError

class YTState {
  static initialize_() {
    __stateData = null
    __stateError = null
  }

  static registryFilename { "yankee-trader-games.won" }
  static profileFilename_(id) { "yankee-trader-%(id)-profile.won" }
  static universeFilename_(id) { "yankee-trader-%(id)-universe.won" }
  static recordsFilename_(id) { "yankee-trader-%(id)-records.won" }

  static defaultProfile(version) {
    if (version == "3.2") {
      return {
        "version": "3.2",
        "maxSector": 1000,
        "macroRepeats": 30,
        "maxPlanets": 75,
        "maxPorts": 300,
        "turnsPerDay": 500,
        "sectorAvoidSlots": 30,
        "missiles": true,
        "plasmaBolts": false,
        "xannor": true,
        "mercenaries": true,
        "sensorRobots": true,
        "dangerScanner": false,
        "spies": false
      }
    }
    if (version == "3.6G mod") {
      return {
        "version": "3.6G mod",
        "maxSector": 2004,
        "macroRepeats": 20,
        "maxPlanets": 100,
        "maxPorts": 1000,
        "turnsPerDay": 500,
        "sectorAvoidSlots": 30,
        "missiles": false,
        "plasmaBolts": true,
        "xannor": true,
        "mercenaries": true,
        "sensorRobots": false,
        "dangerScanner": true,
        "spies": true
      }
    }
    return {
      "version": "3.6",
      "maxSector": 3000,
      "macroRepeats": 20,
      "maxPlanets": 100,
      "maxPorts": 1000,
      "turnsPerDay": 500,
      "sectorAvoidSlots": 30,
      "missiles": true,
      "plasmaBolts": true,
      "xannor": true,
      "mercenaries": true,
      "sensorRobots": false,
      "dangerScanner": true,
      "spies": true
    }
  }

  static defaultGame(name) { defaultGame(name, "modern") }

  static defaultGame(name, version) {
    return {
      "id": "g1",
      "name": name,
      "profile": defaultProfile(version),
      "sectors": {},
      "planets": [],
      "portPairs": [],
      "locations": [],
      "benchmarks": {},
      "lastAnalysis": null,
      "profileDetection": null
    }
  }

  static defaultData {
    return {
      "schema": 2,
      "activeGame": 0,
      "nextGameId": 2,
      "games": [defaultGame("Game 1")]
    }
  }

  static root {
    if (__stateData == null) load()
    return __stateData
  }

  // Existing callers operate on one unified in-memory root. Persistence below
  // projects that root into the registry and the selected per-game stores.
  static data { root["games"][root["activeGame"]] }
  static activeGameIndex { root["activeGame"] }
  static activeGameName { data["name"] }
  static games { root["games"] }
  static gameNames { games.map {|game| game["name"] }.toList }
  static lastError { __stateError }

  static load() {
    __stateError = null
    __stateData = defaultData
    applyRegistry_(readMap_(registryFilename))
    for (game in __stateData["games"]) {
      applyProfile_(game, readMap_(profileFilename_(game["id"])))
      applyUniverse_(game, readMap_(universeFilename_(game["id"])))
      applyRecords_(game, readMap_(recordsFilename_(game["id"])))
    }
    return __stateData
  }

  static save() {
    if (__stateData == null) __stateData = defaultData
    var error = saveRegistry_()
    if (error != null) return error
    for (game in games) {
      error = saveProfile_(game)
      if (error != null) return error
      error = saveUniverse_(game)
      if (error != null) return error
      error = saveRecords_(game)
      if (error != null) return error
    }
    return null
  }

  static readMap_(name) {
    if (!Cache.contains(name)) return null
    var file = Cache.list[name]
    var result = file.open()
    if (result is FileError) {
      noteError_(result.toString)
      return null
    }
    var text = file.read()
    file.close()
    if (text is FileError) {
      noteError_(text.toString)
      return null
    }
    var parsed = WON.deserialize(text)
    if (parsed is WONError) {
      noteError_("%(name): %(parsed)")
      return null
    }
    if (!(parsed is Map)) {
      noteError_("%(name): top-level value is not a map.")
      return null
    }
    return parsed
  }

  static noteError_(message) {
    if (__stateError == null) {
      __stateError = message
    } else {
      __stateError = __stateError + "\n" + message
    }
  }

  static writeStore_(name, value) {
    var file
    if (Cache.contains(name)) {
      file = Cache.list[name]
    } else {
      file = Cache.create(name)
    }
    if (file == null) return "Could not create %(name)."
    var result = file.open()
    if (result is FileError) return result.toString
    result = file.write(WON.serialize(value))
    file.close()
    if (result is FileError) return result.toString
    __stateError = null
    return null
  }

  static registrySnapshot_ {
    var rows = []
    for (game in games) {
      rows.add({
        "id": game["id"],
        "name": game["name"]
      })
    }
    return {
      "schema": 1,
      "activeGame": activeGameIndex,
      "nextGameId": root["nextGameId"],
      "games": rows
    }
  }

  static profileSnapshot_(game) {
    return {
      "schema": 1,
      "gameId": game["id"],
      "profile": game["profile"],
      "profileDetection": game["profileDetection"]
    }
  }

  static universeSnapshot_(game) {
    return {
      "schema": 1,
      "gameId": game["id"],
      "sectors": game["sectors"],
      "lastAnalysis": game["lastAnalysis"]
    }
  }

  static recordsSnapshot_(game) {
    return {
      "schema": 1,
      "gameId": game["id"],
      "planets": game["planets"],
      "portPairs": game["portPairs"],
      "locations": game["locations"],
      "benchmarks": game["benchmarks"]
    }
  }

  static saveRegistry_() {
    return writeStore_(registryFilename, registrySnapshot_)
  }

  static saveProfile_(game) {
    return writeStore_(profileFilename_(game["id"]), profileSnapshot_(game))
  }

  static saveUniverse_(game) {
    return writeStore_(universeFilename_(game["id"]), universeSnapshot_(game))
  }

  static saveRecords_(game) {
    return writeStore_(recordsFilename_(game["id"]), recordsSnapshot_(game))
  }

  static applyRegistry_(store) {
    if (!(store is Map) || store["schema"] != 1 ||
        !(store["games"] is List)) return
    var existing = {}
    for (game in __stateData["games"]) existing[game["id"]] = game
    var merged = []
    var used = {}
    for (row in store["games"]) {
      if (!(row is Map) || !validGameId_(row["id"]) ||
          used[row["id"]] == true) continue
      var id = row["id"]
      var game = existing[id]
      if (game == null) game = defaultGame("Game %(merged.count + 1)")
      game["id"] = id
      if (row["name"] is String && row["name"].trim().count > 0) {
        game["name"] = row["name"]
      }
      normalizeGame_(game, merged.count)
      merged.add(game)
      used[id] = true
    }
    if (merged.count == 0) return
    __stateData["games"] = merged
    var active = store["activeGame"]
    if (!(active is Num) || !active.isInteger || active < 0 ||
        active >= merged.count) active = 0
    __stateData["activeGame"] = active
    var next = store["nextGameId"]
    if (next is Num && next.isInteger && next >= 1) {
      __stateData["nextGameId"] = next
    }
  }

  static applyProfile_(game, store) {
    if (!(store is Map) || store["schema"] != 1 ||
        store["gameId"] != game["id"]) return
    if (store["profile"] is Map) game["profile"] = store["profile"]
    game["profileDetection"] = null
    if (store["profileDetection"] is Map) {
      game["profileDetection"] = store["profileDetection"]
    }
    normalizeGame_(game, 0)
  }

  static applyUniverse_(game, store) {
    if (!(store is Map) || store["schema"] != 1 ||
        store["gameId"] != game["id"]) return
    if (store["sectors"] is Map) game["sectors"] = store["sectors"]
    game["lastAnalysis"] = null
    if (store["lastAnalysis"] is Map) {
      game["lastAnalysis"] = store["lastAnalysis"]
    }
    normalizeGame_(game, 0)
  }

  static applyRecords_(game, store) {
    if (!(store is Map) || store["schema"] != 1 ||
        store["gameId"] != game["id"]) return
    if (store["planets"] is List) game["planets"] = store["planets"]
    if (store["portPairs"] is List) game["portPairs"] = store["portPairs"]
    if (store["locations"] is List) game["locations"] = store["locations"]
    if (store["benchmarks"] is Map) game["benchmarks"] = store["benchmarks"]
    normalizeGame_(game, 0)
  }

  static replace(value) {
    if (!(value is Map)) return "Imported value is not a map."
    if (value["schema"] != 2 || !(value["games"] is List)) {
      return "Imported value is not Yankee Trader schema 2 data."
    }
    __stateData = normalize_(value)
    return save()
  }

  static exportText { WON.serialize(root) }

  static selectGame(index) {
    if (!(index is Num) || !index.isInteger || index < 0 ||
        index >= games.count) return "Invalid game selection."
    root["activeGame"] = index
    return saveRegistry_()
  }

  static addGame(name, version) {
    var clean = name.trim()
    if (clean.count == 0) return "Game name cannot be blank."
    if (gameNameExists_(clean, -1)) return "A game named %(clean) already exists."
    var game = defaultGame(clean, version)
    game["id"] = nextGameId_
    games.add(game)
    root["activeGame"] = games.count - 1
    var error = saveProfile_(game)
    if (error != null) return error
    return saveRegistry_()
  }

  static renameActiveGame(name) {
    var clean = name.trim()
    if (clean.count == 0) return "Game name cannot be blank."
    if (gameNameExists_(clean, activeGameIndex)) {
      return "A game named %(clean) already exists."
    }
    data["name"] = clean
    return saveRegistry_()
  }

  static deleteActiveGame() {
    if (games.count <= 1) return "A BBS must retain at least one game profile."
    var index = activeGameIndex
    games.removeAt(index)
    root["activeGame"] = index.min(games.count - 1)
    return saveRegistry_()
  }

  static setVersionDefaults(version) {
    data["profile"] = defaultProfile(version)
    data["profileDetection"] = null
    return saveProfile_(data)
  }

  static setProfile(version, maxSector, macroRepeats) {
    data["profile"]["version"] = version
    data["profile"]["maxSector"] = maxSector
    data["profile"]["macroRepeats"] = macroRepeats
    data["profileDetection"] = null
    return saveProfile_(data)
  }

  static setProfileOption(name, value) {
    if (defaultProfile("modern")[name] == null &&
        defaultProfile("3.2")[name] == null &&
        defaultProfile("3.6G mod")[name] == null) return "Unknown game setting."
    data["profile"][name] = value
    data["profileDetection"] = null
    return saveProfile_(data)
  }

  static applyDetectedProfile(profile, detection) {
    if (!(profile is Map)) return "Detected game settings are not a map."
    var expected = defaultProfile("modern")
    for (key in expected.keys) {
      if (profile[key] == null) return "Detected setting %(key) is missing."
    }
    data["profile"] = profile
    data["profileDetection"] = detection is Map ? detection : null
    return saveProfile_(data)
  }

  static putSector(sector, kind, note, date) {
    var key = sector.toString
    var row = data["sectors"][key]
    if (row == null) row = {"sector": sector, "warps": [], "port": false}
    row["kind"] = kind
    row["note"] = note
    row["date"] = date
    data["sectors"][key] = row
    return saveUniverse_(data)
  }

  static mergeGraph(graph) {
    for (key in graph.keys) {
      var incoming = graph[key]
      var row = data["sectors"][key]
      if (row == null) row = {"sector": incoming["sector"]}
      row["warps"] = incoming["warps"]
      row["port"] = incoming["port"]
      data["sectors"][key] = row
    }
    return saveUniverse_(data)
  }

  static addPlanet(sector, name, fighters, creditsMillions, forces, date,
      note) {
    data["planets"].add({
      "sector": sector,
      "name": name,
      "fighters": fighters,
      "creditsMillions": creditsMillions,
      "groundForces": forces,
      "date": date,
      "note": note
    })
    return saveRecords_(data)
  }

  static addPortPair(first, second, profitPerHold, note) {
    data["portPairs"].add({
      "first": first,
      "second": second,
      "profitPerHold": profitPerHold,
      "note": note
    })
    return saveRecords_(data)
  }

  static addLocation(sector, kind, owner, date, note) {
    data["locations"].add({
      "sector": sector,
      "kind": kind,
      "owner": owner,
      "date": date,
      "note": note
    })
    return saveRecords_(data)
  }

  static setBenchmark(name, seconds) {
    data["benchmarks"][name] = seconds
    return saveRecords_(data)
  }

  static setAnalysis(result) {
    // Keep the actual path map as well as the derived lists. A live C;10 scan
    // is expensive and must be recoverable from the per-BBS state file.
    data["lastAnalysis"] = result
    return saveUniverse_(data)
  }

  static summary {
    var analysis = data["lastAnalysis"]
    var analyzed = analysis == null ? 0 : analysis["pathCount"]
    return {
      "sectors": data["sectors"].count,
      "planets": data["planets"].count,
      "portPairs": data["portPairs"].count,
      "locations": data["locations"].count,
      "analyzedPaths": analyzed
    }
  }

  static normalize_(value) {
    if (value["schema"] != 2 || !(value["games"] is List)) {
      return defaultData
    }

    if (value["games"].count == 0) value["games"].add(defaultGame("Game 1"))
    for (i in 0...value["games"].count) {
      var game = value["games"][i]
      if (!(game is Map)) {
        game = defaultGame("Game %(i + 1)")
        value["games"][i] = game
      }
      normalizeGame_(game, i)
    }
    assignGameIds_(value["games"])
    var active = value["activeGame"]
    if (!(active is Num) || !active.isInteger || active < 0 ||
        active >= value["games"].count) active = 0
    value["activeGame"] = active
    var next = value["nextGameId"]
    if (!(next is Num) || !next.isInteger || next < 1) next = 1
    while (gameIdExistsIn_(value["games"], "g%(next)")) next = next + 1
    value["nextGameId"] = next
    value["schema"] = 2
    return value
  }

  static normalizeGame_(game, index) {
    if (!(game["name"] is String) || game["name"].trim().count == 0) {
      game["name"] = "Game %(index + 1)"
    }
    if (!(game["profile"] is Map)) game["profile"] = defaultProfile("modern")
    var savedVersion = game["profile"]["version"]
    var version = savedVersion == "3.2" ? "3.2" :
        (savedVersion == "3.6G mod" ? "3.6G mod" : "modern")
    var profile = defaultProfile(version)
    game["profile"]["version"] = profile["version"]
    for (key in profile.keys) {
      if (game["profile"][key] == null) game["profile"][key] = profile[key]
    }
    game["profile"].remove("game")
    if (!(game["sectors"] is Map)) game["sectors"] = {}
    if (!(game["planets"] is List)) game["planets"] = []
    if (!(game["portPairs"] is List)) game["portPairs"] = []
    if (!(game["locations"] is List)) game["locations"] = []
    if (!(game["benchmarks"] is Map)) game["benchmarks"] = {}
    if (game["lastAnalysis"] != null && !(game["lastAnalysis"] is Map)) {
      game["lastAnalysis"] = null
    }
    if (game["profileDetection"] != null &&
        !(game["profileDetection"] is Map)) game["profileDetection"] = null
  }

  static validGameId_(id) {
    if (!(id is String) || id.bytes.count < 2 || id.bytes.count > 12 ||
        id.bytes[0] != 0x67) return false
    for (i in 1...id.bytes.count) {
      var b = id.bytes[i]
      if (b < 0x30 || b > 0x39) return false
    }
    return true
  }

  static assignGameIds_(list) {
    var used = {}
    var next = 1
    for (game in list) {
      var id = game["id"]
      if (!validGameId_(id) || used[id] == true) {
        id = "g%(next)"
        while (used[id] == true) {
          next = next + 1
          id = "g%(next)"
        }
        game["id"] = id
      }
      used[id] = true
      next = next + 1
    }
  }

  static nextGameId_ {
    var next = root["nextGameId"]
    if (!(next is Num) || !next.isInteger || next < 1) next = 1
    var id = "g%(next)"
    while (gameIdExistsIn_(games, id) ||
        Cache.contains(profileFilename_(id)) ||
        Cache.contains(universeFilename_(id)) ||
        Cache.contains(recordsFilename_(id))) {
      next = next + 1
      id = "g%(next)"
    }
    root["nextGameId"] = next + 1
    return id
  }

  static gameIdExistsIn_(list, id) {
    for (game in list) {
      if (game["id"] == id) return true
    }
    return false
  }

  static gameNameExists_(name, except) {
    for (i in 0...games.count) {
      if (i != except && games[i]["name"] == name) return true
    }
    return false
  }
}

YTState.initialize_()
