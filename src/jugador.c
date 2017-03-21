#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> /* clock_gettime() */
#include <sys/types.h> /* getpid() */
#include <unistd.h> /* getpid() */
#include "mpi.h"
#include "probsmus.h"
#include "juegomus.h"
#include "interactivo.h"
#include <sys/time.h>

// Cantidad de elementos en el testigo (array con información del estado actual del juego)
#define ITEMS_TESTIGO 12
// Índices del array testigo, el cual indicará, en cada uno de sus elementos:
#define POSTRE 0		// Indica quién es el jugador postre
#define FASE 1			// Indica en qué fase de la partida nos encontramos
#define JUGANDO 2		// Indica si la partida está en curso
#define PIEDRAS_A 3		// Indica las piedras de la pareja A (Jugadores 0 y 2)
#define PIEDRAS_B 4		// Indica las piedras de la pareja B (Jugadores 1 y 3)
#define AMARRAKOS_A 5	// Indica los amarrakos de la pareja A
#define AMARRAKOS_B 6	// Indica los amarrakos de la pareja B
#define VACAS_A 7		// Indica las vacas de la pareja A
#define VACAS_B 8		// Indica las vacas de la pareja B
#define N_TANTOS 9		// Indica los tantos necesarios para conseguir una vaca
#define N_VACAS 10		// Indica las vacas necesarias para ganar la partida
#define INTER 11		// Indica si estamos en modo interactivo o automático

// Define la personalidad de un jugador
struct personalidad {
	double umbral_grande;
	double umbral_chica;
	double umbral_pares_duples;
	double umbral_pares_medias;
	double umbral_juego;
	double umbral_punto;
	char *tipo;
};
typedef struct personalidad Personalidad;

static void buscar_indice_mano(Carta mi_mano[], Jugada jugadas[], int *ind_mano);
static int quiero_mus(Jugada jugadas[], int ind_mano, int testigo[], Carta mi_mano[], MPI_Comm global_comm);
static void envidar_grande(Envite *envites_G, Jugada jugada, Carta *mi_mano, int *testigo, Personalidad perso, MPI_Comm global_comm);
static void envidar_chica(Envite *envites_C, Jugada jugada, Carta *mi_mano, int *testigo, Personalidad perso, MPI_Comm global_comm);
static void envidar_pares(Envite *envites_P, Jugada jugada, Carta *mi_mano, int *testigo, Personalidad perso, MPI_Comm global_comm);
static void envidar_juego(Envite *envites_J, Jugada jugada, Carta *mi_mano, int *testigo, Personalidad perso, MPI_Comm global_comm);
static void envidar_punto(Envite *envites_Pt, Jugada jugada, Carta *mi_mano, int *testigo, Personalidad perso, MPI_Comm global_comm);
static void comprueba_dejes(Envite envites, int *testigo);
static void comprueba_ordago(Envite envites, int *testigo, Jugada jugadas[], int ind_mano, Carta mi_mano[]);
static int comprueba_tanteo(int *testigo);
static int puedo_apostar(Envite envites, int *testigo);
static void sumar_tantos(int myrank, int ganador, int *testigo, int tantos);
static void dar_personalidad(Personalidad *perso, int interactivo);
static int posicion(int myrank, int rankmano);
static int tanteo_contrario(int myrank, int testigo[]);
static int tanteo_propio(int myrank, int testigo[]);
static void Get_MPI_Tipo_Envite(MPI_Datatype *mpi_tipo_envite);
static void esperar(int myrank, int jugando, MPI_Comm global_comm);
static void print_fase(int fase, int myrank);

