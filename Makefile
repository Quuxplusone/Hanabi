CXXFLAGS += -std=c++14
CXXFLAGS += -W -Wall -Wextra -pedantic -Wno-sign-compare
CXXFLAGS += ${EXTRA_CXXFLAGS}

ifeq ($(FAST),1)
  CXXFLAGS += -O3 -DNDEBUG -DHANABI_SERVER_NDEBUG -flto -Wno-unused-parameter -Wno-unused-variable
endif

ifeq ($(OPENMP),1)
  CXXFLAGS += -fopenmp
  LDFLAGS += -fopenmp
endif

all: run_BlindBot run_SimpleBot run_ValueBot run_HolmesBot \
     run_SmartBot run_CheatBot run_NewCheatBot run_InfoBot

.PHONY clean:
	rm -f *.o
	rm -f run_*Bot
	rm -f exp_*Bot

run_%.o: main.cc %.h
	$(CXX) $(CXXFLAGS) -DBOTNAME=$* main.cc -c -o $@

exp_%.o: experiment-harness.cc %.h
	$(CXX) $(CXXFLAGS) -DBOTNAME=$* experiment-harness.cc -c -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) $< -c -o $@

run_%: run_%.o HanabiServer.o %.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

exp_%: exp_%.o HanabiServer.o %.o
	$(CXX) $(LDFLAGS) -o $@ $^
