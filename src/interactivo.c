/*
 * interactivo.c
 *
 *  Created on: 24 ago. 2016
 *      Author: alumnopp
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interactivo.h"
#include "juegomus.h"
#include "mpi.h"

static void print_mano(Carta mi_mano[]);

/**
 * Función: I_quiero_mus
 * ---------------------
 * Comunicación con el proceso manager que recibe la respuesta
 * del jugador humano a la pregunta de si se quiere o no se quiere
 * mus.
 *
 * mi_mano:     array de Cartas con la jugada del jugador
 * global_comm: comunicador global (incluye al proceso manager)
 *
 * return: 1 si el jugador quiere mus, 0 en caso contrario
 */
int I_quiero_mus(Carta *mi_mano, MPI_Comm global_comm)
{
	int op = 1, resp;
	MPI_Status status;

	printf("Jugador 0 (humano):\n\n");
	print_mano(mi_mano);
	printf("¿Quieres Mus? (s)/n: ");

	MPI_Send(&op, 1, MPI_INT, 0, 0, global_comm);
	MPI_Recv(&resp, 1, MPI_INT, 0, 0, global_comm, &status);
	printf("\n");

	return resp;
}

/**
 * Función: I_get_descartes
 * ------------------------
 * Comunicación con el proceso manager que recibe las cartas  de las que desea
 * descartarse el jugador humano.
 *
 * mi_mano (INOUT):       array de Cartas con la jugada del jugador
 * mis_descartes (INOUT): array de Cartas con las cartas que se quiere descartar
 *                        el jugador
 * global_comm:           comunicador global (incluye al proceso manager)
 *
 * return: la cantidad de cartas de las que se ha descartado el jugador
 */
int I_get_descartes(Carta **mi_mano, Carta **mis_descartes, MPI_Comm global_comm)
{
	int op = 2, i, n_descartes = 0;
	MPI_Status status;
	int descartes[4] = { 0, 0, 0, 0 }; // descartes[i] = 0: no quiero descartar mi_mano[i]
	                                   // descartes[i] = 1: sí quiero descartar mi_mano[i]

	printf("\n");
	print_mano(*mi_mano);
	printf("Indica las cartas de las quieres descartarte separando la lista con comas: ");

	MPI_Send(&op, 1, MPI_INT, 0, 0, global_comm);
	MPI_Recv(&descartes, 4, MPI_INT, 0, 0, global_comm, &status);

	for (i = 0; i < 4; i++)
		if (descartes[i] == 1) n_descartes++;

	//*mis_descartes = (Carta *)malloc(sizeof(Carta) * n_descartes);
	*mis_descartes = malloc_cartas(n_descartes);
	//Carta *en_mano = (Carta *)malloc(sizeof(Carta) * (4 - n_descartes));
	Carta *en_mano = malloc_cartas(4 - n_descartes);
	int j = 0, k = 0;
	for (i = 0; i < 4; i++) {
		if (descartes[i] == 1) {
			memcpy(*mis_descartes + j, *mi_mano + i, sizeof(Carta));
			j++;
		}
		else {
			memcpy(en_mano + k, *mi_mano + i, sizeof(Carta));
			k++;
		}
	}

	// Copiamos las no descartadas al array de mi_mano
	//*mi_mano = (Carta *)realloc(*mi_mano, sizeof(Carta) * (4 - n_descartes));
	*mi_mano = realloc_cartas(mi_mano, 4, 4 - n_descartes);
	memcpy(*mi_mano, en_mano, (4 - n_descartes) * sizeof(Carta));
	//free(en_mano);
	//free_cartas(en_mano, 4 - n_descartes);

	// Devolver la cantidad de cartas descartadas
	return n_descartes;
}

/**
 * Función: I_envidar
 * ------------------
 * Comunicación con el proceso manager que recibe la cantidad de tantos que el
 * jugador humano desea envidar en un lance cualquiera.
 *
 * envites (INOUT): estructura con información acerca del estado actual de los
 *                  envites
 * ultimo_envite:   tantos envidados hasta el momento
 * tantos:          tantos necesarios para órdago
 * postre:          rank del jugador postre
 * mi_mano:         array de Cartas con la jugada del jugador
 * global_comm:     comunicador global (incluye al proceso manager)
 */
