# PP2015
Práctica Procesamiento Paralelo 2015/16

## Juego del Mus

El objetivo de la práctica era desarrollar una versión del popular juego de cartas "Mus" utilizando las librerías MPI de C, de forma que cada jugador esté representado por un proceso independiente de los demás y que interactúen entre ellos mediante el paso de mensajes.

El juego puede ser interactivo, con la participación de un jugador humano, o totalmente automático.

## Compilación

Dentro del directorio con el código fuente se encuentra un archivo de configuración
para la compilación mediante la utilidad make. Para compilar el proyecto basta con
ejecutar, dentro del directorio /src, el comando "make" o "make all". Por ejemplo:

    alumnopp@VBoxPP:~/src$ make all
    
## Ejecución

El programa puede iniciarse directamente ejecutando el binario Mus generado tras
la compilación:

    alumnopp@VBoxPP:~/bin$ ./Mus

El problema que surge es que he notado que hacerlo de este modo supone que la
salida se desordene de forma exagerada. Por lo tanto, mi recomendación sería que
se ejecutara mediante mpirun:

    alumnopp@VBoxPP:~/bin$ mpirun -np 1 Mus

De este modo mpirun lanza un único proceso del ejecutable Mus, y la salida
se muestra de forma ordenada en la mayoría de las situaciones.
