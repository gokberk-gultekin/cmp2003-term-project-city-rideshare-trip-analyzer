CXX       := g++
CXXFLAGS  := -std=c++17 -O2 -Wall -Wextra -I.
LDFLAGS   :=

APP       := app
TESTBIN   := tests

APP_SRC   := main.cpp analyzer.cpp
TEST_SRC  := test_trip_analyzer.cpp analyzer.cpp catch_amalgamated.cpp

.PHONY: all clean run test list A B C \
        A1 A2 A3 B1 B2 B3 C1 C2 C3

all: $(APP) $(TESTBIN)

# ---------------- build student app ----------------
$(APP): $(APP_SRC) analyzer.h
	$(CXX) $(CXXFLAGS) $(APP_SRC) -o $@ $(LDFLAGS)

# ---------------- build catch2 test runner ----------------
$(TESTBIN): $(TEST_SRC) analyzer.h catch_amalgamated.hpp
	$(CXX) $(CXXFLAGS) $(TEST_SRC) -o $@ $(LDFLAGS)

# ---------------- convenience targets ----------------
run: $(APP)
	./$(APP)

test: $(TESTBIN)
	./$(TESTBIN) -r console -s

# list all tests (useful to verify names/tags)
list: $(TESTBIN)
	./$(TESTBIN) --list-tests

# Run categories (if you want category-level scoring)
A: $(TESTBIN)
	./$(TESTBIN) "[A]" -r console -s

B: $(TESTBIN)
	./$(TESTBIN) "[B]" -r console -s

C: $(TESTBIN)
	./$(TESTBIN) "[C]" -r console -s

# ---------------- per-test targets (point tests) ----------------
# These assume your TEST_CASE names include "A1", "A2", ... OR you tagged them.
# In your provided test file, they are named like "A1 (5%) ...", etc. :contentReference[oaicite:3]{index=3}
A1: $(TESTBIN)
	./$(TESTBIN) "A1*" -r console -s

A2: $(TESTBIN)
	./$(TESTBIN) "A2*" -r console -s

A3: $(TESTBIN)
	./$(TESTBIN) "A3*" -r console -s

B1: $(TESTBIN)
	./$(TESTBIN) "B1*" -r console -s

B2: $(TESTBIN)
	./$(TESTBIN) "B2*" -r console -s

B3: $(TESTBIN)
	./$(TESTBIN) "B3*" -r console -s

C1: $(TESTBIN)
	FAST=1 ./$(TESTBIN) "C1*" -r console -s

C2: $(TESTBIN)
	FAST=1 ./$(TESTBIN) "C2*" -r console -s

C3: $(TESTBIN)
	FAST=1 ./$(TESTBIN) "C3*" -r console -s

clean:
	rm -f $(APP) $(TESTBIN)
