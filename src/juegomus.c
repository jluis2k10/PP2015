#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h> // para offsetof
#include <time.h>
#include "mpi.h"
#include "probsmus.h"
#include "juegomus.h"
#include "interactivo.h"

const char *figuras[]   = { "A", "2", "3", "4", "5", "6", "7", "S", "C", "R" };
const char *figuras8R[] = { "A", "A", "R", "4", "5", "6", "7", "S", "C", "R" };
const char *palos[]     = { "O", "C", "E", "B" };
const char *figuras_largo[] = { "As", "Dos", "Tres", "Cuatro", "Cinco", "Seis", "Siete", "Sota", "Caballo", "Rey" };
const char *palos_largo[]   = { "Oros", "Copas", "Espadas", "Bastos" };

static int compara(const void *a, const void *b);
static void juntar_mazos(Carta **mazo, int *n_cartas_mazo, Carta **descartadas, int *total_descartes);

/**
 * Función: generar_mazo
 * ---------------------
 * Genera el mazo de cartas.
 *
 * mazo (INOUT): array de tipo Carta
 */
void generar_mazo(Carta *mazo)
{
	int i, j;
	for (i=0; i<4; i++) {
		for (j=0; j<10; j++) {
			*mazo[i*10 + j].figura = j;
			*mazo[i*10 + j].palo = i;
		}
	}
}

/**
 * Función: barajar
 * ----------------
 * Baraja el mazo de cartas (desordena el array de Carta).
 *
 * mazo (INOUT):  array a desordenar
 * n_cartas_mazo: cantidad de cartas en el mazo
 */
void barajar(Carta *mazo, int n_cartas_mazo)
{
	int i, j, myrank;
	Carta tmp;
	time_t t;
	srand((unsigned) time(&t));
	for (i=(n_cartas_mazo - 1); i>0; i--) {
		j = rand()%(i+1);
		tmp = mazo[j];
		mazo[j] = mazo[i];
		mazo[i] = tmp;
	}
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	printf("Jugador %d baraja el mazo de cartas.\n", myrank);
	return;
}

/**
 * Función: cortar
 * ---------------
 * El jugador Postre envía el mazo de cartas al jugador que tiene a su
 * izquierda para que éste lo corte.
 *
 * mazo (INOUT):  mazo a cortar
 * n_cartas_mazo: cantidad de cartas en el mazo
 * postre:        rank del jugador postre
 */
void cortar(Carta **mazo, int n_cartas_mazo, int postre)
{
	MPI_Barrier(MPI_COMM_WORLD);
	int i, myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	time_t t;
	srand((unsigned) time(&t));

	if (myrank == postre) {
		Send_cartas(*mazo, n_cartas_mazo, (postre + 3) % 4, MPI_COMM_WORLD);
		Recv_cartas(*mazo, n_cartas_mazo, (postre + 3) % 4, MPI_COMM_WORLD);
	}
	else if (myrank == (postre + 3) % 4) {
		*mazo = malloc_cartas(n_cartas_mazo);
		Recv_cartas(*mazo, n_cartas_mazo, postre, MPI_COMM_WORLD);
		i = (rand() % (n_cartas_mazo - 6)) + 3; // Se corta sin dejar 3 menos de tres cartas por arriba o por abajo
		Carta *mitad_1 = malloc_cartas(i);
		Carta *mitad_2 = malloc_cartas(n_cartas_mazo - i);

		memcpy(mitad_1, *mazo, sizeof(Carta) * i);
		memcpy(mitad_2, *mazo + i, sizeof(Carta) * (n_cartas_mazo - i));
		memcpy(*mazo, mitad_2, sizeof(Carta) * (n_cartas_mazo - i));
		memcpy(*mazo + n_cartas_mazo - i, mitad_1, sizeof(Carta) * i);
		printf("Jugador %d corta el mazo de cartas.\n", myrank);

		Send_cartas(*mazo, n_cartas_mazo, postre, MPI_COMM_WORLD);
		free_cartas(mazo, n_cartas_mazo);
	}
	return;
}

/**
 * Función: reparto_inicial
 * ------------------------
 * El jugador postre reparte cartas a los jugadores, de una en una y en sentido
 * contrario a las agujas del reloj.
 *
 * mazo (INOUT):          mazo de cartas desde donde se reparte
 * n_cartas_mazo (INOUT): cantidad de cartas en el mazo
 * postre:                rank del jugador postre
 * mi_mano:               cartas repartidas a cada jugador
 */
