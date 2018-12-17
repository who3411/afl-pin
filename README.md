Intro
=====
This is the fastest pintool afl-fuzzer out there currently.
And it runs with pintool 3.6, so 4.x x64 kernels are fine.
But ... pintool is super slow.
So this is basically only if you have no other option.
I am currently developing an alternative with DynamoRIO and is 10x faster -
but still, afl qemu mode is 5-10x faster than that ...


Installation
============
1. download, compile and install afl-pin-slowfuzz => https://github.com/who3411/afl/tree/afl-pin-slowfuzz
2. change afl-fuzz.c `shmget` function, `MAPSIZE` → `MAPSIZE + 4`
3. download and unpack pin => https://software.intel.com/en-us/articles/pintool-downloads (download 3.6 or later)
4. execute command

```
gcc -O0 -o testcode/test1 testcode/test1.c
ln -s /path/to/afl afl
PIN_ROOT=/path/to/pin make
sudo make install
sudo bash -c 'PIN_ROOT=/path/to/pin ./afl-fuzz-pin.sh -forkserver -i in_dir -o out_dir -- ./testcode/test1'
```

Options
=======

```
-libs               also instrument the dynamic libraries
-exitpoint target   exit the program when this address/function is reached. speeeed!
-forkserver         install a forkserver. You must set PIN_APP_LD_PRELOAD - or use afl-fuzz-pin.sh
-entrypoint target  function or address where you want to install the forkserver
-alternative        a little bit faster but less quality
```

How to run
==========



When to use it
==============
When you have no source code, normal afl-dyninst is crashing the binary,
qemu mode -Q is not an option and dynamorio is not working either.
Pin is even 90% slower than my dynamorio implementation ...


Limitations
===========
Pin is super slow ... it is the tool of last resort on x86/x64.


Who and where
=============

who3411

https://github.com/who3411/afl-pin forked from https://github.com/vanhauser-thc/afl-pin
