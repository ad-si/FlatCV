.PHONY: help
help: makefile
	@tail -n +4 makefile | grep ".PHONY"

# All source / header files in repository
SRC_FILES := $(wildcard src/*.c)
HDR_FILES := $(wildcard include/*.h)
TEST_FILES := $(wildcard tests/*.c)


.PHONY: test-units
test-units: $(HDR_FILES) $(SRC_FILES) $(TEST_FILES)
	gcc -Wall -Werror \
		-Iinclude tests/test.c src/conversion.c src/perspectivetransform.c \
		-o test_bin \
	&& ./test_bin

	gcc -Wall -Werror \
		-Iinclude src/conversion.c src/perspectivetransform.c tests/apply_test.c \
		-o apply_test \
	&& ./apply_test


.PHONY: test-integration
test-integration: build tests/cli.md
	# Integration tests for CLI
	scrut test --work-directory=$$(pwd) tests/cli.md


.PHONY: test
test: test-units test-integration


.PHONY: test-extended
test-extended:
	gcc \
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
		-fsanitize=undefined,address \
		-fno-omit-frame-pointer \
		-Iinclude src/*.c tests/*.c


.PHONY: benchmark
benchmark: build
	@./benchmark.sh


flatcv: $(HDR_FILES) $(SRC_FILES)
	gcc -Wall -Werror \
		-Iinclude src/cli.c src/conversion.c src/perspectivetransform.c \
		-lm -o $@

.PHONY: mac-build
mac-build: flatcv
	cp flatcv flatcv_mac_arm64


# Linux - Build binary inside Docker and copy it back to host
flatcv_linux_arm64: Dockerfile
	docker build -t flatcv-build .
	docker create --name flatcv-tmp flatcv-build
	docker cp flatcv-tmp:/flatcv ./flatcv_linux_arm64
	docker rm flatcv-tmp

.PHONY: lin-build
lin-build: flatcv_linux_arm64


# Windows - Cross-compilation with mingw-w64
flatcv_windows_x86_64.exe: $(HDR_FILES) $(SRC_FILES)
	x86_64-w64-mingw32-gcc -Wall -Werror -static -static-libgcc \
		-Iinclude src/cli.c src/conversion.c src/perspectivetransform.c \
		-lm -o $@

.PHONY: win-build
win-build: flatcv_windows_x86_64.exe


.PHONY: win-test
win-test: flatcv_windows_x86_64.exe
	@WINEDEBUG=-all \
		MVK_CONFIG_LOG_LEVEL=None \
		wine $<


.PHONY: build
build: flatcv


.PHONY: install
install: build
	sudo cp ./flatcv /usr/local/bin


flatcv.h: include/perspectivetransform.h include/conversion.h
	@echo '/* FlatCV - Amalgamated public header (auto-generated) */' > $@
	@echo '#ifndef FLATCV_H' >> $@
	@echo '#define FLATCV_H' >> $@
	@cat include/perspectivetransform.h >> $@
	@cat include/conversion.h >> $@
	@echo '#endif /* FLATCV_H */' >> $@

flatcv.c: flatcv.h src/conversion.c src/perspectivetransform.c
	@echo '/* FlatCV - Amalgamated implementation (auto-generated) */' > $@
	@echo '#define FLATCV_AMALGAMATION' >> $@
	@echo '#include "flatcv.h"' >> $@
	@cat src/conversion.c >> $@
	@cat src/perspectivetransform.c >> $@
	@echo '/* End of FlatCV amalgamation */' >> $@

.PHONY: combine
combine: flatcv.h flatcv.c


.PHONY: docs
docs:
	doxygen ./doxyfile


.PHONY: release
release: build combine mac-build lin-build win-build
	@echo "1. Add binaries to GitHub release: https://github.com/ad-si/FlatCV/releases"


.PHONY: clean
clean:
	-rm -f \
		apply_test \
		flatcv \
		flatcv.c \
		flatcv.h \
		test_bin
