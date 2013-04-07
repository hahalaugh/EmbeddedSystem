#include "semaphore.h"
void lock(int* lock)
{
	*lock = LOCK_LOCKED;
}

void release(int* lock)
{
	*lock = LOCK_RELEASED;
}

void open(int* door)
{
	*door = DOOR_OPENED;
}

void close(int* door)
{
	*door = DOOR_CLOSED;
}

