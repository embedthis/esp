#
#   esp-macosx-default.mk -- Makefile to build Embedthis ESP for macosx
#

NAME                  := esp
VERSION               := 5.4.1
PROFILE               ?= default
ARCH                  ?= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/;s/arm.*/arm/;s/mips.*/mips/')
CC_ARCH               ?= $(shell echo $(ARCH) | sed 's/x86/i686/;s/x64/x86_64/')
OS                    ?= macosx
CC                    ?= clang
CONFIG                ?= $(OS)-$(ARCH)-$(PROFILE)
BUILD                 ?= build/$(CONFIG)
LBIN                  ?= $(BUILD)/bin
PATH                  := $(LBIN):$(PATH)

ME_COM_COMPILER       ?= 1
ME_COM_EST            ?= 0
ME_COM_HTTP           ?= 1
ME_COM_LIB            ?= 1
ME_COM_MATRIXSSL      ?= 0
ME_COM_MBEDTLS        ?= 0
ME_COM_MDB            ?= 1
ME_COM_MPR            ?= 1
ME_COM_NANOSSL        ?= 0
ME_COM_OPENSSL        ?= 1
ME_COM_OSDEP          ?= 1
ME_COM_PCRE           ?= 1
ME_COM_SQLITE         ?= 1
ME_COM_SSL            ?= 1
ME_COM_VXWORKS        ?= 0
ME_COM_WINSDK         ?= 1

ME_COM_OPENSSL_PATH   ?= "/usr/lib"

ifeq ($(ME_COM_EST),1)
    ME_COM_SSL := 1
endif
ifeq ($(ME_COM_LIB),1)
    ME_COM_COMPILER := 1
endif
ifeq ($(ME_COM_OPENSSL),1)
    ME_COM_SSL := 1
endif

CFLAGS                += -g -w
DFLAGS                +=  $(patsubst %,-D%,$(filter ME_%,$(MAKEFLAGS))) -DME_COM_COMPILER=$(ME_COM_COMPILER) -DME_COM_EST=$(ME_COM_EST) -DME_COM_HTTP=$(ME_COM_HTTP) -DME_COM_LIB=$(ME_COM_LIB) -DME_COM_MATRIXSSL=$(ME_COM_MATRIXSSL) -DME_COM_MBEDTLS=$(ME_COM_MBEDTLS) -DME_COM_MDB=$(ME_COM_MDB) -DME_COM_MPR=$(ME_COM_MPR) -DME_COM_NANOSSL=$(ME_COM_NANOSSL) -DME_COM_OPENSSL=$(ME_COM_OPENSSL) -DME_COM_OSDEP=$(ME_COM_OSDEP) -DME_COM_PCRE=$(ME_COM_PCRE) -DME_COM_SQLITE=$(ME_COM_SQLITE) -DME_COM_SSL=$(ME_COM_SSL) -DME_COM_VXWORKS=$(ME_COM_VXWORKS) -DME_COM_WINSDK=$(ME_COM_WINSDK) 
IFLAGS                += "-I$(BUILD)/inc"
LDFLAGS               += '-Wl,-rpath,@executable_path/' '-Wl,-rpath,@loader_path/'
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
ifeq ($(ME_COM_SSL),1)
    TARGETS           += $(BUILD)/bin
endif
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
	@[ ! -f $(BUILD)/inc/me.h ] && cp projects/esp-macosx-default-me.h $(BUILD)/inc/me.h ; true
	@if ! diff $(BUILD)/inc/me.h projects/esp-macosx-default-me.h >/dev/null ; then\
		cp projects/esp-macosx-default-me.h $(BUILD)/inc/me.h  ; \
	fi; true
	@if [ -f "$(BUILD)/.makeflags" ] ; then \
		if [ "$(MAKEFLAGS)" != "`cat $(BUILD)/.makeflags`" ] ; then \
			echo "   [Warning] Make flags have changed since the last build" ; \
			echo "   [Warning] Previous build command: "`cat $(BUILD)/.makeflags`"" ; \
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
	rm -f "$(BUILD)/obj/est.o"
	rm -f "$(BUILD)/obj/estLib.o"
	rm -f "$(BUILD)/obj/http.o"
	rm -f "$(BUILD)/obj/httpLib.o"
	rm -f "$(BUILD)/obj/mdb.o"
	rm -f "$(BUILD)/obj/mprLib.o"
	rm -f "$(BUILD)/obj/openssl.o"
	rm -f "$(BUILD)/obj/pcre.o"
	rm -f "$(BUILD)/obj/removeFiles.o"
	rm -f "$(BUILD)/obj/sdb.o"
	rm -f "$(BUILD)/obj/sqlite.o"
	rm -f "$(BUILD)/obj/sqlite3.o"
	rm -f "$(BUILD)/obj/watchdog.o"
	rm -f "$(BUILD)/bin/esp-compile.json"
	rm -f "$(BUILD)/bin/esp"
	rm -f "$(BUILD)/bin"
	rm -f "$(BUILD)/bin/libesp.dylib"
	rm -f "$(BUILD)/bin/libhttp.dylib"
	rm -f "$(BUILD)/bin/libmpr.dylib"
	rm -f "$(BUILD)/bin/libpcre.dylib"
	rm -f "$(BUILD)/bin/libsql.dylib"
	rm -f "$(BUILD)/bin/libmpr-openssl.a"
	rm -f "$(BUILD)/bin/espman"

