#ifndef SIMPLEQUEUE_H
#define SIMPLEQUEUE_H

#include "IQueue.h"

namespace ConcurrentQueues
{
  template<class T>  
  class SimpleQueue : public IQueue<T> {			
  private:
    Node<T>* head;
    Node<T>* tail; 
    
  public:
    SimpleQueue() {
      Node<T> *node = new Node<T>();
      node->Next = 0;
      head = tail = node;
    }		
    
    ~SimpleQueue() {
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
      tail->Next = node;
      tail = node;
    }

    bool Dequeue(T* value) {
      Node<T>* node = head;
      Node<T>* next = node->Next;
      if(!next)	return false;
      *value = next->Value;
      head = next;
      delete node;
      return true;
    }
  };
}

#endif
