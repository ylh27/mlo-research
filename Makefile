
# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -g

# Source and object files
SRC = src/route.cpp src/client.cpp src/server.cpp src/helper.c

# Object files
OBJ = $(SRC:.cpp=.o)

# Executable name
EXEC = route

# Default target
all: $(EXEC)

# Link object files to create executable
$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean
