#ifndef IQUEUE_H
#define IQUEUE_H

namespace ConcurrentQueues
{
  template <class T> 
  class IQueue {
  public:
    // Adds a value to the queue
    virtual void Enqueue(T value) = 0;
    // Returns true, pops and fills in *value if
    // the queue is non-empty, false otherwise
    virtual bool Dequeue(T *value) = 0;
    virtual ~IQueue(){}
  };

  template<class T> 
  struct Node {
    T Value;
    Node<T> *Next;
  };
}

#endif
