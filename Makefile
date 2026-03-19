CC = gcc
CFLAGS = -Wall

all: log_analyzer_seq log_analyzer_par

log_analyzer_seq: log_analyzer_seq.c
	$(CC) -o log_analyzer_seq.c log_analyzer_seq $(CFLAGS)

log_analyzer_par: log_analyzer_par.c
	$(CC) -o log_analyzer_par.c log_analyzer_par $(CFLAGS) -pthread

clean:
	rm -f log_analyzer_seq log_analyzer_par