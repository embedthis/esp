#
#   esp-vxworks-default.mk -- Makefile to build Embedthis ESP for vxworks
#

NAME                  := esp
VERSION               := 5.4.1
PROFILE               ?= default
ARCH                  ?= $(shell echo $(WIND_HOST_TYPE) | sed 's/-.*$(ME_ROOT_PREFIX)/')
CPU                   ?= $(subst X86,PENTIUM,$(shell echo $(ARCH) | tr a-z A-Z))
OS                    ?= vxworks
CC                    ?= cc$(subst x86,pentium,$(ARCH))
LD                    ?= ld
CONFIG                ?= $(OS)-$(ARCH)-$(PROFILE)
BUILD                 ?= build/$(CONFIG)
LBIN                  ?= $(BUILD)/bin
PATH                  := $(LBIN):$(PATH)

ME_COM_COMPILER       ?= 1
ME_COM_EST            ?= 0
ME_COM_HTTP           ?= 1
ME_COM_LIB            ?= 1
ME_COM_LINK           ?= 1
ME_COM_MATRIXSSL      ?= 0
ME_COM_MDB            ?= 1
ME_COM_MPR            ?= 1
ME_COM_NANOSSL        ?= 0
ME_COM_OPENSSL        ?= 1
ME_COM_OSDEP          ?= 1
ME_COM_PCRE           ?= 1
ME_COM_SQLITE         ?= 1
ME_COM_SSL            ?= 1
ME_COM_VXWORKS        ?= 1
ME_COM_WINSDK         ?= 1

ME_COM_OPENSSL_PATH   ?= "/usr/lib"

ifeq ($(ME_COM_EST),1)
    ME_COM_SSL := 1
endif
ifeq ($(ME_COM_LIB),1)
    ME_COM_COMPILER := 1
endif
ifeq ($(ME_COM_LINK),1)
    ME_COM_COMPILER := 1
endif
ifeq ($(ME_COM_OPENSSL),1)
    ME_COM_SSL := 1
endif

export WIND_HOME      ?= $(WIND_BASE)/..
export PATH           := $(WIND_GNU_PATH)/$(WIND_HOST_TYPE)/bin:$(PATH)

CFLAGS                += -fno-builtin -fno-defer-pop -fvolatile -w
DFLAGS                += -DVXWORKS -DRW_MULTI_THREAD -D_GNU_TOOL -DCPU=PENTIUM $(patsubst %,-D%,$(filter ME_%,$(MAKEFLAGS))) -DME_COM_COMPILER=$(ME_COM_COMPILER) -DME_COM_EST=$(ME_COM_EST) -DME_COM_HTTP=$(ME_COM_HTTP) -DME_COM_LIB=$(ME_COM_LIB) -DME_COM_LINK=$(ME_COM_LINK) -DME_COM_MATRIXSSL=$(ME_COM_MATRIXSSL) -DME_COM_MDB=$(ME_COM_MDB) -DME_COM_MPR=$(ME_COM_MPR) -DME_COM_NANOSSL=$(ME_COM_NANOSSL) -DME_COM_OPENSSL=$(ME_COM_OPENSSL) -DME_COM_OSDEP=$(ME_COM_OSDEP) -DME_COM_PCRE=$(ME_COM_PCRE) -DME_COM_SQLITE=$(ME_COM_SQLITE) -DME_COM_SSL=$(ME_COM_SSL) -DME_COM_VXWORKS=$(ME_COM_VXWORKS) -DME_COM_WINSDK=$(ME_COM_WINSDK) 
IFLAGS                += "-I$(BUILD)/inc -I$(WIND_BASE)/target/h -I$(WIND_BASE)/target/h/wrn/coreip"
LDFLAGS               += '-Wl,-r'
LIBPATHS              += -L$(BUILD)/bin
LIBS                  += -lgcc

DEBUG                 ?= debug
CFLAGS-debug          ?= -g
DFLAGS-debug          ?= -DME_DEBUG
LDFLAGS-debug         ?= -g
DFLAGS-release        ?= 
CFLAGS-release        ?= -O2
LDFLAGS-release       ?= 
CFLAGS                += $(CFLAGS-$(DEBUG))
DFLAGS                += $(DFLAGS-$(DEBUG))
LDFLAGS               += $(LDFLAGS-$(DEBUG))

