/**
 * Controls - Maintains current input state for BBS single-key input.
 *
 * BBS LIMITATION: Terminals only send one key at a time, no simultaneous keys.
 * We handle this with combined actions (ACCEL_LEFT = accelerate + left).
 * 
 * We also implement "cruise control" behavior:
 * - Vehicle maintains speed when no input (coasting)
 * - Accelerate increases speed
 * - Brake decreases speed
 * - Steering is applied whenever a turn key is pressed
 */

class Controls {
  private inputMap: InputMap;
  private lastKeyTime: { [action: number]: number };
  private activeActions: { [action: number]: boolean };
  private justPressedActions: { [action: number]: boolean };
  private holdThreshold: number;

  // Current frame's intent (derived from all active actions)
  private currentAccel: number;
  private currentSteer: number;
  
  // Track last accel/decel action for determining shell fire direction
  // 1 = last was accelerate, -1 = last was brake/decelerate
  private lastAccelAction: number;

  constructor(inputMap: InputMap) {
    this.inputMap = inputMap;
    this.lastKeyTime = {};
    this.activeActions = {};
    this.justPressedActions = {};
    this.holdThreshold = 200; // ms before action is released (slightly longer for BBS latency)
    this.currentAccel = 0;
    this.currentSteer = 0;
    this.lastAccelAction = 1;  // Default to forward (accelerate)
  }

  /**
   * Process a raw key press.
   */
  handleKey(key: string, now: number): void {
    var action = this.inputMap.getAction(key);
    if (action !== GameAction.NONE) {
      // Track if this is a new press
      if (!this.activeActions[action]) {
        this.justPressedActions[action] = true;
      }

      this.activeActions[action] = true;
      this.lastKeyTime[action] = now;

      // Immediately derive intent from this action
      this.applyAction(action);
    }
  }

  /**
   * Apply an action to the current intent.
   */
  private applyAction(action: GameAction): void {
    switch (action) {
      // Pure actions
      case GameAction.ACCELERATE:
        this.currentAccel = 1;
        this.lastAccelAction = 1;  // Track: accelerate was pressed
        break;
      case GameAction.BRAKE:
        this.currentAccel = -1;
        this.lastAccelAction = -1;  // Track: brake was pressed
        break;
      case GameAction.STEER_LEFT:
        this.currentSteer = -1;
        break;
      case GameAction.STEER_RIGHT:
        this.currentSteer = 1;
        break;

      // Combined actions
      case GameAction.ACCEL_LEFT:
        this.currentAccel = 1;
        this.currentSteer = -1;
        this.lastAccelAction = 1;  // Track: accelerate was pressed
        break;
      case GameAction.ACCEL_RIGHT:
        this.currentAccel = 1;
        this.currentSteer = 1;
        this.lastAccelAction = 1;  // Track: accelerate was pressed
        break;
      case GameAction.BRAKE_LEFT:
        this.currentAccel = -1;
        this.currentSteer = -1;
        this.lastAccelAction = -1;  // Track: brake was pressed
        break;
      case GameAction.BRAKE_RIGHT:
        this.currentAccel = -1;
        this.currentSteer = 1;
        this.lastAccelAction = -1;  // Track: brake was pressed
        break;
    }
  }

  /**
   * Update held state - release stale actions.
   * 
   * CRUISE CONTROL: We only clear steering each frame.
   * Acceleration persists until explicitly changed (brake pressed).
   * This means: press Z to accelerate, release, then just steer with q/e.
   */
  update(now: number): void {
    // Only clear steering - acceleration persists (cruise control)
    this.currentSteer = 0;

    // Check which actions are still active
    for (var actionStr in this.lastKeyTime) {
      var action = parseInt(actionStr, 10);
      if (now - this.lastKeyTime[action] > this.holdThreshold) {
        this.activeActions[action] = false;
        delete this.lastKeyTime[action];
        
        // When an accel/brake action expires, reset accel to 0 (coast)
        if (action === GameAction.ACCELERATE || action === GameAction.BRAKE ||
            action === GameAction.ACCEL_LEFT || action === GameAction.ACCEL_RIGHT ||
            action === GameAction.BRAKE_LEFT || action === GameAction.BRAKE_RIGHT) {
          this.currentAccel = 0;
        }
      } else if (this.activeActions[action]) {
        // Re-apply still-active actions to intent
        this.applyAction(action);
      }
    }
  }

  /**
   * Get current acceleration intent (-1 = brake, 0 = coast, 1 = accelerate).
   */
  getAcceleration(): number {
    return this.currentAccel;
  }

  /**
   * Get current steering intent (-1 = left, 0 = straight, 1 = right).
   */
  getSteering(): number {
    return this.currentSteer;
  }

  /**
   * Check if action is currently active (held).
   */
  isActive(action: GameAction): boolean {
    return this.activeActions[action] === true;
  }

  /**
   * Check if action was just pressed this frame.
   */
  wasJustPressed(action: GameAction): boolean {
    return this.justPressedActions[action] === true;
  }

  /**
   * Consume a just-pressed action (prevents processing multiple times per frame).
   * Returns true if the action was just pressed and is now consumed.
   */
  consumeJustPressed(action: GameAction): boolean {
    if (this.justPressedActions[action] === true) {
      this.justPressedActions[action] = false;
      return true;
    }
    return false;
  }

  /**
   * Clear per-frame state (call at end of frame).
   */
  endFrame(): void {
    this.justPressedActions = {};
  }

  /**
   * Clear all input state.
   */
  clearAll(): void {
    this.activeActions = {};
    this.justPressedActions = {};
    this.lastKeyTime = {};
  }
  
  /**
   * Get the last acceleration/deceleration action.
   * Returns 1 if last was accelerate, -1 if last was brake.
   * Used to determine if shells should fire forward or backward.
   */
  getLastAccelAction(): number {
    return this.lastAccelAction;
  }
}
