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
#define MAXSOLDOMICILIO 4;

//Semáforos
//	mutex_log, para controlar la escritura en los logs.
//	mutex_clientes, para controlar el acceso a la lista de clientes y su modificacion.
//	mutex_solicitudes, para controlar el acceso al contador de solicitdes domiciliarias.
//	mutex_terminarPrograma, para controlar la variable terminar programa.
pthread_mutex_t mutex_log, mutex_clientes, mutex_solicitudes, mutex_terminarPrograma;

//Creamos variable de condicion. Asociadas a un mutex.
//	cond_clienteAtendido, asociado al mutex_clientes, espera a la señal del técnico.
//	cond_domiciliaria, asociado co_solicitudes, ???.
pthread_cond_t cond_clienteAtendido, cond_domiciliaria;

//Variables
//	cliRed, controlar numero de clientes de red. (Para dar numero al id)
//	cliApp, controlar numero de clientes de app. (Para dar numero al id)
//	cliSolicitud, controlar numero de solicitudes para atencion domiciliaria.
//	contCliCola, controla el numero de clientes en cola.
//	terminarPrograma, variable para manejar el fin del programa.
int contCliRed, contCliApp, contCliSolicitud, contCliCola, terminarPrograma;

struct cliente{
	//Tipo de cliente
	char id[10];
	//Valores: 0, sin atender; 1, siendo atendido; 2, atendido.
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
//nuevoCliente, crea nuevos clientes y los añade a la lista de clientes.
void nuevoCliente(int sig);

//	accionesCliente, maneja las acciones de los clientes.
void *accionesCliente(void *arg);

//	accionesTecnico, maneja las acciones de los tecnicos.
void *accionesTecnico(void *arg);

//	accionesTecnicoDomiciliario, maneja las acciones de los tecnicos domiciliarios.
void *accionesTecnicoDomiciliario(void *arg);

//	accionesEncargado, maneja las acciones de los encargados.
void *accionesEncargado(void *arg);

//	terminarPrograma, da valor de 1 a la variable terminarPrograma para no aceptar más solicitudes.
void terminarPrograma(int sig);

//	reordenarLista, con el algortimo de la burbuja, reordena la lista cada vez que entra un cliente en la misma por prioridad.
void reordenarLista();

//	writeLogMessage, metodo para escribir en logFile.
void writeLogMessage(char *id, char *msg);

//	calcularAleatorio, metodo para calcular un numero aleatorio comprendido entre el primer y el segundo parámetro incluidos.
int calcularAleatorio(int min, int max);

//	buscarCliente, busca el cliente en la lista de clientes por su id.
int buscarCliente(char *id);

//	eliminarCliente, elimina el cliente pasado por parametro por su id. Intercambiando posiciones con el cliente a la derecha y poniendo a NULL el últmo.
void eliminarCliente(char *id);

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
		printf("SISTEMA: Terminando programa. Solicitud de nuevo cliente rechazada.\n");
		writeLogMessage("*SISTEMA*: ", "Terminando programa. Solicitud de nuevo cliente rechazada.");

	//Si la variable es distinta de 1, quiere decir que es 0(ejecutando) o >1(matando hilos) y pueden seguir llegando solicitudes.
	}else{
		pthread_mutex_unlock(&terminarPrograma);

		//Comprobamos si hay sitio en la cola.
		pthread_mutex_lock(&contCliCola);

		//Lista de clientes con sitios libres.
		if(contCliCola < 20){
			pthread_mutex_unlock(&contCliCola);

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
				clientes[contCliCola] = clienteApp;
				pthread_mutex_unlock(&mutex_clientes);

				//Reordenamos la lista en funcion de la prioridad.
				reordenarListaClientes();

				//Inicializamos y creamos hilo para el nuevo cliente de APP.
				pthread_t nuevoCliente;
				pthread_create(&nuevoCliente, NULL, accionesCliente, (void*)nuevoIdApp);

				//Imprimimos por pantalla y escribimos en el log.
				printf("SISTEMA: Nuevo cliente de APP añadido a la lista.\n");
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
				clientes[contCliCola] = clienteRed;
				pthread_mutex_unlock(&mutex_clientes);

				//Reordenamos la lista en funcion de la prioridad.
				reordenarListaClientes();

				//Inicializamos y creamos hilo para el nuevo cliente de RED.
				pthread_t nuevoCliente;
				pthread_create(&nuevoCliente, NULL, accionesCliente, (void*)nuevoIdRed);

				//Imprimimos por pantalla y escribimos en el log.
				printf("SISTEMA: Nuevo cliente de RED añadido a la lista.\n");
				writeLogMessage("*SISTEMA*: ", "Nuevo cliente de RED añadido a la lista.");
			}

		//Lista de clientes llena.	
		}else{
			pthread_mutex_unlock(&contCliCola);
			printf("SISTEMA: Lista de clientes llena. Solicitud de nuevo cliente rechazada.\n");
			writeLogMessage("*SISTEMA*: ", "Lista de clientes llena. Solicitud de nuevo cliente rechazada.");

		}
	}
}

