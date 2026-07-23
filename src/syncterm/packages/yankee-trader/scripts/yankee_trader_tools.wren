// Parsers, universe analysis, and command builders for Yankee Trader.

class YTText {
  static numbers(text) {
    var out = []
    var start = -1
    var bytes = text.bytes
    for (i in 0...bytes.count) {
      var digit = bytes[i] >= 48 && bytes[i] <= 57
      if (digit && start < 0) start = i
      if (!digit && start >= 0) {
        out.add(Num.fromString(text[start...i]))
        start = -1
      }
    }
    if (start >= 0) out.add(Num.fromString(text[start...bytes.count]))
    return out
  }

  static uniqueSorted(values) {
    var seen = {}
    for (value in values) seen[value.toString] = value
    var out = seen.values.toList
    out.sort {|a, b| a < b }
    return out
  }

  static join(values, separator) {
    var out = ""
    for (i in 0...values.count) {
      if (i > 0) out = out + separator
      out = out + values[i].toString
    }
    return out
  }

  static lines(text) {
    var normalized = text.replace("\r\n", "\n").replace("\r", "\n")
    return normalized.split("\n")
  }

  static lineContaining(text, marker) {
    for (line in lines(text)) {
      if (line.contains(marker)) return line.trim()
    }
    return null
  }

  static numberOnLine(text, marker) {
    var line = lineContaining(text, marker)
    if (line == null) return null
    var values = numbers(line.replace(",", ""))
    return values.count == 0 ? null : values[0]
  }

  static lastNumberOnLine(text, marker) {
    var line = lineContaining(text, marker)
    if (line == null) return null
    var values = numbers(line.replace(",", ""))
    return values.count == 0 ? null : values[-1]
  }
}

class YTUniverse {
  static nextBinarySectorProbe(lowValid, highInvalid) {
    if (highInvalid - lowValid <= 1) return null
    return ((lowValid + highInvalid) / 2).floor
  }

  // Convert the safe screen captures made by the live detector into settings.
  // maxSector is supplied by the detector's C;2 validity binary search rather
  // than trusted from an error message or inferred from the version banner.
  static detectGameSettings(infoText, versionText, helpText, computerText,
      maxSector) {
    var version = null
    if (versionText.contains("3.6g") || versionText.contains("3.6G") ||
        versionText.contains("YT * Mod")) {
      version = "3.6G mod"
    } else if (versionText.contains("Version 3.2")) {
      version = "3.2"
    } else if (versionText.contains("Version 3.6")) {
      version = "3.6"
    }

    var repeats = YTText.lastNumberOnLine(helpText,
        "any number between")
    if (repeats == null) {
      repeats = YTText.lastNumberOnLine(helpText,
          "repeat any command up to")
    }

    var missiles = helpText.contains("Launch a Cruise Missile")
    if (versionText.contains("Missiles disabled")) missiles = false

    return {
      "version": version,
      "banner": YTText.lineContaining(versionText, "Version"),
      "currentTurns": YTText.numberOnLine(infoText, "Turns"),
      "maxSector": maxSector,
      "macroRepeats": repeats,
      "missiles": missiles,
      "plasmaBolts": helpText.contains("Fire Plasma Bolt"),
      "sensorRobots": helpText.contains("Sensor Robot"),
      "spies": computerText.contains("Show Active Spies")
    }
  }

  static analyze10Scan(text, startSector, maxSector) {
    var paths = {}
    var sector = startSector
    for (raw in YTText.lines(text)) {
      var line = raw.trim()
      if (line.count > 0) {
        var path = YTText.numbers(line)
        if (path.count > 0) {
          paths[sector.toString] = path
          sector = sector + 1
        }
      }
    }

    var result = analyze10Paths(paths, maxSector, null)
    result["targetStart"] = startSector
    result["targetStop"] = sector - 1
    result["queryCount"] = result["pathCount"]
    result["complete"] = true
    return result
  }

