# first install

## Dependencies
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential libstdc++-arm-none-eabi-newlib minicom

## install sdk on some location
git clone https://github.com/raspberrypi/pico-sdk.git --branch master
cd pico-sdk
git submodule update --init
cd ..

## install examples 
git clone https://github.com/raspberrypi/pico-examples.git --branch master

# update sdk
cd pico-sdk
git pull
git submodule update

# set path to SDK in Makefile

## hold boot en reset
make flash
./monitor
