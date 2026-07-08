#include "config.h"

#include "game_str.h"

/* -------------------------------------------------------------------------- */

const char *game_str_mm_continue = "Continue";
const char *game_str_mm_load = "Load Game";
const char *game_str_mm_new = "New Game";
const char *game_str_mm_quit = "Quit to OS";

const char *game_str_tbl_race[RACE_NUM + 1] = { "Human", "Mrrshan", "Silicoid", "Sakkra", "Psilon", "Alkari", "Klackon", "Bulrathi", "Meklar", "Darlok", "Random" };
const char *game_str_tbl_races[RACE_NUM] = { "Humans", "Mrrshans", "Silicoids", "Sakkras", "Psilons", "Alkaris", "Klackons", "Bulrathis", "Meklars", "Darloks" };
const char *game_str_tbl_banner[BANNER_NUM + 1] = { "Blue", "Green", "Purple", "Red", "White", "Yellow", "Random" };
const char *game_str_tbl_gsize[GALAXY_SIZE_NUM] = { "Small", "Medium", "Large", "Huge" };
const char *game_str_tbl_diffic[DIFFICULTY_NUM] = { "Simple", "Easy", "Average", "Hard", "Impossible" };
const char *game_str_tbl_oppon[5] = { "One", "Two", "Three", "Four", "Five" };

const char *game_str_tbl_traits[RACE_NUM * 3] = {
    "EXPERT TRADERS", "AND MAGNIFICENT", "DIPLOMATS",
    "SUPERIOR GUNNERS", "", "",
    "IMMUNE TO", "HOSTILE PLANET", "ENVIRONMENTS",
    "INCREASED", "POPULATION", "GROWTH",
    "SUPERIOR", "RESEARCH", "TECHNIQUES",
    "SUPERIOR PILOTS", "", "",
    "INCREASED", "WORKER", "PRODUCTION",
    "TERRIFIC GROUND", "FIGHTERS", "",
    "ENHANCED", "FACTORY", "CONTROLS",
    "SUPREME SPIES", "", ""
};

const char *game_str_tbl_trait1[TRAIT1_NUM] = {
    "Xenophobic", "Ruthless", "Aggressive", "Erratic", "Honorable", "Pacifistic"
};

const char *game_str_tbl_trait2[TRAIT2_NUM] = {
    "Diplomat", "Militarist", "Expansionist", "Technologist", "Industrialist", "Ecologist"
};

const char *game_str_ng_choose_race = "Choose Race";
const char *game_str_ng_choose_banner = "Choose Banner";
const char *game_str_ng_your_name = "Your Name...";
const char *game_str_ng_home_name = "Home World...";
const char *game_str_ng_ai = "AI";
const char *game_str_ng_computer = "Computer";
const char *game_str_ng_player = "Player";
const char *game_str_ng_cancel = "Cancel";
const char *game_str_ng_ok = "Start Game";
const char *game_str_ng_allai = "Must have at least one human player!";

const char *game_str_tbl_planet_names[PLANET_NAMES_MAX] = {
    "Paranar", "Denubius", "Antares", "Draconis", "Zoctan", "Rigel", "Talas", "Moro", "Quayal", "Neptunus",
    "Jinga", "Argus", "Escalon", "Berel", "Collassa", "Whynil", "Nordia", "Tau Cygni", "Phyco", "Firma",
    "Keeta", "Arietis", "Rhilus", "Willow", "Mu Delphi", "Stalaz", "Gorra", "Beta Ceti", "Spica", "Omicron",
    "Rha", "Kailis", "Vulcan", "Centauri", "Herculis", "Endoria", "Kulthos", "Hyboria", "Zhardan", "Yarrow",
    "Incedius", "Paladia", "Romulas", "Galos", "Uxmai", "Thrax", "Laan", "Imra", "Selia", "Seidon",
    "Tao", "Rana", "Vox", "Maalor", "Xudax", "Helos", "Crypto", "Gion", "Phantos", "Reticuli",
    "Maretta", "Toranor", "Exis", "Tyr", "Ajax", "Obaca", "Dolz", "Drakka", "Ryoun", "Vega",
    "Anraq", "Gienah", "Rotan", "Proxima", "Mobas", "Iranha", "Celtsi", "Dunatis", "Morrig", "Primodius",
    "Nyarl", "Ukko", "Crius", "Hyades", "Kronos", "Guradas", "Rayden", "Kakata", "Misha", "Xendalla",
    "Artemis", "Aurora", "Proteus", "Esper", "Darrian", "Trax", "Xengara", "Nitzer", "Simius", "Bootis",
    "Pollus", "Cygni", "Aquilae", "Volantis", "Tauri", "Regulus", "Klystron", "Lyae", "Capella", "Alcor"
};

const char *game_str_tbl_home_names[RACE_NUM + 1] = {
    "Sol", "Fierias", "Cryslon", "Sssla", "Mentar", "Altair", "Kholdan", "Ursa", "Meklon", "Nazin", "Randomia"
};

const char *game_str_rndempname = "Mr Random";
const char *game_str_planet_name_orion = "Orion";

const char *game_str_tbl_stship_names[NUM_SHIPDESIGNS] = {
    "SCOUT", "FIGHTER", "DESTROYER", "BOMBER", "COLONY SHIP", "NONE"
};

const char *game_str_tbl_copyprotection_ship_names[40] = {
    "Paladin", "Marauder", "Gladiator", "Devastator", "Cutlass",
    "Sky Hawk", "Shark", "Dark Star", "Cyclops", "Panther",
    "Corvette", "Penetrator", "Punisher", "Black Widow", "Destroyer",
    "Warbird", "Piranha", "Banshee", "Hydra", "Cheetah",
    "Dreadstar", "Foxbat", "Hunter", "Hornet", "Star Blazer",
    "Knight", "Kraken", "Avenger", "Manta", "Warcat",
    "Escort", "Nemesis", "Warbear", "Scorpion", "Seeker",
    "Condor", "Sun Fire", "Viper", "Titan", "Lynx"
};

