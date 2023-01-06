// Practica Final SSOO_2022 G12
//
// Integrantes:	Ángel Martínez Fernández (C)
// 				Saad Ali Hussain Kausar
// 				Javier Álvarez Pintor

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>

#define NUMCLIENTES 20;

//Semáforos
//	mutex_log, para controlar la escritura en los logs.
//	mutex_clientes, para controlar el acceso a la lista de clientes y su modificacion.
//	mutex_solicitudes, para controlar el acceso al contador de solicitdes.
//	mutex_terminarPrograma, para controlar la variable terminar programa.
pthread_mutex_t mutex_log, mutex_clientes, mutex_solicitudes, mutex_terminarPrograma;

//Contadores
//	cliRed, controlar numero de clientes de red. (Para dar numero al id)
//	cliApp, controlar numero de clientes de app. (Para dar numero al id)
//	cliSol, controlar numero de solicitudes.
//	terminarPrograma, variable para manejar el fin del programa.
//	clientesEnCola, controla el numero de clientes en cola para el metodo de reordenacion.
int contCliRed, contCliApp, contCliSolicitud, terminarPrograma, clientesEnCola;

struct cliente{
	//Prioridad se asigna por FIFO.
	int prioridad;
	//Valores: 0, sin atender; 1, siendo atendido; 2, sin atender.
	int atendido;
	//Valores: 0, no solicitud; 1, solicitud enviada.
	int solicitud;
	//Tipo de cliente.
	int tipo;
	char id[10];
}

//Inicializado lista de clientes
struct cliente *clientes;

//Fichero log donde se escribiran las acciones
File *logFile;
const char *NombreFichero = "RegistroAverias"

//Declaramos funciones
//	nuevoCliente
void nuevoCliente(int sig);
void *accionesCliente(void *arg);
void *accionesTecnico(void *arg);
void *accionesTecnicoDomiciliario(void *arg);
void *accionesEncargado(void *arg);
void terminarPrograma(int sig);
void reordenarLista();
void writeLogMessage();

int main (int argc, char *argv[]){

	//Creamos hilos como variables locales
	pthread_t tecnico_1, tecnico_2, resprep_1, resprep_2, encargado, atencionDom;

	//Iniciamos semáforos
	pthread_mutex_init(&mutex_log, NULL);
	pthread_mutex_init(&mutex_clientes, NULL);
	pthread_mutex_init(&mutex_solicitudes, NULL);

	//Iniciamos hilos
	pthread_create(&tecnico_1, NULL, funcion, NULL);
	pthread_create(&tecnico_2, NULL, funcion, NULL);
	pthread_create(&resprep_1, NULL, funcion, NULL);
	pthread_create(&resprep_2, NULL, funcion, NULL);
	pthread_create(&encargado, NULL, funcion, NULL);
	pthread_create(&atencionDom, NULL, funcion, NULL);

	//Armar señales
	struct sigaction ss = {0};

	ss.sa_handler = nuevoCliente;
	sigaction (SIGUSR1, &ss, NULL);
	sigaction (SIGUSR2, &ss, NULL);

	ss.sa_handler = terminarPrograma;
	sigaction (SIGINT, &SS, NULL);

	//Inicializamos valores de contadores
	contCliRed = 0;
	contCliApp = 0;
	contCliSolicitud = 0;
	terminarPrograma = 0;

	//Inicializar de forma dinamica el tamaño de la lista de clientes.
	clientes = (struct cliente*) malloc(NUMCLIENTES*sizeof(struct cliente));

	for(int i=0; i<NUMCLIENTES; i++){
		clientes[i].id = NULL;
		//	prioridad: valor [1-10] aleatoriamente
		clientes[i].prioridad = 0;
		//	solicitud: 0, no ha solicitado atencion domiciliaria; 1, ha solicitado.
		clientes[i].solicitud = 0;
		//	estado: 0, sin atender; 1, siendo atendido.
		clientes[i].estado = 0;
		//	tipo: 0, app; 1, red.
		clientes[i].tipo = 0;
	}

	//Inicializar fichero log
	logFile = fopen("RegistroAverias.log", "w");
	writeLogMessage("*SISTEMA*: ", "NUEVA EJECUCIÓN");
	fclose(fichero);
}

void reordenarListaClientes(){

	struct clienteAux;

	for(int i=0; i<clientesEnCola; i++){
		for(int j=0; j<clientesEnCola-1; j++){

			int k = j+1;

			if(clientes[j].prioridad > clientes[k].prioridad){
				clienteAux = clientes[j];
				clientes[j] = clientes[k];
				clientes[k] = clienteAux;
			}
		}
	}
}

void writeLogMessage(char *id, char *msg) {

	// Calculamos la hora actual
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[25];
	strftime(stnow, 25, "%d/%m/%y %H:%M:%S", tlocal);

	// Escribimos en el log
	logFile = fopen(logFileName, "a");
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
	fclose(logFile);
}