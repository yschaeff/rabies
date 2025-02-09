ALL: build/firmware.uf2

build: CMakeLists.txt
	mkdir -p build
	cd build; PICO_SDK_PATH=/home/yuri/repo/rp2040/pico-sdk cmake ".."

build/firmware.uf2: build main.c ws2812.pio rabi.pio
	$(MAKE) -C build/

flash: build/firmware.uf2
	./flash $^

clean:
	rm -rf build generated

.PHONY: ALL flash clean
