include ../compiler.mak 


MSGLIBNAME=../../lib/libmsg.a
DAEMONNAME=../../bin/msgdaemon
PYTHONLIB=../../lib/_msg_bus.so

INCLUDEPATH=../../include

SYSROOT=$(shell $(CC) -print-sysroot )
INCLUDEFLAG=-I$(INCLUDEPATH) -I$(SYSROOT)/usr/include/python2.7
LIBFLAG=-L../../lib -lrt 
DEFINES=

CFLAGS+= $(DEFINES)  $(LIBFLAG) $(INCLUDEFLAG) 



OBJS= msg_bus.o
CFILE=msgdaemon.c
HFILE=$(INCLUDEPATH)/msg_bus.h msg_define.h
LINKLIB=  -lmsg  -lComTool -lapue -lPointPacket

.c.o:
	$(CC) $(CFLAGS) -c $<

# MakeAll dependencies
all:	lib	daemon python-interface

#make _msg_bus.so msg_bus.py
python-interface: $(PYTHONLIB)
$(PYTHONLIB):msg_bus.o msg_bus.i msg_bus_wrap.c msg_bus_wrap.o
	$(CC) -shared msg_bus.o msg_bus_wrap.o $(LINKLIB) $(CFLAGS) -o $@

msg_bus_wrap.c: msg_bus.i
	swig2.0 -threads -python -I../../include -outdir ../../lib msg_bus.i 

#make msglib
lib: $(MSGLIBNAME)

$(MSGLIBNAME):$(OBJS) $(HFILE)
	echo $(AR)
	$(AR) $(MSGLIBNAME) $(OBJS)


#make msg daemon
daemon:$(DAEMONNAME) 
	
$(DAEMONNAME): $(CFILE) $(HFILE)
	$(CC) $(CFILE) $(CFLAGS) $(LINKLIB) -o $(DAEMONNAME)
	$(CC) msgst.c $(LINKLIB) $(CFLAGS) -o ../../bin/msgst 

test: 
	$(CC) putmain.c $(LINKLIB) $(CFLAGS) -o put
	$(CC) getmain.c $(LINKLIB) $(CFLAGS) -o get
	$(CC) initmain.c $(LINKLIB) $(CFLAGS) -o init 
	#$(CC) puttcp.c $(LINKLIB) $(CFLAGS) -o puttcp
	#$(CC) gettcp.c $(LINKLIB) $(CFLAGS) -o gettcp

#clean
clean:
	rm -f *.o
	rm -f $(MSGLIBNAME)
	rm -f $(DAEMONNAME)
	rm -f msg_bus_wrap.c   
	rm -f $(PYTHONLIB)   
	rm -f ../../lib/msg_bus.py

		
