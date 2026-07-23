// Menu-VM tests for Yankee Trader's offline per-BBS state selection.

import "syncterm_menu" for Menu, MenuReadStatus
import "yankee_trader_offline" for YankeeTraderMenu
import "yankee_trader_state" for YTState

class MenuTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== Yankee Trader menu self-test starting ===")

    var loaded = Menu.load(null) == MenuReadStatus.ok
    check_(loaded, "menu BBS model loads")
    if (!loaded) return [__pass, __fail]

    var first = Menu.create("YT Offline Test")
    var second = Menu.create("YT Offline Other")
    check_(first != null && second != null, "personal test entries are created")
    if (first == null || second == null) return [__pass, __fail]
    for (entry in Menu.entries) {
      if (entry.name == "YT Offline Test") first = entry
      if (entry.name == "YT Offline Other") second = entry
    }

    first.wrenScripts = ["yankee_trader"]
    second.wrenScripts = []
    var firstCache = first.cache
    var secondCache = second.cache
    check_(firstCache != null && secondCache != null,
        "each personal BBS exposes an authorized cache Directory")
    check_(!YTState.hasStoredData(firstCache) &&
        !YTState.hasStoredData(secondCache),
        "empty BBS caches are not presented as saved games")
    var splitMarker = firstCache.create("yankee-trader-g1-universe.won")
    check_(splitMarker != null && YTState.hasStoredData(firstCache) &&
        !firstCache.contains(YTState.registryFilename),
        "single-game split files are recognized without a registry")
    firstCache.delete("yankee-trader-g1-universe.won")

    var selected = YankeeTraderMenu.eligibleEntries
    var foundFirst = false
    var foundSecond = false
    for (entry in selected) {
      if (entry.name == "YT Offline Test") foundFirst = true
      if (entry.name == "YT Offline Other") foundSecond = true
    }
    check_(foundFirst && !foundSecond,
        "offline selector includes only personal yankee_trader entries")

    var injected = YTState.useCache(firstCache) == null
    var saved = injected && YTState.save() == null &&
        firstCache.contains(YTState.registryFilename)
    check_(saved && YTState.hasStoredData(firstCache) &&
        !YTState.hasStoredData(secondCache),
        "saved game registry stays isolated in the selected BBS cache")

    var added = saved && YTState.addGame("Second Game", "3.6") == null &&
        YTState.activeGameIndex == 1
    var local = added && YTState.selectGameLocal(0) == null &&
        YTState.activeGameIndex == 0
    var reloaded = local && YTState.useCache(firstCache) == null &&
        YTState.activeGameIndex == 1
    check_(reloaded,
        "initial offline game choice does not rewrite connected preference")

    check_(Menu.timestamp > 1700000000,
        "menu supplies a current timestamp for offline date pickers")

    first.delete()
    for (entry in Menu.entries) {
      if (entry.name == "YT Offline Other") second = entry
    }
    second.delete()
    var total = __pass + __fail
    System.print("=== Yankee Trader menu: %(total) tests, %(__pass) pass, " +
        "%(__fail) fail ===")
    return [__pass, __fail]
  }

  static check_(ok, label) {
    if (ok) {
      __pass = __pass + 1
    } else {
      __fail = __fail + 1
      System.print("  FAIL %(label)")
    }
  }
}
