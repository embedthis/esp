#
#   esp-vxworks-default.mk -- Makefile to build Embedthis ESP for vxworks
#

NAME                  := esp
VERSION               := 5.4.0
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

ME_COM_CGI            ?= 0
ME_COM_COMPILER       ?= 1
ME_COM_DIR            ?= 0
ME_COM_EST            ?= 0
ME_COM_HTTP           ?= 1
ME_COM_LIB            ?= 1
ME_COM_LINK           ?= 1
ME_COM_MDB            ?= 1
ME_COM_MPR            ?= 1
ME_COM_OPENSSL        ?= 1
ME_COM_OSDEP          ?= 1
ME_COM_PCRE           ?= 1
ME_COM_SQLITE         ?= 1
ME_COM_SSL            ?= 1
ME_COM_VXWORKS        ?= 1
ME_COM_WINSDK         ?= 1

ME_COM_OPENSSL_PATH   ?= "/usr"

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
DFLAGS                += -DVXWORKS -DRW_MULTI_THREAD -D_GNU_TOOL -DCPU=PENTIUM $(patsubst %,-D%,$(filter ME_%,$(MAKEFLAGS))) -DME_COM_CGI=$(ME_COM_CGI) -DME_COM_COMPILER=$(ME_COM_COMPILER) -DME_COM_DIR=$(ME_COM_DIR) -DME_COM_EST=$(ME_COM_EST) -DME_COM_HTTP=$(ME_COM_HTTP) -DME_COM_LIB=$(ME_COM_LIB) -DME_COM_LINK=$(ME_COM_LINK) -DME_COM_MDB=$(ME_COM_MDB) -DME_COM_MPR=$(ME_COM_MPR) -DME_COM_OPENSSL=$(ME_COM_OPENSSL) -DME_COM_OSDEP=$(ME_COM_OSDEP) -DME_COM_PCRE=$(ME_COM_PCRE) -DME_COM_SQLITE=$(ME_COM_SQLITE) -DME_COM_SSL=$(ME_COM_SSL) -DME_COM_VXWORKS=$(ME_COM_VXWORKS) -DME_COM_WINSDK=$(ME_COM_WINSDK) 
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


TARGETS               += $(BUILD)/bin/esp-compile.json
TARGETS               += $(BUILD)/bin/esp.out
TARGETS               += $(BUILD)/bin/ca.crt
ifeq ($(ME_COM_HTTP),1)
    TARGETS           += $(BUILD)/bin/http.out
endif
TARGETS               += $(BUILD)/bin/libmprssl.out
TARGETS               += $(BUILD)/bin/espman.out

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
	rm -f "$(BUILD)/obj/edi.o"
	rm -f "$(BUILD)/obj/esp.o"
	rm -f "$(BUILD)/obj/espAbbrev.o"
	rm -f "$(BUILD)/obj/espConfig.o"
	rm -f "$(BUILD)/obj/espFramework.o"
	rm -f "$(BUILD)/obj/espHtml.o"
	rm -f "$(BUILD)/obj/espRequest.o"
	rm -f "$(BUILD)/obj/espTemplate.o"
	rm -f "$(BUILD)/obj/http.o"
	rm -f "$(BUILD)/obj/httpLib.o"
	rm -f "$(BUILD)/obj/makerom.o"
	rm -f "$(BUILD)/obj/manager.o"
	rm -f "$(BUILD)/obj/mdb.o"
	rm -f "$(BUILD)/obj/mprLib.o"
	rm -f "$(BUILD)/obj/mprSsl.o"
	rm -f "$(BUILD)/obj/pcre.o"
	rm -f "$(BUILD)/obj/sdb.o"
	rm -f "$(BUILD)/obj/sqlite.o"
	rm -f "$(BUILD)/obj/sqlite3.o"
	rm -f "$(BUILD)/bin/esp-compile.json"
	rm -f "$(BUILD)/bin/esp.out"
	rm -f "$(BUILD)/bin/ca.crt"
	rm -f "$(BUILD)/bin/http.out"
	rm -f "$(BUILD)/bin/libesp.out"
	rm -f "$(BUILD)/bin/libhttp.out"
	rm -f "$(BUILD)/bin/libmpr.out"
	rm -f "$(BUILD)/bin/libmprssl.out"
	rm -f "$(BUILD)/bin/libpcre.out"
	rm -f "$(BUILD)/bin/libsql.out"
	rm -f "$(BUILD)/bin/espman.out"

