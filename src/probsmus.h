#ifndef PROBSMUS_H_
#define PROBSMUS_H_

#include "estructuras.h"

void generar_jugadas(Jugada jugadas[]);
void probabilidad_descarte(Descarte *descarte, char *jugada);
int es_submano(char sub[], char mano[]);
void Get_MPI_Tipo_Jugada(MPI_Datatype *tipo_jugada);
void print_jugada(Jugada jugada, int i);

#endif /* PROBSMUS_H_ */
