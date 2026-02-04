.PHONY: build demo test demo-traps

build:
	cmake -S . -B build
	cmake --build build

demo: build
	./build/pdp11sim examples/demo.asm
	./build/pdp11sim examples/all_instructions.asm
	$(MAKE) demo-traps

demo-traps: build
	@expect -c 'spawn ./build/pdp11sim examples/traps.asm; send "Zhello\\n"; expect eof'

test: build
	./build/pdp11_tests
