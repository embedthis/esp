#
#   esp-freebsd-default.mk -- Makefile to build Embedthis ESP for freebsd
#

NAME                  := esp
VERSION               := 5.4.0
PROFILE               ?= default
ARCH                  ?= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/arm.*/arm/;s/mips.*/mips/')
CC_ARCH               ?= $(shell echo $(ARCH) | sed 's/x86/i686/;s/x64/x86_64/')
OS                    ?= freebsd
CC                    ?= gcc
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
ME_COM_MDB            ?= 1
ME_COM_MPR            ?= 1
ME_COM_OPENSSL        ?= 1
ME_COM_OSDEP          ?= 1
ME_COM_PCRE           ?= 1
ME_COM_SQLITE         ?= 1
ME_COM_SSL            ?= 1
ME_COM_VXWORKS        ?= 0
ME_COM_WINSDK         ?= 1

ME_COM_OPENSSL_PATH   ?= "/usr"

ifeq ($(ME_COM_EST),1)
    ME_COM_SSL := 1
endif
ifeq ($(ME_COM_LIB),1)
    ME_COM_COMPILER := 1
endif
ifeq ($(ME_COM_OPENSSL),1)
    ME_COM_SSL := 1
endif

CFLAGS                += -fPIC -w
DFLAGS                += -D_REENTRANT -DPIC $(patsubst %,-D%,$(filter ME_%,$(MAKEFLAGS))) -DME_COM_CGI=$(ME_COM_CGI) -DME_COM_COMPILER=$(ME_COM_COMPILER) -DME_COM_DIR=$(ME_COM_DIR) -DME_COM_EST=$(ME_COM_EST) -DME_COM_HTTP=$(ME_COM_HTTP) -DME_COM_LIB=$(ME_COM_LIB) -DME_COM_MDB=$(ME_COM_MDB) -DME_COM_MPR=$(ME_COM_MPR) -DME_COM_OPENSSL=$(ME_COM_OPENSSL) -DME_COM_OSDEP=$(ME_COM_OSDEP) -DME_COM_PCRE=$(ME_COM_PCRE) -DME_COM_SQLITE=$(ME_COM_SQLITE) -DME_COM_SSL=$(ME_COM_SSL) -DME_COM_VXWORKS=$(ME_COM_VXWORKS) -DME_COM_WINSDK=$(ME_COM_WINSDK) 
IFLAGS                += "-I$(BUILD)/inc"
LDFLAGS               += 
LIBPATHS              += -L$(BUILD)/bin
LIBS                  += -ldl -lpthread -lm

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

ME_ROOT_PREFIX        ?= 
ME_BASE_PREFIX        ?= $(ME_ROOT_PREFIX)/usr/local
ME_DATA_PREFIX        ?= $(ME_ROOT_PREFIX)/
ME_STATE_PREFIX       ?= $(ME_ROOT_PREFIX)/var
ME_APP_PREFIX         ?= $(ME_BASE_PREFIX)/lib/$(NAME)
ME_VAPP_PREFIX        ?= $(ME_APP_PREFIX)/$(VERSION)
ME_BIN_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/bin
ME_INC_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/include
ME_LIB_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/lib
ME_MAN_PREFIX         ?= $(ME_ROOT_PREFIX)/usr/local/share/man
ME_SBIN_PREFIX        ?= $(ME_ROOT_PREFIX)/usr/local/sbin
ME_ETC_PREFIX         ?= $(ME_ROOT_PREFIX)/etc/$(NAME)
ME_WEB_PREFIX         ?= $(ME_ROOT_PREFIX)/var/www/$(NAME)
ME_LOG_PREFIX         ?= $(ME_ROOT_PREFIX)/var/log/$(NAME)
ME_SPOOL_PREFIX       ?= $(ME_ROOT_PREFIX)/var/spool/$(NAME)
ME_CACHE_PREFIX       ?= $(ME_ROOT_PREFIX)/var/spool/$(NAME)/cache
ME_SRC_PREFIX         ?= $(ME_ROOT_PREFIX)$(NAME)-$(VERSION)


