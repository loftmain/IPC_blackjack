#include "common.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>


/******************************************

 * 전역 변수 선언

 ******************************************/
int             gnShmID1;      /* Shared Memory Indicator */
int             gnShmID2;      /* Shared Memory Indicator */
int             gnSemID1;      /* Semapore Indicator */
int             gnSemID2;      /* Semapore Indicator */


void play_game(int id)
{
  int shmId = id + 60100;
  int semId = id+1 +60100;
  int my_hand_values[20], dealer_hand_values[20];
	int my_hand_suits[20], dealer_hand_suits[20];
	int nmy = 0, ndealer = 0;
	char buffer[BUFFER_SIZE];
	int nwritten;

  /* Shared Memory */
  key_t        keyShm;       /* Shared Memory Key */
  _ST_SHM        *pstShm2;      /* 공용 메모리 구조체 */

  /* Semapore */
  key_t        keySem;       /* Semapore Key */
  struct sembuf mysem_open  = {0, -1, SEM_UNDO}; // 세마포어 얻기
  struct sembuf mysem_close = {0, 1, SEM_UNDO};  // 세마포어 돌려주기

  keyShm = (key_t)shmId;

  /* 공유 메모리 세그먼트를 연다 - 필요하면 만든다. */
  if( (gnShmID2 = shmget( keyShm, SEGSIZE, IPC_CREAT | IPC_EXCL | 0666 )) == -1 )
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
  if( (pstShm2 = (_ST_SHM *)shmat(gnShmID2, 0, 0)) == NULL )
  {
          perror("shmat");
          exit(1);
  }

  /* Semapore 키값을 생성한다. */
  keySem = (key_t)semId;

  /* 공유 Semapore 세그먼트를 연다 - 필요하면 만든다. */
  if( (gnSemID2 = semget( keySem, 1, IPC_CREAT | IPC_EXCL | 0666 )) == -1 )
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

  /* server side - send first cardset */
  printf("게임 시작 대기중...\n");
  printf("client ID : %d\n", id);

  while(!pstShm2->check) {
     sleep(1);
     printf("waiting...\n");
   } // Is it necessary?

   /******* 임계영역 *******/
   if( semop(gnSemID2, &mysem_open, 1) == -1 )
   {
           perror("semop");
           exit(1);
   }

   if(pstShm2->check)
   {
     /* first cardset recv */
     strncpy(buffer, pstShm2->data, BUFFER_SIZE);

     printf("received cardset: %s\n", buffer);

     int my_sum;
     int dealer_sum;

     my_hand_values[0] = get_value_id(buffer[0]);
     my_hand_suits[0] = get_suit_id(buffer[1]);
     my_hand_values[1] = get_value_id(buffer[2]);
     my_hand_suits[1] = get_suit_id(buffer[3]);
     dealer_hand_values[0] = get_value_id(buffer[4]);
     dealer_hand_suits[0] = get_suit_id(buffer[5]);
     nmy = 2;
     ndealer = 1;

     pstShm2->check = 0;
     semop(gnSemID2, &mysem_close, 1);
     /******* 임계영역 *******/

      while(1){
        /******* 임계영역 *******/
        if( semop(gnSemID2, &mysem_open, 1) == -1 )
        {
                perror("semop");
                exit(1);
        }
    	  int choice;
    	  my_sum = calc_sum(my_hand_values, nmy);

    	  printf("\n");
    	  printf("My Hand: ");
        display_state(my_hand_values, my_hand_suits, nmy);
        printf("Dealer Hand: ");
        display_state(dealer_hand_values, dealer_hand_suits, ndealer);

        /* 내 덱에 합계가 21이 넘어갈 경우 lose */
        if (my_sum > 21)
        {
          printf("\nI'm busted! I lose!\n");
          /*shared memory detached*/
          if(shmdt(pstShm2) == -1) {
             perror("shmdt failed");
             exit(1);
          }
          return;
        }

        printf("\n");
        printf("1. Hit\n");
        printf("2. Stand\n");
        printf("Please choose 1 or 2: ");
        fflush(stdout);

        scanf("%d", &choice);
        if (choice == 1)
        {
          strcpy(buffer, HIT);
          printf("Sending: %s\n", buffer);
          /* HIT send */
          strncpy(pstShm2->data, buffer, BUFFER_SIZE);
          pstShm2->check = 1;
          pstShm2->check2 = 1;
          semop(gnSemID2, &mysem_close, 1);
          /******* 임계영역 *******/

          /*
          while(pstShm2->check){
            sleep(1);
            printf("wait for recive\n");
          }
          */

          /******* 임계영역 *******/
          if( semop(gnSemID2, &mysem_open, 1) == -1 )
          {
                  perror("semop");
                  exit(1);
          }
          /* recv new card */
          strncpy(buffer, pstShm2->data, BUFFER_SIZE);
          pstShm2->check = 0;
          printf("I received: %s\n", buffer);
          my_hand_values[nmy] = get_value_id(buffer[0]);
          my_hand_suits[nmy] = get_suit_id(buffer[1]);
          ++nmy;

          semop(gnSemID2, &mysem_close, 1);
          /******* 임계영역 *******/
        } // 임계영역이 끝났을 시에 server에서

        else if (choice == 2)
    		{
    			strcpy(buffer, STAND);
          printf("Sending: %s\n", buffer);
          strncpy(pstShm2->data, buffer, BUFFER_SIZE);
          pstShm2->check = 1;
          pstShm2->check2 = 1;
          semop(gnSemID2, &mysem_close, 1);
    			break;
    		}
        else
          printf("Unrecognized choice. Choose again.\n");
      }
      /*반복문 바깥*/

      while(!pstShm2->finalcheck){
        sleep(1);
        printf("calculating...\n");
      }

      /******* 임계영역 *******/
      if( semop(gnSemID2, &mysem_open, 1) == -1 )
      {
              perror("semop");
              exit(1);
      }
      unsigned i;
      /* dealer card recv*/
      strncpy(buffer, pstShm2->data, BUFFER_SIZE);
      pstShm2->check = 0;
      printf("I received: %s\n", buffer);
      for (i = 0; i < strlen(buffer); i += 2)
  		{
  			dealer_hand_values[ndealer] = get_value_id(buffer[i]);
  			dealer_hand_suits[ndealer] = get_suit_id(buffer[i + 1]);
  			++ndealer;
  		}
      semop(gnSemID2, &mysem_close, 1);
      /******* 임계영역 *******/

      printf("\n");
  		printf("My Hand: ");
  		display_state(my_hand_values, my_hand_suits, nmy);
  		printf("Dealer Hand: ");
  		display_state(dealer_hand_values, dealer_hand_suits, ndealer);

    	my_sum = calc_sum(my_hand_values, nmy);
    	dealer_sum = calc_sum(dealer_hand_values, ndealer);

    	if (dealer_sum > 21)
    		printf("\nDealer busted! I win!\n");
    	else if (my_sum == dealer_sum)
    		printf("\nMe and the dealer have the same score. It's a push!\n");
    	else if (my_sum < dealer_sum)
    		printf("\nDealer has a higher score. I lose!\n");
    	else
    		printf("\nI have a higher score. I win!\n");

  }

  if(shmdt(pstShm2) == -1) {
     perror("shmdt failed");
     exit(1);
  }

}