clobber: clean
	rm -fr ./$(BUILD)

#
#   init
#

init: $(DEPS_1)
	if [ ! -d /usr/include/openssl ] ; then echo ; \
	echo Install libssl-dev to get /usr/include/openssl ; \
	exit 255 ; \
	fi

#
#   edi.h
#
DEPS_2 += src/edi.h

$(BUILD)/inc/edi.h: $(DEPS_2)
	@echo '      [Copy] $(BUILD)/inc/edi.h'
	mkdir -p "$(BUILD)/inc"
	cp src/edi.h $(BUILD)/inc/edi.h

#
#   esp.h
#
DEPS_3 += src/esp.h

$(BUILD)/inc/esp.h: $(DEPS_3)
	@echo '      [Copy] $(BUILD)/inc/esp.h'
	mkdir -p "$(BUILD)/inc"
	cp src/esp.h $(BUILD)/inc/esp.h

#
#   est.h
#
DEPS_4 += src/est/est.h

$(BUILD)/inc/est.h: $(DEPS_4)
	@echo '      [Copy] $(BUILD)/inc/est.h'
	mkdir -p "$(BUILD)/inc"
	cp src/est/est.h $(BUILD)/inc/est.h

#
#   me.h
#

$(BUILD)/inc/me.h: $(DEPS_5)

#
#   osdep.h
#
DEPS_6 += src/osdep/osdep.h
DEPS_6 += $(BUILD)/inc/me.h

$(BUILD)/inc/osdep.h: $(DEPS_6)
	@echo '      [Copy] $(BUILD)/inc/osdep.h'
	mkdir -p "$(BUILD)/inc"
	cp src/osdep/osdep.h $(BUILD)/inc/osdep.h

#
#   mpr.h
#
DEPS_7 += src/mpr/mpr.h
DEPS_7 += $(BUILD)/inc/me.h
DEPS_7 += $(BUILD)/inc/osdep.h

$(BUILD)/inc/mpr.h: $(DEPS_7)
	@echo '      [Copy] $(BUILD)/inc/mpr.h'
	mkdir -p "$(BUILD)/inc"
	cp src/mpr/mpr.h $(BUILD)/inc/mpr.h

#
#   http.h
#
DEPS_8 += src/http/http.h
DEPS_8 += $(BUILD)/inc/mpr.h

$(BUILD)/inc/http.h: $(DEPS_8)
	@echo '      [Copy] $(BUILD)/inc/http.h'
	mkdir -p "$(BUILD)/inc"
	cp src/http/http.h $(BUILD)/inc/http.h

#
#   mdb.h
#
DEPS_9 += src/mdb.h

$(BUILD)/inc/mdb.h: $(DEPS_9)
	@echo '      [Copy] $(BUILD)/inc/mdb.h'
	mkdir -p "$(BUILD)/inc"
	cp src/mdb.h $(BUILD)/inc/mdb.h

#
#   pcre.h
#
DEPS_10 += src/pcre/pcre.h

$(BUILD)/inc/pcre.h: $(DEPS_10)
	@echo '      [Copy] $(BUILD)/inc/pcre.h'
	mkdir -p "$(BUILD)/inc"
	cp src/pcre/pcre.h $(BUILD)/inc/pcre.h

#
#   sqlite3.h
#
DEPS_11 += src/sqlite/sqlite3.h

$(BUILD)/inc/sqlite3.h: $(DEPS_11)
	@echo '      [Copy] $(BUILD)/inc/sqlite3.h'
	mkdir -p "$(BUILD)/inc"
	cp src/sqlite/sqlite3.h $(BUILD)/inc/sqlite3.h

