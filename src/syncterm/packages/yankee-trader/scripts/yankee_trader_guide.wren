// Detailed in-app adaptation of the Yankee Trader Strategy Guide by Zharous.
// Source material is mirrored locally from https://yt.starflt.com/.

class YTGuide {
  static topics(gameName, p) {
    return [
      ["Current game and guide scope", currentGame_(gameName, p)],
      ["First day and Earth", firstDay_],
      ["Early- and late-game plan", progression_],
      ["Port-pair trading", portPairs_],
      ["Planet-to-port income", planetTrading_],
      ["Exploration and newspapers", exploration_],
      ["Recordkeeping and intelligence", recordkeeping_],
      ["Planet growth and investment", planetGrowth_],
      ["Planet placement and risk", planetPlacement_],
      ["Moving and capturing planets", planetOperations_],
      ["Weapon damage reference", weaponDamage_],
      ["Attacking players", playerCombat_],
      ["Cloak, secrecy, and exposure", secrecy_],
      ["Online-combat quirks and recovery", multiplayerQuirks_],
      ["Xannor behavior and planet defense", xannorDefense_],
      ["Farming Xannor for turns", xannorTurns_],
      ["Xannoron and the Xannor HQ", xannoron_],
      ["Mercenaries", mercenaries_],
      ["Capturing the Mercenary Base", mercenaryBase_],
      ["Dead-end and C;10 mapping", mapping_],
      ["Port and planet scanning", reportScanning_],
      ["Missile scouting", missileScouting_],
      ["Robots, Danger Scanner, and spies", scoutingHardware_],
      ["Estimating a planet's bank", planetBank_],
      ["Backtracing the Xannor HQ", xannorBacktrace_],
      ["Advanced scouting and deception", advanced_],
      ["Special planets", specialPlanets_],
      ["Yankee Trader 3.2", version32_],
      ["Known 3.6 balance bugs", balanceBugs_]
    ]
  }

  static currentGame_(gameName, p) {
    return "# Current Game and Guide Scope\n\n" +
      "**Active game:** %(gameName)\n\n" +
      "- Universe: sectors 1 through %(p["maxSector"])\n" +
      "- Turns per day: %(p["turnsPerDay"])\n" +
      "- Planet limit: %(p["maxPlanets"])\n" +
      "- Port limit: %(p["maxPorts"])\n" +
      "- Computer avoid slots: %(p["sectorAvoidSlots"])\n" +
      "- Command repeat limit: %(p["macroRepeats"])\n" +
      "- Missiles: %(yesNo_(p["missiles"]))\n" +
      "- Plasma bolts: %(yesNo_(p["plasmaBolts"]))\n" +
      "- Xannor: %(yesNo_(p["xannor"]))\n" +
      "- Mercenaries: %(yesNo_(p["mercenaries"]))\n" +
      "- Sensor robots: %(yesNo_(p["sensorRobots"]))\n" +
      "- Danger Scanner: %(yesNo_(p["dangerScanner"]))\n" +
      "- Spies: %(yesNo_(p["spies"]))\n\n" +
      "The remaining topics preserve the concrete tactics, formulas, " +
      "command strings, and version-specific warnings from Zharous's Yankee " +
      "Trader Strategy Guide. Most were written for stock 3.6. A BBS may run " +
      "a patched executable or old data files, so compare advice involving " +
      "missiles, plasma, robots, spies, repeats, or sector limits with the " +
      "active profile above.\n\n" +
      "Numbers in the guide are planning values, not guarantees. Combat, " +
      "mines, plagues, civil wars, port recovery, and planet movement include " +
      "random outcomes. Use the calculators for a specific target and keep a " +
      "safety margin when the target is valuable."
  }

  static firstDay_ { "# First Day and Earth\n\n" +
      "All players begin at Earth in sector 1. Earth sells cargo holds, " +
      "fighters, ground forces, shields, cloaking energy, and special ship " +
      "equipment. Its ordinary prices are poor, but clearance sales can be " +
      "deep enough to change the order in which you build your ship.\n\n" +
      "Play Earth's lottery all five allowed times on early visits. Besides " +
      "the chance of credits, each play is another chance to trigger a sale. " +
      "Normal reference prices are about 100 credits per fighter and 750 per " +
      "ground force, so save major purchases for a strong discount when " +
      "possible.\n\n" +
      "A new player's priorities are:\n\n" +
      "- Reach **1,000 cargo holds**. Holds multiply every profitable trade, " +
      "so this is the foundation for everything else.\n" +
      "- In 3.6, buy a **Danger Scanner** soon afterward. Autopilot can then " +
      "stop before black holes, mines, or hostile fighters instead of " +
      "delivering the ship into them.\n" +
      "- Add shields as good sales appear. Missile damage can leak past " +
      "fighters and kill a poorly shielded ship while fighters remain. The " +
      "original guide treats roughly 10,000 to 100,000 shields as a useful " +
      "long-term range for an established ship.\n" +
      "- Find a nearby profitable port pair, then find a distant dead end for " +
      "the first planet and an overnight hiding place.\n\n" +
      "At the end of a day, either restore cloaking energy near 100 percent or " +
      "quit over or behind a planet with enough ground forces to absorb Xannor " +
      "attacks. New players should not assume a large fighter stack alone " +
      "makes an exposed location safe." }

