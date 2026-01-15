.PHONY: help
help: makefile
	@tail -n +4 makefile | grep ".PHONY"

PREFIX ?= $(HOME)/.local
CC ?= gcc
CFLAGS ?=
# libsanitizer is not supported on all targets
EXTRAC_CFLAGS ?= -fsanitize=undefined,address

# All source / header files in repository
SRC_FILES := $(wildcard src/*.c)
# Source files excluding CLI (for library builds)
LIB_SRC_FILES := $(filter-out src/cli.c, $(SRC_FILES))

HDR_FILES := $(wildcard include/*.h)
HDR_SRC_FILES := \
	$(filter-out include/stb_image.h, \
	$(filter-out include/stb_image_write.h, \
	$(HDR_FILES)))

TEST_FILES := $(wildcard tests/*.c)


.PHONY: format
format:
	clang-format -i $$(fd -e h -e c . src tests)


.PHONY: test-units
test-units: $(HDR_FILES) $(SRC_FILES) $(TEST_FILES)
	$(CC) $(CFLAGS) -Wall -Wextra -Wpedantic \
		-Iinclude tests/test.c $(LIB_SRC_FILES) \
		-lm -o test_bin \
	&& ./test_bin

	$(CC) $(CFLAGS) -Wall -Wextra -Wpedantic \
		-Iinclude $(LIB_SRC_FILES) tests/apply_test.c \
		-lm -o apply_test \
	&& ./apply_test


CLI_TEST_FILES := $(wildcard tests/cli/*.md)

# Detect platform for native binary target
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    FLATCV_NATIVE := flatcv_mac
else ifeq ($(UNAME_S),Linux)
    FLATCV_NATIVE := flatcv_linux
endif

# Integration tests for CLI
.PHONY: test-integration
test-integration: $(FLATCV_NATIVE) $(CLI_TEST_FILES)
	ln -sf $(FLATCV_NATIVE) flatcv
	scrut test --combine-output --work-directory=$$(pwd) $(CLI_TEST_FILES)


.PHONY: test-amalgamation
test-amalgamation: flatcv.h flatcv.c tests/test_amalgamation.c
	# Test that amalgamated files compile and work correctly
	mkdir -p tmp/amalgamation_test
	cp flatcv.h flatcv.c tests/test_amalgamation.c tmp/amalgamation_test/
	cd tmp/amalgamation_test && $(CC) $(CFLAGS) -Wall -Wextra -Wpedantic \
		flatcv.c test_amalgamation.c \
		-lm -o test_amalgamation_bin \
	&& ./test_amalgamation_bin


.PHONY: test-corner-detection
test-corner-detection: flatcv_mac tests/test_corner_detection.py
	# Test corner detection accuracy against ground truth
	./tests/test_corner_detection.py


.PHONY: test
test: flatcv_mac test-units test-integration test-amalgamation test-corner-detection


.PHONY: test-extended
test-extended:
	$(CC) \
		$(CFLAGS) \
		-Wall \
		-Wextra \
		-Wpedantic \
		-Wshadow \
		-Wformat=2 \
		-Wcast-align \
		-Wconversion \
		-Wsign-conversion \
		-Wnull-dereference \
		-Wduplicated-cond \
		-Wduplicated-branches \
		-Wlogical-op \
		-Wuseless-cast \
		-Wno-unused-parameter \
		-Werror \
		-g \
		-fno-omit-frame-pointer \
		$(EXTRA_CFLAGS) \
		-Iinclude src/*.c tests/*.c


.PHONY: analyze
analyze:
	clang --analyze -Iinclude $(SRC_FILES)


flatcv_mac_debug: $(HDR_FILES) $(SRC_FILES)
	@if [ "$$(uname)" != "Darwin" ]; then \
		echo "Error: This target can only be built on macOS"; \
		exit 1; \
	fi
	$(CC) $(CFLAGS) -g -Wall -Wextra -Wpedantic \
		-Iinclude $(SRC_FILES) \
		-DDEBUG_LOGGING \
		-lm -o $@


.PHONY: debug
debug: flatcv_mac_debug
	lldb ./$<


.PHONY: benchmark
benchmark: flatcv_mac
	@./benchmark.sh


.PHONY: leaks
leaks: flatcv_mac
	leaks --atExit -- \
		./flatcv \
			imgs/parrot_hq.jpeg \
			resize 512x512, grayscale, blur 9, sobel \
			tmp/leaks_test.png


flatcv_mac: $(HDR_FILES) $(SRC_FILES)
	@if [ "$$(uname)" != "Darwin" ]; then \
		echo "Error: This target can only be built on macOS"; \
		exit 1; \
	fi
	$(CC) $(CFLAGS) -Wall -Wextra -Wpedantic \
		-Iinclude $(SRC_FILES) \
		-lm -o $@

.PHONY: mac-build
mac-build: flatcv_mac


libflatcv_mac.a: $(HDR_FILES) $(LIB_SRC_FILES)
	@if [ "$$(uname)" != "Darwin" ]; then \
		echo "Error: This target can only be built on macOS"; \
		exit 1; \
	fi
	$(CC) $(CFLAGS) -Wall -Wextra -Wpedantic -c \
		-Iinclude $(LIB_SRC_FILES) \
		-fPIC
	ar rcs $@ *.o

libflatcv_mac.dylib: $(HDR_FILES) $(LIB_SRC_FILES)
	@if [ "$$(uname)" != "Darwin" ]; then \
		echo "Error: This target can only be built on macOS"; \
		exit 1; \
	fi
	$(CC) $(CFLAGS) -Wall -Wextra -Wpedantic -dynamiclib -fPIC \
		-Iinclude $(LIB_SRC_FILES) \
		-lm -o $@

.PHONY: mac-lib
mac-lib: libflatcv_mac.a libflatcv_mac.dylib


# Linux - Native build (run directly on Linux)
flatcv_linux: $(HDR_FILES) $(SRC_FILES)
	@if test "$$(uname)" != "Linux"; \
	then \
		echo "Error: This target can only be built on Linux"; \
		exit 1; \
	fi
	$(CC) $(CFLAGS) -Wall -Wextra -Wpedantic \
		-Iinclude $(SRC_FILES) \
		-lm -o $@

# Linux - Build binary inside Docker and copy it back to host
flatcv_linux_docker: Dockerfile
	docker build -t flatcv-build .
	docker create --name flatcv-tmp flatcv-build
	docker cp flatcv-tmp:/flatcv ./flatcv_linux
	docker rm flatcv-tmp

.PHONY: lin-build
lin-build: flatcv_linux

.PHONY: lin-build-docker
lin-build-docker: flatcv_linux_docker


# Windows - Cross-compilation with mingw-w64
flatcv_windows_x86_64.exe: $(HDR_FILES) $(SRC_FILES)
	x86_64-w64-mingw32-gcc -Wall -Wextra -Wpedantic -static -static-libgcc \
		-Iinclude $(SRC_FILES) \
		-lm -o $@

.PHONY: win-build
win-build: flatcv_windows_x86_64.exe


.PHONY: win-test
win-test: flatcv_windows_x86_64.exe
	@WINEDEBUG=-all \
		MVK_CONFIG_LOG_LEVEL=None \
		wine $<


.PHONY: build
build: mac-build lin-build-docker win-build wasm-build


# WebAssembly build with Emscripten
flatcv.wasm: $(HDR_FILES) $(LIB_SRC_FILES)
	emcc -Wall -Wextra -Wpedantic \
		-Iinclude $(LIB_SRC_FILES) \
		-lm \
		-s WASM=1 \
		-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap","HEAPU8"]' \
		-s EXPORTED_FUNCTIONS='["_malloc","_free","_fcv_grayscale","_fcv_apply_gaussian_blur","_fcv_sobel_edge_detection","_fcv_otsu_threshold_rgba"]' \
		-s ALLOW_MEMORY_GROWTH=1 \
		-s MODULARIZE=1 \
		-s EXPORT_NAME='FlatCV' \
		-o tests/cli/playground/flatcv.js

.PHONY: wasm-build
wasm-build: flatcv.wasm


.PHONY: install
install:
	@if [ "$$(uname)" = "Darwin" ]; then \
		make flatcv_mac; \
		cp flatcv_mac $(DESTDIR)$(PREFIX)/bin/flatcv; \
	elif [ "$$(uname)" = "Linux" ]; then \
		make flatcv_linux; \
		cp flatcv_linux $(PREFIX)/bin/flatcv; \
	else \
		make flatcv_windows_x86_64.exe; \
		echo "Copy flatcv_windows_x86_64.exe to your Windows machine!"; \
	fi


.PHONY: install-lib
install-lib:
	@if [ "$$(uname)" = "Darwin" ]; then \
		make mac-lib flatcv.h; \
		cp libflatcv_mac.a $(DESTDIR)$(PREFIX)/lib/; \
		cp libflatcv_mac.dylib $(DESTDIR)$(PREFIX)/lib/; \
		cp flatcv.h $(DESTDIR)$(PREFIX)/include/; \
	fi


flatcv.h: $(HDR_SRC_FILES) license.txt
	@echo '/* FlatCV - Amalgamated public header (auto-generated) */' > $@
	@echo '/*' >> $@
	@cat license.txt | sed 's/^/ * /' >> $@
	@echo ' */' >> $@
	@echo '' >> $@
	@echo '#ifndef FLATCV_H' >> $@
	@echo '#define FLATCV_H' >> $@
	@echo '#define FLATCV_AMALGAMATION' >> $@
	@for src in $(HDR_SRC_FILES); do \
			echo "// File: $$src" >> $@; \
			cat $$src >> $@; \
		done
	@echo '#endif /* FLATCV_H */' >> $@

flatcv.c: flatcv.h $(LIB_SRC_FILES) license.txt
	@echo '/* FlatCV - Amalgamated implementation (auto-generated) */' > $@
	@echo '/*' >> $@
	@cat license.txt | sed 's/^/ * /' >> $@
	@echo ' */' >> $@
	@echo '' >> $@
	@echo '#define FLATCV_AMALGAMATION' >> $@
	@echo '#include "flatcv.h"' >> $@
	@for src in $(LIB_SRC_FILES); do \
			echo "// File: $$src" >> $@; \
			cat $$src >> $@; \
		done
	@echo '/* End of FlatCV amalgamation */' >> $@

flatcv: flatcv.c flatcv.h src/cli.c
	$(CC) $(CFLAGS) -Wall -Wextra -Wpedantic \
		-Iinclude flatcv.c src/cli.c \
		-lm -o $@

.PHONY: combine
combine: flatcv.h flatcv.c


.PHONY: docs-lib/build
docs-lib/build:
	doxygen ./doxyfile


IMAGES := $(wildcard imgs/*)

# Images must be duplicated so that the file paths are correct
tests/cli/imgs: $(IMAGES)
	rsync -a imgs/ tests/cli/imgs/

.PHONY: docs/serve
docs/serve: tests/cli/imgs
	mdbook serve ./tests

.PHONY: docs/build
docs/build: tests/cli/imgs
	mdbook build ./tests


.PHONY: release
release: combine mac-build lin-build-docker win-build wasm-build
	@echo "1. Add binaries to GitHub release: https://github.com/ad-si/FlatCV/releases"


.PHONY: clean
clean:
	-rm -f \
		apply_test \
		flatcv_* \
		flatcv.c \
		flatcv.h \
		flatcv.js \
		flatcv.wasm \
		tests/cli/playground/flatcv.js \
		tests/cli/playground/flatcv.wasm \
		test_bin \
		test_amalgamation_bin
