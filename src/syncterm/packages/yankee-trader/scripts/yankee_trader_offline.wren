// Offline Yankee Trader selector and planner controller for the trusted menu
// VM. The auto/menu entry point only registers its Alt-Y command.

import "syncterm_menu" for Menu
import "menu_ui" for MenuUi
import "ui_popup" for Alert
import "yankee_trader_state" for YTState
import "yankee_trader_ui" for YankeeTraderUi

class YankeeTraderMenu {
  static eligibleEntries {
    var entries = []
    for (bbs in Menu.entries) {
      if (bbs.type == 0 &&
          bbs.wrenScripts.indexOf("yankee_trader") >= 0) {
        entries.add(bbs)
      }
    }
    return entries
  }

  static chooseIndex_(app, title, message, labels, current) {
    var rows = []
    for (i in 0...labels.count) rows.add([i, labels[i]])
    return MenuUi.choice(app, title, rows, current,
        "# %(title)\n\n%(message)\n\n" +
        "Entries with only one possible choice skip this screen.")
  }

  static alert_(app, title, message) {
    Alert.show(app, title, message)
  }

  static run(app, connected) {
    var entries = eligibleEntries
    if (entries.count == 0) {
      alert_(app, "Yankee Trader Offline",
          "No personal BBS entry has the yankee_trader Wren script enabled.\n\n" +
          "Edit a Yankee Trader entry, open Wren Scripts, and add " +
          "yankee_trader before using the offline planner.")
      return
    }

    var entryIndex = 0
    if (entries.count > 1) {
      entryIndex = chooseIndex_(app, "Select Yankee Trader BBS",
          "Choose the personal directory entry whose saved maps, games, and " +
          "records you want to use.",
          entries.map {|bbs| bbs.name }.toList, 0)
      if (entryIndex == null) return
    }
    var bbs = entries[entryIndex]
    var cache = bbs.cache
    if (cache == null) {
      alert_(app, "Yankee Trader Offline",
          "The cache directory for %(bbs.name) could not be opened.")
      return
    }
    if (!YTState.hasStoredData(cache)) {
      alert_(app, "No Saved Yankee Trader Games",
          "%(bbs.name) has no saved Yankee Trader game data yet.\n\n" +
          "Connect once and open Alt-Y to create or save a game profile.")
      return
    }

    var error = YTState.useCache(cache)
    if (error != null || YTState.lastError != null) {
      var message = error == null ? YTState.lastError : error
      alert_(app, "Yankee Trader State Error",
          "Saved data for %(bbs.name) could not be loaded safely:\n\n%(message)")
      return
    }

    var gameIndex = YTState.activeGameIndex
    if (YTState.games.count > 1) {
      gameIndex = chooseIndex_(app, "Select Yankee Trader Game",
          "Choose the game profile and its independent maps, records, " +
          "settings, and benchmarks.", YTState.gameNames, gameIndex)
      if (gameIndex == null) return
    }
    error = YTState.selectGameLocal(gameIndex)
    if (error != null) {
      alert_(app, "Yankee Trader Offline", error)
      return
    }

    YankeeTraderUi.runOffline(bbs.name, Menu.timestamp)
  }
}
