PROJECT = wakeonlan
CC = g++
CFLAGS = -Wall -std=c++23 -g
SRC_FILES = src/*.cpp
OUT_DIR = build

all: $(PROJECT)

# build app
$(PROJECT): 
	$(CC) $(CFLAGS) -o $(OUT_DIR)/$(PROJECT) $(SRC_FILES)

# --------------
# RECIPES ONLY:
.PHONY: start_station
start_station: 
	./$(OUT_DIR)/$(PROJECT)

.PHONY: clear_build
clear_build:
	rm build/*