#
#   edi.h
#

src/edi.h: $(DEPS_12)

#
#   edi.o
#
DEPS_13 += src/edi.h
DEPS_13 += $(BUILD)/inc/pcre.h

$(BUILD)/obj/edi.o: \
    src/edi.c $(DEPS_13)
	@echo '   [Compile] $(BUILD)/obj/edi.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/edi.o -arch $(CC_ARCH) $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/edi.c

#
#   esp.h
#

src/esp.h: $(DEPS_14)

#
#   esp.o
#
DEPS_15 += src/esp.h

$(BUILD)/obj/esp.o: \
    src/esp.c $(DEPS_15)
	@echo '   [Compile] $(BUILD)/obj/esp.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/esp.o -arch $(CC_ARCH) $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/esp.c

#
#   espAbbrev.o
#
DEPS_16 += src/esp.h

$(BUILD)/obj/espAbbrev.o: \
    src/espAbbrev.c $(DEPS_16)
	@echo '   [Compile] $(BUILD)/obj/espAbbrev.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/espAbbrev.o -arch $(CC_ARCH) $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/espAbbrev.c

#
#   espConfig.o
#
DEPS_17 += src/esp.h

$(BUILD)/obj/espConfig.o: \
    src/espConfig.c $(DEPS_17)
	@echo '   [Compile] $(BUILD)/obj/espConfig.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/espConfig.o -arch $(CC_ARCH) $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/espConfig.c

#
#   espFramework.o
#
DEPS_18 += src/esp.h

$(BUILD)/obj/espFramework.o: \
    src/espFramework.c $(DEPS_18)
	@echo '   [Compile] $(BUILD)/obj/espFramework.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/espFramework.o -arch $(CC_ARCH) $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/espFramework.c

#
#   espHtml.o
#
DEPS_19 += src/esp.h
DEPS_19 += src/edi.h

$(BUILD)/obj/espHtml.o: \
    src/espHtml.c $(DEPS_19)
	@echo '   [Compile] $(BUILD)/obj/espHtml.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/espHtml.o -arch $(CC_ARCH) $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/espHtml.c

#
#   espRequest.o
#
DEPS_20 += src/esp.h

$(BUILD)/obj/espRequest.o: \
    src/espRequest.c $(DEPS_20)
	@echo '   [Compile] $(BUILD)/obj/espRequest.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/espRequest.o -arch $(CC_ARCH) $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/espRequest.c

#
#   espTemplate.o
#
DEPS_21 += src/esp.h

$(BUILD)/obj/espTemplate.o: \
    src/espTemplate.c $(DEPS_21)
	@echo '   [Compile] $(BUILD)/obj/espTemplate.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/espTemplate.o -arch $(CC_ARCH) $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/espTemplate.c

#
#   est.o
#
DEPS_22 += $(BUILD)/inc/mpr.h

$(BUILD)/obj/est.o: \
    src/mpr-est/est.c $(DEPS_22)
	@echo '   [Compile] $(BUILD)/obj/est.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/est.o -arch $(CC_ARCH) $(CFLAGS) $(IFLAGS) src/mpr-est/est.c

#
#   est.h
#

src/est/est.h: $(DEPS_23)

#
#   estLib.o
#
DEPS_24 += src/est/est.h

$(BUILD)/obj/estLib.o: \
    src/est/estLib.c $(DEPS_24)
	@echo '   [Compile] $(BUILD)/obj/estLib.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/estLib.o -arch $(CC_ARCH) $(CFLAGS) $(IFLAGS) src/est/estLib.c

#
#   http.h
#

src/http/http.h: $(DEPS_25)

#
#   http.o
#
DEPS_26 += src/http/http.h

$(BUILD)/obj/http.o: \
    src/http/http.c $(DEPS_26)
	@echo '   [Compile] $(BUILD)/obj/http.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/http.o -arch $(CC_ARCH) $(CFLAGS) $(IFLAGS) src/http/http.c

#
#   httpLib.o
#
DEPS_27 += src/http/http.h
DEPS_27 += $(BUILD)/inc/pcre.h

$(BUILD)/obj/httpLib.o: \
    src/http/httpLib.c $(DEPS_27)
	@echo '   [Compile] $(BUILD)/obj/httpLib.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/httpLib.o -arch $(CC_ARCH) $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/http/httpLib.c

#
#   mdb.h
#

src/mdb.h: $(DEPS_28)

