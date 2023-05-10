
#CC = gcc
CFLAGS += -fPIC -Wall -O0 -g -I. -I../libappf

TARGET=com2net
SRC = com2net.c af_client_start.c   termios.c
OBJ = $(SRC:.c=.o)
DEP = $(SRC:.c=.d)

all: $(TARGET)

$(DEP):%.d:%.c
	$(CC) $(CFLAGS) -MM $< >$@

include $(DEP)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ -L../../libappf2/src -lappf -lm -lrt

clean:
	-$(RM) $(TARGET) $(OBJ) $(DEP)
