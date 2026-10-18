
#include "snapshot.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const unsigned long id = 9001;
    const int paso = 4;
    const char *nombre = "job9001_step4.bin";

    double matriz[4] = {1.0, 2.0, 3.0, 4.0};

    /* Eliminar archivo anterior si existe */
    remove(nombre);

    /* Inicializar bufer y escritor */
    assert(inicializar_instantaneas(2) == 0);
    assert(iniciar_escritor() == 0);

    /* Agregar una instantanea */
    assert(agregar_instantanea(id, paso,
                              matriz, 2, 2) == 0);

    /* Esperar hasta que se guarde */
    assert(esperar_instantanea(id, paso) == 0);

    /* Comprobar que el archivo existe */
    FILE *archivo = fopen(nombre, "rb");
    assert(archivo != NULL);

    int32_t encabezado[3];
    double valores[4];

    /* Leer encabezado y matriz */
    assert(fread(encabezado, sizeof(int32_t),
                 3, archivo) == 3);

    assert(fread(valores, sizeof(double),
                 4, archivo) == 4);

    assert(fclose(archivo) == 0);

    /* Verificar dimensiones y paso */
    assert(encabezado[0] == 2);
    assert(encabezado[1] == 2);
    assert(encabezado[2] == paso);

    /* Verificar los valores de Ez */
    assert(memcmp(valores, matriz,
                  sizeof(matriz)) == 0);

    /* Cerrar el bufer y el escritor */
    cerrar_instantaneas();
    detener_escritor();

    /* No permitir nuevas instantaneas */
    assert(agregar_instantanea(id, paso,
                              matriz, 2, 2) == -1);

    /* Liberar recursos */
    destruir_instantaneas();

    /* Eliminar archivo generado */
    assert(remove(nombre) == 0);

    printf("OK: test_snapshot\n");

    return 0;
}

