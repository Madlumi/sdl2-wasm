linux:
	-mkdir out
	gcc src/main.c `sdl2-config --cflags --libs` -lSDL2_image -lGL -lm -o bp
run: linux
	./bp
wasm:
	-mkdir web_out
	-mkdir out 
	emcc src/main.c  --shell-file src/web/base.html   -s USE_SDL=2 -s USE_SDL_IMAGE=2 -lSDL -sMAX_WEBGL_VERSION=2 --preload-file res -s ALLOW_MEMORY_GROWTH=1 -s SDL2_IMAGE_FORMATS='["png"]' -o web_out/bp.html
local: wasm
	sudo cp -r web_out/*  /var/www/html/
compilecmd:
	bear -- make
clean:
	-rm -r out/
	-rm -r web_out/