clobber: clean
	rm -fr ./$(BUILD)

#
#   edi.h
#
DEPS_1 += src/edi.h

$(BUILD)/inc/edi.h: $(DEPS_1)
	@echo '      [Copy] $(BUILD)/inc/edi.h'
	mkdir -p "$(BUILD)/inc"
	cp src/edi.h $(BUILD)/inc/edi.h

#
#   esp.h
#
DEPS_2 += src/esp.h

$(BUILD)/inc/esp.h: $(DEPS_2)
	@echo '      [Copy] $(BUILD)/inc/esp.h'
	mkdir -p "$(BUILD)/inc"
	cp src/esp.h $(BUILD)/inc/esp.h

#
#   me.h
#

$(BUILD)/inc/me.h: $(DEPS_3)

#
#   osdep.h
#
DEPS_4 += paks/osdep/dist/osdep.h
DEPS_4 += $(BUILD)/inc/me.h

$(BUILD)/inc/osdep.h: $(DEPS_4)
	@echo '      [Copy] $(BUILD)/inc/osdep.h'
	mkdir -p "$(BUILD)/inc"
	cp paks/osdep/dist/osdep.h $(BUILD)/inc/osdep.h

#
#   mpr.h
#
DEPS_5 += paks/mpr/dist/mpr.h
DEPS_5 += $(BUILD)/inc/me.h
DEPS_5 += $(BUILD)/inc/osdep.h

$(BUILD)/inc/mpr.h: $(DEPS_5)
	@echo '      [Copy] $(BUILD)/inc/mpr.h'
	mkdir -p "$(BUILD)/inc"
	cp paks/mpr/dist/mpr.h $(BUILD)/inc/mpr.h

#
#   http.h
#
DEPS_6 += paks/http/dist/http.h
DEPS_6 += $(BUILD)/inc/mpr.h

$(BUILD)/inc/http.h: $(DEPS_6)
	@echo '      [Copy] $(BUILD)/inc/http.h'
	mkdir -p "$(BUILD)/inc"
	cp paks/http/dist/http.h $(BUILD)/inc/http.h

#
#   mdb.h
#
DEPS_7 += src/mdb.h

$(BUILD)/inc/mdb.h: $(DEPS_7)
	@echo '      [Copy] $(BUILD)/inc/mdb.h'
	mkdir -p "$(BUILD)/inc"
	cp src/mdb.h $(BUILD)/inc/mdb.h

#
#   pcre.h
#
DEPS_8 += paks/pcre/dist/pcre.h

$(BUILD)/inc/pcre.h: $(DEPS_8)
	@echo '      [Copy] $(BUILD)/inc/pcre.h'
	mkdir -p "$(BUILD)/inc"
	cp paks/pcre/dist/pcre.h $(BUILD)/inc/pcre.h

#
#   sqlite3.h
#
DEPS_9 += paks/sqlite/dist/sqlite3.h

$(BUILD)/inc/sqlite3.h: $(DEPS_9)
	@echo '      [Copy] $(BUILD)/inc/sqlite3.h'
	mkdir -p "$(BUILD)/inc"
	cp paks/sqlite/dist/sqlite3.h $(BUILD)/inc/sqlite3.h

#
#   edi.h
#

src/edi.h: $(DEPS_10)

