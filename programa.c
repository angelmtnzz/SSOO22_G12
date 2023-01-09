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

//Semaforos para controlar a los empleados segun el cliente a atender
pthread_mutex_t mutex_Clientes;

//Semaforo para controlar el acceso al fichero Log
pthread_mutex_t mutex_Log;

//Semaforo para controlar los clientes que esperan la atencion domiciliaria
pthread_mutex_t mutex_Solicitudes;

//Semaforo para controlar la variable terminarPrograma
pthread_mutex_t terminarPrograma;

//Variable de condicion asociada al semaforo de cliente
pthread_cond_t cond_clienteAtendido;

//Variable de condicion asociada al semaforo de Solicitudes Domiciliarias
pthread_cond_t cond_domiciliaria;

/* VARIABLES GLOBALES
 * contCliRed, contador de clientes de red. (Para dar numero al id)
 * contCliApp, contador de clientes de app. (Para dar numero al id)
 * contCliSol, contador de clientes que solicitan atencion domiciliaria.
 * contCliCola, contador de clientes en cola 
 * terminarPrograma, variable para manejar el fin del programa.
 */
int contCliRed, contCliApp, contCliSolicitud, contCliCola, terminarPrograma;

struct cliente{
	//id sera una cadena de caracteres
	char id[10];

	/* atendido sera 0 si el cliente no está atendido
	 * , sera 1 si el cliente esta siendo atendido
	 * y sera 2 si ya ha sido atendido
	 */
	int atendido;

	/* tipo sera 0 si cliente es de tipo app
	 * y sera 1 si es de tipo red
	 */
	int tipo;

	/* solicitud sera un 0 si el cliente no solicita
	 * atencion domiciliaria
	 * y sera 1 si la ha solicitado
	 */
	int solicitud;

	/* los clientes tendran un numero de prioridad
	 * aleatorio entre 1 y 10
	 */
	int prioridad;
}

//Declaracion de lista de clientes
struct cliente *clientes;

//Fichero log donde se escribiran las acciones correspondientes
FILE *logFile;
const char *NombreFichero = "registroAverias.log";


//Declaramos las funciones a utilizar para la practica

//nuevoCliente, crea nuevos clientes y los añade a la lista de clientes.
void nuevoCliente(int sig);

//accionesCliente, maneja las acciones de los clientes.
void *accionesCliente(void *arg);

//accionesTecnico, maneja las acciones de los tecnicos.
void *accionesTecnico(void *arg);

//accionesTecnicoDomiciliario, maneja las acciones del tecnico domiciliario.
void *accionesTecnicoDomiciliario(void *arg);

//accionesEncargado, maneja las acciones del encargado.
void *accionesEncargado(void *arg);

/* terminarPrograma, da valor de 1 a la variable terminarPrograma
 * para no aceptar más solicitudes.
 */
void terminarPrograma(int sig);

/* reordenarLista, con el algortimo de la burbuja, reordena la 
 * lista por prioridad cada vez que entra un cliente en la misma.
 */
void reordenarLista();

//writeLogMessage, metodo para escribir en logFile.
void writeLogMessage(char *id, char *msg);

//calcularAleatorio, metodo para calcular un numero aleatorio comprendido entre el primer y el segundo parámetro incluidos.
int calcularAleatorio(int min, int max);

//buscarCliente, busca el cliente en la lista de clientes por su id.
int buscarCliente(char *id);

/* eliminarCliente, elimina el cliente pasado por parametro por su id.
 * Intercambiando posiciones con el cliente a la derecha y poniendo a NULL
 * el último.
 */
void eliminarCliente(char *id);

