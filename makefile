
linux:
	-mkdir -p out
	gcc -Isrc/MENGINE src/*.c src/MENGINE/*.c src/EGAME/*.c `sdl2-config --cflags --libs` -lSDL2_image -lSDL2_mixer -lSDL2_ttf -lGL -lm -o bp

run: linux
	./bp

wasm:
	-mkdir -p web_out
	-mkdir -p out
	emcc -Isrc/MENGINE src/*.c src/MENGINE/*.c src/EGAME/*.c \
		--shell-file src/web/base.html \
		-s USE_SDL=2 \
		-s USE_SDL_IMAGE=2 \
		-s SDL2_IMAGE_FORMATS='["png"]' \
		-s USE_SDL_TTF=2 \
		-s USE_SDL_MIXER=2 \
		-s MAX_WEBGL_VERSION=2 \
		-s ALLOW_MEMORY_GROWTH=1 \
		--preload-file res \
		-o web_out/bp.html

local: wasm
	sudo cp -r web_out/* /var/www/html/

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

