/*******************************************************************
 * SERVIDOR no porto 9000, à escuta de novos clientes.  Quando surjem
 * novos clientes os dados por eles enviados são lidos e descarregados no ecran.
 gcc ProjetoSERVER.c -Wall -o server -lcurl -ljson-c
 ./server
 *******************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <signal.h>
#include <json-c/json.h>
#include <curl/curl.h>

#define SERVER_PORT     9000
#define SERVER_PORTSUB     9020
#define BUF_SIZE	1024

//ESTRUTURA PARA MENSAGENS
typedef struct sms{
   float received, sent;
}t_sms;

//ESTRUTURA PARA CHAMADAS
typedef struct call{
   float duration, made, missed, received;
}t_call;

//ESTRUTURA PARA A MEDIA
typedef struct media{
	t_call calls;
	t_sms message;
}t_media;

//ESTRUTURA PARA AS PESSOAS QUE USAM A ISABELA
typedef struct pessoa{
   char *ID;
   char *type;
   char *atividade;
   char *local;
   char *depart;
   struct pessoa *prox;
   t_call calls;
   t_sms message;
}t_person;

char idAtual[BUF_SIZE]; //se a pessoa tentar mudar o id e errar é com esta variavel que guardamos voltamos ao ID anterior
int GLOBALSUB = 0; //esta variavel ajuda-nos a ver se a pessoa esta Subscrita ou nao

void process_client(int fd, t_person *cabeca, t_media *grupoMedia, int fdSub);
void process_sub(int fdSub, t_person *cabeca, char *idAtual, t_media *mediaAnt, t_media *grupoMedia, int nrSub, int *dadosSub);
void erro(char *msg);

t_person *cria_cabecalhoP(void){
	t_person *lista = (t_person*)malloc(sizeof(t_person));
   if (lista != NULL)
   	lista->prox = NULL;
   return lista;
}

t_media *cria_media(void){
	t_media *lista = (t_media*)malloc(sizeof(t_media));
   return lista;
}
//para calcular a media não podemos contar com a media anterior e por isso temos que a "limpar"
t_media *incializaMedia(t_media *media){
	media->calls.duration = 0;
	media->calls.made = 0;
	media->calls.missed = 0;
	media->calls.received = 0;
	media->message.received = 0;
	media->message.sent = 0;
	return media;
}
//calcula a media de todos os utilizadores
t_media *calculaMedia(t_media *media, t_person *lista){
	int i = 0;
	media = incializaMedia(media);
	while(lista->prox != NULL){
		media->calls.duration += lista->calls.duration;
		media->calls.made += lista->calls.made;
		media->calls.missed += lista->calls.missed;
		media->calls.received += lista->calls.received;
		media->message.received += lista->message.received;
		media->message.sent += lista->message.sent;
   	lista = lista->prox;
   	i++;
	}
	media->calls.duration /= i;
	media->calls.made /= i;
	media->calls.missed /= i;
	media->calls.received /= i;
	media->message.received /= i;
	media->message.sent /= i;
	return media;
}
//compara as medias e se houver alguma media diferente dá return de um certo número
int comparaMedias(t_media *atual, t_media *anterior){
	if(atual->calls.duration != anterior->calls.duration){
		return 1;
	}else if(atual->calls.made != anterior->calls.made){
		return 2;
	}else if(atual->calls.missed != anterior->calls.missed){
		return 3;
	}else if(atual->calls.received != anterior->calls.received){
		return 4;
	}else if(atual->message.sent != anterior->message.sent){
		return 5;
	}else if(atual->message.received != anterior->message.received){
		return 6;
	}else{
		return 0;
	}
}
/*****************************************   Vai buscar dados à ISABELA   *************************************************************************/

struct string {
	char *ptr;
	size_t len;
};

//Write function to write the payload response in the string structure
size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
	size_t new_len = s->len + size*nmemb;
	s->ptr = realloc(s->ptr, new_len + 1);
	if (s->ptr == NULL) {
		fprintf(stderr, "realloc() failed\n");
		exit(EXIT_FAILURE);
	}
	memcpy(s->ptr + s->len, ptr, size*nmemb);
	s->ptr[new_len] = '\0';
	s->len = new_len;

	return size*nmemb;
}

