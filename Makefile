compile: build
	$(MAKE) -C build/

build:
	mkdir -p build
	cd build; PICO_SDK_PATH=/home/yuri/repo/rp2040/pico-sdk cmake ".."

flash: compile
	pmount /dev/sda1
	cp build/pio_ws2812.uf2 /media/sda1
	pumount /dev/sda1

clean:
	rm -rf build generated

.PHONY: compile clean
