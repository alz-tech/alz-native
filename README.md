# AlzScript Native

A truly independent programming language. No Node.js. No Python. No dependencies.

Write `.az` files. Compile. Run anywhere.

```az
name = "Wilfort"
print "Hello {name}!"

define greet(person):
    print "Welcome {person}"

greet("ALZ TECH")
```

## Install

```bash
curl -fsSL https://raw.githubusercontent.com/alz-tech/alz-native/main/install.sh | bash
```

Or download a binary directly from [Releases](https://github.com/alz-tech/alz-native/releases).

## Build from source

```bash
git clone https://github.com/alz-tech/alz-native
cd alz-native

gcc -std=c11 -O2 -Iinclude \
  src/value.c src/chunk.c src/vm.c src/lexer.c \
  src/compiler.c src/stdlib.c src/http.c src/db.c src/main.c \
  -o alzc -lm
```

Requires only `gcc` and `make`. Works on Linux, macOS, Termux (Android).

## Language

### Variables & output
```az
name = "Wilfort"
age  = 25
print "Name: {name}, Age: {age}"
```

### Conditions
```az
if age > 18:
    print "Adult"
else:
    print "Minor"
```

### Loops
```az
names = ["Wilfort", "Sarg", "ALZ"]
each name in names:
    print "Hello {name}"
```

### Functions
```az
define add(a, b):
    return a + b

result = add(10, 5)
print "Result: {result}"
```

### Web server
```az
http.route("GET", "/", "index")
http.route("GET", "/user/:name", "get_user")
http.route("POST", "/api/data", "post_data")
http.serve(3000)
```

### Database
```az
-- Save
user = db.save("users", {name: "Wilfort", role: "admin"})

-- Find
found = db.find("users", {name: "Wilfort"})

-- All
all = db.all("users")

-- Search
results = db.search("users", "wilfort")

-- Update
db.update("users", {name: "Wilfort"}, {role: "superadmin"})

-- Remove
db.remove("users", {name: "Wilfort"})

-- Pagination
page = db.page("users", 1, 10)
```

### Stdlib
```az
-- String
print str.upper("hello")       -- HELLO
print str.replace("hi", "hi", "hello")
print str.split("a,b,c", ",")
print str.contains("AlzScript", "Script")

-- Math
print math.sqrt(144)    -- 12
print math.floor(3.9)   -- 3
print math.random()     -- 0.0 - 1.0

-- Date
print date.string()     -- 2026-06-25
print date.time()       -- 14:30:00
print date.year()       -- 2026

-- File
file.write("data.txt", "hello")
content = file.read("data.txt")
print file.exists("data.txt")
```

## Platforms

| Platform | Binary |
|---|---|
| Linux x64 | `alzc-linux-x64` |
| Linux ARM64 | `alzc-linux-arm64` |
| Linux ARM (Termux) | `alzc-linux-arm` |
| Windows x64 | `alzc-windows-x64.exe` |
| macOS Apple Silicon | `alzc-macos-arm64` |
| macOS Intel | `alzc-macos-x64` |

## Built by

**ALZ TECH** — [github.com/alz-tech](https://github.com/alz-tech)