const char *game_str_tbl_monsh_names[MONSTER_NUM] = {
    "SPACE CRYSTAL", "SPACE AMOEBA", "GUARDIAN"
};

const char *game_str_tbl_mon_names[MONSTER_NUM] = {
    "Space Crystal", "Space Amoeba", "Guardian"
};

const char *game_str_ai_colonyship = "COLONY SHIP";

const char *game_str_st_none = "NONE";
const char *game_str_st_none2 = "None";

const char *game_str_tbl_st_weap[WEAPON_NUM - 1] = {
    "NUCLEAR BOMB", "LASER", "NUCLEAR MISSILE", "NUCLEAR MISSILE", "HEAVY LASER", "HYPER-V ROCKET", "HYPER-V ROCKET", "GATLING LASER",
    "NEUTRON PELLET GUN", "HYPER-X ROCKET", "HYPER-X ROCKET", "FUSION BOMB", "ION CANNON", "HEAVY ION CANNON", "SCATTER PACK V", "SCATTER PACK V",
    "DEATH SPORES", "MASS DRIVER", "MERCULITE MISSILE", "MERCULITE MISSILE", "NEUTRON BLASTER", "HEAVY BLAST CANNON", "ANTI-MATTER BOMB", "GRAVITON BEAM",
    "STINGER MISSILE", "STINGER MISSILE", "HARD BEAM", "FUSION BEAM", "HEAVY FUSION BEAM", "OMEGA-V BOMB", "ANTI-MATTER TORP", "MEGABOLT CANNON",
    "PHASOR", "HEAVY PHASOR", "SCATTER PACK VII", "SCATTER PACK VII", "DOOM VIRUS", "AUTO BLASTER", "PULSON MISSILE", "PULSON MISSILE",
    "TACHYON BEAM", "GAUSS AUTOCANNON", "PARTICLE BEAM", "HERCULAR MISSILE", "HERCULAR MISSILE", "PLASMA CANNON", "DISRUPTOR", "PULSE PHASOR",
    "NEUTRONIUM BOMB", "BIO TERMINATOR", "HELLFIRE TORPEDO", "ZEON MISSILE", "ZEON MISSILE", "PROTON TORPEDO", "SCATTER PACK X", "SCATTER PACK X",
    "TRI-FOCUS PLASMA", "STELLAR CONVERTER", "MAULER DEVICE", "PLASMA TORPEDO", "CRYSTAL RAY", "DEATH RAY", "AMEOBA STREAM"
};

const char *game_str_tbl_st_weapx[WEAPON_NUM - 1] = {
    "GROUND ATTACKS ONLY", " ", "2 SHOTS, +1 SPEED", "5 SHOTS", " ", "2 SHOTS, +1 SPEED", "5 SHOTS", "FIRES 4 TIMES/TURN ",
    "HALVES ENEMY SHIELDS", "2 SHOTS, +1 TO HIT", "5 SHOTS, +1 TO HIT", "GROUND ATTACKS ONLY ", " ", " ", "2 SHOTS, MIRVS TO 5", "5 SHOTS, MIRVS TO 5",
    "BIOLOGICAL WEAPON", "HALVES ENEMY SHIELDS", "2 SHOTS, +2 TO HIT", "5 SHOTS, +2 TO HIT", " ", " ", "GROUND ATTACKS ONLY", "STREAMING ATTACK",
    "2 SHOTS, +3 TO HIT", "5 SHOTS, +3 TO HIT", "HALVES SHIELD STR", " ", " ", "GROUND ATTACKS ONLY ", "FIRES ONE PER 2 TURNS", "+3 LEVELS TO HIT",
    " ", " ", "2 SHOTS, MIRVS TO 7", "5 SHOTS, MIRVS TO 7", "BIOLOGICAL WEAPON", "FIRES 3 TIMES/TURN", "2 SHOTS, +4 TO HIT", "5 SHOTS, +4 TO HIT",
    "STREAMING ATTACK", "1/2 SHIELDS, FIRES 4$", "HALVES SHIELDS STR", "2 SHOTS, +5 TO HIT", "5 SHOTS, +5 TO HIT", " ", " ", "FIRES 3 TIMES/TURN",
    "GROUND ATTACKS ONLY", "BIOLOGICAL WEAPON", "HITS ALL FOUR SHIELDS", "2 SHOTS, +6 TO HIT", "5 SHOTS, +6 TO HIT", "FIRES ONE PER 2 TURNS", "2 SHOTS, MIRVS TO 10", "5 SHOTS, MIRVS TO 10",
    " ", "HITS ALL FOUR SHIELDS", "CRUEL BRUTAL DAMAGE", "LOSES 15 DAMAGE/HEX", "", "", ""
};

const char *game_str_tbl_st_comp[SHIP_COMP_NUM - 1] = {
    "MARK I", "MARK II", "MARK III", "MARK IV", "MARK V", "MARK VI", "MARK VII", "MARK VIII",
    "MARK IX", "MARK X", "MARK XI"
};

const char *game_str_tbl_st_engine[SHIP_ENGINE_NUM] = {
    "RETROS", "NUCLEAR", "SUB-LIGHT", "FUSION", "IMPULSE", "ION", "ANTI-MATTER", "INTERPHASED", "HYPERTHRUST"
};

const char *game_str_tbl_st_armor[SHIP_ARMOR_NUM] = {
    "TITANIUM", "TITANIUM II", "DURALLOY", "DURALLOY II", "ZORTRIUM", "ZORTRIUM II", "ANDRIUM", "ANDRIUM II", "TRITANIUM",
    "TRITANIUM II", "ADAMANTIUM", "ADAMANTIUM II", "NEUTRONIUM", "NEUTRONIUM II"
};

const char *game_str_tbl_st_shield[SHIP_SHIELD_NUM - 1] = {
    "CLASS I", "CLASS II", "CLASS III", "CLASS IV", "CLASS V", "CLASS VI", "CLASS VII", "CLASS IX",
    "CLASS XI", "CLASS XIII", "CLASS XV"
};

