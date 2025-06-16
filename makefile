.PHONY: help
help: makefile
	@tail -n +4 makefile | grep ".PHONY"


.PHONY: test
test:
	gcc -Wall test.c flatcv.c perspectivetransform.c -o test_bin && ./test_bin
	gcc -Wall flatcv.c perspectivetransform.c apply_test.c -o apply_test && ./apply_test


flatcv:
	gcc -Wall -lm cli.c flatcv.c perspectivetransform.c -o $@


images/grayscale.jpeg: images/parrot.jpeg flatcv
	./flatcv $< grayscale $@

images/blur.jpeg: images/parrot.jpeg flatcv
	./flatcv $< '(blur 9)' $@

images/grayscale_blur.jpeg: images/parrot.jpeg flatcv
	./flatcv $< grayscale '(blur 9)' $@


.PHONY: clean
clean:
	-rm -f \
		apply_test \
		flatcv \
		test_bin