void I_envidar(Envite *envites, int ultimo_envite, int tantos, int postre, Carta *mi_mano, MPI_Comm global_comm)
{
	int op = 3, resp;
	MPI_Status status;

	printf("\n");
	print_mano(mi_mano);
	printf("\n¿Qué quieres hacer? ");
	if (ultimo_envite > 0 && ultimo_envite < tantos)
		printf("Pasar (0), Ver (%d), Envidar (> %d) o lanzar Órdago (%d): ", ultimo_envite, ultimo_envite, tantos);
	else if (ultimo_envite == 0)
		printf("Pasar (0), Envidar (>= 2) o lanzar Órdago (%d): ", tantos);
	else
		printf("Pasar (0) o ver Órdago (%d): ", tantos);

	MPI_Send(&op, 1, MPI_INT, 0, 0, global_comm);
	MPI_Recv(&resp, 1, MPI_INT, 0, 0, global_comm, &status);


	// Se envida menos que la cantidad del último envite o menos de 2 (envite mínimo):
	if (resp < ultimo_envite || (resp < 2 && ultimo_envite == 0)) {
		// Pasar/cerrar
		if ((envites->estado == nulo && postre == 0) || ( envites->estado != nulo && (envites->ultimo + 3) % 4 == 0 )) {
			// Si soy postre (el último en apostar) y no ha habido envites, o si soy el
			// jugador anterior al último en envidar, puedo cerrar.
			envites->estado = cerrado;
			envites->ultimo = 0;
			printf("Jugador 0 (humano): cierro.\n");
		}
		else printf("Jugador 0 (humano): paso\n");
	}
	// Se envida la misma cantidad que el ultimo envite:
	else if (resp == ultimo_envite && ultimo_envite != 0) {
		// Ver
		if (envites->estado == visto) {
			// Compañero ya ha visto el envite, cerramos
			envites->estado = cerrado;
			envites->ultimo = 0;
			printf("Jugador 0 (humano): cierro.\n");
		}
		else {
			// Lo veo
			envites->envites[0] = ultimo_envite;
			if (envites->ultimo == 1) {
				envites->estado = cerrado; // Cerrar si soy el anterior al último envite
				envites->ultimo = 0;
			}
			else envites->estado = visto;
			printf("Jugador 0 (humano): lo veo\n");
		}
	}
	// Se envida una cantidad superior al último envite e inferior a la necesaria para el órdago:
	else if (resp > ultimo_envite && resp < tantos) {
		envites->envites[0] = resp;
		envites->estado = envite;
		envites->ultimo = 0;
		printf("Jugador 0 (humano): envido %d\n", envites->envites[0]);
	}
	// Se ha envidado una cantidad igual (0 mayor) a la necesaria para el órdago:
	else {
		if (envites->estado == ordago) {
			envites->estado = cerrado;
			printf("Jugador 0 (humano): lo veo!\n");
		}
		else {
			envites->estado = ordago;
			printf("Jugador 0 (humano): órdago!\n");
		}
		envites->envites[0] = tantos;
		envites->ultimo = 0;
	}
}

/**
 * Función: print_mano
 * -------------------
 * Imprime en pantalla la jugada (mano) del jugador.
 *
 * mi_mano: array de Cartas con la jugada del jugador
 */
static void print_mano(Carta mi_mano[])
{
	printf("Mis cartas (%s):\n", mano2figuras8R(mi_mano, 4));
	printf("   1. %s\n", carta2Lchar(mi_mano[0]));
	printf("   2. %s\n", carta2Lchar(mi_mano[1]));
	printf("   3. %s\n", carta2Lchar(mi_mano[2]));
	printf("   4. %s\n", carta2Lchar(mi_mano[3]));
}
