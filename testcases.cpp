#include <stdio.h>
#include <stdlib.h>
#include <tr1/functional>

#include <sys/time.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

#include "IQueue.h"
#include "LockingQueue.h"
#include "LocklessQueue.h"

using ConcurrentQueues::IQueue;
using ConcurrentQueues::LockingQueue;
using ConcurrentQueues::LocklessQueue;
using namespace std;

int numThreads = 10;  //number of threads to use during concurrent tests

typedef std::tr1::function<void()> ThreadBody;

/************** Timing Related Functions ***********/

//Subtract the 'struct timeval' values X and Y, storing the result in RESULT.
//X should be the newer time, Y the older time.  
void timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
       int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
       y->tv_usec -= 1000000 * nsec;
       y->tv_sec += nsec;
    }
	
    if (x->tv_usec - y->tv_usec > 1000000) {
       int nsec = (x->tv_usec - y->tv_usec) / 1000000;
       y->tv_usec += 1000000 * nsec;
       y->tv_sec -= nsec;
    }
     
    /* Compute the time remaining to wait.
       tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;
   
}

//given newer time x and older time y, print out the elapsed time
void printElapsed (struct timeval *x, struct timeval *y) {
  struct timeval myresult;
  timeval_subtract(&myresult, x, y);
  std::cout << myresult.tv_sec << " secs " << myresult.tv_usec << " microsecs"<< std::endl;
}

/************Thread Related Stuff**************/

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

/**************** Test Cases ****************/

void Case1(IQueue<int>* q) { //basic sequential correctness check (this one is not timed). param: fresh queue
  int i = 1;
  while (true) {  //enqueues 1,3,5,7,9,77
    q->Enqueue(i);
	usleep(3000);
	i = i + 2;
	if (i == 11) {
	  i = 77;
	}
	if (i == 79) {
	  break;
	}
  }
  
  int dq1;
  int dq2;
  q->Dequeue(&dq1);
  q->Dequeue(&dq2);
  bool allcorrect = true;
  if (dq1 == 1 && dq2 == 3) {
    cout << "Correct Dequeued Values." << endl;
  } else {
    cout << "Incorrect Dequeued Values." << endl;
	allcorrect = false;
  }
  q->Enqueue(8); //now 5,7,9,77,8
  
  int arr[6] = {5,7,9,77,8};
  
  for (int i = 0; i < 5; i++) {
    int dqcurr;
	q->Dequeue(&dqcurr);
	if (dqcurr != arr[i]) {
	  cout << "Incorrect Dequeued Value" << endl;
	  allcorrect = false;
	}
  }
  
  if (allcorrect) {
    cout << "All values dequeued were correct as expected." << endl;
  }
  
  delete q;  
}

IQueue<int>* Case2(IQueue<int>* q) {  //all enqueues. param: starts with fresh queue
  int i = 0;
  while (true) {
    q->Enqueue(i);
	//usleep(200);
	i++;
	if (i == 500) {
		break;
	}
  }
  
  return q; //needed for Case 3
   
}

void Case3(IQueue<int>* q) { //all dequeues.  parameter comes from Case2
  for (int i = 0; i < 500; i++) {
    int k;
	//usleep(200);
	q->Dequeue(&k);
  }
}

void Case4(IQueue<int>* qq) { //group of adds followed by group of dequeues. param: starts with fresh queue
  IQueue<int>* q = Case2(qq);
  Case3(q);
}

void Case5(IQueue<int>* q) { //mayhem (random add and deletes interlaced). param: starts with fresh queue
  int kkk;
  for (int i = 0; i < 9000; i+=2) {
	q->Enqueue(i);
	if (i >= 5000 || i <= 6000) {
	  q->Dequeue(&kkk);
	}	
  }
  
  for (int j = 0; j < 99900; j++) {
    //usleep(200);
	q->Enqueue(j);
  }
  
  for (int k = 0; k < 35000; k++) {
    q->Enqueue(k);
	q->Enqueue(k*2);
	//usleep(3000);
	q->Enqueue(k*3);
	q->Dequeue(&kkk);
	q->Enqueue(k*5);
	q->Dequeue(&kkk);
  }
  
  for (int c = 0; c < 4800; c++) {
    q->Dequeue(&kkk);
  }
}

/*************** Sequential Test Cases ***************/

/*****Locking Queues********/
void STest1() {
  Case1(new LockingQueue<int>);
}

IQueue<int>* STest2() {
  return Case2(new LockingQueue<int>);
}

void STest3(IQueue<int>* q) {
  Case3(q);
  delete q;
}