const char *game_str_tbl_st_jammer[SHIP_JAMMER_NUM - 1] = {
    "JAMMER I", "JAMMER II", "JAMMER III", "JAMMER IV", "JAMMER V", "JAMMER VI", "JAMMER VII", "JAMMER VIII",
    "JAMMER IX", "JAMMER X"
};

const char *game_str_tbl_st_specsh[SHIP_SPECIAL_NUM] = {
    "NO SPECIALS", "RESERVE TANKS", "COLONY BASE", "BARREN BASE", "TUNDRA BASE", "DEAD BASE", "INFERNO BASE", "TOXIC BASE", 
    "RADIATED BASE", "BATTLE SCANNER", "ANTI-MISSILES", "REPULSOR BEAM", "WARP DISSIPATOR", "ENERGY PULSAR", "INERT STABILIZER", "ZYRO SHIELD", 
    "AUTO REPAIR", "STASIS FIELD", "CLOAKING DEVICE", "ION STREAM", "H-ENERGY FOCUS", "IONIC PULSAR", "BLACK HOLE GEN", "TELEPORTER", 
    "LIGHTNING SHIELD", "NEUTRON STREAM", "ADV DMG CONTROL", "TECH NULLIFIER", "INERT. NULLIFIER", "ORACLE INTERFACE", "DISPLACE DEVICE"
};
const char *game_str_tbl_st_special[SHIP_SPECIAL_NUM - 1] = {
    "RESERVE FUEL TANKS", "STANDARD COLONY BASE", "BARREN COLONY BASE", "TUNDRA COLONY BASE", "DEAD COLONY BASE", "INFERNO COLONY BASE", "TOXIC COLONY BASE", "RADIATED COLONY BASE",
    "BATTLE SCANNER", "ANTI-MISSILE ROCKETS", "REPULSOR BEAM", "WARP DISSIPATOR", "ENERGY PULSAR", "INERTIAL STABILIZER", "ZYRO SHIELD", "AUTOMATED REPAIR",
    "STASIS FIELD", "CLOAKING DEVICE", "ION STREAM PROJECTOR", "HIGH ENERGY FOCUS", "IONIC PULSAR", "BLACK HOLE GENERATOR", "SUB SPACE TELEPORTER", "LIGHTNING SHIELD",
    "NEUTRON STREAM PROJECTOR", "ADV DAMAGE CONTROL", "TECHNOLOGY NULLIFIER", "INERTIAL NULLIFIER", "ORACLE INTERFACE", "DISPLACMENT DEVICE"
};
const char *game_str_tbl_st_specialx[SHIP_SPECIAL_NUM - 1] = {
    "EXTENDS SHIP RANGE BY 3 PARSECS", "ALLOWS NORMAL PLANET LANDINGS", "ALLOWS BARREN PLANET LANDINGS", "ALLOWS TUNDRA PLANET LANDINGS", "ALLOWS DEAD PLANET LANDINGS", "ALLOWS INFERNO PLANET LANDINGS",
    "ALLOWS TOXIC PLANET LANDINGS", "ALLOWS RADIATED PLANET LANDINGS", "DISPLAYS ENEMY SHIP STATS", "40% CHANCE MISSILES DESTROYED", "MOVES ENEMY SHIPS BACK 1 SPACE", "REDUCES SPEED OF ENEMY SHIPS",
    "EXPANDS TO INFLICT 1-5 HITS", "ADDS +2 TO MANEUVERABILITY", "75% CHANCE MISSILES DESTROYED", "HEALS 15% OF SHIP'S HITS A TURN", "ENEMY FROZEN FOR 1 TURN", "RENDERS SHIPS NEARLY INVISIBLE",
    "REDUCES ENEMY ARMOR BY 20%", "INCREASES WEAPON RANGES BY 3", "EXPANDS TO INFLICT 2-10 HITS", "KILLS 25%-100% OF ENEMY SHIPS", "TELEPORTS SHIP IN COMBAT", "100% CHANCE MISSILES DESTROYED",
    "REDUCES ENEMY ARMOR BY 40%", "HEALS 30% OF SHIP'S HITS A TURN", "DESTROYS ENEMY COMPUTERS", "ADDS +4 TO MANEUVERABILITY", "CONCENTRATES BEAM ATTACKS", "1/3 OF ALL ENEMY ATTACKS MISS"
};

const char *game_str_tbl_st_hull[SHIP_HULL_NUM] = { "SMALL", "MEDIUM", "LARGE", "HUGE" };

const char *game_str_sm_crystal = "CRYSTAL";
const char *game_str_sm_amoeba = "AMOEBA";
const char *game_str_sm_game = "Game";
const char *game_str_sm_design = "Design";
const char *game_str_sm_fleet = "Fleet";
const char *game_str_sm_map = "Map";
const char *game_str_sm_races = "Races";
const char *game_str_sm_planets = "Planets";
const char *game_str_sm_tech = "Tech";
const char *game_str_sm_next_turn = "Next Turn";

const char *game_str_tbl_sm_stinfo[STAR_TYPE_NUM] = {
    "Yellow stars offer the best chance of discovering terran and sub-terran planets.",
    "Red stars are old, dull stars that commonly have poor planets.",
    "Green stars are moderately bright and have a wide range of planetary types.",
    "White stars burn incredibly hot and generally have hostile planets.",
    "Blue stars are relatively young stars with mineral rich lifeless planets.",
    "Neutron stars are rare and offer the greatest chance of finding rich planets."
};