#
#   mdb.o
#
DEPS_29 += $(BUILD)/inc/http.h
DEPS_29 += src/edi.h
DEPS_29 += src/mdb.h
DEPS_29 += $(BUILD)/inc/pcre.h

$(BUILD)/obj/mdb.o: \
    src/mdb.c $(DEPS_29)
	@echo '   [Compile] $(BUILD)/obj/mdb.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/mdb.o -arch $(CC_ARCH) $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/mdb.c

#
#   mpr.h
#

src/mpr/mpr.h: $(DEPS_30)

#
#   mprLib.o
#
DEPS_31 += src/mpr/mpr.h

$(BUILD)/obj/mprLib.o: \
    src/mpr/mprLib.c $(DEPS_31)
	@echo '   [Compile] $(BUILD)/obj/mprLib.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/mprLib.o -arch $(CC_ARCH) $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/mpr/mprLib.c

#
#   openssl.o
#
DEPS_32 += $(BUILD)/inc/mpr.h

$(BUILD)/obj/openssl.o: \
    src/mpr-openssl/openssl.c $(DEPS_32)
	@echo '   [Compile] $(BUILD)/obj/openssl.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/openssl.o -arch $(CC_ARCH) -Wno-deprecated-declarations -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/mpr-openssl/openssl.c

#
#   pcre.h
#

src/pcre/pcre.h: $(DEPS_33)

#
#   pcre.o
#
DEPS_34 += $(BUILD)/inc/me.h
DEPS_34 += src/pcre/pcre.h

$(BUILD)/obj/pcre.o: \
    src/pcre/pcre.c $(DEPS_34)
	@echo '   [Compile] $(BUILD)/obj/pcre.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/pcre.o -arch $(CC_ARCH) $(CFLAGS) $(IFLAGS) src/pcre/pcre.c

#
#   removeFiles.o
#

$(BUILD)/obj/removeFiles.o: \
    package/windows/removeFiles.c $(DEPS_35)
	@echo '   [Compile] $(BUILD)/obj/removeFiles.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/removeFiles.o -arch $(CC_ARCH) $(CFLAGS) $(IFLAGS) package/windows/removeFiles.c

#
#   sdb.o
#
DEPS_36 += $(BUILD)/inc/http.h
DEPS_36 += src/edi.h

$(BUILD)/obj/sdb.o: \
    src/sdb.c $(DEPS_36)
	@echo '   [Compile] $(BUILD)/obj/sdb.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/sdb.o -arch $(CC_ARCH) $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/sdb.c

#
#   sqlite3.h
#

src/sqlite/sqlite3.h: $(DEPS_37)

#
#   sqlite.o
#
DEPS_38 += $(BUILD)/inc/me.h
DEPS_38 += src/sqlite/sqlite3.h

$(BUILD)/obj/sqlite.o: \
    src/sqlite/sqlite.c $(DEPS_38)
	@echo '   [Compile] $(BUILD)/obj/sqlite.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/sqlite.o -arch $(CC_ARCH) $(CFLAGS) $(IFLAGS) src/sqlite/sqlite.c

#
#   sqlite3.o
#
DEPS_39 += $(BUILD)/inc/me.h
DEPS_39 += src/sqlite/sqlite3.h

$(BUILD)/obj/sqlite3.o: \
    src/sqlite/sqlite3.c $(DEPS_39)
	@echo '   [Compile] $(BUILD)/obj/sqlite3.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/sqlite3.o -arch $(CC_ARCH) $(CFLAGS) $(IFLAGS) src/sqlite/sqlite3.c

#
#   watchdog.o
#
DEPS_40 += $(BUILD)/inc/mpr.h

$(BUILD)/obj/watchdog.o: \
    src/watchdog/watchdog.c $(DEPS_40)
	@echo '   [Compile] $(BUILD)/obj/watchdog.o'
	$(CC) -c $(DFLAGS) -o $(BUILD)/obj/watchdog.o -arch $(CC_ARCH) $(CFLAGS) -DME_COM_OPENSSL_PATH="$(ME_COM_OPENSSL_PATH)" $(IFLAGS) "-I$(ME_COM_OPENSSL_PATH)/include" src/watchdog/watchdog.c

#
#   esp-compile.json
#
DEPS_41 += src/esp-compile.json

$(BUILD)/bin/esp-compile.json: $(DEPS_41)
	@echo '      [Copy] $(BUILD)/bin/esp-compile.json'
	mkdir -p "$(BUILD)/bin"
	cp src/esp-compile.json $(BUILD)/bin/esp-compile.json