void STest4() {
  LockingQueue<int> *q = new LockingQueue<int>();
  Case4(q);
  delete q;
}

void STest5() {
  LockingQueue<int> *q = new LockingQueue<int>();
  Case5(q);
  delete q;
}

/******Lockless Queues**********/
void STest6() {
  LocklessQueue<int>* q = new LocklessQueue<int>();
  Case1(q->CreateAccessor());
  
}
IQueue<int>* STest7() {
  LocklessQueue<int>* q = new LocklessQueue<int>();
  return Case2(q->CreateAccessor());
}

void STest8(IQueue<int>* q) {
  Case3(q);
  delete q;
}

void STest9() {
  LocklessQueue<int>* q = new LocklessQueue<int>();
  Case4(q->CreateAccessor());
  delete q;
}

void STest10() {
  LocklessQueue<int>* q = new LocklessQueue<int>();
  Case5(q->CreateAccessor());
  delete q;
}

/*************** Concurrent Test Cases **************/
/****** Locking Queues ******/
IQueue<int>* CTest1() {
  pthread_t allthreads[numThreads];
  LockingQueue<int> *q = new LockingQueue<int>();
  for (int i = 0; i < numThreads; i++) {
    allthreads[i] = makeThread(std::tr1::bind(&Case2, q));
  }
  
  for (int i = 0; i < numThreads; i++) {
    pthread_join(allthreads[i], NULL);
  }
  
  return q;
}

void CTest2(IQueue<int>* q) {
  pthread_t allthreads[numThreads];
  for (int i = 0; i < numThreads; i++) {
    allthreads[i] = makeThread(std::tr1::bind(&Case3, q));
  }
  
  for (int i = 0; i < numThreads; i++) {
    pthread_join(allthreads[i], NULL);
  }
  
  delete q;
}

void CTest3() {
  pthread_t allthreads[numThreads];
  LockingQueue<int> *q = new LockingQueue<int>();
  for (int i = 0; i < numThreads; i++) {
    allthreads[i] = makeThread(std::tr1::bind(&Case4, q));
  }
  
  for (int i = 0; i < numThreads; i++) {
    pthread_join(allthreads[i], NULL);
  }
  
  delete q;
}

void CTest4() {
  pthread_t allthreads[numThreads];
  LockingQueue<int> *q = new LockingQueue<int>();
  for (int i = 0; i < numThreads; i++) {
    allthreads[i] = makeThread(std::tr1::bind(&Case5, q));
  }
  
  for (int i = 0; i < numThreads; i++) {
    pthread_join(allthreads[i], NULL);
  }
  
  delete q;
}
/****** Lockless Queues *******/
LocklessQueue<int>* CTest5() {
  pthread_t allthreads[numThreads];
  LocklessQueue<int> *q = new LocklessQueue<int>();
  for (int i = 0; i < numThreads; i++) {
    allthreads[i] = makeThread(std::tr1::bind(&Case2, q->CreateAccessor()));
  }
  
  for (int i = 0; i < numThreads; i++) {
    pthread_join(allthreads[i], NULL);
  }
  
  return q;
}

void CTest6(LocklessQueue<int>* q) {
  pthread_t allthreads[numThreads];
  for (int i = 0; i < numThreads; i++) {
    allthreads[i] = makeThread(std::tr1::bind(&Case3, q->CreateAccessor()));
  }
  
  for (int i = 0; i < numThreads; i++) {
    pthread_join(allthreads[i], NULL);
  }
  
  delete q;
}

void CTest7() {
  pthread_t allthreads[numThreads];
  LocklessQueue<int> *q = new LocklessQueue<int>();
  for (int i = 0; i < numThreads; i++) {
    allthreads[i] = makeThread(std::tr1::bind(&Case4, q->CreateAccessor()));
  }
  
  for (int i = 0; i < numThreads; i++) {
    pthread_join(allthreads[i], NULL);
  }
  
  delete q;
}

void CTest8() {
  pthread_t allthreads[numThreads];
  LocklessQueue<int> *q = new LocklessQueue<int>();
  for (int i = 0; i < numThreads; i++) {
    allthreads[i] = makeThread(std::tr1::bind(&Case5, q->CreateAccessor()));
  }
  
  for (int i = 0; i < numThreads; i++) {
    pthread_join(allthreads[i], NULL);
  }
  
  delete q;
}



