/**
 * RacerDriver - AI driver for race opponents.
 * 
 * Unlike CommuterDriver which just creates slow traffic,
 * RacerDriver actively competes in the race with realistic racing behavior.
 */

class RacerDriver implements IDriver {
  /** Skill level (0-1), affects speed, reactions, and mistakes */
  private skill: number;
  
  /** Target speed as fraction of max (based on skill) */
  private targetSpeed: number;
  
  /** Current steering intent */
  private steerAmount: number;
  
  /** Racer name for display */
  name: string;
  
  /** Racer's preferred racing line offset (-1 to 1) */
  private preferredLine: number;
  
  /** How aggressively this racer defends position (reserved for future) */
  private _aggression: number;
  
  /** Reaction time delay (lower skill = slower reactions, reserved for future) */
  private _reactionDelay: number;
  
  /** Random variation timer for natural-feeling driving */
  private variationTimer: number;
  
  /** Current speed variation */
  private speedVariation: number;
  
  /** Whether this driver is allowed to move (false during countdown) */
  private canMove: boolean;
  
  /** Timer for item usage cooldown - prevents spamming */
  private itemUseCooldown: number;

  constructor(skill: number, name?: string) {
    this.skill = clamp(skill, 0.3, 1.0);
    this.name = name || this.generateName();
    
    // Calculate attributes based on skill
    // Higher skill = faster, more consistent, quicker reactions
    // CPU racers now match player speed (100%) - tuning baseline
    this.targetSpeed = 0.90 + (this.skill * 0.10);  // 0.90 to 1.00 of max speed (270-300)
    this._aggression = 0.3 + (this.skill * 0.5);   // How much they fight for position
    this._reactionDelay = 0.3 - (this.skill * 0.25); // 0.05 to 0.30 seconds
    
    // Preferred racing line varies by racer
    this.preferredLine = (Math.random() - 0.5) * 0.6;  // -0.3 to 0.3
    
    this.steerAmount = 0;
    this.variationTimer = 0;
    this.speedVariation = 0;
    this.canMove = false;  // Start frozen, will be enabled when race starts
    this.itemUseCooldown = 0;
  }
  
  /**
   * Set whether the driver can move (used for countdown blocking).
   */
  setCanMove(canMove: boolean): void {
    this.canMove = canMove;
  }
  
  /**
   * Generate a random racer name.
   */
  private generateName(): string {
    var firstNames = [
      'Max', 'Alex', 'Sam', 'Jordan', 'Casey', 'Morgan', 'Taylor', 'Riley',
      'Ace', 'Blaze', 'Storm', 'Flash', 'Turbo', 'Nitro', 'Dash', 'Spike'
    ];
    var lastNames = [
      'Speed', 'Racer', 'Driver', 'Storm', 'Thunder', 'Blitz', 'Volt', 'Dash',
      'Rocket', 'Jet', 'Zoom', 'Rush', 'Gear', 'Torque', 'Drift', 'Burn'
    ];
    var firstName = firstNames[Math.floor(Math.random() * firstNames.length)];
    var lastName = lastNames[Math.floor(Math.random() * lastNames.length)];
    return firstName + ' ' + lastName;
  }

  /**
   * Update the AI driver.
   */
  update(vehicle: IVehicle, _track: ITrack, dt: number): DriverIntent {
    // Block all movement during countdown
    if (!this.canMove) {
      return {
        accelerate: 0,
        steer: 0,
        useItem: false
      };
    }
    
    // Update item cooldown
    if (this.itemUseCooldown > 0) {
      this.itemUseCooldown -= dt;
    }
    
    // Update variation timer for natural-feeling speed changes
    this.variationTimer += dt;
    if (this.variationTimer > 2 + Math.random() * 2) {
      this.variationTimer = 0;
      // Slight random speed variations (more for lower skill)
      this.speedVariation = (Math.random() - 0.5) * 0.1 * (1 - this.skill);
    }
    
    // Calculate target max speed for this AI
    var maxSpeedForAI = (this.targetSpeed + this.speedVariation) * VEHICLE_PHYSICS.MAX_SPEED;
    
    // Accelerate if below target, coast/brake if above
    var accelerate: number;
    if (vehicle.speed < maxSpeedForAI * 0.95) {
      accelerate = 1;  // Full gas
    } else if (vehicle.speed > maxSpeedForAI * 1.05) {
      accelerate = -0.3;  // Light braking
    } else {
      accelerate = 0;  // Coast to maintain speed
    }
    
    // Slow down if crashed recently
    if (vehicle.isCrashed) {
      accelerate = 0.3;
    }
    
    // Calculate steering - try to stay on preferred racing line
    var currentX = vehicle.playerX || 0;
    var targetX = this.preferredLine;
    
    // Steer toward target line
    var lineDiff = targetX - currentX;
    this.steerAmount = lineDiff * (0.5 + this.skill * 0.5);
    
    // Clamp steering
    this.steerAmount = clamp(this.steerAmount, -0.8, 0.8);
    
    // Add small random wobble (less for higher skill)
    var wobble = (Math.random() - 0.5) * 0.1 * (1 - this.skill);
    this.steerAmount += wobble;
    
    // Keep on road - steer away from edges
    if (currentX < -0.8) {
      this.steerAmount = 0.5;
    } else if (currentX > 0.8) {
      this.steerAmount = -0.5;
    }
    
    // AI Item Usage:
    // TODO: Improve item usage logic when tuning game (consider position, item type, etc.)
    // For now, use simple RNG: ~5% chance per second to use held item
    var shouldUseItem = false;
    if (vehicle.heldItem !== null && this.itemUseCooldown <= 0) {
      // Higher skill = slightly more strategic (uses items more often when behind)
      var useChance = 0.05 * dt;  // ~5% per second base chance
      
      // Boost chance based on position (trailing racers use items more)
      if (vehicle.racePosition > 2) {
        useChance *= 1.5;
      }
      
      if (Math.random() < useChance) {
        shouldUseItem = true;
        this.itemUseCooldown = 2 + Math.random() * 3;  // 2-5 second cooldown
      }
    }
    
    return {
      accelerate: clamp(accelerate, 0, 1),
      steer: this.steerAmount,
      useItem: shouldUseItem
    };
  }
  
  /**
   * Get skill level.
   */
  getSkill(): number {
    return this.skill;
  }
  
  /**
   * Get aggression level (for future overtaking AI).
   */
  getAggression(): number {
    return this._aggression;
  }
  
  /**
   * Get reaction delay (for future dynamic response AI).
   */
  getReactionDelay(): number {
    return this._reactionDelay;
  }
}
