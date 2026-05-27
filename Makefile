C_FLAGS=-Iinclude -Wall
LD_FLAGS=-lraylib -lm

game: main.c
	$(CC) main.c $(C_FLAGS) $(LD_FLAGS) -o $@

.PHONY: run
run: game
	./game

.PHONY: all
all: game

.PHONY: clean
clean:
	$(RM) game
