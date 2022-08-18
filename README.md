# Overview

**jvmkill** is a simple [JVMTI][] agent that forcibly terminates the JVM
when it is unable to allocate memory or create a thread. This is important
for reliability purposes: an `OutOfMemoryError` or thread creation failure
will often leave the JVM in an inconsistent state. Terminating the JVM will
allow it to be restarted by an external process manager.

[JVMTI]: http://docs.oracle.com/javase/8/docs/technotes/guides/jvmti/

It is often useful to automatically dump the Java heap using the
`-XX:+HeapDumpOnOutOfMemoryError` JVM argument. This agent will be
notified and terminate the JVM after the heap dump completes.

# Building

    make JAVA_HOME=/path/to/jdk

# Usage

Run Java with the agent added as a JVM argument:

    -agentpath:/path/to/libjvmkill.so

Alternatively, if modifying the Java command line is not possible, the
above may be added to the `JAVA_TOOL_OPTIONS` environment variable.

# Alternatives

Newer JVMs (8u92+) support the `-XX:ExitOnOutOfMemoryError` option to
exit on out of memory. Unfortunately, this option does not cover thread
creation failure, therefore we still recommend using this agent. It is
fine to use the agent in combination with this JVM option.

For older JVMs, a common alternative to this agent is to use the
`-XX:OnOutOfMemoryError` JVM argument to execute a `kill -9` command.
Unfortunately, the JVM uses the `fork()` system call to execute the kill
command and that system call can fail for large JVMs due to memory
overcommit limits in the operating system. Additionally, as with
`-XX:ExitOnOutOfMemoryError`, this does not cover thread creation failure.
