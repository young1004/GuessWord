#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>

#define WORD_CNT 5
#define BUFSIZE 100

void error_handling(char *message);
void z_handler(int sig);

int main(int argc, char *argv[])
{
    /* 행맨 게임을 위한 변수들 */
    char word_bank[WORD_CNT][BUFSIZE] = {"apple", "banana", "bigdata", "android", "mermaid"}; // 맞춰야하는 문자열을 저장하는 변수
    char q_word[BUFSIZE];                                                                     // 단어 저장소에서 꺼낸 문자열이 담길 변수
    char print_str[BUFSIZE];                                                                  // 현재까지 맞춘 단어를 보여주는 변수

    int try_cnt = 0; // 시도 횟수

    /* 서버와 클라이언트 연결을 위한 변수 */
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;

    pid_t pid;
    struct sigaction act;
    int str_len, state, addr_size;

    // IPC 통신을 위한 변수
    int fd1[2], fd2[2];
    char buffer[BUFSIZE];

    srand(time(NULL));

    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    if (pipe(fd1) < 0 || pipe(fd2) < 0)
        error_handling("Pipe() error!!");

    act.sa_handler = z_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    state = sigaction(SIGCHLD, &act, 0);
    if (state != 0)
        error_handling("sigaction() error");

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    // 항시 켜져있는 서버
    while (true)
    {
        addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &addr_size);
        if (clnt_sock == -1)
            continue;

        pid = fork();
        if (pid == -1)
        {
            close(clnt_sock);
            continue;
        }
        else if (pid > 0) // 부모 프로세스
        {
            close(clnt_sock);

            int index = rand() % WORD_CNT;
            int word_len = 0;
            char clnt_str[BUFSIZE]; // 클라이언트가 입력한 정답을 입력받을 변수
            
            strcpy(q_word, word_bank[index]);

            for (int i = 0; i < strlen(q_word); i++)
                print_str[i] = '_';

            print_str[strlen(q_word)] = '\0';

            word_len = strlen(q_word);

            printf("정답 문자열 : %s\n", q_word);
            printf("가려진 문자열 : %s\n", print_str);

            write(fd1[1], &word_len, sizeof(int)); // 문자 수 전달

            while (true)
            {
                read(fd2[0], clnt_str, BUFSIZE);

                // 단어 입력 시
                if (strlen(clnt_str) == 1)
                {
                    for (int j = 0; j < strlen(q_word); j++)
                        if (clnt_str[0] == q_word[j])
                            print_str[j] = clnt_str[0];
                }
                else
                {
                    if (strcmp(clnt_str, q_word) == 0)
                        strcpy(print_str, clnt_str);
                }
                try_cnt++;

                if (strcmp(q_word, print_str) == 0)
                {
                    char send_str[BUFSIZE]; // 포맷팅을 위한 변수

                    write(fd1[1], "축하드립니다! 정답입니다!", BUFSIZE);
                    sprintf(send_str, "맞힌 단어 : %s", print_str);
                    write(fd1[1], send_str, BUFSIZE);
                    sprintf(send_str, "시도 횟수 : %d", try_cnt);
                    write(fd1[1], send_str, BUFSIZE);
                    write(fd1[1], "프로그램을 종료합니다.", BUFSIZE);
                    write(fd1[1], "", BUFSIZE);
                    break;
                }

                write(fd1[1], print_str, BUFSIZE);
                write(fd1[1], "", BUFSIZE);
            }
        }
        else // 자식 프로세스
        {
            close(serv_sock);
            bool flag = true;

            read(fd1[0], buffer, sizeof(int));
            write(clnt_sock, buffer, sizeof(int));

            while (true)
            {
                read(clnt_sock, buffer, BUFSIZE);
                write(fd2[1], buffer, BUFSIZE);

                while (true)
                {
                    read(fd1[0], buffer, BUFSIZE);
                    write(clnt_sock, buffer, BUFSIZE);
                    printf("buffer 값 : %s\n", buffer);
                    
                    if (strcmp(buffer, "프로그램을 종료합니다.") == 0)
                        flag = false;

                    if (strcmp(buffer, "") == 0)
                        break;
                }
                if (flag == false)
                    break;
            }
        }
    }
    return 0;
}

void z_handler(int sig)
{
    pid_t pid;
    int status;

    pid = waitpid(-1, &status, WNOHANG);
    printf("removed proc id: %d \n", pid);
    printf("Returned data : %d \n\n", WEXITSTATUS(status));
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}