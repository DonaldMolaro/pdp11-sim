.PHONY: build demo test demo-traps demo-traps-extended clean

build:
	cmake -S . -B build
	cmake --build build

demo: build
	./build/pdp11sim examples/demo.asm
	./build/pdp11sim examples/all_instructions.asm
	$(MAKE) demo-traps
	$(MAKE) demo-traps-extended

demo-traps: build
	@expect -c 'spawn ./build/pdp11sim examples/traps.asm; send "Zhello\n"; expect eof'

demo-traps-extended: build
	@expect -c 'spawn ./build/pdp11sim examples/traps_extended.asm; send -- "-42 0x1A\n"; expect eof'

test: build
	./build/pdp11_tests

clean:
	rm -rf build
	rm -f t.mp.tt t.txt /tmp/pdp11_trap_io.txt