//Initilize the payload string
void init_string(struct string *s) {
	s->len = 0;
	s->ptr = malloc(s->len + 1);
	if (s->ptr == NULL) {
		fprintf(stderr, "malloc() failed\n");
		exit(EXIT_FAILURE);
	}
	s->ptr[0] = '\0';
}

//Get the Data from the API and return a JSON Object
struct json_object *get_student_data(){
	struct string s;
	struct json_object *jobj;

	//Intialize the CURL request
	CURL *hnd = curl_easy_init();

	//Initilize the char array (string)
	init_string(&s);

	curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "GET");
	//To run on department network uncomment this request and comment the other
	//curl_easy_setopt(hnd, CURLOPT_URL, "http://10.3.4.75:9014/v2/entities?options=keyValues&type=student&attrs=activity,calls_duration,calls_made,calls_missed,calls_received,department,location,sms_received,sms_sent&limit=1000");
        //To run from outside
	curl_easy_setopt(hnd, CURLOPT_URL, "http://socialiteorion2.dei.uc.pt:9014/v2/entities?options=keyValues&type=student&attrs=activity,calls_duration,calls_made,calls_missed,calls_received,department,location,sms_received,sms_sent&limit=1000");

	//Add headers
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "cache-control: no-cache");
	headers = curl_slist_append(headers, "fiware-servicepath: /");
	headers = curl_slist_append(headers, "fiware-service: socialite");

	//Set some options
	curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, writefunc); //Give the write function here
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, &s); //Give the char array address here

	//Perform the request
	CURLcode ret = curl_easy_perform(hnd);
	if (ret != CURLE_OK){
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(ret));

		/*jobj will return empty object*/
		jobj = json_tokener_parse(s.ptr);

		/* always cleanup */
		curl_easy_cleanup(hnd);
		return jobj;

	}
	else if (CURLE_OK == ret) {
		jobj = json_tokener_parse(s.ptr);
		free(s.ptr);

		/* always cleanup */
		curl_easy_cleanup(hnd);
		return jobj;
	}
	return NULL;

}

t_person *inserePessoas(t_person *lista){
	//JSON obect
	struct json_object *jobj_array, *jobj_obj;
	struct json_object *jobj_object_id, *jobj_object_type, *jobj_object_activity, *jobj_object_location, *jobj_object_callsduration, 
	*jobj_object_callsmade, *jobj_object_callsmissed, *jobj_object_callsreceived, *jobj_object_department, *jobj_object_smsreceived, *jobj_object_smssent;
	int arraylen = 0;
	int i;

	//Get the student data
	jobj_array = get_student_data();

	//Get array length
	arraylen = json_object_array_length(jobj_array);

	//Example of howto retrieve the data
	for (i = 0; i < arraylen; i++) {
		//get the i-th object in jobj_array
		jobj_obj = json_object_array_get_idx(jobj_array, i);
		if (json_object_get_string(json_object_object_get(jobj_obj, "activity")) == NULL){
			continue;
		}
	
		//get the name attribute in the i-th object
		jobj_object_id = json_object_object_get(jobj_obj, "id");
		jobj_object_type = json_object_object_get(jobj_obj, "type");
		jobj_object_activity = json_object_object_get(jobj_obj, "activity");
		jobj_object_location = json_object_object_get(jobj_obj, "location");
		jobj_object_callsduration = json_object_object_get(jobj_obj, "calls_duration");
		jobj_object_callsmade = json_object_object_get(jobj_obj, "calls_made");
		jobj_object_callsmissed = json_object_object_get(jobj_obj, "calls_missed");
		jobj_object_callsreceived= json_object_object_get(jobj_obj, "calls_received");
		jobj_object_department = json_object_object_get(jobj_obj, "department");
		jobj_object_smsreceived = json_object_object_get(jobj_obj, "sms_received");
		jobj_object_smssent = json_object_object_get(jobj_obj, "sms_sent");


		t_person *novo = (t_person*)malloc(sizeof(t_person));
   	if (novo == NULL)
   		return NULL;
   	while(lista->prox != NULL)
      	lista = lista->prox;
   	novo->ID = strdup(json_object_get_string(jobj_object_id));
   	novo->type = strdup(json_object_get_string(jobj_object_type));
   	novo->local = strdup(json_object_get_string(jobj_object_location));
   	novo->depart = strdup(json_object_get_string(jobj_object_department));
   	novo->atividade = strdup(json_object_get_string(jobj_object_activity));
		novo->calls.duration = atoi(json_object_get_string(jobj_object_callsduration));
		novo->calls.made = atoi(json_object_get_string(jobj_object_callsmade));
		novo->calls.missed = atoi(json_object_get_string(jobj_object_callsmissed));
		novo->calls.received = atoi(json_object_get_string(jobj_object_callsreceived));
		novo->message.received = atoi(json_object_get_string(jobj_object_smsreceived));
		novo->message.sent = atoi(json_object_get_string(jobj_object_smssent));
		novo->prox = lista->prox;
		lista->prox = novo;


		//print out the name attribute
		printf("id=%s\n", json_object_get_string(jobj_object_id));
		printf("type=%s\n", json_object_get_string(jobj_object_type));
		printf("activity=%s\n", json_object_get_string(jobj_object_activity));
		printf("location=%s\n", json_object_get_string(jobj_object_location));
		printf("Calls duration(s)=%s\n", json_object_get_string(jobj_object_callsduration));
		printf("Calls made=%s\n", json_object_get_string(jobj_object_callsmade));
		printf("Calls missed=%s\n", json_object_get_string(jobj_object_callsmissed));
		printf("Calls received=%s\n", json_object_get_string(jobj_object_callsreceived));
		printf("Department=%s\n", json_object_get_string(jobj_object_department));
		printf("Sms received=%s\n", json_object_get_string(jobj_object_smsreceived));
		printf("Sms sent=%s\n", json_object_get_string(jobj_object_smssent));
		printf("\n");
	}

	return lista;
}


