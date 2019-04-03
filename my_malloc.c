#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include "my_malloc.h"

/* This struct includes the meta information of each block
 * 4 pointers, 1 bool, 1 size_t
 */
typedef struct _node_info{
    bool isfree;
    struct _node_info * next;
    struct _node_info * prev;
    struct _node_info * free_prev;
    struct _node_info * free_next;
    size_t size;
}_node_info;
typedef struct _node_info node_info;

/* Global variables
 * Mark the head and tail of original list and free list
 */
node_info *head = NULL;
node_info *tail = NULL;
node_info *free_head = NULL;
node_info *free_tail = NULL;
/* This function is to give the block value
 * passed in through parameters
 */
node_info default_value(size_t size,bool free_flag, node_info * f_p,node_info* f_n, node_info * pr,node_info * nt){
    node_info node;
    node.size = size;
    node.isfree = free_flag;
    node.free_prev = f_p;
    node.free_next = f_n;
    node.prev = pr;
    node.next = nt;
    return node;
}


/* This function is to create space for new malloc
 * 1 parameter:
 * size (the required malloc size)
 */
node_info *new_node(size_t size){
    node_info *new_seg;
    new_seg = sbrk(0);//current break address
    void *space = sbrk(sizeof(node_info)+size);//increment the heap space
    if(space == (void*)-1){//check if size is not valid
      //        printf("Error with sbrk!\n");
        new_seg = NULL;
    }
    else{
        *new_seg=default_value(size,false,NULL,NULL,tail,NULL);
        if(tail!=NULL){
            tail->next = new_seg;
        }
        else{
            head = new_seg;
        }
        tail = new_seg;
    }
    return new_seg;
}

/* This function is to realize malloc,
 * based on First Fit or Best Fit algorithm
 * 2 parameters:
 * size (the required malloc size)
 * search (char type, to distinguish ff and bf)
 */
void* self_malloc(size_t size,char search){
    if(size == 0){
        return NULL;
    }
    //printf("Enter malloc\n");
    node_info * node = NULL;
    if(free_head != NULL){
        //printf("Enter here \n");
        node = free_head;
        if(search == 'f'){
            while(node != NULL){
                if(node->size >= size){
                    break;
                }
                node = node->free_next;
            }
        }
        if(search == 'b'){
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
        }
        if(node == NULL){
            node = new_node(size);
            if(node==NULL){
                return NULL;
            }
        }
        else{
            if(node->size > sizeof(node_info) + size){
                node_info *rest;
                rest = (node_info*)((void*)(node+1) + size);
                *rest = default_value(node->size - size - sizeof(node_info),true,NULL,NULL,node,node->next);
                if(node->next == NULL){
                    tail = rest;
                }
                else{
                    rest->next->prev = rest;
                }
                node->size = size;
                node->next = rest;
                node->isfree = false;
                //freelist--maybe should abstract
                if(node->free_prev != NULL){
                    rest->free_prev = node->free_prev;
                    node->free_prev->free_next = rest;
                    if(node->free_next == NULL){
                        free_tail = rest;
                        rest->free_next = NULL;
                    }
                    else{//node->free_next != NULL
                        rest->free_next = node->free_next;
                        node->free_next->free_prev = rest;
                    }
                    node->free_prev = NULL;
                    node->free_next = NULL;
                }
                else{//node->free_prev == NULL
                    if(node->free_next!=NULL){
                        free_head = rest;
                        rest->free_prev =    NULL;
                        rest->free_next = node->free_next;
                        node->free_next->free_prev = rest;
                        node->free_next = NULL;
                    }
                    else{//node->free_next==NULL
                        free_head = rest;
                        free_tail = rest;
                        rest->free_next = NULL;
                        rest->free_prev = NULL;
                    }
                }
                //endfreelist
            }
            else{
                node->isfree = false;
                //freelist
                if(node->free_prev == NULL){
                    if(node->free_next!=NULL){
                        free_head = node->free_next;
                        node->free_next->free_prev = NULL;
                    }
                    else{//node->free_next == NULL
                        free_tail = NULL;
                        free_head = NULL;//freelist is now empty
                    }
                    node->free_next = NULL;
                }
                else{//node->free_prev != NULL
                    if(node->free_next == NULL){
                        free_tail = node->free_prev;
                        node->free_prev->free_next = NULL;
                        //node->free_prev=NULL;
                    }
                    else{//node->free_next != NULL
                        node->free_next->free_prev = node->free_prev;
                        node->free_prev->free_next = node->free_next;
                        //node->free_prev=NULL;
                        node->free_next=NULL;
                    }
                    node->free_prev=NULL;
                }
            }
        }
    }
    //endfreelist
    else{//head==null
        node = new_node(size);
        if(node==NULL){
            return NULL;
        }
    }
    return (void*)(node + 1);
}
//ff_malloc
void *ff_malloc(size_t size){
    return self_malloc(size,'f');
}

