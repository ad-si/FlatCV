.PHONY: help
help: makefile
	@tail -n +4 makefile | grep ".PHONY"


.PHONY: test
test:
	gcc -Wall test.c simplecv.c perspectivetransform.c -o test_bin && ./test_bin
	gcc -Wall simplecv.c perspectivetransform.c apply_test.c -o apply_test && ./apply_test


simplecv:
	gcc -Wall -lm cli.c simplecv.c perspectivetransform.c -o $@


images/grayscale.jpeg: images/parrot.jpeg simplecv
	./simplecv $< grayscale $@

images/blur.jpeg: images/parrot.jpeg simplecv
	./simplecv $< '(blur 9)' $@

images/grayscale_blur.jpeg: images/parrot.jpeg simplecv
	./simplecv $< grayscale '(blur 9)' $@


.PHONY: clean
clean:
	-rm -f \
		apply_test \
		simplecv \
		test_bin