  static progression_ { "# Early- and Late-game Plan\n\n" +
      "The guide's opening game is an information-building phase. A useful " +
      "early checklist is:\n\n" +
      "- Map dead ends, keeping separate lists for sectors with and without " +
      "ports.\n" +
      "- Record profitable C;16 port pairs, their current inventories, and " +
      "how many full loads the smaller inventory supports.\n" +
      "- Build repeatable C;9 planet checks and C;2 port checks for known " +
      "dead ends and owned planets.\n" +
      "- Establish several planets rather than concentrating the whole economy " +
      "in one place. Bring productive planets to at least 100,000 combined " +
      "production when plasma bolts are available.\n" +
      "- Keep a list of empty, never-used dead ends for overnight hiding. " +
      "Locations one or two sectors beyond an ordinary travel route are more " +
      "useful than famous or frequently scanned locations.\n" +
      "- Watch Earth for exceptional fighter, shield, and ground-force sales.\n\n" +
      "A mature daily loop is more specific than 'earn money and get stronger':\n\n" +
      "1. Scout from a location away from valuable planets. Find one or more " +
      "Xannor groups and put sectors you want to preserve into C;7 avoids.\n" +
      "2. Visit a productive planet and spend most turns selling its cargo to " +
      "the local port. Move to another planet when either side of the market " +
      "is exhausted.\n" +
      "3. Before turns run out, travel to a recorded Xannor group. If it is " +
      "large, use adjacent-sector plasma to reduce it toward 100,000, then " +
      "finish with fighters to refill turns.\n" +
      "4. Spend the restored turns on another planet or another mapping pass. " +
      "Repeat while useful Xannor groups and productive planets remain.\n\n" +
      "Late-game priorities include locating and exploiting the Mercenary Base " +
      "and Xannoron, keeping hostile planets in the avoid list during missile " +
      "work, banking credits only on well-defended hidden planets, and placing " +
      "small retaliatory caches of plasma bolts on multiple planets so one " +
      "ship loss does not remove every offensive option." }

  static portPairs_ { "# Port-pair Trading\n\n" +
      "Use the Computer's **C;16** report to find two nearby ports where one " +
      "sells what the other buys. The port colors identify port types; the " +
      "decision-making numbers are the per-hold profit, route distance, and " +
      "available inventory.\n\n" +
      "With 1,000 holds and a displayed spread of 104 credits per hold, one " +
      "round trip earns about 104,000 credits and consumes four turns: one move " +
      "and one dock at each end. The panel's Port-pair earnings calculator uses " +
      "this same `holds x spread x trips` model.\n\n" +
      "Inspect each port with **C;2** before committing the day. The commodity " +
      "with the largest inventory is normally the commodity the port sells. A " +
      "port can change what it sells as inventories change. The smallest useful " +
      "inventory across both sides is the bottleneck: a 1,913,164-unit " +
      "bottleneck supports about 1,913 complete 1,000-hold loads. Profit falls " +
      "as inventories approach zero and prices drift toward the stock values of " +
      "20 for ore, 30 for organics, and 40 for equipment.\n\n" +
      "To travel to sector 115, use `c;3;115`. If the Danger Scanner stops at a " +
      "hazard in sector 70, either deal with it or place 70 in an avoid slot " +
      "with `c;7;1;70`, then ask for the route again.\n\n" +
      "A 3.6 trade loop between sectors 115 and 117 can be entered as " +
      "`m;117;p;;;;;m;115;p;;;;/r20` after the first purchase. Stock 3.6 has a " +
      "repeat bug: values above 10 behave as 20. Ctrl-R replays the saved " +
      "command and holding Ctrl-X asks the door to break out of the repeat as " +
      "soon as it safely can." }

  static planetTrading_ { "# Planet-to-port Income\n\n" +
      "Selling a planet's production to a port in the same sector is the best " +
      "routine use of turns. A port-pair round trip spends four turns moving " +
      "and docking twice; a planet-to-port cycle spends one turn, requires no " +
      "purchase at the far end, and converts production the planet created " +
      "while you were offline into credits.\n\n" +
      "The original command patterns are:\n\n" +
      "- `L;3;;P;;;0/R20` - sell equipment, then return what the port sells to " +
      "the planet.\n" +
      "- `L;2;;P;;;0/R20` - the same cycle for organics.\n" +
      "- `L;1;;P;;;0/R20` - the same cycle for ore.\n\n" +
      "Equipment is normally the most valuable commodity, so a common plan is " +
      "to build equipment production, place or move the planet to a port buying " +
      "equipment at a high price, and cycle until the planet or port is " +
      "temporarily exhausted. Then move to another productive planet and give " +
      "the first port time to recover.\n\n" +
      "Port buy and sell inventories grow about 10 percent per day. Exhausting " +
      "the port in the same hidden dead end as a valuable planet wastes that " +
      "future income source. Once total port inventory reaches roughly ten " +
      "million units, a port plague can sharply reduce its inventory and may " +
      "change the commodity it sells.\n\n" +
      "Owning a port is not automatically profitable. A port owner benefits " +
      "when other players buy there. Purchasing from another player's port can " +
      "also tell that player the sector was active when they collect port " +
      "credits; selling cargo to it does not produce the same signal." }

  static exploration_ { "# Exploration and Newspapers\n\n" +
      "Dead ends are valuable because a planet, ship, or special location at the " +
      "end of a branch is less likely to be crossed by ordinary traffic or a " +
      "random missile path. They still have to be discovered. Start by routing " +
      "to a high-numbered sector away from Earth, then follow one numeric " +
      "direction through the graph: consistently choose the higher outgoing " +
      "sector, or consistently choose the lower, while scanning every stop. In " +
      "a 3,000-sector universe, the original guide found many useful branches " +
      "in roughly the 2,800-3,000 range.\n\n" +
      "Record both port and no-port dead ends. A dead-end port buying equipment " +
      "is an attractive place to work a planet. An empty no-port dead end can " +
      "be a better overnight location because port reports cannot probe it and " +
      "there is no economic reason for another player to visit.\n\n" +
      "Read both today's and the previous newspaper. Record named planets, " +
      "fighters, mine hits, Xannor and mercenary activity, and every sector " +
      "associated with another player. One newspaper event may be falsified, " +
      "so treat a single item as a lead rather than a fact. A mercenary report " +
      "about 'sector mines,' for example, may actually point toward the Xannor " +
      "HQ.\n\n" +
      "If exploration lands on overwhelming fighters, Xannor, mercenaries, or a " +
      "black hole, use emergency warp with **W**. It spends many turns and can " +
      "overheat the engines, potentially consuming the rest of the day's turns, " +
      "but it is preferable to losing a developed ship. In 3.6, a Danger " +
      "Scanner makes autopilot exploration much safer; in 3.2, scout routes with " +
      "sensor robots because adjacent scans do not expose black holes." }