const char *game_str_sm_range = "Range";
const char *game_str_sm_parsec = "Parsec";
const char *game_str_sm_parsecs = "Parsecs";
const char *game_str_sm_parsecs2 = "PARSECS";
const char *game_str_sm_colony = "Colony";
const char *game_str_sm_lastrep = "Last Reported As A";
const char *game_str_sm_stargate = "STAR GATE";
const char *game_str_sm_prodnone = "NONE";
const char *game_str_sm_prod_y = "Y";
const char *game_str_sm_defupg = "UPGRD";
const char *game_str_sm_defshld = "SHIELD";
const char *game_str_sm_refit = "REFIT";
const char *game_str_sm_irc[] = { "IRC II", "IRC III", "IRC IV", "IRC V", "IRC VI", "IRC VII" };
const char *game_str_sm_indmax = "MAX";
const char *game_str_sm_indres = "RESERV";
const char *game_str_sm_ecowaste = "WASTE";
const char *game_str_sm_ecoclean = "CLEAN";
const char *game_str_sm_ecoatmos = "ATMOS";
const char *game_str_sm_ecotform = "T-FORM";
const char *game_str_sm_ecosoil = "SOIL";
const char *game_str_sm_ecogaia = "GAIA";
const char *game_str_sm_ecopop = "POP";
const char *game_str_sm_unexplored = "UNEXPLORED";
const char *game_str_sm_nohabit = "NO HABITABLE";
const char *game_str_sm__planets = "PLANETS";

const char *game_str_tbl_sm_pltype[PLANET_TYPE_NUM + 1] = {
    "NO HABITABLE PLANETS", "RADIATED", "TOXIC", "INFERNO", "DEAD",
    "TUNDRA", "BARREN", "MINIMAL", "DESERT", "STEPPE", "ARID", "OCEAN",
    "JUNGLE", "TERRAN", "GAIA"
};

const char *game_str_sm_plague = "Plague";
const char *game_str_sm_nova = "Nova";
const char *game_str_sm_comet = "Comet";
const char *game_str_sm_pirates = "Pirates";
const char *game_str_sm_rebellion = "Rebellion";
const char *game_str_sm_unrest = "UNREST";
const char *game_str_sm_accident = "Accident";

const char *game_str_tbl_sm_pgrowth[PLANET_GROWTH_NUM] = {
    "HOSTILE", " ", "FERTILE", "GAIA"
};

const char *game_str_tbl_sm_pspecial[PLANET_SPECIAL_NUM] = {
    "ULTRA POOR", "POOR", "", "ARTIFACTS", "RICH", "ULTRA RICH", "4$ TECH"
};

const char *game_str_sm_pop = "POP";
const char *game_str_sm_max = "MAX";

const char *game_str_sm_hasreached = "has reached its";
const char *game_str_sm_indmaxof = "industry maximum of";
const char *game_str_sm_factories = "factories";
const char *game_str_sm_extrares = " The extra spent was placed in the planetary reserve.";
const char *game_str_sm_popmaxof = "population maximum of";
const char *game_str_sm_colonists = "colonists";
const char *game_str_sm_hasterraf = "has been terraformed to a";
const char *game_str_tbl_sm_terraf[3] = {
    "normal", "fertile", "gaia"
};
const char *game_str_sm_envwith = "environment with";
const char *game_str_tbl_sm_envmore[3] = {
    "", "150% of ", "double "
};
const char *game_str_sm_stdgrow = "the standard growth rate";
const char *game_str_sm_hasfsgate = "has finished building a stargate";
const char *game_str_sm_hasfshield = "has completed building a Class";
const char *game_str_sm_planshield = "Planetary Shield";
const char *game_str_sm_planratio = " Planetary spending ratios may be changed at this time. ";

const char *game_str_sm_fleetdep = "FLEET DEPLOYMENT";
const char *game_str_sm_destoor = "DESTINATION IS OUT OF RANGE,";
const char *game_str_sm_destoor2 = "OUT OF RANGE";
const char *game_str_sm_parsfromcc = "PARSECS FROM CLOSEST COLONY";
const char *game_str_sm_eta = "ETA";
const char *game_str_sm_turn = "TURN";
const char *game_str_sm_turns = "TURNS";
const char *game_str_sm_chdest = "Choose destination and number to send";

const char *game_str_sm_outsr = "OUT SHIP RANGE BY";

const char *game_str_sm_sreloc = "Ship Relocation";
const char *game_str_sm_sreloc2 = "Choose another star system under your control to redirect newly built ships to.";
const char *game_str_sm_delay = "DELAY";

const char *game_str_sm_seltr = "Select a destination star system to send colonists or troops to.";
const char *game_str_sm_notrange = "You do not have the required ship range to reach the system.";
const char *game_str_sm_notrange1 = "The star system is";
const char *game_str_sm_notrange2 = "parsecs away and you have a maximum range of";
const char *game_str_sm_notrange3 = "parsecs.";
const char *game_str_sm_trfirste = "You must first explore a star system and form a new colony before transporting colonists.";
const char *game_str_sm_trcontr1 = "You must have at least controlled";
const char *game_str_sm_trcontr2 = "environments to land troops onto the planet.";
const char *game_str_sm_trfirstc = "You must first build a ship equipped with a colony base and create a new colony before sending out transports.";
const char *game_str_sm_trwarna = "Warning, destination is owned by an ally";
const char *game_str_sm_trwarnm1 = "Warning - Target planet can only support";
const char *game_str_sm_trwarnm2 = "million!";
const char *game_str_sm_trchnum1 = "Choose number of colonists to transport";
const char *game_str_sm_trchnum2 = "Choose number of troops to transport";
const char *game_str_sm_trans1 = "Transport";
const char *game_str_sm_transs = "Transports";
const char *game_str_sm_tdest = "Destination";

const char *game_str_sm_bomb1 = "Bomb the";
const char *game_str_sm_bomb2 = "Enemy Planet?";
const char *game_str_sm_trinb1 = "Troop Transport";
const char *game_str_sm_trinb1s = "Troop Transport";
const char *game_str_sm_trinb2 = "Currently Enroute";

const char *game_str_sm_obomb1 = "Orbital";
const char *game_str_sm_obomb2 = "Bombardment";
const char *game_str_sm_cdest1 = "colony";
const char *game_str_sm_cdest2 = "destroyed";
const char *game_str_sm_ineff1 = "bombing";
const char *game_str_sm_ineff2 = "ineffective";
const char *game_str_sm_bkill1 = "MILLION";
const char *game_str_sm_bkill2 = "KILLED";
const char *game_str_sm_bfact1 = "FACTORY";
const char *game_str_sm_bfact1s = "FACTORIES";
const char *game_str_sm_bfact2 = "DESTROYED";