#
#   edi.o
#
DEPS_11 += src/edi.h
DEPS_11 += $(BUILD)/inc/pcre.h

$(BUILD)/obj/edi.o: \
    src/edi.c $(DEPS_11)
	@echo '   [Compile] $(BUILD)/obj/edi.o'
	$(CC) -c -o $(BUILD)/obj/edi.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/edi.c

#
#   esp.h
#

src/esp.h: $(DEPS_12)

#
#   esp.o
#
DEPS_13 += src/esp.h

$(BUILD)/obj/esp.o: \
    src/esp.c $(DEPS_13)
	@echo '   [Compile] $(BUILD)/obj/esp.o'
	$(CC) -c -o $(BUILD)/obj/esp.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/esp.c

#
#   espAbbrev.o
#
DEPS_14 += src/esp.h

$(BUILD)/obj/espAbbrev.o: \
    src/espAbbrev.c $(DEPS_14)
	@echo '   [Compile] $(BUILD)/obj/espAbbrev.o'
	$(CC) -c -o $(BUILD)/obj/espAbbrev.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/espAbbrev.c

#
#   espConfig.o
#
DEPS_15 += src/esp.h

$(BUILD)/obj/espConfig.o: \
    src/espConfig.c $(DEPS_15)
	@echo '   [Compile] $(BUILD)/obj/espConfig.o'
	$(CC) -c -o $(BUILD)/obj/espConfig.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/espConfig.c

#
#   espFramework.o
#
DEPS_16 += src/esp.h

$(BUILD)/obj/espFramework.o: \
    src/espFramework.c $(DEPS_16)
	@echo '   [Compile] $(BUILD)/obj/espFramework.o'
	$(CC) -c -o $(BUILD)/obj/espFramework.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/espFramework.c

#
#   espHtml.o
#
DEPS_17 += src/esp.h
DEPS_17 += src/edi.h

$(BUILD)/obj/espHtml.o: \
    src/espHtml.c $(DEPS_17)
	@echo '   [Compile] $(BUILD)/obj/espHtml.o'
	$(CC) -c -o $(BUILD)/obj/espHtml.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/espHtml.c

#
#   espRequest.o
#
DEPS_18 += src/esp.h

$(BUILD)/obj/espRequest.o: \
    src/espRequest.c $(DEPS_18)
	@echo '   [Compile] $(BUILD)/obj/espRequest.o'
	$(CC) -c -o $(BUILD)/obj/espRequest.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/espRequest.c

#
#   espTemplate.o
#
DEPS_19 += src/esp.h

$(BUILD)/obj/espTemplate.o: \
    src/espTemplate.c $(DEPS_19)
	@echo '   [Compile] $(BUILD)/obj/espTemplate.o'
	$(CC) -c -o $(BUILD)/obj/espTemplate.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/espTemplate.c

#
#   http.h
#

paks/http/dist/http.h: $(DEPS_20)

#
#   http.o
#
DEPS_21 += paks/http/dist/http.h

$(BUILD)/obj/http.o: \
    paks/http/dist/http.c $(DEPS_21)
	@echo '   [Compile] $(BUILD)/obj/http.o'
	$(CC) -c -o $(BUILD)/obj/http.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" paks/http/dist/http.c

#
#   httpLib.o
#
DEPS_22 += paks/http/dist/http.h

$(BUILD)/obj/httpLib.o: \
    paks/http/dist/httpLib.c $(DEPS_22)
	@echo '   [Compile] $(BUILD)/obj/httpLib.o'
	$(CC) -c -o $(BUILD)/obj/httpLib.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" paks/http/dist/httpLib.c

#
#   mpr.h
#

paks/mpr/dist/mpr.h: $(DEPS_23)

#
#   makerom.o
#
DEPS_24 += paks/mpr/dist/mpr.h

