# descript
Declarative scripting language experiment

Todo:

**Core**
- [ ] Global event queue
- [ ] Containers (object that owns/groups multiple instances with a single ID)
- [ ] Data objects
  - [ ] Messages
  - [ ] Channels
- [ ] Stack variables
- [ ] Mitigation for infinite loops
- [ ] Non-scalar variable type support
  - [ ] Arrays for variables
  - [ ] Dictionaries for variables
  - [ ] Structs for variables
- [ ] Wire kinds (plug-powered, state-powered, pulsed)
- [ ] Bytecode validation
- [ ] Serialize custom node data to assembly
- [ ] Coallesce events (dependencies, inputs, etc.)
- [ ] Power control nodes
- [ ] Delayed (or multi-tick) activation
- [ ] Threading model
- [ ] Snapshots
- [ ] Debug data
- [ ] Environment flags for compiler
- [ ] Serialize source graph name hash and data hash w/ API to verify on load

**API**
- [ ] Rename assembly to something less likely to be confused with assembler
- [ ] Standard library of node types
- [ ] Extensible variable types
- [ ] Documentation
- [ ] Runtime error codes and tracking
- [ ] Assembly validation failure codes
- [ ] Expose compiler and assembly format version numbers for external compiler usage
- [ ] Explore possibility of C API

**Samples**
- [ ] Demo
- [ ] Timer system example
- [ ] Schema and node database
- [ ] Dear Imgui editor
- [ ] Simple game
- [ ] Debugger
- [ ] Profiler
- [ ] Network-like tests