int main(int argc, char *argv[])
{
	if (argc < 4) {
		printf("Error: se necesitan tres argumentos (tantos, vacas y manual/automático).\n");
		exit(EXIT_FAILURE);
	}

	setvbuf( stdout, NULL, _IONBF, 0 );
	int i, ronda = 0, anuncio_mus = 0, myrank, size;
	int n_cartas_mazo, total_descartes = 0, ind_mano = 0;
	Jugada jugadas[NUM_MANOS];
	Carta *mazo, *descartadas, *mi_mano;
	Personalidad perso;
	MPI_Status status;
	MPI_Comm manager_comm, jugadores_comm, global_comm;
	MPI_Datatype mpi_tipo_jugada, mpi_tipo_envite;
	// Array con información del estado actual del juego
	int testigo[ITEMS_TESTIGO] = { 0, 0, 1, 0, 0, 0, 0, 0, 0, strtol(argv[2], NULL, 10), strtol(argv[1], NULL, 10), strtol(argv[3], NULL, 10) };

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_dup(MPI_COMM_WORLD, &jugadores_comm);
	MPI_Comm_get_parent(&manager_comm);
	if (manager_comm == MPI_COMM_NULL) {
		fprintf(stderr, "Error: no se encuentra el proceso manager\n");
		exit(1);
		MPI_Finalize();
	}
	MPI_Intercomm_merge(manager_comm, 1, &global_comm);

	dar_personalidad(&perso, testigo[INTER]);
	Get_MPI_Tipo_Jugada(&mpi_tipo_jugada);
	Get_MPI_Tipo_Envite(&mpi_tipo_envite);

	// Recibimos la matriz de jugadas desde el proceso manager
	MPI_Bcast(jugadas, NUM_MANOS, mpi_tipo_jugada, 0, manager_comm);

	// Jugador 0 recibe el mazo de cartas y saca una carta al azar para decidir quién será el primer postre
	if (myrank == 0) {
		MPI_Probe(0, 0, manager_comm, &status);
		MPI_Get_count(&status, MPI_CHAR, &n_cartas_mazo);
		n_cartas_mazo = n_cartas_mazo / sizeof(Carta);
		mazo = malloc_cartas(n_cartas_mazo);
		Recv_cartas(mazo, n_cartas_mazo, 0, manager_comm);
		printf("\nJugador %d recibe el mazo de cartas.\n", myrank);
		barajar(mazo, 40);
		printf("Jugador %d saca el %s, el repatidor inicial será el Jugador %d.\n", myrank, carta2Lchar(mazo[0]), (*mazo[0].palo + 1) % 4);
		testigo[POSTRE] = (*mazo[0].palo + 1) % 4;
		Send_cartas(mazo, n_cartas_mazo, testigo[POSTRE], MPI_COMM_WORLD);
		free_cartas(&mazo, n_cartas_mazo);
	}
	MPI_Bcast(testigo, ITEMS_TESTIGO, MPI_INT, 0, MPI_COMM_WORLD);

	// El postre recibe el mazo del Jugador 0 y comienza la partida
	if (myrank == testigo[POSTRE]) {
		MPI_Probe(0, 0, MPI_COMM_WORLD, &status);
		MPI_Get_count(&status, MPI_CHAR, &n_cartas_mazo);
		n_cartas_mazo = n_cartas_mazo / sizeof(Carta);
		mazo = malloc_cartas(n_cartas_mazo);
		Recv_cartas(mazo, n_cartas_mazo, 0, MPI_COMM_WORLD);
		printf("Jugador %d recibe el mazo de cartas.\n", myrank);
		printf("\n*** COMIENZA LA PARTIDA ***\n\n");
	}

	/*
	 * BUCLE PRINCIPAL DEL JUEGO
	 */
	while (testigo[JUGANDO]) {
		/*
		 * Reparto inicial de cartas
		 */
		if (testigo[FASE] == 0)	esperar(myrank, testigo[JUGANDO], global_comm);
		if (testigo[FASE] == 0) {
			if (myrank == testigo[POSTRE]) barajar(mazo, 40);
			cortar(&mazo, 40, testigo[POSTRE]);
			reparto_inicial(&mazo, &n_cartas_mazo, testigo[POSTRE], &mi_mano);
			buscar_indice_mano(mi_mano, jugadas, &ind_mano);
			testigo[FASE] = 1;
		}

		/*
		 * Mus corrido/Mus o no Mus
		 */
		if (testigo[FASE] == 1)	esperar(myrank, testigo[JUGANDO], global_comm);
		i = 0;
		while (testigo[FASE] == 1) {
			if (i == 0) print_fase(testigo[FASE], myrank);
			if (i == 4) { // Los 4 Jugadores quieren mus, nos vamos a descartes
				testigo[FASE] = 2;
				break;
			}
			if (ronda == 0) {
				// Mus corrido
				if (myrank == (testigo[POSTRE] + 1) % 4) {
					// Soy mano
					anuncio_mus = quiero_mus(jugadas, ind_mano, testigo, mi_mano, global_comm);
					if (anuncio_mus) printf("Jugador %d: \"Mus\".\n", myrank);
					else printf("Jugador %d: \"No quiero mus\".\n", myrank);
				}
				MPI_Bcast(&anuncio_mus, 1, MPI_INT, (testigo[POSTRE] + 1) % 4, MPI_COMM_WORLD);
				if (anuncio_mus) {
					pasar_mazo(&mazo, &n_cartas_mazo, testigo[POSTRE]);
					pasar_mazo(&descartadas, &total_descartes, testigo[POSTRE]);
					testigo[POSTRE] = (testigo[POSTRE] + 1) % 4;
				}
				else if (!anuncio_mus) testigo[FASE] = 3;
			}
			else {
				// Mus o no mus
				if (myrank == (testigo[POSTRE] + i + 1) % 4) {
					// Me toca anunciar
					anuncio_mus = quiero_mus(jugadas, ind_mano, testigo, mi_mano, global_comm);
					if (anuncio_mus) printf("Jugador %d: \"Mus\".\n", myrank);
					else printf("Jugador %d: \"No quiero mus\".\n", myrank);
				}
				MPI_Bcast(&anuncio_mus, 1, MPI_INT, (testigo[POSTRE] + i + 1) % 4, MPI_COMM_WORLD);
				if (!anuncio_mus) testigo[FASE] = 3;
			}
			i++;
			MPI_Barrier(MPI_COMM_WORLD);
		}

		/*
		 * Descartes
		 */
		i = 0;
		while (testigo[FASE] == 2) {
			if (i == 0)	print_fase(testigo[FASE], myrank);
			MPI_Barrier(MPI_COMM_WORLD); // stdout ordenada
			descartar(testigo[POSTRE], testigo[INTER], &mi_mano, jugadas, &mazo, &n_cartas_mazo, &descartadas, &total_descartes, global_comm);
			buscar_indice_mano(mi_mano, jugadas, &ind_mano);
			testigo[FASE] = 1; // Tras realizar los descartes se vuelve a la fase de Mus o No Mus
		}

		/*
		 * Lance de Grandes
		 */
		if (testigo[FASE] == 3)	esperar(myrank, testigo[JUGANDO], global_comm);
		i = 0; // Ronda de envites
		Envite envites_G = { {0, 0}, -1, nulo };
		while (testigo[FASE] == 3) {
			if (i == 0)	print_fase(testigo[FASE], myrank);
			if (myrank == (testigo[POSTRE] + 1) % 4 && i == 0) {
				// Mano en ronda 0 siempre puede envidar
				envidar_grande(&envites_G, jugadas[ind_mano], mi_mano, testigo, perso, global_comm);
				MPI_Send(&envites_G, 1, mpi_tipo_envite, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
			}
			MPI_Recv(&envites_G, 1, mpi_tipo_envite, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
			if (puedo_apostar(envites_G, testigo))
				envidar_grande(&envites_G, jugadas[ind_mano], mi_mano, testigo, perso, global_comm);
			if (envites_G.estado != cerrado || (envites_G.estado == cerrado && myrank != (envites_G.ultimo + 3) % 4))
				MPI_Send(&envites_G, 1, mpi_tipo_envite, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
			if (envites_G.estado == cerrado)
				testigo[FASE] = 4;
			i++;
		}
		if (myrank == testigo[POSTRE] && testigo[FASE] == 4) {
			printf("\nEnvites en el Lance: Pareja A = %d - Pareja B = %d\n\n", envites_G.envites[0], envites_G.envites[1]);
		}
		// Comprobar dejes u órdago
		comprueba_dejes(envites_G, testigo);
		comprueba_ordago(envites_G, testigo, jugadas, ind_mano, mi_mano);

		/**
		 * Lance de Chicas
		 */
		if (testigo[FASE] == 4)	esperar(myrank, testigo[JUGANDO], global_comm);
		i = 0; // Ronda de envites
		Envite envites_C = { {0, 0}, -1, nulo };
		while (testigo[FASE] == 4) {
			if (i == 0)	print_fase(testigo[FASE], myrank);
			if (myrank == (testigo[POSTRE] + 1) % 4 && i == 0) {
				// Mano en ronda 0 siempre puede envidar
				envidar_chica(&envites_C, jugadas[ind_mano], mi_mano, testigo, perso, global_comm);
				MPI_Send(&envites_C, 1, mpi_tipo_envite, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
			}
			MPI_Recv(&envites_C, 1, mpi_tipo_envite, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
			if (puedo_apostar(envites_C, testigo))
				envidar_chica(&envites_C, jugadas[ind_mano], mi_mano, testigo, perso, global_comm);
			if (envites_C.estado != cerrado || (envites_C.estado == cerrado && myrank != (envites_C.ultimo + 3) % 4))
				MPI_Send(&envites_C, 1, mpi_tipo_envite, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
			if (envites_C.estado == cerrado)
				testigo[FASE] = 5;
			i++;
		}
		if (myrank == testigo[POSTRE] && testigo[FASE] == 5) {
			printf("\nEnvites en el Lance: Pareja A = %d - Pareja B = %d\n\n", envites_C.envites[0], envites_C.envites[1]);
		}
		// Comprobar dejes u órdago
		comprueba_dejes(envites_C, testigo);
		comprueba_ordago(envites_C, testigo, jugadas, ind_mano, mi_mano);

		/**
		 * Lance Pares
		 */
		if (testigo[FASE] == 5)	esperar(myrank, testigo[JUGANDO], global_comm);
		i = 0; // Ronda de envites
		Envite envites_P = { {0, 0}, -1, nulo };
		int anunciado = 0; // Indica si un Jugador ha anunciado ya o todavía no si tiene o no tiene Pares
		int anuncio_pares[2] = {0, 0}; // Indica si algún jugador de cada pareja tiene o no tiene Pares
		while (testigo[FASE] == 5) {
			if (i == 0)	print_fase(testigo[FASE], myrank);
			// Anuncios
			if (anunciado == 0) {
				if (myrank == (testigo[POSTRE] + 1) % 4) {
					if (jugadas[ind_mano].pares == true) {
						anuncio_pares[myrank % 2] = 1;
						printf("Jugador %d: llevo pares\n", myrank);
					}
					else printf("Jugador %d: no llevo\n", myrank);
					MPI_Send(anuncio_pares, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
					MPI_Recv(&anuncio_pares, 2, MPI_INT, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
					anunciado = 1;
				}
				else {
					MPI_Recv(&anuncio_pares, 2, MPI_INT, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
					if (jugadas[ind_mano].pares == true) {
						anuncio_pares[myrank % 2] = 1;
						printf("Jugador %d: llevo pares\n", myrank);
					}
					else printf("Jugador %d: no llevo\n", myrank);
					MPI_Send(anuncio_pares, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
					anunciado = 1;
				}
				MPI_Bcast(&anuncio_pares, 2, MPI_INT, testigo[POSTRE], MPI_COMM_WORLD);
			}
			if (anuncio_pares[0] == 0 || anuncio_pares[1] == 0) {
				// Alguna de las parejas no tiene jugada de Pares, no se juega el lance
				testigo[FASE] = 6;
				break;
			}
			else {
				if (myrank == (testigo[POSTRE] + 1) % 4 && i == 0) {
					// Mano en ronda 0 siempre puede envidar
					envidar_pares(&envites_P, jugadas[ind_mano], mi_mano, testigo, perso, global_comm);
					MPI_Send(&envites_P, 1, mpi_tipo_envite, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
				}
				MPI_Recv(&envites_P, 1, mpi_tipo_envite, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
				if (puedo_apostar(envites_P, testigo))
					envidar_pares(&envites_P, jugadas[ind_mano], mi_mano, testigo, perso, global_comm);
				if (envites_P.estado != cerrado || (envites_P.estado == cerrado && myrank != (envites_P.ultimo + 3) % 4))
					MPI_Send(&envites_P, 1, mpi_tipo_envite, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
				if (envites_P.estado == cerrado)
					testigo[FASE] = 6;
				i++;
			}
		}
		if (myrank == testigo[POSTRE] && testigo[FASE] == 6) {
			printf("\nEnvites en el Lance: Pareja A = %d - Pareja B = %d\n\n", envites_P.envites[0], envites_P.envites[1]);
		}
		// Comprobar dejes u órdago
		comprueba_dejes(envites_P, testigo);
		comprueba_ordago(envites_P, testigo, jugadas, ind_mano, mi_mano);

		/**
		 * Lance Juego
		 */
		if (testigo[FASE] == 6)	esperar(myrank, testigo[JUGANDO], global_comm);
		i = 0; // Ronda de envites
		Envite envites_J = { {0, 0}, -1, nulo };
		anunciado = 0; // Indica si un Jugador ha anunciado ya o todavía no si tiene o no tiene Juego
		int anuncio_juego[2] = {0, 0}; // Indica si algún jugador de cada pareja tiene o no tiene Juego
		while (testigo[FASE] == 6) {
			if (i == 0)	print_fase(testigo[FASE], myrank);
			// Anuncios
			if (anunciado == 0) {
				if (myrank == (testigo[POSTRE] + 1) % 4) {
					if (jugadas[ind_mano].juego == true) {
						anuncio_juego[myrank % 2] = 1;
						printf("Jugador %d: llevo juego\n", myrank);
					}
					else printf("Jugador %d: no llevo\n", myrank);
					MPI_Send(anuncio_juego, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
					MPI_Recv(&anuncio_juego, 2, MPI_INT, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
					anunciado = 1;
				}
				else {
					MPI_Recv(&anuncio_juego, 2, MPI_INT, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
					if (jugadas[ind_mano].juego == true) {
						anuncio_juego[myrank % 2] = 1;
						printf("Jugador %d: llevo juego\n", myrank);
					}
					else printf("Jugador %d: no llevo\n", myrank);
					MPI_Send(anuncio_juego, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
					anunciado = 1;
				}
				MPI_Bcast(&anuncio_juego, 2, MPI_INT, testigo[POSTRE], MPI_COMM_WORLD);
			}
			if (anuncio_juego[0] == 0 && anuncio_juego[1] == 0) {
				// Nadie tiene juego, hay que jugar al punto
				testigo[FASE] = 7;
				break;
			}
			else if (anuncio_juego[0] == 0 || anuncio_juego[1] == 0) {
				// Alguna de las parejas no tiene juego, no se juega el lance (ni el de punto por tener alguna juego)
				testigo[FASE] = 8;
				break;
			}
			else {
				if (myrank == (testigo[POSTRE] + 1) % 4 && i == 0) {
					// Mano en ronda 0 siempre puede envidar
					envidar_juego(&envites_J, jugadas[ind_mano], mi_mano, testigo, perso, global_comm);
					MPI_Send(&envites_J, 1, mpi_tipo_envite, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
				}
				MPI_Recv(&envites_J, 1, mpi_tipo_envite, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
				if (puedo_apostar(envites_J, testigo))
					envidar_juego(&envites_J, jugadas[ind_mano], mi_mano, testigo, perso, global_comm);
				if (envites_J.estado != cerrado || (envites_J.estado == cerrado && myrank != (envites_J.ultimo + 3) % 4))
					MPI_Send(&envites_J, 1, mpi_tipo_envite, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
				if (envites_J.estado == cerrado)
					testigo[FASE] = 8;
				i++;
			}
		}
		MPI_Barrier(MPI_COMM_WORLD);
		if (myrank == testigo[POSTRE] && testigo[FASE] == 8 && (anuncio_juego[0] == 1 || anuncio_juego[1] == 1)) {
			printf("\nEnvites en el Lance: Pareja A = %d - Pareja B = %d\n\n", envites_J.envites[0], envites_J.envites[1]);
		}
		// Comprobar dejes u órdago
		comprueba_dejes(envites_J, testigo);
		comprueba_ordago(envites_J, testigo, jugadas, ind_mano, mi_mano);

		/**
		 * Lance Punto
		 */
		if (testigo[FASE] == 7)	esperar(myrank, testigo[JUGANDO], global_comm);
		i = 0; // Ronda de envites
		Envite envites_Pt = { {0, 0}, -1, nulo };
		while (testigo[FASE] == 7) {
			if (i == 0)	print_fase(testigo[FASE], myrank);
			if (myrank == (testigo[POSTRE] + 1) % 4 && i == 0) {
				// Mano en ronda 0 siempre puede envidar
				envidar_punto(&envites_Pt, jugadas[ind_mano], mi_mano, testigo, perso, global_comm);
				MPI_Send(&envites_Pt, 1, mpi_tipo_envite, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
			}
			MPI_Recv(&envites_Pt, 1, mpi_tipo_envite, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
			if (puedo_apostar(envites_Pt, testigo))
				envidar_punto(&envites_Pt, jugadas[ind_mano], mi_mano, testigo, perso, global_comm);
			if (envites_Pt.estado != cerrado || (envites_Pt.estado == cerrado && myrank != (envites_Pt.ultimo + 3) % 4))
				MPI_Send(&envites_Pt, 1, mpi_tipo_envite, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
			if (envites_Pt.estado == cerrado)
				testigo[FASE] = 8;
			i++;
		}
		MPI_Barrier(MPI_COMM_WORLD);
		if (myrank == testigo[POSTRE] && testigo[FASE] == 8 && anuncio_juego[0] == 0 && anuncio_juego[1] == 0) {
			printf("\nEnvites en el Lance: Pareja A = %d - Pareja B = %d\n\n", envites_Pt.envites[0], envites_Pt.envites[1]);
		}
		// Comprobar dejes u órdago
		comprueba_dejes(envites_Pt, testigo);
		comprueba_ordago(envites_Pt, testigo, jugadas, ind_mano, mi_mano);

		/**
		 * Tanteo
		 */
		while (testigo[FASE] == 8) {
			esperar(myrank, testigo[JUGANDO], global_comm);
			int ganador;
			if (myrank == testigo[POSTRE]) printf("\n*** Tanteo ***\n\n");
			MPI_Barrier(MPI_COMM_WORLD);
			print_manos(myrank, testigo[POSTRE], jugadas[ind_mano].cartas, mi_mano);

			// Lance de Grandes
			MPI_Barrier(MPI_COMM_WORLD);
			ganador = ganador_grande(myrank, testigo[POSTRE], jugadas, ind_mano, mi_mano);
			if (envites_G.envites[0] == envites_G.envites[1]) {
				if (myrank == ganador) printf("\nTantos de Grandes:\n   - Jugador %d gana en el lance de Grandes.\n", ganador);
				if (envites_G.envites[0] != 0) {
					if (myrank == ganador % 2) printf("   - Jugador %d: nos llevamos %d.\n", myrank, envites_G.envites[0]);
					sumar_tantos(myrank, ganador, testigo, envites_G.envites[0]);
				}
				else {
					if (myrank == ganador % 2) printf("   - Jugador %d: nos llevamos 1.\n", myrank);
					sumar_tantos(myrank, ganador, testigo, 1);
				}
			}
			if (comprueba_tanteo(testigo)) break;

			// Lance de Chicas
			MPI_Barrier(MPI_COMM_WORLD);
			ganador = ganador_chica(myrank, testigo[POSTRE], jugadas, ind_mano, mi_mano);
			if (envites_C.envites[0] == envites_C.envites[1]) {
				if (myrank == ganador) printf("\nTantos de Chicas:\n   - Jugador %d gana en el lance de Chicas.\n", ganador);
				if (envites_C.envites[0] != 0) {
					if (myrank == ganador % 2) printf("   - Jugador %d: nos llevamos %d.\n", myrank, envites_C.envites[0]);
					sumar_tantos(myrank, ganador, testigo, envites_C.envites[0]);
				}
				else {
					if (myrank == ganador % 2) printf("   - Jugador %d: nos llevamos 1.\n", myrank);
					sumar_tantos(myrank, ganador, testigo, 1);
				}
			}
			if (comprueba_tanteo(testigo)) break;

			// Lance de Pares
			MPI_Barrier(MPI_COMM_WORLD);
			if (anuncio_pares[0] == 1 || anuncio_pares[1] == 1) {
				if (myrank == 0) printf("\nTantos de Pares:\n");
				int jug_pares[4];
				int t = (int) jugadas[ind_mano].tipo_pares;
				MPI_Allgather(&t, 1, MPI_INT, jug_pares, 1, MPI_INT, MPI_COMM_WORLD);

				if (envites_P.envites[0] > envites_P.envites[1]) {
					// Gana Pareja A (Jugadores 0 y 2)
					if (myrank % 2 == 0 && jugadas[ind_mano].pares == true) {
						printf("   - Jugador %d: nos llevamos %d (tengo %s).\n", myrank, jug_pares[myrank], pares2char(jugadas[ind_mano].tipo_pares));
					}
					sumar_tantos(myrank, 0, testigo, jug_pares[0] + jug_pares[2]);
				}
				else if (envites_P.envites[1] > envites_P.envites[0]) {
					// Gana Pareja B (Jugadores 1 y 3)
					if (myrank % 2 == 1 && jugadas[ind_mano].pares == true) {
						printf("   - Jugador %d: nos llevamos %d (tengo %s).\n", myrank, jug_pares[myrank], pares2char(jugadas[ind_mano].tipo_pares));
					}
					sumar_tantos(myrank, 1, testigo, jug_pares[1] + jug_pares[3]);
				}
				else {
					// Empate en los envites
					ganador = ganador_pares(myrank, testigo[POSTRE], jugadas, ind_mano, mi_mano);
					if (myrank == ganador) printf("   - Jugador %d gana en el lance de Pares.\n", ganador);
					if (myrank % 2 == ganador % 2 && jugadas[ind_mano].pares == true)
						printf("   - Jugador %d: nos llevamos %d (tengo %s).\n", myrank, jug_pares[myrank], pares2char(jugadas[ind_mano].tipo_pares));
					sumar_tantos(myrank, ganador, testigo, jug_pares[ganador % 2] + jug_pares[(ganador % 2) + 2]);
					if (envites_P.envites[0] != 0) {
						if (myrank == ganador % 2) printf("   - Jugador %d: nos llevamos %d (envite).\n", myrank, envites_P.envites[0]);
						sumar_tantos(myrank, ganador, testigo, envites_P.envites[0]);
					}
				}
			}
			if (comprueba_tanteo(testigo)) break;

			// Lance de Juego/Punto
			MPI_Barrier(MPI_COMM_WORLD);
			if (anuncio_juego[0] == 1 || anuncio_juego[1] == 1) {
				// Juego
				if (myrank == 0) printf("\nTantos de Juego:\n");
				int jug_juego[4];
				int t;
				if (jugadas[ind_mano].valor_juego == 31)
					t = 3;
				else if (jugadas[ind_mano].valor_juego != 31 && jugadas[ind_mano].juego == true)
					t = 2;
				else
					t = 0;
				MPI_Allgather(&t, 1, MPI_INT, jug_juego, 1, MPI_INT, MPI_COMM_WORLD);
				if (envites_J.envites[0] > envites_J.envites[1]) {
					// Gana pareja A (Jugadores 0 y 2)
					if (myrank % 2 == 0 && jug_juego[myrank] != 0)
						printf("   - Jugador %d: nos llevamos %d (tengo %d)\n", myrank, jug_juego[myrank], jugadas[ind_mano].valor_juego);
					sumar_tantos(myrank, 0, testigo, jug_juego[0] + jug_juego[2]);
				}
				else if (envites_J.envites[1] > envites_J.envites[0]) {
					// Gana pareja B (Jugadores 1 y 3)
					if (myrank % 2 == 1 && jug_juego[myrank] != 0)
						printf("   - Jugador %d: nos llevamos %d (tengo %d)\n", myrank, jug_juego[myrank], jugadas[ind_mano].valor_juego);
					sumar_tantos(myrank, 1, testigo, jug_juego[1] + jug_juego[3]);
				}
				else {
					// Empate en los envites
					ganador = ganador_juego(myrank, testigo[POSTRE], jugadas, ind_mano, mi_mano);
					if (myrank == ganador) printf("   - Jugador %d gana el lance de Juego.\n", ganador);
					if (myrank % 2 == ganador % 2 && jugadas[ind_mano].juego == true)
						printf("   - Jugador %d: nos llevamos %d (tengo %d)\n", myrank, jug_juego[myrank], jugadas[ind_mano].valor_juego);
					sumar_tantos(myrank, ganador, testigo, jug_juego[ganador % 2] + jug_juego[(ganador % 2) + 2]);
					if (envites_J.envites[0] != 0) {
						if (myrank == ganador % 2) printf("   - Jugador %d: nos llevamos %d (envite).\n", myrank, envites_J.envites[0]);
						sumar_tantos(myrank, ganador, testigo, envites_J.envites[0]);
					}
				}
			}
			else {
				// Punto
				MPI_Barrier(MPI_COMM_WORLD);
				if (envites_Pt.envites[0] == envites_Pt.envites[1]) {
					// Empate en los envites
					ganador = ganador_punto(myrank, testigo[POSTRE], jugadas, ind_mano, mi_mano);
					if (myrank == ganador) printf("\nTantos de Punto:\n   - Jugador %d gana en el lance de Punto.\n", ganador);
					if (envites_Pt.envites[0] != 0 && myrank == ganador % 2)
						printf("   - Jugador %d: nos llevamos %d (envites).\n", myrank, envites_Pt.envites[0]);
					if (myrank == ganador % 2)
						printf("   - Jugador %d: nos llevamos 1 (por Punto).\n", myrank);
					sumar_tantos(myrank, ganador, testigo, envites_Pt.envites[0] + 1);
				}
			}
			if (comprueba_tanteo(testigo)) break;
			testigo[FASE] = 9;
		}

		/**
		 * Nueva ronda
		 */
		if (testigo[FASE] == 9) {
			esperar(myrank, testigo[JUGANDO], global_comm);
			if (myrank == testigo[POSTRE]) {
				printf("\n\n *******************\n *** Nueva Ronda ***\n *******************\n");
				printf("\n   Pareja A: %d tantos (%d piedras y %d amarrakos) \n", testigo[PIEDRAS_A] + 5*testigo[AMARRAKOS_A], testigo[PIEDRAS_A], testigo[AMARRAKOS_A]);
				printf("   Pareja B: %d tantos (%d piedras y %d amarrakos)\n", testigo[PIEDRAS_B] + 5*testigo[AMARRAKOS_B], testigo[PIEDRAS_B], testigo[AMARRAKOS_B]);
				printf("   Pareja A: %d vacas\n", testigo[VACAS_A]);
				printf("   Pareja B: %d vacas\n\n", testigo[VACAS_B]);
			}
			recoger_cartas(testigo[POSTRE], &mi_mano, &mazo, &n_cartas_mazo, &descartadas, &total_descartes);
			pasar_mazo(&mazo, &n_cartas_mazo, testigo[POSTRE]);
			testigo[POSTRE] = (testigo[POSTRE] + 1) % 4;
			ronda++;
			testigo[FASE] = 0;
		}

		/**
		 * Fin de partida
		 */
		if (testigo[FASE] == 99)
			esperar(myrank, testigo[JUGANDO], global_comm);

	} // Fín bucle principal de juego

	MPI_Barrier(MPI_COMM_WORLD);
	// Mostramos la personalidad de cada jugador, por curiosidad
	printf(" - Personalidad Jugador %d: %s\n", myrank, perso.tipo);

	// Finalizamos la ejecución
	MPI_Type_free(&mpi_tipo_jugada);
	MPI_Comm_disconnect(&manager_comm);
    MPI_Finalize();
	return 0;
}

/**
 * Función: buscar_indice_mano
 * ---------------------------
 * Busca, en la matriz de jugadas, la mano que tiene un jugador y devuelve su índice
 * en dicha matriz. De este modo no tendremos que buscarla cada vez que haga falta.
 *
 * mi_mano:          array con las cartas del jugador
 * jugadas:          matriz con todas las jugadas posibles y sus probabilidades de
 *                   victoria/valor
 * ind_mano (INOUT): índice de la mano del jugador en la matriz de jugadas
 */
static void buscar_indice_mano(Carta *mi_mano, Jugada jugadas[], int *ind_mano)
{
	*ind_mano = 0;
	char *tmp_mano = malloc(sizeof(char)*5);
	tmp_mano = mano2figuras8R(mi_mano, 4);
	while (strcmp(tmp_mano, jugadas[*ind_mano].cartas) != 0) *ind_mano += 1;
	free(tmp_mano);
}

/**
 * Función: quiero_mus
 * -------------------
 * Políticas/algoritmo para decidir si un jugador quiere mus (descartarse) o no quiere
 * mus. En este aspecto no influye la personalidad del jugador.
 *
 * jugadas:     matriz con todas las jugadas posibles y sus probabilidades de victoria/valor
 * ind_mano:    posición de la mano del jugador en la matriz de jugadas
 * testigo:     array con información sobre el estado actual del juego
 * mi_mano:     array con las cartas del jugador
 * global_comm: intercomunicador que incluye al proceso manager
 *
 * return: 1 si quiere mus, 0 si no quiere mus
 */
static int quiero_mus(Jugada jugadas[], int ind_mano, int testigo[], Carta mi_mano[], MPI_Comm global_comm)
{
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	if (testigo[INTER] && myrank == 0) return I_quiero_mus(mi_mano, global_comm);

	if (tanteo_contrario(myrank, testigo) >= testigo[N_TANTOS] - 5) { // Si contrario a falta de 5
		if(jugadas[ind_mano].juego)
			return 0; // Cortar mus si tengo juego
		else
			return 1;
	}
	else if (myrank == (testigo[POSTRE] + 1) % 4) { // Si soy mano
		if (tanteo_propio(myrank, testigo) >= testigo[N_TANTOS] - 5) { // Si estoy a falta de 5
			if (jugadas[ind_mano].P_gana_grande > 0.9 || jugadas[ind_mano].P_gana_chica > 0.9)
				return 0; // Cortar mus si tengo buenas probabilidades de ganar en grande o chica
			else if ((jugadas[ind_mano].tipo_pares == medias || jugadas[ind_mano].tipo_pares == duples) && jugadas[ind_mano].juego)
				return 0; // Cortar mus si tengo duples o medias junto con juego
			else return 1;
		}
		else {
			if (jugadas[ind_mano].tipo_pares == duples || jugadas[ind_mano].tipo_pares == medias || jugadas[ind_mano].valor_juego == 31)
				return 0; // Cortar mus si tengo duples, medias o la una
			else return 1;
		}
	}
	else if (myrank == (testigo[POSTRE] + 3) % 4) { // Si compañero es mano
		if ((jugadas[ind_mano].tipo_pares == duples || jugadas[ind_mano].tipo_pares == medias) && jugadas[ind_mano].valor_juego == 31)
			return 0; // Cortar mus si tengo duples o medias junto con la una
		else return 1;
	}
	else { // Ni mi compañero ni yo somos mano
		if (jugadas[ind_mano].tipo_pares == duples || jugadas[ind_mano].tipo_pares == medias || jugadas[ind_mano].juego)
			return 0; // Cortar mus si tengo duples, medias o juego
		else return 1;

	}
}

/**
 * Función: puedo_apostar
 * ----------------------
 * Determina si un jugador puede o no puede apostar (envidar) cuando le
 * llegue el turno de hablar.
 *
 * envites: estructura con información acerca del estado de los envites
 *          durante un lance cualquiera
 * testigo: array con información acerca del estado actual del juego
 *
 * return: 1 si el jugador puede apostar, 0 en caso contrario
 */
static int puedo_apostar(Envite envites, int *testigo)
{
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	// Apuestas cerradas
	if (envites.estado == cerrado)
		return 0;
	// Compañero ya tiene el envite mayor (no le subo a mi compañero)
	if (envites.ultimo == (myrank + 2) % 4)
		return 0;
	// He sido el último en envidar y la pareja contrario lo ha visto
	if (envites.estado == visto && envites.ultimo == myrank)
		return 0;
	// Compañero ha visto un órdago (no tiene sentido verlo de nuevo)
	if (envites.estado == visto && envites.envites[(myrank + 2) % 4] == testigo[N_TANTOS])
		return 0;

	return 1;
}

/**
 * Función: dar_ordago
 * -------------------
 * Anunciar un órdago.
 *
 * envites (INOUT): estructura con información acerca del estado de los envites durante
 *                  un lance cualquiera
 * myrank:          rank del jugador
 * tantos:          tantos necesarios para el órdago
 */
static void dar_ordago(Envite *envites, int myrank, int tantos)
{
	if (envites->estado == ordago) {
		envites->estado = cerrado;
		printf("Jugador %d: lo veo!\n", myrank);
	}
	else {
		envites->estado = ordago;
		printf("Jugador %d: órdago!\n", myrank);
	}
	envites->envites[myrank % 2] = tantos;
	envites->ultimo = myrank;
}

/**
 * Función: envidar
 * ----------------
 * Envidar dos tantos más.
 *
 * envites (INOUT): estructura con información acerca del estado de los envites durante
 *                  un lance cualquiera
 * ultimo_envite:   tantos envidados hasta el momento
 * myrank:          rank del jugador
 * tantos_ord:      tantos máximos que se pueden envidar antes de que el envite se
 *                  convierta en un órdago
 */
static void envidar(Envite *envites, int ultimo_envite, int myrank, int tantos_ord)
{
	envites->envites[myrank % 2] = 2 + ultimo_envite;
	if (envites->envites[myrank % 2] >= tantos_ord) {
		dar_ordago(envites, myrank, tantos_ord);
		return;
	}
	envites->estado = envite;
	envites->ultimo = myrank;
	printf("Jugador %d: envido %d\n", myrank, envites->envites[myrank % 2]);
}

/**
 * Función: envidar_fuerte
 * -----------------------
 * Envidar entre 3 y 10 tantos.
 *
 * envites (INOUT): estructura con información acerca del estado de los envites durante
 *                  un lance cualquiera
 * ultimo_envite:   tantos envidados hasta el momento
 * myrank:          rank del jugador
 * tantos_ord:      tantos máximos que se pueden envidar antes de que el envite se
 *                  convierta en un órdago
 */
static void envidar_fuerte(Envite *envites, int ultimo_envite, int myrank, int tantos_ord)
{
	struct timespec ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	srand(ts.tv_nsec * getpid());

	int i = (rand() % 8) + 3;
	envites->envites[myrank % 2] = i + ultimo_envite;
	if (envites->envites[myrank % 2] >= tantos_ord) {
		dar_ordago(envites, myrank, tantos_ord);
		return;
	}
	envites->estado = envite;
	envites->ultimo = myrank;
	printf("Jugador %d: envido %d\n", myrank, envites->envites[myrank % 2]);
}

/**
 * Función: ver_envite
 * -------------------
 * Igualar el envite de la pareja contraria.
 *
 * envites (INOUT): estructura con información acerca del estado de los envites durante
 *                  un lance cualquiera
 * ultimo_envite:   tantos envidados hasta el momento
 * myrank:          rank del jugador
 *
 */
static void ver_envite(Envite *envites, int ultimo_envite, int myrank)
{
	if (envites->estado == visto) {
		// Compañero ya ha visto el envite, cerramos
		envites->estado = cerrado;
		envites->ultimo = myrank;
		printf("Jugador %d: cierro.\n", myrank);
	}
	else {
		// Lo veo
		envites->envites[myrank % 2] = ultimo_envite;
		if (envites->ultimo == (myrank + 1) % 4) {
			envites->estado = cerrado; // Cerrar si soy el anterior al último envite
			envites->ultimo = myrank;
		}
		else envites->estado = visto;
		printf("Jugador %d: lo veo\n", myrank);
	}
}

/**
 * Función: pasar
 * --------------
 * Acción de pasar (no envidar/ver envite).
 *
 * envites (INOUT): estructura con información acerca del estado de los envites durante
 *                  un lance cualquiera
 * myrank:          rank del jugador
 * postre:          rank del jugador postre
 */
static void pasar(Envite *envites, int myrank, int postre)
{
	if ((envites->estado == nulo && myrank == postre) ||
			(envites->estado != nulo && myrank == (envites->ultimo + 3) % 4)) {
		// Si soy postre (el último en apostar) y no ha habido envites, o si soy el
		// jugador anterior al último en envidar, puedo cerrar.
		envites->estado = cerrado;
		envites->ultimo = myrank;
		printf("Jugador %d: cierro.\n", myrank);
	}
	else printf("Jugador %d: paso\n", myrank);
}

/**
 * Función: envidar_grande
 * -----------------------
 * Políticas/algoritmo para envidar en el lance de Grandes.
 *
 * envites_G (INOUT): estructura con información acerca del estado de los envites
 * jugada:            representación de la jugada del jugador (RRRR, CS76...) con sus
 *                    probabilidades de victoria en cada lance
 * mi_mano:			  cartas del jugador
 * testigo:           array con información sobre el estado actual del juego
 * perso:             personalidad del jugador (umbrales de probabilidades mínimas que
 *                    hay que tener para envidar)
 * global_comm:       intercomunicador que incluye al proceso manager
 */
static void envidar_grande(Envite *envites_G, Jugada jugada, Carta *mi_mano, int *testigo, Personalidad perso, MPI_Comm global_comm)
{
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	int MANO = (testigo[POSTRE] + 1) % 4; // Jugador Mano

	// Hack para solucionar la inicialización de envites.ultimo a -1
	int ultimo_envite;
	if (envites_G->ultimo == -1)
		ultimo_envite = 0;
	else
		ultimo_envite = envites_G->envites[envites_G->ultimo % 2];

	if (testigo[INTER] && myrank == 0) {
		I_envidar(envites_G, ultimo_envite, testigo[N_TANTOS], testigo[POSTRE], mi_mano, global_comm);
		return;
	}

	if (jugada.P_gana_grande > perso.umbral_grande) {
		if (ultimo_envite < 10)
			envidar_fuerte(envites_G, ultimo_envite, myrank, testigo[N_TANTOS]);
		else if (ultimo_envite >= 10 && posicion(myrank, MANO) < posicion(envites_G->ultimo, MANO))
			dar_ordago(envites_G, myrank, testigo[N_TANTOS]);
		else
			ver_envite(envites_G, ultimo_envite, myrank);
	}
	else if (jugada.P_gana_grande > perso.umbral_grande - 0.05) {
		if (ultimo_envite < 5)
			envidar(envites_G, ultimo_envite, myrank, testigo[N_TANTOS]);
		else
			ver_envite(envites_G, ultimo_envite, myrank);
	}
	else
		pasar(envites_G, myrank, testigo[POSTRE]);
}

/**
 * Función: envidar_chica
 * ----------------------
 * Políticas/algoritmo para envidar en el lance de Chicas.
 *
 * envites_C (INOUT): estructura con información acerca del estado de los envites
 * jugada:            representación de la jugada del jugador (RRRR, CS76...) con sus
 *                    probabilidades de victoria en cada lance
 * mi_mano:			  cartas del jugador
 * testigo:           array con información sobre el estado actual del juego
 * perso:             personalidad del jugador (umbrales de probabilidades mínimas que
 *                    hay que tener para envidar)
 * global_comm:       intercomunicador que incluye al proceso manager
 */
static void envidar_chica(Envite *envites_C, Jugada jugada, Carta *mi_mano, int *testigo, Personalidad perso, MPI_Comm global_comm)
{
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	int MANO = (testigo[POSTRE] + 1) % 4; // Jugador Mano

	// Hack para solucionar la inicialización de envites.ultimo a -1
	int ultimo_envite;
	if (envites_C->ultimo == -1)
		ultimo_envite = 0;
	else
		ultimo_envite = envites_C->envites[envites_C->ultimo % 2];

	if (testigo[INTER] && myrank == 0) {
		I_envidar(envites_C, ultimo_envite, testigo[N_TANTOS], testigo[POSTRE], mi_mano, global_comm);
		return;
	}

	if (jugada.P_gana_chica > perso.umbral_chica) {
		if (ultimo_envite < 10)
			envidar_fuerte(envites_C, ultimo_envite, myrank, testigo[N_TANTOS]);
		else if (ultimo_envite >= 10 && posicion(myrank, MANO) < posicion(envites_C->ultimo, MANO))
			dar_ordago(envites_C, myrank, testigo[N_TANTOS]);
		else
			ver_envite(envites_C, ultimo_envite, myrank);
	}
	else if (jugada.P_gana_chica > perso.umbral_chica - 0.05) {
		if (ultimo_envite < 5)
			envidar(envites_C, ultimo_envite, myrank, testigo[N_TANTOS]);
		else
			ver_envite(envites_C, ultimo_envite, myrank);
	}
	else
		pasar(envites_C, myrank, testigo[POSTRE]);
}

/**
 * Función: envidar_pares
 * ----------------------
 * Políticas/algoritmo para envidar en el lance de Pares.
 *
 * envites_P (INOUT): estructura con información acerca del estado de los envites
 * jugada:            representación de la jugada del jugador (RRRR, CS76...) con sus
 *                    probabilidades de victoria en cada lance
 * mi_mano:			  cartas del jugador
 * testigo:           array con información sobre el estado actual del juego
 * perso:             personalidad del jugador (umbrales de probabilidades mínimas que
 *                    hay que tener para envidar)
 * global_comm:       intercomunicador que incluye al proceso manager
 */
static void envidar_pares(Envite *envites_P, Jugada jugada, Carta *mi_mano, int *testigo, Personalidad perso, MPI_Comm global_comm)
{
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	int MANO = (testigo[POSTRE] + 1) % 4; // Jugador Mano

	// Hack para solucionar la inicialización de envites.ultimo a -1
	int ultimo_envite;
	if (envites_P->ultimo == -1)
		ultimo_envite = 0;
	else
		ultimo_envite = envites_P->envites[envites_P->ultimo % 2];

	if (testigo[INTER] && myrank == 0) {
		I_envidar(envites_P, ultimo_envite, testigo[N_TANTOS], testigo[POSTRE], mi_mano, global_comm);
		return;
	}

	if (jugada.tipo_pares == duples) {
		if (jugada.P_gana_pares > perso.umbral_pares_duples) {
			if (ultimo_envite < 15)
				envidar(envites_P, ultimo_envite, myrank, testigo[N_TANTOS]);
			else if (ultimo_envite >= 15 && posicion(myrank, MANO) < posicion(envites_P->ultimo, MANO))
				// Envite contrario superior a 15 y soy mano sobre el último en envidar, órdago
				dar_ordago(envites_P, myrank, testigo[N_TANTOS]);
			else
				// Envite contrario superior a 15 y NO soy mano sobre el último en envidar, veo
				ver_envite(envites_P, ultimo_envite, myrank);
		}
		else {
			if (ultimo_envite < 10)
				envidar(envites_P, ultimo_envite, myrank, testigo[N_TANTOS]);
			else if (ultimo_envite >= 10 && posicion(myrank, MANO) < posicion(envites_P->ultimo, MANO))
				// Envite contrario superior a 10 y soy mano sobre el último en envidar, órdago
				dar_ordago(envites_P, myrank, testigo[N_TANTOS]);
			else
				// Envite contrario superior a 10 y NO soy mano sobre el último en envidar, veo
				ver_envite(envites_P, ultimo_envite, myrank);
		}
	}
	else if (jugada.tipo_pares == medias) {
		if (jugada.P_gana_pares > perso.umbral_pares_medias) {
			if (ultimo_envite < 3)
				envidar(envites_P, ultimo_envite, myrank, testigo[N_TANTOS]);
			else if (ultimo_envite >= 3 && posicion(myrank, MANO) < posicion(envites_P->ultimo, MANO))
				// Envite contrario superior a 3 y soy mano sobre el último en envidar, órdago
				dar_ordago(envites_P, myrank, testigo[N_TANTOS]);
			else
				// Envite contrario superior a 3 y NO soy mano sobre el último en envidar, veo
				ver_envite(envites_P, ultimo_envite, myrank);
		}
		else {
			if (ultimo_envite < 3)
				envidar(envites_P, ultimo_envite, myrank, testigo[N_TANTOS]);
			else
				// Envite contrario superior a 3, veo
				ver_envite(envites_P, ultimo_envite, myrank);
		}
	}
	else if (jugada.tipo_pares == parejas) {
		if (jugada.valor_pares == 107) { // Tengo pareja de Reyes
			if (ultimo_envite < 3 && (posicion(myrank, MANO) < posicion(envites_P->ultimo, MANO) || envites_P->ultimo == -1 ))
				envidar(envites_P, ultimo_envite, myrank, testigo[N_TANTOS]);
			else if (ultimo_envite < 3 && posicion(myrank, MANO) > posicion(envites_P->ultimo, MANO))
				ver_envite(envites_P, ultimo_envite, myrank);
			else pasar(envites_P, myrank, testigo[POSTRE]);
		}
		else pasar(envites_P, myrank, testigo[POSTRE]);
	}
	else pasar(envites_P, myrank, testigo[POSTRE]);
}

/**
 * Función: envidar_juego
 * ----------------------
 * Políticas/algoritmo para envidar en el lance de Juego.
 *
 * envites_J (INOUT): estructura con información acerca del estado de los envites
 * jugada:            representación de la jugada del jugador (RRRR, CS76...) con sus
 *                    probabilidades de victoria en cada lance
 * mi_mano:			  cartas del jugador
 * testigo:           array con información sobre el estado actual del juego
 * perso:             personalidad del jugador (umbrales de probabilidades mínimas que
 *                    hay que tener para envidar)
 * global_comm:       intercomunicador que incluye al proceso manager
 */
static void envidar_juego(Envite *envites_J, Jugada jugada, Carta *mi_mano, int *testigo, Personalidad perso, MPI_Comm global_comm)
{
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	int MANO = (testigo[POSTRE] + 1) % 4; // Jugador Mano

	// Hack para solucionar la inicialización de envites.ultimo a -1
	int ultimo_envite;
	if (envites_J->ultimo == -1)
		ultimo_envite = 0;
	else
		ultimo_envite = envites_J->envites[envites_J->ultimo % 2];

	if (testigo[INTER] && myrank == 0) {
		I_envidar(envites_J, ultimo_envite, testigo[N_TANTOS], testigo[POSTRE], mi_mano, global_comm);
		return;
	}

	if (jugada.P_gana_juego > perso.umbral_juego) {
		if (ultimo_envite < 7 && posicion(myrank, MANO) < posicion(envites_J->ultimo, MANO)) // envite < 7 & soy mano sobre el último jugador en envidar
			envidar(envites_J, ultimo_envite, myrank, testigo[N_TANTOS]);
		else if (ultimo_envite >= 7 && posicion(myrank, MANO) < posicion(envites_J->ultimo, MANO))
			dar_ordago(envites_J, myrank, testigo[N_TANTOS]);
		else if (ultimo_envite == 0 && posicion(myrank, MANO) > posicion(envites_J->ultimo, MANO))
			envidar(envites_J, ultimo_envite, myrank, testigo[N_TANTOS]);
		else if (ultimo_envite < 10 && posicion(myrank, MANO) > posicion(envites_J->ultimo, MANO))
			ver_envite(envites_J, ultimo_envite, myrank);
		else pasar(envites_J, myrank, testigo[POSTRE]);
	}
	else pasar(envites_J, myrank, testigo[POSTRE]);
}

/**
 * Función: envidar_punto
 * ----------------------
 * Políticas/algoritmo para envidar en el lance de Punto.
 *
 * envites_Pt (INOUT): estructura con información acerca del estado de los envites
 * jugada:            representación de la jugada del jugador (RRRR, CS76...) con sus
 *                    probabilidades de victoria en cada lance
 * mi_mano:			  cartas del jugador
 * testigo:           array con información sobre el estado actual del juego
 * perso:             personalidad del jugador (umbrales de probabilidades mínimas que
 *                    hay que tener para envidar)
 * global_comm:       intercomunicador que incluye al proceso manager
 */
static void envidar_punto(Envite *envites_Pt, Jugada jugada, Carta *mi_mano, int *testigo, Personalidad perso, MPI_Comm global_comm)
{
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	int MANO = (testigo[POSTRE] + 1) % 4; // Jugador Mano

	// Hack para solucionar la inicialización de envites.ultimo a -1
	int ultimo_envite;
	if (envites_Pt->ultimo == -1)
		ultimo_envite = 0;
	else
		ultimo_envite = envites_Pt->envites[envites_Pt->ultimo % 2];

	if (testigo[INTER] && myrank == 0) {
		I_envidar(envites_Pt, ultimo_envite, testigo[N_TANTOS], testigo[POSTRE], mi_mano, global_comm);
		return;
	}

	if (jugada.P_gana_punto > perso.umbral_punto) {
		if (ultimo_envite < 10)
			envidar(envites_Pt, ultimo_envite, myrank, testigo[N_TANTOS]);
		else if (ultimo_envite >= 10 && posicion(myrank, MANO) < posicion(envites_Pt->ultimo, MANO))
			ver_envite(envites_Pt, ultimo_envite, myrank);
		else if (ultimo_envite == 10 && posicion(myrank, MANO) > posicion(envites_Pt->ultimo, MANO))
			ver_envite(envites_Pt, ultimo_envite, myrank);
		else pasar(envites_Pt, myrank, testigo[POSTRE]);
	}
	else pasar(envites_Pt, myrank, testigo[POSTRE]);
}

/**
 * Función: comprueba_dejes
 * ------------------------
 * Comprueba los posibles dejes en un lance cualquiera y suma los tantos
 * que se ganen por ello.
 *
 * envites:         estructura con información acerca del estado de los envites
 *                  durante un lance cualquiera
 * testigo (INOUT): array con información acerca del estado actual del juego
 */
static void comprueba_dejes(Envite envites, int *testigo)
{
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	MPI_Barrier(MPI_COMM_WORLD);

	// Pareja B (Jugadores 1 y 3) no han visto/superado el envite de algún
	// jugador de la Pareja A (Jugadores 0 y 2)
	if (myrank == 0 && envites.envites[0] > 0 && envites.envites[1] == 0) {
		printf("Jugador 0: nos llevamos 1.\n");
		testigo[PIEDRAS_A]++;
		if (testigo[PIEDRAS_A] == 5) {
			printf("Jugador 0: amarrako.\n");
			testigo[AMARRAKOS_A]++;
			testigo[PIEDRAS_A] = 0;
		}
	}
	else if (myrank == 0 && envites.envites[0] > envites.envites[1] && envites.envites[1] != 0) {
		printf("Jugador 0: nos llevamos %d.\n", envites.envites[1]);
		testigo[PIEDRAS_A] = testigo[PIEDRAS_A] + envites.envites[1];
		if (testigo[PIEDRAS_A] >= 5) {
			printf("Jugador 0: %d amarrako/s\n", testigo[PIEDRAS_A] / 5);
			testigo[AMARRAKOS_A] = testigo[AMARRAKOS_A] + testigo[PIEDRAS_A] / 5;
			testigo[PIEDRAS_A] = testigo[PIEDRAS_A] % 5;
		}
	}
	MPI_Bcast(testigo, ITEMS_TESTIGO, MPI_INT, 0, MPI_COMM_WORLD);

	// Pareja A (Jugadores 0 y 2) no han visto/superado el envite de algún
	// jugador de la Pareja A (Jugadores 1 y 3)
	if (myrank == 1 && envites.envites[1] > 0 && envites.envites[0] == 0) {
		printf("Jugador 1: nos llevamos 1.\n");
		testigo[PIEDRAS_B]++;
		if (testigo[PIEDRAS_B] == 5) {
			printf("Jugador 1: amarrako.\n");
			testigo[AMARRAKOS_B]++;
			testigo[PIEDRAS_B] = 0;
		}
	}
	else if (myrank == 1 && envites.envites[1] > envites.envites[0] && envites.envites[0] != 0) {
		printf("Jugador 1: nos llevamos %d.\n", envites.envites[0]);
		testigo[PIEDRAS_B] = testigo[PIEDRAS_B] + envites.envites[0];
		if (testigo[PIEDRAS_B] >= 5) {
			printf("Jugador 1: %d amarrako/s\n", testigo[PIEDRAS_B] / 5);
			testigo[AMARRAKOS_B] = testigo[AMARRAKOS_B] + testigo[PIEDRAS_B] / 5;
			testigo[PIEDRAS_B] = testigo[PIEDRAS_B] % 5;
		}
	}
	MPI_Bcast(testigo, ITEMS_TESTIGO, MPI_INT, 1, MPI_COMM_WORLD);

	comprueba_tanteo(testigo);
}

/**
 * Función: comprueba_ordago
 * -------------------------
 * Comprueba si se ha lanzado y aceptado un órdago en algún lance cualquiera
 * y determina quién es el ganador para sumar los tantos correspondientes.
 *
 * envites:         estructura con información acerca del estado de los envites
 *                  durante un lance cualquiera
 * testigo (INOUT): array con información acerca del estado actual del juego
 * jugadas:         matriz con todas las jugadas posibles y su valor para cada
 *                  lance
 * ind_mano:        posición de la mano del jugador en la matriz de jugadas
 * mi_mano:         array de cartas del jugador
 */
static void comprueba_ordago(Envite envites, int *testigo, Jugada jugadas[], int ind_mano, Carta mi_mano[])
{
	if (!testigo[JUGANDO]) return;
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	if (envites.envites[0] == testigo[N_TANTOS] && envites.envites[1] == testigo[N_TANTOS]) {
		if (myrank == testigo[POSTRE]) printf("\n*** Se ha lanzado y aceptado un órdago ***\n");
		print_manos(myrank, testigo[POSTRE], jugadas[ind_mano].cartas, mi_mano);
		MPI_Barrier(MPI_COMM_WORLD);
		int ganador = -1; // Inicializar para no enfadar al compilador

		if (testigo[FASE] == 4)
			ganador = ganador_grande(myrank, testigo[POSTRE], jugadas, ind_mano, mi_mano);
		else if (testigo[FASE] == 5)
			ganador = ganador_chica(myrank, testigo[POSTRE], jugadas, ind_mano, mi_mano);
		else if (testigo[FASE] == 6)
			ganador = ganador_pares(myrank, testigo[POSTRE], jugadas, ind_mano, mi_mano);
		else if (testigo[FASE] == 8)
			ganador = ganador_juego(myrank, testigo[POSTRE], jugadas, ind_mano, mi_mano);
		else if (testigo[FASE] == 9)
			ganador = ganador_punto(myrank, testigo[POSTRE], jugadas, ind_mano, mi_mano);

		if (myrank == ganador) printf("\n*** El jugador %d ha ganado el órdago! ***\n", ganador);

		if (ganador % 2 == 0)
			testigo[AMARRAKOS_A] = testigo[N_TANTOS] / 5;
		else
			testigo[AMARRAKOS_B] = testigo[N_TANTOS] / 5;

		comprueba_tanteo(testigo);
	}
}

/**
 * Función: comprueba_tanteo
 * -------------------------
 * Comprueba los tantos que lleva cada pareja para saber si se debe sumar una vaca
 * (y resetear los tantos) o si se han alcanzado las vacas suficientes para ganar
 * la partida (y finalizar la ejecución del programa).
 *
 * testigo (INOUT): array con información sobre el estado actual de la partida
 *
 * return: 1 si se alcanzan los tantos suficientes para sumar una vaca o si se
 *         alcanzan las vacas suficientes para ganar la partida, 0 en caso
 *         contrario
 */
static int comprueba_tanteo(int *testigo)
{
	if (!testigo[JUGANDO]) return 1;
	int vaca = 0, myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	// Alguna pareja ha alcanzado los tantos necesarios para llevarse una vaca
	if (testigo[PIEDRAS_A] + 5 * testigo[AMARRAKOS_A] >= testigo[N_TANTOS]) {
		if (myrank == 0) printf("\n*** Equipo A (Jugadores 0 y 2) han superado los %d tantos y se llevan 1 vaca ***\n", testigo[N_TANTOS]);
		vaca = 1;
		testigo[VACAS_A]++;
	}
	else if (testigo[PIEDRAS_B] + 5 * testigo[AMARRAKOS_B] >= testigo[N_TANTOS]) {
		if (myrank == 1) printf("\n*** Equipo B (Jugadores 1 y 3) han superado los %d tantos y se llevan 1 vaca ***\n", testigo[N_TANTOS]);
		vaca = 1;
		testigo[VACAS_B]++;
	}

	// Alguna pareja ha alcanzado las vacas necesarias para llevarse la partida
	if (testigo[VACAS_A] == testigo[N_VACAS]) {
		if (myrank == 0) printf("\n*** Equipo A (Jugadores 0 y 2) han alcanzado las %d vacas y ganan la partida ***\n", testigo[N_VACAS]);
		vaca = 2;
	}
	else if (testigo[VACAS_B] == testigo[N_VACAS]) {
		if (myrank == 1) printf("\n*** Equipo B (Jugadores 1 y 3) han alcanzado las %d vacas y ganan la partida ***\n", testigo[N_VACAS]);
		vaca = 2;
	}

	if (vaca == 1) {
		testigo[PIEDRAS_A] = 0;
		testigo[PIEDRAS_B] = 0;
		testigo[AMARRAKOS_A] = 0;
		testigo[AMARRAKOS_B] = 0;
		testigo[FASE] = 9;
		return 1;
	}
	else if (vaca == 2) {
		testigo[PIEDRAS_A] = 0;
		testigo[PIEDRAS_B] = 0;
		testigo[AMARRAKOS_A] = 0;
		testigo[AMARRAKOS_B] = 0;
		testigo[FASE] = 99;
		testigo[JUGANDO] = 0;
		return 1;
	}

	return 0;
}

/**
 * Función: sumar_tantos
 * ---------------------
 * Suma los tantos obtenidos por una pareja en un lance cualquiera.
 *
 * myrank:          rank del jugador
 * ganador:         rank del jugador ganador del lance
 * testigo (INOUT): array con información sobre el estado actual del juego
 * tantos:          tantos que se quieren sumar
 */
static void sumar_tantos(int myrank, int ganador, int *testigo, int tantos)
{
	if (ganador % 2 == 0) { // Ganador es jugador de pareja A
		testigo[PIEDRAS_A] = testigo[PIEDRAS_A] + tantos;
		if (testigo[PIEDRAS_A] >= 5) {
			if (myrank == 0) printf("   - Jugador 0: %d amarrako/s\n", testigo[PIEDRAS_A]/5);
			testigo[AMARRAKOS_A] = testigo[AMARRAKOS_A] + testigo[PIEDRAS_A] / 5;
			testigo[PIEDRAS_A] = testigo[PIEDRAS_A] % 5;
		}
	}
	else { // Ganador es jugador de pareja B
		testigo[PIEDRAS_B] = testigo[PIEDRAS_B] + tantos;
		if (testigo[PIEDRAS_B] >= 5) {
			if (myrank == 1) printf("   - Jugador 1: %d amarrako/s\n", testigo[PIEDRAS_B]/5);
			testigo[AMARRAKOS_B] = testigo[AMARRAKOS_B] + testigo[PIEDRAS_B] / 5;
			testigo[PIEDRAS_B] = testigo[PIEDRAS_B] % 5;
		}
	}
}

/**
 * Función: posicion
 * -----------------
 * Determina la posición (distancia) de un jugador respecto al jugador que
 * es mano en ese momento.
 * Por ejemplo, si el Jugador 1 es mano, el Jugador 2 está a una distancia
 * igual a 1, el Jugador 0 está a una distancia igual a 3, etc.
 *
 * myrank:   rank del jugador sobre el que se quiere conocer su posición
 *           (distancia) respecto al jugador mano
 * rankmano: rank del jugador mano
 *
 * return: la posición (distancia) del jugador respecto al jugador mano
 */
static int posicion(int myrank, int rankmano)
{
	// Hack para solucionar el problema de la inicialización de Envite.ultimo a -1
	if (myrank == -1) return 5;

	int posiciones[4] = { rankmano, (rankmano + 1) % 4, (rankmano + 2) % 4, (rankmano + 3) % 4 };
	int i;

	for (i = 0; i < 4; i++)
		if (posiciones[i] == myrank) return i;
	return 0;
}

/**
 * Función: tanteo_contrario
 * -------------------------
 * Calcula y devuelve los tantos de la pareja contraria.
 *
 * myrank:  rank del jugador
 * testigo: array con información acerca del estado actual del juego
 *
 * return: tanteo de la pareja contraria
 */
static int tanteo_contrario(int myrank, int testigo[])
{
	if (myrank == 0 || myrank == 2)
		return (testigo[PIEDRAS_B] + 5*testigo[AMARRAKOS_B]);
	else
		return (testigo[PIEDRAS_A] + 5*testigo[AMARRAKOS_A]);
}

/**
 * Función: tanteo_propio
 * -------------------------
 * Calcula y devuelve mis tantos.
 *
 * myrank:  rank del jugador
 * testigo: array con información acerca del estado actual del juego
 *
 * return: mi tanteo
 */
static int tanteo_propio(int myrank, int testigo[])
{
	if (myrank == 0 || myrank == 2)
		return (testigo[PIEDRAS_A] + 5*testigo[AMARRAKOS_A]);
	else
		return (testigo[PIEDRAS_B] + 5*testigo[AMARRAKOS_B]);
}

/**
 * Función: Get_MPI_Tipo_Envite
 * ----------------------------
 * Crea el tipo MPI necesario para poder enviar/recibir mensajes que incluyan
 * la estructura Envite.
 *
 * mpi_tipo_envite (INOUT): tipo MPI
 */
static void Get_MPI_Tipo_Envite(MPI_Datatype *mpi_tipo_envite)
{
	MPI_Aint offsets[3] = { offsetof(Envite, envites), offsetof(Envite, ultimo), offsetof(Envite, estado) };
	int blocklengths[3] = { sizeof(int) * 4, sizeof(int), sizeof(char) };
	MPI_Datatype tipos_elementos_envite[3] = { MPI_INT, MPI_INT, MPI_INT };
	MPI_Datatype tipo_tmp;
	MPI_Type_create_struct(3, blocklengths, offsets, tipos_elementos_envite, &tipo_tmp);
	MPI_Type_commit(&tipo_tmp);
	*mpi_tipo_envite = tipo_tmp;
	return;
}

/**
 * Función: dar_personalidad
 * -------------------------
 * Elige aleatoriamente una personalidad (umbrales de probabilidad de ganar en
 * cada lance) para cada jugador entre las tres posibles.
 *
 * perso (INOUT): estructura que contiene los umbrales de probabilidad según
 *                la personalidad elegida
 * interactivo:   1 si estamos en modo interactivo (participa jugador humano),
 *                0 en caso contrario
 */
static void dar_personalidad(Personalidad *perso, int interactivo)
{
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	if (interactivo == 1 && myrank == 0) {
		// Jugador humano
		perso->tipo = "Jugador humano";
		return;
	}

	struct timespec ts;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
	srand(ts.tv_nsec * getpid());
	int i = (rand() % 3);

	if (i == 0) {
		// Conservador
		perso->tipo = "Conservadora";
		perso->umbral_grande = 0.95;
		perso->umbral_chica = 0.95;
		perso->umbral_pares_duples = 0.97;
		perso->umbral_pares_medias = 0.9;
		perso->umbral_juego = 0.63; // 31 o 32
		perso->umbral_punto = 0.9;  // >= 28
	}
	else if (i == 1) {
		// Neutral
		perso->tipo = "Neutral";
		perso->umbral_grande = 0.85;
		perso->umbral_chica = 0.85;
		perso->umbral_pares_duples = 0.95;
		perso->umbral_pares_medias = 0.88;
		perso->umbral_juego = 0.50; // 31, 32 o 40
		perso->umbral_punto = 0.8;  // >= 27
	}
	else {
		// Agresivo
		perso->tipo = "Agresiva";
		perso->umbral_grande = 0.75;
		perso->umbral_chica = 0.75;
		perso->umbral_pares_duples = 0.92;
		perso->umbral_pares_medias = 0.86;
		perso->umbral_juego = 0.45; // 31, 32, 40 o 37
		perso->umbral_punto = 0.75; // >= 26
	}
}

/**
 * Función: esperar
 * ----------------
 * Comunicación con el proceso manager para esperar a que se pulse la
 * tecla [INTRO] y continuar con la ejecución del programa.
 *
 * myrank:      rank del jugador
 * jugando:     1 si el juego debe continuar, 0 en caso contrario
 * global_comm: comunicador global (incluye al proceso manager)
 */
static void esperar(int myrank, int jugando, MPI_Comm global_comm)
{
	if (myrank == 0) {
		MPI_Status status;
		int m = 0;
		MPI_Send(&m, 1, MPI_INT, 0, 0, global_comm); // Señalar que queremos hacer una espera
		MPI_Send(&jugando, 1, MPI_INT, 0, 0, global_comm); // Enviar si seguimos jugando o no
		MPI_Recv(&jugando, 1, MPI_INT, 0, 0, global_comm, &status);
	}
	MPI_Barrier(MPI_COMM_WORLD);
}

/**
 * Función: print_fase
 * -------------------
 * Imprime en pantalla la fase actual del juego.
 *
 * fase:   entero que indica la fase actual
 * myrank: rank del jugador
 */
static void print_fase(int fase, int myrank)
{
	if (fase == 1 && myrank == 0) {
		printf("\n +----------------------+\n");
		printf(" | Fase de mus o no mus |\n");
		printf(" +----------------------+\n\n");
	}
	else if (fase == 2 && myrank == 0) {
		printf("\n +-------------------+\n");
		printf(" | Fase de descartes |\n");
		printf(" +-------------------+\n\n");
	}
	else if (fase == 3 && myrank == 0) {
		printf("\n +------------------+\n");
		printf(" | Lance de Grandes |\n");
		printf(" +------------------+\n\n");
	}
	else if (fase == 4 && myrank == 0) {
		printf("\n +-----------------+\n");
		printf(" | Lance de Chicas |\n");
		printf(" +-----------------+\n\n");
	}
	else if (fase == 5 && myrank == 0) {
		printf("\n +----------------+\n");
		printf(" | Lance de Pares |\n");
		printf(" +----------------+\n\n");
	}
	else if (fase == 6 && myrank == 0) {
		printf("\n +----------------+\n");
		printf(" | Lance de Juego |\n");
		printf(" +----------------+\n\n");
	}
	else if (fase == 7 && myrank == 0) {
		printf("\n +----------------+\n");
		printf(" | Lance de Punto |\n");
		printf(" +----------------+\n\n");
	}
	MPI_Barrier(MPI_COMM_WORLD);
}
