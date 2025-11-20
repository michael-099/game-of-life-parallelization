OBJS = life_openmp.o gnu.o gnuplot_i.o
CC = gcc
CFLAGS = -O3 -fopenmp -Wall

all: life

life: $(OBJS)
	$(CC) $(CFLAGS) -o life $(OBJS) -lm

life_openmp.o: life_openmp.c
	$(CC) $(CFLAGS) -c life_openmp.c

gnu.o: gnu.c gnu.h
	$(CC) $(CFLAGS) -c gnu.c

gnuplot_i.o: gnuplot_i.c gnuplot_i.h
	$(CC) $(CFLAGS) -c gnuplot_i.c

clean:
	rm -f *.o life
