AR6002 ROM Firmware Patch Sanity Test

There are shell scripts (testX.sh) that each test some aspect
of ROM firmware patching support.  Collectively, these provide
a simple confidence check.

Each test is described below.  Each test exits with 0 on success.
On failure, tests prints an "ERROR" message and exit with a value
of 1.

The cleanup.sh script can be used to clean state between tests.

The "runall.sh" scripts runs all tests in order, running cleanup
between each test. The runall.expected file shows the expected
results from runall.sh.  To use any test or the runall script,
load the Host driver to the point where it reaches BMI, then
execute the test.

Note1: Tests assume that bmiloader, rompatcher, and fwpatch
are available (e.g. on your $PATH).

Note2: There are no tests for error handling. It should be
assumed that all released patch distributions are error free
with respect to syntax and RPDF metadata.



Test Descriptions

TEST1: Simple patch
  Installs a single patch which changes 32 bytes at ROM address
  0x004f3000 to remap to RAM address 0x0052d000 and loads the
  RAM area with 
	0x00112233 0x44556677 0x8899aabb 0xccddeeff
	0x00112233 0x44556677 0x8899aabb 0xccddeeff
  This test is repeated for each of the available patchids.

TEST2: Multiple patches in a single RPDF file
  Installs three patches.  The first patch changes 32 bytes at
  ROM address 0x004f3000 to remap to RAM address 0x0052d020 and
  loads the RAM area with
	0x00001111 0x22223333 0x44445555 0x66667777
	0x00001111 0x22223333 0x44445555 0x66667777

  The second patch changes 32 bytes at ROM address 0x004f3020 to
  remap to RAM address 0x0052d060 and loads the RAM area with
  	0x00112233 0x44556677 0x8899aabb 0xccddeeff
  	0x00112233 0x44556677 0x8899aabb 0xccddeeff

  The third patch changes 64 bytes at ROM address 0x004f3040 to
  remap to RAM address 0x0052d100 and loads the RAM area with
	0x01234567 0x89abcdef 0x01234567 0x89abcdef
	0x01234567 0x89abcdef 0x01234567 0x89abcdef

TEST3: Large number of patches (32)
  Installs 32 patches of 32 bytes each, covering a total of
  512 contiguous bytes.  ROM address range starting at 0x004f3000
  is remapped to RAM addresses starting at 0x0052d160.  Values are
  32-bit incrementing counters starting with 0x12345678 and ending
  with 0x12345777.

TEST4: Large size patch (2KB)
  Installs a single patch of 2048 bytes, remapping ROM addresses
  starting at 0x004f3000 to RAM addresses at 0x0052d000.  Values
  written to RAM are incrementing counters starting with 0x11111111
  and ending with 0x11111310.

TEST5: Preloaded patch with 0-size patch data
  First uses bmiloader to write 32 bytes starting at RAM address
  0x0052d060.  Bytes are all "0x55".  Then installs a single patch
  with remap length 32 but ZERO actual patch bytes from ROM address
  0x004f3000 to RAM address 0x0052d060

TEST6: Unused trailing data in RPDF file
  Same as TEST2, but with unsed data at the bottom of the rpdf file.
  This unused data should be ignored.

TEST7: Remap lengths and patch lengths that differ
  First uses bmiloader to clear 128 bytes starting at RAM address
  0x0052d000.  Then installs two patches.  The first patch remaps
  32 bytes from ROM address 0x004f3020 to RAM address 0x0052d060
  and writes only 8 bytes of 0x11 to 0x0052d060.  The second patch
  remaps 32 bytes from ROM address 0x004f3000 to RAM address 0x0052d000
  and writes 40 bytes of 0x22 to 0x0052d010

TEST8: Multiple patches, installed atomically
  Same as TEST2, but uses a single INSTALL/ACTIVATE on the final
  patch and INSTALL ONLY on earlier patches.

TEST9: Multiple ROM ranges remapped to a single RAM range
  First uses bmiloader to clear 64 bytes starting at RAM address
  0x0052d060.  Then installs two patches.  The first patch remaps
  32 bytes from ROM address 0x004f3000 to RAM address 0x0052d060
  with no associated patch data.  The second patch remaps 32 bytes
  from ROM address 0x004f3020 to RAM address 0x0052d060 (the same
  RAM address as above) and writes one word at RAM address
  0x0052d060 with the value 0x99999999.
