.PHONY: help
help: makefile
	@tail -n +4 makefile | grep ".PHONY"

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
	gcc -Wall -Wextra -Wpedantic \
		-Iinclude tests/test.c $(LIB_SRC_FILES) \
		-lm -o test_bin \
	&& ./test_bin

	gcc -Wall -Wextra -Wpedantic \
		-Iinclude $(LIB_SRC_FILES) tests/apply_test.c \
		-o apply_test \
	&& ./apply_test


.PHONY: test-integration
test-integration: build tests/cli.md
	# Integration tests for CLI
	scrut test --work-directory=$$(pwd) tests/cli.md


.PHONY: test
test: build test-units test-integration


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
benchmark: flatcv
	@./benchmark.sh


flatcv: $(HDR_FILES) $(SRC_FILES)
	gcc -Wall -Wextra -Wpedantic \
		-Iinclude $(SRC_FILES) \
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
build: flatcv


.PHONY: install
install: flatcv
	sudo cp $< /usr/local/bin


flatcv.h: $(HDR_SRC_FILES)
	@echo '/* FlatCV - Amalgamated public header (auto-generated) */' > $@
	@echo '#ifndef FLATCV_H' >> $@
	@echo '#define FLATCV_H' >> $@
	@echo '#define FLATCV_AMALGAMATION' >> $@
	@for src in $(HDR_SRC_FILES); do \
			echo "// File: $$src" >> $@; \
			cat $$src >> $@; \
		done
	@echo '#endif /* FLATCV_H */' >> $@

flatcv.c: flatcv.h $(LIB_SRC_FILES)
	@echo '/* FlatCV - Amalgamated implementation (auto-generated) */' > $@
	@echo '#define FLATCV_AMALGAMATION' >> $@
	@echo '#include "flatcv.h"' >> $@
	@for src in $(LIB_SRC_FILES); do \
			echo "// File: $$src" >> $@; \
			cat $$src >> $@; \
		done
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
