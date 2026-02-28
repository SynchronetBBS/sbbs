/**
 * Item types available in the game.
 */
enum ItemType {
  NONE = 0,
  // Single-use items
  MUSHROOM,
  GREEN_SHELL,      // Straight path
  RED_SHELL,        // Homes to next vehicle ahead
  BLUE_SHELL,       // Homes to 1st place only
  SHELL,            // Legacy - treated as RED_SHELL
  BANANA,
  // Triple (pack of 3) variants
  MUSHROOM_TRIPLE,
  GREEN_SHELL_TRIPLE,
  RED_SHELL_TRIPLE,
  SHELL_TRIPLE,     // Legacy - treated as RED_SHELL_TRIPLE
  BANANA_TRIPLE,
  // Duration-based items
  MUSHROOM_GOLDEN,  // Unlimited boosts for duration
  STAR,             // Invincibility + speed
  LIGHTNING,        // Slow all opponents
  BULLET            // Max speed + autopilot
}

/**
 * Get the base item type (strips TRIPLE/GOLDEN variants)
 */
function getBaseItemType(type: ItemType): ItemType {
  switch (type) {
    case ItemType.MUSHROOM_TRIPLE:
    case ItemType.MUSHROOM_GOLDEN:
      return ItemType.MUSHROOM;
    case ItemType.GREEN_SHELL_TRIPLE:
      return ItemType.GREEN_SHELL;
    case ItemType.RED_SHELL_TRIPLE:
    case ItemType.SHELL_TRIPLE:
    case ItemType.SHELL:
      return ItemType.RED_SHELL;
    case ItemType.BANANA_TRIPLE:
      return ItemType.BANANA;
    case ItemType.BLUE_SHELL:
      return ItemType.BLUE_SHELL;
    default:
      return type;
  }
}

/**
 * Get initial uses for an item type.
 */
function getItemUses(type: ItemType): number {
  switch (type) {
    case ItemType.MUSHROOM_TRIPLE:
    case ItemType.GREEN_SHELL_TRIPLE:
    case ItemType.RED_SHELL_TRIPLE:
    case ItemType.SHELL_TRIPLE:
    case ItemType.BANANA_TRIPLE:
      return 3;
    default:
      return 1;
  }
}

/**
 * Check if item is duration-based (applies effect over time).
 */
function isDurationItem(type: ItemType): boolean {
  switch (type) {
    case ItemType.MUSHROOM_GOLDEN:
    case ItemType.STAR:
    case ItemType.LIGHTNING:
    case ItemType.BULLET:
      return true;
    default:
      return false;
  }
}

/**
 * Get duration for duration-based items.
 */
function getItemDuration(type: ItemType): number {
  switch (type) {
    case ItemType.MUSHROOM_GOLDEN: return 8.0;
    case ItemType.STAR: return 8.0;
    case ItemType.LIGHTNING: return 5.0;
    case ItemType.BULLET: return 8.0;
    default: return 0;
  }
}

/**
 * Check if item stays in slot while effect is active (blocks new pickups).
 * Golden Mushroom, Bullet, and Ghost stay in slot until effect ends.
 */
function itemStaysInSlotWhileActive(type: ItemType): boolean {
  switch (type) {
    case ItemType.MUSHROOM_GOLDEN:
    case ItemType.BULLET:
      return true;
    default:
      return false;
  }
}

/**
 * Held item data - tracks type, uses remaining, and activation state.
 */
interface HeldItemData {
  type: ItemType;
  uses: number;      // For pack items (triple mushroom = 3 uses)
  activated: boolean; // For duration items that stay in slot (Golden Mushroom, Bullet)
}

/**
 * Active effect on a vehicle (duration-based items).
 */
interface ActiveEffect {
  type: ItemType;
  duration: number;      // Time remaining
  sourceVehicleId: number;  // Who applied this effect (for lightning)
}

/**
 * Item - Base interface for pickup items.
 */
interface IItem extends IEntity {
  /** Item type */
  type: ItemType;

  /** Time until item respawns after pickup (seconds) */
  respawnTime: number;

  /** Current respawn countdown (-1 if available) */
  respawnCountdown: number;
}

/**
 * Base Item class.
 */
class Item extends Entity implements IItem {
  type: ItemType;
  respawnTime: number;
  respawnCountdown: number;
  
  // Destruction effect state
  destructionTimer: number;      // Time remaining for destruction animation
  destructionStartTime: number;  // When destruction started (for animation phase)
  pickedUpByPlayer: boolean;     // Whether the player (not NPC) picked this up

  constructor(type: ItemType) {
    super();
    this.type = type;
    this.respawnTime = 2.5;  // Default respawn time in seconds
    this.respawnCountdown = -1;
    this.destructionTimer = 0;
    this.destructionStartTime = 0;
    this.pickedUpByPlayer = false;
  }

  /**
   * Check if item is available for pickup.
   */
  isAvailable(): boolean {
    return this.active && this.respawnCountdown < 0 && this.destructionTimer <= 0;
  }
  
  /**
   * Check if item is currently showing destruction animation.
   */
  isBeingDestroyed(): boolean {
    return this.destructionTimer > 0;
  }

  /**
   * Mark item as picked up with destruction effect.
   * @param byPlayer - true if picked up by human player (shows screen effect)
   */
  pickup(byPlayer: boolean = false): void {
    this.destructionTimer = 0.4;  // 400ms destruction animation
    this.destructionStartTime = Date.now();
    this.pickedUpByPlayer = byPlayer;
  }

  /**
   * Update respawn timer and destruction animation.
   */
  updateRespawn(dt: number): void {
    // Update destruction animation
    if (this.destructionTimer > 0) {
      this.destructionTimer -= dt;
      if (this.destructionTimer <= 0) {
        // Animation finished, start respawn countdown
        this.respawnCountdown = this.respawnTime;
        this.destructionTimer = 0;
      }
    }
    // Update respawn countdown
    else if (this.respawnCountdown >= 0) {
      this.respawnCountdown -= dt;
    }
  }
}
