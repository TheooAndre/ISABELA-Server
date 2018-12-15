/*************************************************************
 * CLIENTE liga ao servidor (definido em argv[1]) no porto especificado  
 gcc ProjetoCLIENTE.c -Wall -o cliente
 ./cliente 0 9000
 *************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#define BUF_SIZE	1024
#define SERVER_PORTSUB     9020
#define ID_SIZE 50

void erro(char *msg);

void clrscr(){
    system("@cls||clear");
}

//recebe e faz print das notificações
void notifica(int subs_fd){
  char buffer[BUF_SIZE];
  int nread = 0;
  while(1){
    nread = read(subs_fd, buffer, BUF_SIZE - 1); //1
    buffer[nread] = '\0';
    printf("%s", buffer);
    write(subs_fd,"lido",BUF_SIZE-1);
  }
}

int main(int argc, char *argv[]) {
  char id[ID_SIZE];
  int nread = 0;
  char endServer[100];
  int fd, fdSub;
  struct sockaddr_in addr, addrSub;
  struct hostent *hostPtr;
  char buffer[BUF_SIZE];
  char ip[BUF_SIZE];
  char comando[100];
  int dados[10];
  int nrDados = 0;
  int nrSub = 0, opcSub = 0;

  if (argc != 3) {
    printf("cliente <host> <port>\n");
  	exit(-1);
  }

  strcpy(endServer, argv[1]);
  if ((hostPtr = gethostbyname(endServer)) == 0)
    erro("Nao consegui obter endereço");

  bzero((void *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  addr.sin_port = htons((short) atoi(argv[2]));

  if((fd = socket(AF_INET,SOCK_STREAM,0)) == -1)
    erro("socket");
  if( connect(fd,(struct sockaddr *)&addr,sizeof (addr)) < 0)
    erro("Connect");

  inet_ntop(AF_INET,bzero,ip,BUF_SIZE);

  bzero((void *) &addrSub, sizeof(addrSub));
  addrSub.sin_family = AF_INET;
  addrSub.sin_addr.s_addr = ((struct in_addr *)(hostPtr->h_addr))->s_addr;
  addrSub.sin_port = htons(SERVER_PORTSUB);
  
  if((fdSub = socket(AF_INET,SOCK_STREAM,0)) == -1)
    erro("socket");
  if( connect(fdSub,(struct sockaddr *)&addrSub,sizeof (addrSub)) < 0)
    erro("Connect");

  inet_ntop(AF_INET,bzero,ip,BUF_SIZE);

  if (fork() == 0) {
      notifica(fdSub);
      exit(0);
    }

  
  while(1) {
    clrscr();
    printf("            --->  Menu  <---\n> Introduza | ID | para definir o seu id.\n> Introduza | DADOS | para visualizar os seus dados.\n> Introduza | GRUPO | para visualizar a media do seu grupo.\n> Introduza | SUBS | para ativar ou desativar a subscrição.\n> Introduza | SAIR | para sair do programa.\n");
    printf("\n>");
    scanf("%s",comando);
    //recebe um comando do cliente e envia-o para o servidor
    write(fd, comando, 1 + strlen(comando)); //1
    nread = read(fd, buffer, BUF_SIZE - 1); //2
    buffer[nread] = '\0';
    printf("\n%s\n",buffer);

    if(strcmp(comando,"SAIR") == 0) {
		  exit(0);
    }

    if(strcmp(comando,"ID") == 0){
    	printf("\n>");
      scanf("%s", id);
      printf("\n>");
      write(fd, id, 1 + strlen(id));  //3
      nread = read(fd, buffer, BUF_SIZE - 1); //4
      buffer[nread] = '\0';
      printf("%s\n",buffer);
      printf("\n\n [ Prima ENTER para voltar ao menu ]\n");
      getchar();
      getchar();
    }

    if(strcmp(comando,"DADOS") == 0){
      if(strcmp(buffer, "Erro: ID nao definido! Use o comando 'ID' para o definir.") == 0){
      }else{
        clrscr();
        printf("Quantos dados deseja visualizar? Se introduzir 11 irá visualizar todos os dados\n");
        scanf("%d", &nrDados);
        sprintf(buffer, "%d",nrDados);
        write(fd,buffer, 1 + strlen(buffer));
        if(nrDados == 11){
          nread = read(fd,buffer,BUF_SIZE-1);
          buffer[nread] = '\0';
          printf("%s\n",buffer);
          }
        else{
          read(fd,buffer,BUF_SIZE-1);
          printf("Introduza o/os números que designam o/os dado/s que quer visualizar\n1  | O seu ID\n2  | A sua atividade\n3  | A sua localização\n4  | A sua duração de chamadas\n5  | A quantidade de chamadas que efectou\n6  | A qantidade de chamadas perdidas\n7  | A quantidade de chamadas recebidas\n8  | O seu departamento\n9  | A quantidade de sms recebidos nos ultimos 5 segundos\n10 | A quantidade de sms enviados nos ultimos 5 segundos\n");
          for(int j = 0; j < nrDados; j++){
            printf("\n>");
            scanf("%d", &dados[j]);
          }
          printf("\n");
          for(int i = 0; i < nrDados; i++){
            sprintf(buffer, "%d",dados[i]);
            write(fd,buffer, 1 + strlen(buffer));
            read(fd,buffer,BUF_SIZE-1);
            printf("%s\n",buffer);
          }
        }
      }
      printf("\n\n [ Prima ENTER para voltar ao menu ]\n");
        getchar();
        getchar();
    }
    if(strcmp(comando,"GRUPO") == 0){
      for(int i = 0; i < 5; i++){
          write(fd,"lido",1 + strlen("lido")); //3
          nread = read(fd,buffer,BUF_SIZE-1);
          buffer[nread] = '\0';
          printf("%s\n",buffer);
        }
      printf("\n\n [ Prima ENTER para voltar ao menu ]\n");
      getchar();
      getchar();
    }

    if(strcmp(comando,"SUBS") == 0){
    	if(strcmp(buffer, "Erro: ID nao definido! Use o comando 'ID' para o definir.") == 0){
    		printf("\n\n [ Prima ENTER para voltar ao menu ]\n");
      	getchar();
      	getchar();
      }else {
      	printf("\n>");
      	scanf("%s", buffer);
      	write(fd, buffer, 1 + strlen(buffer)); //3
    		read(fd,ip,BUF_SIZE-1); //4
    		if(strcmp(ip, "Deixar de subs") == 0){
    			printf("\n\n [ Prima ENTER para voltar ao menu ]\n");
      		getchar();
      		getchar();
    		}else{
	      	if(strcmp(buffer, "Y") == 0 || strcmp(buffer, "y") == 0){
	      		printf("Quantos dados deseja visualizar? Se introduzir 7 irá ser notificado de todos os dados\n");
	      		scanf("%d", &nrSub);
	        		sprintf(buffer, "%d",nrSub);
	        		write(fd,buffer, 1 + strlen(buffer));
	      		if(nrSub < 7 ){
	      			printf("Introduza o/os números que designam o/os dado/s que quer visualizar\n1 | A sua duração de chamadas\n2 | A quantidade de chamadas que efectou\n3 | A qantidade de chamadas perdidas\n4 | A quantidade de chamadas recebidas\n5 | A quantidade de sms recebidos nos ultimos 5 segundos\n6 | A quantidade de sms enviados nos ultimos 5 segundos\n");
	      			for(int i = 0; i < nrSub; i++){
	      				printf("\n>");
	      				fflush(stdout);
	            		scanf("%d", &opcSub);
	        				sprintf(buffer, "%d",opcSub);
	            		read(fd,ip,BUF_SIZE-1);
	            		write(fd, buffer, BUF_SIZE-1);
	      			}
	      		}
    			}
      	}
      }
    }
  }
  close(fd);
  exit(0);
}

void erro(char *msg){
	printf("Erro: %s\n", msg);
	exit(-1);
}
