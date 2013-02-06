# Remove the comment to create a DSO out of the files *not* containing
# a main() function - common1.c and common2.c
SONAME=libphysicalmem.so.0

CXXFLAGS += -I ../module
# LDFLAGS += $(shell pkg-config --libs $(PACKAGES))

