ifndef CC
CC:=gcc
endif

ifndef JAVA_HOME
    $(error JAVA_HOME not set)
endif

ifeq ($(shell uname),Darwin)
INCLUDE= -I"$(JAVA_HOME)/include" -I"$(JAVA_HOME)/include/darwin"
else
INCLUDE= -I"$(JAVA_HOME)/include" -I"$(JAVA_HOME)/include/linux"
endif
CFLAGS=-Wall -Werror -fPIC -shared $(INCLUDE)

TARGET=libjvmkill.so
BUILD_DIR=build

.PHONY: all clean test

all:
	@echo "Building jvmkill with $(CC)"
	$(CC) $(CFLAGS) -o $(TARGET) jvmkill.c
	chmod 644 $(TARGET)

clean:
	rm -f $(TARGET)
	rm -f *.class
	rm -f *.hprof
	rm -rf $(BUILD_DIR)

test: all
	$(JAVA_HOME)/bin/javac JvmKillTest.java
	$(JAVA_HOME)/bin/java -Xmx1m \
	    -XX:+HeapDumpOnOutOfMemoryError \
	    -XX:OnOutOfMemoryError='/bin/echo hello' \
	    -agentpath:$(PWD)/$(TARGET) \
	    -cp $(PWD) JvmKillTest

# Extract JNI headers from Corretto JDK11 docker image
jni_headers_arm64:
	mkdir -p $(BUILD_DIR)/arm64
	docker run -v `pwd`/$(BUILD_DIR):/work -it amazoncorretto:11 bash -c 'cp -r $${JAVA_HOME}/include /work/arm64/'

# Run cross-compiler for aarch64 (arm64) target
arm64: jni_headers_arm64
	./docker/dockcross-aarch64 -a --rm bash -c 'make TARGET=libjvmkill-arm64.so JAVA_HOME=$(BUILD_DIR)/arm64'
