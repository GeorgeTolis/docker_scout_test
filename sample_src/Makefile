#
# In order to execute this "Makefile" just type "make"
#

MNGR_OBJS = fss_manager.o list.o queue.o utils.o
CONS_OBJS = fss_console.o utils.o
WORK_OBJS = worker.o queue.o utils.o

MNGR_SRC = fss_manager.c list.c utils.c queue.c
CONS_SRC = fss_console.c utils.c
WORK_SRC = worker.c queue.c utils.c

MNGR_OUT = fss_manager
CONS_OUT = fss_console
WORK_OUT = worker

CC	= gcc
FLAGS   = -g -c 
# -g option enables debugging mode 
# -c flag generates object code for separate files

# Create all executables
all: $(MNGR_OUT) $(CONS_OUT) $(WORK_OUT)

# Create manager/console individualy
$(MNGR_OUT): $(MNGR_OBJS)
	$(CC) -g $(MNGR_OBJS) -o $@

$(CONS_OUT): $(CONS_OBJS)
	$(CC) -g $(CONS_OBJS) -o $@

$(WORK_OUT): $(WORK_OBJS)
	$(CC) -g $(WORK_OBJS) -o $@

# Create/compile the individual files separately
%.o: %.c
	$(CC) $(FLAGS) $< -o $@
	
# Clean the objects and executables
clean:
	rm -f $(MNGR_OBJS) $(CONS_OBJS) $(WORK_OBJS) $(MNGR_OUT) $(CONS_OUT) $(WORK_OUT) fss_in fss_out

# do a bit of accounting
count:
	wc -l *.c *.h fss_script.sh README
