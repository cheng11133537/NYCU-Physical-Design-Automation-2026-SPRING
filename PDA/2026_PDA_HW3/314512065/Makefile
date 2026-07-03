CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall

TARGET = Legalizer
SRCS = main.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
