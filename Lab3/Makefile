LLVM_CONFIG = llvm-config-10
CXX = clang++-10
CXXFLAGS = -fno-rtti -std=c++17 `$(LLVM_CONFIG) --cxxflags`
LDFLAGS = `$(LLVM_CONFIG) --ldflags --libs core analysis support passes` -shared
SRC = CGraph.cpp
TARGET = CGraph.so

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) -Wall -fPIC -shared $(SRC) -o $(TARGET) $(CXXFLAGS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
