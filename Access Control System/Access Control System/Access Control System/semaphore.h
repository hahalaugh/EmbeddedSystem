#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#define DOOR_CLOSED 0
#define DOOR_OPENED 1

#define LOCK_LOCKED 0
#define LOCK_RELEASED 1


int outerLock = LOCK_LOCKED;
int outerDoor = DOOR_CLOSED;

int innerLock = LOCK_LOCKED;
int innerDoor = DOOR_CLOSED;


void lock(int* lock);
void release(int* lock);
void open(int* door);
void close(int* door);
#endif