TARGETS               += $(BUILD)/bin/esp-compile.json
TARGETS               += $(BUILD)/bin/esp
TARGETS               += $(BUILD)/bin/ca.crt
ifeq ($(ME_COM_HTTP),1)
    TARGETS           += $(BUILD)/bin/http
endif
TARGETS               += $(BUILD)/bin/libmprssl.so
TARGETS               += $(BUILD)/bin/espman

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
	@[ ! -x $(BUILD)/bin ] && mkdir -p $(BUILD)/bin; true
	@[ ! -x $(BUILD)/inc ] && mkdir -p $(BUILD)/inc; true
	@[ ! -x $(BUILD)/obj ] && mkdir -p $(BUILD)/obj; true
	@[ ! -f $(BUILD)/inc/me.h ] && cp projects/esp-freebsd-default-me.h $(BUILD)/inc/me.h ; true
	@if ! diff $(BUILD)/inc/me.h projects/esp-freebsd-default-me.h >/dev/null ; then\
		cp projects/esp-freebsd-default-me.h $(BUILD)/inc/me.h  ; \
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
	rm -f "$(BUILD)/bin/esp"
	rm -f "$(BUILD)/bin/ca.crt"
	rm -f "$(BUILD)/bin/http"
	rm -f "$(BUILD)/bin/libesp.so"
	rm -f "$(BUILD)/bin/libhttp.so"
	rm -f "$(BUILD)/bin/libmpr.so"
	rm -f "$(BUILD)/bin/libmprssl.so"
	rm -f "$(BUILD)/bin/libpcre.so"
	rm -f "$(BUILD)/bin/libsql.so"
	rm -f "$(BUILD)/bin/espman"

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
	$(CC) -c -o $(BUILD)/obj/edi.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) src/edi.c

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
	$(CC) -c -o $(BUILD)/obj/esp.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) src/esp.c

#
#   espAbbrev.o
#
DEPS_14 += src/esp.h

$(BUILD)/obj/espAbbrev.o: \
    src/espAbbrev.c $(DEPS_14)
	@echo '   [Compile] $(BUILD)/obj/espAbbrev.o'
	$(CC) -c -o $(BUILD)/obj/espAbbrev.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) src/espAbbrev.c

#
#   espConfig.o
#
DEPS_15 += src/esp.h

$(BUILD)/obj/espConfig.o: \
    src/espConfig.c $(DEPS_15)
	@echo '   [Compile] $(BUILD)/obj/espConfig.o'
	$(CC) -c -o $(BUILD)/obj/espConfig.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) src/espConfig.c

#
#   espFramework.o
#
DEPS_16 += src/esp.h

$(BUILD)/obj/espFramework.o: \
    src/espFramework.c $(DEPS_16)
	@echo '   [Compile] $(BUILD)/obj/espFramework.o'
	$(CC) -c -o $(BUILD)/obj/espFramework.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) src/espFramework.c

#
#   espHtml.o
#
DEPS_17 += src/esp.h
DEPS_17 += src/edi.h

$(BUILD)/obj/espHtml.o: \
    src/espHtml.c $(DEPS_17)
	@echo '   [Compile] $(BUILD)/obj/espHtml.o'
	$(CC) -c -o $(BUILD)/obj/espHtml.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) src/espHtml.c

#
#   espRequest.o
#
DEPS_18 += src/esp.h

$(BUILD)/obj/espRequest.o: \
    src/espRequest.c $(DEPS_18)
	@echo '   [Compile] $(BUILD)/obj/espRequest.o'
	$(CC) -c -o $(BUILD)/obj/espRequest.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) src/espRequest.c

#
#   espTemplate.o
#
DEPS_19 += src/esp.h

$(BUILD)/obj/espTemplate.o: \
    src/espTemplate.c $(DEPS_19)
	@echo '   [Compile] $(BUILD)/obj/espTemplate.o'
	$(CC) -c -o $(BUILD)/obj/espTemplate.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) src/espTemplate.c

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
	$(CC) -c -o $(BUILD)/obj/http.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) paks/http/dist/http.c

