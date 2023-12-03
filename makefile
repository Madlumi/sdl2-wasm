linux:
	gcc main.c `sdl2-config --cflags --libs ` -lSDL2_image -o i

wasm:
	emcc -c main.c -o out/app.o -s USE_SDL=2
	emcc out/app.o -o out/index.html -s USE_SDL=2

web: wasm
	sudo cp out/index.html out/index.js out/index.wasm /var/www/html/
