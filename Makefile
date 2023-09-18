game: game.c
	clang -std=c99 -D_XOPEN_SOURCE=700 -Os game.c `pkg-config --cflags --libs chafa` -lncursesw -lm -o game
clean:
	rm game