  static recordkeeping_ { "# Recordkeeping and Intelligence\n\n" +
      "Useful notes answer operational questions: Which planets can earn money " +
      "today? Which need fighters or ground forces? Which locations were exposed " +
      "recently? Which empty dead ends remain safe? A bare sector number cannot " +
      "answer any of those.\n\n" +
      "For each planet record its sector, name, port status, defensive fighters, " +
      "banked credits, ground forces, important cargo or weapons, last-visited " +
      "date, and the date it was revealed. Mark planets in multi-sector branches " +
      "so a route can visit nearby planets together. Keep Xannoron, the " +
      "Mercenary Base, black holes, hostile fighter groups, and other players' " +
      "planets in the separate location records.\n\n" +
      "The original notation used an asterisk for a sector with a port, a " +
      "different marker for a planet outside a safe dead end, indentation for " +
      "multiple planets in one branch, and entries such as `1 75$ 150 6/26` for " +
      "1,000 defensive fighters, 75 million credits, 150,000 ground forces, and " +
      "the last visit date. Its important feature was not the punctuation; it " +
      "was consistently retaining the quantities and dates.\n\n" +
      "At the beginning of a day, compare the saved planet and fighter records " +
      "with **C;11** and **C;13**. After a death, also run C;9 against the former " +
      "planet sectors because another player can claim them without a useful " +
      "newspaper notice. Check a planet's port with C;2, or a no-port planet with " +
      "C;9, before visiting. Hostile fighters turn a port report into `No " +
      "information available`, which warns that mercenaries or another force " +
      "may be waiting over the planet." }

  static planetGrowth_ { "# Planet Growth and Investment\n\n" +
      "Planets convert cargo, purchased productivity, and banked credits into " +
      "daily resources. These are three different investment choices.\n\n" +
      "**Cargo:** Leaving cargo on a planet makes next-day production of that " +
      "commodity about one tenth of the stored quantity. Untouched cargo and " +
      "production therefore compound at roughly 10 percent per day. Equipment " +
      "is usually the best commodity to seed because it has the highest normal " +
      "sale value.\n\n" +
      "**Purchased productivity:** Spending credits with the planet's `$` " +
      "command uses no turns. About 8,333,250 credits brings a new planet to the " +
      "important 100,000-total-productivity breakpoint that produces one plasma " +
      "bolt per day. For an existing planet, use Productivity investment rather " +
      "than guessing the amount needed for the next breakpoint.\n\n" +
      "**Banked credits:** Credits remain stealable but produce daily credits, " +
      "fighters, missiles, mines, ground forces, and plasma. Every 25 million " +
      "banked credits produces roughly one plasma bolt per day. This is why the " +
      "original notes often banked credits in 25-million increments.\n\n" +
      "Useful production reference points from the guide are:\n\n" +
      "- Each 60,000 production points yields about 20,000 of each commodity, " +
      "60,000 fighters, 24 missiles, 2 mines, and 0.6 plasma bolts per day.\n" +
      "- Each 5 million banked credits yields about 1,000 ore, 250 organics, " +
      "167 equipment, 10,000 fighters, 50 missiles, 20 mines, 50,000 credits, " +
      "500 ground forces, and 0.2 plasma bolts per day.\n\n" +
      "The Planet daily production calculator uses the recovered spreadsheet " +
      "formula and is preferable when cargo, credits, purchased production, and " +
      "existing ground forces all contribute at once." }

  static planetPlacement_ { "# Planet Placement and Long-term Risk\n\n" +
      "Secrecy is a planet's strongest defense. Put valuable banks at the far " +
      "end of a dead-end chain, not merely somewhere inside it. A planet with " +
      "another sector beyond it has more chances to intercept random missiles, " +
      "and several planets in one branch are all compromised when any one is " +
      "found.\n\n" +
      "Spread the economy across multiple planets. That limits the damage from " +
      "theft, destruction, or one exposed branch and gives the empire more room " +
      "to grow before random limits intervene. Keep modest weapon caches on " +
      "several planets so a killed ship can retaliate after rebuilding.\n\n" +
      "Planet productivity and ground forces have important risk thresholds:\n\n" +
      "- Plagues have been observed from about 169,000 productivity through " +
      "well above 1.5 million. They independently reduce productivity and ground " +
      "forces by a random 1 to 99 percent.\n" +
      "- Above one million ground forces, a planet has a daily chance of civil " +
      "war. Above two million, the guide reports a civil war every day. Civil " +
      "war can also reduce the bank by a random 1 to 99 percent.\n" +
      "- Xannor defense should be sized from current Xannor score. Divide that " +
      "score by 50,000 for the guide's conservative ground-force target.\n\n" +
      "Defensive fighters hide a planet from C;9 and stop mercenaries from " +
      "taking its fighters. They also create exposure: mercenaries joining the " +
      "defense can announce the sector, and a port in the same sector can reveal " +
      "the hostile fighters through C;2 scanning. Replace small guards that " +
      "Xannor destroys; an abandoned group below roughly 100 can itself defect " +
      "to the mercenaries." }

  static planetOperations_ { "# Moving and Capturing Planets\n\n" +
      "Move a planet when its local port can no longer buy enough of its cargo, " +
      "or when the location has been exposed. Before moving it, remove banked " +
      "credits and ground forces and park ship fighters in sector defense. A " +
      "move has a small explosion chance, and an explosion can remove roughly " +
      "10 to 50 percent of the ship's fighters. Moving into an occupied planet " +
      "also destroys the moving planet, so visit or scan the destination first.\n\n" +
      "Use **C;14** to find nearby ports and **C;2;sector** to verify the buyer, " +
      "price, and capacity before taking the movement risk. Moving only as far " +
      "as necessary limits the cost in older versions. After moving a planet, " +
      "move the ship as well: some 3.6 missile logic continues to treat the ship " +
      "as being at the old location until the ship performs an actual move.\n\n" +
      "To capture a hostile planet, land and attack with ground forces. Combat " +
      "gives the defender an initial attack, then the attacker two attacks, then " +
      "the defender four, with the later cycle repeating. Each attack can remove " +
      "roughly one half to one times the attacking force, so a large numerical " +
      "advantage is much safer than an even match.\n\n" +
      "Plasma can reduce ground forces before landing, but it damages " +
      "productivity at the same time. Fire one bolt to reveal the actual " +
      "productivity, use Plasma damage and safe shot with the appropriate " +
      "one-, two-, or three-commodity multiplier, and keep at least one bolt of " +
      "damage below the destruction point. Missiles can then remove additional " +
      "ground forces without plasma's simultaneous productivity hit." }

