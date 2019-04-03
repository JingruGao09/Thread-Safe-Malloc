#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include "my_malloc.h"
#include<pthread.h>
/* This struct includes the meta information of each block
 * 2 pointers, 1 bool, 1 size_t
 */
typedef struct _node_info{
    bool isfree;
    struct _node_info * free_prev;
    struct _node_info * free_next;
    size_t size;
}_node_info;
typedef struct _node_info node_info;


__thread node_info *free_head = NULL;//thread local storage
node_info *fhead = NULL;//global
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;//Initialized with default pthread mutex attributes

/* This function is to give the block value
 * passed in through parameters
 */
node_info default_value(size_t size,bool free_flag, node_info * f_p,node_info* f_n){
    node_info node;
    node.size = size;
    node.isfree = free_flag;
    node.free_prev = f_p;
    node.free_next = f_n;
    return node;
}

/* This function is to create space for new malloc
 * with lock on sbrk
 * 1 parameter:
 * size (the required malloc size)
 */
node_info *new_node_nolock(size_t size){
    pthread_mutex_lock(&lock);
    node_info *new_seg = sbrk(0);
    void *space = sbrk(sizeof(node_info)+size);//increment the heap space
    pthread_mutex_unlock(&lock);
    if(space == (void*)-1){//check if size is not valid
      return NULL;
    }
    *new_seg=default_value(size,false,NULL,NULL);
    return new_seg;
}

/* This function is to find the best fit block to put the data
 * because lock and no_lock version use diff free list head
 * we pass a parameter to distinguish it
 */
node_info * best_fit(size_t size, char lock_nolock){
  node_info * node = NULL;
  if(lock_nolock == 'n'){
    node = free_head;
  }
  if(lock_nolock == 'l'){
    node = fhead;
  }
  node_info *min_node = NULL;
  size_t min_space = 0;
  size_t curr_diff = 0;
  while(node != NULL){
    if(node->size >= size){
      min_space = node->size - size;
      min_node = node;
      break;
    }
    node = node->free_next;
  }
  while(node!=NULL){
    if(node->size >= size){
      curr_diff = node->size - size;
      if(curr_diff==0){
	min_node = node;
	break;
      }
      if(min_space > curr_diff){
	min_node = node;
	min_space = node->size - size;
      }
    }
    node = node->free_next;
  }
  node = min_node;
  return node;    
}

/* This function is to create space for new malloc
 * for lock version
 * 1 parameter:
 * size (the required malloc size)
 */
node_info *new_node(size_t size){
    node_info *new_seg = sbrk(0);
    void *space = sbrk(sizeof(node_info)+size);//increment the heap space
    if(space == (void*)-1){//check if size is not valid
      return NULL;
    }
    *new_seg=default_value(size,false,NULL,NULL);
    return new_seg;
}

/* This function is to realize malloc
 * with lock at the start of malloc
 * 1 parameter:
 * size (the required malloc size)
 */
void *ts_malloc_lock(size_t size){
  if(size == 0){
        return NULL;
  }
  pthread_mutex_lock(&lock);
  node_info * node = NULL;
  node = best_fit(size,'l');
  if(node == NULL){
    node = new_node(size);
  }
  else{
    if(node->size > sizeof(node_info) + size){
      node_info *rest;
      rest = (node_info*)((void*)(node+1) + size);
      rest->size=node->size - size - sizeof(node_info);
      rest->isfree=true;
      rest->free_next=node->free_next;
      
      if(node->free_next!=NULL){
	node->free_next->free_prev=rest;
      }
      else{
	rest->free_next=NULL;
      }
      rest->free_prev=node->free_prev;
      if(node->free_prev!=NULL){
	node->free_prev->free_next=rest;
      }
      else{
	fhead=rest;
      }
      
      if(node==fhead){
	fhead=rest;
      }
      node->free_next=NULL;
      node->size = size;
      node->isfree = false;
    }
    else{
      node->isfree = false;	
      if(node->free_prev!=NULL){
	node->free_prev->free_next=node->free_next;
	if(node->free_next!=NULL){
	  node->free_next->free_prev=node->free_prev;
	}
      }
      else{
	fhead=node->free_next;
	if(fhead!=NULL){
	  fhead->free_prev=NULL;
	}
      }
    }
  }
  node->free_prev=NULL;
  node->free_next=NULL;
  pthread_mutex_unlock(&lock);
  return (void*)(node + 1);
}

/* This function is to coalesce neighbouring freed blocks
 * 2 parameters:
 * curr(current block to coalesce)
 * curr_next(next block to coalesce)
 */
void coalesce(node_info*curr,node_info*curr_next){
  curr->size=curr->size+curr_next->size+sizeof(node_info);
  curr->free_next=curr_next->free_next;
  if(curr_next->free_next==NULL){
    curr->free_next=NULL;
  }
  else{
    curr->free_next->free_prev=curr;
  }
  curr_next->free_next=NULL;//added
}

