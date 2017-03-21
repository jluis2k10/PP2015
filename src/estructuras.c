/*
 * estructuras.c
 *
 *  Created on: 28 ago. 2016
 *      Author: alumnopp
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "estructuras.h"

/**
 * Función: malloc_cartas
 * ----------------------
 * Inicializa/reserva memoria para un array de cartas.
 *
 * n: tamaño del array de cartas
 *
 * return: el array inicializado
 */
Carta * malloc_cartas(int n)
{
	int i;
	Carta *cartas = (Carta *)malloc(sizeof(Carta) * n);
	for(i = 0; i < n; i++) {
		cartas[i].figura = malloc(sizeof(Nombre_carta));
		cartas[i].palo = malloc(sizeof(Palo_carta));
	}
	return cartas;
}

/**
 * Función: free_cartas
 * --------------------
 * Libera la memoria ocupada por un array de cartas.
 *
 * cartas (INOUT): array de cartas
 * n:              tamaño del array
 */
void free_cartas(Carta **cartas, int n)
{
	int i;
	for (i = 0; i < n; i++) {
		free((*cartas+i)->figura);
		free((*cartas+i)->palo);
	}
	free(*cartas);
	*cartas = NULL;
}

/**
 * Función: realloc_cartas
 * -----------------------
 * Redimensiona el tamaño en memoria de un array de cartas.
 *
 * cartas (INOUT): el array de cartas original
 * old:            tamaño del array original
 * new:            nuevo tamaño
 *
 * return:         nuevo array con el tamaño requerido
 */
Carta * realloc_cartas(Carta **cartas, int old, int new)
{
	Carta *tmp = malloc_cartas(new);
	if (new >= old)	memmove(tmp, *cartas, sizeof(Carta) * old);
	else memmove(tmp, *cartas, sizeof(Carta) * new);
	free(*cartas);
	return tmp;
}

/**
 * Función: cartas2serial
 * ----------------------
 * Serializa un array de cartas, de modo que pueda ser enviado con
 * operaciones de paso de mensajes de MPI.
 * NOTA: el array de cartas contiene la estructura Carta, que a su vez
 * está compuesta por punteros que evidentemente no pueden enviarse
 * directamente con las primitivas de MPI, con lo cual es necesario
 * serializar/deserializar su contenido real.
 *
 * cartas: array que se quiere serializar
 * n:      tamaño del array
 *
 * return: array serializado
 */
static char * cartas2serial(Carta *cartas, int n)
{
	int i;
	char *serial = malloc(n * (sizeof(Nombre_carta) + sizeof(Palo_carta)));
	for (i = 0; i < n; i++) {
		memcpy(serial + (sizeof(Carta))*i, cartas[i].figura, sizeof(Nombre_carta));
		memcpy(serial + (sizeof(Carta))*i + sizeof(Nombre_carta), cartas[i].palo, sizeof(Palo_carta));
	}
	return serial;
}

/**
 * Función: serial2cartas
 * ----------------------
 * "Deserializa" una cadena de caracteres para converirla en un
 * array de cartas.
 *
 * serial:         cadena de caracteres
 * cartas (INOUT): array de cartas
 */
static void serial2cartas(char *serial, int n, Carta *cartas)
{
	int i;
	for (i = 0; i < n; i++) {
		memcpy(cartas[i].figura, serial + (sizeof(Carta))*i, sizeof(Nombre_carta));
		memcpy(cartas[i].palo, serial + (sizeof(Carta))*i + sizeof(Nombre_carta), sizeof(Palo_carta));
	}
}

/**
 * Función: Send_cartas
 * --------------------
 * Envía un array de cartas al proceso indicado.
 *
 * cartas: array de cartas a enviar
 * n:      tamaño del array de cartas
 * dest:   proceso destino del mensaje
 * comm:   comunicador MPI
 */
void Send_cartas(Carta *cartas, int n, int dest, MPI_Comm comm)
{
	char *serial = cartas2serial(cartas, n);
	int num_caracteres = n * (sizeof(Nombre_carta) + sizeof(Palo_carta));
	MPI_Send(serial, num_caracteres, MPI_CHAR, dest, 0, comm);
}

/**
 * Función: Recv_cartas
 * --------------------
 * Recibe un array de cartas del proceso origen.
 *
 * cartas (INOUT): array de cartas recibido
 * n:              tamaño del array de cartas
 * orig:           proceso remitente del mensaje
 * comm:           comunicador MPI
 */
void Recv_cartas(Carta *cartas, int n, int orig, MPI_Comm comm)
{
	MPI_Status status;
	int num_caracteres = n * sizeof(Carta);
	char *serial = malloc(num_caracteres);
	MPI_Recv(serial, num_caracteres, MPI_CHAR, orig, 0, comm, &status);
	serial2cartas(serial, n, cartas);
}
