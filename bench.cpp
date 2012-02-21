#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <tr1/functional>
#include <time.h>
#include "IQueue.h"
#include "SimpleQueue.h"
#include "LockingQueue.h"
#include "LocklessQueue.h"

//  Compile with :
// g++ IQueue.h SimpleQueue.h LockingQueue.h LocklessQueue.h bench.cpp -Wall -lrt -lpthread -o bench

// Used at the end of each to test to print results
#define RESULT(s) printf("%s\t%s\t%ld\n", s, sum == 0 ? "PASS" : "FAIL", end-begin); 

using ConcurrentQueues::IQueue;
using ConcurrentQueues::SimpleQueue;
using ConcurrentQueues::LockingQueue;
using ConcurrentQueues::LocklessQueue;

// Thread Creation Functions, from MCP Lab Code
typedef std::tr1::function<void()> ThreadBody;
static void* threadFunction(void* arg) {
  ThreadBody* c = reinterpret_cast<ThreadBody*>(arg);
  (*c)();
  delete c;
  return 0;
}

pthread_t makeThread(ThreadBody body) {
  ThreadBody* copy = new ThreadBody(body);

  void* arg = reinterpret_cast<void*>(copy);
  pthread_t thread;
  if (pthread_create(&thread, NULL, threadFunction, arg) != 0) {
    perror("Can't create thread");
    delete copy;
    exit(1);
  }
  return thread;
}

// Timer Function
typedef uint64_t Ticks;
Ticks ClockGetTime(){
  timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (uint64_t)ts.tv_sec * 1000000LL + (uint64_t)ts.tv_nsec / 1000LL;
}

// Sieve is used to generate some workload between queue operations
// source : http://www.algolist.net/Algorithms/Number_theoretic/Sieve_of_Eratosthenes
int sieve(int upperBound) {
  int count = 0;
  int upperBoundSquareRoot = (int)sqrt((double)upperBound);
  bool *isComposite = new bool[upperBound + 1];
  memset(isComposite, 0, sizeof(bool) * (upperBound + 1));
  for (int m = 2; m <= upperBoundSquareRoot; m++)
        if (!isComposite[m])
              for (int k = m * m; k <= upperBound; k += m)
                    isComposite[k] = true;
  for (int m = upperBoundSquareRoot; m <= upperBound; m++)
        if (!isComposite[m])
              count++;
  delete [] isComposite;
  return count;
}

// Enqueue some data into a queue
// Used to start a new queue with some initial data to
// avoid having many dequeues run hit an empty queue.
// Return the sum of the values added.
int seed_queue(IQueue<int>* q, int seed){
  long sum = 0;
  int x;
  for(int i=0;i<seed;i++){
    x = i % 37;
    q->Enqueue(x);
    sum += x;
  }
  return sum;
}

// Empty everything out of the queue.
// Return the sum of the values removed.
int empty_queue(IQueue<int>* q){
  long sum = 0;
  int x;
  while(q->Dequeue(&x))
    sum += x;
  return sum;
}

// Performs an Enqueue of Dequeue based on a 
// set of random numbers.  Will perform a sieve
// up to sieveBound between each operation.
// Fills in sum with the sum of the values enqueued
// minus the sum of the values dequeued.
void random_worker(IQueue<int>* q, int iterations, int sieveBound, int* randoms, int offset, long* sum){
  int x = 0;
  long localSum = 0;
  for(int i=0;i<iterations;i++){
    int r = randoms[offset+i];
    if(sieveBound > 0)
      sieve(sieveBound);
    if(r % 2 == 0){
      x = r % 37;
      q->Enqueue(x);
      localSum += x;
    }else{
      if(q->Dequeue(&x))
        localSum -= x;
    }
  }
  *sum += localSum;
}

// Performs a series of Enqueues then Dequeues.
// Similar to random_worker in all other regards.
void series_worker(IQueue<int>* q, int iterations, int sieveBound, int enqueueCount, int dequeueCount, long* sum){
  int x = 0;
  long localSum = 0;
  for(int i=0;i<iterations;i++){
    for(int j=0;j<enqueueCount;j++){
      if(sieveBound > 0)
        sieve(sieveBound);  
      x = i % 37;
      q->Enqueue(x);
      localSum += x;
    }
    for(int j=0;j<dequeueCount;j++){
      if(sieveBound > 0)
        sieve(sieveBound);      
      if(q->Dequeue(&x))
        localSum -= x;      
    }
  }
  *sum += localSum;
}

