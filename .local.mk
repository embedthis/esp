#
#	Local Makefile commands
#
NAME			:= esp
VERSION			:= $(shell pak edit version)

.PHONY:	configure

download:
	@echo Download link: https://s3.amazonaws.com/embedthis.public/$(NAME)-$(VERSION).tgz

configure:
	./configure --release

build:
	make boot

package:
	me package

prep:
	rm -fr build

#
#	Release a new version
#
publish:
	me doc projects 
	me package
	aws s3 --quiet cp build/*/img/$(NAME)-src.tgz s3://embedthis.public/$(NAME)-src.tgz
	aws s3 --quiet cp build/*/img/$(NAME)-src.tgz s3://embedthis.public/$(NAME)-$(VERSION).tgz
	aws s3 --quiet cp build/macosx-x64-release/img/$(NAME)-$(VERSION)-apple-macosx-x64.pkg s3://embedthis.software/$(NAME)-$(VERSION)-apple-macosx-x64.pkg

promote: prep configure build package publish download