int main()
{
  /* Shared Memory */
  key_t        keyShm;       /* Shared Memory Key */
  key_t        keySem;       /* Semapore Key */
  _ST_SHM        *pstShm1;      /* 공용 메모리 구조체 */
  char buffer[BUFFER_SIZE];
  /* Shared Memory의 키값을 생성한다. */
  keyShm = (key_t)60100;
  keySem = (key_t)60101;
  int id;
  struct sembuf mysem_open  = {0, -1, SEM_UNDO}; // 세마포어 얻기
  struct sembuf mysem_close = {0, 1, SEM_UNDO};  // 세마포어 돌려주기

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

  //semctl( gnSemID2, 0, SETVAL, sem_union );

  /* 반복문일 필요 없을 거같음 */
  while(1) {

    printf("input \"start\" ==> ");
    fgets(buffer, BUFFER_SIZE, stdin); //start

    /* 공유메모리에 데이터 쓰기 */
    strncpy(pstShm1->data, buffer, BUFFER_SIZE);
    pstShm1->check = 1;
    /* server가 먼저 실행 */
    while(pstShm1->check);

    /******* 임계영역 *******/
    if( semop(gnSemID1, &mysem_open, 1) == -1 )
    {
           perror("semop");
           exit(1);
    }
    /* 쓴 데이터가 ‘quit’이면 while 문 벗어남 */
    if(!strncmp(pstShm1->data, "quit", 4)) {
       break;
    }


    strcpy(buffer, pstShm1->data);
    printf("recv: %s\n", buffer);
    semop(gnSemID1, &mysem_close, 1);
    /******* 임계영역 *******/
    id=buffer[0]-48;
    play_game(id);
    printf("game end!\n");

    /* shared memory & semapore del */
    printf("[SIGNAL] : Shared meory segment marked for deletion\n");
    shmctl( gnShmID2, IPC_RMID, 0 );
    printf("[SIGNAL] : Semapore segment marked for deletion\n");
    semctl( gnSemID2, IPC_RMID, 0 );
    break;
  }
  if(shmdt(pstShm1) == -1) {
     perror("shmdt failed");
     exit(1);
  }
}
