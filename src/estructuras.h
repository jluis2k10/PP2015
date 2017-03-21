#ifndef ESTRUCTURAS_H_
#define ESTRUCTURAS_H_

#include "mpi.h"

typedef enum { false, true } Boolean;
/* Cantidad de manos diferentes en el mus, sin distinguir por palos o por posición. */
enum NUM_MANOS { NUM_MANOS = 330 };
/* Figuras posibles de las cartas */
typedef enum { as, dos, tres, cuatro, cinco, seis, siete, sota, caballo, rey } Nombre_carta;
/* Palos posibles de las cartas */
typedef enum { oros, copas, espadas, bastos } Palo_carta;
/* Tipos posibles de jugadas de Pares */
typedef enum { duples = 3, medias = 2, parejas = 1, nada = 0 } Tpares;
/* Estados posibles de un envite en un momento determinado del lance */
typedef enum { nulo, envite, visto, ordago, cerrado } Estado_envites;

struct carta {
	Nombre_carta *figura;
	Palo_carta *palo;
};
typedef struct carta Carta;

struct descarte {
	Carta *cartas_en_mano;
	Carta *cartas_descartadas;
	int num_descartes;
	double prob_obtener_jugada;
};
typedef struct descarte Descarte;

struct jugada {
	char cartas[5];		  // Representación de la jugada (RRRR, RRCS... )
	double P_gana_grande; // Probabilidad de ganar la Grande
	double P_gana_chica;  // Probabilidad de ganar la Chica
	double P_gana_pares;  // Probabilidad de ganar los Pares
	double P_gana_juego;  // Probabilidad de ganar en el Juego
	double P_gana_punto;  // Probabilidad de ganar al Punto
	int pos_en_chicas;	  // Posición de la mano en orden ascendente
	Boolean pares;		  // ¿Tiene pares?
	Tpares tipo_pares;	  // ¿Duples, Medias o Parejas?
	int valor_pares;	  // Valor/Escala de la mano de Pares
	Boolean juego;		  // ¿Tiene juego?
	int valor_juego;	  // Valor de la mano en el Juego/Puntos
};
typedef struct jugada Jugada;

struct envite {
	int envites[2];	// Apuestas de los jugadores
	int ultimo;		// Último jugador que ha envidado
	Estado_envites estado;
};
typedef struct envite Envite;

Carta * malloc_cartas(int n);
void free_cartas(Carta **cartas, int n);
Carta * realloc_cartas(Carta **cartas, int old, int new);
void Send_cartas(Carta *cartas, int n, int dest, MPI_Comm comm);
void Recv_cartas(Carta *cartas, int n, int orig, MPI_Comm comm);

#endif /* ESTRUCTURAS_H_ */