ME_ROOT_PREFIX        ?= deploy
ME_BASE_PREFIX        ?= $(ME_ROOT_PREFIX)
ME_DATA_PREFIX        ?= $(ME_VAPP_PREFIX)
ME_STATE_PREFIX       ?= $(ME_VAPP_PREFIX)
ME_BIN_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_INC_PREFIX         ?= $(ME_VAPP_PREFIX)/inc
ME_LIB_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_MAN_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_SBIN_PREFIX        ?= $(ME_VAPP_PREFIX)
ME_ETC_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_WEB_PREFIX         ?= $(ME_VAPP_PREFIX)/web
ME_LOG_PREFIX         ?= $(ME_VAPP_PREFIX)
ME_SPOOL_PREFIX       ?= $(ME_VAPP_PREFIX)
ME_CACHE_PREFIX       ?= $(ME_VAPP_PREFIX)
ME_APP_PREFIX         ?= $(ME_BASE_PREFIX)
ME_VAPP_PREFIX        ?= $(ME_APP_PREFIX)
ME_SRC_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/src/$(NAME)-$(VERSION)


TARGETS               += $(BUILD)/bin/ca.crt
ifeq ($(ME_COM_HTTP),1)
    TARGETS           += $(BUILD)/bin/libhttp.out
endif
ifeq ($(ME_COM_SQLITE),1)
    TARGETS           += $(BUILD)/bin/libsql.out
endif

unexport CDPATH

ifndef SHOW
.SILENT:
endif

all build compile: prep $(TARGETS)

.PHONY: prep

prep:
	@echo "      [Info] Use "make SHOW=1" to trace executed commands."
	@if [ "$(CONFIG)" = "" ] ; then echo WARNING: CONFIG not set ; exit 255 ; fi
	@if [ "$(ME_APP_PREFIX)" = "" ] ; then echo WARNING: ME_APP_PREFIX not set ; exit 255 ; fi
	@if [ "$(WIND_BASE)" = "" ] ; then echo WARNING: WIND_BASE not set. Run wrenv.sh. ; exit 255 ; fi
	@if [ "$(WIND_HOST_TYPE)" = "" ] ; then echo WARNING: WIND_HOST_TYPE not set. Run wrenv.sh. ; exit 255 ; fi
	@if [ "$(WIND_GNU_PATH)" = "" ] ; then echo WARNING: WIND_GNU_PATH not set. Run wrenv.sh. ; exit 255 ; fi
	@[ ! -x $(BUILD)/bin ] && mkdir -p $(BUILD)/bin; true
	@[ ! -x $(BUILD)/inc ] && mkdir -p $(BUILD)/inc; true
	@[ ! -x $(BUILD)/obj ] && mkdir -p $(BUILD)/obj; true
	@[ ! -f $(BUILD)/inc/me.h ] && cp projects/esp-vxworks-default-me.h $(BUILD)/inc/me.h ; true
	@if ! diff $(BUILD)/inc/me.h projects/esp-vxworks-default-me.h >/dev/null ; then\
		cp projects/esp-vxworks-default-me.h $(BUILD)/inc/me.h  ; \
	fi; true
	@if [ -f "$(BUILD)/.makeflags" ] ; then \
		if [ "$(MAKEFLAGS)" != "`cat $(BUILD)/.makeflags`" ] ; then \
			echo "   [Warning] Make flags have changed since the last build: "`cat $(BUILD)/.makeflags`"" ; \
		fi ; \
	fi
	@echo "$(MAKEFLAGS)" >$(BUILD)/.makeflags

clean:
	rm -f "$(BUILD)/obj/httpLib.o"
	rm -f "$(BUILD)/obj/mprLib.o"
	rm -f "$(BUILD)/obj/openssl.o"
	rm -f "$(BUILD)/obj/pcre.o"
	rm -f "$(BUILD)/obj/sqlite.o"
	rm -f "$(BUILD)/obj/sqlite3.o"
	rm -f "$(BUILD)/bin/ca.crt"
	rm -f "$(BUILD)/bin/libhttp.out"
	rm -f "$(BUILD)/bin/libmpr.out"
	rm -f "$(BUILD)/bin/libpcre.out"
	rm -f "$(BUILD)/bin/libsql.out"
	rm -f "$(BUILD)/bin/libopenssl.a"

clobber: clean
	rm -fr ./$(BUILD)

#
#   http.h
#
DEPS_1 += src/http/http.h

$(BUILD)/inc/http.h: $(DEPS_1)
	@echo '      [Copy] $(BUILD)/inc/http.h'
	mkdir -p "$(BUILD)/inc"
	cp src/http/http.h $(BUILD)/inc/http.h

#
#   me.h
#

$(BUILD)/inc/me.h: $(DEPS_2)

#
#   osdep.h
#
DEPS_3 += src/osdep/osdep.h
DEPS_3 += $(BUILD)/inc/me.h

