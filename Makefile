CC = gcc
CFLAGS = -Wall

all: sensor_analyzer_seq sensor_analyzer_par

sensor_analyzer_seq: sensor_analyzer_seq.c
	$(CC) sensor_analyzer_seq.c -o sensor_analyzer_seq $(CFLAGS) -lrt -lm

sensor_analyzer_par: sensor_analyzer_par.c
	$(CC) sensor_analyzer_par.c -o sensor_analyzer_par $(CFLAGS) -pthread -lm

clean:
	rm -f sensor_analyzer_seq sensor_analyzer_par