void reparto_inicial(Carta **mazo, int *n_cartas_mazo, int postre, Carta **mi_mano)
{
	MPI_Barrier(MPI_COMM_WORLD);
	int i, j;
	//MPI_Status status;
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	*mi_mano = malloc_cartas(4);

	if (myrank == postre) {
		printf("Jugador %d realiza el reparto inicial de cartas.\n", myrank);
		for (i=0; i<4; i++) {
			for (j=0; j<4; j++)
				Send_cartas(*mazo+(4*i+j), 1, (postre + 1 + j) % 4, MPI_COMM_WORLD);
		}
		// Sacar las 16 primeras cartas del mazo (que son las que se han repartido)
		Carta *restantes = malloc_cartas(24);
		memmove(restantes, *mazo + 16, sizeof(Carta)*24);
		*mazo = realloc_cartas(mazo, 40, 24);
		memmove(*mazo, restantes, sizeof(Carta) * 24);
		*n_cartas_mazo = 24;
	}
	// Todos reciben las cartas de 1 en 1 (incluido el postre que se comunica consigo mismo)
	for(i=0; i<4; i++)
		Recv_cartas(*mi_mano+i, 1, postre, MPI_COMM_WORLD);
	return;
}

/**
 * Función: pasar_mazo
 * -------------------
 * El jugador Postre envía el mazo de cartas al jugador que tiene a su derecha.
 * Se utiliza cuando acaba una ronda y hay que cambiar de jugador Postre o en la
 * fase de Mus Corrido.
 *
 * mazo (INOUT):          mazo de cartas a enviar/recibir
 * n_cartas_mazo (INOUT): cantidad de cartas en el mazo enviado/recibido
 * postre:                rank del jugador postre
 */
void pasar_mazo(Carta **mazo, int *n_cartas_mazo, int postre)
{
	MPI_Status status;
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	if (myrank == postre) {
		// Enviar mazo
		Send_cartas(*mazo, *n_cartas_mazo, (postre + 1) % 4, MPI_COMM_WORLD);
		printf("Jugador %d pasa el mazo a Jugador %d.\n", myrank, (postre + 1) % 4);
		*n_cartas_mazo = 0;
	}
	else if (myrank == (postre + 1) % 4) {
		// Recibir mazo
		MPI_Probe(postre, 0, MPI_COMM_WORLD, &status);
		MPI_Get_count(&status, MPI_CHAR, n_cartas_mazo);
		*n_cartas_mazo = *n_cartas_mazo / sizeof(Carta);
		*mazo = malloc_cartas(*n_cartas_mazo);
		Recv_cartas(*mazo, *n_cartas_mazo, postre, MPI_COMM_WORLD);
		printf("Jugador %d recibe el mazo.\n", myrank);
	}
	return;
}

/**
 * Función: descartar
 * ------------------
 * El jugador postre recibe las cartas que descartan los jugadores (incluído él mismo),
 * las almacena en el mazo de descartadas y a continuación envía a cada jugador, en
 * orden y sacándolas del mazo, el número de cartas que se han descartado.
 *
 * postre:                  rank del jugador postre
 * mi_mano (INOUT):         cartas con las que juega cada jugador
 * jugadas:                 array con todas las jugadas posibles y sus probabilidades de
 *                          victoria según las reglas del juego
 * mazo (INOUT):            mazo de cartas que todavía no se han jugado
 * n_cartas_mazo (INOUT):   cantidad de cartas en el mazo
 * descartadas (INOUT):     mazo de cartas ya jugadas (y descartadas por algún jugador)
 * total_descartes (INOUT): cantidad de cartas en el mazo de descartadas
 * global_comm:             intercomunicador que incluye al proceso manager
 */