$(BUILD)/inc/osdep.h: $(DEPS_3)
	@echo '      [Copy] $(BUILD)/inc/osdep.h'
	mkdir -p "$(BUILD)/inc"
	cp src/osdep/osdep.h $(BUILD)/inc/osdep.h

#
#   mpr.h
#
DEPS_4 += src/mpr/mpr.h
DEPS_4 += $(BUILD)/inc/me.h
DEPS_4 += $(BUILD)/inc/osdep.h

$(BUILD)/inc/mpr.h: $(DEPS_4)
	@echo '      [Copy] $(BUILD)/inc/mpr.h'
	mkdir -p "$(BUILD)/inc"
	cp src/mpr/mpr.h $(BUILD)/inc/mpr.h

#
#   pcre.h
#
DEPS_5 += src/pcre/pcre.h

$(BUILD)/inc/pcre.h: $(DEPS_5)
	@echo '      [Copy] $(BUILD)/inc/pcre.h'
	mkdir -p "$(BUILD)/inc"
	cp src/pcre/pcre.h $(BUILD)/inc/pcre.h

#
#   sqlite3.h
#
DEPS_6 += src/sqlite/sqlite3.h

$(BUILD)/inc/sqlite3.h: $(DEPS_6)
	@echo '      [Copy] $(BUILD)/inc/sqlite3.h'
	mkdir -p "$(BUILD)/inc"
	cp src/sqlite/sqlite3.h $(BUILD)/inc/sqlite3.h

#
#   http.h
#

src/http/http.h: $(DEPS_7)

#
#   httpLib.o
#
DEPS_8 += src/http/http.h
DEPS_8 += $(BUILD)/inc/pcre.h

$(BUILD)/obj/httpLib.o: \
    src/http/httpLib.c $(DEPS_8)
	@echo '   [Compile] $(BUILD)/obj/httpLib.o'
	$(CC) -c -o $(BUILD)/obj/httpLib.o $(CFLAGS) $(DFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(ME_COM_OPENSSL_PATH)/include" src/http/httpLib.c

#
#   mpr.h
#

src/mpr/mpr.h: $(DEPS_9)

#
#   mprLib.o
#
DEPS_10 += src/mpr/mpr.h

$(BUILD)/obj/mprLib.o: \
    src/mpr/mprLib.c $(DEPS_10)
	@echo '   [Compile] $(BUILD)/obj/mprLib.o'
	$(CC) -c -o $(BUILD)/obj/mprLib.o $(CFLAGS) $(DFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(ME_COM_OPENSSL_PATH)/include" src/mpr/mprLib.c

#
#   openssl.o
#
DEPS_11 += $(BUILD)/inc/mpr.h

$(BUILD)/obj/openssl.o: \
    src/mpr-openssl/openssl.c $(DEPS_11)
	@echo '   [Compile] $(BUILD)/obj/openssl.o'
	$(CC) -c -o $(BUILD)/obj/openssl.o $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" "-I$(BUILD)/inc" "-I$(ME_COM_OPENSSL_PATH)/include" src/mpr-openssl/openssl.c

#
#   pcre.h
#

src/pcre/pcre.h: $(DEPS_12)

#
#   pcre.o
#
DEPS_13 += $(BUILD)/inc/me.h
DEPS_13 += src/pcre/pcre.h

$(BUILD)/obj/pcre.o: \
    src/pcre/pcre.c $(DEPS_13)
	@echo '   [Compile] $(BUILD)/obj/pcre.o'
	$(CC) -c -o $(BUILD)/obj/pcre.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/pcre/pcre.c

#
#   sqlite3.h
#

src/sqlite/sqlite3.h: $(DEPS_14)

#
#   sqlite.o
#
DEPS_15 += $(BUILD)/inc/me.h
DEPS_15 += src/sqlite/sqlite3.h

$(BUILD)/obj/sqlite.o: \
    src/sqlite/sqlite.c $(DEPS_15)
	@echo '   [Compile] $(BUILD)/obj/sqlite.o'
	$(CC) -c -o $(BUILD)/obj/sqlite.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/sqlite/sqlite.c

#
#   sqlite3.o
#
DEPS_16 += $(BUILD)/inc/me.h
DEPS_16 += src/sqlite/sqlite3.h

$(BUILD)/obj/sqlite3.o: \
    src/sqlite/sqlite3.c $(DEPS_16)
	@echo '   [Compile] $(BUILD)/obj/sqlite3.o'
	$(CC) -c -o $(BUILD)/obj/sqlite3.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/sqlite/sqlite3.c

#
#   http-ca-crt
#
DEPS_17 += src/http/ca.crt

$(BUILD)/bin/ca.crt: $(DEPS_17)
	@echo '      [Copy] $(BUILD)/bin/ca.crt'
	mkdir -p "$(BUILD)/bin"
	cp src/http/ca.crt $(BUILD)/bin/ca.crt

ifeq ($(ME_COM_SSL),1)
#
#   openssl
#
DEPS_18 += $(BUILD)/obj/openssl.o

$(BUILD)/bin/libopenssl.a: $(DEPS_18)
	@echo '      [Link] $(BUILD)/bin/libopenssl.a'
	ar -cr $(BUILD)/bin/libopenssl.a "$(BUILD)/obj/openssl.o"
endif

#
#   libmpr
#
DEPS_19 += $(BUILD)/inc/osdep.h
ifeq ($(ME_COM_SSL),1)
    DEPS_19 += $(BUILD)/bin/libopenssl.a
endif
DEPS_19 += $(BUILD)/inc/mpr.h
DEPS_19 += $(BUILD)/obj/mprLib.o

ifeq ($(ME_COM_OPENSSL),1)
    LIBS_19 += -lssl
    LIBPATHS_19 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_19 += -lcrypto
    LIBPATHS_19 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_19 += -lopenssl
    LIBPATHS_19 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_EST),1)
    LIBS_19 += -lestssl
