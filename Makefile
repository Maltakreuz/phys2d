all:
	g++ main.cpp -o a.exe `sdl2-config --cflags --libs` -lSDL2_ttf