const char *game_str_sm_traad1 = "transports attempting to land on";
const char *game_str_sm_traad2 = "were all destroyed.";
const char *game_str_sm_trbdb1 = "The base at";
const char *game_str_sm_trbdb2 = "was destroyed before the transports arrived leaving the colonists without supplies and shelter. All have perished.";

const char *game_str_sm_inorbit = "In Orbit";

const char *game_str_sm_ship_everywhere = "Build everywhere";
const char *game_str_sm_ship_replace = "Replace";

const char *game_str_tbl_roman[31] = {
    " ", "I", "II", "III", "IV", "V", "VI", "VII",
    "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV",
    "XVI", "XVII", "XVIII", "XIX", "XX", "XXI", "XXII", "XXIII",
    "XXIV", "XXV", "XXVI", "XXVII", "XXVIII", "XXIX", "XXX"
};

const char *game_str_no_events = "No Events";
const char *game_str_no_elections = "No Elections";
const char *game_str_bc = "BC";
const char *game_str_y = "Y";
const char *game_str_year = "Year ";
const char *game_str_year0 = "/Year";
const char *game_str_year1 = "Year:";
const char *game_str_player = "Player";

const char *game_str_pl_reserve = "Reserve";
const char *game_str_pl_plague = "PLAGUE";
const char *game_str_pl_nova = "SUPER NOVA";
const char *game_str_pl_comet = "COMET";
const char *game_str_pl_pirates = "PIRATES";
const char *game_str_pl_rebellion = "REBELLION";
const char *game_str_pl_unrest = "UNREST";
const char *game_str_pl_accident = "ACCIDENT";
const char *game_str_pl_spending = "Spending Costs";
const char *game_str_pl_tincome = "Total Income";
const char *game_str_pl_transof = "Transfer of planetary";
const char *game_str_pl_resto = "reserves to";

const char *game_str_sd_cancel = "CANCEL";
const char *game_str_sd_build = "BUILD";
const char *game_str_sd_clear = "CLEAR";
const char *game_str_sd_comp = "Computer";
const char *game_str_sd_shield = "Shield";
const char *game_str_sd_ecm = "Ecm";
const char *game_str_sd_armor = "Armor";
const char *game_str_sd_engine = "Engine";
const char *game_str_sd_man = "Maneuver";
const char *game_str_tbl_sd_spec[SPECIAL_SLOT_NUM] = {
    "Special 1", "Special 2", "Special 3"
};
const char *game_str_tbl_sd_weap[WEAPON_SLOT_NUM] = {
    "Weapon 1", "Weapon 2", "Weapon 3", "Weapon 4",
};
const char *game_str_sd_count = "Count";
const char *game_str_sd_sweap = "Ship Weapons";
const char *game_str_sd_damage = "Damage";
const char *game_str_sd_rng = "Rng";
const char *game_str_sd_notes = "Notes";
const char *game_str_sd_hp = "HIT POINTS";
const char *game_str_sd_warp = "WARP";
const char *game_str_sd_def = "DEF";
const char *game_str_sd_cspeed = "COMBAT SPEED";
const char *game_str_sd_absorbs = "ABSORBS";
const char *game_str_sd_hit = "HIT";
const char *game_str_sd_hits = "HITS";
const char *game_str_sd_misdef = "MISSILE DEF";
const char *game_str_sd_att = "ATTACK LEVEL";
const char *game_str_sd_comptype = "COMPUTER TYPE";
const char *game_str_sd_cost = "COST";
const char *game_str_sd_size = "SIZE";
const char *game_str_sd_power = "POWER";
const char *game_str_sd_space = "SPACE";
const char *game_str_sd_comps = "COMPUTERS";
const char *game_str_sd_shieldtype = "SHIELD TYPE";
const char *game_str_sd_shields = "SHIELDS";
const char *game_str_sd_ecmtype = "ECM TYPE";
const char *game_str_sd_ecm2 = "ECM";
const char *game_str_sd_armortype = "ARMOR TYPE";
const char *game_str_sd_armor2 = "ARMOR";
const char *game_str_sd_engtype = "ENGINE TYPE";
const char *game_str_sd_numengs = "NUM ENGINES";
const char *game_str_sd_engs = "ENGINES";
const char *game_str_sd_man1 = "MANEUVER";
const char *game_str_sd_man2 = "MANEUVERABILITY";
const char *game_str_sd_class = "CLASS";
const char *game_str_sd_speed = "SPEED";
const char *game_str_sd_max = "MAX";
const char *game_str_sd_weapname = "WEAPON NAME";
const char *game_str_sd_descr = "DESCRIPTION";
const char *game_str_sd_dmg = "DMG";
const char *game_str_sd_weaps = "WEAPONS";
const char *game_str_sd_specname = "SPECIAL NAME";
const char *game_str_sd_specs = "SPECIAL DEVICES";

const char *game_str_sd_wgroup_names[WEAPON_GROUP_NUM] = {
    "WEAPONS", "BEAMS", "KINETIC", "BOMBS", "MISSILES", "BIOLOGICAL", "SPECIAL"
};

const char *game_str_sp_only6 = "Only 6 ships may be commisioned at one time. 1/4 the decomissioned ship's cost is placed in the planetary reserve.";
const char *game_str_sp_wantscrap = "Do you want to scrap this ship?";
const char *game_str_sp_before = "Before a new design can be created, you must first scrap one of the six current designs.";
const char *game_str_sp_cost = "Cost";

const char *game_str_fl_station = "STATION";
const char *game_str_fl_inorbit = "IN ORBIT";
const char *game_str_fl_moving = "MOVING TO";
const char *game_str_fl_unknown = "UNKNOWN";
const char *game_str_fl_system = "SYSTEM";

