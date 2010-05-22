fwpatch.c is the Firmware Patching application, which applies firmware
patches according to a ROM Patch Distribution File (RPDF).  A sanity
test for fwpatch, including sample RPDF files, can be found in
host/tests/fwpatch.

rompatcher.c is a lower-level utility -- useful for testing and perhaps
for special cases -- which provides more direct access to ROM patching
capabilities.  It does not deal with RPDF files.

rpdf.txt describes the format of a ROM Patch Distribution File.