#
#   httpLib.o
#
DEPS_22 += paks/http/dist/http.h

$(BUILD)/obj/httpLib.o: \
    paks/http/dist/httpLib.c $(DEPS_22)
	@echo '   [Compile] $(BUILD)/obj/httpLib.o'
	$(CC) -c -o $(BUILD)/obj/httpLib.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) paks/http/dist/httpLib.c

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
	$(CC) -c -o $(BUILD)/obj/makerom.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) paks/mpr/dist/makerom.c

#
#   manager.o
#
DEPS_25 += paks/mpr/dist/mpr.h

$(BUILD)/obj/manager.o: \
    paks/mpr/dist/manager.c $(DEPS_25)
	@echo '   [Compile] $(BUILD)/obj/manager.o'
	$(CC) -c -o $(BUILD)/obj/manager.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) paks/mpr/dist/manager.c

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
	$(CC) -c -o $(BUILD)/obj/mdb.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) src/mdb.c

#
#   mprLib.o
#
DEPS_28 += paks/mpr/dist/mpr.h

$(BUILD)/obj/mprLib.o: \
    paks/mpr/dist/mprLib.c $(DEPS_28)
	@echo '   [Compile] $(BUILD)/obj/mprLib.o'
	$(CC) -c -o $(BUILD)/obj/mprLib.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) paks/mpr/dist/mprLib.c

#
#   mprSsl.o
#
DEPS_29 += paks/mpr/dist/mpr.h

$(BUILD)/obj/mprSsl.o: \
    paks/mpr/dist/mprSsl.c $(DEPS_29)
	@echo '   [Compile] $(BUILD)/obj/mprSsl.o'
	$(CC) -c -o $(BUILD)/obj/mprSsl.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" paks/mpr/dist/mprSsl.c

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
	$(CC) -c -o $(BUILD)/obj/pcre.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) paks/pcre/dist/pcre.c

#
#   sdb.o
#
DEPS_32 += $(BUILD)/inc/http.h
DEPS_32 += src/edi.h

$(BUILD)/obj/sdb.o: \
    src/sdb.c $(DEPS_32)
	@echo '   [Compile] $(BUILD)/obj/sdb.o'
	$(CC) -c -o $(BUILD)/obj/sdb.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) src/sdb.c

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
	$(CC) -c -o $(BUILD)/obj/sqlite.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) paks/sqlite/dist/sqlite.c

#
#   sqlite3.o
#
DEPS_35 += $(BUILD)/inc/me.h
DEPS_35 += paks/sqlite/dist/sqlite3.h

$(BUILD)/obj/sqlite3.o: \
    paks/sqlite/dist/sqlite3.c $(DEPS_35)
	@echo '   [Compile] $(BUILD)/obj/sqlite3.o'
	$(CC) -c -o $(BUILD)/obj/sqlite3.o $(LDFLAGS) $(CFLAGS) $(DFLAGS) $(IFLAGS) paks/sqlite/dist/sqlite3.c

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

$(BUILD)/bin/libsql.so: $(DEPS_37)
	@echo '      [Link] $(BUILD)/bin/libsql.so'
	$(CC) -shared -o $(BUILD)/bin/libsql.so $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/sqlite3.o" $(LIBS) 
endif

#
#   libmpr
#
DEPS_38 += $(BUILD)/inc/osdep.h
DEPS_38 += $(BUILD)/inc/mpr.h
DEPS_38 += $(BUILD)/obj/mprLib.o

$(BUILD)/bin/libmpr.so: $(DEPS_38)
	@echo '      [Link] $(BUILD)/bin/libmpr.so'
	$(CC) -shared -o $(BUILD)/bin/libmpr.so $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/mprLib.o" $(LIBS) 

ifeq ($(ME_COM_PCRE),1)
#
#   libpcre
#
DEPS_39 += $(BUILD)/inc/pcre.h
DEPS_39 += $(BUILD)/obj/pcre.o

