
all: run_BlindBot run_SimpleBot run_ValueBot run_CheatBot

.PHONY clean:
	rm -f *.o
	rm -f run_*Bot

run_%.o: main.cc %.h
	$(CXX) $(CXXFLAGS) -DBOTNAME=$* main.cc -c -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) $< -c -o $@

run_%: run_%.o HanabiServer.o %.o
	$(CXX) -o $@ $^
