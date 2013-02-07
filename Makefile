SUBDIRS=module lib test

include master-dir.mk

test: module lib
