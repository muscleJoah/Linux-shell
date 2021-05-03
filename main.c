#include <stdio.h>
#include <unistd.h>//유닉스에서 사용하는 C컴파일러 헤더파일
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>//파일제어 헤더파일
#define MAXLINE 40

int global_index = 1;

typedef struct {
    char cmnd [MAXLINE];
    int index;
} Cmnd_struct;

typedef struct {
    Cmnd_struct list [10]; /* stores last 10 commands */
    int size;
} History;
char commandBuffer[MAXLINE] = { '\0', };
void inputCommands(char *args[]);//명령어 입력 함수
void freeArguments(char *args[]);//입력된 문자열 메모리 해제 함수
void exitShell(int *should_run);//쉘을 종료함수
void redirectionOut(char *args[]);//redirection(OUT) 함수
void redirectonIn(char *args[]);//redirection(IN) 함수
int background(char *args[]);//백그라운드 설정 함수
int setFlag(char *args[]);
void pipeFunction(char *args[]);//파이프를 생성 함수
void add_to_history(History* history, char command [MAXLINE]);
void display_history(History* history);


int main()
{

    History* history = (History*) calloc(1, sizeof(History)); //10개의 history 저장
    
	char *args[MAXLINE/2 +1] = { '\0', };
	int should_run = 1; //프로그램 나갈지 정하기위한 flag 
	int bg = 0; //백그라운드 설정을 위한 flag
	int flag = 3;

	while (should_run) {
        char buf[255];
        getcwd(buf,255);
		printf("~%s$",buf);
		inputCommands(args);
		//명령어를 입력
		bg = background(args);
		//백그라운드 설정
    


		if (strcmp(args[0], "exit") == 0)
			exitShell(&should_run);//쉘 종료

        else if(strcmp(args[0], "cd") == 0){
            if((chdir(args[1])) == -1)
            printf("No Directory\n");
        }
        else if(strcmp( args[0], "history") == 0)
        {
            display_history(history);
        }
		else {//명령어 수행
            add_to_history(history, commandBuffer);
			pid_t pid;
			pid = fork();
			if (pid == 0) {
				flag = setFlag(args);
				
				switch (flag) {
				case 0:// >
					redirectionOut(args);
					execvp(args[0], args);
					break;
				case 1:// <
					redirectonIn(args);
					execvp(args[0], args);
					break;
				case 2:// |
					pipeFunction(args);
					break;
				case 3://일반적인 command 가 들어왔을떄
					execvp(args[0], args);
					break;
				}
			}
			else {
				freeArguments(args);
				if (bg == 0) wait(NULL);
				else continue;
			}
		}
	}
	return 0;
}

int cnt = 0;
;
void inputCommands(char *args[])//명령어를 입력 함수
{
	int argIndex = 0;
    int cmdIndex = 0;
	
	fgets(commandBuffer, sizeof(commandBuffer), stdin);//명령어 입력
   
	commandBuffer[strlen(commandBuffer) - 1] = 0;
	for (int i = 0, j = 0; i < strlen(commandBuffer) + 1; i++) {
		if (commandBuffer[i] == ' ' || commandBuffer[i] == '\0') {//공백이나 NULL인 경우
			args[argIndex] = (char *)malloc(sizeof(char)*(j + 1));//메모리를 동적으로 할당
			strncpy(args[argIndex], &commandBuffer[i - j], j);//공백이나 NULL 전까지의 문자열을 복사
			args[argIndex++][j] = 0;
			cnt++;
			j = -1;
		}
		j++;
	}

   
}

void freeArguments(char *args[])//문자열의 메모리 해제 함수
{
	int i = 0;
	while (i < cnt) {
		free(args[i]);
		i++;
	}
	cnt = 0;
}

void exitShell(int *should_run)//쉘을 종료 함수
{
	*should_run = 0;
}

void redirectionOut(char *args[])
{
	int i = 0, fd;

	while (strcmp(args[i], ">") != 0)//'>' 와 같을 시 루프 탈출 
		i++;

	if (args[i]) {
		if (args[i + 1] == NULL) return;//문자 > 다음 아무 문자열이 없으면 반환
		else {//파일을 연다
			if ((fd = open(args[i + 1], O_RDWR | O_CREAT | S_IROTH, 0644)) == -1) {
				fprintf(stderr, "%s\n", args[i + 1]);
				return;
			}
		}
		dup2(fd, 1);//출력을 넣어준다
		close(fd);
		args[i] = NULL;
		args[i + 1] = NULL;

		while (args[i] != NULL) {
			args[i] = args[i + 2];
		}
		args[i] = NULL;
	}
}