endif

$(BUILD)/bin/libmpr.out: $(DEPS_19)
	@echo '      [Link] $(BUILD)/bin/libmpr.out'
	$(CC) -r -o $(BUILD)/bin/libmpr.out $(LDFLAGS) $(LIBPATHS)  "$(BUILD)/obj/mprLib.o" $(LIBPATHS_19) $(LIBS_19) $(LIBS_19) $(LIBS) 

ifeq ($(ME_COM_PCRE),1)
#
#   libpcre
#
DEPS_20 += $(BUILD)/inc/pcre.h
DEPS_20 += $(BUILD)/obj/pcre.o

$(BUILD)/bin/libpcre.out: $(DEPS_20)
	@echo '      [Link] $(BUILD)/bin/libpcre.out'
	$(CC) -r -o $(BUILD)/bin/libpcre.out $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/pcre.o" $(LIBS) 
endif

ifeq ($(ME_COM_HTTP),1)
#
#   libhttp
#
DEPS_21 += $(BUILD)/bin/libmpr.out
ifeq ($(ME_COM_PCRE),1)
    DEPS_21 += $(BUILD)/bin/libpcre.out
endif
DEPS_21 += $(BUILD)/inc/http.h
DEPS_21 += $(BUILD)/obj/httpLib.o

ifeq ($(ME_COM_OPENSSL),1)
    LIBS_21 += -lssl
    LIBPATHS_21 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_21 += -lcrypto
    LIBPATHS_21 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_21 += -lopenssl
    LIBPATHS_21 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_EST),1)
    LIBS_21 += -lestssl
endif

$(BUILD)/bin/libhttp.out: $(DEPS_21)
	@echo '      [Link] $(BUILD)/bin/libhttp.out'
	$(CC) -r -o $(BUILD)/bin/libhttp.out $(LDFLAGS) $(LIBPATHS)  "$(BUILD)/obj/httpLib.o" $(LIBPATHS_21) $(LIBS_21) $(LIBS_21) $(LIBS) 
endif

ifeq ($(ME_COM_SQLITE),1)
#
#   libsql
#
DEPS_22 += $(BUILD)/inc/sqlite3.h
DEPS_22 += $(BUILD)/obj/sqlite3.o

$(BUILD)/bin/libsql.out: $(DEPS_22)
	@echo '      [Link] $(BUILD)/bin/libsql.out'
	$(CC) -r -o $(BUILD)/bin/libsql.out $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/sqlite3.o" $(LIBS) 
endif

#
#   installPrep
#

installPrep: $(DEPS_23)
	if [ "`id -u`" != 0 ] ; \
	then echo "Must run as root. Rerun with "sudo"" ; \
	exit 255 ; \
	fi

#
#   stop
#

stop: $(DEPS_24)

#
#   installBinary
#

installBinary: $(DEPS_25)

#
#   start
#

start: $(DEPS_26)

#
#   install
#
DEPS_27 += installPrep
DEPS_27 += stop
DEPS_27 += installBinary
DEPS_27 += start

install: $(DEPS_27)

#
#   uninstall
#
DEPS_28 += stop

uninstall: $(DEPS_28)

#
#   version
#

version: $(DEPS_29)
	echo $(VERSION)