  static analyze10Paths(paths, maxSector, source) {
    var frequency = {}
    var pathCount = 0
    for (key in paths.keys) {
      var path = paths[key]
      if (path.count > 0) {
        pathCount = pathCount + 1
        for (item in path) {
          var itemKey = item.toString
          var count = frequency[itemKey] == null ? 0 : frequency[itemKey]
          frequency[itemKey] = count + 1
        }
      }
    }

    var coverage = []
    for (key in frequency.keys) {
      var n = Num.fromString(key)
      if (frequency[key] == 1 && n >= 1 && n <= maxSector) coverage.add(n)
    }
    coverage.sort {|a, b| a < b }

    var deadEnds = []
    for (target in coverage) {
      var path = paths[target.toString]
      if (path != null && path.count >= 2) {
        deadEnds.add([path[-2], path[-1]])
      }
    }

    var linked = []
    var rifts = []
    var linkedSeen = {}
    var riftSeen = {}
    for (key in paths.keys) {
      var path = paths[key]
      var foundLink = false
      var foundRift = false
      var i = path.count - 1
      while (i >= 1 && (!foundLink || !foundRift)) {
        var destination = path[i]
        var destinationKey = destination.toString
        if (!foundLink && (path[i] - path[i - 1]).abs > 10 &&
            linkedSeen[destinationKey] == null) {
          linked.add([path[i - 1], destination,
              frequency[destinationKey] == null ? 0 : frequency[destinationKey]])
          linkedSeen[destinationKey] = true
          foundLink = true
        }
        var count = frequency[destinationKey]
        if (!foundRift && count != null && count > 19 && count < 120 &&
            riftSeen[destinationKey] == null) {
          rifts.add([path[i - 1], destination, count])
          riftSeen[destinationKey] = true
          foundRift = true
        }
        i = i - 1
      }
    }

    var directWarps = []
    if (source != null) {
      for (key in paths.keys) {
        var path = paths[key]
        if (path.count == 2 && path[0] == source) directWarps.add(path[1])
      }
      directWarps = YTText.uniqueSorted(directWarps)
    }

    return {
      "source": source,
      "pathCount": pathCount,
      "paths": paths,
      "coverage": coverage,
      "deadEnds": deadEnds,
      "linked": linked,
      "rifts": rifts,
      "directWarps": directWarps
    }
  }

  static pathTo(analysis, target) {
    if (!(analysis is Map) || !(analysis["paths"] is Map)) return null
    var path = analysis["paths"][target.toString]
    return path is List && path.count > 0 ? path : null
  }

  static routesContainingSector(analysis, sector) {
    return matchingRoutes_(analysis, Fn.new {|path|
      for (item in path) {
        if (item == sector) return true
      }
      return false
    })
  }

  static routesContainingEdge(analysis, from, to) {
    return matchingRoutes_(analysis, Fn.new {|path|
      for (i in 1...path.count) {
        if (path[i - 1] == from && path[i] == to) return true
      }
      return false
    })
  }

  static matchingRoutes_(analysis, matches) {
    var found = []
    if (!(analysis is Map) || !(analysis["paths"] is Map)) return found
    for (key in analysis["paths"].keys) {
      var target = Num.fromString(key)
      var path = analysis["paths"][key]
      if (target != null && path is List && path.count > 0 &&
          matches.call(path)) found.add([target, path])
    }
    found.sort {|a, b| a[0] < b[0] }
    return found
  }

  static parsePathFinder(text, source, target, maxSector) {
    var marker = " is:"
    var start = text.indexOf(marker)
    if (start < 0) return []
    start = start + marker.bytes.count
    // The route is complete before the turn summary. Never scan through the
    // following status line, Computer prompt, or next queued 10;source;target
    // command: all of those contain sector-looking numbers.
    var stop = text.indexOf("Course will take", start)
    if (stop < 0) stop = text.indexOf("Computer command", start)
    if (stop < 0) stop = text.indexOf("Enter start for path search", start)
    if (stop < 0) stop = text.bytes.count
    var values = YTText.numbers(text[start...stop])
    var route = []
    var seen = {}
    var started = false
    for (sector in values) {
      if (!started && sector == source) started = true
      if (started && sector >= 1 && sector <= maxSector) {
        if (seen[sector.toString] != null) return []
        if (route.count == 0 || route[-1] != sector) route.add(sector)
        seen[sector.toString] = true
        if (sector == target) return route
      }
    }
    return []
  }

  static parseSensorLog(text) {
    var graph = {}
    var current = null
    for (raw in YTText.lines(text)) {
      var line = raw.trim()
      if (line.contains("Sector:")) {
        var values = YTText.numbers(line)
        if (values.count > 0) {
          current = values[0]
          var key = current.toString
          if (graph[key] == null) {
            graph[key] = {"sector": current, "warps": [], "port": false}
          }
        }
      } else if (current != null && line.contains("Warps lead to:")) {
        graph[current.toString]["warps"] = YTText.uniqueSorted(
            YTText.numbers(line))
      } else if (current != null && line.contains("Port:")) {
        graph[current.toString]["port"] = true
      }
    }
    return graph
  }

  static deadEnds(graph) {
    var out = []
    for (key in graph.keys) {
      var row = graph[key]
      if (row["warps"].count == 1) {
        out.add([row["sector"], row["port"]])
      }
    }
    out.sort {|a, b| a[0] < b[0] }
    return out
  }

  static extractPortSectors(text) {
    var out = []
    for (raw in YTText.lines(text)) {
      var line = raw.trim()
      if (line.contains("Sector:")) {
        var values = YTText.numbers(line)
        if (values.count > 0) out.add(values[0])
      }
    }
    return YTText.uniqueSorted(out)
  }

