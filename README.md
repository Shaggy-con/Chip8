
# CHIP-8 Emulator

A simple CHIP-8 emulator written in C, featuring screen and audio support.

## Features
- Runs CHIP-8 ROMs (games and tests)
- Screen and audio integration using SDL2
- Includes test ROMs and popular games (Tetris, Brick, UFO)

## Prerequisites
- C compiler (e.g., GCC)
- SDL2

## Build Instructions
1. Clone the repository:
   ```bash
   git clone https://github.com/AmruthSD/Chip8.git
   ```

2. Build the emulator:
   ```bash
   make
   ```

## Usage
Run the emulator with a ROM file:
```bash
./chip8 file_name.ch8
```

Examples:
- Test suite: `./chip8 BC_test.ch8`
- Tetris: `./chip8 tetris.ch8`

## ROMs
Included ROMs:
- `BC_test.ch8` (test)
- `test_opcode.ch8` (test)
- `Brick (Brix hack).ch8` (game)
- `tetris.ch8` (game)
- `UFO` (game)
- `IBM_Logo.ch8` (demo)


