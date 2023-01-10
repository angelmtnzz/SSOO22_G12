#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include<stdbool.h>
#include<string.h>

#define NUMCLIENTES 20
#define MAXSOLDOMICILIO 4

//Semaforos para controlar a los empleados segun el cliente a atender
pthread_mutex_t mutex_clientes;

//Semaforo para controlar el acceso al fichero Log
pthread_mutex_t mutex_log;

//Semaforo para controlar los clientes que esperan la atencion domiciliaria
pthread_mutex_t mutex_solicitudes;

//Semaforo para controlar la variable varTerminarPrograma
pthread_mutex_t mutex_terminarPrograma;

//Variable de condicion asociada al semaforo de cliente
pthread_cond_t cond_cliAtendido;

//Variable de condicion asociada al semaforo de Solicitudes Domiciliarias
pthread_cond_t cond_domiciliaria;

/* VARIABLES GLOBALES
 * contCliRed, contador de clientes de red. (Para dar numero al id)
 * contCliApp, contador de clientes de app. (Para dar numero al id)
 * contCliSol, contador de clientes que solicitan atencion domiciliaria.
 * contCliCola, contador de clientes en cola 
 * varTerminarPrograma, variable para manejar el fin del programa.
 */
int contCliRed, contCliApp, contCliSolicitud, contCliCola, varTerminarPrograma;

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
};

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

/* terminarPrograma, da valor de 1 a la variable varTerminarPrograma
 * para no aceptar más solicitudes.
 */
void terminarPrograma(int sig);

/* reordenarLista, con el algortimo de la burbuja, reordena la 
 * lista por prioridad cada vez que entra un cliente en la misma.
 */
void reordenarListaClientes();

//writeLogMessage, metodo para escribir en logFile.
void writeLogMessage(char *id, char *msg);

//calcularAleatorio, metodo para calcular un numero aleatorio comprendido entre el primer y el segundo parámetro incluidos.
int calcularAleatorio(int min, int max);

//buscarCliente, busca el cliente en la lista de clientes por su id.
int buscarCliente(char *id);

/* calcularTiempoAtencion, devuelve el numero de segundos que dura
 * la atencion
*/
int calcularTiempoAtencion(int tipoLlamada);

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
	pthread_t tecnico_1, tecnico_2;
	pthread_t respRep_1, respRep_2;
	pthread_t encargado, atencionDom;

	/* Menú para añadir clientes */
	printf("Introduce 'kill -10 %d' en otro terminal para que llegue el cliente de tipo App \n",getpid());
	printf("Introduce 'kill -12 %d' en otro terminal para que llegue el cliente de tipo Red \n",getpid());
	printf("Introduce 'kill -2 %d' en otro terminal para finalizar el programa \n",getpid());

	//Inicializacion de los semáforos
	pthread_mutex_init(&mutex_log, NULL);
	pthread_mutex_init(&mutex_clientes, NULL);
	pthread_mutex_init(&mutex_solicitudes, NULL);
	pthread_mutex_init(&mutex_terminarPrograma, NULL);

	//Se crean los hilos
	pthread_create(&tecnico_1, NULL, accionesTecnico, NULL);
	pthread_create(&tecnico_2, NULL, accionesTecnico, NULL);
	pthread_create(&respRep_1, NULL, accionesTecnico, NULL);
	pthread_create(&respRep_2, NULL, accionesTecnico, NULL);
	pthread_create(&encargado, NULL, accionesEncargado, NULL);
	pthread_create(&atencionDom, NULL, accionesTecnicoDomiciliario, NULL);
	
	//Armamos señales
	struct sigaction ss;

	ss.sa_handler = nuevoCliente;
	sigaction(SIGUSR1, &ss, NULL);
	sigaction(SIGUSR2, &ss, NULL);

	ss.sa_handler = terminarPrograma;
	sigaction(SIGINT, &ss, NULL);

	//inicializamos contadores declarados de manera global
	contCliCola = 0;
	contCliApp = 0;
	contCliRed = 0;
	contCliSolicitud = 0;
	varTerminarPrograma = 0;

	//Definimos el tamaño de la lista clientes de forma dinamica
	clientes = (struct cliente*) malloc(NUMCLIENTES*sizeof(struct cliente));

	//inicializacion del struct
	for(int i = 0; i < NUMCLIENTES; i++){
		clientes[i].id = NULL;
		clientes[i].tipo = 0;
        clientes[i].atendido = 0;
        clientes[i].prioridad = 0;
        clientes[i].solicitud = 0;
	}

	//Inicializar fichero log
	writeLogMessage("*SISTEMA*: ", "NUEVA EJECUCIÓN");

	while(1){

		pthread_mutex_lock(&mutex_terminarPrograma);

		//Mientras varTerminarPrograma vale 0, seguimos ejecutando el programa
		if(varTerminarPrograma == 0){
			pthread_mutex_unlock(&mutex_terminarPrograma);
			
			//Nos quedamos a la espera de una señal
			pause();
		}

		/* Para terminar el programa, los hilos incrementarán el contador
		 * terminarPrograma desde su valor=1 hasta 7 (6 hilos).
		 */
		else if(varTerminarPrograma == 7){
			pthread_mutex_unlock(&mutex_terminarPrograma);
			writeLogMessage("*SISTEMA*: ", "FIN EJECUCION");
			exit(0);
		}
	}
	free(clientes);
}