  static parseRouteBlock(text, maxSector) {
    var route = []
    var lines = YTText.lines(text)
    for (line in lines) {
      if (line.contains(",")) route.addAll(YTText.numbers(line))
    }
    if (route.count == 0) {
      for (i in 2...(lines.count - 1).max(2)) {
        route.addAll(YTText.numbers(lines[i]))
      }
    }
    var clean = []
    for (sector in route) {
      if (sector >= 1 && sector <= maxSector &&
          (clean.count == 0 || clean[-1] != sector)) clean.add(sector)
    }
    return clean
  }
}

class YTCommands {
  static mainHelpCommand { "?\r" }

  static portScans(sectors, perBatch) {
    return repeatBatches_(sectors, perBatch, "c;2;", ";2;", ";1")
  }

  static planetScans(sectors, perBatch) {
    return repeatBatches_(sectors, perBatch, "c;9;", ";9;", ";1")
  }

  static missiles(sectors, perBatch) {
    return repeatBatches_(sectors, perBatch, "!;", ";1;!;", ";1")
  }

  static robots(sectors, perBatch) {
    return repeatBatches_(sectors, perBatch, "R;", ";R;", "")
  }

  static travelAndScan(sectors, perBatch) {
    return repeatBatches_(sectors, perBatch, "c;3;", ";y;s;c;3;", ";y;s")
  }

  static routeScans(sectors, perBatch) {
    return repeatBatches_(sectors, perBatch, "m;", ";s;m;", ";s")
  }

  static c10Command(sector) { c10Command(1, sector) }

  static c10Command(source, sector) { "10;%(source);%(sector)\r" }

  static c10Queue(startSector, endSector) {
    var out = []
    for (sector in startSector..endSector) out.add(c10Command(sector))
    return out
  }

  static avoid(sectors) { avoid(sectors, 30) }

  static avoid(sectors, slots) {
    var count = sectors.count.min(slots)
    var out = "c"
    for (i in 0...count) out = out + ";7;%(i + 1);%(sectors[i])"
    return out
  }

  static clearAvoid { clearAvoid(30) }

  static clearAvoid(slots) {
    var out = "c"
    for (slot in 1..slots) out = out + ";7;%(slot);"
    return out + ";1"
  }

  static surroundMercenaries(center, adjacent, fighters) {
    var out = avoid([center])
    for (sector in adjacent) {
      out = out + ";c;3;%(sector);y;f;%(fighters)"
    }
    return out
  }

  static scanRoute(route) {
    if (route.count == 0) return ""
    return routeScans(route, route.count)[0]
  }

  static defaultMacros(repeatCount) {
    var r = repeatCount.max(1)
    return [
      ["F1", "Player ranking", "C;4;;1", "C;4;;1"],
      ["F2", "Today's newspaper", "C;8;T;NS;1", "C;8;T;NS;1"],
      ["F3", "Yesterday's newspaper", "C;8;Y;NS;1", "C;8;Y;NS;1"],
      ["F4", "Scan and move", "S;M", "S;M"],
      ["F5", "Sell planet equipment to port",
          repeatCommand_("L;3;;P;;;0;L;3;;P;;;0", r),
          "L;3;;P;;;0;L;3;;P;;;0 (repeated %(r) times)"],
      ["F6", "Sell planet organics to port",
          repeatCommand_("L;2;;P;;;0;L;2;;P;;;0", r),
          "L;2;;P;;;0;L;2;;P;;;0 (repeated %(r) times)"],
      ["F7", "Sell planet ore to port",
          repeatCommand_("L;1;;P;;;0;L;1;;P;;;0", r),
          "L;1;;P;;;0;L;1;;P;;;0 (repeated %(r) times)"],
      ["F8", "Transfer port cargo to planet",
          repeatCommand_("L;T;C;P;;", r),
          "L;T;C;P;; (repeated %(r) times)"],
      ["F9", "Sell equipment and refill planet",
          repeatCommand_("P;;;;;L;T;C;3;", r),
          "P;;;;;L;T;C;3; (repeated %(r) times)"],
      ["F10", "Sell organics and refill planet",
          repeatCommand_("P;;;;;L;T;C;2;", r),
          "P;;;;;L;T;C;2; (repeated %(r) times)"],
      ["F11", "Sell ore and refill planet",
          repeatCommand_("P;;;;;L;T;C;1;", r),
          "P;;;;;L;T;C;1; (repeated %(r) times)"],
      ["F12", "Alternative breadcrumb mapping", "F;1;S;M", "F;1;S;M"]
    ]
  }

  static repeatCommand_(command, count) {
    var out = command
    var separator = command.endsWith(";") ? "" : ";"
    for (i in 1...count) out = out + separator + command
    return out
  }

  static repeatBatches_(sectors, perBatch, prefix, between, suffix) {
    var out = []
    if (sectors.count == 0) return out
    var size = perBatch.max(1)
    var index = 0
    while (index < sectors.count) {
      var stop = (index + size).min(sectors.count)
      var line = prefix + sectors[index].toString
      for (i in (index + 1)...stop) line = line + between + sectors[i].toString
      out.add(line + suffix)
      index = stop
    }
    return out
  }
}
