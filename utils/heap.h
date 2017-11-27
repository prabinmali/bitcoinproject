#ifndef heap_h
#define heap_h


typedef struct heap {
  long size;
  long index;
  void** data;
} Heap;


Heap* heapInitialize(long size);

void heapInsert(Heap *h, void* data, int(*compare)());

void* heapPop(Heap* h, int(*compare)());

long heapLen(Heap*h);

#endif