/*************** Main Program Starts Here *************/
//Runs every test, records times, then prints them all out at the very end.
//One optional parameter: number of threads to use during concurrent tests.
//Please use a reasonable amount (using way too many will run out of memory)
//If parameter is not specified, the program defaults to 10 threads.
int main( int argc, const char* argv[] ) {
    //check existence of optional argument
	if (argc == 2) {
	  int nthreads = atoi(argv[1]);
	  if (nthreads > 0) {
	    numThreads = nthreads;
	  }	  
	}

	struct timeval begin; //begin timestamps for each individual test
	struct timeval end; //end timestamps for each individual test
	struct timeval myresult; //stores the time difference for each test

    cout << "Sequential Tests:" << endl;
	
	cout << "\nSeq Test 1: Locking Queue, basic correctness check" << endl;
	STest1();  //basic correctness check does not get timed
	
	cout << "\nSeq Test 2: Locking Queue, all enqueues" << endl;
	gettimeofday(&begin, NULL);
	IQueue<int>* q1 = STest2();
	gettimeofday(&end, NULL);
	printElapsed(&end, &begin);
	
	cout << "\nSeq Test 3: Locking Queue, all dequeues" << endl;
	gettimeofday(&begin, NULL);
	STest3(q1);
	gettimeofday(&end, NULL);
	printElapsed(&end, &begin);
	
	cout << "\nSeq Test 4: Locking Queue, group of adds followed by group of dequeues" << endl;
	gettimeofday(&begin, NULL);
	STest4();
	gettimeofday(&end, NULL);
	printElapsed(&end, &begin);
	
	cout << "\nSeq Test 5: Locking Queue, mayhem" << endl;
	gettimeofday(&begin, NULL);
	STest5();
	gettimeofday(&end, NULL); 
	printElapsed(&end, &begin);
	
	cout << "\nSeq Test 6: LockLESS Queue, basic correctness check" << endl;
	STest6();
	
	cout << "\nSeq Test 7: LockLESS Queue, all enqueues" << endl;
	gettimeofday(&begin, NULL);
	IQueue<int>* lq1 = STest7();
	gettimeofday(&end, NULL);
	printElapsed(&end, &begin);
	
	cout << "\nSeq Test 8: LockLESS Queue, all dequeues" << endl;
	gettimeofday(&begin, NULL);
	STest8(lq1);
	gettimeofday(&end, NULL);
	printElapsed(&end, &begin);
	
	cout << "\nSeq Test 9: LockLESS Queue, group of adds followed by group of dequeues" << endl;
	gettimeofday(&begin, NULL);
	STest9();
	gettimeofday(&end, NULL);
	printElapsed(&end, &begin);
	
	cout << "\nSeq Test 10: LockLESS Queue, mayhem" << endl;
	gettimeofday(&begin, NULL);
	STest10();
	gettimeofday(&end, NULL); 
	printElapsed(&end, &begin);
	
	cout << "\nConcurrent Tests:" << endl;
	cout << "\nConc Test 1: Locking Queue, all enqueues" << endl;
	gettimeofday(&begin, NULL);
	IQueue<int>* q2 = CTest1();
	gettimeofday(&end, NULL); 
	printElapsed(&end, &begin);
	
	cout << "\nConc Test 2: Locking Queue, all dequeues" << endl;
	gettimeofday(&begin, NULL);
	CTest2(q2);
	gettimeofday(&end, NULL); 
	printElapsed(&end, &begin);
	
	cout << "\nConc Test 3: Locking Queue, group of adds followed by group of dequeues" << endl;
	gettimeofday(&begin, NULL);
	CTest3();
	gettimeofday(&end, NULL); 
	printElapsed(&end, &begin);
	
	cout << "\nConc Test 4: Locking Queue, mayhem" << endl;
	gettimeofday(&begin, NULL);
	CTest4();
	gettimeofday(&end, NULL); 
	printElapsed(&end, &begin);
	
	cout << "\nConc Test 5: LockLESS Queue, all enqueues" << endl;
	gettimeofday(&begin, NULL);
	LocklessQueue<int>* lq2 = CTest5();
	gettimeofday(&end, NULL); 
	printElapsed(&end, &begin);
	
	cout << "\nConc Test 6: LockLESS Queue, all dequeues" << endl;
	gettimeofday(&begin, NULL);
	CTest6(lq2);
	gettimeofday(&end, NULL); 
	printElapsed(&end, &begin);
	
	cout << "\nConc Test 7: LockLESS Queue, group of adds followed by group of dequeues" << endl;
	gettimeofday(&begin, NULL);
	CTest7();
	gettimeofday(&end, NULL); 
	printElapsed(&end, &begin);
	
	cout << "\nConc Test 8: LockLESS Queue, mayhem" << endl;
	gettimeofday(&begin, NULL);
	CTest8();
	gettimeofday(&end, NULL); 
	printElapsed(&end, &begin);
	
    exit(0);
}
