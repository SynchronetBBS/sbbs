/**
 * FixedTimestep - Maintains consistent physics tick rate.
 *
 * Decouples game logic from render rate. Logic runs at fixed intervals
 * (e.g., 60Hz) while rendering happens as fast as possible.
 */

interface TimestepConfig {
  /** Logic ticks per second */
  tickRate: number;
  /** Maximum ticks to run per frame (prevents spiral of death) */
  maxTicksPerFrame: number;
}

class FixedTimestep {
  private tickDuration: number;
  private maxTicks: number;
  private accumulator: number;

  constructor(config: TimestepConfig) {
    this.tickDuration = 1000 / config.tickRate;
    this.maxTicks = config.maxTicksPerFrame;
    this.accumulator = 0;
  }

  /**
   * Add elapsed real time to accumulator.
   * Returns number of logic ticks to run this frame.
   */
  update(deltaMs: number): number {
    this.accumulator += deltaMs;

    var ticks = 0;
    while (this.accumulator >= this.tickDuration && ticks < this.maxTicks) {
      this.accumulator -= this.tickDuration;
      ticks++;
    }

    // If we hit max ticks, discard remaining time to prevent spiral
    if (ticks >= this.maxTicks) {
      this.accumulator = 0;
    }

    return ticks;
  }

  /**
   * Get interpolation factor for smooth rendering.
   * Returns 0-1 representing progress toward next tick.
   */
  getAlpha(): number {
    return this.accumulator / this.tickDuration;
  }

  /**
   * Get tick duration in seconds (for physics calculations).
   */
  getDt(): number {
    return this.tickDuration / 1000;
  }

  /**
   * Reset accumulator (call after pause/unpause).
   */
  reset(): void {
    this.accumulator = 0;
  }
}
