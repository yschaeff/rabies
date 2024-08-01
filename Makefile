compile: build
	$(MAKE) -C build/

build:
	mkdir -p build
	export PICO_SDK_PATH=/home/yuri/repo/rp2040/pico-sdk
	cd build;cmake ".."

flash: compile
	pmount /dev/sda1
	cp build/firmware.uf2 /media/sda1
	pumount /dev/sda1

clean:
	rm -rf build

.PHONY: compile clean