//Definimos la funcion nuevoCliente
void nuevoCliente(int sig){

	pthread_mutex_lock(&mutex_terminarPrograma);
	
	//Comprobamos si pueden llegar mas solicitudes
	if(varTerminarPrograma == 1){

		pthread_mutex_unlock(&mutex_terminarPrograma);

		printf("SISTEMA: Terminando programa. Solicitud de nuevo cliente rechazada.\n");
		writeLogMessage("*SISTEMA*: ", "Terminando programa. Solicitud de nuevo cliente rechazada.");

	}
	/* Si la variable es distinta de 1, quiere decir que es 0(ejecutando)
	 * o >1(matando hilos) y pueden seguir llegando solicitudes.
	 */
	else{

		pthread_mutex_unlock(&mutex_terminarPrograma);

		//Comprobamos si hay sitio en la cola
		pthread_mutex_lock(&mutex_clientes);

		//Lista de clientes con sitios libres
		if(contCliCola < NUMCLIENTES){

			pthread_mutex_unlock(&mutex_clientes);

			//Si se ha introducido un cliente de tipo APP
			if(sig == SIGUSR1){

				//Incrementamos el contador de clientes App
				contCliApp++;

				//Creamos ID para el nuevo cliente de APP
				char *nuevoIdApp;
				sprintf(nuevoIdApp, "cliApp_%d",contCliApp);

				//Generamos una prioridad para el nuevo cliente de APP
				int prioridadApp = calcularAleatorio(1,10);

				/* Creamos el nuevo cliente de APP y añadimos sus 
				 * parametros establecidos por el momento
				 */
				struct cliente clienteApp = {nuevoIdApp, 0, 0, 0, prioridadApp};

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


			}

			else if(sig == SIGUSR2){

				//Incrementamos el contador de clientes App
				contCliRed++;

				//Creamos ID para el nuevo cliente de RED
				char *nuevoIdRed;
				sprintf(nuevoIdRed, "cliRed_%d",contCliRed);

				//Generamos una prioridad para el nuevo cliente de RED
				int prioridadRed = calcularAleatorio(1,10);

				/* Creamos el nuevo cliente de RED y añadimos sus 
				 * parametros establecidos por el momento
				 */			
				struct cliente clienteRed = {nuevoIdRed, 0, 1, 0, prioridadRed};

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
		}

		else{
			pthread_mutex_unlock(&mutex_clientes);
			printf("SISTEMA: Lista de clientes llena. Solicitud de nuevo cliente rechazada.\n");
			writeLogMessage("*SISTEMA*: ", "Lista de clientes llena. Solicitud de nuevo cliente rechazada.");
		}

	}

}

//Definimos la funcion accionesCliente
void *accionesCliente(void *arg){
	
	//Casteamos el parametro recibido de void a char
	char *id = *(char*)arg;

	int atendido = 0;
	int tipo = -1;
    int posicion = 0;
	int espera = 0;
	int cansado = 0;

	printf("%s: Nueva solicitud aceptada.\n", id);
	writeLogMessage(id, "Nueva solicitud aceptada");

	pthread_mutex_lock(&mutex_clientes);
	posicion = buscarCliente(id);
	atendido = clientes[posicion].atendido;
	tipo = clientes[posicion].tipo;
	pthread_mutex_unlock(&mutex_clientes);

	do{
		/* En este caso el cliente todavia no ha sido atendido
		 * ni esta siendo atendido
		*/
		if(atendido == 0){

			int tipoAccionClienteApp = calcularAleatorio(1,100);
			
			//pierden interes y abandonan
			if(tipoAccionClienteApp <= 10){
			
			    printf("%s: Nueva solicitud aceptada.\n", id);
			    writeLogMessage(id, "Nueva solicitud aceptada");

				pthread_mutex_lock(&mutex_clientes);
				eliminarCliente(id);
				pthread_mutex_unlock(&mutex_clientes);
				
				pthread_exit(NULL);

			}

			//se cansan de esperar y abandonan
			else if(tipoAccionClienteApp <= 30 && espera == 0){
				
				printf("%s: Me he cansado de esperar. Abandono\n", id);
				writeLogMessage(id, "Me he cansado de esperar. Abandono");
				
				pthread_mutex_lock(&mutex_clientes);
                eliminarCliente(id);
                pthread_mutex_unlock(&mutex_clientes);
				
				pthread_exit(NULL);

			}

			//pierden conexion y abandonan
			else if(tipoAccionClienteApp <= 35){

				printf("%s: Ha perdido la conexion. Abandona\n", id);
                writeLogMessage(id, "Ha perdido la conexion. Abandona");

				pthread_mutex_lock(&mutex_clientes);
                eliminarCliente(id);
                pthread_mutex_unlock(&mutex_clientes);

				//Se mata el hilo del cliente
				pthread_exit(NULL);
	
			}

			sleep(2);
			cansado = (cansado+2)%8;

	}while(atendido == 0);
	
	//El cliente está siendo atendido
	if(atendido == 1){

		printf("%s: Estoy siendo atendido.\n", id);
        writeLogMessage(id, "Estoy siendo atendido.");	
		
		/*Espera por señal del tecnico. (Similar a un pause).
		 * Siempre asociados a un mutex.	
		 * Wait debe realizarse con mutex cerrado.
		 * Hilo se suspende hasta que otro señaliza la condición
		 *  y el mutex asociado se desbloquea
		*/
		pthread_cond_wait(&cond_cliAtendido, &mutex_clientes);

	}

	if(tipo == 1){
		
		int tipoAccionClienteRed = calcularAleatorio(1,100);

		if(tipoAccionClienteRed <= 30){
			
			printf("%s: Solicito atencion domiciliaria.\n", id);
            writeLogMessage(id, "Solicito atencion domiciliaria.");
			
			//0, fuera de lista domiciliaria; 1, dentro de la lista
			int dentroListaDom = 0;
			do{
				/* Comprobamos el numero de solicitudes domiciliarias
				 * pendientes
				*/
				pthread_mutex_lock(&mutex_solicitudes);

				/* Si son menos de 4, incrementamos el valor en unoy
				 * será atendido en el domicilio.
				*/
				if(contCliSolicitud < MAXSOLDOMICILIO){
					
					contCliSolicitud++;
					pthread_mutex_unlock(&mutex_solicitudes);

					dentroListaDom = 1;

					//Escribimos en el log que espera para ser atendido.
					printf("%s: Espero para ser atendido por atencion domiciliaria.\n", id);
					writeLogMessage(id, "Espero para ser atendido por atencion domiciliaria.");

					//Cambiamos el valor de solicitud por 1
					pthread_mutex_lock(&mutex_clientes);
					posicion = buscarCliente(id);
					clientes[posicion].solicitud = 1;
					pthread_mutex_unlock(&mutex_clientes);

					//Si es el cuarto, avisa al técnico domiciliario
					pthread_mutex_lock(&mutex_solicitudes);

					if(contCliSolicitud == MAXSOLDOMICILIO){
						pthread_mutex_unlock(&mutex_solicitudes);
						pthread_cond_signal(&cond_domiciliaria);
					}

					else{
						pthread_mutex_unlock(&mutex_solicitudes);
					}

					/* Bloqueamos a la espera de que 
					 * clientes[posicion].solicitud=0 
					 * (Ya ha finalizado su atencion.
					*/
					pthread_cond_wait(&cond_domiciliaria, &mutex_solicitudes);

				    //Comunicamos que su atención ha finalizado.
					printf("%s: Fin de atencion domiciliaria.\n", id);
					writeLogMessage(id, "Fin de atencion domiciliaria.");



				}

				/* Si el numero de solicitudes pendientes 
			     * de atencion dom. es mayor que 4, 
				*/
				else{
					pthread_mutex_unlock(&mutex_solicitudes);
					sleep(3);
				}
			}while(dentroListaDom == 0);
		}	

		//Eliminamos al cliente de la cola de clientes.
		pthread_mutex_lock(&mutex_clientes);
		eliminarCliente(id);
		pthread_mutex_unlock(&mutex_clientes);

		//Matamos al hilo de cliente.
		pthread_exit(NULL);
				
	}


}

//Definimos la funcion accionesTecnico
void *accionesTecnico(void *arg){
	
	//Casteamos el parametro recibido de void a char
	char *id = *(char*)arg;
	do{
		char *idClientePorAtender = NULL;
		int cliError = 0, contCliAtendidos = 0;

		pthread_mutex_lock(&mutex_clientes);
		for(int i = 0; i < contCliCola; i++){
			//Si es uno de los tecnicos y el cliente que atendera es de app
			if(strcmp(id, "tecnico_1") == 0 || strcmp(id, "tecnico_2") == 0 && clientes[i].tipo == 0){
			
				idClientePorAtender = clientes[i].id;

				clientes[i].atendido = 1;

				break;
			}

			/* Si es uno de los responsables de reparacion
			 * y el cliente que atendera es de app
			*/
			else if(strcmp(id, "respRep_1") == 0 || strcmp(id, "respRep_2" ) == 0 && clientes[i].tipo == 1){

				idClientePorAtender = clientes[i].id;

				clientes[i].atendido = 1;

				break;
			}
		}
		pthread_mutex_unlock(&mutex_clientes);

		//No hay clientes por atender, el empleado duerme 1s
		if(idClientePorAtender == NULL){
			sleep(1);
		}

		//Hay clientes por atender
		else{

			contCliAtendidos++;
			int tipoLlamada  = calcularAleatorio(1,100);
			int tiempoAtencion = calcularTiempoAtencion(tipoLlamada);
			
			printf("%s: Comienza la atencion al cliente.\n", id);
			writeLogMessage(id, "Comienza la atencion al cliente");
			sleep(tiempoAtencion);
			printf("%s: Finalizacion de atencion al cliente.\n", id);
			writeLogMessage(id, "Finalizacion de atencion al cliente");

			//todo en regla
			if(tipoLlamada <= 80){
				printf("%s: Todo está en regla.\n", id);
				writeLogMessage(id, "Todo está en regla");
			}

			//mal identificados
			else if(tipoLlamada <= 90){
				printf("%s: Identificacion errónea.\n", id);
				writeLogMessage(id, "Identificacion errónea.");
			}

			//compañia erronea
			else{
				printf("%s: Compania erronea.\n", id);
				writeLogMessage(id, "Compania erronea");

				/* En este caso como el cliente se equivoco de compañia
				 * y va a abandonar entonces cliError se cambia a -1
				 */
				cliError = -1;
			}

			pthread_mutex_lock(&mutex_clientes);

			/* Si encuentro el id del cliente atendido en la cola
			 * cambio el valor de atendido a 2 porque ya ha sido
			 * atendido
		    */ 
			for(int i = 0; i < contCliCola; i++){
				if(strcmp(clientes[i].id, idClientePorAtender) == 0){
					clientes[i].atendido = 2;
					cliente[i].solicitud = cliError;
					break;
				}
			}
			pthread_mutex_unlock(&mutex_clientes);

			//El tecnico descansa cuando los clientes que atiende son 5
			if(strcmp(id, "tecnico_1") == 0 || strcmp(id, "tecnico_2") == 0 && contCliAtendidos == 5) {
				sleep(5);
				contCliAtendidos = 0;
			}
			//El responsable de reparacion descansa cuando los clientes que atiende son 6
			if(strcmp(id, "respRep_1") == 0 || strcmp(id, "respRep_2") == 0 && contCliAtendidos == 6) {
				sleep(5);
				contCliAtendidos = 0;
			}
		}
	}while(1);
}


//Definimos la funcion accionesEncargado
void *accionesEncargado(void *arg){

	do{
		char *idClientePorAtender = NULL;
		int cliError = 0;

		/* Buscar cliente para atender de su tipo, atendiendo
		 * a prioridad y sino FIFO. Ya ordenados en el nuevoCliente.
		*/
		pthread_mutex_lock(&mutex_clientes);
		for(int i = 0; i < contCliCola; i++){
			if(clientes[i].tipo == 1){
		
				idClientePorAtender = clientes[i].id;

				clientes[i].atendido = 1;

				break;
			}

		}

		/* Si no ha encontrado aún a nigún cliente de red,
		 * recorremos de nuevo en búsqueda de un cliente de APP.
		*/
		if(idClientePorAtender == NULL){
			for(int i = 0; i < contCliCola; i++){
				if(clientes[i].tipo == 1){
			
					idClientePorAtender = clientes[i].id;

					clientes[i].atendido = 1;

					break;
				}
			}
		}
		pthread_mutex_unlock(&mutex_clientes);

		/* Si todavia no ha encontrado ningun cliente que atender
		 * duerme 3 segundos */
		if(idClientePorAtender == NULL){
			sleep(3);
			/* Se comprueba la variable varTerminarPrograma.
			 * Si es mayor que uno, se mata el hilo
			*/
			pthread_mutex_lock(&mutex_terminarPrograma);
			if(varTerminarPrograma > 1){
				varTerminarPrograma++;
				pthread_mutex_unlock(&mutex_terminarPrograma);
				pthread_exit(NULL);
			}
			pthread_mutex_unlock(&mutex_terminarPrograma);
		
		}

		//Hay cliente para atender
		else{

			int tipoLlamada  = calcularAleatorio(1,100);
			int tiempoAtencion = calcularTiempoAtencion(tipoLlamada);
			
			printf("Encargado: Comienza la atencion al cliente.\n");
			writeLogMessage("Encargado", "Comienza la atencion al cliente");
			sleep(tiempoAtencion);
			printf("Encargado: Finalizacion de atencion al cliente.\n");
			writeLogMessage("Encargado", "Finalizacion de atencion al cliente");

			//todo en regla
			if(tipoLlamada <= 80){
				printf("Encargado: Todo está en regla.\n");
				writeLogMessage("Encargado", "Todo está en regla");
			}

			//mal identificados
			else if(tipoLlamada <= 90){
				printf("Encargado: Identificacion errónea.\n");
				writeLogMessage("Encargado", "Identificacion errónea");
			}

			//compañia erronea
			else{
				printf("Encargado: Compania erronea.\n");
				writeLogMessage("Encargado", "Compania erronea");
				cliError = -1;
			}

			pthread_mutex_lock(&mutex_clientes);
			for(int i = 0; i < contCliCola; i++){
				if(strcmp(clientes[i].id, idClientePorAtender) == 0){

					clientes[i].atendido = 2;
					clientes[i].solicitud = cliError;
					break;

				}
			}
			pthread_mutex_unlock(&mutex_clientes);

		}
	}while(1);
}

//Definimos la funcion accionesTecnicoDomiciliario
void *accionesTecnicoDomiciliario(void *arg){

	/* Comprueba el numero de solicitudes y se queda 
	 * bloqueado (cond_wait) mientras sea menor que 4
	*/
	pthread_mutex_lock(&mutex_solicitudes);

	//Todavia el numero de solicitudes no es 4
	if(contCliSolicitud < 4){
		pthread_mutex_unlock(&mutex_solicitudes);
		pthread_cond_wait(&cond_domiciliaria, &mutex_solicitudes);
	}

	//El numero de solicitudes ya es 4 
	else{
		pthread_mutex_unlock(&mutex_solicitudes);
	}

	//Guardamos en el log que comienza la atencion.
	printf("Tecnico Domiciliario: Comienzo la atención al cliente.\n");
	writeLogMessage("Tecnico Domiciliario: ", "Comienzo la atención al cliente.");
	
	/* Un for para atender los clientes que estan esperando por atencion
	 * domiciliaria
	*/
	for(int i=0; i<MAXSOLDOMICILIO; i++){

		sleep(1);

		// Guardamos en log que hemos atendido a un cliente
		printf("Tecnico Domiciliario: Atendido un cliente.\n");
		writeLogMessage("Tecnico Domiciliario: ", "Atendido un cliente.");

		// Cambia el flag de solicitud a 0.
		pthread_mutex_lock(&mutex_clientes);

		for(int i=0; i<contCliCola; i++){

			/* Nos aseguramos que el cliente solicitó atencion
			 * domiciliaria
			*/
			if(clientes[i].solicitud == 1){
				clientes[i].solicitud = 0;
				break;
			}
		}
		pthread_mutex_unlock(&mutex_clientes);

		/* Ya han sido atendidos todos los clientes que 
		 * solitaron atencion domiciliaria
		 * Ponemos a 0 el contador de clientes que solicitaron
		 * atencion domiciliaria
		*/
		pthread_mutex_lock(&mutex_solicitudes);
		contCliSolicitud = 0;
		pthread_mutex_unlock(&mutex_solicitudes);

		//Guardamos en el log que se ha finalizado la atencion domiciliaria.
		printf("Tecnico Domiciliario: Finalizada atencion domiciliaria.\n");
		writeLogMessage("Tecnico Domiciliario: ", "Finalizada atencion domiciliaria.");

		/* Se avisa a los que esperaban por solicitud domiciliaria que 
		 * se ha finalizado la atencion.
		 */
		for(int i=0; i<contCliCola; i++){

			pthread_mutex_lock(&mutex_solicitudes);
			pthread_cond_signal(&cond_domiciliaria);
			pthread_mutex_unlock(&mutex_solicitudes);

		}

		/* Comprobamos si, tras la ejecucion,
		 * tenemos que matar al hilo.
		*/
		pthread_mutex_lock(&mutex_terminarPrograma);
		if(varTerminarPrograma > 1){
			varTerminarPrograma++;
			pthread_mutex_unlock(&mutex_terminarPrograma);
			pthread_exit(NULL);
		}
		pthread_mutex_unlock(&mutex_terminarPrograma);
	
	}while(1)
}

// Definimos la funcion terminarPorgrama
void terminarPrograma(int sig){
	
	pthread_mutex_lock(&mutex_terminarPrograma);
	//Variable terminarPrograma con valor 1, no deja que lleguen más solicitudes, prepara el final de programa.
	varTerminarPrograma = 1;
	pthread_mutex_unlock(&mutex_terminarPrograma);

}

//Definimos una funcion para ordenar la lista de clientes segun la prioridad
void reordenarListaClientes(){
	struct cliente clienteAux = {};
	for(int i = 0; i < contCliCola; i++){
		for(int j = 0; j < contCliCola-1; j++){
			
			int k = j + 1;

			if(clientes[j].prioridad > clientes[k].prioridad){
				clienteAux = clientes[j];
				clientes[j] = clientes[k];
				clientes[k] = clienteAux;
			}
		}

	}
}

//Definimos la funcion de escribir los mensajes en el log
void writeLogMessage ( char *id , char *msg ){

	// Calculamos la hora actual
	time_t now = time (0) ;
	struct tm * tlocal = localtime (& now );
	char stnow [25];
	strftime ( stnow , 25 , " %d/ %m/ %y %H: %M: %S", tlocal );

	// Escribimos en el log, pero no sin antes bloquearlo
	pthread_mutex_lock(&mutex_log);

	logFile = fopen (NombreFichero, "a");
	fprintf (logFile, "[ %s] %s: %s\n", stnow , id , msg ) ;
	fclose (logFile);

	//Y lo desbloqueamos
	pthread_mutex_unlock(&mutex_log);
}

//Definimos la funcion calculaAleatorios
int calculaAleatorios(int min, int max){
	return rand()%(max-min+1)+min;
}

/* Definimos la funcion buscarCliente
 * Devuelve la posicion de un cliente en cola
 * teniendo el id de este
 */
int buscarCliente(char *id){
	for(int i = 0; i < contCliCola; i++){
		if(strcmp(clientes[i].id, id) == 0){
			return i;
		}
	}	
	return 99;	
}

/* Definimos la funcion eliminarCliente
 * El cliente del que se pasa el id es 
 * eliminado de la cola de clientes
 * Intercambiando posiciones con el cliente 
 * a la derecha y poniendo a NULL el último.
 */
void eliminarCliente(char *id){
	int posicion = buscarCliente(id);
	struct cliente ultimo = {};

	for(int i = posicion; i < contCliCola - 1; i++){
		clientes[i]= clientes[i+1];
	}

	clientes[contCliCola-1] = ultimo;
}

/* Definimos la funcion calcularTiempoAtencion
 * Devuelve el tiempo que dormira el cliente
 * que esta siendo atendido
*/
int calcularTiempoAtencion(int tipoLlamada){
	int tiempoAtencion;
	
	//todo en regla, 1-4s y siguen
	if(tipoLlamada <= 80){
		tiempoAtencion = calcularAleatorio(1,4);
	}

	//mal identificados, 2-6s y siguen
	else if(80 < tipoLlamada && tipoLlamada <= 90){
		tiempoAtencion = calcularAleatorio(2,6);
	}

	//compañia erronea, 1-2s y abandonan
	else{
		tiempoAtencion = calcularAleatorio(1,2);
	}

	return tiempoAtencion;
}