ifeq ($(ME_COM_SQLITE),1)
#
#   libsql
#
DEPS_42 += $(BUILD)/inc/sqlite3.h
DEPS_42 += $(BUILD)/obj/sqlite3.o

$(BUILD)/bin/libsql.dylib: $(DEPS_42)
	@echo '      [Link] $(BUILD)/bin/libsql.dylib'
	$(CC) -dynamiclib -o $(BUILD)/bin/libsql.dylib -arch $(CC_ARCH) $(LDFLAGS) $(LIBPATHS) -install_name @rpath/libsql.dylib -compatibility_version 5.4 -current_version 5.4 "$(BUILD)/obj/sqlite3.o" $(LIBS) 
endif

ifeq ($(ME_COM_SSL),1)
#
#   openssl
#
DEPS_43 += $(BUILD)/obj/openssl.o

$(BUILD)/bin/libmpr-openssl.a: $(DEPS_43)
	@echo '      [Link] $(BUILD)/bin/libmpr-openssl.a'
	ar -cr $(BUILD)/bin/libmpr-openssl.a "$(BUILD)/obj/openssl.o"
endif

ifeq ($(ME_COM_EST),1)
#
#   libest
#
DEPS_44 += $(BUILD)/inc/osdep.h
DEPS_44 += $(BUILD)/inc/est.h
DEPS_44 += $(BUILD)/obj/estLib.o

$(BUILD)/bin/libest.a: $(DEPS_44)
	@echo '      [Link] $(BUILD)/bin/libest.a'
	ar -cr $(BUILD)/bin/libest.a "$(BUILD)/obj/estLib.o"
endif

ifeq ($(ME_COM_SSL),1)
#
#   est
#
ifeq ($(ME_COM_EST),1)
    DEPS_45 += $(BUILD)/bin/libest.a
endif
DEPS_45 += $(BUILD)/obj/est.o

$(BUILD)/bin/libmpr-estssl.a: $(DEPS_45)
	@echo '      [Link] $(BUILD)/bin/libmpr-estssl.a'
	ar -cr $(BUILD)/bin/libmpr-estssl.a "$(BUILD)/obj/est.o"
endif

#
#   libmpr
#
DEPS_46 += $(BUILD)/inc/osdep.h
ifeq ($(ME_COM_SSL),1)
ifeq ($(ME_COM_OPENSSL),1)
    DEPS_46 += $(BUILD)/bin/libmpr-openssl.a
endif
endif
ifeq ($(ME_COM_SSL),1)
ifeq ($(ME_COM_EST),1)
    DEPS_46 += $(BUILD)/bin/libmpr-estssl.a
endif
endif
DEPS_46 += $(BUILD)/inc/mpr.h
DEPS_46 += $(BUILD)/obj/mprLib.o

ifeq ($(ME_COM_OPENSSL),1)
    LIBS_46 += -lmpr-openssl
    LIBPATHS_46 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_46 += -lssl
    LIBPATHS_46 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_46 += -lcrypto
    LIBPATHS_46 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_EST),1)
    LIBS_46 += -lest
endif
ifeq ($(ME_COM_EST),1)
    LIBS_46 += -lmpr-estssl
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_46 += -lmpr-openssl
    LIBPATHS_46 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/libmpr.dylib: $(DEPS_46)
	@echo '      [Link] $(BUILD)/bin/libmpr.dylib'
	$(CC) -dynamiclib -o $(BUILD)/bin/libmpr.dylib -arch $(CC_ARCH) $(LDFLAGS) $(LIBPATHS)  -install_name @rpath/libmpr.dylib -compatibility_version 5.4 -current_version 5.4 "$(BUILD)/obj/mprLib.o" $(LIBPATHS_46) $(LIBS_46) $(LIBS_46) $(LIBS) 

ifeq ($(ME_COM_PCRE),1)
#
#   libpcre
#
DEPS_47 += $(BUILD)/inc/pcre.h
DEPS_47 += $(BUILD)/obj/pcre.o

$(BUILD)/bin/libpcre.dylib: $(DEPS_47)
	@echo '      [Link] $(BUILD)/bin/libpcre.dylib'
	$(CC) -dynamiclib -o $(BUILD)/bin/libpcre.dylib -arch $(CC_ARCH) $(LDFLAGS) -compatibility_version 5.4 -current_version 5.4 $(LIBPATHS) -install_name @rpath/libpcre.dylib -compatibility_version 5.4 -current_version 5.4 "$(BUILD)/obj/pcre.o" $(LIBS) 
