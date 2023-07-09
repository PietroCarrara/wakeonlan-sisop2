PROJECT = wakeonlan
CC = g++
CFLAGS = -Wall -std=c++23 -g -pthread
SRC_FILES = src/*.cpp
OUT_DIR = build

all: $(PROJECT)

# build app
$(PROJECT): 
	$(CC) $(CFLAGS) -o $(OUT_DIR)/$(PROJECT) $(SRC_FILES)

# --------------
# RECIPES ONLY:
.PHONY: client
client: 
	./$(OUT_DIR)/$(PROJECT)

.PHONY: manager
manager:
	./$(OUT_DIR)/$(PROJECT) manager

.PHONY: clear_build
clear_build:
	rm build/*