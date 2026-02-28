/**
 * Utility math functions for 2D operations.
 */

interface Point2D {
  x: number;
  y: number;
}

interface Vector2D {
  x: number;
  y: number;
}

/**
 * Clamp value between min and max.
 */
function clamp(value: number, min: number, max: number): number {
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

/**
 * Linear interpolation.
 */
function lerp(a: number, b: number, t: number): number {
  return a + (b - a) * t;
}

/**
 * Distance between two points.
 */
function distance(a: Point2D, b: Point2D): number {
  var dx = b.x - a.x;
  var dy = b.y - a.y;
  return Math.sqrt(dx * dx + dy * dy);
}

/**
 * Distance squared (faster, good for comparisons).
 */
function distanceSquared(a: Point2D, b: Point2D): number {
  var dx = b.x - a.x;
  var dy = b.y - a.y;
  return dx * dx + dy * dy;
}

/**
 * Normalize a vector to unit length.
 */
function normalize(v: Vector2D): Vector2D {
  var len = Math.sqrt(v.x * v.x + v.y * v.y);
  if (len === 0) return { x: 0, y: 0 };
  return { x: v.x / len, y: v.y / len };
}

/**
 * Dot product of two vectors.
 */
function dot(a: Vector2D, b: Vector2D): number {
  return a.x * b.x + a.y * b.y;
}

/**
 * Rotate a point around origin by angle (radians).
 */
function rotate(p: Point2D, angle: number): Point2D {
  var cos = Math.cos(angle);
  var sin = Math.sin(angle);
  return {
    x: p.x * cos - p.y * sin,
    y: p.x * sin + p.y * cos
  };
}

/**
 * Convert degrees to radians.
 */
function degToRad(degrees: number): number {
  return degrees * Math.PI / 180;
}

/**
 * Convert radians to degrees.
 */
function radToDeg(radians: number): number {
  return radians * 180 / Math.PI;
}

/**
 * Wrap angle to -PI to PI range.
 */
function wrapAngle(angle: number): number {
  while (angle > Math.PI) angle -= 2 * Math.PI;
  while (angle < -Math.PI) angle += 2 * Math.PI;
  return angle;
}

/**
 * Sign of a number (-1, 0, or 1).
 */
function sign(x: number): number {
  if (x > 0) return 1;
  if (x < 0) return -1;
  return 0;
}
