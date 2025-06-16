.PHONY: help
help: makefile
	@tail -n +4 makefile | grep ".PHONY"


.PHONY: test
test:
	gcc -Wall test.c simplecv.c perspectivetransform.c -o test_bin && ./test_bin
	gcc -Wall simplecv.c perspectivetransform.c apply_test.c -o apply_test && ./apply_test


.PHONY: clean
clean:
	-rm -f test_bin
	-rm -f apply_test
