#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h> // para offsetof
#include "mpi.h"
#include "juegomus.h"
#include "probsmus.h"

static const char cartas[] = "A4567SCR";
static const int valor_cartas[] = {1, 4, 5, 6, 7, 10, 10, 10};

static double calcular_probabilidad_jugadas(char *jugadaA, char *jugadaB);
static int calcular_combinaciones(int n, int k);
static void busca_pares(Jugada *jugada);
static Boolean juego_superior(int juegoA, int juegoB);

/**
 * Función: generar_jugadas
 * ----------------------
 * Genera todas las jugadas posibles de una partida de mus a 8 reyes (330),
 * junto con información relevante acerca de cada una de estas jugadas como
 * por ejemplo la probabilidad que tiene de vencer en cada uno de los
 * lances del juego.
 * Las probabilidades se calculan sin tener nunca en cuenta la jugada del
 * compañero (sin señas) y como si fueran siempre para el jugador "jugada",
 * es decir que en caso de empate, se ganaría.
 *
 * jugadas (INOUT): matriz donde almacenar las 330 jugadas y su información
 */
void generar_jugadas(Jugada jugadas[])
{
	char jugada[5];
	/* Matriz auxiliar para almacenar las jugadas en orden inverso (ascendente) */
	char aux_jugadas_chica[NUM_MANOS][5];
	/* Matriz 330x330 donde almacenar todas las probabilidades condicionadas
	 * (probabilidad de que el contrario saque una jugada determinada en función
	 * de la jugada que tenga yo) */
	double probabilidad_jugadas[NUM_MANOS][NUM_MANOS];

	/* Para generar las jugadas, contaremos en octal hacia atrás desde 7777 (RRRR)
	 * hasta 0000 (AAAA) */
	int oct = 07777;
	int i = 0;
	int j = NUM_MANOS - 1;
	while (oct >= 0) {
		sprintf(jugada, "%04o", oct);

		/* Jugadas en orden descendente (RRRR, RRRC, ..., AAAA), que es el orden para
		 * el lance de Grandes */
		if (jugada[0] >= jugada[1] && jugada[1] >= jugada[2] && jugada[2] >= jugada[3]) {
			jugadas[i].cartas[0] = cartas[jugada[0] - '0'];
			jugadas[i].cartas[1] = cartas[jugada[1] - '0'];
			jugadas[i].cartas[2] = cartas[jugada[2] - '0'];
			jugadas[i].cartas[3] = cartas[jugada[3] - '0'];
			jugadas[i].cartas[4] = '\0';
			// Es más cómodo calcular el valor para los lances de Juego/Puntos aquí
			jugadas[i].valor_juego = valor_cartas[jugada[0] - '0'] + valor_cartas[jugada[1] - '0'] + valor_cartas[jugada[2] - '0'] + valor_cartas[jugada[3] - '0'];
			i++;
		}

		/* Jugadas en orden ascendente (AAAA, 4AAA, 5AAA, ..., RRRR), que es el orden
		 * para el lance de Chicas */
		if (jugada[0] <= jugada[1] && jugada[1] <= jugada[2] && jugada[2] <= jugada[3]) {
			aux_jugadas_chica[j][0] = cartas[jugada[3] - '0']; /* Nótese que se invierte el orden en el que se generan las cartas */
			aux_jugadas_chica[j][1] = cartas[jugada[2] - '0'];
			aux_jugadas_chica[j][2] = cartas[jugada[1] - '0'];
			aux_jugadas_chica[j][3] = cartas[jugada[0] - '0'];
			aux_jugadas_chica[j][4] = '\0';
			j--;
		}

		oct--;
	}

	/* Para cada jugada:
	 *   1. Determinamos si tiene Pares
	 *   2. Determinamos si tiene Juego
	 *   3. Ayudándonos de la matriz auxiliar donde se han almacenado las jugadas en orden
	 *      ascendente, determinamos la posición que tiene la jugada para el lance de Chicas
	 *   4. Rellenamos la fila correspondiente de la matriz probabilidad_jugadas donde
	 *      se almacenan las probabilidades de sacar una jugada determinada en función de la
	 *      que ya se ha sacado */
	for (i=0; i<NUM_MANOS; i++) {
		busca_pares(&jugadas[i]);
		if (jugadas[i].valor_juego < 41 && jugadas[i].valor_juego > 30)
			jugadas[i].juego = true;
		else
			jugadas[i].juego = false;
		int pos = 0;
		while (strcmp(jugadas[i].cartas, aux_jugadas_chica[pos]) != 0) pos++;
		jugadas[i].pos_en_chicas = pos;
		for (j=0; j<NUM_MANOS; j++) {
			probabilidad_jugadas[i][j] = calcular_probabilidad_jugadas(jugadas[i].cartas, jugadas[j].cartas);
		}
	}

	// Ahora se calculan las probabilidades que tiene cada jugada de ganar en cada lance
	for (i=0; i<NUM_MANOS; i++) {

		/* Para el lance de Grandes es sencillo: la jugada se gana a sí misma y a todas
		 * las que estén por debajo (están ordenadas de mayor a menor) */
		jugadas[i].P_gana_grande = 0.0;
		for (j=i; j<NUM_MANOS; j++) {
			jugadas[i].P_gana_grande += probabilidad_jugadas[i][j];
		}

		/* Para el lance de Chicas hacemos uso del valor donde se ha almacenado
		 * previamente la posición de cada jugada al ordenarlas de menor a mayor */
		jugadas[i].P_gana_chica = 0.0;
		for (j=0; j<NUM_MANOS; j++) {
			if (jugadas[i].pos_en_chicas <= jugadas[j].pos_en_chicas)
				jugadas[i].P_gana_chica += probabilidad_jugadas[i][j];
		}

		// Si se ha determinado que la jugada tiene jugada de Pares, hacemos lo siguiente
		jugadas[i].P_gana_pares = 0.0;
		if (jugadas[i].pares == true) {
			// Obtener la probabilidad de que el jugador contrario tenga alguna jugada con pares
			double ppares = 0.0;
			for (j=0; j<NUM_MANOS; j++) {
				if (jugadas[j].pares == true)
					ppares += probabilidad_jugadas[i][j];
			}
			/* Se obtiene la probabilidad de que el contrario saque una jugada concreta de
			 * pares condicionado a haber sacado alguna jugada con pares y, si la jugada es
			 * peor, se suma a la probabilidad que tiene mi jugada de resultar ganadora */
			for (j=0; j<NUM_MANOS; j++) {
				if (jugadas[j].pares == true && jugadas[i].valor_pares >= jugadas[j].valor_pares)
					jugadas[i].P_gana_pares += probabilidad_jugadas[i][j] / ppares;
			}
		}

		// Si se ha determinado que la jugada tiene jugada con Juego, hacemos lo siguiente
		jugadas[i].P_gana_juego = 0.0;
		jugadas[i].P_gana_punto = 0.0;
		if (jugadas[i].juego == true) {
			// Obtener la probabilidad de que el jugador contrario tenga alguna jugada con Juego
			double pjuego = 0.0;
			for (j=0; j<NUM_MANOS; j++) {
				if (jugadas[j].juego == true)
					pjuego += probabilidad_jugadas[i][j];
			}
			/* Se obtiene la probabilidad de que el contrario saque una jugada concreta con
			 * Juego condicionado a haber sacado alguna jugada con Juego y, si la jugada es
			 * peor, se suma a la probabilidad que tiene mi jugada de resultar ganadora */
			for (j=0; j<NUM_MANOS; j++) {
				if (jugadas[j].juego == true && juego_superior(jugadas[i].valor_juego, jugadas[j].valor_juego) == true)
					jugadas[i].P_gana_juego += probabilidad_jugadas[i][j] / pjuego;
			}
		} else {
			// No hay Juego, pero hacemos lo mismo para el lance de Puntos
			double ppuntos = 0.0;
			for (j=0; j<NUM_MANOS; j++) {
				if (jugadas[j].juego == false)
					ppuntos += probabilidad_jugadas[i][j];
			}
			for (j=0; j<NUM_MANOS; j++) {
				if (jugadas[j].juego == false && (31 - jugadas[i].valor_juego) <= (31 - jugadas[j].valor_juego))
					jugadas[i].P_gana_punto += probabilidad_jugadas[i][j] / ppuntos;
			}
		}
	}

	return;
}