const char *game_str_gm_tchar = "TJOASDMBUEIPRN";
const char *game_str_tbl_gm_spec[PLANET_SPECIAL_NUM] = {
    "U POOR", "POOR", "", "ARTIFACTS", "RICH", "U RICH", "ORION"
};
const char *game_str_gm_unable = "Unable to land on";
const char *game_str_gm_prod = "PROD";
const char *game_str_gm_tech = "TECH";
const char *game_str_gm_1_3 = "1/3";
const char *game_str_gm_1_2 = "1/2";
const char *game_str_gm_2x = "2$ ";
const char *game_str_gm_3x = "3$ ";
const char *game_str_gm_4x = "4$ ";
const char *game_str_gm_prodb1 = "PRODUCTION BONUSES";
const char *game_str_gm_prodb2 = "APPLY TO INDUSTRY,";
const char *game_str_gm_prodb3 = "SHIPS AND DEFENSE";
const char *game_str_gm_gmap = "Galaxy Map";
const char *game_str_gm_mapkey = "Map Key";

const char *game_str_bs_line1 = "How many missile";
const char *game_str_bs_line2 = "bases to eliminate?";
const char *game_str_bs_bases = "Bases";

const char *game_str_tbl_te_field[TECH_FIELD_NUM] = {
    "Computer", "Construction", "Force Field", "Planetology", "Propulsion", "Weapon"
};
const char *game_str_te_adv = "Advanced";
const char *game_str_te_tech = "Tech";
const char *game_str_te_techno = "Technology";
const char *game_str_te_techno2 = "technology";
const char *game_str_te_genimp = "General improvements of existing";
const char *game_str_te_nmis = "Missiles tipped with nuclear warheads that explode for 4 points of damage and move at a speed of 2.";
const char *game_str_te_nbomb = "Bombs that explode for 3-12 points of damage on ground targets only.";
const char *game_str_te_scrange = "SCANNER RANGE";
const char *game_str_te_rctrl = "Robot Controls";
const char *game_str_te_col = "col";
const char *game_str_te_fwaste = "FACTORY WASTE";
const char *game_str_te_gcombat = "GROUND COMBAT";
const char *game_str_te_tform = "TERRAFORM";
const char *game_str_te_wasteel = "WASTE ELIMINATION";
const char *game_str_te_shrange = "SHIP RANGE";
const char *game_str_te_max = "MAX";
const char *game_str_te_rp = "RP";

const char *game_str_nt_achieve = "Scientists Achieve A";
const char *game_str_nt_break = "Breakthrough";
const char *game_str_nt_infil = "Spies Infiltrate The Research Center At";
const char *game_str_nt_ruins = "Troopers At The Ruins Of";
const char *game_str_nt_discover = "Discover";
const char *game_str_nt_orion = "Troopers Landing on Orion Discover";
const char *game_str_nt_scouts = "Scouts Exploring The Ruins Of";
const char *game_str_nt_choose = "Choose the area of research our scientists now focus on";
const char *game_str_nt_reveal = "Scientists Reveal Their";
const char *game_str_nt_secrets = "Secrets";
const char *game_str_nt_frame = "Your spies managed to frame another race for the theft";
const char *game_str_nt_victim = "Choose the victim race:";
const char *game_str_nt_doyou = "Do you want to ";
const char *game_str_nt_inc = "increase the ";
const char *game_str_nt_redueco = "reduce the ecology ratio of all of your colonies to the minimum amount necessary to keep them clean?";
const char *game_str_nt_ind = "industry ratios of all of your colonies to upgrade your factory controls?";
const char *game_str_nt_ecoall = "ecology ratios of all of your colonies";
const char *game_str_nt_terra = " in order to begin terraforming your planets?";
const char *game_str_nt_def = "defense ratio of all of your colonies to build the new planetary shields?";
const char *game_str_nt_ecostd = "ecology ratio of your colonies with standard environments";
const char *game_str_nt_ecohost = "ecology ratio of your colonies with hostile environments";
const char *game_str_tbl_nt_adj[4] = {
    "NO", "+25%", "+50%", "+75%"
};
const char *game_str_adj_set = "set the ";
const char *game_str_adj_ratios = " ratios of all of your colonies?";
const char *game_str_adj_max = " ratios of all of your colonies to maximum?";
const char *game_str_adj_slider[PLANET_SLIDER_NUM] = {
    "ship", "defense", "industry", "ecology", "technology"
};
const char *game_str_adj_colonies = " colonies?";
const char *game_str_adj_lock = " slider lock for all of your colonies?";
const char *game_str_adj_lock2 = " slider lock for ";
const char *game_str_adj_special[7] = {
    "ultra poor", "poor", "normal", "artifacts", "rich", "ultra rich", "4x tech"
};

const char *game_str_ra_nocont = "No Contact";
const char *game_str_ra_notpres = "Not Present";
const char *game_str_ra_secline1 = "SECURITY INCREASES THE CHANCE OF";
const char *game_str_ra_secline2 = "CATCHING ALL ENEMY SPIES. ";
const char *game_str_ra_alloc = "Allocations";
const char *game_str_ra_planres = "Planetary Resources";
const char *game_str_ra_diplo = "Diplomat";
const char *game_str_ra_gone = "Gone";
const char *game_str_ra_nospies = "NO SPIES";
const char *game_str_ra_spy = "SPY";
const char *game_str_ra_spies = "SPIES";
const char *game_str_tbl_ra_treaty[TREATY_NUM] = {
    "No Treaty", "Non-Aggression Pact", "Alliance", "War", "Final War"
};
const char *game_str_ra_trade = "Trade";
const char *game_str_ra_notrade = "No Trade";
const char *game_str_tbl_ra_relat[17] = {
    "FEUD", "HATE", "DISCORD", "TROUBLED", "TENSE", "RESTLESS", "WARY", "UNEASE",
    "NEUTRAL", "RELAXED", "AMIABLE", "CALM", "AFFABLE", "PEACEFUL", "FRIENDLY", "UNITY",
    "HARMONY"
};
const char *game_str_ra_stats = "Racial Stats";

const char *game_str_re_reportis = "REPORT IS";
const char *game_str_re_current = "CURRENT";
const char *game_str_re_yearsold = "years old";
const char *game_str_re_alliance = "Alliances";
const char *game_str_re_wars = "Wars";
const char *game_str_re_environ = "ENVIRON";

