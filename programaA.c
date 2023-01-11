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

#define NUMCLIENTES 20
#define MAXSOLDOMICILIO 4

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
//	contCliRed, controlar numero de clientes de red. (Para dar numero al id)
//	contCliApp, controlar numero de clientes de app. (Para dar numero al id)
//	contCliSolicitud, controlar numero de solicitudes para atencion domiciliaria.
//	contCliCola, controla el numero de clientes en cola.
//	terminarPrograma, variable para manejar el fin del programa.
int contCliRed, contCliApp, contCliSolicitud, contCliCola, varTerminarPrograma;

struct cliente{
	//Tipo de cliente
	char id[10];
	//Tipo de cliente. 0, APP; 1, RED.
	int tipo;
	//Valores: 0, sin atender; 1, siendo atendido; 2, atendido.
	int atendido;
	//Valores: 0, no solicitud; 1, solicitud enviada; -1 compañia equivocada.
	int solicitud;
	//Prioridad se asigna por FIFO.
	int prioridad;
};

//Inicializado lista de clientes
struct cliente *clientes;

//Fichero log donde se escribiran las acciones
FILE *logFile;

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
void reordenarListaClientes();

//	writeLogMessage, metodo para escribir en logFile.
void writeLogMessage(char *id, char *msg);

//	calcularAleatorio, metodo para calcular un numero aleatorio comprendido entre el primer y el segundo parámetro incluidos.
int calcularAleatorio(int min, int max);

//	buscarCliente, busca el cliente en la lista de clientes por su id.
int buscarCliente(char *id);

//	eliminarCliente, elimina el cliente pasado por parametro por su id. Intercambiando posiciones con el cliente a la derecha y poniendo a NULL el últmo.
void eliminarCliente(char *id);

//	calcularTiempoAtencion, calcula el tiempo de atencion segun el tipo de la llamada recibida.
int calcularTiempoAtencion(int tipoDeLlamada);

int main (int argc, char *argv[]){

	/* Menú para añadir clientes */
	printf("Introduce 'kill -10 %d' en otro terminal para que llegue el cliente de tipo App \n",getpid());
	printf("Introduce 'kill -12 %d' en otro terminal para que llegue el cliente de tipo Red \n",getpid());
	printf("Introduce 'kill -2 %d' en otro terminal para finalizar el programa \n",getpid());

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
	pthread_create(&tecnico_1, NULL, accionesTecnico, (void*)&"tecnico_1");
	pthread_create(&tecnico_2, NULL, accionesTecnico, (void*)&"tecnico_2");
	pthread_create(&resprep_1, NULL, accionesTecnico, (void*)&"resprep_1");
	pthread_create(&resprep_2, NULL, accionesTecnico, (void*)&"resprep_2");
	pthread_create(&encargado, NULL, accionesEncargado, NULL);
	pthread_create(&atencionDom, NULL, accionesTecnicoDomiciliario, NULL);

	//Armar señales
	struct sigaction ss = {0};

	ss.sa_handler = nuevoCliente;
	sigaction (SIGUSR1, &ss, NULL);
	sigaction (SIGUSR2, &ss, NULL);

	ss.sa_handler = terminarPrograma;
	sigaction (SIGINT, &ss, NULL);

	//Inicializamos valores de contadores
	contCliRed = 0;
	contCliApp = 0;	
	contCliSolicitud = 0;
	contCliCola = 0;
	varTerminarPrograma = 0;

	//Inicializar de forma dinamica el tamaño de la lista de clientes.
	clientes = (struct cliente*) malloc(NUMCLIENTES*sizeof(struct cliente));

	for(int i=0; i<NUMCLIENTES; i++){
		strcpy(clientes[i].id, "0");
		//	solicitud: 0, no ha solicitado atencion domiciliaria; 1, ha solicitado.
		clientes[i].solicitud = 0;
		//	estado: 0, sin atender; 1, siendo atendido.
		clientes[i].atendido = 0;
		//	tipo: 0, app; 1, red.
		clientes[i].tipo = 0;
		//	prioridad: valor [1-10] aleatoriamente
		clientes[i].prioridad = 0;
	}

	//Inicializar fichero log
	writeLogMessage("*SISTEMA*: ", "NUEVA EJECUCIÓN");

	//Espera infinita para señales.
	while(1){

		pthread_mutex_lock(&mutex_terminarPrograma);

		//Mientras varTerminarPrograma tenga valor 0, seguimos ejecutando el programa.
		if(varTerminarPrograma == 0){
			pthread_mutex_unlock(&mutex_terminarPrograma);
			pause();

		//Para terminar el programa, los hilos incrementarán el contador varTerminarPrograma desde su valor=1 hasta 7 (6 hilos).	
		}else if(varTerminarPrograma == 7){
			pthread_mutex_unlock(&mutex_terminarPrograma);
			printf("SISTEMA: FIN EJECUCIÓN\n");
			writeLogMessage("*SISTEMA*: ", "FIN EJECUCIÓN");
			free(clientes);
			exit(0);
		}
	}
}

