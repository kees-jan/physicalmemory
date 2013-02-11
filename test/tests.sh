#!/bin/sh -xe

# OK values
START=0xC0000000
SIZE=0x10000000

#Derived
SHOULD_FIT=$SIZE
SHOULD_FIT_GENEROUSLY=$(($SIZE/2))
SHOULD_NOT_FIT=$(($SIZE + 1))

# Constants
PAGESIZE=4096

# Derived values
END=$(($START+$SIZE))
PAGEFRACTION=$(($PAGESIZE/4))

try_load_module()
{
    insmod ../module/physicalmemory.ko "$*" && rmmod ../module/physicalmemory.ko
}

cleanup()
{
    ( cd ../module && ./physicalmemory_unload )
}

trap 'cleanup' 0

echo Correct behaviour
try_load_module start=$START size=$SIZE
try_load_module start=$START end=$END

echo Completely specfied
try_load_module start=$START size=$SIZE end=$END

echo Not page aligned
! try_load_module start=$(($START+$PAGEFRACTION)) size=$SIZE
! try_load_module start=$(($START+$PAGEFRACTION)) end=$END
! try_load_module start=$START size=$(($SIZE-$PAGEFRACTION))
! try_load_module start=$START end=$(($END-$PAGEFRACTION))

echo Overspecified
! try_load_module start=$START size=$SIZE end=$(($END-$PAGESIZE))

echo Underspecified
! try_load_module size=$SIZE
! try_load_module end=$END
! try_load_module size=$SIZE end=$END
! try_load_module start=$START


echo Runtime tests
(cd ../module && ./physicalmemory_load start=$START size=$SIZE)
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-flush
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-allocate $SHOULD_FIT
! LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-allocate $SHOULD_NOT_FIT
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-free $SHOULD_FIT
! LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-free $SHOULD_NOT_FIT
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-map $SHOULD_FIT
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-map-incorrectly $SHOULD_FIT_GENEROUSLY

echo Succes!