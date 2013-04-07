#ifndef CONTROL_H
#define CONTROL_H

#include "macro.h"
static int outerLock = LOCK_LOCKED;
static int outerDoor = DOOR_CLOSED;

static int innerLock = LOCK_LOCKED;
static int innerDoor = DOOR_CLOSED;

void lock(int* lock);
void release(int* lock);
void open(int* door);
void close(int* door);

#endif
