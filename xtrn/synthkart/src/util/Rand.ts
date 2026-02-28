/**
 * Simple pseudo-random number generator.
 * Provides deterministic sequences for reproducible gameplay.
 */

class Rand {
  private seed: number;

  constructor(seed?: number) {
    this.seed = seed !== undefined ? seed : Date.now();
  }

  /**
   * Get next random number in [0, 1).
   * Uses a simple LCG (Linear Congruential Generator).
   */
  next(): number {
    // LCG parameters (same as glibc)
    this.seed = (this.seed * 1103515245 + 12345) & 0x7fffffff;
    return this.seed / 0x7fffffff;
  }

  /**
   * Get random integer in [min, max] inclusive.
   */
  nextInt(min: number, max: number): number {
    return Math.floor(this.next() * (max - min + 1)) + min;
  }

  /**
   * Get random float in [min, max).
   */
  nextFloat(min: number, max: number): number {
    return this.next() * (max - min) + min;
  }

  /**
   * Get random boolean with given probability of true.
   */
  nextBool(probability?: number): boolean {
    var p = probability !== undefined ? probability : 0.5;
    return this.next() < p;
  }

  /**
   * Pick random element from array.
   */
  pick<T>(array: T[]): T {
    var index = this.nextInt(0, array.length - 1);
    return array[index];
  }

  /**
   * Shuffle array in place.
   */
  shuffle<T>(array: T[]): T[] {
    for (var i = array.length - 1; i > 0; i--) {
      var j = this.nextInt(0, i);
      var temp = array[i];
      array[i] = array[j];
      array[j] = temp;
    }
    return array;
  }

  /**
   * Reset seed for reproducible sequences.
   */
  setSeed(seed: number): void {
    this.seed = seed;
  }
}

// Global random instance
var globalRand = new Rand();
