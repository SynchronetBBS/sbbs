/**
 * Clock - Time measurement for game loop.
 * Uses Synchronet's system.timer for millisecond-precision time.
 */

class Clock {
  private lastTime: number;

  constructor() {
    this.lastTime = this.now();
  }

  /**
   * Get current time in milliseconds.
   */
  now(): number {
    // system.timer returns seconds with ms precision
    return system.timer * 1000;
  }

  /**
   * Get elapsed time since last call to getDelta().
   * Caps at 250ms to prevent spiral of death after lag/pause.
   */
  getDelta(): number {
    var currentTime = this.now();
    var delta = currentTime - this.lastTime;
    this.lastTime = currentTime;

    // Cap delta to prevent issues after pause/lag
    if (delta > 250) {
      delta = 250;
    }

    return delta;
  }

  /**
   * Reset the clock (call after pause/unpause).
   */
  reset(): void {
    this.lastTime = this.now();
  }
}
