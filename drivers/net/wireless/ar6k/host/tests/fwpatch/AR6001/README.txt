AR6000 ROM Firmware Patch Sanity Test

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
  Installs a single patch which changes 16 bytes at ROM address
  0xa1030000 to remap to RAM address 0xa0013000 and loads the
  RAM area with 0x00112233 0x44556677 0x8899aabb 0xccddeeff.
  This test is repeated for each of the available patchids.

TEST2: Multiple patches in a single RPDF file
  Installs three patches.  The first patch changes 16 bytes at
  ROM address 0xa1030000 to remap to RAM address 0xa0013020 and
  loads the RAM area with 0x00001111 0x22223333 0x44445555 0x66667777.
  The second patch changes 16 bytes at ROM address 0xa1030010 to
  remap to RAM address 0xa0013070 and loads the RAM area with
  0x00112233 0x44556677 0x8899aabb 0xccddeeff.  The third patch
  changes 32 bytes at ROM address 0xa1030020 to remap to RAM
  address 0xa0013120 and loads the RAM area with
  0x01234567 0x89abcdef 0x01234567 0x89abcdef
  0x01234567 0x89abcdef 0x01234567 0x89abcdef.

TEST3: Large number of patches (32)
  Installs 32 patches of 16 bytes each, covering a total of
  512 contiguous bytes.  ROM address range starting at 0xa1030000
  is remapped to RAM addresses starting at 0xa0013050.  Values are
  32-bit incrementing counters starting with 0x12345678 and ending
  with 0x123456f7.

TEST4: Large size patch (2KB)
  Installs a single patch of 2048 bytes, remapping ROM addresses
  starting at 0xa1030000 to RAM addresses at 0xa0013000.  Values
  written to RAM are incrementing counters starting with 0x11111111
  and ending with 0x11111310.

TEST5: Preloaded patch with 0-size patch data
  First uses bmiloader to write 16 bytes starting at RAM address
  0xa0013070.  Bytes are all "0x55".  Then installs a single patch
  with remap length 16 but ZERO actual patch bytes from ROM address
  0xa1030000 to RAM address 0xa0013070.

TEST6: Unused trailing data in RPDF file
  Same as TEST2, but with unsed data at the bottom of the rpdf file.
  This unused data should be ignored.

TEST7: Remap lengths and patch lengths that differ
  First uses bmiloader to clear 64 bytes starting at RAM address
  0xa0013040.  Then installs two patches.  The first patch remaps
  16 bytes from ROM address 0xa1030010 to RAM address 0xa0013070
  and writes only 8 bytes of 0x11 to 0xa0013070.  The second patch
  remaps 16 bytes from ROM address 0xa1030000 to RAM address 0xa0013040
  and writes 32 bytes of 0x22 to 0xa0013040.

TEST8: Multiple patches, installed atomically
  Same as TEST2, but uses a single INSTALL/ACTIVATE on the final
  patch and INSTALL ONLY on earlier patches.

TEST9: Multiple ROM ranges remapped to a single RAM range
  First uses bmiloader to clear 16 bytes starting at RAM address
  0xa0013060.  Then installs two patches.  The first patch remaps
  16 bytes from ROM address 0xa1030000 to RAM address 0xa0013060
  with no associated patch data.  The second patch remaps 16 bytes
  from ROM address 0xa1030010 to RAM address 0xa0013060 (the same
  RAM address as above) and writes one word at RAM address
  0xa0013060 with the value 0x99999999.
