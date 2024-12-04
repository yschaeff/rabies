ALL: build/firmware.uf2

build: CMakeLists.txt
	mkdir -p build
	cd build; PICO_SDK_PATH=/home/yuri/repo/rp2040/pico-sdk cmake ".."

build/firmware.uf2: build main.c ws2812.pio rabi.pio
	$(MAKE) -C build/

flash: build/firmware.uf2
	pmount /dev/sda1
	cp build/firmware.uf2 /media/sda1
	pumount /dev/sda1

clean:
	rm -rf build generated

.PHONY: ALL flash clean