  static weaponDamage_ { "# Weapon Damage Reference\n\n" +
      "**Plasma bolts** have quadratic damage. At full strength, `n` bolts " +
      "remove about `50,000 x n x n` fighters, `25,000 x n x n` shields, " +
      "`250 x n x n` mines, or `1,000 x n x n` ground forces. A planet also " +
      "loses productivity in every active commodity: the total is about 1,000, " +
      "2,000, or 3,000 times `n x n` when one, two, or three commodity fields " +
      "are productive. Productivity reaching zero destroys the planet even if " +
      "ground forces remain.\n\n" +
      "Plasma is full strength at a target one sector away, then loses two " +
      "percentage points per additional sector. The multiplier is " +
      "`100 - 2 x (distance - 1)` percent and reaches zero at 51 sectors. " +
      "Plasma first hits fighters, then a player, then dropped mines, then the " +
      "planet. It can reveal and hit a cloaked player and can destroy friendly " +
      "planets.\n\n" +
      "**Cruise missiles** average roughly 2,450-2,550 fighter damage, 1,150 " +
      "shield damage, one mine, 12.5 ground forces, and 1,000-3,000 planet " +
      "productivity per missile in the recovered calculator. A small part of " +
      "missile damage can bypass fighters and strike shields. Missiles can pass " +
      "a small fighter group if some remain, but a planet blocks the route.\n\n" +
      "**Mines** detonate about 10 percent of the remaining field at each hit " +
      "and have extremely variable results. They attack fighters, then shields, " +
      "then holds; emergency warp may still save the target. Treat mine-kill " +
      "estimates as risk ranges, not arithmetic.\n\n" +
      "For a player in an adjacent sector, the plasma estimate is " +
      "`ceil(sqrt((fighters + 2 x shields) / 50,000))`. The calculators also " +
      "handle staged layers, distance loss, missiles, and safe planet shots." }

  static playerCombat_ { "# Attacking Players\n\n" +
      "Choose weapons based on retaliation and target state, not only nominal " +
      "damage.\n\n" +
      "- Fighter attacks deplete defending fighters and then shields. If you " +
      "win, dropped mines can detonate immediately in your sector.\n" +
      "- Cruise missiles can provoke an automatic return salvo, often a large " +
      "fraction of the target's missiles. They may miss a cloaked player unless " +
      "that player was revealed during the session.\n" +
      "- Plasma has no missile retaliation and ignores cloak, but must be fired " +
      "from nearby for useful damage.\n\n" +
      "A practical staged attack is fighters first, plasma for the remaining " +
      "combined fighter-and-shield strength, and missiles when their direct or " +
      "planet damage is useful. If you merely suspect a cloaked ship is in a " +
      "sector, one plasma bolt can reveal it without first buying the global " +
      "anti-cloak.\n\n" +
      "Defensive fighters may surrender instead of dying. Below a one-to-one " +
      "attacker ratio, expect none. Around two attackers per defender, the " +
      "guide's rule of thumb is about 20 percent surrender; around five to one, " +
      "about 50 percent; above ten to one, all defenders surrender. A surrender " +
      "adds fighters but reveals the location in the newspaper.\n\n" +
      "Killing a player directly lets the victor loot ship credits, missiles, " +
      "plasma bolts, ground forces, and owned ports. It also makes every planet " +
      "temporarily claimable by landing. The victim can return with a fresh " +
      "ship and restored turns, so the useful window is short: secure known " +
      "planets and remove exposed resources before assuming the fight is over." }

  static secrecy_ { "# Cloak, Secrecy, and Exposure\n\n" +
      "The guide's central defensive claim is that hidden resources survive " +
      "better than fortified, well-known resources. Keep valuable planets in " +
      "separate dead ends, do not camp over them during an active war, and move " +
      "or empty a planet soon after its location appears in the newspaper.\n\n" +
      "Record the **date** of every reveal. A location exposed today should be " +
      "treated as actively threatened; an old location may eventually become " +
      "usable again if opponents' notes are poor, but it never becomes " +
      "mathematically safe.\n\n" +
      "Events that can publish or otherwise expose a sector include missile and " +
      "plasma activity, surrendering fighters, mine hits, retaliation missiles, " +
      "and mercenaries joining defensive fighters. Bribing mercenaries and " +
      "fighting defensive fighters all the way to destruction do not produce " +
      "the same newspaper clue.\n\n" +
      "A player with any positive cloak who has not been revealed in the current " +
      "session is normally safe from direct missile targeting. Earth anti-cloak, " +
      "a plasma hit, or repeated local scans can change that. Full cloak also " +
      "improves fighter combat toward a two-to-one exchange against hostile " +
      "fighters; at zero cloak it is closer to one-to-one.\n\n" +
      "Camping over a planet protects against some random Xannor missile fire " +
      "but advertises the planet when attacks land there. During active " +
      "hostilities, a fully cloaked ship in an unrelated, unused no-port dead " +
      "end is often the better compromise." }

