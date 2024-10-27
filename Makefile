SRC := mvim.c
INCLUDE := ./
TARGET := mvim

$(TARGET): $(SRC)
	$(CC) -o $@ $^ -I$(INCLUDE) -Wall -Wextra -pedantic -std=c99
