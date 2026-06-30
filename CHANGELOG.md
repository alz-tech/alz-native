# Changelog

## v1.0.0 — Initial Release

### Language
- Variables, strings, numbers, booleans, null
- String interpolation: `"Hello {name}!"`
- Lists and objects
- If / else if / else
- While loops
- Each/for loops
- Functions with parameters and return values
- Recursion

### Stdlib
- `str` — upper, lower, trim, split, replace, contains, reverse, len, slice
- `math` — sqrt, floor, ceil, round, abs, pow, min, max, random, pi
- `date` — string, time, year, month, day, hour, minute, now
- `file` — read, write, append, exists, delete
- `type` — of, str, num, bool
- `env` — get
- `sys` — exit

### HTTP Server
- Raw socket server, zero dependencies
- GET / POST / PUT / DELETE / PATCH
- URL parameter matching: `/user/:name`
- Query string parsing
- JSON body parsing
- CORS headers built in

### Database
- JSON file-based, zero dependencies
- save, find, findById, all, where
- update, updateById, remove, removeById
- count, first, last, clear, drop
- search (full-text), sort, page
- Auto-generated IDs and timestamps

### Platforms
- Linux x64
- Linux ARM64
- Linux ARM (Termux/Android)
- Windows x64
- macOS ARM64 (Apple Silicon)
- macOS x64 (Intel)
