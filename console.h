#include "types.h"
#include "spinlock.h"
 struct consLock{
  struct spinlock lock;
  int locking;
} ;
#define INPUT_BUF 128
struct inputStruct{
  struct spinlock lock;
  char buf[INPUT_BUF];
  uint r;  // Read index
  uint w;  // Write index
  uint e;  // Edit index
} ;

#define CRTPORT 0x3d4
