TARGET = sim_endurance
OBJS = main.o

CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lpspgu -lpspgum -lpspvfpu -lpspdisplay -lpspge -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Sim Endurance PSP

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