  static multiplayerQuirks_ { "# Online-combat Quirks and Recovery\n\n" +
      "Stock 3.6 multiplayer has a dangerous stale-location bug. Other players' " +
      "views of a ship location are not continuously refreshed. If an opponent " +
      "leaves and rejoins after seeing you in a newspaper event, the door may " +
      "continue to resolve attacks at that old sector even after you move. " +
      "Treat a fresh public location as dangerous for the remainder of the " +
      "session, not merely until the next move.\n\n" +
      "A player killed while still online is moved to an isolated sector 1 and " +
      "temporarily retains the old ship's equipment until leaving the game. The " +
      "guide's recovery sequence is to emergency-warp out, land on or create a " +
      "planet, unload valuable credits and weapons, then leave and return for " +
      "the replacement ship and restored daily turns. Recover the cache after " +
      "obtaining a Danger Scanner.\n\n" +
      "During the interval between death and return, another player can claim " +
      "the victim's planets just by landing; unrest removes much of their ground " +
      "force defense. If a teammate dies, visit each team planet and change its " +
      "ground-force allocation immediately so opponents do not take it for free.\n\n" +
      "A mine 'kill' is delayed until the victim actually logs in and encounters " +
      "the field. The mine layer receives no direct ship loot, and the planets " +
      "remain unavailable until the death is processed. This is one reason mines " +
      "are poor substitutes for a deliberate kill in 3.6." }

  static xannorDefense_ { "# Xannor Behavior and Planet Defense\n\n" +
      "There are twenty Xannor groups: one guards Xannoron and nineteen roam. " +
      "The final roaming group hunts the top player each night and can reveal " +
      "the named planet where that player is hiding. A top player who insists on " +
      "camping over planets can make planet names less informative, but an " +
      "unrelated hidden sector is safer.\n\n" +
      "The scoreboard value is about 100 points per Xannor fighter. Thus a " +
      "500,000,000 Xannor score represents roughly 5,000,000 fighters. For a " +
      "conservative worst-case planet defense, divide the Xannor score by 50,000 " +
      "and keep at least that many ground forces on the planet. The panel's " +
      "Xannor calculator uses the same rule.\n\n" +
      "Xannor regenerate each day by up to 20 percent of the top player's score, " +
      "without exceeding that player's score under ordinary conditions. Banked " +
      "credits help a planet replace ground forces: the guide observes that " +
      "banking about one tenth of the top-player score on each planet can keep " +
      "daily ground-force production level with a worst-case Xannor loss over " +
      "time, though plague and civil-war randomness still matter.\n\n" +
      "Use **C;7** avoids for known Xannor, friendly players, and other sectors " +
      "that missiles or autopilot should not enter. A friendly or team ship can " +
      "return fire just as readily as an enemy if a scouting missile hits it.\n\n" +
      "Emergency warp is the exit from an unexpectedly large group. Full cloak " +
      "both improves the fighter exchange rate and reduces how many roaming " +
      "groups find the ship during the nightly sweep." }

  static xannorTurns_ { "# Farming Xannor for Turns\n\n" +
      "Only Xannor destroyed by **fighters** award turns. The reward is five " +
      "turns per 1,000 Xannor. Missiles and plasma are preparation tools; their " +
      "kills do not count. In stock 3.6, turns cannot rise above the daily " +
      "starting amount, so timing is essential.\n\n" +
      "In a 500-turn game, 100,000 fighter kills refill the entire day. If 200 " +
      "turns remain, only 300 can be restored. Before traveling to the group, " +
      "ask C;3 for the route length without accepting it. Spend turns trading " +
      "until only the travel cost remains, then make the trip and take the " +
      "fighter kill near zero turns.\n\n" +
      "For a group larger than the desired final 100,000, fire plasma from an " +
      "adjacent sector. The guide's thresholds for leaving about 100,000 are:\n\n" +
      "- 150,000 or more: 1 bolt\n" +
      "- 300,000 or more: 2 bolts\n" +
      "- 550,000 or more: 3 bolts\n" +
      "- 900,000 or more: 4 bolts\n" +
      "- 1,350,000 or more: 5 bolts\n" +
      "- 1,900,000 or more: 6 bolts\n" +
      "- 2,550,000 or more: 7 bolts\n" +
      "- 3,300,000 or more: 8 bolts\n" +
      "- 4,150,000 or more: 9 bolts\n" +
      "- 5,100,000 or more: 10 bolts\n\n" +
      "The compact estimate is `floor(sqrt(Xannor / 50,000 - 2))`. Recheck the " +
      "reported group after firing; distance and intervening targets can change " +
      "the result. Destroying a group with fighters also gives another chance " +
      "for an Earth clearance sale." }

  static xannoron_ { "# Xannoron and the Xannor HQ\n\n" +
      "The Xannor HQ is both a major target and the source of random and " +
      "retaliatory Xannor missiles. Destroying it stops those missiles until the " +
      "next rebuild. Attacking it also makes the guilty player the focus of the " +
      "nineteen roaming groups that night, so arrange the overnight defense " +
      "before the attack.\n\n" +
      "After a conventional capture, do not expect ordinary ownership. Ground " +
      "forces left on Xannoron become Xannor property at maintenance. If no " +
      "forces remain, the planet is relocated and rebuilt with a small ground " +
      "defense. In the ordinary strategy, loot it, then hide over the Mercenary " +
      "Base or another planet with enough ground forces to absorb the mass " +
      "retaliation.\n\n" +
      "A more advanced option is to leave a very large defensive fighter group " +
      "in the Xannoron sector. At maintenance, all surviving Xannor plus roughly " +
      "`top player score / 500` new fighters attempt to retake it. The Xannor " +
      "planning calculator estimates this combined force.\n\n" +
      "Holding Xannoron can remove roaming Xannor and random missiles from the " +
      "rest of the universe, deny opponents fighter-kill turn refills, and yield " +
      "the planet's daily million-credit income. In a solo game, temporarily " +
      "moving most other score-bearing resources onto planets reduces the new " +
      "retake force. To break another player's hold, raise the top score or " +
      "weaken the defending fighters immediately before maintenance." }