void series_sequential_simple(int iterations, int sieveBound, int enqueueCount, int dequeueCount){
  long sum = 0;
  SimpleQueue<int> simple;
  if(dequeueCount > enqueueCount)
    sum += seed_queue(&simple, iterations / (dequeueCount - enqueueCount)); 
  Ticks begin = ClockGetTime();
  series_worker(&simple, iterations, sieveBound, enqueueCount, dequeueCount, &sum);
  Ticks end = ClockGetTime();    
  sum -= empty_queue(&simple);  
  RESULT("Sequential Simple  ");
}

void series_sequential_locking(int iterations, int sieveBound, int enqueueCount, int dequeueCount){
  long sum = 0;
  LockingQueue<int> locking;
  if(dequeueCount > enqueueCount)
    sum += seed_queue(&locking, iterations / (dequeueCount - enqueueCount));  
  Ticks begin = ClockGetTime();
  series_worker(&locking, iterations, sieveBound, enqueueCount, dequeueCount, &sum);
  Ticks end = ClockGetTime();  
  sum -= empty_queue(&locking);
  RESULT("Sequential Locking ");
}

void series_sequential_lockless(int iterations, int sieveBound, int enqueueCount, int dequeueCount){
  long sum = 0;
  LocklessQueue<int> lockless;
  IQueue<int>* a = lockless.CreateAccessor();  
  if(dequeueCount > enqueueCount)
    sum += seed_queue(a, iterations / (dequeueCount - enqueueCount));  
  Ticks begin = ClockGetTime();
  series_worker(a, iterations, sieveBound, enqueueCount, dequeueCount, &sum);
  Ticks end = ClockGetTime();    
  sum -= empty_queue(a);
  delete a;  
  RESULT("Sequential Lockless");
}

void series_concurrent_locking(int iterations, int sieveBound, int enqueueCount, int dequeueCount, int num_threads){
  long sum = 0;
  LockingQueue<int> locking;
  if(dequeueCount > enqueueCount)
    sum += seed_queue(&locking, iterations / (dequeueCount - enqueueCount));   
  long sums[num_threads];
  pthread_t threads[num_threads];
  int n = iterations / num_threads;
  Ticks begin = ClockGetTime();  
  for(int i=0;i<num_threads;i++){
    sums[i] = 0;
    threads[i] = makeThread(std::tr1::bind(&series_worker, &locking, n, sieveBound, enqueueCount, dequeueCount, &sums[i]));
  }
	for(int i=0;i<num_threads;i++)
		pthread_join(threads[i],NULL);    
  Ticks end = ClockGetTime();
 	for(int i=0;i<num_threads;i++)
    sum += sums[i];
  sum -= empty_queue(&locking);
  RESULT("Concurrent Locking ");
}

void series_concurrent_lockless(int iterations, int sieveBound, int enqueueCount, int dequeueCount, int num_threads){
  long sum = 0;  
  LocklessQueue<int> lockless;
  IQueue<int>* a = lockless.CreateAccessor();  
  if(dequeueCount > enqueueCount)
    sum += seed_queue(a, iterations / (dequeueCount - enqueueCount));   
  long sums[num_threads];
  IQueue<int>* queues[num_threads];
  pthread_t threads[num_threads];
  int n = iterations / num_threads;
  Ticks begin = ClockGetTime();  
  for(int i=0;i<num_threads;i++){
    sums[i] = 0;  
    queues[i] = lockless.CreateAccessor();
    threads[i] = makeThread(std::tr1::bind(&series_worker, queues[i], n, sieveBound, enqueueCount, dequeueCount, &sums[i]));
  }  
	for(int i=0;i<num_threads;i++){
		pthread_join(threads[i],NULL);    
    delete queues[i];
  }
  Ticks end = ClockGetTime();
 	for(int i=0;i<num_threads;i++)
    sum += sums[i];
  sum -= empty_queue(a);
  delete a;
  RESULT("Concurrent Lockless");  
}

void random_sequential_simple(int iterations, int sieveBound, int* randoms){
  SimpleQueue<int> simple;
  long sum = 0;
  Ticks begin = ClockGetTime();
  random_worker(&simple, iterations, sieveBound, randoms, 0, &sum);
  Ticks end = ClockGetTime();
  sum -= empty_queue(&simple);
  RESULT("Sequential Simple  ");
}

void random_sequential_locking(int iterations, int sieveBound, int* randoms){
  LockingQueue<int> locking; 
  long sum = 0;
  Ticks begin = ClockGetTime();
  random_worker(&locking, iterations, sieveBound, randoms, 0, &sum);
  Ticks end = ClockGetTime();  
  sum -= empty_queue(&locking);
  RESULT("Sequential Locking ");
}

