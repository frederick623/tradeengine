# Trade Engine

Need Boost>1.85 and explictly set c++20 to let b2 to build Boost Cobalt

1. Download Boost source
2. Navigate into folder
3. ./bootstrap.sh
4. ./b2 cxxstd=20
5. Back to source folder
6. Use cmake to configure, build and run

The program uses Boost Task (coroutine) to act as module to be executed. The goal is to flexibly add input source (e.g. Reader) and processor (e.g. Exchange) which communicates through channel (task-based and message passing).

Therefore it can support more modules to be run in one io_context and also possibly allow different communcation channel between modules.
