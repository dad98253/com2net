
#CC = gcc
CFLAGS += -fPIC -Wall -O3 -g -I. -I../libappf -UHAVE_CONFIG_H
#CFLAGS += -fPIC -Wall -O0 -g3 -I. -I../../libappf2/src -DDEBUG -UHAVE_CONFIG_H

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