  static mercenaries_ { "# Mercenaries\n\n" +
      "Purchases at unowned ports fund batches of roaming mercenaries for the " +
      "next day. They also form when a player dies and all sector-defense " +
      "fighters change allegiance, or when a small abandoned defense group " +
      "defects. Groups under 100 can defect; 50 or fewer is especially likely.\n\n" +
      "A mercenary bribe costs only **2.5 credits per fighter**, far below " +
      "Earth's ordinary fighter price. If the group refuses because you fought " +
      "mercenaries earlier in the current session, leave and re-enter before " +
      "trying the bribe.\n\n" +
      "Mercenaries passing an unguarded planet bypass its ground forces and take " +
      "the stored fighters. If defensive fighters are present, the mercenaries " +
      "join them instead, but the newspaper can announce the sector. A solo " +
      "player can guard every planet with 100 or more fighters and replace the " +
      "guards Xannor removes; a competitive player has to balance that protection " +
      "against revealing the location.\n\n" +
      "To absorb a huge unaffordable group, surround it shortly before " +
      "maintenance. First put the mercenary sector in a C;7 avoid slot so " +
      "autopilot never enters it. Visit every adjacent sector and leave at least " +
      "100 defensive fighters. If the group moves into one of them overnight, " +
      "it joins for free. For mercenaries in 2390 with exits through 2394, 2383, " +
      "and 2392, the guide's pattern is `c;7;1;2390`, followed by visits and " +
      "`f;100` drops in all three neighbors. Do this late so random Xannor fire " +
      "has less opportunity to punch a hole in the surround." }

  static mercenaryBase_ { "# Capturing the Mercenary Base\n\n" +
      "The Mercenary Base is a fixed special planet and is not the source or " +
      "controller of wandering mercenary groups. Its value is the large bank, " +
      "fighter stock, and repeatable maintenance behavior.\n\n" +
      "A direct capture needs a large ground-force advantage. Alternatives are " +
      "to wait for a plague, use Xannor or missiles to soften it, or use plasma " +
      "very carefully. Plasma damages ground forces and each productive " +
      "commodity simultaneously. Fire one bolt, read the resulting productivity, " +
      "then use Plasma damage and safe shot. On a new evenly productive base, " +
      "five bolts are the guide's conservative pre-plague ceiling; six can " +
      "destroy it. A previously attacked or plagued base may tolerate less.\n\n" +
      "After capture, the base can be moved. Maintenance restores 25 million " +
      "credits. If it has zero ground forces, it also rebuilds to a full force " +
      "of roughly 150,000. Leave about five ground forces instead: maintenance " +
      "then adds only a few hundred rather than the full defense. One force is " +
      "riskier because a plague can round it down to zero and trigger the large " +
      "rebuild.\n\n" +
      "For an attack built around plasma, accumulate banked credits and bolts " +
      "across several planets, consider capturing Xannoron first for its weapon " +
      "stock, and finish with missiles or an overwhelming landing force. Never " +
      "use a generic bolt count without measuring current productivity; the " +
      "planet is more valuable captured than accidentally vaporized." }

  static mapping_ { "# Dead-end and C;10 Mapping\n\n" +
      "A **C;10 shortest-path sweep uses zero turns** and captures how the " +
      "Computer routes from one chosen source sector to many targets. Run it " +
      "from a strategically useful viewpoint, not automatically sector 1. A " +
      "single viewpoint describes routes from that source; another source can " +
      "expose a different side of one-way links.\n\n" +
      "The control panel saves every returned route, not just counts. In the map " +
      "viewer:\n\n" +
      "- Direct outgoing warps are the first step seen from the source.\n" +
      "- Coverage sectors are useful scan or travel viewpoints appearing across " +
      "many captured routes.\n" +
      "- Dead-end candidates are path leaves worth verifying with a live sensor.\n" +
      "- Long-link and possible-rift edges are numerical or structural anomalies. " +
      "Open an edge to inspect the full captured route before acting on it.\n\n" +
      "C;10 paths alone do not report a sector's complete outgoing warp list. " +
      "Travel-and-sensor scans add that directed topology and record ports, " +
      "planets, and fighters. Use the map to choose a small set of high-value " +
      "viewpoints, then verify them live.\n\n" +
      "The original exhaustive method logged travel through every sector, " +
      "extracted sectors with only one displayed outgoing warp, removed " +
      "duplicates, and split port from no-port dead ends. Once that list existed, " +
      "daily scouting could target actual branch ends rather than every fifth " +
      "sector. The Wren mapper and sensor records replace the Notepad and " +
      "spreadsheet cleanup, but the strategic goal is unchanged." }

  static reportScanning_ { "# Port and Planet Scanning\n\n" +
      "Computer reports can scout without spending turns or creating newspaper " +
      "combat events.\n\n" +
      "**C;2 port reports:** The Computer withholds port information when hostile " +
      "fighters occupy the sector. Run C;2 over every saved port and investigate " +
      "each `No information available` result. It can indicate Xannor, " +
      "mercenaries, another player's fighters, or the defended sector of a " +
      "special planet. This only covers port sectors, roughly one third of a " +
      "typical universe, but it is silent and cannot trigger missile retaliation.\n\n" +
      "**C;9 planet reports:** Run C;9 over known dead ends to locate unguarded " +
      "planets. It is also silent, but defensive fighters hide a planet from the " +
      "report. Separate scripts for dead ends with and without ports make it " +
      "clear whether a failed C;2 is fighter-related and whether C;9 is the only " +
      "available report.\n\n" +
      "For your own planets, combine checks such as " +
      "`C;2;2971;2;2962;9;2805;1`: use C;2 where a port exists and C;9 where it " +
      "does not. At the start of the day this finds mercenaries waiting over a " +
      "planet and shows which local ports can still buy a large equipment load. " +
      "After a multi-day absence, compare the results with C;11 and C;13 before " +
      "routing the ship through old records.\n\n" +
      "The control panel's Scan saved ports action builds reviewed C;2 batches " +
      "from live sensor data, so the current map becomes the reusable port list." }

