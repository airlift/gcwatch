# Overview

**gcwatch** is a simple [JVM TI][] agent that monitors stop-the-world
garbage collections and makes them available via a simple TCP server
interface. This allows detecting if the machine and Java process is still
running, even while the normal Java socket operations are blocked by the
garbage collection event.

[JVM TI]: https://docs.oracle.com/javase/9/docs/specs/jvmti.html

# Protocol

The server returns either `OK` or `GC xxx` where `xxx` is the number of
milliseconds since the garbage collection began, followed by a newline.

# Building

    make JAVA_HOME=/path/to/jdk

# Usage

Run Java with the agent added as a JVM argument, specifying the port
number for the agent to listen on:

    -agentpath:/path/to/libgcwatch.so=port=2000

Alternatively, if modifying the Java command line is not possible, the
above may be added to the `JAVA_TOOL_OPTIONS` environment variable.
