all:
	gcc chip8.c -o chip8 `sdl2-config --cflags --libs`
