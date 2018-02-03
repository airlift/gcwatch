ifndef JAVA_HOME
    $(error JAVA_HOME not set)
endif

INCLUDE= -I"$(JAVA_HOME)/include" -I"$(JAVA_HOME)/include/linux"
CFLAGS=-Wall -Werror -std=c++11 -fPIC -shared $(INCLUDE)

TARGET=libgcwatch.so

.PHONY: all clean test

all:
	g++ $(CFLAGS) -o $(TARGET) gcwatch.cpp
	chmod 644 $(TARGET)

clean:
	rm -f $(TARGET)
	rm -f *.class

test: all
	$(JAVA_HOME)/bin/javac GcWatchTest.java
	$(JAVA_HOME)/bin/java -XX:+UseG1GC -Xmx1g -Xms1g \
	    -agentpath:$(PWD)/$(TARGET)=port=2000 \
	    -cp $(PWD) GcWatchTest