void random_sequential_lockless(int iterations, int sieveBound, int* randoms){
  LocklessQueue<int> lockless;
  long sum = 0;
  IQueue<int>* a = lockless.CreateAccessor();
  Ticks begin = ClockGetTime();
  random_worker(a, iterations, sieveBound, randoms, 0, &sum);
  Ticks end = ClockGetTime();  
  sum -= empty_queue(a);
  delete a;
  RESULT("Sequential Lockless");
}

void random_concurrent_locking(int iterations, int sieveBound, int* randoms, int num_threads){
  LockingQueue<int> locking;
  pthread_t threads[num_threads];
  long sums[num_threads];
  int n = iterations / num_threads;
  Ticks begin = ClockGetTime();  
  for(int i=0;i<num_threads;i++){
    sums[i] = 0;  
    threads[i] = makeThread(std::tr1::bind(&random_worker, &locking, n, sieveBound, randoms, n*i, &sums[i]));
  }
	for(int i=0;i<num_threads;i++)
		pthread_join(threads[i],NULL);    
  Ticks end = ClockGetTime();  
  long sum = 0;
	for(int i=0;i<num_threads;i++)
    sum += sums[i];
  sum -= empty_queue(&locking);
  RESULT("Concurrent Locking ");    
}

void random_concurrent_lockless(int iterations, int sieveBound, int* randoms, int num_threads){
  LocklessQueue<int> lockless;
  IQueue<int>* queues[num_threads];
  pthread_t threads[num_threads];
  long sums[num_threads];
  int n = iterations / num_threads;
  Ticks begin = ClockGetTime();  
  for(int i=0;i<num_threads;i++){
    sums[i] = 0;  
    queues[i] = lockless.CreateAccessor();
    threads[i] = makeThread(std::tr1::bind(&random_worker, queues[i], n, sieveBound, randoms, n*i, &sums[i]));
  }  
	for(int i=0;i<num_threads;i++){
		pthread_join(threads[i],NULL);    
    delete queues[i];
  }
  Ticks end = ClockGetTime();  
  long sum = 0;
	for(int i=0;i<num_threads;i++)
    sum += sums[i];  
  IQueue<int>* a = lockless.CreateAccessor();
  sum -= empty_queue(a);
  delete a;    
  RESULT("Concurrent Lockless");
}

void random_tests(int iterations, int sieveBound, int threads){ 
  printf("\nRandom Tests\n");
  
  // Generate randoms to determine queue ops
  int* randoms = new int[iterations];
  srand(time(0));
  for(int i=0;i<iterations;i++)
    randoms[i] = rand();
  
  random_sequential_simple(iterations, sieveBound, randoms);
  random_sequential_locking(iterations, sieveBound, randoms);
  random_sequential_lockless(iterations, sieveBound, randoms);
  random_concurrent_locking(iterations, sieveBound, randoms, threads);
  random_concurrent_lockless(iterations, sieveBound, randoms, threads);
  
  delete[] randoms;
}


void series_tests(int iterations, int sieveBound, int threads, int bias){
  // Adjust iterations for the bias as to not carry out too many operations.
  iterations /= bias+1;
  printf("\nEnqueue Bias Series Tests\n");
  series_sequential_simple(iterations, sieveBound, bias, 1);
  series_sequential_locking(iterations, sieveBound, bias, 1);
  series_sequential_lockless(iterations, sieveBound, bias, 1);    
  series_concurrent_locking(iterations, sieveBound, bias, 1, threads);
  series_concurrent_lockless(iterations, sieveBound, bias, 1, threads);
  printf("\nDequeue Bias Series Tests\n");  
  series_sequential_simple(iterations, sieveBound, 1, bias);  
  series_sequential_locking(iterations, sieveBound, 1, bias);
  series_sequential_lockless(iterations, sieveBound, 1, bias);  
  series_concurrent_locking(iterations, sieveBound, 1, bias, threads);
  series_concurrent_lockless(iterations, sieveBound, 1, bias, threads);  
}

int main( int argc, const char* argv[] )
{
  int iterations = -1;
  int sieveBound = -1;  
  int threads = -1;
  int bias = -1;

	if (argc == 5){
    iterations = atoi(argv[1]);
    sieveBound = atoi(argv[2]);    
    threads = atoi(argv[3]);
    bias = atoi(argv[4]);
  }
  
  if(iterations <= 0) iterations = 1000000;
  if(threads <= 0) threads = 8;
  if(sieveBound < 0) sieveBound = 100;
  if(bias <= 0) bias = 2;
 
  printf("\nIterations %d\n", iterations);
  printf("Sieve Bound %d\n", sieveBound);  
  printf("Threads %d\n", threads);
  printf("Series Bias %d\n", bias);

  random_tests(iterations, sieveBound, threads);
  series_tests(iterations, sieveBound, threads, bias);
   
  printf("\n");
  return 0;
}
