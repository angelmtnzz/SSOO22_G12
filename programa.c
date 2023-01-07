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
#include <string.h>

#define NUMCLIENTES 20;

//Semáforos
//	mutex_log, para controlar la escritura en los logs.
//	mutex_clientes, para controlar el acceso a la lista de clientes y su modificacion.
//	mutex_solicitudes, para controlar el acceso al contador de solicitdes.
//	mutex_terminarPrograma, para controlar la variable terminar programa.
pthread_mutex_t mutex_log, mutex_clientes, mutex_solicitudes, mutex_terminarPrograma;

//Variables
//	cliRed, controlar numero de clientes de red. (Para dar numero al id)
//	cliApp, controlar numero de clientes de app. (Para dar numero al id)
//	cliSol, controlar numero de solicitudes.
//	terminarPrograma, variable para manejar el fin del programa.
//	clientesEnCola, controla el numero de clientes en cola.
int contCliRed, contCliApp, contCliSolicitud, terminarPrograma, clientesEnCola;

struct cliente{
	//Tipo de cliente
	char id[10];
	//Valores: 0, sin atender; 1, siendo atendido; 2, sin atender.
	int atendido;
	//Tipo de cliente.
	int tipo;
	//Valores: 0, no solicitud; 1, solicitud enviada.
	int solicitud;
	//Prioridad se asigna por FIFO.
	int prioridad;
}

//Inicializado lista de clientes
struct cliente *clientes;

//Fichero log donde se escribiran las acciones
File *logFile;
const char *NombreFichero = "RegistroAverias"

//Declaramos funciones
//	nuevoCliente, crea nuevos clientes y los añade a la lista de clientes.
//	accionesCliente, maneja las acciones de los clientes.
//	accionesTecnico, maneja las acciones de los tecnicos.
//	accionesTecnicoDomiciliario, maneja las acciones de los tecnicos domiciliarios.
//	accionesEncargado, maneja las acciones de los encargados.
//	terminarPrograma, da valor de 1 a la variable terminarPrograma para no aceptar más solicitudes.
//	reordenarLista, con el algortimo de la burbuja, reordena la lista cada vez que entra un cliente en la misma por prioridad.
//	writeLogMessage, metodo para escribir en logFile.
//	calcularAleatorio, metodo para calcular un numero aleatorio comprendido entre el primer y el segundo parámetro incluidos.
void nuevoCliente(int sig);
void *accionesCliente(void *arg);
void *accionesTecnico(void *arg);
void *accionesTecnicoDomiciliario(void *arg);
void *accionesEncargado(void *arg);
void terminarPrograma(int sig);
void reordenarLista();
void writeLogMessage();
int calcularAleatorio(int min, int max);

int main (int argc, char *argv[]){

	//Iniciamos semilla para aleatorios
	srand(time(NULL));

	//Creamos hilos como variables locales
	pthread_t tecnico_1, tecnico_2, resprep_1, resprep_2, encargado, atencionDom;

	//Iniciamos semáforos
	pthread_mutex_init(&mutex_log, NULL);
	pthread_mutex_init(&mutex_clientes, NULL);
	pthread_mutex_init(&mutex_solicitudes, NULL);
	pthread_mutex_init(&mutex_terminarPrograma, NULL);

	//Iniciamos hilos
	pthread_create(&tecnico_1, NULL, accionesTecnico, NULL);
	pthread_create(&tecnico_2, NULL, accionesTecnico, NULL);
	pthread_create(&resprep_1, NULL, accionesTecnico, NULL);
	pthread_create(&resprep_2, NULL, accionesTecnico, NULL);
	pthread_create(&encargado, NULL, accionesEncargado, NULL);
	pthread_create(&atencionDom, NULL, accionesTecnicoDomiciliario, NULL);

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

	//Espera infinita para señales.
	while(1){

		pthread_mutex_lock(&mutex_terminarPrograma);

		//Mientras terminarPrograma tenga valor 0, seguimos ejecutando el programa.
		if(terminarPrograma == 0){
			pthread_mutex_unlock(&mutex_terminarPrograma);
			pause();

		//Para terminar el programa, los hilos incrementarán el contador terminarPrograma desde su valor=1 hasta 7 (6 hilos).	
		}else if(terminarPrograma == 7){
			pthread_mutex_unlock(&mutex_terminarPrograma);
			writeLogMessage("*SISTEMA*: ", "FIN EJECUCIÓN");
			exit(0);
		}
	}
}

