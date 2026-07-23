// Trusted main-menu entry point for offline Yankee Trader planning.

import "syncterm" for Key
import "main_menu" for MainMenu
import "yankee_trader_offline" for YankeeTraderMenu

MainMenu.registerCommand(Key.altY, "Alt-Y",
    "Open the Yankee Trader offline maps, planners, calculators, and records",
    Fn.new {|app, connected| YankeeTraderMenu.run(app, connected) })
