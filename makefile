linux:
	-mkdir -p out
	gcc -Isrc/MENGINE src/*.c src/MENGINE/*.c `sdl2-config --cflags --libs` -lSDL2_image -lGL -lm -o bp
run: linux
	./bp
wasm:
	-mkdir web_out
	-mkdir out
	emcc -Isrc/MENGINE src/*.c src/MENGINE/*.c --shell-file src/web/base.html   -s USE_SDL=2 -s USE_SDL_IMAGE=2 -lSDL -sMAX_WEBGL_VERSION=2 --preload-file res -s ALLOW_MEMORY_GROWTH=1 -s SDL2_IMAGE_FORMATS='["png"]' -o web_out/bp.html
local: wasm
	sudo cp -r web_out/*  /var/www/html/
runweb: wasm
	python3 -m http.server --directory web_out
runweb_b: wasm
	python3 -m http.server --directory web_out & \
	sleep 1 && firefox http://localhost:8000/bp.html
compilecmd:
	bear -- make
clean:
	-rm -r out/
	-rm -r web_out/

