CC = gcc
CFLAGS = -Wall

all: sensor_analyzer_seq sensor_analyzer_par

sensor_analyzer_seq: sensor_analyzer_seq.c
	$(CC) -o sensor_analyzer_seq.c sensor_analyzer_seq $(CFLAGS)

sensor_analyzer_par: sensor_analyzer_par.c
	$(CC) -o sensor_analyzer_par.c sensor_analyzer_par $(CFLAGS) -pthread

clean:
	rm -f sensor_analyzer_seq sensor_analyzer_par