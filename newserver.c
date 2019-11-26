#include "common.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/sem.h>
#include <signal.h>        /* For signal() */
#define MAX_PLAYERS 4

void* play_game_one(void *data);
int card_values[52];
int card_suits[52];
int ncard;
pthread_mutex_t card_mutex;
int players_hand_values[MAX_PLAYERS][20], dealers_hand_values[MAX_PLAYERS][20];
int players_hand_suits[MAX_PLAYERS][20], dealers_hand_suits[MAX_PLAYERS][20];
int nplayers[MAX_PLAYERS], ndealers[MAX_PLAYERS];

int             gnShmID1;      /* Shared Memory Indicator */
int             gnSemID1;      /* Semapore Indicator */

union semun
{
        int val;
        struct semid_ds *buf;
        unsigned short int *array;
};

void *set_shutdown ()
{
        printf("[SIGNAL] : Got shutdown signal\n");
        shmctl( gnShmID1, IPC_RMID, 0 );
        printf("[SIGNAL] : Shared meory segment marked for deletion\n");;
        semctl( gnSemID1, IPC_RMID, 0 );
        printf("[SIGNAL] : Semapore segment marked for deletion\n");
        exit (1);
}

main()
{
  int id;
  pthread_t threads[MAX_PLAYERS];
  /* Shared Memory */
  key_t keyShm;       /* Shared Memory Key */
  key_t keySem;       /* Semapore Key */
  _ST_SHM *pstShm1;      /* 공용 메모리 구조체 */
  init_cards();
  int count = 1;
  keyShm = (key_t)60100;
  keySem = (key_t)60101;
  char buffer[BUFFER_SIZE];
  union semun sem_union;
  struct sembuf mysem_open  = {0, -1, SEM_UNDO}; // 세마포어 얻기
  struct sembuf mysem_close = {0, 1, SEM_UNDO};  // 세마포어 돌려주기
  /******************************************
   * 공유 메모리 사용
   ******************************************/
  /* 공유 메모리 세그먼트를 연다 - 필요하면 만든다. */
  if( (gnShmID1 = shmget( keyShm, SEGSIZE, IPC_CREAT | IPC_EXCL | 0666 )) == -1 )
  {
          printf("Shared memory segment exist - opening as client\n");
          /* Segment probably already exists - try as a client */
          if( (gnShmID1 = shmget( keyShm, SEGSIZE, 0 )) == -1 )
          {
                  perror("shmget");
                  exit(1);
          }
  }
  else
  {
          printf("Creating new shared memory segment\n");
  }

  /* 현재 프로세스에 공유 메모리 세그먼트를 연결한다. */
  if( (pstShm1 = (_ST_SHM *)shmat(gnShmID1, 0, 0)) == NULL )
  {
          perror("shmat");
          exit(1);
  }

  /* 공유 Semapore 세그먼트를 연다 - 필요하면 만든다. */
  if( (gnSemID1 = semget( keySem, 1, IPC_CREAT | IPC_EXCL | 0666 )) == -1 )
  {
    printf("Semapore segment exist - opening as client\n");
    /* Segment probably already exists - try as a client */
    if( (gnSemID1 = semget( keySem, 0, 0 )) == -1 )
    {
      perror("semget");
      exit(1);
    }
  }
  else
  {
    printf("Creating new semapore segment\n");
  }

  /* Signal 등록 */
  (void) signal (SIGINT, (void (*)()) set_shutdown);

  /* 공유 메모리 초기화 */
  pstShm1->check = 0;
  /* Semapore 초기화 */
  sem_union.val = 1;
  semctl( gnSemID1, 0, SETVAL, sem_union );

  while(1) {
     if(pstShm1->check) {
        if(strncmp(pstShm1->data, "start", 5) == 0){
          if( semop(gnSemID1, &mysem_open, 1) == -1 )
          {
                 perror("semop");
                 exit(1);
          }
          printf("start! : %s\n", pstShm1->data);
          pthread_create(&threads[count], NULL, play_game_one, (void*) count);


          printf("count: %d\n", count);
          sprintf(buffer, "%d", count);
          //while(!pstShm1->check);

          strcpy(pstShm1->data, buffer);


          pstShm1->check = 0;
          count+=1;
          semop(gnSemID1, &mysem_close, 1);

      }
    }
  }
}

