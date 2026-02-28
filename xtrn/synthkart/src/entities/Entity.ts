/**
 * Entity - Base interface for all game objects.
 */

interface IEntity {
  /** Unique entity ID */
  id: number;

  /** X position (lateral, world units) */
  x: number;

  /** Y position (vertical, world units) - usually 0 for ground */
  y: number;

  /** Z position (along track, world units) */
  z: number;

  /** Rotation/heading in radians */
  rotation: number;

  /** Speed (world units per second) */
  speed: number;

  /** Whether entity is active in the game */
  active: boolean;
}

/**
 * Simple entity ID generator.
 */
var nextEntityId = 1;

function generateEntityId(): number {
  return nextEntityId++;
}

/**
 * Base entity class with common properties.
 */
class Entity implements IEntity {
  id: number;
  x: number;
  y: number;
  z: number;
  rotation: number;
  speed: number;
  active: boolean;

  constructor() {
    this.id = generateEntityId();
    this.x = 0;
    this.y = 0;
    this.z = 0;
    this.rotation = 0;
    this.speed = 0;
    this.active = true;
  }
}
