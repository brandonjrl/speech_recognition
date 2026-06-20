CC = gcc
CFLAGS = -Wall -I../components/aura_sr/include
TARGET = aura_sim
SRCS = main_sim.c aura_sr_sim.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
