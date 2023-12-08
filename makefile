linux:
	-mkdir out
	gcc src/*.c `sdl2-config --cflags --libs ` -lGL -lSDL2_image -o i
run: linux
	./i
wasm:
	-mkdir out
	-mkdir build
	echo "step1:"
	emcc -c src/*.c -s USE_SDL=2 -o build/app.o 
	echo "step2:"
	emcc build/app.o -o build/index.html -s USE_SDL=2 -sMAX_WEBGL_VERSION=2 

web: wasm
	sudo cp build/index.html build/index.js build/index.wasm /var/www/html/
clean:
	-rm -r out/
	-rm -r build/
