#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define MAX_CNT 100
#define BUF_SIZE 100



typedef struct clnt_userdata{
  char UID[30];
  int socket_index;
;
}clnt_userdata;

void error_handling(char *msg);
void *Relay_clnt(void *arg);

int recv_cnt  = 0; // 접속한 recv클라이언트 숫자
int handle_cnt = 0; // 접속한 handle클라이언트 숫자

clnt_userdata handle_userdata[MAX_CNT];
clnt_userdata recv_userdata[MAX_CNT];

int recv_socks[MAX_CNT];
int handle_socks[MAX_CNT]; // 구분하기전에 들어오는 배열. 

//생성된 쓰레드의 UID를 저장하는 배열
char thread_UID[100][30];
int thread_cnt = 0;

pthread_mutex_t mutex;


int main(int argc, char *argv[]){
	int serv_sock, clnt_sock;
	int insertFlag = -1;	
	int clnt_adr_sz;

	struct sockaddr_in serv_adr, clnt_adr;
	char buffer[BUF_SIZE];
	char UID[30];
	pthread_t thread_id;

	if(argc != 2){
		printf("Usage : %s <port> \n", argv[0]);
		exit(1);
	}


	pthread_mutex_init(&mutex, NULL);
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);

	printf("동작이 되고 있는 것이다. \n");
	printf("Socket : %d \n", serv_sock);

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));
	
	if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr))==-1)
		error_handling("bind() error");	

	if(listen(serv_sock, 5) == -1)
		error_handling("listen() error");
	
	while(1)
	{
		clnt_adr_sz = sizeof(clnt_adr);
		clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);
		
		pthread_mutex_lock(&mutex);
		//임계영역 
		if(read(clnt_sock, buffer, sizeof(buffer)) == -1){
			error_handling("clnt kind matching error");
		}


		// 생각해봐야하는 점, buffer 값이 "0", "1" 이 아닌 값이 read로 읽힐 수 있는가?
		// 소켓 종류를 구분한다.
		if(strcmp(buffer,"0") == 0){
			insertFlag = 0;
		}
		else
		{
			insertFlag = 1;
		}
			
		// UID를 입력 받는다.
		if(read(clnt_sock, buffer, sizeof(buffer)) == -1){
			error_handling("clnt UID read error"); 
		}
		
		strcpy(UID, buffer);
	
		// 1, handle sock 일때,그에 맞는 구조체 배열의 UID를 확인한다.
		if(insertFlag){
			for(int i = 0; i < handle_cnt; i++){
				if(UID == handle_userdata[i].UID){
					strcpy(UID, "0");
					break; // 중복 
				}
			}
			if(strcmp(UID, "0") != 0){
				handle_socks[handle_cnt] = clnt_sock;
				strcpy(handle_userdata[handle_cnt].UID, UID);
				handle_userdata[handle_cnt].socket_index = handle_cnt;
				handle_cnt++;
			}
		}
		// recv 소켓 일 때, 
		else{
			for(int i = 0; i < recv_cnt; i++){
				if(UID == recv_userdata[i].UID){
					strcpy(UID, "0");
					break;
				}
			}
			// 중복 아니라면,
			if(strcmp(UID, "0") != 0)
			{
				recv_socks[recv_cnt] = clnt_sock;
				strcpy(recv_userdata[recv_cnt].UID, UID);
				recv_userdata[recv_cnt].socket_index = recv_cnt;
				recv_cnt++;
			}
		}

		for(int i = 0; i < thread_cnt; i++){
			// 이미 해당하는 UID에 맞는 쓰레드를 
			if(strcmp(UID, thread_UID[i]) == 0){
				strcpy(UID, "0");
				break;
			}
		}

		if(strcmp(UID, "0") == 0){
			// UID = 0, 잘못된 UID, 매칭이 안되는 것,
			// 다음 루틴으로, 
			continue;
		}

				
		strcpy(thread_UID[thread_cnt++], UID);
		pthread_create(&thread_id, NULL, Relay_clnt, (void*)UID);
		pthread_detach(thread_id);
		printf("matching Thread num : %d \n", thread_cnt);
	}

	close(serv_sock);
	return 0;
}



void* Relay_clnt(void* str)
{
	char UID[30];
	clnt_userdata recv_user;
	clnt_userdata handle_user;


	int recv_sock;
	int handle_sock;
	int time=0;
	
	char command_buf[BUF_SIZE];
	char output_buf[1000];
	int command_len;
	int output_len;
	
	strcpy(UID, (char*)str);
	while(1)
	{
		int i=0;

		for(i=0; i<recv_cnt;i++)
		{
			//recv_clnt에 UID 있는 지 검사 
			if(strcmp(UID,recv_userdata[i].UID)==0)
			{
				recv_user=recv_userdata[i];
				break;
			}
		}
			
		if(strcmp(UID,recv_user.UID)!=0)
		{
			continue;
		}
		
		for(i=0;i<handle_cnt;i++)
		{
			//hanldle_clnt에 UID 있는 지 검사 
			if(strcmp(UID,handle_userdata[i].UID)==0)
			{
				handle_user=handle_userdata[i];
				break;
			}
		}
		
		if(strcmp(UID,handle_user.UID)==0)
		{
			break;
		}

		time++;
		sleep(1);

		if(time==300)
		{
			printf("쓰레드 대기시간 초과");
			return;
		}
	}
		
	recv_sock=recv_socks[recv_user.index];
	handle_sock=handle_socks[handle_user.index];
	
	while(1)
	{
		if((command_len=read(handle_sock,command_buf,BUF_SIZE))!=0)
			write(recv_sock,command_buf,command_len);
		else
			printf("handle Client 접속종료");

		if((output_len=read(recv_sock,output_buf,1000))!=0)	
			write(handle_sock,output_buf,output_len);
		else
			printf("recv Client 접속종료");
	}

	return;		
}

void error_handling(char *msg){
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}







