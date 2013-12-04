EXEC = main
SOURCES = $(wildcard *.cpp)
HEADERS = $(wildcard *.h*)
OBJECTS = $(SOURCES:.cpp=.o)

INCLUDE = /usr/local/include/
LIB = /usr/local/lib/ -lgflags -lboost_log-mt -lboost_log_setup-mt -lboost_filesystem-mt -lboost_thread-mt -lboost_system-mt


CFLAGS = -O3 --std=c++11 -DNDEBUG

CPPLINT = python ~/Code/cpplint/cpplint.py
LINT_FILTER = -legal/copyright,-build/include,-build/header_guard,-readability/braces,-readability/streams

OUTPUT_CSV = /tmp/performance_results.csv

all: $(EXEC)

main: $(OBJECTS)
	clang++ $(CFLAGS) -L$(LIB) $(OBJECTS) -o $(EXEC)

%.o: %.cpp $(HEADERS)
	clang++ $(CFLAGS) -I$(INCLUDE) -c $< -o $@ 

clean:
	rm -f $(EXEC) $(OBJECTS)

lint:
	$(CPPLINT) --filter=$(LINT_FILTER) *.h *.cpp

analysis:
	python analysis/driver.py $(OUTPUT_CSV)
	Rscript analysis/regression.R $(OUTPUT_CSV) analysis/images

.PHONY: analysis
