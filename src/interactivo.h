/*
 * interactivo.h
 *
 *  Created on: 24 ago. 2016
 *      Author: alumnopp
 */

#ifndef SRC_INTERACTIVO_H_
#define SRC_INTERACTIVO_H_

#include "estructuras.h"
#include "mpi.h"

int I_quiero_mus(Carta *mi_mano, MPI_Comm global_comm);
int I_get_descartes(Carta **mi_mano, Carta **mis_descartes, MPI_Comm global_comm);
void I_envidar(Envite *envites, int ultimo_envite, int tantos, int postre, Carta *mi_mano, MPI_Comm global_comm);

#endif /* SRC_INTERACTIVO_H_ */
