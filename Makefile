# Parte A (FDTD + barrera). En el repo del equipo con includes/, src/, tests/:
#   make test
#   make test-race RACE_NX=512 RACE_STEPS=300 RACE_RUNS=20
CC       ?= gcc
CFLAGS   ?= -std=c17 -Wall -Wextra -pedantic -O2 -pthread
INC_DIR  ?= includes
SRC_DIR  ?= src
TEST_DIR ?= tests
BUILD    ?= build
RACE_RUNS  ?= 10
RACE_NX    ?= 257
RACE_STEPS ?= 200

CPPFLAGS := -I$(INC_DIR)
LDLIBS   := -lm
CORE     := $(SRC_DIR)/fdtd_solver.c $(SRC_DIR)/barrier.c

TESTS := test_barrier test_fdtd test_determinism

.PHONY: all test test-race sin-barrera bench clean
all: $(addprefix $(BUILD)/,$(TESTS)) $(BUILD)/fdtd_bench

$(BUILD):
	mkdir -p $@

$(BUILD)/test_barrier: $(TEST_DIR)/test_barrier.c $(SRC_DIR)/barrier.c | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ $(LDLIBS) -o $@
$(BUILD)/test_fdtd: $(TEST_DIR)/test_fdtd.c $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ $(LDLIBS) -o $@
$(BUILD)/test_determinism: $(TEST_DIR)/test_determinism.c $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/fdtd_bench: $(SRC_DIR)/fdtd_bench.c $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ $(LDLIBS) -o $@

bench: $(BUILD)/fdtd_bench
	@$(BUILD)/fdtd_bench --header
	@for t in 1 2 4; do $(BUILD)/fdtd_bench 512 512 100 $$t 0; done

test: all
	$(BUILD)/test_barrier
	$(BUILD)/test_fdtd
	$(BUILD)/test_determinism

sin-barrera: | $(BUILD)
	$(CC) $(CFLAGS) -DSIN_BARRERA $(CPPFLAGS) $(TEST_DIR)/test_determinism.c $(CORE) $(LDLIBS) -o $(BUILD)/test_determinism_race

test-race: sin-barrera
	@fails=0; \
	for i in $$(seq $(RACE_RUNS)); do \
	  $(BUILD)/test_determinism_race $(RACE_NX) $(RACE_STEPS) >/dev/null 2>&1 || fails=$$((fails+1)); \
	done; \
	echo "SIN_BARRERA: $$fails de $(RACE_RUNS) ejecuciones detectaron diferencias"; \
	if [ $$fails -ge 1 ]; then echo "OK: la prueba de determinismo detecta la carrera"; \
	else echo "AVISO: no se observo la carrera en esta maquina"; exit 1; fi

clean:
	rm -rf $(BUILD)