#!/bin/sh -xe

# OK values
START=0xC0000000
SIZE=0x10000000

# Constants
PAGESIZE=4096

#Derived
SHOULD_FIT=$SIZE
SHOULD_FIT_GENEROUSLY=$(($SIZE/2))
SHOULD_NOT_FIT=$(($SIZE + $PAGESIZE))

# Derived values
END=$(($START+$SIZE))
PAGEFRACTION=$(($PAGESIZE/4))

try_load_module()
{
    insmod ../module/physicalmemory.ko "$*" && rmmod ../module/physicalmemory.ko
}

cleanup()
{
    wait
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
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-allocate $SHOULD_FIT
! LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-allocate $SHOULD_NOT_FIT
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-free $SHOULD_FIT
! LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-free $SHOULD_NOT_FIT
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-map $SHOULD_FIT
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-map-incorrectly $SHOULD_FIT_GENEROUSLY
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-free-mapped-region $SHOULD_FIT
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-alignment
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-lib $SHOULD_FIT_GENEROUSLY
! LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-lib $SHOULD_NOT_FIT
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-lib-alignment

echo Concurrency tests
sleep 2 &
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-fork $SHOULD_FIT_GENEROUSLY   # This one doesn't wait for its child
LD_LIBRARY_PATH=../lib/_lib/:  ../test/_bin/test-fork2 $SHOULD_FIT_GENEROUSLY
wait

echo Succes!
