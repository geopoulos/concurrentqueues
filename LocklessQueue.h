#ifndef LOCKLESSQUEUE_H
#define LOCKLESSQUEUE_H

#include <list>
#include <map>
#include "IQueue.h"
#include <stdio.h>
#define CAS(a,x,y) __sync_bool_compare_and_swap(a,x,y)

using std::list;
using std::map;

// I'm not sure if using stl containers is a performance hit
// at this level.  They are only really used in scan operations
// which are infrequent.  After some testing, we can manually
// implement these if they are problematic.

namespace ConcurrentQueues
{

template<class T> 
class LocklessQueue {
private:

  // Max number of Hazard Pointers per HPRec, Queues need 2
  static const int K = 2;
  
  // Each ThreadAccessor will have a reference to one of these.
  // These records keep state for each thread for accessing the queue.
  struct HPRec {
    char padding0[64];
    Node<T>* HP[K];
    char padding1[64];
    HPRec* Next;
    bool Active;
    list<Node<T>*> RetireList;
    HPRec() : HP(), Next(0), Active(true), RetireList() {}
  };    
  
  int H; // Current number of hazard records
  HPRec* HeadHPRec; // First record in chain
  Node<T>* Tail; // Tail of the queue
  Node<T>* Head; // Head of the queue
  
  // When the size of a RetireList gets larger than this, scan is called.
  int R(){ return this->H * this->K; }
  
  // Allocates a new Hazard Record for a new thread
  HPRec* allocateHPRec() {
    // First try to reuse an old one that is not active anymore
    for(HPRec* hprec = this->HeadHPRec; hprec; hprec = hprec->Next){
      if(hprec->Active) continue;
      if(!CAS(&hprec->Active, false, true)) continue;
      return hprec;
    }  
  
    // Didn't find any old ones, so increment total count
    int oldcount;
    do { oldcount = this->H; }
    while(!CAS(&this->H, oldcount, oldcount+K));
  
    // Create a new one
    HPRec* hprec = new HPRec();
    
    // Add it to the HPRec chain
    HPRec* oldhead;
    do {
      oldhead = this->HeadHPRec;
      hprec->Next = oldhead;
    }while(!CAS(&this->HeadHPRec, oldhead, hprec));
  
    return hprec;
  }
  
  // Instead of deleting a record when done, 
  // just deactivate it for possible reuse
  void retireHPRec(HPRec* hprec) {
    for(int i=0;i<K;i++)
      hprec->HP[i] = 0;
    hprec->Active = false;
  }
  
  // Each thread that needs to use the queue will 
  // access it through an instance of this object
  
  // This is a FRIEND class, so it can access the
  // private members of LocklessQueue.  It is also
  // a private class so it cannot be accessed outside
  // of LocklessQueue.  The idea is for this class
  // is to encapsulate the per thread data instead of
  // using something like Thread Local Storage.  An
  // IQueue<T> reference is returned intead of this
  // class explicitly when created.
  class ThreadAccessor : public IQueue<T> {
  private:
    LocklessQueue<T>* queue;
    HPRec* hprec; 
    
    // Try to release any unrefenced nodes by
    // scanning the HPRec chain
    void scan(HPRec* head){    
      // Part 1:
      // Find any nodes that are currently in use
      map<Node<T>*,bool> plist;      
      HPRec* hprec = head;
      while(hprec){
        for(int i=0; i<this->queue->K;i++) {
          Node<T>* hptr = hprec->HP[i];
          if(hptr) plist[hptr] = true;
        }
        hprec = hprec->Next;
      }
      
      // Part 2:
      // Run through the RetireList and delete any
      // that aren't referenced in Part 1
      list<Node<T>*> tmplist;
      tmplist.swap(this->hprec->RetireList);
      while(!tmplist.empty()){
        Node<T>* node = tmplist.front();      
        tmplist.pop_front();      
        if(plist.count(node)){
          this->hprec->RetireList.push_back(node);
        }else{
          delete node;
        }
      }
    }
    
    // This will move retired nodes from inactive records
    // into an active record so they can be relased
    void helpscan(){
      for(HPRec* hprec = this->queue->HeadHPRec; hprec; hprec = hprec->Next){
        if(hprec->Active) continue;
        if(!CAS(&hprec->Active, false, true)) continue;
        while(!hprec->RetireList.empty()){
          Node<T>* node = hprec->RetireList.front();
          hprec->RetireList.pop_front();
          this->hprec->RetireList.push_back(node);
          HPRec* head = this->queue->HeadHPRec;
          if((int)this->hprec->RetireList.size() >= this->queue->R())
            this->scan(head);
        }
      }
    }
    
    // Retire, instead of freeing immediately.
    // The node may be referenced by another record.
    void retireNode(Node<T>* node) {
      this->hprec->RetireList.push_back(node);
      HPRec* head = this->queue->HeadHPRec;
      if((int)this->hprec->RetireList.size() >= this->queue->R()) {
        this->scan(head);
        this->helpscan();
      }
    }
    
  public:
    ThreadAccessor(LocklessQueue<T>* queue) : queue(queue) {
      this->hprec = this->queue->allocateHPRec();
    }
    
    ~ThreadAccessor() {
      this->queue->retireHPRec(this->hprec);
    }
    
    // Lockless Enqueue
    void Enqueue(T value) {
      Node<T>* node = new Node<T>();
      node->Value = value;
      node->Next = 0;
      
      Node<T>* t;
      Node<T>* next;
      while(true){
        t = this->queue->Tail;
        this->hprec->HP[0] = t;
        if(this->queue->Tail != t) continue;
        next = t->Next;
        if(this->queue->Tail != t) continue;
        if(next){ CAS(&this->queue->Tail, t, next); continue; }
        if(CAS(&t->Next, 0, node)) break;
      }
      CAS(&this->queue->Tail, t, node);
    }
    
    // Lockless Dequeue
    bool Dequeue(T* value) {
      Node<T>* h;
      Node<T>* t;
      Node<T>* next;
      while(true){
        h = this->queue->Head;
        this->hprec->HP[0] = h;
        if(this->queue->Head != h) continue;
        t = this->queue->Tail;
        next = h->Next;
        this->hprec->HP[1] = next;
        if(this->queue->Head != h) continue;
        if(!next) return false;
        if(h == t){ CAS(&this->queue->Tail, t, next); continue; }
        *value = next->Value;
        if(CAS(&this->queue->Head, h, next)) break;
      }
      this->retireNode(h);
      return true;
    }
  };
  
  friend class ThreadAccessor;

public:
  LocklessQueue() : H(0), HeadHPRec(0) {
    //Create a sentinel node initially.
    Node<T> *node = new Node<T>();
    node->Next = 0;
    this->Head = this->Tail = node;  
  }
  
  ~LocklessQueue() {
    HPRec* hprec = this->HeadHPRec;
    // Delete records
    while(hprec){
      HPRec* next = hprec->Next;
      // Delete retired nodes in each record
      while(!hprec->RetireList.empty()){
        Node<T>* node = hprec->RetireList.front();
        hprec->RetireList.pop_front();
        delete node;
      }
      delete hprec;
      hprec = next;
    }
    // Delete nodes in queue
    Node<T>* node = this->Head;
    while(node){
      Node<T>* next = node->Next;
      delete node;
      node = next;
    }
  }
  
  // Returns a pointer that should be freed
  // when not being used any longer.
  IQueue<T>* CreateAccessor() {
    return new ThreadAccessor(this);
  }  
};

}

#endif
