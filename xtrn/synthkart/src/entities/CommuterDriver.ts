/**
 * CommuterDriver - Simple AI driver for traffic obstacles.
 * 
 * Commuters drive at a fixed slower speed with minor random drift.
 * They don't race - they're just obstacles that create traffic.
 * 
 * Designed for extension: shares IDriver interface with HumanDriver
 * and CpuDriver, so commuters can be swapped for racers easily.
 */

class CommuterDriver implements IDriver {
  /** Speed as fraction of max (0.3 = 30% of max speed) */
  private speedFactor: number;
  
  /** How much the commuter drifts laterally */
  private driftAmount: number;
  
  /** Current drift direction (-1, 0, or 1) */
  private driftDirection: number;
  
  /** Timer for changing drift direction */
  private driftTimer: number;
  
  /** How long before drift direction changes */
  private driftInterval: number;
  
  /** Whether this commuter is active (driving) or dormant (waiting) */
  private active: boolean;
  
  /** Distance at which commuter wakes up when player approaches */
  private activationRange: number;
  
  constructor(speedFactor?: number) {
    // Commuters drive at 30-50% of max speed
    this.speedFactor = speedFactor !== undefined ? speedFactor : 0.3 + Math.random() * 0.2;
    this.driftAmount = 0.1 + Math.random() * 0.1;  // Slight lateral movement
    this.driftDirection = 0;
    this.driftTimer = 0;
    this.driftInterval = 2 + Math.random() * 3;  // Change direction every 2-5 seconds
    this.active = false;  // Start dormant, activate when player approaches
    this.activationRange = 400;  // Wake up when player is within 400 units
  }
  
  /**
   * Update commuter driver - returns simple forward motion with drift.
   * Dormant commuters don't move until player approaches.
   */
  update(vehicle: IVehicle, _track: ITrack, dt: number): DriverIntent {
    // If dormant, don't move at all
    if (!this.active) {
      return {
        accelerate: 0,
        steer: 0,
        useItem: false
      };
    }
    
    // Update drift timer
    this.driftTimer += dt;
    if (this.driftTimer >= this.driftInterval) {
      this.driftTimer = 0;
      // Randomly pick new drift direction, biased toward center
      var rand = Math.random();
      if (rand < 0.3) {
        this.driftDirection = -1;  // Drift left
      } else if (rand < 0.6) {
        this.driftDirection = 1;   // Drift right
      } else {
        this.driftDirection = 0;   // Go straight
      }
      
      // If far from center, bias drift back toward center
      if (vehicle.playerX > 0.5) {
        this.driftDirection = -1;
      } else if (vehicle.playerX < -0.5) {
        this.driftDirection = 1;
      }
    }
    
    return {
      accelerate: this.speedFactor,
      steer: this.driftDirection * this.driftAmount,
      useItem: false
    };
  }
  
  /**
   * Get the target speed factor for this commuter.
   */
  getSpeedFactor(): number {
    return this.speedFactor;
  }
  
  /**
   * Check if this commuter is active (driving) or dormant (waiting).
   */
  isActive(): boolean {
    return this.active;
  }
  
  /**
   * Activate this commuter - called when player comes within range.
   */
  activate(): void {
    this.active = true;
  }
  
  /**
   * Deactivate this commuter - returns to dormant state.
   */
  deactivate(): void {
    this.active = false;
  }
  
  /**
   * Get the activation range for this commuter.
   */
  getActivationRange(): number {
    return this.activationRange;
  }
}