$(BUILD)/bin/libpcre.so: $(DEPS_39)
	@echo '      [Link] $(BUILD)/bin/libpcre.so'
	$(CC) -shared -o $(BUILD)/bin/libpcre.so $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/pcre.o" $(LIBS) 
endif

ifeq ($(ME_COM_HTTP),1)
#
#   libhttp
#
DEPS_40 += $(BUILD)/bin/libmpr.so
ifeq ($(ME_COM_PCRE),1)
    DEPS_40 += $(BUILD)/bin/libpcre.so
endif
DEPS_40 += $(BUILD)/inc/http.h
DEPS_40 += $(BUILD)/obj/httpLib.o

LIBS_40 += -lmpr
ifeq ($(ME_COM_PCRE),1)
    LIBS_40 += -lpcre
endif

$(BUILD)/bin/libhttp.so: $(DEPS_40)
	@echo '      [Link] $(BUILD)/bin/libhttp.so'
	$(CC) -shared -o $(BUILD)/bin/libhttp.so $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/httpLib.o" $(LIBPATHS_40) $(LIBS_40) $(LIBS_40) $(LIBS) 
endif

#
#   libesp
#
ifeq ($(ME_COM_SQLITE),1)
    DEPS_41 += $(BUILD)/bin/libsql.so
endif
ifeq ($(ME_COM_HTTP),1)
    DEPS_41 += $(BUILD)/bin/libhttp.so
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

ifeq ($(ME_COM_SQLITE),1)
    LIBS_41 += -lsql
endif
ifeq ($(ME_COM_HTTP),1)
    LIBS_41 += -lhttp
endif
LIBS_41 += -lmpr
ifeq ($(ME_COM_PCRE),1)
    LIBS_41 += -lpcre
endif

$(BUILD)/bin/libesp.so: $(DEPS_41)
	@echo '      [Link] $(BUILD)/bin/libesp.so'
	$(CC) -shared -o $(BUILD)/bin/libesp.so $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/edi.o" "$(BUILD)/obj/espAbbrev.o" "$(BUILD)/obj/espConfig.o" "$(BUILD)/obj/espFramework.o" "$(BUILD)/obj/espHtml.o" "$(BUILD)/obj/espRequest.o" "$(BUILD)/obj/espTemplate.o" "$(BUILD)/obj/mdb.o" "$(BUILD)/obj/sdb.o" $(LIBPATHS_41) $(LIBS_41) $(LIBS_41) $(LIBS) 

#
#   espcmd
#
ifeq ($(ME_COM_SQLITE),1)
    DEPS_42 += $(BUILD)/bin/libsql.so
endif
DEPS_42 += $(BUILD)/bin/libesp.so
DEPS_42 += $(BUILD)/obj/esp.o

ifeq ($(ME_COM_SQLITE),1)
    LIBS_42 += -lsql
endif
LIBS_42 += -lesp
ifeq ($(ME_COM_HTTP),1)
    LIBS_42 += -lhttp
endif
LIBS_42 += -lmpr
ifeq ($(ME_COM_PCRE),1)
    LIBS_42 += -lpcre
endif

$(BUILD)/bin/esp: $(DEPS_42)
	@echo '      [Link] $(BUILD)/bin/esp'
	$(CC) -o $(BUILD)/bin/esp $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/esp.o" $(LIBPATHS_42) $(LIBS_42) $(LIBS_42) $(LIBS) $(LIBS) 

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
DEPS_44 += $(BUILD)/bin/libhttp.so
DEPS_44 += $(BUILD)/obj/http.o

LIBS_44 += -lhttp
LIBS_44 += -lmpr
ifeq ($(ME_COM_PCRE),1)
    LIBS_44 += -lpcre
endif

$(BUILD)/bin/http: $(DEPS_44)
	@echo '      [Link] $(BUILD)/bin/http'
	$(CC) -o $(BUILD)/bin/http $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/http.o" $(LIBPATHS_44) $(LIBS_44) $(LIBS_44) $(LIBS) $(LIBS) 
