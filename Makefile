include ~/Projects/config-files/Makefile/build-common.mk

EXE := create

SRC := source.c
OBJ := $(patsubst %.c, %.o, $(SRC))
DEP := $(patsubst %.c, %.d, $(SRC))

CFLAGS += -O3

LDFLAGS += -lm -lpng

.PHONY: clean

$(EXE): $(OBJ)
	$(CC) $(OBJ) -o $(EXE) $(LDFLAGS)

-include $(DEP)

clean:
	$(RM) $(OBJ) $(DEP) $(EXE)
