/* 단어를 추측하는 게임 guessWord를 제공하는 서버 프로그램
   작성 일시 : 2020-10-18 03:17
   
   [주요 기능]
    - 클라이언트가 접속 시 5개의 단어 중 임의의 단어를 선택하여 퀴즈 게임을 제공
    - 클라이언트가 알파벳 하나를 입력 시, 그 알파벳이 단어에 포함되는지 여부를 확인
    - 만약 단어에 클라이언트가 입력한 알파벳이 포함되어 있으면 해당 문자 위치에 알파벳을 표기하여 힌트 제공
    - 클라이언트가 문자열을 입력 시, 해당 문자열이 퀴즈 정답인지 확인
    - 만약 모든 알파벳을 맞추거나 문자열로 한번에 해당 단어를 맞추면 게임이 끝남
    - 게임이 끝날 시, 최종 시도 횟수와 해당 단어를 클라이언트에 전송

    [구현 방식]
    - 서버는 부모/자식 프로세스로 나뉘어 동작
    - 부모 프로세스는 저장된 5개의 word bank에서 랜덤한 단어를 추출하여 해당 단어의 길이를 자식 프로세스에 IPC 통신을 통해 전달
    - 자식 프로세스는 단어 길이를 클라이언트에 전달
    - 클라이언트가 추측한 알파벳 혹은 단어를 입력하면 자식 프로세스가 해당 데이터를 전송받음
    - 전송받은 데이터를 IPC 통신을 통해 부모에 프로세스에 전달
    - 부모는 전달된 데이터를 정답 단어와 비교하여 결과값을 IPC 통신을 통해 자식 프로세스에 전달
    - 자식 프로세스는 부모 프로세스로부터 전달받은 결과값을 클라이언트에 전달
    - 퀴즈 단어를 클라이언트가 맞출 시, 부모 프로세스로부터 전달받은 정답과 최종 시도 횟수를 클라이언트에 전달
*/
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
    char word_bank[WORD_CNT][BUFSIZE] = {"ickle", "galaxy", "mermaid", "counseling", "dandelion"}; // 맞춰야하는 문자열을 저장하는 변수
    char q_word[BUFSIZE];                                                                          // 단어 저장소에서 꺼낸 문자열이 담길 변수
    char print_str[BUFSIZE];                                                                       // 현재까지 맞춘 단어를 보여주는 변수

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
            char send_str[BUFSIZE]; // 포맷팅을 위한 변수

            strcpy(q_word, word_bank[index]);

            for (int i = 0; i < strlen(q_word); i++)
                print_str[i] = '_';

            print_str[strlen(q_word)] = '\0';

            word_len = strlen(q_word);

            printf("정답 문자열 : %s\n", q_word);

            write(fd1[1], &word_len, sizeof(int)); // 문자 수 전달

            try_cnt = 0;
            while (true)
            {
                read(fd2[0], clnt_str, BUFSIZE);

                // 단어 입력 시
                if (strlen(clnt_str) == 1)
                {
                    int flag = false;
                    for (int j = 0; j < strlen(q_word); j++)
                        if (clnt_str[0] == q_word[j])
                        {
                            print_str[j] = clnt_str[0];
                            flag = true;
                        }
                    if(flag == false)
                        write(fd1[1], "퀴즈에 해당 문자가 없습니다. 다른 문자를 입력해보세요!", BUFSIZE);
                }
                else
                {
                    if (strcmp(clnt_str, q_word) == 0)
                        strcpy(print_str, clnt_str);
                    else
                        write(fd1[1], "오답입니다!. 다른 단어를 입력해보세요!", BUFSIZE);
                }
                try_cnt++;

                if (strcmp(q_word, print_str) == 0)
                {
                    write(fd1[1], "축하드립니다! 정답입니다!", BUFSIZE);
                    sprintf(send_str, "맞힌 단어 : %s", print_str);
                    write(fd1[1], send_str, BUFSIZE);
                    sprintf(send_str, "시도 횟수 : %d", try_cnt);
                    write(fd1[1], send_str, BUFSIZE);
                    write(fd1[1], "프로그램을 종료합니다.", BUFSIZE);
                    write(fd1[1], "", BUFSIZE);
                    break;
                }

                sprintf(send_str, "힌트 : %s", print_str);
                write(fd1[1], send_str, BUFSIZE);
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