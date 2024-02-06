linux:
	-mkdir out
	gcc src/* `sdl2-config --cflags --libs ` -lSDL2_image -o i
run: linux
	./i
wasm:
	-mkdir out
	-mkdir build
	emcc -c src/* -o build/app.o -s USE_SDL=2
	emcc build/app.o -o build/index.html -s USE_SDL=2

web: wasm
	sudo cp build/index.html build/index.js build/index.wasm /var/www/html/
compilecmd: clean 
	bear -- make
clean:
	-rm -r out/
	-rm -r build/