const char *game_str_sc_caught = "Spies Caught  Yours  Theirs";

const char *game_str_bp_scombat = "Space Combat";
const char *game_str_bp_attack = "attack";
const char *game_str_bp_attacks = "attacks";
const char *game_str_bp_won = "won";
const char *game_str_bt_auto_move = "AUTO MOVE";
const char *game_str_bt_pop = "POPULATION";
const char *game_str_bt_ind = "INDUSTRY";
const char *game_str_bt_bases = "MISSILE BASES";
const char *game_str_bt_subint = "SUBSPACE INTERDICTOR";
const char *game_str_bt_launch = "LAUNCHERS";
const char *game_str_bt_coldest = "Colony Was Destroyed!";

const char *game_str_es_youresp1 = "YOUR SPIES HAVE INFILTRATED A";
const char *game_str_es_youresp2 = "BASE";
const char *game_str_es_youresp3 = "CHOOSE THE TYPE OF TECHNOLOGY TO STEAL";
const char *game_str_es_thesp1 = "Espionage";
const char *game_str_es_thesp2 = "spies steal the plans for:";
const char *game_str_es_unkn = "Unknown";

const char *game_str_sb_choose = "Choose target for sabotage";
const char *game_str_sb_lastrep = "Last Report:";
const char *game_str_sb_pop = "Population:";
const char *game_str_sb_fact = "Factories:";
const char *game_str_sb_bases = "Missile Bases:";
const char *game_str_sb_unkn = "Unknown spy";
const char *game_str_sb_your = "Your";
const char *game_str_sb_spies = "spies";
const char *game_str_sb_increv = "incited a revolt!";
const char *game_str_sb_inc1 = "incited";
const char *game_str_sb_inc2 = "rebels. Unrest now at";
const char *game_str_sb_destr = "destroyed";
const char *game_str_sb_fact2 = "factory";
const char *game_str_sb_facts = "factories";
const char *game_str_sb_mbase = "missile base";
const char *game_str_sb_mbases = "missile bases";
const char *game_str_sb_failed = "failed!";
const char *game_str_sb_nofact = "No factories to sabotage";
const char *game_str_sb_nobases = "No missile bases to sabotage";
const char *game_str_sb_noinc = "failed to incite any rebels";
const char *game_str_sb_frame = "Your spies managed to frame another race for the sabotage";

const char *game_str_ex_planeta = "Planetary";
const char *game_str_ex_scanner = "scanners";
const char *game_str_ex_scout = "Scout ships";
const char *game_str_ex_explore = "explore a new";
const char *game_str_ex_starsys = "star system";
const char *game_str_ex_build = "Build a";
const char *game_str_ex_colony = "new colony?";
const char *game_str_ex_popgr = "POPULATION GROWTH";
const char *game_str_ex_resopnt = "RESOURCE POINTS";
const char *game_str_ex_fromind = "FROM INDUSTRY ARE";
const char *game_str_ex_techpnt = "TECHNOLOGY POINTS";
const char *game_str_ex_fromres = "FROM RESEARCH";
const char *game_str_ex_aredbl = "ARE DOUBLED.";
const char *game_str_ex_arequad = "ARE QUADRUPLED.";
const char *game_str_ex_pg1[3] = {
    "Hostile", "Ecologicaly", "Ecological"
};
const char *game_str_ex_pg2[3] = {
    "Environment", "Fertile", "Gaia"
};
const char *game_str_ex_pg3[3] = {
    "IS HALVED.", "IS +50% NORMAL.", "IS DOUBLED."
};
const char *game_str_ex_ps1[5] = {
    "Ultra Poor", "Mineral Poor", "Artifacts", "Mineral Rich", "Ultra Rich"
};
const char *game_str_ex_ps2[4] = {
    "CUT TO ONE-THIRD.", "HALVED.", "DOUBLED.", "TRIPLED."
};

const char *game_str_la_colony = "Colony Name...";
const char *game_str_la_inyear = "In the year";
const char *game_str_la_the = "the";
const char *game_str_la_formnew = "s form a new colony";

const char *game_str_gr_carmor = "Combat Armor";
const char *game_str_gr_outof = "out of";
const char *game_str_gr_transs = "transports";
const char *game_str_gr_reclaim = "transports land to reclaim the colony";
const char *game_str_gr_penetr = "penetrate";
const char *game_str_gr_defenss = "defenses";
const char *game_str_gr_troops = "Troops";
const char *game_str_gr_rebel = "Rebel";
const char *game_str_gr_gcon = "Ground Combat On";
const char *game_str_gr_scapt = "s Capture";
const char *game_str_gr_itroops = "Imperial Troops Recapture";
const char *game_str_gr_succd = "s Successfully Defend";
const char *game_str_gr_fcapt = "factories captured";
const char *game_str_gr_tsteal = "technology stolen";
const char *game_str_gr_tnew = "new tech found";

const char *game_str_el_no = "no";
const char *game_str_el_vote = "vote";
const char *game_str_el_votes = "votes";
const char *game_str_el_total = "total";
const char *game_str_el_start = "The High Council has convened to elect one leader to be Emperor of the Galaxy...";
const char *game_str_el_emperor = "Emperor";
const char *game_str_el_ofthe = "of the";
const char *game_str_el_and = "and";
const char *game_str_el_for = "for";
const char *game_str_el_nomin = "have been nominated.";
const char *game_str_el_abs1 = "The";
const char *game_str_el_abs2 = "abstain (";
const char *game_str_el_dots = ")...";
const char *game_str_el_your = "Your choice (";
const char *game_str_el_bull = "[";
const char *game_str_el_self = "Yourself";
const char *game_str_el_abs = "Abstain";
const char *game_str_el_chose1 = "In the year";
const char *game_str_el_chose2 = ", the Council has chosen";
const char *game_str_el_chose3 = "to be the High Master of the New Republic.";
const char *game_str_el_neither = "Neither leader has a two thirds majority...";
const char *game_str_el_accept = "Do you accept the ruling?";
const char *game_str_el_yes = "Yes";
const char *game_str_el_no2 = "No";
const char *game_str_el_sobeit = "So be it. You defy the ruling of the council. Now you will feel the wrath of the New Republic!";
const char *game_str_el_isnow = "is now High Master.";

