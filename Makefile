include ~/Projects/config-files/Makefile/build-common.mk

EXE := create

SRC := source.c
OBJ := $(patsubst %.c, %.o, $(SRC))
DEP := $(patsubst %.c, %.d, $(SRC))

#CFLAGS += -pg -lc -g
CFLAGS += -O3

LDFLAGS += -lm -lpng
#LDFLAGS += -pg -lc -g

.PHONY: clean

$(EXE): $(OBJ)
	$(CC) $(OBJ) -o $(EXE) $(LDFLAGS)

-include $(DEP)

clean:
	$(RM) $(OBJ) $(DEP) $(EXE)
