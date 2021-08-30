ChangeLog
=========

All notable changes to the project are documented in this file.

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


[v1.3]: https://github.com/troglobit/mping/compare/v1.2...v1.3