endif

ifeq ($(ME_COM_HTTP),1)
#
#   libhttp
#
DEPS_48 += $(BUILD)/bin/libmpr.dylib
ifeq ($(ME_COM_PCRE),1)
    DEPS_48 += $(BUILD)/bin/libpcre.dylib
endif
DEPS_48 += $(BUILD)/inc/http.h
DEPS_48 += $(BUILD)/obj/httpLib.o

ifeq ($(ME_COM_OPENSSL),1)
    LIBS_48 += -lmpr-openssl
    LIBPATHS_48 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_48 += -lssl
    LIBPATHS_48 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_48 += -lcrypto
    LIBPATHS_48 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_EST),1)
    LIBS_48 += -lest
endif
ifeq ($(ME_COM_EST),1)
    LIBS_48 += -lmpr-estssl
endif
LIBS_48 += -lmpr
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_48 += -lmpr-openssl
    LIBPATHS_48 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_PCRE),1)
    LIBS_48 += -lpcre
endif
ifeq ($(ME_COM_PCRE),1)
    LIBS_48 += -lpcre
endif
LIBS_48 += -lmpr

$(BUILD)/bin/libhttp.dylib: $(DEPS_48)
	@echo '      [Link] $(BUILD)/bin/libhttp.dylib'
	$(CC) -dynamiclib -o $(BUILD)/bin/libhttp.dylib -arch $(CC_ARCH) $(LDFLAGS) $(LIBPATHS)  -install_name @rpath/libhttp.dylib -compatibility_version 5.4 -current_version 5.4 "$(BUILD)/obj/httpLib.o" $(LIBPATHS_48) $(LIBS_48) $(LIBS_48) $(LIBS) -lpam 
endif

#
#   libesp
#
ifeq ($(ME_COM_SQLITE),1)
    DEPS_49 += $(BUILD)/bin/libsql.dylib
endif
ifeq ($(ME_COM_HTTP),1)
    DEPS_49 += $(BUILD)/bin/libhttp.dylib
endif
DEPS_49 += $(BUILD)/inc/edi.h
DEPS_49 += $(BUILD)/inc/esp.h
DEPS_49 += $(BUILD)/inc/mdb.h
DEPS_49 += $(BUILD)/obj/edi.o
DEPS_49 += $(BUILD)/obj/espAbbrev.o
DEPS_49 += $(BUILD)/obj/espConfig.o
DEPS_49 += $(BUILD)/obj/espFramework.o
DEPS_49 += $(BUILD)/obj/espHtml.o
DEPS_49 += $(BUILD)/obj/espRequest.o
DEPS_49 += $(BUILD)/obj/espTemplate.o
DEPS_49 += $(BUILD)/obj/mdb.o
DEPS_49 += $(BUILD)/obj/sdb.o

ifeq ($(ME_COM_SQLITE),1)
    LIBS_49 += -lsql
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_49 += -lmpr-openssl
    LIBPATHS_49 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_49 += -lssl
    LIBPATHS_49 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_49 += -lcrypto
    LIBPATHS_49 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_EST),1)
    LIBS_49 += -lest
endif
ifeq ($(ME_COM_EST),1)
    LIBS_49 += -lmpr-estssl
endif
LIBS_49 += -lmpr
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_49 += -lmpr-openssl
    LIBPATHS_49 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_PCRE),1)
    LIBS_49 += -lpcre
endif
ifeq ($(ME_COM_HTTP),1)
    LIBS_49 += -lhttp
endif
ifeq ($(ME_COM_PCRE),1)
    LIBS_49 += -lpcre
endif
LIBS_49 += -lmpr
ifeq ($(ME_COM_HTTP),1)
    LIBS_49 += -lhttp
endif
ifeq ($(ME_COM_SQLITE),1)
    LIBS_49 += -lsql
endif

$(BUILD)/bin/libesp.dylib: $(DEPS_49)
	@echo '      [Link] $(BUILD)/bin/libesp.dylib'
	$(CC) -dynamiclib -o $(BUILD)/bin/libesp.dylib -arch $(CC_ARCH) $(LDFLAGS) $(LIBPATHS)  -install_name @rpath/libesp.dylib -compatibility_version 5.4 -current_version 5.4 "$(BUILD)/obj/edi.o" "$(BUILD)/obj/espAbbrev.o" "$(BUILD)/obj/espConfig.o" "$(BUILD)/obj/espFramework.o" "$(BUILD)/obj/espHtml.o" "$(BUILD)/obj/espRequest.o" "$(BUILD)/obj/espTemplate.o" "$(BUILD)/obj/mdb.o" "$(BUILD)/obj/sdb.o" $(LIBPATHS_49) $(LIBS_49) $(LIBS_49) $(LIBS) -lpam 

