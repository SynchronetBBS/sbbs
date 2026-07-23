// Yankee Trader calculators converted from ytcalc.xls and yt.starflt.com.

class YTCalc {
  static plasmaPercent(distance) {
    if (distance == null || distance < 1) return 0
    return (100 - (2 * (distance - 1))).max(0).round
  }

  static plasmaDamage(bolts, targetMultiplier, distance) {
    if (bolts == null || bolts < 0 || targetMultiplier == null) return 0
    return (bolts * bolts * targetMultiplier *
        plasmaPercent(distance) / 100).round
  }

  static plasmaDamageTable(bolts, distance) {
    return {
      "fighters": plasmaDamage(bolts, 50000, distance),
      "shields": plasmaDamage(bolts, 25000, distance),
      "mines": plasmaDamage(bolts, 250, distance),
      "groundForces": plasmaDamage(bolts, 1000, distance),
      "productivity1": plasmaDamage(bolts, 1000, distance),
      "productivity2": plasmaDamage(bolts, 2000, distance),
      "productivity3": plasmaDamage(bolts, 3000, distance)
    }
  }

  // Spreadsheet behavior: each resource layer is treated as another firing
  // stage, so the square roots are added rather than the damage totals.
  static plasmaBoltsNeeded(fighters, shields, mines, groundForces,
      productivityMax) {
    var total = rootPart_(fighters, 50000) + rootPart_(shields, 25000)
    if (productivityMax == null) {
      total = total + rootPart_(mines, 300) +
          rootPart_(groundForces, 1000)
    } else if (groundForces == null) {
      total = total + rootPart_(mines, 250) +
          rootPart_(productivityMax, 3000)
    } else {
      total = total + rootPart_(mines, 300) +
          rootPart_(groundForces, 1000).min(
              rootPart_(productivityMax, 3000))
    }
    return total.ceil
  }

  static playerPlasmaBolts(fighters, shields) {
    return (((fighters.max(0)) + (2 * shields.max(0))) / 50000).sqrt.ceil
  }

  static safePlasmaBolts(remaining, targetMultiplier, distance) {
    if (remaining == null || remaining <= 0 || targetMultiplier <= 0) return 0
    var fraction = plasmaPercent(distance) / 100
    if (fraction <= 0) return 0
    return ((remaining - 1) / (targetMultiplier * fraction)).sqrt.floor
  }

  static missileDamage(missiles) {
    var m = missiles.max(0)
    return {
      "fightersMin": m * 2450,
      "fightersMax": m * 2550,
      "shields": m * 1150,
      "mines": m,
      "groundForces": m * 12.5,
      "productivityMin": m * 1000,
      "productivityMax": m * 3000
    }
  }

  static missilesNeeded(fightersMin, fightersMax, shields, mines,
      groundForces, productivityMax) {
    return (part_(fightersMin, 2450) + part_(fightersMax, 2550) +
        part_(shields, 1150) + part_(mines, 1) +
        part_(groundForces, 12.5) + part_(productivityMax, 3000)).ceil
  }

  static productivityCredits(fightersPerDay) {
    if (fightersPerDay < 0 || fightersPerDay > 99999) return null
    return (((100000 - fightersPerDay) * 250 / 3) / 250).ceil * 250
  }

  static planetProduction(ore, organics, equipment, bankedCredits,
      spentCredits, currentForces) {
    var cargo = ore + organics + equipment
    var spend = spentCredits / 250
    return {
      "ore": 1 + ore / 10 + bankedCredits / 10000 + spend,
      "organics": 1 + organics / 10 + bankedCredits / 20000 + spend,
      "equipment": 1 + equipment / 10 + bankedCredits / 30000 + spend,
      "fighters": 3 + bankedCredits / 500 + cargo / 10 + spend * 3,
      "missiles": bankedCredits / 100000 +
          (cargo / 25000 + spend * 3 / 2500).floor,
      "mines": bankedCredits / 250000 +
          (cargo / 250000 + spend * 3 / 25000).floor,
      "credits": bankedCredits / 100,
      "groundForces": bankedCredits / 10000 +
          ((currentForces - 1) / 100).floor,
      "plasmaBolts": (bankedCredits / 25000000).floor +
          (cargo / 1000000 + spentCredits * 3 / 25000000).floor,
      "totalCredits": bankedCredits + spentCredits
    }
  }

  static planetBankEstimate(currentForces, increasePerMinute) {
    var perDay = (increasePerMinute * 1440).ceil - currentForces / 100
    return {
      "groundForcesPerDay": perDay,
      "millionCredits": perDay / 100
    }
  }

  static xannorSafeForces(score) { score.max(0) / 50000 }

  static xannorPlasmaBolts(fighters) {
    var value = fighters / 50000 - 2
    if (value <= 0) return 0
    return value.sqrt.floor
  }

  static xannoronHoldAttack(topScore, currentXannor) {
    return currentXannor.max(0) + topScore.max(0) / 500
  }

  static mercenaryBribe(fighters) { (fighters.max(0) * 2.5).ceil }

  static surrender(attacking, defending) {
    if (defending <= 0) {
      return {"ratio": 0, "surrendering": 0, "remaining": 0}
    }
    var ratio = attacking / defending
    var count
    if (ratio < 1) {
      count = 0
    } else if (ratio > 10) {
      count = defending
    } else {
      count = defending * (0.05 + 0.09 * ratio)
    }
    return {
      "ratio": ratio,
      "surrendering": count,
      "remaining": defending - count
    }
  }

  static portPairProfit(holds, profitPerHold, roundTrips) {
    return {
      "credits": holds.max(0) * profitPerHold.max(0) * roundTrips.max(0),
      "turns": roundTrips.max(0) * 4
    }
  }

  static rootPart_(value, divisor) {
    if (value == null || value <= 0) return 0
    return (value / divisor).sqrt
  }

  static part_(value, divisor) {
    if (value == null || value <= 0) return 0
    return value / divisor
  }
}