//Inicio del main 
int main (int argc, char *argv[]){

	//Iniciamos semilla para aleatorios
	srand(time(NULL));

	//Declaracion de hilos como variables locales
	phtread_t tecnico_1, tecnico_2;
	phtread_t respRep_1, respRep_2;
	phtread_t encargado, atencionDom;
	
	//Inicializacion de los MUTEX
	pthread_mutex_init(&mutex_Log, NULL);
	pthread_mutex_init(&mutex_Clientes, NULL);
	pthread_mutex_init(&mutex_Colicitudes, NULL);
	pthread_mutex_init(&mutex_TerminarPrograma, NULL);

	//Iniciamos hilos
	pthread_create(&tecnico_1, NULL, accionesTecnico, NULL);
	pthread_create(&tecnico_2, NULL, accionesTecnico, NULL);
	pthread_create(&respRep_1, NULL, accionesTecnico, NULL);
	pthread_create(&respRep_2, NULL, accionesTecnico, NULL);
	pthread_create(&encargado, NULL, accionesEncargado, NULL);
	pthread_create(&atencionDom, NULL, accionesTecnicoDomiciliario, NULL);

	//Armar señales
	struct sigaction ss;

	ss.sa_handler = nuevoCliente;
	sigaction (SIGUSR1, &ss, NULL);
	sigaction (SIGUSR2, &ss, NULL);

	ss.sa_handler = terminarPrograma;
	sigaction (SIGINT, &SS, NULL);

	//inicializamos contadores declarados de manera global
	contCliRed = 0;
	contCliApp = 0;	
	contCliSolicitud = 0;
	terminarPrograma = 0;

	//Definimos el tamaño de la lista clientes de forma dinamica
	clientes = (struct cliente*) malloc(NUM_CLIENTES*sizeOf(struct cliente));

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

		//Espera por señal del tecnico. (Similar a un pause). Siempre asociados a un mutex.	
		//	Wait debe realizarse con mutex cerrado.
		//	El hilo se suspende hasta que otro señaliza la condición y el mutex asociado se desbloquea.
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

	char *id = *(char*)arg;
	struct cliente clientePorAtender = [NULL, 0, 0, 0, 0];

	//Tipo de llamada para tiempos de atencion.
	int tipoDeLlamada = calcularAleatorio(1, 10);
	int tiempoDeAtencion = 1;

	int clienteEncontradoTecnico = 0;

	do{

		//1. Buscar cliente para atender de su tipo, atendiendo a prioridad y sino FIFO.
		pthread_mutex_lock(&mutex_clientes);

		pthread_mutex_unlock(&mutex_clientes);


	}while(clienteEncontradoTecnico);
	sleep(1);

	//2. Cambiamos el flag de atendido.
	pthread_mutex_lock(&mutex_clientes);
	clientePorAtender.atendido = 1;
	pthread_mutex_unlock(&mutex_clientes);

	//3. Calculamos el tiempo de atención.
	tiempoDeAtencion = calcularTiempoAtencion(tipoDeLlamada);

	//4. Guardamos en el log que comienza la atendion.
	printf("%s: Comenzamos la atención al cliente.\n", id);
	writeLogMessage(id, "Comenzamos la atención al cliente.");

	//5. Dormimos el tiempo de atención.
	sleep(tiempo de tiempoDeAtencion);

	//6 y 7. Guardamos en el log que finaliza la atencion y el motivo.
	printf("%s: Finalizamos la atención al cliente.\n", id);
	writeLogMessage(id, "Finalizamos la atención al cliente.");

	if(tipoDeLlamada <= 8){
		printf("%s: Motivo: Todo en orden.\n", id);
		writeLogMessage(id, "Motivo: Todo en orden.");
	}else if(tipoDeLlamada <= 9){
		printf("%s: Motivo: Cliente mal identificado.\n", id);
		writeLogMessage(id, "Motivo: Cliente mal identificado.");
	}else{
		printf("%s: Motivo: Compañia erronea.\n", id);
		writeLogMessage(id, "Motivo: Compañia erronea.");
	}

	//8. Cambiamos el flag de atendido  
	pthread_mutex_lock(&mutex_clientes);
	clientePorAtender.atendido = 2;
	pthread_mutex_unlock(&mutex_clientes);

	//9. Mira si toca descanso. (Cada 5 clientes atendidos).

	//10. Vuelta a buscar el siguiente.

}

