set pagination off
set architecture i386
set disassembly-flavor intel
set remotetimeout 10
define xbox-clones
  p *(int*)&'?cloneCount@Scratch@@2HA'
  p *(int*)&'?maxClones@Scratch@@2HA'
end
target remote 127.0.0.1:1234
# Use a hardware breakpoint: the BIOS has not loaded the XBE's code yet.
hbreak main
continue