void redirectonIn(char *args[])
{
	int i = 1, fd;

	while (strcmp(args[i], "<") != 0)
		//'<' 와 같을 시 루프문 탈출 
		i++;

	if (args) {
		if (!args[i + 1]) return;
		else {
			if ((fd = open(args[i + 1], O_RDONLY)) == -1) {
				//파일을 연다
				fprintf(stderr, "%s\n", args[i + 1]);
				return;
			}
		}
		dup2(fd, 0);
		//입력을 넣어준다
		close(fd);

		while (args[i] != NULL) {
			args[i] = args[i + 2];
			i++;
		}
		args[i] = NULL;
	}
}

void pipeFunction(char *args[])
{
	int i = 0, j = 0, fd[2];
	pid_t pid;
	char *argsPipe1[MAXLINE], *argsPipe2[MAXLINE];

	while (args[i] != NULL) {
		//|문자 전의 문자열 주소 복사
		if (strcmp(args[i], "|") == 0) {
			argsPipe1[i] = NULL;
			break;
		}
		argsPipe1[i] = args[i];
		i++;
	}
	i++;
	while (args[i] != NULL) {
		//|문자 이후의 문자열 주소 복사
		argsPipe2[j] = args[i];
		i++; j++;
	}

	//파이프를 생성한다
	if (pipe(fd) == -1) {
		//실패시 에러발생
		fprintf(stderr, "Pipe failed");
		exit(1);
	}
	close(0);//STDIN_FILENO 표준입력 닫기
	close(fd[1]);//입력 파이프 닫기
	dup(fd[0]);//식별자 복사
	close(fd[0]);
	execvp(argsPipe1[0], argsPipe1);//앞쪽 명령어 실행
	exit(1);

	pid = fork();
	if (pid == 0) {
		close(1);
		close(fd[0]);
		dup(fd[1]);
		close(fd[1]);
		execvp(argsPipe2[0], argsPipe2);
		
		exit(0);
	}
}

int background(char *args[])
{
	int i = 0;
	while (args[i] != NULL) {//명령어 끝에 문자 &가 있으면 백그라운드 설정
		if (strcmp(args[i], "&") == 0) {
			free(args[i]);
			args[i] = NULL;
			return 1;
		}i++;
	}
	return 0;
}

int setFlag(char *args[])
{
	int i = 0;
	while (i < cnt) {
		if (strcmp(args[i], ">") == 0) return 0;//redirection >
		else if (strcmp(args[i], "<") == 0) return 1;//redirection <
		else if (strcmp(args[i], "|") == 0) return 2;//파이프
		i++;
	}
	return 3;//일반
}


void add_to_history(History* history, char command[MAXLINE]) 
{
    if((int)command[0] ==  '!')
    {
        if(history->size == 0)
        {
            printf("No commands in history.\n");
        }
        else if((int)command[1] ==  '!') //!!가 커맨드일때
        {
            strcpy(command, history->list[ (history->size) - 1].cmnd) ; //마지막으로 작동한 커맨드
        }
        else //!n일때
        {
            int n = atoi(&command[1]); 
            int i;
            for(i=0; i<10; i++)
            {
                if(n == history->list[i].index)
                {
                    strcpy(command, history->list[i].cmnd); 
                    add_to_history(history, command); //!n명령어로 작성한 command 를 history stack 위로 올린다.
                    printf("\nhistory> %s\n", history->list[i].cmnd);
                    return;
                }
            }
            printf("No such command in history.\n");
        }
        return;
    }
    
    Cmnd_struct* temp = (Cmnd_struct*) calloc(1, sizeof(Cmnd_struct));
    temp->index = global_index++;
    strcpy(temp->cmnd, command); /* temp->cmnd = command */
    
    if(history->size < 10) 
    {
        history->list[(history->size)++] = *temp;
    }
    else // history list가 가득차 있을때
    {
        int i;
        for(i=0; i<10; i++)
        {
            history->list[i] = history->list[i+1];  
        }
        history->list[9] = *temp; //새로운 커맨드 삽입
    }
}



void display_history(History* history)
{
    if(history->size == 0)
    {
        printf("No commands in history.\n");
    }
    else
    {
        int i;
        for(i=history->size-1; i>=0; i--)
        {
            printf("%d %s\n", history->list[i].index, history->list[i].cmnd);
        }
    }

}
 