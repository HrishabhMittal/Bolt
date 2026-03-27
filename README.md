# Bolt

Bolt is a statically-typed programming language running on the Bolt Virtual Machine (BVM). 
This langauge takes heavy inspiration from syntax of Golang.

## Features
- BVM bytecode execution
- package & import system
- strong typing with inbuilt `u/i8-64`, `f32`, `f64`, `bool`, `string`
- `extern` functions for linking to byteCode implementations

## Quick Start
**Build:**
```bash
git clone https://github.com/HrishabhMittal/bvm
./build.sh
```

**Run Example:**
```bash
./run.sh
```

## Syntax Example
note that running the example below currently required `fmt/` folder to be in the compiling directory,
and running it requires you to provide the stdlib implementations of print functions dynamically.
See `run.sh` for more details.

```go
package main
import "fmt"

function main() (i32) {
    fmt.printstring("Hello World\n")
    return 0
}
```
