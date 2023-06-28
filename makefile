PROJECT = wakeonlan
CC = g++
CFLAGS = -Wall -std=c++23 -g
MAIN = src/main.cpp
OUT_DIR = build

OBJECTS = main.o

all: $(PROJECT)

# build app and clean *.o files
$(PROJECT): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(OUT_DIR)/$(PROJECT) $(OBJECTS) && rm $(OBJECTS) 

# build main.o file
main.o: $(MAIN) 
	$(CC) $(CFLAGS) -c $(MAIN)

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