$(BUILD)/obj/makerom.o: \
    paks/mpr/dist/makerom.c $(DEPS_24)
	@echo '   [Compile] $(BUILD)/obj/makerom.o'
	$(CC) -c -o $(BUILD)/obj/makerom.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" paks/mpr/dist/makerom.c

#
#   manager.o
#
DEPS_25 += paks/mpr/dist/mpr.h

$(BUILD)/obj/manager.o: \
    paks/mpr/dist/manager.c $(DEPS_25)
	@echo '   [Compile] $(BUILD)/obj/manager.o'
	$(CC) -c -o $(BUILD)/obj/manager.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" paks/mpr/dist/manager.c

#
#   mdb.h
#

src/mdb.h: $(DEPS_26)

#
#   mdb.o
#
DEPS_27 += $(BUILD)/inc/http.h
DEPS_27 += src/edi.h
DEPS_27 += src/mdb.h
DEPS_27 += $(BUILD)/inc/pcre.h

$(BUILD)/obj/mdb.o: \
    src/mdb.c $(DEPS_27)
	@echo '   [Compile] $(BUILD)/obj/mdb.o'
	$(CC) -c -o $(BUILD)/obj/mdb.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/mdb.c

#
#   mprLib.o
#
DEPS_28 += paks/mpr/dist/mpr.h

$(BUILD)/obj/mprLib.o: \
    paks/mpr/dist/mprLib.c $(DEPS_28)
	@echo '   [Compile] $(BUILD)/obj/mprLib.o'
	$(CC) -c -o $(BUILD)/obj/mprLib.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" paks/mpr/dist/mprLib.c

#
#   mprSsl.o
#
DEPS_29 += paks/mpr/dist/mpr.h

