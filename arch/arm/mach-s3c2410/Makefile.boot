zreladdr-y	:= 0x30008000
params_phys-y	:= 0x30000100
ifeq ($(CONFIG_PM_H1940),y)
	zreladdr-y	:= 0x30108000
	params_phys-y	:= 0x30100100
endif
ifeq ($(CONFIG_MACH_N35),y)
	zreladdr-y	:= 0x30208000
	params_phys-y	:= 0x30200100
endif
