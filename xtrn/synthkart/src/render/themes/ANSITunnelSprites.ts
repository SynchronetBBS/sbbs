/**
 * ANSITunnelSprites - Minimal roadside sprites for the ANSI Tunnel theme.
 * Since the ANSI art dominates the visuals, sprites are kept minimal/abstract.
 */

var ANSITunnelSprites = {
  // Abstract digital markers - minimal so they don't clash with ANSI art
  left: [
    // Digital beacon - simple vertical line
    {
      lines: [
        '│',
        '│',
        '│',
        '│'
      ],
      colors: [LIGHTCYAN, CYAN, LIGHTCYAN, CYAN],
      width: 1,
      height: 4
    },
    // Data node - small box
    {
      lines: [
        '┌┐',
        '└┘'
      ],
      colors: [CYAN, CYAN],
      width: 2,
      height: 2
    },
    // Signal pole
    {
      lines: [
        ' ◊',
        ' │',
        ' │'
      ],
      colors: [LIGHTCYAN, DARKGRAY, DARKGRAY],
      width: 2,
      height: 3
    },
    // Binary pillar
    {
      lines: [
        '1',
        '0',
        '1',
        '0'
      ],
      colors: [LIGHTCYAN, CYAN, LIGHTCYAN, CYAN],
      width: 1,
      height: 4
    }
  ],
  
  right: [
    // Mirror of left side
    {
      lines: [
        '│',
        '│',
        '│',
        '│'
      ],
      colors: [LIGHTCYAN, CYAN, LIGHTCYAN, CYAN],
      width: 1,
      height: 4
    },
    {
      lines: [
        '┌┐',
        '└┘'
      ],
      colors: [CYAN, CYAN],
      width: 2,
      height: 2
    },
    {
      lines: [
        '◊ ',
        '│ ',
        '│ '
      ],
      colors: [LIGHTCYAN, DARKGRAY, DARKGRAY],
      width: 2,
      height: 3
    },
    {
      lines: [
        '0',
        '1',
        '0',
        '1'
      ],
      colors: [CYAN, LIGHTCYAN, CYAN, LIGHTCYAN],
      width: 1,
      height: 4
    }
  ],
  
  // Background layer - mostly empty, let ANSI show through
  background: {
    layers: [
      // Minimal distant markers
      {
        speed: 0.1,
        sprites: [
          {
            x: 10,
            lines: ['·'],
            colors: [DARKGRAY],
            width: 1,
            height: 1
          },
          {
            x: 30,
            lines: ['·'],
            colors: [CYAN],
            width: 1,
            height: 1
          },
          {
            x: 50,
            lines: ['·'],
            colors: [DARKGRAY],
            width: 1,
            height: 1
          },
          {
            x: 70,
            lines: ['·'],
            colors: [CYAN],
            width: 1,
            height: 1
          }
        ]
      }
    ]
  }
};
