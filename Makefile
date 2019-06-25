.PHONY: release clean

# build release
release:
	mkdir -p build
	cd build && cmake .. && make

# flash to device
flash: release
	cd build && arm-none-eabi-objcopy --only-section=.text --output-target binary bluepill bluepill.bin
	cd build && st-flash write bluepill.bin 0x8000000

clean:
	rm -rf build
