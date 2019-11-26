
#ifndef COMMON_H
#define	COMMON_H


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRUE 1
#define FALSE 0
#define BUFFER_SIZE 32
#define LETS_PLAY "0xdeadbeef"
#define HIT "HIT"
#define STAND "STAND"
#define MAX_WAIT_RCV 5
#define MAX_WAIT_GAME 30
#define MAX_PLAYERS 4
#define SEGSIZE  sizeof(_ST_SHM)

const struct timeval tv_wait_inf = {.tv_sec = 0, .tv_usec = 0};
const struct timeval tv_wait_rcv = {.tv_sec = MAX_WAIT_RCV, .tv_usec = 0};
const struct timeval tv_wait_game = {.tv_sec = MAX_WAIT_GAME, .tv_usec = 0};
const struct timeval tv_wait_game3 = {.tv_sec = MAX_WAIT_GAME + 3, .tv_usec = 0};

const char* suits[] = {"spades", "hearts", "diamonds", "clubs"};
const char* values[] = {"dummy", "Ace", "2", "3", "4", "5", "6", "7", "8", "9", "10", "Jack", "Queen", "King"};
const char suit_codes[] = {'S', 'H', 'D', 'C'};
const char value_codes[] = {'0', 'A', '2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J', 'Q', 'K'};
typedef struct {
	int check;
	int check2;
	int finalcheck;
	char data[BUFFER_SIZE];
} _ST_SHM;

void error(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(1);
}

int get_suit_id(char suit)
{
	unsigned i;
	for (i = 0; i < sizeof (suit_codes); ++i)
		if (suit == suit_codes[i])
			return i;
	return -1;
}

int get_value_id(char value)
{
	unsigned i;
	for (i = 0; i < sizeof (value_codes); ++i)
		if (value == value_codes[i])
			return i;
	return -1;
}

int calc_sum(const int hand_values[], int ncards)
{
	int i;
	int sum = 0;

	for (i = 0; i < ncards; ++i)
		if (hand_values[i] < 10)
			sum += hand_values[i];
		else
			sum += 10;

	for (i = 0; i < ncards; ++i)
		if (hand_values[i] == 1)
			if (sum + 10 <= 21)
				sum += 10;

	return sum;
}

void display_state(int hand_values[], int hand_suits[], int ncards)
{
	int i;
	int sum = calc_sum(hand_values, ncards);
	for (i = 0; i < ncards; ++i)
	{
		if (i > 0)
			printf(", ");
		printf("%s-%s", values[hand_values[i]], suits[hand_suits[i]]);
	}
	printf(" Sum: %d", sum);
	if (sum > 21)
		printf("; BUSTED");
	printf("\n");
}

struct timeval difftimeval(const struct timeval tv1, const struct timeval tv2)
{
	long utotal = 1000 * 1000 * (tv1.tv_sec - tv2.tv_sec) + tv1.tv_usec - tv2.tv_usec;
	struct timeval tv = {.tv_sec = utotal / (1000 * 1000), .tv_usec = utotal % (1000 * 1000)};
	return tv;
}

struct timeval elapsed_since(const struct timeval tv1)
{
	struct timeval tv2;
	gettimeofday(&tv2, NULL);
	return difftimeval(tv2, tv1);
}

int read_with_timeout(int clientSockFd, char buffer[], const struct timeval tv_maxtime)
{
	int nread = 0;
	struct timeval tv_start;

	gettimeofday(&tv_start, NULL);

	while (nread < BUFFER_SIZE)
	{
		int n;
		struct timeval tv_left = {.tv_sec = 0, .tv_usec = 0};
		if (tv_maxtime.tv_sec > 0)
			tv_left = difftimeval(tv_maxtime, elapsed_since(tv_start));

		if (0 > (setsockopt(clientSockFd, SOL_SOCKET, SO_RCVTIMEO, &tv_left, sizeof (tv_left))))
		{
			printf("Error setting clientSockFd %d with setsockopt\n", clientSockFd);
			return FALSE;
		}

		if (0 > (n = read(clientSockFd, buffer + nread, BUFFER_SIZE - nread)))
		{
			/* error("Error reading from client"); */
			printf("Response from socket %d timed out\n", clientSockFd);
			return FALSE;
		}

		nread += n;
	}
	return TRUE;
}


#endif	/* COMMON_H */
