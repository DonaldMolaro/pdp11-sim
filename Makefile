.PHONY: build demo test demo-traps demo-traps-extended demo-guess-hex demo-slot-machine demo-treasure-doors demo-rps clean

build:
	cmake -S . -B build
	cmake --build build

demo: build
	./build/pdp11sim examples/demo.asm
	./build/pdp11sim examples/all_instructions.asm
	$(MAKE) demo-traps
	$(MAKE) demo-traps-extended
	$(MAKE) demo-guess-hex
	$(MAKE) demo-slot-machine
	$(MAKE) demo-treasure-doors
	$(MAKE) demo-rps

demo-traps: build
	@expect -c 'spawn ./build/pdp11sim examples/traps.asm; send "Zhello\n"; expect eof'

demo-traps-extended: build
	@expect -c 'spawn ./build/pdp11sim examples/traps_extended.asm; send -- "-42 0x1A\n"; expect eof'

demo-guess-hex: build
	@expect -c 'spawn ./build/pdp11sim examples/guess_hex.asm; send -- "0\n1\n2\n"; expect eof'

demo-slot-machine: build
	@expect -c 'spawn ./build/pdp11sim examples/slot_machine.asm; send -- "\n\nq\n"; expect eof'

demo-treasure-doors: build
	@expect -c 'spawn ./build/pdp11sim examples/treasure_doors.asm; send -- "2\n"; expect eof'

demo-rps: build
	@expect -c 'spawn ./build/pdp11sim examples/rock_paper_scissors.asm; send -- "1\n"; expect eof'

test: build
	./build/pdp11_tests

clean:
	rm -rf build
	rm -f t.mp.tt t.txt /tmp/pdp11_trap_io.txt
