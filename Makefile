CPPLINT = python ~/Code/cpplint/cpplint.py
LINT_FILTER = -legal/copyright,-build/include,-build/header_guard,-readability/braces,-readability/streams
OUTPUT_CSV = /tmp/performance_results.csv
BUILD = build

build:
	mkdir -p $(BUILD) && cd $(BUILD) && cmake ../ -G Ninja && ninja -v

lint:
	$(CPPLINT) --filter=$(LINT_FILTER) *.h *.cpp

analysis:
	python analysis/driver.py $(BUILD)/perf $(OUTPUT_CSV)
	Rscript analysis/regression.R $(OUTPUT_CSV) analysis/images

.PHONY: analysis build lint
