ChangeLog
=========

All notable changes to the project are documented in this file.


[v2.0][] - 2022-05-12
---------------------

### Changes
- Add IPv6 support, still ASM
- Detect address family from command line group argument, or `-6`


[v1.6][] - 2021-10-31
---------------------

### Changes
- Add Dockerfile with build + deploy to ghcr.io
- Use `clock_gettime()` to reliably measure time instead of unreliable
  `gettimeofday()`, which only gives the (auto-)adjustable wall clock
- Improved error messages from `init_socket()`, e.g. inform the user if
  they are trying to use an interface without MULTICAST flag set
- Replace `etime=` and `atime=` with ping-like `time=`.  This change is
  actually a follow-up to [v1.3][] where the random delay in the receiver
  was dropped
- The mping `VERSION` define is now extracted from the sources rather
  than defined in the Makefile.  This facilitates easier import in other
  project's build systems and ensures the version is not lost


[v1.5][] - 2021-09-05
---------------------

### Changes
- Update README, tips and usage section
- Add `-w DEADLINE`, like regular ping(1) tool, this adds the ping(1)
  behavior to the `-c COUNT` option, instead of exit after COUNT sent
  mpings, we now wait DEADLINE seconds to *receive* COUNT mpings

### Fixes
- Don't exit with error if receiving more than COUNT packets


[v1.4][] - 2021-08-31
---------------------

### Changes
- Add man page to section 1
- Add missing license heading to source file

### Fixes
- Fix missing newline and options in usage text


[v1.3][] - 2021-08-30
---------------------

### Changes
- Reindent to Linux KNF
- Simplfy command line args
- Add `-c COUNT` to stop after COUNT packet instead of hardcoded 5
- Add `-W SEC` for sender timeout
- Add `-q` for quiet output
- Change `-v` to `-d` for debug output
- Change `-V` to `-v` to show version information
- Drop random delay in receiver mode, not useful in production
- Add basic test case

### Fixes
- Interface address and default interface lookup
- Fix formatting of messages to align between sender and receiver
- Exit with error code if we receive fewer packets than expected
- Set IP header TTL value to allow for traversing routers


v1.2 - 2014-06-20
-----------------

Initial version, available from the toolbox repo.

[UNRELEASED]: https://github.com/troglobit/mping/compare/v2.0...HEAD
[v2.0]: https://github.com/troglobit/mping/compare/v1.6...v2.0
[v1.6]: https://github.com/troglobit/mping/compare/v1.5...v1.6
[v1.5]: https://github.com/troglobit/mping/compare/v1.4...v1.5
[v1.4]: https://github.com/troglobit/mping/compare/v1.3...v1.4
[v1.3]: https://github.com/troglobit/mping/compare/v1.2...v1.3