$(BUILD)/obj/mprSsl.o: \
    paks/mpr/dist/mprSsl.c $(DEPS_29)
	@echo '   [Compile] $(BUILD)/obj/mprSsl.o'
	$(CC) -c -o $(BUILD)/obj/mprSsl.o $(CFLAGS) $(DFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" "-I$(ME_COM_OPENSSL_PATH)/include" paks/mpr/dist/mprSsl.c

#
#   pcre.h
#

paks/pcre/dist/pcre.h: $(DEPS_30)

#
#   pcre.o
#
DEPS_31 += $(BUILD)/inc/me.h
DEPS_31 += paks/pcre/dist/pcre.h

$(BUILD)/obj/pcre.o: \
    paks/pcre/dist/pcre.c $(DEPS_31)
	@echo '   [Compile] $(BUILD)/obj/pcre.o'
	$(CC) -c -o $(BUILD)/obj/pcre.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" paks/pcre/dist/pcre.c

#
#   sdb.o
#
DEPS_32 += $(BUILD)/inc/http.h
DEPS_32 += src/edi.h

$(BUILD)/obj/sdb.o: \
    src/sdb.c $(DEPS_32)
	@echo '   [Compile] $(BUILD)/obj/sdb.o'
	$(CC) -c -o $(BUILD)/obj/sdb.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" src/sdb.c

#
#   sqlite3.h
#

paks/sqlite/dist/sqlite3.h: $(DEPS_33)

#
#   sqlite.o
#
DEPS_34 += $(BUILD)/inc/me.h
DEPS_34 += paks/sqlite/dist/sqlite3.h

$(BUILD)/obj/sqlite.o: \
    paks/sqlite/dist/sqlite.c $(DEPS_34)
	@echo '   [Compile] $(BUILD)/obj/sqlite.o'
	$(CC) -c -o $(BUILD)/obj/sqlite.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" paks/sqlite/dist/sqlite.c

#
#   sqlite3.o
#
DEPS_35 += $(BUILD)/inc/me.h
DEPS_35 += paks/sqlite/dist/sqlite3.h

$(BUILD)/obj/sqlite3.o: \
    paks/sqlite/dist/sqlite3.c $(DEPS_35)
	@echo '   [Compile] $(BUILD)/obj/sqlite3.o'
	$(CC) -c -o $(BUILD)/obj/sqlite3.o $(CFLAGS) $(DFLAGS) "-I$(BUILD)/inc" "-I$(WIND_BASE)/target/h" "-I$(WIND_BASE)/target/h/wrn/coreip" paks/sqlite/dist/sqlite3.c

#
#   esp-compile.json
#
DEPS_36 += src/esp-compile.json

$(BUILD)/bin/esp-compile.json: $(DEPS_36)
	@echo '      [Copy] $(BUILD)/bin/esp-compile.json'
	mkdir -p "$(BUILD)/bin"
	cp src/esp-compile.json $(BUILD)/bin/esp-compile.json

ifeq ($(ME_COM_SQLITE),1)
#
#   libsql
#
DEPS_37 += $(BUILD)/inc/sqlite3.h
DEPS_37 += $(BUILD)/obj/sqlite3.o

$(BUILD)/bin/libsql.out: $(DEPS_37)
	@echo '      [Link] $(BUILD)/bin/libsql.out'
	$(CC) -r -o $(BUILD)/bin/libsql.out $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/sqlite3.o" $(LIBS) 
endif

#
#   libmpr
#
DEPS_38 += $(BUILD)/inc/osdep.h
DEPS_38 += $(BUILD)/inc/mpr.h
DEPS_38 += $(BUILD)/obj/mprLib.o

$(BUILD)/bin/libmpr.out: $(DEPS_38)
	@echo '      [Link] $(BUILD)/bin/libmpr.out'
	$(CC) -r -o $(BUILD)/bin/libmpr.out $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/mprLib.o" $(LIBS) 

ifeq ($(ME_COM_PCRE),1)
#
#   libpcre
#
DEPS_39 += $(BUILD)/inc/pcre.h
DEPS_39 += $(BUILD)/obj/pcre.o

$(BUILD)/bin/libpcre.out: $(DEPS_39)
	@echo '      [Link] $(BUILD)/bin/libpcre.out'
	$(CC) -r -o $(BUILD)/bin/libpcre.out $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/pcre.o" $(LIBS) 
endif

ifeq ($(ME_COM_HTTP),1)
#
#   libhttp
#
DEPS_40 += $(BUILD)/bin/libmpr.out
ifeq ($(ME_COM_PCRE),1)
    DEPS_40 += $(BUILD)/bin/libpcre.out
endif
DEPS_40 += $(BUILD)/inc/http.h
DEPS_40 += $(BUILD)/obj/httpLib.o

$(BUILD)/bin/libhttp.out: $(DEPS_40)
	@echo '      [Link] $(BUILD)/bin/libhttp.out'
	$(CC) -r -o $(BUILD)/bin/libhttp.out $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/httpLib.o" $(LIBS) 
endif

#
#   libesp
#
ifeq ($(ME_COM_SQLITE),1)
    DEPS_41 += $(BUILD)/bin/libsql.out
endif
ifeq ($(ME_COM_HTTP),1)
    DEPS_41 += $(BUILD)/bin/libhttp.out
endif
DEPS_41 += $(BUILD)/inc/edi.h
DEPS_41 += $(BUILD)/inc/esp.h
DEPS_41 += $(BUILD)/inc/mdb.h
DEPS_41 += $(BUILD)/obj/edi.o
DEPS_41 += $(BUILD)/obj/espAbbrev.o
DEPS_41 += $(BUILD)/obj/espConfig.o
DEPS_41 += $(BUILD)/obj/espFramework.o
DEPS_41 += $(BUILD)/obj/espHtml.o
DEPS_41 += $(BUILD)/obj/espRequest.o
DEPS_41 += $(BUILD)/obj/espTemplate.o
DEPS_41 += $(BUILD)/obj/mdb.o
DEPS_41 += $(BUILD)/obj/sdb.o

$(BUILD)/bin/libesp.out: $(DEPS_41)
	@echo '      [Link] $(BUILD)/bin/libesp.out'
	$(CC) -r -o $(BUILD)/bin/libesp.out $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/edi.o" "$(BUILD)/obj/espAbbrev.o" "$(BUILD)/obj/espConfig.o" "$(BUILD)/obj/espFramework.o" "$(BUILD)/obj/espHtml.o" "$(BUILD)/obj/espRequest.o" "$(BUILD)/obj/espTemplate.o" "$(BUILD)/obj/mdb.o" "$(BUILD)/obj/sdb.o" $(LIBS) 

#
#   espcmd
#
ifeq ($(ME_COM_SQLITE),1)
    DEPS_42 += $(BUILD)/bin/libsql.out
endif
DEPS_42 += $(BUILD)/bin/libesp.out
DEPS_42 += $(BUILD)/obj/esp.o

$(BUILD)/bin/esp.out: $(DEPS_42)
	@echo '      [Link] $(BUILD)/bin/esp.out'
	$(CC) -o $(BUILD)/bin/esp.out $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/esp.o" $(LIBS) -Wl,-r 

#
#   http-ca-crt
#
DEPS_43 += paks/http/dist/ca.crt

$(BUILD)/bin/ca.crt: $(DEPS_43)
	@echo '      [Copy] $(BUILD)/bin/ca.crt'
	mkdir -p "$(BUILD)/bin"
	cp paks/http/dist/ca.crt $(BUILD)/bin/ca.crt

ifeq ($(ME_COM_HTTP),1)
#
#   httpcmd
#
DEPS_44 += $(BUILD)/bin/libhttp.out
DEPS_44 += $(BUILD)/obj/http.o

$(BUILD)/bin/http.out: $(DEPS_44)
	@echo '      [Link] $(BUILD)/bin/http.out'
	$(CC) -o $(BUILD)/bin/http.out $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/http.o" $(LIBS) -Wl,-r 
endif

#
#   libmprssl
#
DEPS_45 += $(BUILD)/bin/libmpr.out
DEPS_45 += $(BUILD)/obj/mprSsl.o

ifeq ($(ME_COM_OPENSSL),1)
    LIBS_45 += -lssl
    LIBPATHS_45 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_45 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_45 += -lcrypto
    LIBPATHS_45 += -L"$(ME_COM_OPENSSL_PATH)/lib"
    LIBPATHS_45 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/libmprssl.out: $(DEPS_45)
	@echo '      [Link] $(BUILD)/bin/libmprssl.out'
	$(CC) -r -o $(BUILD)/bin/libmprssl.out $(LDFLAGS) $(LIBPATHS)   "$(BUILD)/obj/mprSsl.o" $(LIBPATHS_45) $(LIBS_45) $(LIBS_45) $(LIBS) 

#
#   manager
#
DEPS_46 += $(BUILD)/bin/libmpr.out
DEPS_46 += $(BUILD)/obj/manager.o

$(BUILD)/bin/espman.out: $(DEPS_46)
	@echo '      [Link] $(BUILD)/bin/espman.out'
	$(CC) -o $(BUILD)/bin/espman.out $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/manager.o" $(LIBS) -Wl,-r 

#
#   installPrep
#

installPrep: $(DEPS_47)
	if [ "`id -u`" != 0 ] ; \
	then echo "Must run as root. Rerun with "sudo"" ; \
	exit 255 ; \
	fi

#
#   stop
#

stop: $(DEPS_48)

#
#   installBinary
#

installBinary: $(DEPS_49)

#
#   start
#

start: $(DEPS_50)

#
#   install
#
DEPS_51 += installPrep
DEPS_51 += stop
DEPS_51 += installBinary
DEPS_51 += start

install: $(DEPS_51)

#
#   uninstall
#
DEPS_52 += stop

uninstall: $(DEPS_52)

#
#   version
#

version: $(DEPS_53)
	echo 5.4.0

