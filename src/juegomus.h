#ifndef JUEGOMUS_H_
#define JUEGOMUS_H_

#include "estructuras.h"

void generar_mazo(Carta *mazo);
void barajar(Carta *mazo, int n_cartas_mazo);
void cortar(Carta **mazo, int n_cartas_mazo, int postre);
void reparto_inicial(Carta **mazo, int *num_cartas, int postre, Carta **mi_mano);
void pasar_mazo(Carta **mazo, int *n_cartas_mazo, int postre);
void descartar(int postre, int interactivo, Carta **mi_mano, Jugada *jugadas, Carta **mazo, int *n_cartas_mazo, Carta **descartadas, int *total_descartes, MPI_Comm global_comm);
int get_descartes(Carta **mi_mano, Jugada *jugadas, Carta **descartes);
int ganador_grande(int myrank, int postre, Jugada jugadas[], int ind_mano, Carta mi_mano[]);
int ganador_chica(int myrank, int postre, Jugada jugadas[], int ind_mano, Carta mi_mano[]);
int ganador_pares(int myrank, int postre, Jugada jugadas[], int ind_mano, Carta mi_mano[]);
int ganador_juego(int myrank, int postre, Jugada jugadas[], int ind_mano, Carta mi_mano[]);
int ganador_punto(int myrank, int postre, Jugada jugadas[], int ind_mano, Carta mi_mano[]);
void recoger_cartas(int postre, Carta **mi_mano, Carta **mazo, int *n_cartas_mazo, Carta **descartadas, int *total_descartes);
void print_manos(int myrank, int postre, char *cartas, Carta mi_mano[]);
char *carta2char(Carta carta);
char *carta2Lchar(Carta carta);
char *mano2figuras8R(Carta *mano, int tam_mano);
char *pares2char(Tpares par);
void print_mazo(Carta *mazo, int n);

#endif /* JUEGOMUS_H_ */
