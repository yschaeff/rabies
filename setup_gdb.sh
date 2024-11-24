gdb-multiarch -ex 'set architecture armv6-m' -ex 'target remote :3333' -ex 'file Build/app.elf' -ex 'load' -ex 'continue'
#gdb-multiarch -ex 'set architecture armv6-m' -ex 'target remote :3333' -ex 'load Build/app.elf' -ex 'file Build/app.elf' -ex 'continue'