/**
 * Función: busca_pares
 * --------------------
 * Busca jugadas con Pares en una jugada (duples, medias y parejas). Añade
 * también un valor numérico a la estructura Jugada para que sea sencillo
 * comparar jugadas con Pares entre ellas, de modo que cuanto mayor sea
 * este valor, mejor será la jugada con Pares (así, la jugada C77A tendrá
 * el mismo valor que la jugada RS77).
 *
 * jugada (INOUT): la jugada que se está comprobando
 */
static void busca_pares(Jugada *jugada)
{
	int pos = 0;

	/**
	 * Busca duples: x x x x
	 */
	if(jugada->cartas[0] == jugada->cartas[1] && jugada->cartas[0] == jugada->cartas[2] && jugada->cartas[0] == jugada->cartas[3]) {
		jugada->tipo_pares = duples;
		jugada->pares = true;
		while (cartas[pos] != jugada->cartas[0]) pos++;
		jugada->valor_pares = jugada->tipo_pares * 100 + pos * 10 + pos; /* Valor = 3XX) */
		return;
	}

	/**
	 * Busca medias: x x x c
	 */
	if(jugada->cartas[0] == jugada->cartas[1] && jugada->cartas[1] == jugada->cartas[2]) {
		jugada->tipo_pares = medias;
		jugada->pares = true;
		while (cartas[pos] != jugada->cartas[0]) pos++;
		jugada->valor_pares = jugada->tipo_pares * 100 + pos * 10; /* Valor = 2X0 */
		return;
	}

	/**
	 * Busca medias: c x x x
	 */
	if(jugada->cartas[1] == jugada->cartas[2] && jugada->cartas[2] == jugada->cartas[3]) {
		jugada->tipo_pares = medias;
		jugada->pares = true;
		while (cartas[pos] != jugada->cartas[1]) pos++;
		jugada->valor_pares = jugada->tipo_pares * 100 + pos * 10; /* Valor = 2X0 */
		return;
	}

	/**
	 * Busca duples: x x y y
	 */
	if(jugada->cartas[0] == jugada->cartas[1] && jugada->cartas[2] == jugada->cartas[3]) {
		jugada->tipo_pares = duples;
		jugada->pares = true;
		while (cartas[pos] != jugada->cartas[0]) pos++;
		jugada->valor_pares = jugada->tipo_pares * 100 + pos * 10;
		pos = 0;
		while (cartas[pos] != jugada->cartas[2]) pos++;
		jugada->valor_pares += pos; /* Valor = 3XY */
		return;
	}

	/**
	 * Busca parejas: x x c c
	 */
	if(jugada->cartas[0] == jugada->cartas[1]) {
		jugada->tipo_pares = parejas;
		jugada->pares = true;
		while (cartas[pos] != jugada->cartas[0]) pos++;
		jugada->valor_pares = jugada->tipo_pares * 100 + pos; /* Valor = 10X */
		return;
	}

	/**
	 * Busca parejas: c x x c
	 */
	if(jugada->cartas[1] == jugada->cartas[2]) {
		jugada->tipo_pares = parejas;
		jugada->pares = true;
		while (cartas[pos] != jugada->cartas[1]) pos++;
		jugada->valor_pares = jugada->tipo_pares * 100 + pos; /* Valor = 10X */
		return;
	}

	/**
	 * Busca parejas: c c x x
	 */
	if(jugada->cartas[2] == jugada->cartas[3]) {
		jugada->tipo_pares = parejas;
		jugada->pares = true;
		while (cartas[pos] != jugada->cartas[2]) pos++;
		jugada->valor_pares = jugada->tipo_pares * 100 + pos; /* Valor = 10X */
		return;
	}

	// No hay jugada de Pares
	jugada->tipo_pares = nada;
	jugada->pares = false;
	jugada->valor_pares = 0;
	return;
}

