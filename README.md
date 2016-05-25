#Chan

Chan is a cross-platfrom zero-copy multi-producer multi-consumer
asynchronous FIFO queue written in C.

[![Build status](https://ci.appveyor.com/api/projects/status/0irf77l0k72o876x?svg=true)](https://ci.appveyor.com/project/nclack/chan)

## Introduction

I tried to keep the interface simple.  See the detailed [API docs][1]
for more.  The documentation and testing framework have some
examples of use.

Reading and writing to the queue are both performed via `Chan_Next()`.
Whether a "read" or a "write" happens depends on the mode passed to
`Chan_Open()`, which returns a reference-counted "reader" or "writer"
reference to the queue.

Chan was designed so that `Chan_Next()` operations block the calling
thread under some circumstances.  With proper use, this design garauntees that
certain networks of threads (namely, directed acyclic graphs) connected by 
Chan instances will deliver every message emited from a producer to a consumer
in that network.  Once the network is assembled, messages will be recieved in
topological order.

## Building

I use [CMake][2] to configure and build.  There are no other dependencies
besides that and a compiler such as gcc, Visual Studio, or clang.

Building usually looks like this:

     mkdir build
     cd build
     cmake ..
     make

[1]: http://nclack.github.com/chan/build/doc/apihtml/index.html
[2]: http://www.cmake.org/