void nuevoCliente(int sig){

	pthread_mutex_lock(&terminarPrograma);
	//Comprobamos si puden llegar más solicitudes.
	if(terminarPrograma==1){
		pthread_mutex_unlock(&terminarPrograma);
		printf("SISTEMA: Terminando programa. Solicitud de nuevo cliente rechazada.");
		writeLogMessage("*SISTEMA*: ", "Terminando programa. Solicitud de nuevo cliente rechazada.");

	//Si la variable es distinta de 1, quiere decir que es 0(ejecutando) o >1(matando hilos) y pueden seguir llegando solicitudes.
	//																	 ???????????????????
	}else{
		pthread_mutex_unlock(&terminarPrograma);

		//Comprobamos si hay sitio en la cola.
		pthread_mutex_lock(&clientesEnCola);

		//Lista de clientes con sitios libres.
		if(clientesEnCola < 20){
			pthread_mutex_unlock(&clientesEnCola);

			if(sig == SIGUSR1){
				//Incrementamos contador de clientes de APP.
				contCliApp++;
				//Creamos ID para el nuevo cliente de APP.
				char nuevoIdApp[10];
				sprintf(nuevoIdApp, "cliApp_%d", contCliApp);
				//Generamos una prioridad para el nuevo cliente de APP.
				int prioridadApp = calcularAleatorio(1, 10);
				//Creamos el nuevo cliente de APP y añadimos sus parametros establecidos por el momento.
				struct cliente clienteApp = [nuevoIdApp, 0, 0, 0, prioridadApp];
				//Añadimos el nuevo cliente de APP a la lista de clientes.
				pthread_mutex_lock(&mutex_clientes);
				clientes[clientesEnCola] = clienteApp;
				pthread_mutex_unlock(&mutex_clientes);
				//Reordenamos la lista en funcion de la prioridad.
				reordenarListaClientes();
				//Inicializamos y creamos hilo para el nuevo cliente de APP.
				pthread_t nuevoCliente;
				pthread_create(&nuevoCliente, NULL, accionesCliente, (void*)nuevoIdApp);
				//Imprimimos por pantalla y escribimos en el log.
				printf("SISTEMA: Nuevo cliente de APP añadido a la lista.");
				writeLogMessage("*SISTEMA*: ", "Nuevo cliente de APP añadido a la lista.");

			}else if(sig == SIGUSR2){
				//Incrementamos contador de clientes de RED.
				contCliRed++;
				//Creamos ID para el nuevo cliente de RED.
				char nuevoIdRed[10];
				sprintf(nuevoIdRed, "cliRed_%d", contCliRed);
				//Generamos una prioridad para el nuevo cliente de RED.
				int prioridadRed = calcularAleatorio(1, 10);
				//Creamos el nuevo cliente de RED y añadimos sus parametros establecidos por el momento.
				struct cliente clienteRed = [nuevoIdRed, 0, 1, 0, prioridadRed];
				//Añadimos el nuevo cliente de RED a la lista de clientes.
				pthread_mutex_lock(&mutex_clientes);
				clientes[clientesEnCola] = clienteRed;
				pthread_mutex_unlock(&mutex_clientes);
				//Reordenamos la lista en funcion de la prioridad.
				reordenarListaClientes();
				//Inicializamos y creamos hilo para el nuevo cliente de RED.
				pthread_t nuevoCliente;
				pthread_create(&nuevoCliente, NULL, accionesCliente, (void*)nuevoIdRed);
				//Imprimimos por pantalla y escribimos en el log.
				printf("SISTEMA: Nuevo cliente de RED añadido a la lista.");
				writeLogMessage("*SISTEMA*: ", "Nuevo cliente de RED añadido a la lista.");
			}

		//Lista de clientes llena.	
		}else{
			pthread_mutex_unlock(&clientesEnCola);
			printf("SISTEMA: Lista de clientes llena. Solicitud de nuevo cliente rechazada.");
			writeLogMessage("*SISTEMA*: ", "Lista de clientes llena. Solicitud de nuevo cliente rechazada.");

		}
	}
}

void *accionesCliente(void *arg){

}

void *accionesTecnico(void *arg){

}

void *accionesTecnicoDomiciliario(void *arg){

}

void *accionesEncargado(void *arg){

}

void terminarPrograma(int sig){
	pthread_mutex_lock(&mutex_terminarPrograma);
	//Variable terminarPrograma con valor 1, no deja que lleguen más solicitudes, prepara el final de programa.
	terminarPrograma = 1;
	pthread_mutex_unlock(&mutex_terminarPrograma);

}

void reordenarListaClientes(){

	pthread_mutex_lock(&mutex_clientes);

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
	pthread_mutex_unlock(&mutex_clientes);
}

void writeLogMessage(char *id, char *msg) {

	// Calculamos la hora actual
	time_t now = time(0);
	struct tm *tlocal = localtime(&now);
	char stnow[25];
	strftime(stnow, 25, "%d/%m/%y %H:%M:%S", tlocal);

	// Escribimos en el log
	pthread_mutex_lock(&mutex_log);
	logFile = fopen(logFileName, "a");
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
	fclose(logFile);
	pthread_mutex_unlock(&mutex_log);
}

int calcularAleatorio(int min, int max){
	return rand()%(max-min+1)+min;
}