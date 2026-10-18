# make          Compilar el proyecto
# make test     Ejecutar las pruebas
# make bench    Ejecutar benchmark
# make sin-barrera   Compilar sin barrera
# make test-race     Comprobar condiciones de carrera
# make clean    Limpiar archivos generados

CC       ?= gcc
CFLAGS   ?= -std=c17 -Wall -Wextra -pedantic -O2 -pthread
LDFLAGS  ?= -pthread

INC_DIR  ?= includes
SRC_DIR  ?= src
TEST_DIR ?= tests
BUILD    ?= build
BIN      ?= bin

RACE_RUNS  ?= 10
RACE_NX    ?= 257
RACE_STEPS ?= 200

CPPFLAGS := -I$(INC_DIR) -MMD -MP
LDLIBS   := -lm

CORE  := $(SRC_DIR)/fdtd_solver.c $(SRC_DIR)/barrier.c
TESTS := test_barrier test_fdtd test_determinism

MOTOR_OBJ := $(BUILD)/fdtd_solver.o $(BUILD)/barrier.o

SERVER_OBJ := $(addprefix $(BUILD)/, \
    fdtd_server.o protocolo.o job_manager.o queue.o \
    dispatcher.o rwlock.o shared_state.o snapshot.o system.o) \
    $(MOTOR_OBJ)

CLIENT_OBJ := $(BUILD)/fdtd_client.o $(BUILD)/protocolo.o

# Incluir los tests de C que existan en tests/
C_TESTS := $(patsubst $(TEST_DIR)/%.c,%, \
    $(wildcard $(addprefix $(TEST_DIR)/, \
    test_queue.c test_job_manager.c test_rwlock.c test_snapshot.c)))

C_TEST_BIN := $(addprefix $(BUILD)/,$(C_TESTS))

.PHONY: all test test-race sin-barrera bench clean

all: $(addprefix $(BUILD)/,$(TESTS)) \
     $(BUILD)/fdtd_bench \
     $(BIN)/fdtd_server $(BIN)/fdtd_client

# Crear carpetas
$(BUILD) $(BIN):
	mkdir -p $@

# Pruebas de la persona A
$(BUILD)/test_barrier: $(TEST_DIR)/test_barrier.c $(SRC_DIR)/barrier.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ $(LDLIBS) -o $@

$(BUILD)/test_fdtd: $(TEST_DIR)/test_fdtd.c $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ $(LDLIBS) -o $@

$(BUILD)/test_determinism: $(TEST_DIR)/test_determinism.c $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ $(LDLIBS) -o $@

# Benchmark de la persona A
$(BUILD)/fdtd_bench: $(SRC_DIR)/fdtd_bench.c $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ $(LDLIBS) -o $@

bench: $(BUILD)/fdtd_bench
	@$(BUILD)/fdtd_bench --header
	@for t in 1 2 4; do $(BUILD)/fdtd_bench 512 512 100 $$t 0; done

# Pruebas sin barrera de la persona A
sin-barrera: | $(BUILD)
	$(CC) $(CFLAGS) -DSIN_BARRERA -I$(INC_DIR) $(TEST_DIR)/test_determinism.c $(CORE) $(LDLIBS) -o $(BUILD)/test_determinism_race

test-race: sin-barrera
	@fails=0; \
	for i in $$(seq $(RACE_RUNS)); do \
	  $(BUILD)/test_determinism_race $(RACE_NX) $(RACE_STEPS) >/dev/null 2>&1 || fails=$$((fails+1)); \
	done; \
	echo "SIN_BARRERA: $$fails de $(RACE_RUNS) ejecuciones detectaron diferencias"; \
	if [ $$fails -ge 1 ]; then echo "OK: la prueba de determinismo detecta la carrera"; \
	else echo "AVISO: no se observo la carrera en esta maquina"; exit 1; fi

# Servidor
$(BIN)/fdtd_server: $(SERVER_OBJ) | $(BIN)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Cliente
$(BIN)/fdtd_client: $(CLIENT_OBJ) | $(BIN)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Compilar archivos fuente
$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

# Prueba de protocolo de la persona B
$(BUILD)/test_protocolo: $(TEST_DIR)/test_protocolo.c $(SRC_DIR)/protocolo.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ $(LDLIBS) -o $@

# Prueba de cola FIFO
$(BUILD)/test_queue: $(TEST_DIR)/test_queue.c $(SRC_DIR)/queue.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ $(LDLIBS) -o $@

# Prueba del gestor de trabajos
$(BUILD)/test_job_manager: $(TEST_DIR)/test_job_manager.c $(SRC_DIR)/job_manager.c $(SRC_DIR)/queue.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ $(LDLIBS) -o $@

# Prueba de lectores y escritores
$(BUILD)/test_rwlock: $(TEST_DIR)/test_rwlock.c $(SRC_DIR)/rwlock.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ $(LDLIBS) -o $@

# Prueba de instantaneas
$(BUILD)/test_snapshot: $(TEST_DIR)/test_snapshot.c $(SRC_DIR)/snapshot.c | $(BUILD)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ $(LDLIBS) -o $@

# Ejecutar las pruebas
test: all $(BUILD)/test_protocolo $(C_TEST_BIN)
	@set -e; \
	for t in $(addprefix $(BUILD)/,$(TESTS)) $(BUILD)/test_protocolo $(C_TEST_BIN); do \
	  echo "Ejecutando $$t"; ./$$t; \
	done

# Limpiar archivos generados
clean:
	rm -rf $(BUILD) $(BIN)

# Dependencias de encabezados
-include $(wildcard $(BUILD)/*.d)