endif

#
#   libmprssl
#
DEPS_45 += $(BUILD)/bin/libmpr.so
DEPS_45 += $(BUILD)/obj/mprSsl.o

LIBS_45 += -lmpr
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
ifeq ($(ME_COM_EST),1)
    LIBS_45 += -lest
endif

$(BUILD)/bin/libmprssl.so: $(DEPS_45)
	@echo '      [Link] $(BUILD)/bin/libmprssl.so'
	$(CC) -shared -o $(BUILD)/bin/libmprssl.so $(LDFLAGS) $(LIBPATHS)   "$(BUILD)/obj/mprSsl.o" $(LIBPATHS_45) $(LIBS_45) $(LIBS_45) $(LIBS) 

#
#   manager
#
DEPS_46 += $(BUILD)/bin/libmpr.so
DEPS_46 += $(BUILD)/obj/manager.o

LIBS_46 += -lmpr

$(BUILD)/bin/espman: $(DEPS_46)
	@echo '      [Link] $(BUILD)/bin/espman'
	$(CC) -o $(BUILD)/bin/espman $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/manager.o" $(LIBPATHS_46) $(LIBS_46) $(LIBS_46) $(LIBS) $(LIBS) 

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
	mkdir -p "$(ME_APP_PREFIX)" ; \
	rm -f "$(ME_APP_PREFIX)/latest" ; \
	ln -s "5.4.0" "$(ME_APP_PREFIX)/latest" ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp $(BUILD)/bin/esp $(ME_VAPP_PREFIX)/bin/esp ; \
	mkdir -p "$(ME_BIN_PREFIX)" ; \
	rm -f "$(ME_BIN_PREFIX)/esp" ; \
	ln -s "$(ME_VAPP_PREFIX)/bin/esp" "$(ME_BIN_PREFIX)/esp" ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp $(BUILD)/bin/espman $(ME_VAPP_PREFIX)/bin/espman ; \
	mkdir -p "$(ME_BIN_PREFIX)" ; \
	rm -f "$(ME_BIN_PREFIX)/espman" ; \
	ln -s "$(ME_VAPP_PREFIX)/bin/espman" "$(ME_BIN_PREFIX)/espman" ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp $(BUILD)/bin/ca.crt $(ME_VAPP_PREFIX)/bin/ca.crt ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp $(BUILD)/bin/esp-compile.json $(ME_VAPP_PREFIX)/bin/esp-compile.json ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp $(BUILD)/bin/libhttp.so $(ME_VAPP_PREFIX)/bin/libhttp.so ; \
	cp $(BUILD)/bin/libmpr.so $(ME_VAPP_PREFIX)/bin/libmpr.so ; \
	cp $(BUILD)/bin/libmprssl.so $(ME_VAPP_PREFIX)/bin/libmprssl.so ; \
	cp $(BUILD)/bin/libpcre.so $(ME_VAPP_PREFIX)/bin/libpcre.so ; \
	cp $(BUILD)/bin/libsql.so $(ME_VAPP_PREFIX)/bin/libsql.so ; \
	cp $(BUILD)/bin/libesp.so $(ME_VAPP_PREFIX)/bin/libesp.so ; \
	if [ "$(ME_COM_EST)" = 1 ]; then true ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp $(BUILD)/bin/libest.so $(ME_VAPP_PREFIX)/bin/libest.so ; \
	fi ; \
	mkdir -p "$(ME_VAPP_PREFIX)/inc" ; \
	cp $(BUILD)/inc/me.h $(ME_VAPP_PREFIX)/inc/me.h ; \
	mkdir -p "$(ME_INC_PREFIX)/esp" ; \
	rm -f "$(ME_INC_PREFIX)/esp/me.h" ; \
	ln -s "$(ME_VAPP_PREFIX)/inc/me.h" "$(ME_INC_PREFIX)/esp/me.h" ; \
	cp src/esp.h $(ME_VAPP_PREFIX)/inc/esp.h ; \
	mkdir -p "$(ME_INC_PREFIX)/esp" ; \
	rm -f "$(ME_INC_PREFIX)/esp/esp.h" ; \
	ln -s "$(ME_VAPP_PREFIX)/inc/esp.h" "$(ME_INC_PREFIX)/esp/esp.h" ; \
	cp src/edi.h $(ME_VAPP_PREFIX)/inc/edi.h ; \
	mkdir -p "$(ME_INC_PREFIX)/esp" ; \
	rm -f "$(ME_INC_PREFIX)/esp/edi.h" ; \
	ln -s "$(ME_VAPP_PREFIX)/inc/edi.h" "$(ME_INC_PREFIX)/esp/edi.h" ; \
	cp src/osdep/osdep.h $(ME_VAPP_PREFIX)/inc/osdep.h ; \
	mkdir -p "$(ME_INC_PREFIX)/esp" ; \
	rm -f "$(ME_INC_PREFIX)/esp/osdep.h" ; \
	ln -s "$(ME_VAPP_PREFIX)/inc/osdep.h" "$(ME_INC_PREFIX)/esp/osdep.h" ; \
	cp src/est/est.h $(ME_VAPP_PREFIX)/inc/est.h ; \
	mkdir -p "$(ME_INC_PREFIX)/esp" ; \
	rm -f "$(ME_INC_PREFIX)/esp/est.h" ; \
	ln -s "$(ME_VAPP_PREFIX)/inc/est.h" "$(ME_INC_PREFIX)/esp/est.h" ; \
	cp src/http/http.h $(ME_VAPP_PREFIX)/inc/http.h ; \
	mkdir -p "$(ME_INC_PREFIX)/esp" ; \
	rm -f "$(ME_INC_PREFIX)/esp/http.h" ; \
	ln -s "$(ME_VAPP_PREFIX)/inc/http.h" "$(ME_INC_PREFIX)/esp/http.h" ; \
	cp src/mpr/mpr.h $(ME_VAPP_PREFIX)/inc/mpr.h ; \
	mkdir -p "$(ME_INC_PREFIX)/esp" ; \
	rm -f "$(ME_INC_PREFIX)/esp/mpr.h" ; \
	ln -s "$(ME_VAPP_PREFIX)/inc/mpr.h" "$(ME_INC_PREFIX)/esp/mpr.h" ; \
	cp src/pcre/pcre.h $(ME_VAPP_PREFIX)/inc/pcre.h ; \
	mkdir -p "$(ME_INC_PREFIX)/esp" ; \
	rm -f "$(ME_INC_PREFIX)/esp/pcre.h" ; \
	ln -s "$(ME_VAPP_PREFIX)/inc/pcre.h" "$(ME_INC_PREFIX)/esp/pcre.h" ; \
	cp src/sqlite/sqlite3.h $(ME_VAPP_PREFIX)/inc/sqlite3.h ; \
	mkdir -p "$(ME_INC_PREFIX)/esp" ; \
	rm -f "$(ME_INC_PREFIX)/esp/sqlite3.h" ; \
	ln -s "$(ME_VAPP_PREFIX)/inc/sqlite3.h" "$(ME_INC_PREFIX)/esp/sqlite3.h" ; \
	mkdir -p "$(ME_VAPP_PREFIX)/doc/man/man1" ; \
	cp doc/contents/man/esp.1 $(ME_VAPP_PREFIX)/doc/man/man1/esp.1 ; \
	mkdir -p "$(ME_MAN_PREFIX)/man1" ; \
	rm -f "$(ME_MAN_PREFIX)/man1/esp.1" ; \
	ln -s "$(ME_VAPP_PREFIX)/doc/man/man1/esp.1" "$(ME_MAN_PREFIX)/man1/esp.1"

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
	rm -fr "$(ME_VAPP_PREFIX)" ; \
	rm -f "$(ME_APP_PREFIX)/latest" ; \
	rmdir -p "$(ME_APP_PREFIX)" 2>/dev/null ; true

#
#   version
#

version: $(DEPS_53)
	echo 5.4.0