const char *game_str_au_facts = "factories";
const char *game_str_au_bases = "missile bases";
const char *game_str_au_treaty = "treaty";
const char *game_str_au_allian = "Alliance";
const char *game_str_au_nonagg = "Non-Aggression Pact";
const char *game_str_au_tradea = "Trade Agreement";
const char *game_str_au_amreca = "(ambassador recalled)";
const char *game_str_au_tech = "tech";
const char *game_str_au_framed = "(you were framed)";
const char *game_str_au_bull = "[";
const char *game_str_au_inxchng = "In exchange you will receive:";
const char *game_str_au_whatif1 = "What if we were to also offer";
const char *game_str_au_whatif2 = "as an incentive";
const char *game_str_au_perrec1 = "Perhaps you would reconsider if we also provided";
const char *game_str_au_ques = "?";
const char *game_str_au_howmay = "How may our empire serve you:";
const char *game_str_au_youprte = "You propose a treaty:";
const char *game_str_au_youprta = "You propose a trade agreement for:";
const char *game_str_au_youract = "Your actions:";
const char *game_str_au_whatech = "What type of technology interests you?";
const char *game_str_au_whatrad = "What will you trade for it?";
const char *game_str_au_whatoff = "What do you offer as tribute?";
const char *game_str_au_perthr1 = "Perhaps if you were to throw in";
const char *game_str_au_perthr2 = "we could deal.";
const char *game_str_au_alsoof1 = "If you also offer us";
const char *game_str_au_alsoof2 = "we would accept.";
const char *game_str_au_whowar = "Who should we declare war on?";
const char *game_str_au_whobrk = "Who should we break our treaty with?";
const char *game_str_au_bcpery = "BC / year";
const char *game_str_au_whattr = "What do you offer as tribute?";
const char *game_str_au_techn = "[ Technology";
const char *game_str_au_nextp = "[ Next page";
const char *game_str_au_back = "[ Back";

const char *game_str_au_opts_main[6] = {
    "[ Propose Treaty",
    "[ Form Trade Agreement",
    "[ Threaten/Break Treaty and Trade",
    "[ Offer Tribute",
    "[ Exchange Technology",
    "[ Good Bye"
};
const char *game_str_au_opts_treaty[6] = {
    "[ Non-Aggression Pact",
    "[ Alliance",
    "[ Peace Treaty",
    "[ Declaration of War on Another Race",
    "[ Break Alliance With Another Race",
    "[ Forget It"
};
const char *game_str_au_opts_agree[2] = {
    "[ Agree",
    "[ Forget It"
};
const char *game_str_au_opts_accept[2] = {
    "[ Accept",
    "[ Reject"
};
const char *game_str_au_opts_threaten[5] = {
    "[ Break Non-Aggression Pact",
    "[ Break Alliance",
    "[ Break Trade Agreement",
    "[ Threaten To Attack",
    "[ Forget It"
};
const char *game_str_au_optsmp1[4] = {
    "[ Agree",
    "[ Forget It",
    "[ Demand BC",
    "[ Demand Technology"
};

const char *game_str_tr_cont1 = "Contact has been broken with the";
const char *game_str_tr_cont2 = "empire!";
const char *game_str_tr_fuel1 = "The fleet orbiting";
const char *game_str_tr_fuel2 = "has been cut off from refueling supply lines and has been lost.";

const char *game_str_sv_envir = "environment";
const char *game_str_sv_stargt = "Star Gate";
const char *game_str_sv_shild1 = "CLASS";
const char *game_str_sv_shild2 = "SHIELD";
const char *game_str_sv_psize = "PLANET SIZE:";
const char *game_str_sv_fact = "FACTORIES:";
const char *game_str_sv_waste = "WASTE:";
const char *game_str_sv_pop = "POPULATION:";
const char *game_str_sv_growth = "GROWTH:";
const char *game_str_sv_techp = "Technology points from research are";
const char *game_str_sv_resp = "Resource points from industry are";
const char *game_str_sv_1_3x = "cut to one-third.";
const char *game_str_sv_1_2x = "halved.";
const char *game_str_sv_2x = "doubled.";
const char *game_str_sv_3x = "tripled.";
const char *game_str_sv_4x = "quadrupled.";
const char *game_str_sv_popgr = "Population growth is";
const char *game_str_sv_pg1[3] = {
    "Hostile", "Fertile", "Gaia"
};
const char *game_str_sv_pg2[3] = {
    "halved.", "50% greater.", "doubled."
};

const char *game_str_in_loading = "Loading Master of Orion...";
const char *game_str_wl_won_1 = "Escorted by Honor Guard, High Master ";
const char *game_str_wl_won_2 = "returns to Orion, throne world of the Ancients.";
const char *game_str_wl_won_3 = "The Galactic Imperium has been reformed...";
const char *game_str_wl_3_good_1 = "A new era has dawned. We must set aside our past";
const char *game_str_wl_3_good_2 = "conflicts and greet a new millenium as a united galaxy";
const char *game_str_wl_3_tyrant_1 = "The universe is mine and all shall bow before the";
const char *game_str_wl_3_tyrant_2 = "might of ";
const char *game_str_wl_3_tyrant_3 = ", Master of Orion.";
const char *game_str_wl_3_tyrant_4 = "Master of the Universe...";
const char *game_str_wl_exile_1 = "Exiled from the known galaxy, Emperor ";
const char *game_str_wl_exile_2 = "sets forth to conquer new worlds.";
const char *game_str_wl_exile_3 = "vowing to return and claim the renowned title of";
const char *game_str_wl_exile_4 = " Master of Orion...";

const char *game_str_gnn_end_good = "And that's the way it is...";
const char *game_str_gnn_end_tyrant = "Oh well, another millenium serving under a ruthless tyrant...";
const char *game_str_gnn_also = "Also in the news...";
