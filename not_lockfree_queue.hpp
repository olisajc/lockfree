#pragma once
#include <gtest/gtest_prod.h>
#include <stdexcept>
using namespace std;
/*
    invariants:
    - head and tail is never null
    - Real List starts from head->next
    - if head->next = nil list is empty and head=tail
    - tail holds the last entry in a list, tail-next is always nil
*/
template<typename T>
class not_lock_free{
    class empty_exception : public exception{
        public:
            const char* what(){
                return "queue is empty";
            };
    };
    struct node {    
        T data;
        node* next; 
        node(T t):data(t),next(nullptr){};
        node():data(T{}),next(nullptr){};
    };
    private:
        node* head;
        node* tail;
        FRIEND_TEST(not_lock_free_test,head_tail_equal_on_construction);
        FRIEND_TEST(not_lock_free_test,push_into_empty_list);
        FRIEND_TEST(not_lock_free_test,push);
        FRIEND_TEST(not_lock_free_test,pop_empty_queue);
        FRIEND_TEST(not_lock_free_test,pop);
        FRIEND_TEST(not_lock_free,pop_removes_all);
    public:
        not_lock_free(){
            node * dummy = new node();
            head=dummy;
            tail=dummy;
        };
        void push(T t){
            node* new_node = new node(t);
            this->tail->next=new_node;
            tail=new_node;
        };
        T pop(){
            if(!this->head->next){
                throw empty_exception();
            };
            node * head_next=head->next;
            T result = head_next->data;
            head->next=head->next->next;
            if (!head->next) tail=head;
            delete head_next;
            return result;
        };

};

