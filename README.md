# Bolt: a Golang like* language that compiles to NASM ELF64 Assembly
This language syntax currently draw a lot of inspiration from golang but is not yet finalised and the syntax of the language might change in the future.

## Implemented Stuff
- [X] Lexer
- [ ] Pre-Processor
- [X] Parser
- [X] Emitter

# Changing
- (Maybe) Change from assembly to a VM with custom bytecode to make this crossplatform

# To Do

- [ ] imports
- [ ] external linking
- [ ] change syntax
- [ ] string support
- [ ] call to and link with c stdlib
- [ ] better abstraction?
- [ ] better testcases
- [ ] structs with methods
- [ ] enums
- [ ] make everything a class, introducing types like i8-64, f32, f64
- [ ] build stdlib