#
#   espcmd
#
ifeq ($(ME_COM_SQLITE),1)
    DEPS_50 += $(BUILD)/bin/libsql.dylib
endif
DEPS_50 += $(BUILD)/bin/libesp.dylib
DEPS_50 += $(BUILD)/obj/esp.o

ifeq ($(ME_COM_SQLITE),1)
    LIBS_50 += -lsql
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_50 += -lmpr-openssl
    LIBPATHS_50 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_50 += -lssl
    LIBPATHS_50 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_50 += -lcrypto
    LIBPATHS_50 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_EST),1)
    LIBS_50 += -lest
endif
ifeq ($(ME_COM_EST),1)
    LIBS_50 += -lmpr-estssl
endif
LIBS_50 += -lmpr
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_50 += -lmpr-openssl
    LIBPATHS_50 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_PCRE),1)
    LIBS_50 += -lpcre
endif
ifeq ($(ME_COM_HTTP),1)
    LIBS_50 += -lhttp
endif
ifeq ($(ME_COM_PCRE),1)
    LIBS_50 += -lpcre
endif
LIBS_50 += -lmpr
LIBS_50 += -lesp
ifeq ($(ME_COM_HTTP),1)
    LIBS_50 += -lhttp
endif
ifeq ($(ME_COM_SQLITE),1)
    LIBS_50 += -lsql
endif

$(BUILD)/bin/esp: $(DEPS_50)
	@echo '      [Link] $(BUILD)/bin/esp'
	$(CC) -o $(BUILD)/bin/esp -arch $(CC_ARCH) $(LDFLAGS) $(LIBPATHS)  "$(BUILD)/obj/esp.o" $(LIBPATHS_50) $(LIBS_50) $(LIBS_50) $(LIBS) -lpam 

ifeq ($(ME_COM_HTTP),1)
#
#   httpcmd
#
DEPS_51 += $(BUILD)/bin/libhttp.dylib
DEPS_51 += $(BUILD)/obj/http.o

ifeq ($(ME_COM_OPENSSL),1)
    LIBS_51 += -lmpr-openssl
    LIBPATHS_51 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_51 += -lssl
    LIBPATHS_51 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_51 += -lcrypto
    LIBPATHS_51 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_EST),1)
    LIBS_51 += -lest
endif
ifeq ($(ME_COM_EST),1)
    LIBS_51 += -lmpr-estssl
endif
LIBS_51 += -lmpr
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_51 += -lmpr-openssl
    LIBPATHS_51 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_PCRE),1)
    LIBS_51 += -lpcre
endif
LIBS_51 += -lhttp
ifeq ($(ME_COM_PCRE),1)
    LIBS_51 += -lpcre
endif
LIBS_51 += -lmpr

$(BUILD)/bin/http: $(DEPS_51)
	@echo '      [Link] $(BUILD)/bin/http'
	$(CC) -o $(BUILD)/bin/http -arch $(CC_ARCH) $(LDFLAGS) $(LIBPATHS) "$(BUILD)/obj/http.o" $(LIBPATHS_51) $(LIBS_51) $(LIBS_51) $(LIBS) 
endif

#
#   import-certs
#

import-certs: $(DEPS_52)

ifeq ($(ME_COM_SSL),1)
#
#   install-certs
#
DEPS_53 += src/certs/samples/ca.crt
DEPS_53 += src/certs/samples/ca.key
DEPS_53 += src/certs/samples/dh.pem
DEPS_53 += src/certs/samples/ec.crt
DEPS_53 += src/certs/samples/ec.key
DEPS_53 += src/certs/samples/roots.crt
DEPS_53 += src/certs/samples/self.crt
DEPS_53 += src/certs/samples/self.key
DEPS_53 += src/certs/samples/test.crt
DEPS_53 += src/certs/samples/test.key

