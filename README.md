# descript
Declarative scripting language experiment

Done:
- [x] Instance event queue
- [x] Remove dsBlob it's only used for one thing
- [x] Example C++ class implementation of nodes
- [x] Separate functions/indices/etc for input vs output plugs
- [x] Flatten plug and slot indices in assembly
- [x] Make DLL
- [x] Re-examine use of UUID as default identifier type
- [x] Expressions
- [x] Functions
- [x] External dependency notification
- [x] Rethink Runtime Host to be a richer extension point for embedding apps

Todo:
- [ ] Global event queue
- [ ] Coallesce events (dependencies, inputs, etc.)
- [ ] Rename assembly to something less likely to be confused with assembler
- [ ] Standard library of node types
- [ ] Stack variables
- [ ] Mitigation for infinite loops
- [ ] Containers (object that owns/groups multiple instances with a single ID)
- [ ] Refactor dsExpression and dsExpressionCompiler to be public APIs
- [ ] Data objects
  - [ ] Messages
  - [ ] Channels
- [ ] Serialize custom node data to assembly
- [ ] Extensible variable types
- [ ] Wire kinds (plug-powered, state-powered, pulsed)
- [ ] Non-scalar variable type support
  - [ ] Arrays for variables
  - [ ] Dictionaries for variables
  - [ ] Structs for variables
- [ ] Bytecode validation
- [ ] Power control nodes
- [ ] Delayed (or multi-tick) activation
- [ ] Threading model
- [ ] Snapshots
- [ ] Debug data
- [ ] Environment flags for compiler
- [ ] Explore C API
- [ ] Instance pooling
- [ ] Network-like tests
- [ ] Timer system example
- [ ] Demo
  - [ ] Schema and node database
  - [ ] Dear Imgui editor
  - [ ] Simple game
  - [ ] Debugger
  - [ ] Profiler
- [ ] Documentation
- [ ] Serialize source graph name hash and data hash w/ API to verify on load
- [ ] Expose compiler and assembly format version numbers for external compiler usage
- [ ] Assembly validation failure codes
- [ ] Runtime error codes and tracking
- [ ] CI