  static missileScouting_ { "# Missile Scouting\n\n" +
      "A one-missile probe reveals objects along its route and at the target, so " +
      "a prepared target list can map far more than direct ship travel. It is " +
      "also conspicuous and dangerous: enemy players and Xannor may return fire, " +
      "and the newspaper can expose both discoveries and the firing route.\n\n" +
      "Reduce the risk in one of three ways:\n\n" +
      "- Fire from a disposable random sector far from valuable planets, with " +
      "enough fighters and shields to survive retaliation.\n" +
      "- Fire from **behind**, rather than on top of, a friendly planet with " +
      "large ground forces. The planet intercepts return missiles, but every hit " +
      "can reveal it.\n" +
      "- First cause a Xannor missile to enter a black hole. In affected 3.6 " +
      "builds the Xannor stop all random and retaliatory missile fire for the " +
      "rest of that login session.\n\n" +
      "Put known player planets, friendly ships, special locations, and Xannor " +
      "groups reserved for turn farming into C;7 avoids before a broad sweep. " +
      "Clear or revise the avoids before ordinary travel. As the first Xannor " +
      "groups are found, later probes become less productive because many " +
      "wanderers sit in dead ends outside the common grid routes.\n\n" +
      "A mature loop is: scout until a useful Xannor group is found; spend turns " +
      "trading planet cargo; travel to that group near zero turns; use plasma to " +
      "leave about 100,000; kill those with fighters; spend the refill; and " +
      "repeat. Missile scouting is then serving a specific economic loop rather " +
      "than merely producing screen output." }

  static scoutingHardware_ { "# Robots, Danger Scanner, and Spies\n\n" +
      "These tools are version-specific and change the safest mapping strategy.\n\n" +
      "**Sensor robots** belong to Yankee Trader 3.2. Send one along a planned " +
      "autopilot route before committing the ship. In 3.2, black holes do not " +
      "appear on adjacent scans and there is no Danger Scanner, so an unscouted " +
      "route can end in a black hole or mine field. Robots are also effective " +
      "daily probes for known dead ends when searching for Xannor.\n\n" +
      "**Danger Scanner** belongs to stock 3.6 and costs about 250,000 credits. " +
      "It stops autopilot before black holes, mines, and hostile fighters. Add " +
      "the hazard to C;7 and reroute, attack it deliberately, or leave and " +
      "re-enter if a randomly placed black hole makes the route inconvenient.\n\n" +
      "**Spies** belong to stock 3.6, cost about 333,000 credits each, and are " +
      "limited to three. They roam and report enemy forces or planets. In " +
      "high-turn games without missiles, buy three early in the day and start " +
      "them far apart, such as sectors 500, 1500, and 2500 in a 3,000-sector " +
      "universe, or place them over known hostile-planet areas. One-way links " +
      "gradually funnel roaming spies toward sector 1, so respread them rather " +
      "than expecting permanent coverage.\n\n" +
      "Patched games may combine or remove these features. Use the current game " +
      "profile, not the version name alone, to decide which scouting actions are " +
      "valid." }

  static planetBank_ { "# Estimating a Planet's Bank\n\n" +
      "A planet's ground forces grow from both existing ground forces and banked " +
      "credits. By measuring that growth, you can estimate an enemy bank without " +
      "landing on the planet.\n\n" +
      "Record the exact ground-force count, wait at least 60 seconds, then scan " +
      "again from the same or an adjacent sector. Merely being in the sector or " +
      "using **S** updates the minute-by-minute value. **C;9 does not**, so a " +
      "series of C;9 reports cannot supply the measurement.\n\n" +
      "Existing ground forces produce about one percent per day. The calculator " +
      "uses:\n\n" +
      "- estimated daily growth = `ceil(gain per minute x 1,440) - " +
      "current forces / 100`\n" +
      "- estimated bank in millions = `daily growth / 100`\n\n" +
      "For example, apparent growth of 2,500 ground forces per day after removing " +
      "the existing-force contribution suggests roughly 25 million credits in " +
      "the bank. Minute rounding and other activity make this an estimate, so " +
      "measure over several minutes when the decision matters.\n\n" +
      "For multiple enemy planets, record one adjacent sector for each. Run a " +
      "travel-and-sensor sequence over those adjacent sectors, note every ground " +
      "force count, wait, and repeat the same sequence. This turns the estimate " +
      "into a repeatable intelligence pass and also shows which planet banks are " +
      "growing or being emptied." }

  static xannorBacktrace_ { "# Backtracing the Xannor HQ\n\n" +
      "When a missile hits a roaming Xannor group, the return missile originates " +
      "at the hidden Xannor HQ, not at the group you attacked. Fighter " +
      "breadcrumbs can therefore trace the return route backward. This is a " +
      "mid- or high-level missile-game tactic: repeated return salvos can bypass " +
      "fighters and damage shields, so bring substantial shielding.\n\n" +
      "Procedure:\n\n" +
      "1. Find a stable Xannor group with at least about 50,000 fighters.\n" +
      "2. Create a planet in sector 1 with a few ground forces. It identifies " +
      "return paths that collapse through sector 1 and cannot be traced from the " +
      "current branch.\n" +
      "3. Move to a high random sector or an area suspected to be near the HQ.\n" +
      "4. Prepare a repeatable one-missile command, such as `!;2315;1/` for a " +
      "group in sector 2315.\n" +
      "5. Leave one defensive fighter in every sector adjacent to the current " +
      "sector, then replay the missile.\n" +
      "6. Observe which breadcrumb the return missile hits. Move there, seed all " +
      "of its adjacent sectors, and fire again.\n" +
      "7. If the sector-1 planet is hit, abandon this branch and restart from a " +
      "different high sector.\n\n" +
      "The Adjacent fighter breadcrumbs tool automates the repeated neighbor " +
      "placement from a verified Main Command prompt. Record each hit as a path, " +
      "maintain Xannor and friendly sectors in C;7 avoids for unrelated work, and " +
      "stop if shielding loss makes the next retaliation unsafe." }