/* This function is to coalesce neighbouring freed blocks
 * 2 parameters:
 * curr(current block to coalesce)
 * curr_next(next block to coalesce)
 */
void coalesce(node_info *curr,node_info *curr_next){
    curr->next = curr_next->next;
    curr->size = curr->size + curr_next->size + sizeof(node_info);
    if(curr_next->next==NULL){
        tail=curr;
        free_tail = curr;//adding
        //curr->free_next->free_prev = NULL;//adding
        curr->free_next = NULL;//adding
    }
    else{
        curr->next->prev = curr;
        curr->free_next = curr_next->free_next;
        if(curr_next->free_next != NULL){
            curr_next->free_next->free_prev = curr;
        }
        else{//curr_next->free_next == NULL,free_tail
            free_tail = curr;
            curr_next->free_prev = NULL;
            curr_next->next = NULL;
            curr_next->prev = NULL;
        }
    }
}

/* This function is to check if the pointer
 * passed in to free in valid
 * 1 parameter:
 * ptr(user passed in pointer which points to the block to be freed)
 */
bool free_pointer_check(void *ptr){
    bool valid_pointer = false;
    if(head == NULL){
      perror("Nothing has been malloc!\n");
        exit(EXIT_FAILURE);
    }
    else{
        if((node_info*)ptr < head || ptr > sbrk(0)){
            perror("Not a valid pointer to allocated memory!\n");
            exit(EXIT_FAILURE);
        }
        else{
            valid_pointer = true;
        }
    }
    return valid_pointer;
}

/* This function is to free allocated space
 * 1 parameter:
 * ptr(user passed in pointer which points to the block to be freed)
 */
void ff_free(void *ptr){
    node_info *temp = ptr;
    temp--;
    if(ptr == NULL){
      //printf("Free null pointer, nothing happens!\n");
        return;
    }
    if(free_pointer_check(ptr)){
        if(temp->isfree == true){
            perror("Double freeing!\n");
            exit(EXIT_FAILURE);
        }
        temp->isfree = true;
        if(free_head == NULL){
            free_head = temp;
            free_tail = temp;
            return;
        }
        node_info * iter = temp->next;
        while(iter != NULL){
            if(iter->isfree == true){
                break;
            }
            iter = iter->next;
        }
        if(iter != NULL){
            temp->free_prev = iter->free_prev;
            //temp->free_next = iter;
            if(iter->free_prev!=NULL){
                iter->free_prev->free_next = temp;
            }
            else{
                free_head = temp;
            }
            iter->free_prev = temp;
        }
        else{//iter==null
            free_tail = temp;
            node_info *iter2 = temp->prev;
            while(iter2 != NULL){
                if(iter2->isfree == true){
                    break;
                }
                iter2 = iter2->prev;
            }
            if(iter2 != NULL){
                iter2->free_next = temp;
            }
            temp->free_prev = iter2;
        }
        temp->free_next = iter;
        if(temp->next!=NULL){
            if(temp->next->isfree == true){
                coalesce(temp,temp->next);
            }
        }
        if(temp->prev!=NULL){
            if(temp->prev->isfree == true){
                temp = temp->prev;
                coalesce(temp,temp->next);
            }
        }
        if(temp->free_prev == NULL){
            free_head = temp;
        }
        if(temp->free_next == NULL){
            free_tail = temp;
        }
    }
}

/* This function implement malloc using Best Fit algorithm
 * 1 paramenter:
 * size (the required malloc size)
 */
void *bf_malloc(size_t size){
  return self_malloc(size,'b');
}
/* This function free specific memory
 * 1 paramenter:
 * address of data to be freed
 */
void bf_free(void *ptr){
    ff_free(ptr);
}

/* This function calculate size of list
 * no parameter passed in
 * use global variable
 */
unsigned long get_data_segment_size(){
    node_info *iter=head;
    unsigned long data_size=0;
    while(iter!=NULL){
        data_size = data_size + iter->size + sizeof(node_info);
        iter=iter->next;
    }
    return data_size;
}

/* This function calculate calculate free_space by freelist
 * no parameter passed in
 * use global variable
 */
unsigned long get_data_segment_free_space_size(){
    node_info *iter = free_head;
    unsigned long    free_size = 0;
    while(iter!=NULL){
        free_size = free_size + iter->size + sizeof(node_info);
        iter=iter->free_next;
    }
    return free_size;
}