$(BUILD)/bin: $(DEPS_53)
	@echo '      [Copy] $(BUILD)/bin'
	mkdir -p "$(BUILD)/bin"
	cp src/certs/samples/ca.crt $(BUILD)/bin/ca.crt
	cp src/certs/samples/ca.key $(BUILD)/bin/ca.key
	cp src/certs/samples/dh.pem $(BUILD)/bin/dh.pem
	cp src/certs/samples/ec.crt $(BUILD)/bin/ec.crt
	cp src/certs/samples/ec.key $(BUILD)/bin/ec.key
	cp src/certs/samples/roots.crt $(BUILD)/bin/roots.crt
	cp src/certs/samples/self.crt $(BUILD)/bin/self.crt
	cp src/certs/samples/self.key $(BUILD)/bin/self.key
	cp src/certs/samples/test.crt $(BUILD)/bin/test.crt
	cp src/certs/samples/test.key $(BUILD)/bin/test.key
endif

#
#   watchdog
#
DEPS_54 += $(BUILD)/bin/libmpr.dylib
DEPS_54 += $(BUILD)/obj/watchdog.o

ifeq ($(ME_COM_OPENSSL),1)
    LIBS_54 += -lmpr-openssl
    LIBPATHS_54 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_OPENSSL),1)
ifeq ($(ME_COM_SSL),1)
    LIBS_54 += -lssl
    LIBPATHS_54 += -L"$(ME_COM_OPENSSL_PATH)"
endif
endif
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_54 += -lcrypto
    LIBPATHS_54 += -L"$(ME_COM_OPENSSL_PATH)"
endif
ifeq ($(ME_COM_EST),1)
    LIBS_54 += -lest
endif
ifeq ($(ME_COM_EST),1)
    LIBS_54 += -lmpr-estssl
endif
LIBS_54 += -lmpr
ifeq ($(ME_COM_OPENSSL),1)
    LIBS_54 += -lmpr-openssl
    LIBPATHS_54 += -L"$(ME_COM_OPENSSL_PATH)"
endif

$(BUILD)/bin/espman: $(DEPS_54)
	@echo '      [Link] $(BUILD)/bin/espman'
	$(CC) -o $(BUILD)/bin/espman -arch $(CC_ARCH) $(LDFLAGS) $(LIBPATHS)  "$(BUILD)/obj/watchdog.o" $(LIBPATHS_54) $(LIBS_54) $(LIBS_54) $(LIBS) 

#
#   installPrep
#

installPrep: $(DEPS_55)
	if [ "`id -u`" != 0 ] ; \
	then echo "Must run as root. Rerun with "sudo"" ; \
	exit 255 ; \
	fi

#
#   stop
#

stop: $(DEPS_56)

#
#   installBinary
#

installBinary: $(DEPS_57)
	mkdir -p "$(ME_APP_PREFIX)" ; \
	rm -f "$(ME_APP_PREFIX)/latest" ; \
	ln -s "$(VERSION)" "$(ME_APP_PREFIX)/latest" ; \
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
	if [ "$(ME_COM_SSL)" = 1 ]; then true ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp $(BUILD)/bin/roots.crt $(ME_VAPP_PREFIX)/bin/roots.crt ; \
	fi ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp $(BUILD)/bin/esp-compile.json $(ME_VAPP_PREFIX)/bin/esp-compile.json ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp $(BUILD)/bin/libhttp.dylib $(ME_VAPP_PREFIX)/bin/libhttp.dylib ; \
	cp $(BUILD)/bin/libmpr.dylib $(ME_VAPP_PREFIX)/bin/libmpr.dylib ; \
	cp $(BUILD)/bin/libpcre.dylib $(ME_VAPP_PREFIX)/bin/libpcre.dylib ; \
	cp $(BUILD)/bin/libsql.dylib $(ME_VAPP_PREFIX)/bin/libsql.dylib ; \
	cp $(BUILD)/bin/libesp.dylib $(ME_VAPP_PREFIX)/bin/libesp.dylib ; \
	if [ "$(ME_COM_EST)" = 1 ]; then true ; \
	mkdir -p "$(ME_VAPP_PREFIX)/bin" ; \
	cp $(BUILD)/bin/libest.dylib $(ME_VAPP_PREFIX)/bin/libest.dylib ; \
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

start: $(DEPS_58)

#
#   install
#
DEPS_59 += installPrep
DEPS_59 += stop
DEPS_59 += installBinary
DEPS_59 += start

install: $(DEPS_59)

#
#   uninstall
#
DEPS_60 += stop

uninstall: $(DEPS_60)
	rm -fr "$(ME_VAPP_PREFIX)" ; \
	rm -f "$(ME_APP_PREFIX)/latest" ; \
	rmdir -p "$(ME_APP_PREFIX)" 2>/dev/null ; true

#
#   version
#

version: $(DEPS_61)
	echo $(VERSION)