void *accionesTecnicoDomiciliario(void *arg){

	char *id = *(char*)arg;

	//1. Comprueba el numero de solicitudes y se queda bloqueado (cond_wait) mientras sea menor que 4.

	//2. Guardamos en el log que comienza la atencion.
	printf("%s: Comenzamos la atención al cliente.\n", id);
	writeLogMessage(id, "Comenzamos la atención al cliente.");

	//3. Duerme un segundo por petición.

	//4. Guardamos en log que hemos atendido a uno.
	printf("%s: Atendido un cliente.\n", id);
	writeLogMessage(id, "Atendido un cliente.");

	//5. Cambia el flag de solicitud a 0.
	pthread_mutex_lock(&);

	pthread_mutex_unlock(&);

	//6. Cuando el ultimo es atendido, contCliSolicitud=0;

	//7. Guardamos en el log que se ha finalizado la atencion domiciliaria.
	printf("%s: Finalizada atencion domiciliaria.\n", id);
	writeLogMessage(id, "Finalizada atencion domiciliaria.");

	//8. Se avisa a los que esperaban por solicitud domiciliaria que se ha finalizado la atencion.

	//9. Volvemos al 1.
}

void *accionesEncargado(void *arg){

	char *id = *(char*)arg;
	struct cliente clientePorAtenderEncargado = [NULL, 0, 0, 0, 0];

	int clienteEncontradoEncargado = 0;

	int tipoDeLlamadaEncargado = calcularAleatorio(1, 10);
	int tiempoDeAtencionEncargado = 1;

	do{

		//1. Busca cliente para atender. (1. red, 2. app, 3. prioridad, 4. FIFO).

	}while(clienteEncontradoEncargado == 0);
	sleep(3);

	//2. Cambiamos el flag de atendido.
	pthread_mutex_lock(&mutex_clientes);
	clientePorAtenderEncargado.atendido = 1;
	pthread_mutex_unlock(&mutex_clientes);

	//3. Calculamos el tiempo de atencion.
	tiempoDeAtencionEncargado = calcularTiempoAtencion(tipoDeLlamadaEncargado);

	//4. Guardamos en el log que comienza la atendion.
	printf("%s: Comenzamos la atención al cliente.\n", id);
	writeLogMessage(id, "Comenzamos la atención al cliente.");

	//5. Dormimos el tiempo de atencion.
	sleep(tiempoDeAtencionEncargado);

	//6 y 7. Guardamos en el log que finaliza la atencion y el motivo.
	printf("%s: Finalizamos la atención al cliente.\n", id);
	writeLogMessage(id, "Finalizamos la atención al cliente.");

	if(tipoDeLlamada <= 8){
		printf("%s: Motivo: Todo en orden.\n", id);
		writeLogMessage(id, "Motivo: Todo en orden.");
	}else if(tipoDeLlamada <= 9){
		printf("%s: Motivo: Cliente mal identificado.\n", id);
		writeLogMessage(id, "Motivo: Cliente mal identificado.");
	}else{
		printf("%s: Motivo: Compañia erronea.\n", id);
		writeLogMessage(id, "Motivo: Compañia erronea.");
	}

	//8. Cambiamos flag atendido.
	pthread_mutex_lock(&mutex_clientes);
	clientePorAtenderEncargado.atendido = 2;
	pthread_mutex_unlock(&mutex_clientes);

	//9. Vuelta al paso 1 y seguimos buscando.
}

void terminarPrograma(int sig){
	pthread_mutex_lock(&mutex_terminarPrograma);
	//Variable terminarPrograma con valor 1, no deja que lleguen más solicitudes, prepara el final de programa.
	terminarPrograma = 1;
	pthread_mutex_unlock(&mutex_terminarPrograma);

}

void reordenarListaClientes(){

	pthread_mutex_lock(&mutex_clientes);

	struct cliente clienteAux = [NULL, 0, 0, 0, 0];

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

int calcularTiempoAtencion(int tipoDeLlamada){

	int tiempoDeAtencion = 0;

	//	Todo en regla, 1-4s y siguen.
	if(tipoDeLlamada <= 8){
		tiempoDeAtencion = calcularAleatorio(1,4);

	//	Mal identificados, 2-6s y siguen.
	}else if(tipoDeLlamada <= 9){
		tiempoDeAtencion = calcularAleatorio(2,6);

	// Compañia erronea, 1-2s y abandonan.
	}esle{
		tiempoDeAtencion = calcularAleatorio(1,2);
	}

	return tiempoDeAtencion;
}
