#Linux Makefile

#BLDTYPE = release
BLDTYPE = debug

export CXX = g++
export CC = gcc
ifeq ($(BLDTYPE),debug)
export CXXFLAGS += -Wall -g -D_REENTRANT -D_TUDEBUGTRACE -DPROVISION_IE
export CFLAGS += -Wall -Wnested-externs -g -D_REENTRANT -D_TUDEBUGTRACE -DPROVISION_IE
else
export CXXFLAGS += -Wall -D_REENTRANT -DPROVISION_IE
export CFLAGS += -Wall -Wnested-externs -D_REENTRANT -DPROVISION_IE
endif
export LDFLAGS = -r
export LIBPATH = -L/usr/local/lib
export INCLUDE = -I/usr/local/include

LIBS =  trace/tutrace.lib \
	slist/slist.lib \
	Portability/Portability.lib \
	UdpLib/UdpLib.lib \
	OobUfd/OobUfd.lib \
	InbEap/InbEap.lib \
	InbIp/InbIp.lib \
	InbWlan/InbWlan.lib \
	UPnPILib/UPnPILib.lib \
	UPnPCpLib/UPnPCpLib.lib \
	InbUPnPCp/InbUPnPCp.lib \
	UPnPDevLib/UPnPDevLib.lib \
	InbUPnPDev/InbUPnPDev.lib \
	Transport/Transport.lib \
	StateMachine/StateMachine.lib \
	RegProtocol/RegProtocol.lib \
	MasterControl/MasterControl.lib \
	TlvBase/WscTlvBase.lib\
	WscCmd/WscCmd.lib \
	-lpthread -lcurses -lc -lssl -lcrypto

default: 
	@cd trace && $(MAKE)
	@cd slist && $(MAKE)
	@cd Portability && $(MAKE)
	@cd TlvBase && $(MAKE)
	@cd UdpLib && $(MAKE)
	@cd OobUfd && $(MAKE)
	@cd InbEap && $(MAKE)
	@cd InbIp && $(MAKE)
	@cd InbWlan && $(MAKE)
	@cd UPnPILib && $(MAKE)
	@cd UPnPCpLib && $(MAKE)
	@cd InbUPnPCp && $(MAKE)
	@cd UPnPDevLib && $(MAKE)
	@cd InbUPnPDev && $(MAKE)
	@cd Transport && $(MAKE)
	@cd StateMachine && $(MAKE)
	@cd RegProtocol && $(MAKE)
	@cd MasterControl && $(MAKE)
	@cd WscCmd && $(MAKE)
	$(CXX) $(OBJS) $(LIBS) $(LIBPATH) -o wsccmd
ifeq ($(BLDTYPE),release)
	strip wsccmd
endif

clean:
	@cd trace && $(MAKE) clean
	@cd slist && $(MAKE) clean
	@cd Portability && $(MAKE) clean
	@cd TlvBase && $(MAKE) clean
	@cd UdpLib && $(MAKE) clean
	@cd OobUfd && $(MAKE) clean
	@cd InbEap && $(MAKE) clean
	@cd InbIp && $(MAKE) clean
	@cd InbWlan && $(MAKE) clean
	@cd UPnPILib && $(MAKE) clean
	@cd UPnPCpLib && $(MAKE) clean
	@cd InbUPnPCp && $(MAKE) clean
	@cd UPnPDevLib && $(MAKE) clean
	@cd InbUPnPDev && $(MAKE) clean
	@cd Transport && $(MAKE) clean
	@cd StateMachine && $(MAKE) clean
	@cd RegProtocol && $(MAKE) clean
	@cd MasterControl && $(MAKE) clean
	@cd WscCmd && $(MAKE) clean
	@rm -f wsccmd
