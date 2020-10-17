/* 단어를 추측하는 게임 guessWord를 제공하는 클라이언트 프로그램
    작성 일시 : 2020-10-18 03:17

    [주요 기능]
     - 서버가 작동중인 경우, 서버와 통신을 통해 단어를 추측하는 게임 제공
     - 서버로부터 퀴즈를 제공받아 정답을 맞출 수 있음
     - 최초 실행 시, 퀴즈 단어의 길이를 힌트로 제공받음
     - 제공받은 단어 길이를 통해 알파벳 혹은 문자열을 입력
     - 대문자와 소문자가 섞여있을 시, 자동으로 소문자로 변환하여 서버에 전달
     - 이미 입력한 알파벳 집합을 힌트로 제공하여 문제 해결을 도와줌
     - 알파벳이 아닌 문자 혹은 문자열 입력 시, 다시 입력하도록 함
     - 이미 입력했던 알파벳이나 알파벲이 아닌 문자/문자열을 입력 시, 시도 횟수에 포함되지 않음.
     - 최종적으로 단어를 맞춘 경우, 맞춘 단어와 함께 최종 시도 횟수를 제공
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define BUFSIZE 100
#define AlPHA_SIZE 24

void error_handling(char *message);

int main(int argc, char *argv[])
{
    char aleady_input[AlPHA_SIZE] = {""}; // 입력했던 데이터 표시를 위한 변수
    int alin_cnt = 0;                     // 이미 입력한 문자열의 인덱스를 저장
    char str[BUFSIZE];                    // 사용자가 입력한 정답을 받을 문자열 변수
    char serv_message[BUFSIZE];           // 서버에서 보낸 문자열을 받을 변수

    int sock;
    char message[BUFSIZE]; // 서버에서 온 메시지를 받을 변수
    int str_len = 0;

    bool flag = true;

    struct sockaddr_in serv_addr;

    if (argc != 3)
    {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    sock = socket(PF_INET, SOCK_STREAM, 0);

    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error!");

    read(sock, &str_len, sizeof(int));
    printf("정답 문자 길이 : %d\n", str_len);
    str_len = 0;

    while (true)
    {
        // 이미 입력핸던 문자에 대해 가이드 제공
        if (alin_cnt != 0)
            printf("이미 입력했던 문자들 : [ ");
        for (int a = 0; a < alin_cnt; a++)
            printf("%c ", aleady_input[a]);
        if (alin_cnt != 0)
            printf("]\n");

        // 정답 입력받기
        printf("알파벳 혹은 단어 전체를 입력하세요: ");
        scanf("%s", str);

        bool check_alpha = true;
        // 입력받은 문자열을 소문자로 바꾸고 알파벳인지 검사!
        for (int k = 0; k < strlen(str); k++)
        {

            if (!isalpha(str[k])) // 알파벳인지 검사
            {
                check_alpha = false;
                break;
            }
            str[k] = tolower(str[k]); // 전부 소문자로 변환
        }

        // 입력한 문자나 문자열에 알파벳이 아닌 문자가 섞여있으면 건너뜀!
        if (!check_alpha)
        {
            printf("알파벳만 입력해주세요!\n");
            continue;
        }
        // 이미 입력한 문자열일시 건너뜀!
        if (strlen(str) == 1)
        {
            bool check_aleady = true;
            for (int b = 0; b < alin_cnt; b++)
            {
                if (aleady_input[b] == str[0])
                {
                    printf("이미 입력한 적이 있는 문자입니다!\n");
                    check_aleady = false;
                    break;
                }
            }
            if (!check_aleady)
                continue;
        }

        // 가이드라인에 입력한 단어를 저장
        if (strlen(str) == 1)
        {
            aleady_input[alin_cnt] = str[0];
            alin_cnt++;
        }

        write(sock, str, BUFSIZE);

        while (true)
        {
            read(sock, serv_message, BUFSIZE);
            if (strcmp(serv_message, "") == 0)
                break;
            printf("%s\n", serv_message);

            if (strcmp(serv_message, "프로그램을 종료합니다.") == 0)
                flag = false;
        }

        if (flag == false)
            break;
            
    }

    close(sock);
    return 0;
}

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}