/**
 * Función: calcular_probabilidad_jugadas
 * --------------------------------------
 * Obtiene la probabilidad que hay de que el contrario tenga una jugada concreta
 * condicionado a la jugada que tenga yo.
 * Primero se determina qué cartas quedan en la baraja descontando las de mi
 * jugada. A continuación se determinan las cartas necesarias para la jugada del
 * jugador contrario. Con estos dos datos podemos calcular todas las
 * combinaciones posibles de sacar la jugada del jugador contrario con las
 * cartas que le he dejado en la baraja, y de ahí obtener la probabilidad
 * (combinaciones posibles/combinaciones totales).
 *
 * jugadaA: mi jugada (RRCA, S7AA...)
 * jugadaB: la posible jugada contraria
 *
 * return: probabilidad de obtener jugadaB condicionado a tener jugadaA
 */
static double calcular_probabilidad_jugadas(char *jugadaA, char *jugadaB)
{
	// Cantidad total de cada carta en la baraja { A, 4, 5, 6, 7, S, C, R }
	int cartas_restantes[8] = { 8, 4, 4, 4, 4, 4, 4, 8 };
	// Cantidad total de cada carta en la mano contraria { A, 4, 5, 6, 7, S, C, R }
	int cartas_en_jugadaB[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	int pos, i;
	int CF = 1; // Casos favorables en los que aparece la jugada contraria

	/* Calcular las cartas que quedan en la baraja tras retirar las de
	 * mi jugada, y obtener también el array con el número de cartas en
	 * la jugada del jugador contrario */
	for (i=0; i<4; i++) {
		pos = 0;
		while (cartas[pos] != jugadaA[i]) pos++;
		cartas_restantes[pos]--;
		pos = 0;
		while (cartas[pos] != jugadaB[i]) pos++;
		cartas_en_jugadaB[pos]++;
	}

	/* Obtener todas las combinaciones para una jugada teniendo en cuenta
	 * las cartas que quedan en la baraja.
	 * Por ejemplo si mi jugada es RR77 en la baraja quedan 6xR, 4xC, 4xS,
	 * 2x7, 4x6, 4x5, 4x4 y 8xA. El número de formas de sacar R7AA es
	 * 6C1 (combinaciones de 6 elementos (reyes) sacados de 1 en 1) x
	 *     2C1 (combinaciones de 2 elementos (sietes) sacados de 1 en 1) x
	 *     8C2 (combinaciones de 2 elementos (ases) sacados de 2 en 2) */
	for (i=0; i<8; i++) {
		if (cartas_en_jugadaB[i] > 0) {
			CF = CF * calcular_combinaciones(cartas_restantes[i], cartas_en_jugadaB[i]);
		}
	}

	// CF/CP (combinaciones de 36 cartas tomadas de 4 en 4)
	return (double) CF / calcular_combinaciones(36,4);
}

/**
 * Función: probabilidad_descarte
 * ------------------------------
 * Calcula la probabilidad de sacar una jugada concreta en función de las cartas que
 * se hayan descartado. Por ejemplo la probabilidad de sacar RR77 si me quedo con RR
 * habiendo descartado S5.
 *
 * descarte (INOUT): estructura que contiene los detalles del descarte (cartas que
 *                   se quedan en la mano, cartas descartadas y la probabilidad
 *                   acumulada de sacar jugada en función del propio descarte)
 * jugada:           la jugada sobre la que se quiere obtener la probabilidad de
 *                   que aparezca
 */
void probabilidad_descarte(Descarte *descarte, char *jugada)
{
	int i, pos;
	char *mano = mano2figuras8R(descarte->cartas_en_mano, 4 - descarte->num_descartes);
	char *desc = mano2figuras8R(descarte->cartas_descartadas, descarte->num_descartes);

	// Si no se puede obtener la jugada con las cartas que quedan en la mano, salir
	if (!es_submano(mano, jugada))
		return;

	int p; // Número de cartas que quedan en la mano
	if (mano == NULL)
		p = 0;
	else
		p = strlen(mano);
	int q = strlen(desc); // Número de cartas descartadas
	// Cantidad total de cada carta en la baraja { A, 4, 5, 6, 7, S, C, R }
	int cartas_restantes[8] = { 8, 4, 4, 4, 4, 4, 4, 8 };
	// Cantidad de cartas de cada tipo que quedan en la mano
	int cartas_en_mano[8]   = { 0, 0, 0, 0, 0, 0, 0, 0 };
	// Cantidad de cartas de cada tipo en la jugada que se quiere sacar
	int cartas_objetivo[8]  = { 0, 0, 0, 0, 0, 0, 0, 0 };

	/* Actualizar las cartas que quedan en la baraja tras descontar las
	 * que hay en la mano y las que se descartan. */
	for (i=0; i<p; i++) {
		pos = 0;
		while (mano[i] != cartas[pos]) pos++;
		cartas_restantes[pos]--;
		cartas_en_mano[pos]++; // Aprovechamos el loop para actualizar la cantidad de cada carta en la mano
	}
	for (i=0; i<q; i++) {
		pos = 0;
		while (desc[i] != cartas[pos]) pos++;
		cartas_restantes[pos]--;
	}
	// Obtener el número de cartas de cada tipo para la jugada objetivo...
	for (i=0; i<4; i++) {
		pos = 0;
		while (jugada[i] != cartas[pos]) pos++;
		cartas_objetivo[pos]++;
	}
	// ... y restarle las cartas que ya tenga en la mano
	for (i=0; i<8; i++) {
		cartas_objetivo[i] -= cartas_en_mano[i];
	}

	// Calcular probabilidad
	int CF = 1, CP; // Casos favorables y posibles
	double prob = 0.0;
	for (i=0; i<8; i++) {
		if (cartas_objetivo[i] > 0)
			CF = CF * calcular_combinaciones(cartas_restantes[i], cartas_objetivo[i]);
	}
	CP = calcular_combinaciones(36, q); // 36 cartas cogidas de q en q (cartas descartadas)
	prob = (double)CF/(double)CP;
	descarte->prob_obtener_jugada += prob; // Nos interesa la probabilidad acumulada de obtener todas las jugadas objetivo con el descarte

	return;
}

/**
 * Función: es_submano
 * -------------------
 * Compara la representación en caracteres de dos conjuntos de cartas
 * y determina si una es subconjunto de otra. Así, la mano "RS" sería
 * submano de la mano "RCSA".
 *
 * sub:  mano sobre la que se quiere saber si pertenece a otra
 * mano: mano con la que comprobar la anterior
 *
 * return: 1 si es submano, 0 en caso contrario
 */
int es_submano(char sub[], char mano[])
{
	if (sub == NULL)
		return 1;

	int i,j,sum=0;
	int s = strlen(sub);
	int m = strlen(mano);
	int *en_sub =  calloc(s, sizeof(int));
	int *en_mano = calloc(m, sizeof(int));

	for (i=0; i<s; i++) {
		for (j=0; j<m; j++) {
			if (sub[i] == mano[j] && !en_sub[i] && !en_mano[j]) {
				en_sub[i] = 1;
				en_mano[j] = 1;
				break;
			}
		}
	}

	for (i=0; i<m; i++)
		sum += en_mano[i];
	if (sum == s)
		return 1;
	return 0;
}

/**
 * Función: calcular_combinaciones
 * -------------------------------
 * Calcula nCk, es decir el coeficiente binomial o combinaciones, el número de formas
 * en que se puede sacar un subconjunto k de elementos (sin repetición) a partir del
 * conjunto n de elementos.
 *
 * n: número de elementos en el conjunto
 * k: número de elementos en el subconjunto, o cuántos elementos del conjunto n se toman
 *    en cada combinación posible
 *
 * returns: el número total de combinaciones
 */
static int calcular_combinaciones(int n, int k)
{
	int i, n_fact = 1, k_fact = 1;

	if (n == 0 || n < k) return 0;
	if (k == 0 || n == k) return 1;

	for (i=0; i<k; i++)
		n_fact = n_fact * (n - i);
	for (i=1; i<=k; i++)
		k_fact = k_fact * i;

	return n_fact / k_fact;
}

/**
 * Función: juego_superior
 * -----------------------
 * Compara los puntos de dos jugadas y determina cuál de ellas es superior en
 * el lance de Juego.
 *
 * juegoA: puntuación de mi jugada
 * juegoB: puntuación de jugada contraria
 *
 * returns: true en caso de que mi jugada sea superior
 * 		    false en caso de que mi jugada sea inferior
 */

static Boolean juego_superior(int juegoA, int juegoB)
{
	if (juegoA == 31 || juegoA == juegoB) /* En caso de empate gana mi jugada */
		return true;
	if (juegoA == 32 && juegoB > 32)
		return true;
	if (juegoA == 40 && juegoB > 32)
		return true;
	if (juegoA == 37 && (juegoB > 32 && juegoB < 37))
		return true;
	if (juegoA == 36 && (juegoB > 32 && juegoB < 36))
		return true;
	if (juegoA == 35 && (juegoB == 34 || juegoB == 33))
		return true;
	if (juegoA == 34 && juegoB == 33)
		return true;
	return false;
}

/**
 * Función: Get_MPI_Tipo_Jugada
 * ----------------------------
 * Crea el tipo MPI necesario para poder enviar/recibir mensajes que incluyan
 * la estructura Jugada.
 *
 * tipo_jugada (INOUT): tipo MPI
 */
void Get_MPI_Tipo_Jugada(MPI_Datatype *tipo_jugada)
{
	MPI_Aint offsets[12] = {offsetof(Jugada, cartas), offsetof(Jugada, P_gana_grande), offsetof(Jugada, P_gana_chica),
			offsetof(Jugada, P_gana_pares), offsetof(Jugada, P_gana_juego), offsetof(Jugada, P_gana_punto),
			offsetof(Jugada, pos_en_chicas), offsetof(Jugada, pares), offsetof(Jugada, tipo_pares),
			offsetof(Jugada, valor_pares), offsetof(Jugada, juego), offsetof(Jugada, valor_juego)};
	int blocklengths[12] = {sizeof(char)*5, sizeof(double), sizeof(double), sizeof(double), sizeof(double),
			sizeof(double), sizeof(int), sizeof(char), sizeof(int), sizeof(int), sizeof(char), sizeof(int)};
	MPI_Datatype tipos_elementos_jugada[12] = {MPI_CHAR, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE,
			MPI_DOUBLE,	MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};
	MPI_Datatype tipo_tmp;

	/* OJO!: Por algún motivo MPI_Type_create_struct no funciona si se indica correctamente
	 * los elementos que tiene la estructura de la que parte (12), pero sí lo hace al decirle
	 * que son 2. He revisado *todo* (tamaño de los bloques, desplazamientos...) y parece
	 * estar bien, no encuentro explicación. Al indicarle un tamaño de 12 elementos se
	 * crea una estructura demasiado grande y luego hay fallos al enviar/recibir. */
	MPI_Type_create_struct(2, blocklengths, offsets, tipos_elementos_jugada, &tipo_tmp);
	MPI_Type_commit(&tipo_tmp);
	*tipo_jugada = tipo_tmp;
	return;
}

void print_jugada(Jugada jugada, int i)
{
	printf("%d. ", i);
	printf("%s - ", jugada.cartas);
	printf("P(Grande): %f - ", jugada.P_gana_grande*100);
	printf("P(Chica): %f - ", jugada.P_gana_chica*100);
	printf("Par: %d (%d) - ", jugada.pares, jugada.valor_pares);
	printf("P(Pares): %f - ", jugada.P_gana_pares*100);
	printf("Juego (%d): %d - ", jugada.juego, jugada.valor_juego);
	printf("P(Juego): %f - ", jugada.P_gana_juego*100);
	printf("P(Puntos): %f", jugada.P_gana_punto*100);
	printf("\n");
}
