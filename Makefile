CC ?= cc

ifeq ($(OS),Windows_NT)
	SUFFIX ?=.exe
else
	SUFFIX ?=
endif

.PHONY: clean all

all: imc$(SUFFIX) imi$(SUFFIX)

imc$(SUFFIX): imc.c
	$(CC) -O2 -o imc$(SUFFIX) -Wall -Wextra -pedantic imc.c

imi$(SUFFIX): imi.c
	$(CC) -O2 -o imi$(SUFFIX) -Wall -Wextra -pedantic imi.c -lraylib -lm

clean:
	rm -f imc$(SUFFIX) imi$(SUFFIX)
