compile: build
	$(MAKE) -C build/

build:
	mkdir -p build
	cd build; PICO_SDK_PATH=/home/yuri/repo/rp2040/pico-sdk cmake ".."

flash: compile
	#cp build/firmware.uf2 /media/root/RPI-RP2
	pmount /dev/sda1
	cp build/pio_ws2812.uf2 /media/sda1
	pumount /dev/sda1

clean:
	rm -rf build

.PHONY: compile clean
