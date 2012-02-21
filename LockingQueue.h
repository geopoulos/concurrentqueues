#ifndef LOCKINGQUEUE_H
#define LOCKINGQUEUE_H

#include <pthread.h>
#include "IQueue.h"

namespace ConcurrentQueues
{
  template<class T>  
  class LockingQueue : public IQueue<T> {			
  private:
    pthread_mutex_t enqMutex;
    pthread_mutex_t deqMutex;
    Node<T>* head;
    Node<T>* tail; 
    
  public:
    LockingQueue() {
      Node<T> *node = new Node<T>();
      node->Next = 0;
      head = tail = node;
      pthread_mutex_init(&enqMutex,0);
      pthread_mutex_init(&deqMutex,0);
    }		
    
    ~LockingQueue() {
      pthread_mutex_destroy(&enqMutex);
      pthread_mutex_destroy(&deqMutex);
      Node<T>* node = head;
      while(node){
        Node<T>* next = node->Next;
        delete node;
        node = next;
      }
    }

    void Enqueue(T value) {
      Node<T>* node = new Node<T>();
      node->Value = value;
      node->Next = 0;
      pthread_mutex_lock(&enqMutex);
      tail->Next = node;
      tail = node;
      pthread_mutex_unlock(&enqMutex);
    }

    bool Dequeue(T* value) {
      pthread_mutex_lock(&deqMutex);
      Node<T>* node = head;
      Node<T>* next = node->Next;
      if(!next) {
        pthread_mutex_unlock(&deqMutex);
        return false;
      }
      *value = next->Value;
      head = next;
      pthread_mutex_unlock(&deqMutex);
      delete node;
      return true;
    }
  };
}

#endif
