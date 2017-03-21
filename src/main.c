/*
 * main.c
 *
 *  Created on: 18 de jun. de 2016
 *      Author: alumnopp
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpi.h"
#include "estructuras.h"
#include "probsmus.h"
#include "juegomus.h"

static void op_quiero_mus(MPI_Comm global_comm, int rank_jug);
static void op_get_descartes(MPI_Comm global_comm, int rank_jug);
static void op_envidar(MPI_Comm global_comm, int rank_jug);

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);
	int jugando = 1, myrank, size, op;
	MPI_Comm jugadores_comm; // intercomunicador con jugadores
	MPI_Comm global_comm;	 // intracomunicador global (jugadores +  manager)
	int tam_universo = 5;    // workers + manager
	Jugada jugadas[NUM_MANOS];
	Carta *mazo = malloc_cartas(40);
	MPI_Datatype mpi_tipo_jugada;
	MPI_Status status;

	char in[4];
	int tantos = 0, vacas = 0, modo = 1;
	printf("\n¿Vacas para ganar la partida? (3): ");
	if (fgets(in, 4, stdin) != NULL) vacas = strtol(in, NULL, 10);
	if (vacas <= 0) vacas = 3;
	printf("\n¿Tantos para cada vaca? (40): ");
	if (fgets(in, 4, stdin) != NULL) tantos = strtol(in, NULL, 10);
	if (tantos <= 0) tantos = 40;
	printf("\n¿Modo interactivo (participa jugador humano) o automático? (i)/a: ");
	if (fgets(in, 3, stdin) != NULL) if (strcmp(in, "a\n") == 0) modo = 0;


	char **args = malloc(4*sizeof(char*));
	args[0] = malloc(4*sizeof(char*));
	args[1] = malloc(4*sizeof(char*));
	args[2] = malloc(2*sizeof(char*));
	sprintf(args[0], "%d", vacas);
	sprintf(args[1], "%d", tantos);
	sprintf(args[2], "%d", modo);

	printf("\nAVISO: pulsa [INTRO] para avanzar hasta que finalice la ejecución.");
	fflush(NULL);
	getchar();

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	if (size > 1) {
		if (myrank == 0)
			fprintf(stderr, "Error: sólo se permite un proceso manager (padre). Se han encontrado %d procesos. Ejecute con \"mpiexec -np 1 ./main\"\n", size);
		MPI_Finalize();
		exit(1);
	}

	generar_mazo(mazo);
	generar_jugadas(jugadas);

	MPI_Comm_spawn("./Jugador", args, tam_universo - 1, MPI_INFO_NULL, 0, MPI_COMM_SELF, &jugadores_comm, MPI_ERRCODES_IGNORE);
	MPI_Intercomm_merge(jugadores_comm, 0, &global_comm);
	Get_MPI_Tipo_Jugada(&mpi_tipo_jugada);
	MPI_Bcast(jugadas, NUM_MANOS, mpi_tipo_jugada, MPI_ROOT, jugadores_comm);
	Send_cartas(mazo, 40, 0, jugadores_comm);
	free_cartas(&mazo, 40);

	// El proceso se queda a la espera de recibir un mensaje que indique qué
	// operación realizar
	while (jugando) {
		MPI_Recv(&op, 1, MPI_INT, MPI_ANY_SOURCE, 0, global_comm, &status);
		if (op == 0) {
			// Se quiere hacer una espera (pausa hasta pulsar la tecla [INTRO]
			MPI_Recv(&jugando, 1, MPI_INT, MPI_ANY_SOURCE, 0, global_comm, &status);
			fflush(NULL);
			getchar();
			MPI_Send(&jugando, 1, MPI_INT, status.MPI_SOURCE, 0, global_comm);
		}
		else {
			// Se requiere intervención del jugador humano para alguna operación
			switch (op) {
			case 1:
				op_quiero_mus(global_comm, status.MPI_SOURCE);
				break;
			case 2:
				op_get_descartes(global_comm, status.MPI_SOURCE);
				break;
			case 3:
				op_envidar(global_comm, status.MPI_SOURCE);
				break;
			default:
				break;
			}
		}
	}

	free(args[0]); free(args[1]); free(args[2]); free(args);

	MPI_Type_free(&mpi_tipo_jugada);
	MPI_Comm_disconnect(&jugadores_comm);
	MPI_Finalize();

	return 0;
}

/**
 * Función: op_quiero_mus
 * ----------------------
 * Recoge la entrada del usuario y envía un mensaje al proceso esclavo
 * con la respuesta. Esta respuesta será un 0 si el usuario no quiere
 * mus (pulsa la tecla [n]) o 1 en caso contrario.
 *
 * global_comm: comunicador global que incluye a los procesos esclavos
 * rank_jug:    rank del proceso esclavo que controla el usuario
 */
static void op_quiero_mus(MPI_Comm global_comm, int rank_jug)
{
	int resp;
	char *in = malloc(sizeof(char)*3);
	if (fgets(in, 3, stdin) != NULL) {
		if (strcmp(in, "n\n") == 0) resp = 0;
		else resp = 1;
		MPI_Send(&resp, 1, MPI_INT, rank_jug, 0, global_comm);
	}
	free(in);
}

/**
 * Función: op_get_descartes
 * -------------------------
 * Recoge la entrada del usuario y envía un mensaje al proceso esclavo
 * con la respuesta. La entrada estará formada un máximo de cuatro enteros
 * separados con comas. Estos enteros se usan para generar un array
 * también de 4 enteros que contendrá un 1 en la posición i si la carta
 * número i de la mano del jugador debe descartarse.
 *
 * global_comm: comunicador global que incluye a los procesos esclavos
 * rank_jug:    rank del proceso esclavo que controla el usuario
 */
static void op_get_descartes(MPI_Comm global_comm, int rank_jug)
{
	char in[8]; // máx 1,2,3,4\n
	int i = 0, descartes[4] = { 0, 0, 0, 0 };
	if (fgets(in, 8, stdin) != NULL) {
		char *i_carta;
		i_carta = strtok(in, ","); // índice de la carta en la mano (1, 2, 3 o 4)
		while (i_carta != NULL) {
			i = strtol(i_carta, NULL, 10);
			if (i > 0 && i < 5) descartes[i-1] = 1;
			i_carta = strtok(NULL, ",");
		}
		MPI_Send(&descartes, 4, MPI_INT, rank_jug, 0, global_comm);
	}
}

/**
 * Función: op_envidar
 * -------------------
 * Recoge la entrada del usuario y envía un mensaje al proceso esclavo
 * con esta entrada. Simplemente transforma un caracter a entero y lo
 * envía.
 *
 * global_comm: comunicador global que incluye a los procesos esclavos
 * rank_jug:    rank del proceso esclavo que controla el usuario
 */
static void op_envidar(MPI_Comm global_comm, int rank_jug)
{
	char *in = malloc(sizeof(char)*4);
	if (fgets(in, 4, stdin) != NULL) {
		int resp = strtol(in, NULL, 10);
		MPI_Send(&resp, 1, MPI_INT, rank_jug, 0, global_comm);
	}
	free(in);
}
