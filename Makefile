#include ~/Projects/config-files/Makefile/build-common.mk

EXE := create
OUTPUT_DIR := out/

SRC := source.c
OBJ := $(patsubst %.c, %.o, $(SRC))
DEP := $(patsubst %.c, %.d, $(SRC))

#CFLAGS += -pg -lc -g
CFLAGS += -O3
CFLAGS += -MMD
CFLAGS += $(shell pkg-config --cflags libpng)

LDFLAGS += -lm
LDFLAGS += $(shell pkg-config --libs libpng)
#LDFLAGS += -pg -lc -g

.PHONY: clean

$(EXE): $(OBJ) $(OUTPUT_DIR)
	$(CC) $(OBJ) -o $(EXE) $(LDFLAGS)

-include $(DEP)

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

clean:
	$(RM) $(OBJ) $(DEP) $(EXE)