/* This function is to free allocated space
 * with lock at the start of free
 * Note: we need to unlock before every return in case of program going to dead lock
 * 1 parameter:
 * ptr(user passed in pointer which points to the block to be freed)
 */
void ts_free_lock(void *ptr){
    if(ptr == NULL){
      return;
    }
    pthread_mutex_lock(&lock);
    node_info *temp = ptr;
    temp--;
    temp->isfree = true;
    if(fhead == NULL){
      fhead = temp;
      pthread_mutex_unlock(&lock);
      return;
    }
    if(temp<fhead){
      temp->free_next=fhead;
      fhead->free_prev=temp;
      fhead=temp;
      fhead->free_prev=NULL;
    }
    else{
      node_info *iter=fhead;
      while(iter!=NULL){
	if(iter->free_next!=NULL){
	  if(temp>iter &&temp<iter->free_next){
	    break;
	  }
	}
	else{
	  break;
	}
	iter=iter->free_next;
      }  
      if(iter!=NULL){
	if(iter->free_next!=NULL){	
	  temp->free_next=iter->free_next;
	  temp->free_prev=iter;
	  temp->free_next->free_prev=temp;
	  iter->free_next=temp;
	}
	else{
	  temp->free_prev=iter;
	  iter->free_next=temp;
	}
      }
    }
    if(temp->free_next!=NULL){
      if(((size_t)temp->free_next-temp->size-sizeof(node_info))==(size_t)temp){
	coalesce(temp,temp->free_next);
      }
    }
    if(temp->free_prev!=NULL){
      if(((size_t)temp->free_prev+temp->free_prev->size+sizeof(node_info))==(size_t)temp){
    	coalesce(temp->free_prev,temp);
      }
    }
    pthread_mutex_unlock(&lock);
}


/* This function is to realize malloc
 * we only need to use lock at create new space for the nod
 * 1 parameter:
 * size (the required malloc size)
 */
void* ts_malloc_nolock(size_t size){
  if(size == 0){
        return NULL;
  }
  node_info * node = NULL;
  node = best_fit(size,'n');
  if(node == NULL){
    node = new_node_nolock(size);
  }
  else{
    if(node->size > sizeof(node_info) + size){
      node_info *rest;
      rest = (node_info*)((void*)(node+1) + size);
      rest->size=node->size - size - sizeof(node_info);
      rest->isfree=true;
      rest->free_next=node->free_next;
      if(node->free_next!=NULL){
	node->free_next->free_prev=rest;
      }
      else{
	rest->free_next=NULL;
      }
      rest->free_prev=node->free_prev;
      if(node->free_prev!=NULL){
	node->free_prev->free_next=rest;
      }
      else{
	free_head=rest;
      }
      if(node==free_head){
	free_head=rest;
      }
      node->free_next=NULL;
      node->size = size;
      node->isfree = false;
    }
    else{
      node->isfree = false;	
      if(node->free_prev!=NULL){
	node->free_prev->free_next=node->free_next;
	if(node->free_next!=NULL){
	  node->free_next->free_prev=node->free_prev;
	}
      }
      else{
	free_head=node->free_next;
	if(free_head!=NULL){
	  free_head->free_prev=NULL;
	}
      }
    }   
  }
  node->free_prev=NULL;
  node->free_next=NULL;
  return (void*)(node + 1);
}

/* This function is to free allocated space
 * for no_lock version, which is just as normal free,
 * Note: we need to compare physical address of two neighbour block
 * to make sure they can be merged together
 * 1 parameter:
 * ptr(user passed in pointer which points to the block to be freed)
 */
void ts_free_nolock(void *ptr){
  if(ptr == NULL){
      return;
  }
  node_info *temp = ptr;
  temp--;
  temp->isfree = true;
  if(free_head == NULL){
    free_head = temp;
    return;
  }
  if(temp<free_head){
    temp->free_next=free_head;
    free_head->free_prev=temp;
    free_head=temp;
    free_head->free_prev=NULL;
  }
  else{
    node_info *iter=free_head;
    while(iter!=NULL){
      if(iter->free_next!=NULL){
	if(temp>iter &&temp<iter->free_next){
	  break;
	}
      }
      else{
	break;
      }
      iter=iter->free_next;
    }  
    if(iter!=NULL){
      if(iter->free_next!=NULL){	
	temp->free_next=iter->free_next;
	temp->free_prev=iter;
	temp->free_next->free_prev=temp;
	iter->free_next=temp;
      }
      else{
	temp->free_prev=iter;
	iter->free_next=temp;
      }
    }
  }
  if(temp->free_next!=NULL){
    if(((size_t)temp->free_next-temp->size-sizeof(node_info))==(size_t)temp){
      coalesce(temp,temp->free_next);
    }
  }
  if(temp->free_prev!=NULL){
    if(((size_t)temp->free_prev+temp->free_prev->size+sizeof(node_info))==(size_t)temp){
      coalesce(temp->free_prev,temp);
    }
  }
}

