# Overview

**jvmkill** is a simple [JVMTI][] agent that forcibly terminates the JVM
when it is unable to create a thread. This is important for reliability
purposes: a thread creation failure will often leave the JVM in an
inconsistent state. Terminating the JVM will allow it to be restarted by
an external process manager.

[JVMTI]: http://docs.oracle.com/javase/8/docs/technotes/guides/jvmti/

For out-of-memory conditions (heap or metaspace exhaustion), use
`-XX:+ExitOnOutOfMemoryError`.

# Building

    make JAVA_HOME=/path/to/jdk

# Usage

Run Java with the agent added as a JVM argument:

    -agentpath:/path/to/libjvmkill.so

Alternatively, if modifying the Java command line is not possible, the
above may be added to the `JAVA_TOOL_OPTIONS` environment variable.