void nuevoCliente(int sig){

	pthread_mutex_lock(&mutex_terminarPrograma);
	//Comprobamos si puden llegar más solicitudes.
	if(varTerminarPrograma==1){
		pthread_mutex_unlock(&mutex_terminarPrograma);
		printf("SISTEMA: Terminando programa. Solicitud de nuevo cliente rechazada.\n");
		writeLogMessage("*SISTEMA*: ", "Terminando programa. Solicitud de nuevo cliente rechazada.");

	//Si la variable es distinta de 1, quiere decir que es 0(ejecutando) o >1(matando hilos) y pueden seguir llegando solicitudes.
	}else{
		pthread_mutex_unlock(&mutex_terminarPrograma);

		//Comprobamos si hay sitio en la cola.
		pthread_mutex_lock(&mutex_clientes);

		//Lista de clientes con sitios libres.
		if(contCliCola < 20){
			contCliCola++;
			pthread_mutex_unlock(&mutex_clientes);

			if(sig == SIGUSR1){

				//Incrementamos contador de clientes de APP.
				contCliApp++;

				//Creamos ID para el nuevo cliente de APP.
				char nuevoIdApp[10];
				sprintf(nuevoIdApp, "cliApp_%d", contCliApp);

				//Generamos una prioridad para el nuevo cliente de APP.
				int prioridadApp = calcularAleatorio(1, 10);

				//Creamos el nuevo cliente de APP y añadimos sus parametros establecidos por el momento.
				struct cliente clienteApp = {*nuevoIdApp, 0, 0, 0, prioridadApp};

				//Añadimos el nuevo cliente de APP a la lista de clientes.
				pthread_mutex_lock(&mutex_clientes);
				clientes[contCliCola] = clienteApp;
				pthread_mutex_unlock(&mutex_clientes);

				//Reordenamos la lista en funcion de la prioridad.
				reordenarListaClientes();

				//Inicializamos y creamos hilo para el nuevo cliente de APP.
				pthread_t nuevoCliente;
				pthread_create(&nuevoCliente, NULL, accionesCliente, (void*)nuevoIdApp);

			}else if(sig == SIGUSR2){

				//Incrementamos contador de clientes de RED.
				contCliRed++;

				//Creamos ID para el nuevo cliente de RED.
				char nuevoIdRed[10];
				sprintf(nuevoIdRed, "cliRed_%d", contCliRed);

				//Generamos una prioridad para el nuevo cliente de RED.
				int prioridadRed = calcularAleatorio(1, 10);

				//Creamos el nuevo cliente de RED y añadimos sus parametros establecidos por el momento.
				struct cliente clienteRed = {*nuevoIdRed, 0, 1, 0, prioridadRed};

				//Añadimos el nuevo cliente de RED a la lista de clientes.
				pthread_mutex_lock(&mutex_clientes);
				clientes[contCliCola] = clienteRed;
				pthread_mutex_unlock(&mutex_clientes);

				//Reordenamos la lista en funcion de la prioridad.
				reordenarListaClientes();

				//Inicializamos y creamos hilo para el nuevo cliente de RED.
				pthread_t nuevoCliente;
				pthread_create(&nuevoCliente, NULL, accionesCliente, (void*)nuevoIdRed);

			}

		//Lista de clientes llena.	
		}else{
			pthread_mutex_unlock(&mutex_clientes);
			printf("SISTEMA: Lista de clientes llena. Solicitud de nuevo cliente rechazada.\n");
			writeLogMessage("*SISTEMA*: ", "Lista de clientes llena. Solicitud de nuevo cliente rechazada.");

		}
	}
}

