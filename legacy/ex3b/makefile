CC = gcc
CFLAGS = -std=c99 -Wall -Wvla -Wextra

TWEETS_OBJ = tweets_generator.o markov_chain.o linked_list.o
SNAKES_OBJ = snakes_and_ladders.o markov_chain.o linked_list.o

.PHONY: all clean

all: tweets_generator snakes_and_ladders

tweets_generator: $(TWEETS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

snakes_and_ladders: $(SNAKES_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

linked_list.o: linked_list.c linked_list.h
	$(CC) $(CFLAGS) -c linked_list.c

markov_chain.o: markov_chain.c markov_chain.h linked_list.h
	$(CC) $(CFLAGS) -c markov_chain.c

tweets_generator.o: tweets_generator.c markov_chain.h linked_list.h
	$(CC) $(CFLAGS) -c tweets_generator.c

snakes_and_ladders.o: snakes_and_ladders.c markov_chain.h linked_list.h
	$(CC) $(CFLAGS) -c snakes_and_ladders.c

clean:
	rm -f *.o tweets_generator snakes_and_ladders