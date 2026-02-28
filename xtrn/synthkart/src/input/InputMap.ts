/**
 * Game actions that can be triggered by input.
 * 
 * BBS TERMINAL INPUT LIMITATION:
 * Terminals can only send one key at a time - no simultaneous key detection.
 * We use combined actions (ACCEL_LEFT = accelerate + turn left) to work around this.
 */
enum GameAction {
  NONE = 0,
  
  // Pure actions
  ACCELERATE,       // Speed up, no turn
  BRAKE,            // Slow down, no turn
  STEER_LEFT,       // Turn left, coast (maintain speed)
  STEER_RIGHT,      // Turn right, coast
  
  // Combined actions (for single-key input)
  ACCEL_LEFT,       // Accelerate + turn left
  ACCEL_RIGHT,      // Accelerate + turn right
  BRAKE_LEFT,       // Brake + turn left
  BRAKE_RIGHT,      // Brake + turn right
  
  // Other
  USE_ITEM,
  PAUSE,
  QUIT
}

/**
 * Maps raw keyboard input to game actions.
 * 
 * CONTROL SCHEME:
 * 
 * ARROW KEYS:
 *   Up    = Accelerate straight
 *   Down  = Brake
 *   Left  = Turn left (cruise)
 *   Right = Turn right (cruise)
 * 
 * NUMPAD:
 *   7 = Accelerate + Left    8 = Accelerate    9 = Accelerate + Right
 *   4 = Turn Left (cruise)   5 = Brake         6 = Turn Right (cruise)
 *   1 = Brake + Left         2 = Brake         3 = Brake + Right
 * 
 * OTHER:
 *   Space = Use Item
 *   P / Enter = Pause
 *   Q / X / Escape = Quit
 */
class InputMap {
  private bindings: { [key: string]: GameAction };

  constructor() {
    this.bindings = {};
    this.setupDefaultBindings();
  }

  private setupDefaultBindings(): void {
    // === ARROW KEYS ===
    this.bind(KEY_UP, GameAction.ACCELERATE);    // Gas straight
    this.bind(KEY_DOWN, GameAction.BRAKE);       // Brake
    this.bind(KEY_LEFT, GameAction.STEER_LEFT);  // Turn left (cruise)
    this.bind(KEY_RIGHT, GameAction.STEER_RIGHT);// Turn right (cruise)

    // === NUMPAD ===
    // Top row: accelerate + direction
    this.bind('7', GameAction.ACCEL_LEFT);   // Gas + left
    this.bind('8', GameAction.ACCELERATE);   // Gas straight
    this.bind('9', GameAction.ACCEL_RIGHT);  // Gas + right
    // Middle row: cruise control steering
    this.bind('4', GameAction.STEER_LEFT);   // Turn left (cruise)
    this.bind('5', GameAction.BRAKE);        // Brake
    this.bind('6', GameAction.STEER_RIGHT);  // Turn right (cruise)
    // Bottom row: brake + direction
    this.bind('1', GameAction.BRAKE_LEFT);   // Brake + left
    this.bind('2', GameAction.BRAKE);        // Brake straight
    this.bind('3', GameAction.BRAKE_RIGHT);  // Brake + right

    // === OTHER CONTROLS ===
    this.bind(' ', GameAction.USE_ITEM);     // Space = use item
    this.bind('p', GameAction.PAUSE);        // P = pause
    this.bind('P', GameAction.PAUSE);
    this.bind('\r', GameAction.PAUSE);       // Enter = pause
    this.bind('q', GameAction.QUIT);         // Q = quit
    this.bind('Q', GameAction.QUIT);
    this.bind('x', GameAction.QUIT);         // X = quit
    this.bind('X', GameAction.QUIT);
    this.bind('\x1b', GameAction.QUIT);      // Escape = quit
  }

  /**
   * Bind a key to an action.
   */
  bind(key: string, action: GameAction): void {
    this.bindings[key] = action;
  }

  /**
   * Get action for a key.
   */
  getAction(key: string): GameAction {
    var action = this.bindings[key];
    return action !== undefined ? action : GameAction.NONE;
  }
}