/*******************************************************************************************************************************/


int main() {
	t_person *cabeca = cria_cabecalhoP();
	t_media *grupoMedia = cria_media();
	int fd, client, fdSub, sub;
   struct sockaddr_in addr, addrSub, client_addr, sub_addr;
   int client_addr_size, sub_addr_size;

   //Insere todas as pessoas da isabela na estrutura t_person
   inserePessoas(cabeca);

   bzero((void *) &addr, sizeof(addr));
   addr.sin_family = AF_INET;
   addr.sin_addr.s_addr = htonl(INADDR_ANY);
   addr.sin_port = htons(SERVER_PORT);


   //criamos dois sockets, um para o menu e outro para as notificações
   if ( (fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		erro("na funcao socket");
   if ( bind(fd,(struct sockaddr*)&addr,sizeof(addr)) < 0)
		erro("na funcao bind  1");
   if( listen(fd, 5) < 0)
		erro("na funcao listen");

	bzero((void *) &addrSub, sizeof(addrSub));
   addrSub.sin_family = addr.sin_family;
   addrSub.sin_addr.s_addr = addr.sin_addr.s_addr;
   addrSub.sin_port = htons(SERVER_PORTSUB);

	if ( (fdSub = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		erro("na funcao socket");
   if ( bind(fdSub,(struct sockaddr*)&addrSub,sizeof(addrSub)) < 0)
		erro("na funcao bind  2");
   if( listen(fdSub, 5) < 0)
		erro("na funcao listen");

   while (1) {
   	client_addr_size = sizeof(client_addr);
    	client = accept(fd,(struct sockaddr *)&client_addr,(socklen_t *)&client_addr_size);
    	sub_addr_size = sizeof(sub_addr);
    	sub = accept(fdSub,(struct sockaddr *)&sub_addr,(socklen_t *)&sub_addr_size);
    	if (client > 0) {
      	if (fork() == 0) {
        		close(fd);
        		close(fdSub);
        		process_client(client, cabeca, grupoMedia, sub);
        		exit(0);
        	}

    	close(client);
    	}
  	}
  	return 0;
}

void process_client(int client_fd, t_person *cabeca, t_media *grupoMedia, int fdSub){
	int verifica = 0;
	char idInput[BUF_SIZE];
	int flag = 0;
	int nread = 0;
	int nrDados = 0;
	int nrSub = 0;
	int escolha = 0;
	char buffer[BUF_SIZE];
	char str[BUF_SIZE];
	char confirma[BUF_SIZE];
	int dadosSub[7];
	t_person *aux = cabeca->prox;
	t_media *mediaAnt = cria_media();
	pid_t pid;

	while(1){
		//o primeiro read recebe o comando que inserimos no menu
		nread = read(client_fd, buffer, BUF_SIZE - 1); //1
    	buffer[nread] = '\0';
		if(strcmp(buffer,"ID") == 0) {
			verifica = 1;
			write(client_fd,"Insira o seu ID: ",strlen("Insira o seu ID: ") + 1);  //2
			nread = read(client_fd, idInput, BUF_SIZE-1);  //3
			idInput[nread] = '\0';
			//verificar se o id existe
			aux = cabeca->prox;
			while((strcmp(aux->ID, idInput) != 0 ) && aux->prox != NULL)
				aux = aux->prox;
			if(aux->prox == NULL && (strcmp(aux->ID, idInput) != 0)){
				write(client_fd,"Erro! ID nao encontrado",strlen("Erro! ID nao encontrado")+1); //4
				aux = cabeca->prox;
				//Se o ID nao foi encontrado na ISABELA volta ao ID anterior
				while((strcmp(aux->ID, idAtual) != 0 ) && aux->prox != NULL)
					aux = aux->prox;
				if(aux->prox == NULL)
					verifica = 0;
			}else {
				write(client_fd,"Bem vindo! ID encontrado!",strlen("Bem vindo! ID encontrado!")+1);//4
				strcpy(idAtual, idInput);
				printf("%s", idAtual);
				if(flag == 1)
					kill(pid, SIGKILL);   //"Mata" a subscrição do id anterior
				flag = 1;
				GLOBALSUB = 0;
			}
		}

		else if(strcmp(buffer,"DADOS") == 0) {
			if(verifica == 0){
				strcpy(str,"Erro: ID nao definido! Use o comando 'ID' para o definir.");
				write(client_fd,str,1 + strlen(str)); //2
			}else{
				write(client_fd, "", 1 + strlen("")); // 2
				nread = read(client_fd, buffer, BUF_SIZE-1);
				buffer[nread] = '\0';
				nrDados = atoi(buffer);
				if(nrDados == 11){
					sprintf(str,"O seu ID é %s.\nDe momento, esta %s.\nA sua localização atual é %s.\nA duração das chamadas foi %.2f segundos.\nFez %.2f chamadas.\nPerdeu %.2f chamadas.\nRecebeu %.2f chamadas.\nO seu departamento é o %s.\nNos ultimos 5 segudos recebeu %.2f sms.\nNos ultimos 5 segudos enviou %.2f sms.",aux->ID, aux->atividade, aux->local, aux->calls.duration, aux->calls.made, aux->calls.missed, aux->calls.received, aux->depart, aux->message.received, aux->message.sent);
        			write(client_fd,str, 1 + strlen(str));
        		}
        		else{
        			write(client_fd, "", 1 + strlen(""));
        		 	for(int i = 0; i < nrDados; i++){
        		 		//este for vai se realizar nrDados vezes, recebe qual é o dado que a pessoa quer visualizar e envia para o cliente
        		 		nread = read(client_fd, buffer, BUF_SIZE-1);
        		 		buffer[nread] = '\0';
        		 		escolha = atoi(buffer);
						if(escolha == 1){
							sprintf(str,"O seu ID é %s.",aux->ID);
							write(client_fd,str, 1 + strlen(str));   //2
						}
						else if(escolha == 2){
							sprintf(str,"A sua atividade atual é %s.",aux->atividade);
							write(client_fd,str, 1 + strlen(str));
						}
						else if(escolha == 3){
							sprintf(str,"A sua localização atual é %s.",aux->local);
							write(client_fd,str, 1 + strlen(str));
						}
						else if(escolha == 4){
							sprintf(str,"A duração das chamadas foi %.2f segundos.",aux->calls.duration);
							write(client_fd,str, 1 + strlen(str));
						}
						else if(escolha == 5){
							sprintf(str,"Efectuou %.2f chamadas.",aux->calls.made);
							write(client_fd,str, 1 + strlen(str));
						}
						else if(escolha == 6){
							sprintf(str,"Perdeu %.2f chamadas.",aux->calls.missed);
							write(client_fd,str, 1 + strlen(str));
						}
						else if(escolha == 7){
							sprintf(str,"Recebeu %.2f chamadas.",aux->calls.received);
							write(client_fd,str, 1 + strlen(str));
						}
						else if(escolha == 8){
							sprintf(str,"O seu departamento é o %s.",aux->depart);
							write(client_fd,str, 1 + strlen(str));
						}
						else if(escolha == 9){
							sprintf(str,"Nos ultimos 5 segudos recebeu %.2f sms.",aux->message.received);
							write(client_fd,str, 1 + strlen(str));
						}
						else if(escolha == 10){
							sprintf(str,"Nos ultimos 5 segudos enviou %.2f sms.",aux->message.sent);
							write(client_fd,str, 1 + strlen(str));
						}
					}
				}	
			}
		}

		else if(strcmp(buffer,"GRUPO") == 0) {
			//calcula a media atual e envia para o cliente as medias
			calculaMedia(grupoMedia, cabeca);
			sprintf(str,"A media da duração das chamadas é %.2f segundos.",grupoMedia->calls.duration);
			write(client_fd,str, 1 + strlen(str));
			read(client_fd, confirma, BUF_SIZE-1);
			sprintf(str,"A media de chamadas feitas é %.2f segundos.",grupoMedia->calls.made);
			write(client_fd,str, 1 + strlen(str));
			read(client_fd, confirma, BUF_SIZE-1);
			sprintf(str,"A media de chamadas perdidas é %.2f segundos.",grupoMedia->calls.missed);
			write(client_fd,str, 1 + strlen(str));
			read(client_fd, confirma, BUF_SIZE-1);
			sprintf(str,"A media de chamadas recebidas é %.2f segundos.",grupoMedia->calls.received);
			write(client_fd,str, 1 + strlen(str));
			read(client_fd, confirma, BUF_SIZE-1);
			sprintf(str,"A media de menssagens recebidas é %.2f segundos.",grupoMedia->message.received);
			write(client_fd,str, 1 + strlen(str));
			read(client_fd, confirma, BUF_SIZE-1);
			sprintf(str,"A media de menssagens enviadas é %.2f segundos.",grupoMedia->message.sent);
			write(client_fd,str, 1 + strlen(str));
		}
		else if(strcmp(buffer,"SUBS") == 0){
			if(verifica == 0){
				strcpy(str,"Erro: ID nao definido! Use o comando 'ID' para o definir.");
				write(client_fd,str,1 + strlen(str)); //2
			}else if(GLOBALSUB == 0){
					strcpy(str, "Não está subscrito a notificações, para se subscrever insira [ Y ]");
					write(client_fd, str, 1 + strlen(str)); //2
					nread = read(client_fd, str, BUF_SIZE-1); //3
					write(client_fd, "", BUF_SIZE-1); //4
					str[nread] = '\0';
					if(strcmp(str, "Y") == 0 || strcmp(str, "y") == 0){
						GLOBALSUB = 1;
						nread = read(client_fd, str, BUF_SIZE-1);//5
						str[nread] = '\0';
						nrSub = atoi(str);
						if(nrSub == 7){
							pid = fork();
							if (pid == 0) {
        						process_sub(fdSub, cabeca, idAtual, mediaAnt, grupoMedia, nrSub, dadosSub);
								exit(0);
        					}
        				}else{ 
        					for(int i = 0; i < nrSub; i++){
        						//este for realiza-se nrSub vezes e a sua função é receber quais sao os dados que as pessoas querem 
        						//receber e insere-os num array
        						write(client_fd, "", BUF_SIZE-1);
        						nread = read(client_fd, str, BUF_SIZE-1);//5
								str[nread] = '\0';
								dadosSub[i] = atoi(str);
        					}
        					pid = fork();
							if (pid == 0) {
        						process_sub(fdSub, cabeca, idAtual, mediaAnt, grupoMedia, nrSub, dadosSub);
								exit(0);
        					}
        				}
        			}
        	}

							else if(GLOBALSUB == 1){
								//se a pessoa estiver subscrita. esta função dá a possivilidade de o cliente deixar de ser notificado
								strcpy(str, "Está subscrito a notificações, para deixar de ser notificado insira [ Y ]");
								write(client_fd, str, 1 + strlen(str));//2
								nread = read(client_fd, str, BUF_SIZE-1);//3
								str[nread] = '\0';
								write(client_fd, "Deixar de subs", BUF_SIZE-1); //4
								if(strcmp(str, "Y") == 0 || strcmp(str, "y") == 0){
									GLOBALSUB = 0;
									kill(pid, SIGKILL);
								}
							}
		}	
		else if(strcmp(buffer,"SAIR") == 0) {
				kill(pid, SIGKILL);
				write(client_fd,"",1 + strlen(str)); //2
		}
		else{
			write(client_fd,"",1 + strlen(str)); //2
		}

		fflush(stdout);
	}
	close(client_fd);
}
void process_sub(int fdSub, t_person *cabeca, char *idAtual, t_media *mediaAnt, t_media *grupoMedia, int nrSub, int *dados){
	t_person *aux = cabeca->prox;
	char strS[BUF_SIZE];
	int ret;
	int avanca = 0;

	while((strcmp(aux->ID, idAtual) != 0) && aux->prox != NULL )
		aux = aux->prox;
	while(1){
		inserePessoas(cabeca);
		calculaMedia(grupoMedia, cabeca);
		ret = comparaMedias(mediaAnt, grupoMedia);
		//srSub == 7 significa que o cliente quer ser notificado de todos os dados e ret != 0 é quando ha alguma mudança na media
		if(ret !=  0 && nrSub == 7){
				sprintf(strS,"\n\n[ ATUALIZACAO DE MEDIAS ]\n\nA media da duração das chamadas é %.2f segundos.\nA media de chamadas feitas é %.2f segundos.\nA media de chamadas perdidas é %.2f segundos.\nA media de chamadas recebidas é %.2f segundos.\nA media de menssagens recebidas é %.2f segundos.\nA media de menssagens enviadas é %.2f segundos.\n",grupoMedia->calls.duration, grupoMedia->calls.made, grupoMedia->calls.missed, grupoMedia->calls.received, grupoMedia->message.received, grupoMedia->message.sent);
				write(fdSub,strS, BUF_SIZE-1);
				read(fdSub, strS, BUF_SIZE-1);
		}
		else{
			//verifica se houve alguma mudança nos dados que o cliente quer ser informado
			for(int i = 0; i < nrSub; i++){
				if(dados[i] == ret)
					avanca = 1;
			}
			if(avanca){
				write(fdSub,"\n\n[ ATUALIZACAO DE MEDIAS ]\n\n", BUF_SIZE-1);
				read(fdSub, strS, BUF_SIZE-1);
				for(int i = 0; i < nrSub; i++){
					if(dados[i] == 1){
					sprintf(strS,"A media da duração das chamadas é %.2f segundos.\n",grupoMedia->calls.duration);
					write(fdSub,strS, BUF_SIZE-1);
					read(fdSub, strS, BUF_SIZE-1);
					}
					if(dados[i] == 2){
					sprintf(strS,"A media de chamadas feitas é %.2f segundos.\n", grupoMedia->calls.made);
					write(fdSub,strS, BUF_SIZE-1);
					read(fdSub, strS, BUF_SIZE-1);
					}
					if(dados[i] == 3){
					sprintf(strS,"A media de chamadas perdidas é %.2f segundos.\n", grupoMedia->calls.missed);
					write(fdSub,strS, BUF_SIZE-1);
					read(fdSub, strS, BUF_SIZE-1);
					}
					if(dados[i] == 4){
					sprintf(strS,"A media de chamadas recebidas é %.2f segundos.\n", grupoMedia->calls.received);
					write(fdSub,strS, BUF_SIZE-1);
					read(fdSub, strS, BUF_SIZE-1);
					}
					if(dados[i] == 5){
					sprintf(strS,"A media de menssagens recebidas é %.2f segundos.\n", grupoMedia->message.received);
					write(fdSub,strS, BUF_SIZE-1);
					read(fdSub, strS, BUF_SIZE-1);
					}
					if(dados[i] == 6){
					sprintf(strS,"A media de menssagens enviadas é %.2f segundos.\n", grupoMedia->message.sent);
					write(fdSub,strS, BUF_SIZE-1);
					read(fdSub, strS, BUF_SIZE-1);
					}
				}
			}
		}
		calculaMedia(mediaAnt, cabeca);
		avanca = 0;
		sleep(20);
	}
}

void erro(char *msg){
	printf("Erro: %s\n", msg);
	exit(-1);
}
