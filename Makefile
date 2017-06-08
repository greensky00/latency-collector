LDFLAGS = -pthread
CXXFLAGS = \
	-g -D_GNU_SOURCE \
	-I. -I./src -I./tests \
	--std=c++11 \

CXXFLAGS += -Wall
#CXXFLAGS += -O3

LATENCY_TEST = \
	tests/latency_test.o \

PROGRAMS = \
	tests/latency_test \

all: $(PROGRAMS)

tests/latency_test: $(LATENCY_TEST)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(PROGRAMS) ./*.o ./*.so ./*/*.o ./*/*.so