void *accionesCliente(void *arg){

	char *id = (char*)arg;
	int atendido = 0;
	int tipo = -1;
	int posicion = 0;
	int espera = 0;
	int cansado = 0;

	pthread_mutex_lock(&mutex_clientes);
	posicion = buscarCliente(id);
	atendido = clientes[posicion].atendido;
	tipo = clientes[posicion].tipo;
	pthread_mutex_unlock(&mutex_clientes);

	if(tipo==0){
		printf("%d\n", tipo);
		printf("%s: Entro al sistema por problemas de APP.\n", id);
		writeLogMessage(id, "Entro al sistema por problemas de APP.");
	}
	else{
		printf("%d\n", tipo);
		printf("%s: Entro al sistema por problemas de RED.\n", id);
		writeLogMessage(id, "Entro al sistema por problemas de RED.");
	}

	do{

	int numAleatorio = 0;

		//Comprobamos si está siendo atendido el cliente. No está siendo atendido ni ha sido atendido.
		if(atendido == 0){

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
				printf("%s: He perdido la conexion. Abandono.\n", id);
				writeLogMessage(id, "He perdido la conexion. Abandono.");

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

			printf("%s:  Solicito atencion domiciliaria.\n", id);
			writeLogMessage(id, " Solicito atencion domiciliaria.");

			//0, fuera de lista domiciliaria; 1, dentro lista.
			int dentroListaDom = 0;

			do{
				//Comprobamos el número de solicitudes domiciliarias pendientes.
				pthread_mutex_lock(&mutex_solicitudes);

				//Si son menos de 4, incrementamos el valor en unoy será atendido en el domicilio.(???)
				if(contCliSolicitud<MAXSOLDOMICILIO){
					contCliSolicitud++;
					pthread_mutex_unlock(&mutex_solicitudes);

					dentroListaDom = 1;

					//I, Escribimos en el log que espera para ser atendido.
					printf("%s:  Espero para ser atendido por atencion domiciliaria.\n", id);
					writeLogMessage(id, " Espero para ser atendido por atencion domiciliaria.");

					//II, Cambiamos el valor de solcitud por 1.
					pthread_mutex_lock(&mutex_clientes);
					posicion = buscarCliente(id);
					clientes[posicion].solicitud = 1;
					pthread_mutex_unlock(&mutex_clientes);

					//III, Si es el cuarto, avisa al técnico.
					pthread_mutex_lock(&mutex_solicitudes);

					if(contCliSolicitud == MAXSOLDOMICILIO){
						pthread_mutex_unlock(&mutex_solicitudes);
						pthread_cond_signal(&cond_domiciliaria);
					}else{
						pthread_mutex_unlock(&mutex_solicitudes);
					}

					//IV, Bloqueamos a la espera de que clientes[posicion].solicitud=0 (Ya ha finalizado su atencion.
					pthread_cond_wait(&cond_domiciliaria, &mutex_solicitudes);

					//V, Comunicamos que su atención ha finalizado.
					printf("%s:  Fin de atencion domiciliaria.\n", id);
					writeLogMessage(id, " Fin de atencion domiciliaria.");

				}else{
					//Si el numero de solicitudes pendientes de atencion dom. es mayor que 4, 
					pthread_mutex_unlock(&mutex_solicitudes);
					sleep(3);
				}

			//???
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

void *accionesTecnico(void *arg){

	char *id = (char*)arg;

	do{
	//Tipo de llamada para tiempos de atencion.
	int tipoDeLlamada = calcularAleatorio(1, 100);
	int tiempoDeAtencion = 1;

	int cliError = 0;
	int contCliAtendidos = 0;

	char *idClientePorAtender = NULL;

		//1. Buscar cliente para atender de su tipo, atendiendo a prioridad y sino FIFO. Ya ordenados en el nuevoCliente.
		pthread_mutex_lock(&mutex_clientes);

		for(int i=0; i<contCliCola; i++){
			//printf("Tengo que aparentar que trabajo %s. ¿Lo intento con el cliente? Cliente atendido %d", id, clientes[i].tipo);
			//2. Cambiamos flag atendido si somos tecnicos y el cliente es de tipo APP (tipo==0). 
			if(clientes[i].tipo == 0 && clientes[i].atendido==0 && (strcmp(id, "tecnico_1") == 0 || strcmp(id, "tecnico_2") == 0)){
				idClientePorAtender = clientes[i].id;
				clientes[i].atendido = 1;
				break;

			//2.Cambiamos flag atendido si somos responsables de reapraciones y el cliente es de tipo RED (tipo==1).
			}else if(clientes[i].tipo == 1 &&  clientes[i].atendido==0 && (strcmp(id, "respprep_1") == 0 || strcmp(id, "resprep_2") == 0)){
				idClientePorAtender = clientes[i].id;
				clientes[i].atendido = 1;
				break;
			}
		}

		pthread_mutex_unlock(&mutex_clientes);

		if(idClientePorAtender == NULL){
			sleep(1);
			pthread_mutex_lock(&mutex_terminarPrograma);
			if(varTerminarPrograma > 1){
				varTerminarPrograma++;
				pthread_mutex_unlock(&mutex_terminarPrograma);
				pthread_exit(NULL);
			}
			pthread_mutex_unlock(&mutex_terminarPrograma);
		
		}else{
		
			contCliAtendidos++;

			//3. Calculamos el tiempo de atención.
			tiempoDeAtencion = calcularTiempoAtencion(tipoDeLlamada);

			//4. Guardamos en el log que comienza la atendion.
			printf("%s: Comenzamos la atención al cliente.\n", id);
			writeLogMessage(id, "Comenzamos la atención al cliente.");

			//5. Dormimos el tiempo de atención.
			sleep(tiempoDeAtencion);

			//6 y 7. Guardamos en el log que finaliza la atencion y el motivo.
			printf("%s: Finalizamos la atención al cliente.\n", id);
			writeLogMessage(id, "Finalizamos la atención al cliente.");

			if(tipoDeLlamada <= 80){
				printf("%s: Todo en orden.\n", id);
				writeLogMessage(id, "Todo en orden.");
			}else if(tipoDeLlamada <= 90){
				printf("%s: Cliente mal identificado.\n", id);
				writeLogMessage(id, "Cliente mal identificado.");
			}else{
				printf("%s: Compañia erronea.\n", id);
				writeLogMessage(id, "Compañia erronea.");
				cliError = -1;
			}

			//8. Cambiamos el flag de atendido  
			pthread_mutex_lock(&mutex_clientes);

			for(int i=0; i<contCliCola; i++){

				if(strcmp(clientes[i].id,idClientePorAtender) == 0){
					clientes[i].atendido = 2;
					clientes[i].solicitud = cliError;
					break;
				}
			}

			pthread_mutex_unlock(&mutex_clientes);

			//9. Mira si toca descanso. (Cada 5 clientes atendidos).
			if((strcmp(id, "tecnico_1") == 0 || strcmp(id, "tecnico_2") == 0) && contCliAtendidos >= 5){
				
				printf("%s: Me voy al descanso.\n", id);
				writeLogMessage(id, "Me voy al descanso.");

				sleep(5);

				printf("%s: Vuelvo del descanso.\n", id);
				writeLogMessage(id, "Vuelvo del descanso.");

				contCliAtendidos = 0;

			}else if((strcmp(id, "respprep_1") == 0 || strcmp(id, "respprep_2_2") == 0) && contCliAtendidos >= 6){
				
				printf("%s: Me voy al descanso.\n", id);
				writeLogMessage(id, "Me voy al descanso.");

				sleep(5);

				printf("%s: Vuelvo del descanso.\n", id);
				writeLogMessage(id, "Vuelvo del descanso.");

				contCliAtendidos = 0;
			}
		}

		//10. Vuelta a buscar el siguiente.
	}while(1);
}

void *accionesEncargado(void *arg){

	do{

	//Tipo de llamada para tiempos de atencion.
	int tipoDeLlamada = calcularAleatorio(1, 100);
	int tiempoDeAtencion = 1;

	int clienteError = 0;

	char *idClientePorAtender = NULL;

		//1. Buscar cliente para atender de su tipo, atendiendo a prioridad y sino FIFO. Ya ordenados en el nuevoCliente.
		pthread_mutex_lock(&mutex_clientes);

		//	Recorremos una primera vez la cola en busca de clientes de RED. 
		for(int i=0; i<contCliCola; i++){
			if(clientes[i].tipo == 1 && clientes[i].atendido==0 ){
				idClientePorAtender = clientes[i].id;

				//2. Cambiamos el flag de atendido.
				clientes[i].atendido = 1;
				break;
			}
		}

		//Si no ha cogigo aún a nigún cliente, recorremos de nuevo en búsqueda de un cliente de APP.
		if(idClientePorAtender == NULL){

			for(int i=0; i<contCliCola; i++){
				if(clientes[i].tipo == 0 && clientes[i].atendido==0 ){
					idClientePorAtender = clientes[i].id;

					//2. Cambiamos el flag de atendido.
					clientes[i].atendido = 1;
					break;
				}
			}
		}

		pthread_mutex_unlock(&mutex_clientes);

		//No encontramos clientes, dormimos 3s.
		if(idClientePorAtender == NULL){

			sleep(3);

			//Comprobamos la variable varTerminarPrograma. Si es mayor que uno, mataremos al hilo. (>=???)
			pthread_mutex_lock(&mutex_terminarPrograma);
			if(varTerminarPrograma > 1){
				varTerminarPrograma++;
				pthread_mutex_unlock(&mutex_terminarPrograma);
				pthread_exit(NULL);
			}
			pthread_mutex_unlock(&mutex_terminarPrograma);

		//Hemos encontrado cliente por atender.
		}else{

			//3. Calculamos el tiempo de atención.
			tiempoDeAtencion = calcularTiempoAtencion(tipoDeLlamada);

			//4. Guardamos en el log que comienza la atendion.
			printf("Encargado: Comenzamos la atención al cliente.\n");
			writeLogMessage("Encargado", "Comenzamos la atención al cliente.");

			//5. Dormimos el tiempo de atención.
			sleep(tiempoDeAtencion);

			//6 y 7. Guardamos en el log que finaliza la atencion y el motivo.
			printf("Encargado: Finalizamos la atención al cliente.\n");
			writeLogMessage("Encargado", "Finalizamos la atención al cliente.");

			if(tipoDeLlamada <= 80){
				printf("Encargado: Motivo: Todo en orden.\n");
				writeLogMessage("Encargado", "Motivo: Todo en orden.");
			}else if(tipoDeLlamada <= 90){
				printf("Encargado: Motivo: Cliente mal identificado.\n");
				writeLogMessage("Encargado", "Motivo: Cliente mal identificado.");
			}else{
				printf("Encargado: Motivo: Compañia erronea.\n");
				writeLogMessage("Encargado", "Motivo: Compañia erronea.");
				clienteError = -1;
			}

			//8. Cambiamos el flag de atendido  
			pthread_mutex_lock(&mutex_clientes);

			for(int i=0; i<contCliCola; i++){

				if(strcmp(clientes[i].id,idClientePorAtender) == 0){
					clientes[i].atendido = 2;
					clientes[i].solicitud = clienteError;
					break;
				}
			}

			pthread_mutex_unlock(&mutex_clientes);
		}
	//9. Vuelta a buscar el siguiente.
	}while(1);
}

void *accionesTecnicoDomiciliario(void *arg){

	do{

		//1. Comprueba el numero de solicitudes y se queda bloqueado (cond_wait) mientras sea menor que 4.
		pthread_mutex_lock(&mutex_solicitudes);

		if(contCliSolicitud < 4){
			pthread_mutex_unlock(&mutex_solicitudes);
			pthread_cond_wait(&cond_domiciliaria, &mutex_solicitudes);
		}else{
			pthread_mutex_unlock(&mutex_solicitudes);
		}


		//2. Guardamos en el log que comienza la atencion.
		printf("Tecnico Domiciliario: Comenzamos la atención al cliente.\n");
		writeLogMessage("Tecnico Domiciliario: ", "Comenzamos la atención al cliente.");

		//3. Duerme un segundo por petición.

		for(int i=0; i<4; i++){

			sleep(1);

			//4. Guardamos en log que hemos atendido a uno.
			printf("Tecnico Domiciliario: Atendido un cliente.\n");
			writeLogMessage("Tecnico Domiciliario: ", "Atendido un cliente.");

			//5. Cambia el flag de solicitud a 0.
			pthread_mutex_lock(&mutex_clientes);

			for(int i=0; i<contCliCola; i++){

				if(clientes[i].solicitud == 1){
					clientes[i].solicitud = 0;
					break;
				}
			}
			pthread_mutex_unlock(&mutex_clientes);
		}

		//6. Cuando el ultimo es atendido, contCliSolicitud=0;
		pthread_mutex_lock(&mutex_solicitudes);
		contCliSolicitud = 0;
		pthread_mutex_unlock(&mutex_solicitudes);

		//7. Guardamos en el log que se ha finalizado la atencion domiciliaria.
		printf("Tecnico Domiciliario: Finalizada atencion domiciliaria.\n");
		writeLogMessage("Tecnico Domiciliario: ", "Finalizada atencion domiciliaria.");

		//8. Se avisa a los que esperaban por solicitud domiciliaria que se ha finalizado la atencion.
		for(int i=0; i<contCliCola; i++){

			pthread_mutex_lock(&mutex_solicitudes);
			pthread_cond_signal(&cond_domiciliaria);
			pthread_mutex_unlock(&mutex_solicitudes);

		}

		//Comprobamos si tenemos que, tras la ejecucion (???), tenemos que matar al hilo.
		pthread_mutex_lock(&mutex_terminarPrograma);
			if(varTerminarPrograma > 1){
				varTerminarPrograma++;
				pthread_mutex_unlock(&mutex_terminarPrograma);
				pthread_exit(NULL);
			}
			pthread_mutex_unlock(&mutex_terminarPrograma);
		
		//9. Volvemos al paso 1.
	}while(1);
}

void terminarPrograma(int sig){
	pthread_mutex_lock(&mutex_terminarPrograma);
	//Variable terminarPrograma con valor 1, no deja que lleguen más solicitudes, prepara el final de programa.
	varTerminarPrograma = 1;
	pthread_mutex_unlock(&mutex_terminarPrograma);

}

void reordenarListaClientes(){

	pthread_mutex_lock(&mutex_clientes);

	struct cliente clienteAux = {0, 0, 0, 0, 0};

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
	logFile = fopen("RegistroAverias.log", "a");
	fprintf(logFile, "[%s] %s: %s\n", stnow, id, msg);
	fclose(logFile);
	pthread_mutex_unlock(&mutex_log);
}

int calcularAleatorio(int min, int max){
	return rand()%(max-min+1)+min;
}

int buscarCliente(char *id){

	int posicion = 0;

	for(int i=0; i<contCliCola; i++){
		if(strcmp(clientes[i].id, id) == 0){
			posicion = i;
			break;

		} 
	}
	return posicion;
}

void eliminarCliente(char *id){

	int posicion = buscarCliente(id);
	struct cliente ultimo = {0, 0, 0, 0, 0};

	for(int i=posicion; i<contCliCola-1; i++){
		clientes[i] = clientes[i+1];
	}
	clientes[contCliCola-1] = ultimo;
}

int calcularTiempoAtencion(int tipoDeLlamada){

	int tiempoDeAtencion = 0;

	//	Todo en regla, 1-4s y siguen.
	if(tipoDeLlamada <= 80){
		tiempoDeAtencion = calcularAleatorio(1,4);

	//	Mal identificados, 2-6s y siguen.
	}else if(tipoDeLlamada <= 90){
		tiempoDeAtencion = calcularAleatorio(2,6);

	// Compañia erronea, 1-2s y abandonan.
	}else{
		tiempoDeAtencion = calcularAleatorio(1,2);
	}

	return tiempoDeAtencion;
}