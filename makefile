.PHONY: help
help: makefile
	@tail -n +4 makefile | grep ".PHONY"

# All source / header files in repository
SRC_FILES := $(wildcard *.c)
HDR_FILES := $(wildcard *.h)


.PHONY: test
test:
	gcc -Wall test.c conversion.c perspectivetransform.c -o test_bin \
		&& ./test_bin
	gcc -Wall conversion.c perspectivetransform.c apply_test.c -o apply_test \
		&& ./apply_test


flatcv: $(SRC_FILES) $(HDR_FILES)
	gcc -Wall -lm cli.c conversion.c perspectivetransform.c -o $@


.PHONY: build
build: flatcv


.PHONY: install
install: build
	sudo cp ./flatcv /usr/local/bin


images/grayscale.jpeg: images/parrot.jpeg flatcv
	./flatcv $< grayscale $@

images/blur.jpeg: images/parrot.jpeg flatcv
	./flatcv $< blur 9 $@

images/grayscale_blur.jpeg: images/parrot.jpeg flatcv
	./flatcv $< grayscale, blur 9 $@

images/bw_smart.png: images/parrot.jpeg flatcv
	./flatcv $< bw_smart $@


flatcv.h: perspectivetransform.h conversion.h
	@echo '/* FlatCV - Amalgamated public header (auto-generated) */' > $@
	@echo '#ifndef FLATCV_H' >> $@
	@echo '#define FLATCV_H' >> $@
	@cat perspectivetransform.h >> $@
	@cat conversion.h >> $@
	@echo '#endif /* FLATCV_H */' >> $@

flatcv.c: flatcv.h conversion.c perspectivetransform.c
	@echo '/* FlatCV - Amalgamated implementation (auto-generated) */' > $@
	@echo '#define FLATCV_AMALGAMATION' >> $@
	@echo '#include "flatcv.h"' >> $@
	@cat conversion.c >> $@
	@cat perspectivetransform.c >> $@
	@echo '/* End of FlatCV amalgamation */' >> $@

.PHONY: combine
combine: flatcv.h flatcv.c


.PHONY: docs
docs:
	doxygen ./doxyfile


.PHONY: clean
clean:
	-rm -f \
		apply_test \
		flatcv \
		flatcv.c \
		flatcv.h \
		test_bin