  static advanced_ { "# Advanced Scouting and Deception\n\n" +
      "**Whole-universe patrol:** Once dead ends and planet approaches are known, " +
      "build a route that visits the sector next to every branch and scans. In " +
      "no-missile 3.6e-style games this can find planets without entering their " +
      "defended sector, avoiding a newspaper fighter battle. Similar patrols can " +
      "visit every owned planet to equalize ground forces, harvest fighters, and " +
      "replace sector guards.\n\n" +
      "**Missile draining:** Find a hostile player, stay two sectors away, create " +
      "a planet, land, and move that planet one sector toward the target. Until " +
      "the ship itself performs an M move, retaliation is resolved against the " +
      "intervening planet while the ship remains two sectors away. Fire one " +
      "missile repeatedly until return fire stops. A dead-end setup can provide " +
      "the same protected geometry.\n\n" +
      "**Group 20 location clue:** Immediately before maintenance, a top player " +
      "can temporarily lower score to second place, then log in before others " +
      "after maintenance. The twentieth Xannor group's newspaper attack may name " +
      "the planet hiding the actual top player. This is only useful when the " +
      "planet name is distinctive.\n\n" +
      "**De-cloaking:** Fire one plasma bolt into a suspected sector, or repeatedly " +
      "scan it with a sequence such as `s;s;s;s;s;s/r20`. Players often camp over " +
      "planets, but an experienced opponent may use an empty dead end instead.\n\n" +
      "**Black-hole scattering:** Single missiles fired into a black hole are " +
      "redirected around the universe with the black hole treated as their source. " +
      "Targets retaliate toward the hole. In affected 3.6 versions, one Xannor " +
      "retaliation into the hole disables further Xannor missile fire for that " +
      "login session, creating a safer window for scouting or attacking the HQ.\n\n" +
      "Each tactic depends on a specific engine quirk. Test it with expendable " +
      "resources on the particular BBS before building a day's plan around it." }

  static specialPlanets_ { "# Special Planets\n\n" +
      "**The Wanderer** relocates when a player tries to move it. Treat its " +
      "location as temporary intelligence rather than a permanent waypoint.\n\n" +
      "**Xannoron** cannot be moved. Ground forces left there become Xannor forces " +
      "at maintenance. With no ground forces, it relocates or rebuilds with a " +
      "small defense. Holding the sector requires defensive fighters sized for " +
      "all surviving Xannor plus the score-based retake force described in the " +
      "Xannoron topic.\n\n" +
      "**The Mercenary Base** can be moved after capture and restores 25 million " +
      "banked credits each day. Leaving zero ground forces causes a full defense " +
      "near 150,000 to return; leaving about five produces only a small addition. " +
      "This makes a previously captured base a repeatable income target if its " +
      "location and post-maintenance defense are tracked.\n\n" +
      "All three should be stored as dated location records. Their unusual " +
      "maintenance behavior makes an old undated sector actively misleading, " +
      "especially for the Wanderer and a rebuilt Xannoron." }

  static version32_ { "# Yankee Trader 3.2\n\n" +
      "YT 3.2 is not simply 3.6 with fewer sectors. Important differences include:\n\n" +
      "- 1,000 sectors, 75 planets, and a working repeat count up to 30.\n" +
      "- No plasma bolts, Mercenary Base, Danger Scanner, or spies.\n" +
      "- Sensor robots can scout any sector and should precede autopilot because " +
      "black holes are invisible on adjacent scans.\n" +
      "- Mines and missiles are sold at Earth; mines are much more effective.\n" +
      "- Planet productivity cannot be bought directly. Build it by transferring " +
      "cargo, making high-volume cheap equipment especially valuable.\n" +
      "- Planet banks pay about five percent interest rather than one percent; " +
      "ground forces grow faster and are not constrained by 3.6 civil wars.\n" +
      "- Fighter production is about one third of 3.6. Missiles average about 15 " +
      "ground forces each.\n" +
      "- Xannor fighter kills can accumulate turns above the normal daily cap.\n\n" +
      "The 3.2 economic loop is to find ports selling massive equipment volume " +
      "for one to three credits, transfer it to planets, move those planets to " +
      "high-price equipment buyers, then hide them in dead ends. Earth sales " +
      "remain the practical source of large fighter, shield, and ground-force " +
      "purchases.\n\n" +
      "For Xannor, use missiles to reduce a large group toward roughly 125,000, " +
      "then finish with fighters for a 500-turn reward. A rough missile planning " +
      "value is 2,500-3,000 Xannor fighters per missile. Send sensor robots to the " +
      "known dead-end list each day to find the next group safely." }

  static balanceBugs_ { "# Known 3.6 Balance Bugs\n\n" +
      "Some BBSes patch stock 3.6 because several mechanics can dominate an " +
      "otherwise long economic game. Knowing which rules are active is part of " +
      "understanding the local game; do not assume an archived exploit is welcome " +
      "on a public BBS.\n\n" +
      "**Earth ownership loop:** In an unpatched game, the owner of Earth receives " +
      "all credits another player spends there. Two cooperating accounts can " +
      "alternate buying Earth and spending the refunded bank, generating " +
      "effectively unlimited Earth purchases. Common patches make Earth " +
      "unaffordable.\n\n" +
      "**Global anti-cloak sweep:** Stock anti-cloak can reveal every player for " +
      "the current session. A rich player can then fire single missiles across " +
      "the universe, kill Xannor groups for replacement turns, find players, kill " +
      "them with quadratic plasma, and claim their planets in one day. Common " +
      "balance patches make anti-cloak unaffordable or greatly reduce Xannor turn " +
      "rewards.\n\n" +
      "**Repeat bug:** In 3.6, repeats from 11 through 19 behave as 20. Plan " +
      "turn-consuming loops around the actual behavior and use Ctrl-X to stop a " +
      "long repeat.\n\n" +
      "**Black-hole Xannor shutdown:** In affected 3.6 builds, a Xannor missile " +
      "entering a black hole disables all further Xannor missile fire until the " +
      "player leaves and rejoins. This behavior is absent from 3.2.\n\n" +
      "Use Detect game settings for queryable limits and ask the sysop about " +
      "balance patches. A version banner cannot reveal every executable patch." }

  static yesNo_(value) { value == true ? "enabled" : "disabled" }
}