void* play_game_one(void *data)
{
  int gnShmID2;      /* Shared Memory Indicator */
  int gnSemID2;      /* Semapore Indicator */
  int id = (int)data;
  int shmId = (int)data + 60100;
  int semId = (int)data+1 +60100;
  char buffer[BUFFER_SIZE];
  int nwritten;
  int player_sum, dealer_sum;
  int *player_hand_values = players_hand_values[id], *dealer_hand_values = dealers_hand_values[id];
  int *player_hand_suits = players_hand_suits[id], *dealer_hand_suits = dealers_hand_suits[id];
  int i;

  /* Shared Memory */
  key_t keyShm;       /* Shared Memory Key */
  _ST_SHM *shared_data;      /* 공용 메모리 구조체 */

  /* Semapore */
  key_t keySem;       /* Semapore Key */
  union semun sem_union;
  struct sembuf mysem_open  = {0, -1, SEM_UNDO}; // 세마포어 얻기
  struct sembuf mysem_close = {0, 1, SEM_UNDO};  // 세마포어 돌려주기

  /* Shared Memory의 키값을 생성한다. */
  keyShm = (key_t)shmId;
  /* Semapore 키값을 생성한다. */
  keySem = (key_t)semId;
  /* 공유 메모리 세그먼트를 연다 - 필요하면 만든다. */
  if((gnShmID2 = shmget( keyShm, SEGSIZE, IPC_CREAT | IPC_EXCL | 0666 )) == -1)
  {
    printf("Shared memory segment exist - opening as client\n");
    /* 세그먼트가 이미 존재한다면 - try as a client */
    if( (gnShmID2 = shmget( keyShm, SEGSIZE, 0 )) == -1 )
    {
      perror("shmget");
      exit(1);
          }
  }
  else
  {
    printf("Creating new shared memory segment\n");
  }
  /* 현재 프로세스에 공유 메모리 세그먼트를 연결한다. */
  if((shared_data = (_ST_SHM *)shmat(gnShmID2, 0, 0)) == NULL)
  {
    perror("shmat");
    exit(1);
  }
  /* 공유 Semapore 세그먼트를 연다 - 필요하면 만든다. */
  if((gnSemID2 = semget( keySem, 1, IPC_CREAT | IPC_EXCL | 0666 )) == -1)
  {
    printf("Semapore segment exist - opening as client\n");
    /* 세그먼트가 이미 존재한다면 - try as a client */
    if( (gnSemID2 = semget( keySem, 0, 0 )) == -1 )
    {
      perror("semget");
      exit(1);
    }
  }
  else
  {
    printf("Creating new semapore segment\n");
  }

  /* 공유 메모리 초기화 */
  shared_data->check = 0;
  shared_data->check2 = 0;
  shared_data->finalcheck=0;
  /* Semapore 초기화 */
  sem_union.val = 1;
  semctl( gnSemID2, 0, SETVAL, sem_union );

  /*첫 카드 뽑기*/
  nplayers[id] = 0;
	ndealers[id] = 0;
  /*딜러들의 카드덱은 공유 - mutex로 raceCondition 제어*/
  pthread_mutex_lock(&card_mutex);
	{
		for (i = 0; i < 2; ++i)
		{
			player_hand_values[nplayers[id]] = card_values[ncard];
			player_hand_suits[nplayers[id]] = card_suits[ncard];
			++ncard;
			++nplayers[id];
		}

		dealer_hand_values[ndealers[id]] = card_values[ncard];
		dealer_hand_suits[ndealers[id]] = card_suits[ncard];
		++ncard;
		++ndealers[id];
	}
	pthread_mutex_unlock(&card_mutex);

  buffer[0] = value_codes[player_hand_values[0]];
	buffer[1] = suit_codes[player_hand_suits[0]];
	buffer[2] = value_codes[player_hand_values[1]];
	buffer[3] = suit_codes[player_hand_suits[1]];
	buffer[4] = value_codes[dealer_hand_values[0]];
	buffer[5] = suit_codes[dealer_hand_suits[0]];
	buffer[6] = 0;

  /******* 임계영역 *******/
  if( semop(gnSemID2, &mysem_open, 1) == -1 )
	{
		perror("semop");
		exit(1);
	}
  /* shared memory 전송 */
  strncpy(shared_data->data, buffer, BUFFER_SIZE);
	printf("send: first cardset\n");
  shared_data->check = 1;
  semop(gnSemID2, &mysem_close, 1);
  /******* 임계영역 *******/

  while(shared_data->check) {
     sleep(1);
     printf("waiting...\n");
   }
  /* 첫 카드를 주고서*/
  printf("\n");
  printf("Player 1 Hand: " );
  display_state(player_hand_values, player_hand_suits, nplayers[id]);
  printf("Dealer Hand with player 1: ");
  display_state(dealer_hand_values, dealer_hand_suits, ndealers[id]);

  while(TRUE){
    if(shared_data->check){
      if( semop(gnSemID2, &mysem_open, 1) == -1 ) // 대기상태
      {
        perror("semop");
        exit(1);
      }
    }
    /* HIT, STAND signal 빋기*/
    if(shared_data->check)
    {
      strncpy(buffer, shared_data->data, BUFFER_SIZE);
      printf("I received from player 1: %s\n", buffer);
      strncpy(shared_data->data, "\0", BUFFER_SIZE);
      /* HIT 결과 전송 */

      if (strcmp(buffer, HIT) == 0)
      {
        pthread_mutex_lock(&card_mutex);
        {
          player_hand_values[nplayers[id]] = card_values[ncard];
          player_hand_suits[nplayers[id]] = card_suits[ncard];

          buffer[0] = value_codes[player_hand_values[nplayers[id]]];
          buffer[1] = suit_codes[player_hand_suits[nplayers[id]]];
          buffer[2] = 0;

          ++ncard;
          ++nplayers[id];
        }
        pthread_mutex_unlock(&card_mutex);

        strncpy(shared_data->data, buffer, BUFFER_SIZE);
        printf("I send to player 1: %s\n", buffer);
        shared_data->check=0;
        buffer[0] = '\0';
        semop(gnSemID2, &mysem_close, 1);

        /* 플레이어 결과가 21이 넘을 경우*/
        if (calc_sum(player_hand_values, nplayers[id]) > 21)
        {
          printf("Player 1 busted. Dealer wins.\n");
          return NULL;
        }
        /* 플레이어 결과가 21일 경우 이기기 때문에 break */
        else if (calc_sum(player_hand_values, nplayers[id]) == 21)
          break;
      }
      /* 플레이어의 signal이 STAND인 경우 stop*/
      else if (strcmp(buffer, STAND) == 0){
        semop(gnSemID2, &mysem_close, 1);
        break;
      }
    }
  }
  i=0;
  while (calc_sum(dealer_hand_values, ndealers[id]) < 17)
  {
    dealer_hand_values[ndealers[id]] = card_values[ncard];
    dealer_hand_suits[ndealers[id]] = card_suits[ncard];
    buffer[i++] = value_codes[dealer_hand_values[ndealers[id]]];
    buffer[i++] = suit_codes[dealer_hand_suits[ndealers[id]]];
    ++ncard;
    ++ndealers[id];
  }
  buffer[i] = 0;

  printf("\n");
  printf("Player 1 Hand: ");
  display_state(player_hand_values, player_hand_suits, nplayers[id]);
  printf("Dealer 1 Hand: ");
  display_state(dealer_hand_values, dealer_hand_suits, ndealers[id]);

  if( semop(gnSemID2, &mysem_open, 1) == -1 )
  {
          perror("semop");
          exit(1);
  }
  strncpy(shared_data->data, buffer, BUFFER_SIZE);
  shared_data->finalcheck=1;
  semop(gnSemID2, &mysem_close, 1);

  player_sum = calc_sum(player_hand_values, nplayers[id]);
  dealer_sum = calc_sum(dealer_hand_values, ndealers[id]);

  if (dealer_sum > 21)
    printf("\nDealer busted! Player %d wins!\n", id);
  else if (player_sum == dealer_sum)
    printf("\nPlayer %d and dealer have the same score. It's a push!\n", id);
  else if (player_sum < dealer_sum)
    printf("\nDealer has a higher score than player %d. Dealer wins!\n", id);
  else
    printf("\nPlayer %d has a higher score. Player wins!\n", id);

    /* 세마포어, shared memory 제거*/
  if(shmdt(shared_data) == -1) {
     perror("shmdt failed");
     exit(1);
  }
}
