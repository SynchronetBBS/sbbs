// Connected Yankee Trader entry point for SyncTERM.
// Install for a BBS entry and press Alt-Y while connected.

import "syncterm" for Hook, Key
import "yankee_trader_ui" for YankeeTraderUi

var YankeeTrader = YankeeTraderUi

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
Hook.onMatchClean("Not enough turns left to autopilot this course",
    Fn.new {|match| YankeeTrader.onAutopilotTurnLimit_(match) })
Hook.onMatchClean("Sorry but you have no turns left",
    Fn.new {|match| YankeeTrader.onNoTurns_(match) })
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