void *accionesCliente(void *arg){

	char *id = *(char*)arg;
	int atendido = 0;
	int tipo = -1;
	int posicion = 0;
	int espera = 0;

	//Guardamos en log hora y cliente. 
	printf("%s: Nueva solicitud aceptada.\n", id);
	writeLogMessage(id, "Nueva solicitud aceptada.");

	pthread_mutex_lock(&mutex_clientes);
	posicion = buscarCliente(id);
	atendido = clientes[posicion].atendido;
	tipo = clientes[posicion].tipo;
	pthread_mutex_unlock(&mutex_clientes);

	do{

	int numAleatorio = 0;

		//Comprobamos si está siendo atendido el cliente. No está siendo atendido ni ha sido atendido.
		if(atenido == 0){

			numAleatorio = calcularAleatorio(1,100);

			//Pierden interes y abandonan.
			if(numAleatorio <= 10){

				//Escribimos en el log que abandonan.
				printf("%s: Encuentro dificil la aplicacion. Abandono.\n", id);
				writeLogMessage(id, "Encuentro dificil la aplicacion. Abandono.");

				//Elimiamos al cliente de la cola.
				pthread_mutex_lock(&mutex_clientes);
				eliminarCliente(id);
				pthread_mutex_unlock(&mutex_clientes);

				//Matamos al hilo del cliente.
				pthread_exit(NULL);

			//Se cansan de esperar y abandonan.
			}else if(numAleatorio <=30 && espera == 0){

				//Escribimos en el log que abandona.
				printf("%s: Me he cansado de esperar. Abandono.\n", id);
				writeLogMessage(id, "Me he cansado de esperar. Abandono.");

				//Eliminamos al cliente de la cola.
				pthread_mutex_lock(&mutex_clientes);
				eliminarCliente(id);
				pthread_mutex_unlock(&mutex_clientes);

				//Matamos al hilo del cliente.
				pthread_exit(NULL);
				
			//Pierden conexion y abandonan.
			}else if(numAleatorio <=35){

				//Escribimos en el log que abandona.
				printf("%s: Ha perdido la conexion. Abandono.\n", id);
				writeLogMessage(id, "Ha perdido la conexion. Abandono.");

				//Eliminamos al cliente de la cola de clientes.
				pthread_mutex_lock(&mutex_clientes);
				eliminarCliente(id);
				pthread_mutex_unlock(&mutex_clientes);

				//Matamos al cliente.
				pthread_exit(NULL);
			}
			sleep(2);
			cansado = (cansado+2)%8;
		}
	//Mientras no estén siendo atendidos o no hayan sido atendidos...
	}while(atendido == 0);

	//Si están siendo atendidos...
	if(atendido == 1){

		printf("%s: Estoy siendo atendido.\n", id);
		writeLogMessage(id, "Estoy siendo atendido.");	

		//Espera por señal del tecnico. (Similar a un pause).
		pthread_cond_wait(&cond_clienteAtendido, &mutex_clientes);
	}


	if(tipo == 1){

		int numAleatorioRed = calcularAleatorio(1, 100);

		if(numAleatorioRed <=30){

			printf("%s: Solicito atencion domiciliaria.\n", id);
			writeLogMessage(id, "Solicito atencion domiciliaria.");

			//0, fuera de lista domiciliaria; 1, dentro lista.
			int dentroListaDom = 0;

			do{
				//Comprobamos el número de solicitudes domiciliarias pendientes.
				pthread_mutex_lock(&mutex_solicitudes);

				//Si son menos de 4, incrementamos el valor en unoy será atendido en el domicilio.(???)
				if(contCliSolicitud<AXSOLDOMICILIO){
					contCliSolicitud++;
					pthread_mutex_unlock(&mutex_solicitudes);

					dentroListaDom = 1;

					//I, Escribimos en el log que espera para ser atendido.
					printf("%s: Espero para ser atendido por atencion domiciliaria.\n", id);
					writeLogMessage(id, "Espero para ser atendido por atencion domiciliaria.");

					//II, Cambiamos el valor de solcitud por 1.
					pthread_mutex_lock(&mutex_clientes);
					posicion = buscarCliente(id);
					clientes[posicion].solicitud = 1;
					pthread_mutex_unlock(&mutex_clientes);

					//III, Si es el cuarto, avisa al técnico.
					pthread_mutex_lock(&mutex_solicitudes);

					if(contCliSolicitud == MAXSOLDOMICILIO){
						pthread_mutex_unlock(&mutex_solicitudes);
						pthread_cond_signal(&cond_domiciliaria, &mutex_solicitudes);
					}else{
						pthread_mutex_unlock(&mutex_solicitudes);
					}

					//IV, Bloqueamos a la espera de que clientes[posicion].solicitud=0 (Ya ha finalizado su atencion.
					pthread_cond_wait(&cond_domiciliaria, &mutex_solicitudes);

					//V, Comunicamos que su atención ha finalizado.
					printf("%s: Fin de atencion domiciliaria.\n", id);
					writeLogMessage(id, "Fin de atencion domiciliaria.");

				}else{
					//Si el numero de solicitudes pendientes de atencion dom. es mayor que 4, 
					pthread_mutex_unlock(&mutex_solicitudes);
					sleep(3);
				}

			//???
			while(dentroListaDom == 0);
		}

		//Eliminamos al cliente de la cola de clientes.
		pthread_mutex_lock(&mutex_clientes);
		eliminarCliente(id);
		pthread_mutex_unlock(&mutex_clientes);

		//Matamos al hilo de cliente.
		pthread_exit(NULL);
	}
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

	for(int i=0; i<contCliCola; i++){
		for(int j=0; j<contCliCola-1; j++){

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

int buscarCliente(char *id){

	for(int i=0; i<contCliCola; i++){
		if(strcmp(clientes[i].id, id) == 0){
			return i;
		} 
	}
	return 99;
}

void eliminarCliente(char *id){

	int posicion = buscarCliente(id);
	struct cliente ultimo = [NULL, 0, 0, 0, 0];

	for(int i=posicion; i<contCliCola-1; i++){
		clientes[i] = clientes[i+1];
	}
	clientes[contCliCola-1] = ultimo;
}