void descartar(int postre, int interactivo, Carta **mi_mano, Jugada *jugadas, Carta **mazo, int *n_cartas_mazo, Carta **descartadas, int *total_descartes, MPI_Comm global_comm)
{
	MPI_Status status;
	int i, myrank, num_descartes;
	Carta *mis_descartes, *descartes_jugador;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	// Primero, determinar de qué cartas quiere descartarse cada jugador
	// Comunicación en anillo para hacerlo en orden y no interferir en stdout en caso
	// de estar en modo interactivo
	int t = 0; // testigo para comunicación en anillo
	if (myrank == (postre + 1) % 4) {
		if (interactivo && myrank == 0)
			num_descartes = I_get_descartes(mi_mano, &mis_descartes, global_comm);
		else
			num_descartes = get_descartes(mi_mano, jugadas, &mis_descartes);
		printf("Jugador %d descarta %d cartas.\n", myrank, num_descartes);
		MPI_Send(&t, 1, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}
	else {
		MPI_Recv(&t, 1, MPI_INT, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
		if (interactivo && myrank == 0)
			num_descartes = I_get_descartes(mi_mano, &mis_descartes, global_comm);
		else
			num_descartes = get_descartes(mi_mano, jugadas, &mis_descartes);
		printf("Jugador %d descarta %d cartas.\n", myrank, num_descartes);
		if (myrank != postre) MPI_Send(&t, 1, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}

	if (myrank != postre) {
		// Envío mis descartes al postre
		Send_cartas(mis_descartes, num_descartes, postre, MPI_COMM_WORLD);
		// Recibo las cartas del postre
		Carta *recibidas = malloc_cartas(num_descartes);
		Recv_cartas(recibidas, num_descartes, postre, MPI_COMM_WORLD);
		*mi_mano = realloc_cartas(mi_mano, 4 - num_descartes, 4);
		memcpy(*mi_mano + 4 - num_descartes, recibidas, sizeof(Carta) * num_descartes);
	}
	else {
		// Recibir los descartes del resto de jugadores y enviarles las cartas que necesitan, en orden
		for (i=0; i<3; i++) {
			// Recibir las cartas del jugador
			int n_descartes = 0;
			MPI_Probe((postre + i + 1) % 4, 0, MPI_COMM_WORLD, &status);
			MPI_Get_count(&status, MPI_CHAR, &n_descartes);
			n_descartes = n_descartes / sizeof(Carta);
			descartes_jugador = malloc_cartas(n_descartes);
			Recv_cartas(descartes_jugador, n_descartes, (postre + i + 1) % 4, MPI_COMM_WORLD);

			// Almacenar las cartas recibidas en el mazo de descartadas
			*descartadas = realloc_cartas(descartadas, *total_descartes, *total_descartes + n_descartes);
			memcpy(*descartadas + *total_descartes, descartes_jugador, n_descartes * sizeof(Carta));
			*total_descartes += n_descartes;

			// Antes de enviar nuevas cartas, comprobar que quedan suficientes en el mazo
			if (*n_cartas_mazo < n_descartes) {
				juntar_mazos(mazo, n_cartas_mazo, descartadas, total_descartes);
				barajar(*mazo, *n_cartas_mazo);
			}

			// Enviar nuevas cartas al jugador
			printf("Jugador %d reparte %d cartas a Jugador %d\n", myrank, n_descartes, (postre + i + 1) % 4);
			Send_cartas(*mazo, n_descartes, (postre + i + 1) % 4, MPI_COMM_WORLD);

			// Actualizar las cartas que quedan en el mazo tras enviar
			Carta *tmp_mazo = malloc_cartas(*n_cartas_mazo - n_descartes);
			memcpy(tmp_mazo, *mazo + n_descartes, (*n_cartas_mazo - n_descartes) * sizeof(Carta));
			*mazo = realloc_cartas(mazo, *n_cartas_mazo, *n_cartas_mazo - n_descartes);
			memcpy(*mazo, tmp_mazo, (*n_cartas_mazo - n_descartes) * sizeof(Carta));
			*n_cartas_mazo -= n_descartes;
		}
		// Descarto las mias y me reparto
		printf("Jugador %d coge %d cartas.\n", myrank, num_descartes);
		*descartadas = realloc_cartas(descartadas, *total_descartes, *total_descartes + num_descartes);
		memcpy(*descartadas + *total_descartes, mis_descartes, num_descartes * sizeof(Carta));
		*total_descartes += num_descartes;
		*mi_mano = realloc_cartas(mi_mano, 4 - num_descartes, 4);

		// Antes de repartirme nuevas cartas, comprobar que quedan suficientes en el mazo
		if (*n_cartas_mazo < num_descartes) {
			juntar_mazos(mazo, n_cartas_mazo, descartadas, total_descartes);
			barajar(*mazo, *n_cartas_mazo);
		}

		memcpy(*mi_mano + 4 - num_descartes, *mazo, num_descartes * sizeof(Carta));
		Carta *tmp_mazo = malloc_cartas(*n_cartas_mazo - num_descartes);
		memcpy(tmp_mazo, *mazo + num_descartes, (*n_cartas_mazo - num_descartes) * sizeof(Carta));
		*mazo = realloc_cartas(mazo, *n_cartas_mazo, *n_cartas_mazo - num_descartes);
		memcpy(*mazo, tmp_mazo, (*n_cartas_mazo - num_descartes) * sizeof(Carta));
		*n_cartas_mazo -= num_descartes;
	}
	return;
}

/**
 * Función: juntar_mazos
 * ---------------------
 * Une dos mazos de cartas en uno.
 *
 * mazo (INOUT):            primer mazo, que es además donde iran a parar las cartas del segundo mazo
 * n_cartas_mazo (INOUT):   cantidad de cartas en el primer mazo
 * descartadas (INOUT):     segundo mazo (quedará vacío tras la operación)
 * total_descartes (INOUT): cantidad de cartas en el segundo mazo
 */
static void juntar_mazos(Carta **mazo, int *n_cartas_mazo, Carta **descartadas, int *total_descartes)
{
	int myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	printf("Jugador %d: no quedan cartas suficientes en el mazo, utilizo las descartadas.\n", myrank);
	*mazo = realloc_cartas(mazo, *n_cartas_mazo, *n_cartas_mazo + *total_descartes);
	memcpy(*mazo + *n_cartas_mazo, *descartadas, *total_descartes * sizeof(Carta));
	*n_cartas_mazo += *total_descartes;
	free_cartas(descartadas, *total_descartes);
	*total_descartes = 0;
}

/**
 * Función: get_descartes
 * ----------------------
 * Determina qué cartas quiere descartarse cada jugador.
 * Primero se hayan todos los posibles descartes que pueden hacerse (4 de una carta, 6 de
 * dos cartas, 4 de tres cartas y 1 de cuatro cartas).
 * A continuación, se busca en la matriz de jugadas (que contiene todas las jugadas posibles
 * en el juego del Mus y sus probabilidades de victoria en cada Lance) las jugadas que nos
 * interesaría obtener tras recibir nuevas cartas, que serían jugadas con 31, Duples o
 * Medias.
 * Después, para cada uno de los posibles descartes, calcular la probabilidad de obtener
 * las jugadas que nos interesan con las cartas que nos quedarían en la mano.
 * El posible descarte que obtenga la probabilidad más alta de que nos salga una jugada
 * que nos interesa, será el que haremos finalmente.
 *
 * ATENCIÓN: es interesante darse cuenta de que la política (o algoritmo) de descartes
 * busca obtener jugadas con 31, Duples o Medias, ignorando las buenas jugadas de
 * Grande o Chica, ya que éstas son inherentemente contrarias entre ellas. La decisión
 * de si queremos descartarnos o no se hace en otra función (quiero_mus).
 *
 * mi_mano (INOUT):   cartas con las que juega el jugador
 * jugadas:           matriz con todas las posibles jugadas y sus probabilidades de
 *                    victoria en cada lance del juego
 * descartes (INOUT): cartas que decide descartarse el jugador
 *
 * returns: cantidad de cartas que queremos descartar
 */
int get_descartes(Carta **mi_mano, Jugada *jugadas, Carta **descartes)
{
	int i,j,k,pos;
	Descarte *posibles_descartes = malloc(sizeof(Descarte)*15);

	// Arrays para almacenar los índices en la matriz de jugadas de las jugadas que nos interesan.
	// Nada elaborado, sabemos exactamente el número de jugadas de cada tipo que hay.
	int *jugadas31 = malloc(sizeof(int)*25), *jugadasDuples = malloc(sizeof(int)*36), *jugadasMedias = malloc(sizeof(int)*50);

	/**
	 * Calcular los 15 descartes que se pueden hacer
	 */
	// 4 descartes de una carta
	for (i=0; i<4; i++) {
		posibles_descartes[i].cartas_en_mano = malloc(sizeof(Carta)*3);
		posibles_descartes[i].cartas_descartadas = malloc(sizeof(Carta));
		posibles_descartes[i].num_descartes = 1;
		pos = 0;
		for (j=0; j<4; j++) {
			if (j != i) {
				memcpy(posibles_descartes[i].cartas_en_mano + pos, *mi_mano + j, sizeof(Carta));
				pos++;
			} else
				memcpy(posibles_descartes[i].cartas_descartadas, *mi_mano + j, sizeof(Carta));
		}
	}

	// 6 descartes de dos cartas
	pos = 4;
	for (i=0; i<3; i++) {
		for (j=0; j<4; j++) {
			if(j>i) {
				posibles_descartes[pos].num_descartes = 2;
				posibles_descartes[pos].cartas_en_mano = malloc(sizeof(Carta)*2);
				memcpy(posibles_descartes[pos].cartas_en_mano, *mi_mano + i, sizeof(Carta));
				memcpy(posibles_descartes[pos].cartas_en_mano + 1, *mi_mano + j, sizeof(Carta));
				// No encuentro mejor forma de hacerlo :(
				posibles_descartes[pos].cartas_descartadas = malloc(sizeof(Carta)*2);
				int p = 0;
				for (k=0; k<4; k++) {
					if (k != i && k != j) {
						memcpy(posibles_descartes[pos].cartas_descartadas + p, *mi_mano + k, sizeof(Carta));
						p++;
					}
				}
				pos++;
			}
		}
	}

	// 4 descartes de tres cartas
	for (i=10; i<14; i++) {
		posibles_descartes[i].cartas_en_mano = malloc(sizeof(Carta));
		posibles_descartes[i].cartas_descartadas = malloc(sizeof(Carta)*3);
		posibles_descartes[i].num_descartes = 3;
		pos = 0;
		for (j=0; j<4; j++) {
			if (j == i-10)
				memcpy(posibles_descartes[i].cartas_en_mano, *mi_mano + j, sizeof(Carta));
			else {
				memcpy(posibles_descartes[i].cartas_descartadas + pos, *mi_mano + j, sizeof(Carta));
				pos++;
			}
		}
	}

	// 1 descarte de cuatro cartas
	posibles_descartes[14].cartas_descartadas = malloc(sizeof(Carta)*4);
	posibles_descartes[14].num_descartes = 4;
	memcpy(posibles_descartes[14].cartas_descartadas, *mi_mano, sizeof(Carta)*4);

	// Buscamos los índices de las jugadas que nos interesan
	int p1=0, p2=0, p3=0;
	for (i=0; i<330; i++) {
		if (jugadas[i].valor_juego == 31) {
			jugadas31[p1] = i;
			p1++;
		}
		else if (jugadas[i].valor_pares >= 300) {
			jugadasDuples[p2] = i;
			p2++;
		}
		else if (jugadas[i].valor_pares >= 200 && jugadas[i].valor_pares < 300) {
			jugadasMedias[p3] = i;
			p3++;
		}
	}

	// Para cada descarte, calular y sumar unas con otras las probabilidades de obtener
	// Juego, Duples o Medias con las cartas que se quedan en la mano
	double prob = 0;
	for (i=0; i<15; i++) {
		// Jugadas con Juego (31 ptos.)
		for (j=0; j<25; j++)
			probabilidad_descarte(&(posibles_descartes[i]), jugadas[jugadas31[j]].cartas);
		// Jugadas con Duples
		for (j=0; j<36; j++)
			probabilidad_descarte(&(posibles_descartes[i]), jugadas[jugadasDuples[j]].cartas);
		// Jugadas con Medias
		for (j=0; j<50; j++)
			probabilidad_descarte(&(posibles_descartes[i]), jugadas[jugadasMedias[j]].cartas);
		if (posibles_descartes[i].prob_obtener_jugada > prob) {
			prob = posibles_descartes[i].prob_obtener_jugada;
			pos = i;
		}
	}

	// Ya sabemos el descarte que queremos hacer, arreglar los arrays de mi_mano y descartes
	// en consecuencia.
	//*descartes = (Carta *)malloc(sizeof(Carta) * posibles_descartes[pos].num_descartes);
	*descartes = malloc_cartas(posibles_descartes[pos].num_descartes);
	for (i=0; i<posibles_descartes[pos].num_descartes; i++)
		memcpy(*descartes + i, posibles_descartes[pos].cartas_descartadas + i, sizeof(Carta));
	*mi_mano = realloc_cartas(mi_mano, 4, 4 - posibles_descartes[pos].num_descartes);
	for (i=0; i<(4-posibles_descartes[pos].num_descartes); i++)
		memcpy(*mi_mano + i, posibles_descartes[pos].cartas_en_mano + i, sizeof(Carta));

	// Devolver el número de cartas que nos hemos descartado
	return posibles_descartes[pos].num_descartes;
}

/**
 * Función: ganador_grande
 * -----------------------
 * Determina el jugador con la mejor jugada en el lance de Grandes.
 *
 * NOTA: la matriz jugadas ya está ordenada de mayor a menor según el valor
 * de las cartas (RRRR, RRRC, RRRS, ..., AAAA), de modo que la mano cuyo índice
 * en la matriz jugadas sea menor (de 0 a 329) es la que tiene la mejor
 * jugada en el lance de Grandes.
 *
 * myrank:   mi rank
 * postre:   rank del jugador postre
 * jugadas:  matriz con todas las posibles jugadas y su valor para cada lance
 * ind_mano: el índice que señala la posición de mi jugada en la matriz de jugadas
 * mi_mano:  cartas con las que juego
 *
 * returns: rank del jugador con la mejor jugada en el lance de Grandes
 */
int ganador_grande(int myrank, int postre, Jugada jugadas[], int ind_mano, Carta mi_mano[])
{
	MPI_Status status;
	int res_lance[2] = {-1, 0}; // Rank jugador ganador, posición/valor de la mano
	if (myrank == (postre + 1) % 4) {
		res_lance[0] = myrank;
		res_lance[1] = ind_mano;
		MPI_Send(&res_lance, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}
	else {
		MPI_Recv(&res_lance, 2, MPI_INT, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
		if (ind_mano < res_lance[1]) {
			res_lance[0] = myrank;
			res_lance[1] = ind_mano;
		}
		if (myrank != postre) MPI_Send(&res_lance, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}
	MPI_Bcast(&res_lance, 2, MPI_INT, postre, MPI_COMM_WORLD);
	return res_lance[0];
}

/**
 * Función: ganador_chica
 * ----------------------
 * Determina el jugador con la mejor jugada en el lance de Chicas.
 *
 * myrank:   mi rank
 * postre:   rank del jugador postre
 * jugadas:  matriz con todas las posibles jugadas y su valor para cada lance
 * ind_mano: el índice que señala la posición de mi jugada en la matriz de jugadas
 * mi_mano:  cartas con las que juego
 *
 * returns: rank del jugador con la mejor jugada en el lance de Chicas
 */
int ganador_chica(int myrank, int postre, Jugada jugadas[], int ind_mano, Carta mi_mano[])
{
	MPI_Status status;
	int res_lance[2] = {-1, 0}; // Rank jugador ganador, posición/valor de la mano
	if (myrank == (postre + 1) % 4) {
		res_lance[0] = myrank;
		res_lance[1] = jugadas[ind_mano].pos_en_chicas;
		MPI_Send(&res_lance, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}
	else {
		MPI_Recv(&res_lance, 2, MPI_INT, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
		if (jugadas[ind_mano].pos_en_chicas < res_lance[1]) {
			res_lance[0] = myrank;
			res_lance[1] = jugadas[ind_mano].pos_en_chicas;
		}
		if (myrank != postre) MPI_Send(&res_lance, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}
	MPI_Bcast(&res_lance, 2, MPI_INT, postre, MPI_COMM_WORLD);
	return res_lance[0];
}

/**
 * Función: ganador_pares
 * ----------------------
 * Determina el jugador con la mejor jugada en el lance de Pares.
 *
 * myrank:   mi rank
 * postre:   rank del jugador postre
 * jugadas:  matriz con todas las posibles jugadas y su valor para cada lance
 * ind_mano: el índice que señala la posición de mi jugada en la matriz de jugadas
 * mi_mano:  cartas con las que juego
 *
 * returns: rank del jugador con la mejor jugada en el lance de Pares
 */
int ganador_pares(int myrank, int postre, Jugada jugadas[], int ind_mano, Carta mi_mano[])
{
	MPI_Status status;
	int res_lance[2] = {-1, 0}; // Rank jugador ganador, posición/valor de la mano
	if (myrank == (postre + 1) % 4) {
		res_lance[0] = myrank;
		res_lance[1] = jugadas[ind_mano].valor_pares;
		MPI_Send(&res_lance, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}
	else {
		MPI_Recv(&res_lance, 2, MPI_INT, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
		if (jugadas[ind_mano].valor_pares > res_lance[1]) {
			res_lance[0] = myrank;
			res_lance[1] = jugadas[ind_mano].valor_pares;
		}
		if (myrank != postre) MPI_Send(&res_lance, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}
	MPI_Bcast(&res_lance, 2, MPI_INT, postre, MPI_COMM_WORLD);
	return res_lance[0];
}

/**
 * Función: ganador_juego
 * ----------------------
 * Determina el jugador con la mejor jugada en el lance de Juego.
 *
 * myrank:   mi rank
 * postre:   rank del jugador postre
 * jugadas:  matriz con todas las posibles jugadas y su valor para cada lance
 * ind_mano: el índice que señala la posición de mi jugada en la matriz de jugadas
 * mi_mano:  cartas con las que juego
 *
 * returns: rank del jugador con la mejor jugada en el lance de Juego
 */
int ganador_juego(int myrank, int postre, Jugada jugadas[], int ind_mano, Carta mi_mano[])
{
	MPI_Status status;
	int res_lance[2] = {-1, 0}; // Rank jugador ganador, posición/valor de la mano
	if (myrank == (postre + 1) % 4) {
		res_lance[0] = myrank;
		res_lance[1] = jugadas[ind_mano].valor_juego;
		MPI_Send(&res_lance, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}
	else {
		MPI_Recv(&res_lance, 2, MPI_INT, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
		if (jugadas[ind_mano].valor_juego == 31 && res_lance[1] != 31) {
			res_lance[0] = myrank;
			res_lance[1] = jugadas[ind_mano].valor_juego;
		}
		else if (jugadas[ind_mano].valor_juego == 32 && res_lance[1] != 31 && res_lance[1] != 32) {
			res_lance[0] = myrank;
			res_lance[1] = jugadas[ind_mano].valor_juego;
		}
		else if (res_lance[1] != 31 && res_lance[1] != 32 && jugadas[ind_mano].valor_juego > res_lance[1]) {
			res_lance[0] = myrank;
			res_lance[1] = jugadas[ind_mano].valor_juego;
		}
		if (myrank != postre) MPI_Send(&res_lance, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}
	MPI_Bcast(&res_lance, 2, MPI_INT, postre, MPI_COMM_WORLD);
	return res_lance[0];
}

/**
 * Función: ganador_punto
 * ----------------------
 * Determina el jugador con la mejor jugada en el lance de Punto.
 *
 * myrank:   mi rank
 * postre:   rank del jugador postre
 * jugadas:  matriz con todas las posibles jugadas y su valor para cada lance
 * ind_mano: el índice que señala la posición de mi jugada en la matriz de jugadas
 * mi_mano:  cartas con las que juego
 *
 * returns: rank del jugador con la mejor jugada en el lance de Punto
 */
int ganador_punto(int myrank, int postre, Jugada jugadas[], int ind_mano, Carta mi_mano[])
{
	MPI_Status status;
	int res_lance[2] = {-1, 0}; // Rank jugador ganador, posición/valor de la mano
	if (myrank == (postre + 1) % 4) {
		res_lance[0] = myrank;
		res_lance[1] = jugadas[ind_mano].valor_juego;
		MPI_Send(&res_lance, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}
	else {
		MPI_Recv(&res_lance, 2, MPI_INT, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
		if (jugadas[ind_mano].valor_juego > res_lance[1]) {
			res_lance[0] = myrank;
			res_lance[1] = jugadas[ind_mano].valor_juego;
		}
		if (myrank != postre) MPI_Send(&res_lance, 2, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}
	MPI_Bcast(&res_lance, 2, MPI_INT, postre, MPI_COMM_WORLD);
	return res_lance[0];
}

/**
 * Función: recoger_cartas
 * -----------------------
 * Recoger todas las cartas (las de cada jugador, las del mazo de descartadas y las del mazo) y
 * juntarlas en el mazo. Se realiza cada vez que finaliza una ronda y hay que pasarle las cartas
 * al siguiente jugador para que éste las reparta.
 *
 * postre:                  rank del jugador postre
 * mi_mano (INOUT):         array de cartas que tiene cada jugador
 * mazo (INOUT):            array de cartas que no se han repartido, que es donde se juntarán
 *                          todas y lo que se enviará al siguiente jugador
 * n_cartas_mazo (INOUT):   cantidad de cartas en el mazo
 * descartadas (INOUT):     array de cartas que han sido descartadas por los jugadores
 * total_descartes (INOUT): cantidad de cartas descartadas
 */
void recoger_cartas(int postre, Carta **mi_mano, Carta **mazo, int *n_cartas_mazo, Carta **descartadas, int *total_descartes)
{
	int myrank, i;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

	if (myrank == postre) {
		printf("Jugador %d recoge las cartas.\n", myrank);
		// Recibir las cartas de los jugadores y meterlas en el mazo
		for (i = 1; i < 4; i++) {
			Carta *recv = malloc_cartas(4);
			Recv_cartas(recv, 4, (myrank + i) % 4, MPI_COMM_WORLD);
			*mazo = realloc_cartas(mazo, *n_cartas_mazo, *n_cartas_mazo + 4);
			memcpy(*mazo + (*n_cartas_mazo), recv, 4 * sizeof(Carta));
			*n_cartas_mazo += 4;
		}

		// Meter las cartas propias en el mazo
		*mazo = realloc_cartas(mazo, *n_cartas_mazo, *n_cartas_mazo + 4);
		memcpy(*mazo + (*n_cartas_mazo), *mi_mano, 4 * sizeof(Carta));
		*n_cartas_mazo += 4;

		// Meter las descartadas en el mazo
		*mazo = realloc_cartas(mazo, *n_cartas_mazo, *n_cartas_mazo + *total_descartes);
		memcpy(*mazo + (*n_cartas_mazo), *descartadas, *total_descartes * sizeof(Carta));
		*n_cartas_mazo += *total_descartes;
		*total_descartes = 0;
	}
	// Enviar las cartas de mi mano al jugador postre
	else Send_cartas(*mi_mano, 4, postre, MPI_COMM_WORLD);
	MPI_Barrier(MPI_COMM_WORLD);
}

/**
 * Función: print_manos
 * --------------------
 * Imprime en pantalla las cartas de cada jugador, en orden, empezando por
 * el jugador que es mano.
 *
 * myrank:  rank del jugador
 * postre:  rank del jugador postre
 * cartas:  representación en caracteres de la mano (RRRR, RRCC, ...)
 * mi_mano: array de Carta que contiene las cartas del jugador
 */
void print_manos(int myrank, int postre, char *cartas, Carta mi_mano[])
{
	int t = 0; // testigo para comunicación en anillo, no tiene otra utilidad
	MPI_Status status;
	MPI_Barrier(MPI_COMM_WORLD);

	if (myrank == (postre + 1) % 4) {
		printf("Cartas Jugador %d (%s):\n", myrank, cartas);
		printf("   - %s\n", carta2Lchar(mi_mano[0]));
		printf("   - %s\n", carta2Lchar(mi_mano[1]));
		printf("   - %s\n", carta2Lchar(mi_mano[2]));
		printf("   - %s\n", carta2Lchar(mi_mano[3]));
		MPI_Send(&t, 1, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}
	else {
		MPI_Recv(&t, 1, MPI_INT, (myrank + 3) % 4, 0, MPI_COMM_WORLD, &status);
		printf("Cartas Jugador %d (%s):\n", myrank, cartas);
		printf("   - %s\n", carta2Lchar(mi_mano[0]));
		printf("   - %s\n", carta2Lchar(mi_mano[1]));
		printf("   - %s\n", carta2Lchar(mi_mano[2]));
		printf("   - %s\n", carta2Lchar(mi_mano[3]));
		if (myrank != postre) MPI_Send(&t, 1, MPI_INT, (myrank + 1) % 4, 0, MPI_COMM_WORLD);
	}

}

/**
 * Función: carta2char
 * -------------------
 * Devuelve una representación en 2 caracteres de una carta. Por ejemplo
 * el As de Oros sería AO, el Tres de Copas sería 3C, etc.
 *
 * carta: Carta que se representará
 *
 * return: la representación en dos caracteres de la carta
 */
char *carta2char(Carta carta)
{
	char *c = malloc(sizeof(char)*3);
	strcpy(c, figuras[*carta.figura]);
	strcat(c, palos[*carta.palo]);
	return c;
}

/**
 * Función: carta2char
 * -------------------
 * Devuelve la representación en caracteres "larga" de una carta (As de Oros,
 * Tres de Copas, etc.)
 *
 * carta: Carta que se representará
 *
 * return: la representación en caracteres de la carta
 */
char *carta2Lchar(Carta carta)
{
	char *c = malloc(sizeof(char) * (strlen(figuras_largo[*carta.figura]) + strlen(palos_largo[*carta.palo]) + 5) );
	strcpy(c, figuras_largo[*carta.figura]);
	strcat(c, " de ");
	strcat(c, palos_largo[*carta.palo]);
	return c;
}

/**
 * Función: mano2figuras8R
 * -----------------------
 * Convierte un conjunto de cartas en su equivalente según las reglas
 * del Mus a 8 Reyes (los treses se tratan como reyes y los doses como
 * ases, y se ignoran los palos), y además lo ordena de mayor a menor.
 * Así, el grupo de cartas Dos de Espadas, Tres de Oros y Rey de Copas
 * se transformará en "RRA".
 *
 * cartas:   conjunto de cartas que se va a convertir
 * tam_mano: cantidad de elementos en el conjunto de cartas
 *
 * return: la representación en caracteres del conjunto de cartas
 */
char *mano2figuras8R(Carta *cartas, int tam_mano)
{
	if (tam_mano == 0)
		return (char) 0;
	int i, *mano = malloc(sizeof(int)*tam_mano);
	char *c = calloc(tam_mano+1, sizeof(char));
	for (i=0; i<tam_mano; i++)
		mano[i] = *cartas[i].figura;
	qsort(mano, tam_mano, sizeof(int), &compara);
	for (i=0; i<tam_mano; i++) {
		memcpy(&(c[i]), figuras8R[mano[i]], strlen(figuras8R[mano[i]]));
	}
	c[tam_mano] = '\0';
	free(mano);
	return c;
}

/**
 * Función: pares2char
 * -------------------
 * Devuelve la representación en caracteres del enum Tpares.
 *
 * par: uno de los enum de Tpares
 *
 * return: la representación en caracteres
 */
char *pares2char(Tpares par)
{
	switch(par) {
	case 1:
		return "parejas";
		break;
	case 2:
		return "medias";
		break;
	case 3:
		return "duples";
		break;
	default:
		return "nada";
	}
}

/**
 * Función: compara
 * ----------------
 * Función auxiliar de qsort() en mano2figuras8R que se utiliza para ordenar el
 * resultado de esta función. Determina si una carta es mayor que otra. Hay
 * que tener en cuenta el enum Nombre_carta, según el cual se asignan valores
 * a cada figura de las cartas (as = 0, dos = 1, ..., rey = 9).
 *
 * _a: valor de la carta
 * _b: valor de la carta
 *
 * return: entero positivo si b>a, entero negativo si b<a, 0 si b==a
 */
static int compara(const void *_a, const void *_b)
{
	int *a, *b;

	a = (int *) _a;
	b = (int *) _b;

	// Los treses se tratan como reyes, carta mayor siempre
	if (*a == 2 && *b == 2)
		return 0;
	if (*b == 2)
		return 1;
	if (*a == 2)
		return -1;
	return (*b - *a);
}

/**
 * Función: print_mazo
 * -------------------
 * Imprime en pantalla la representación de un conjunto de estructuras
 * Carta (un mazo de cartas).
 *
 * mazo: conjunto de cartas que se quiere imprimir
 * n:    cantidad de elementos en el conjunto de cartas
 */
void print_mazo(Carta *mazo, int n) {
	int i, myrank;
	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
	printf("%d: ", myrank);
	for (i=0; i<n; i++) {
		printf("%s ", carta2char(mazo[i]));
	}
	printf("\n");
	return;
}
