
#pragma once
#include <memory>
#include <atomic>
#include <gtest/gtest.h>
#include <iostream>
#include <functional>
using namespace std;


template<typename T>
class rcq{
    public:
        void push(T t);
        unique_ptr<T> pop();
        struct node;
        struct counter;
        struct counted_node{
            uintptr_t count = 0;
            node* node = nullptr;
            counted_node():count(0),node(nullptr){
};
            bool operator==(const counted_node& other) const{
                return other.count==this->count && other.node == this->node;
            };
            
        };
        struct node{
            T data;
            atomic<counted_node> next;
            atomic<counter> counter;
            node():data(T{}),next(counted_node{}){
            };
            node(T t):data(t),next(counted_node{}){

            };
        };

        struct counter{
            int external;
            int internal;
            counter():external(2),internal(0){
            };

        };

        rcq(){
            counted_node cn{};
            cn.node=new node{};
            cn.count = 1;
            front.store(cn);
            back.store(cn);
        };
        void set_delete_func(function<void(node*)> deleter){
            this->deleteFunc=deleter;
        };
        static_assert(has_unique_object_representations<counted_node>::value, "counted_node must have unique object representations");
    private:
        atomic<counted_node> front;
        atomic<counted_node> back;
        FRIEND_TEST(TESTRCQ, FIRST_PUSH);
        FRIEND_TEST(TESTRCQ, PUSH);
        FRIEND_TEST(TESTRCQ, CONCURRENT_PUSH);
        function<void(node*)> deleteFunc = [](node* n){
        };
        
};

template<typename T>
void rcq<T>::push(T t){
    auto i=0;
    auto new_node= counted_node{};
    new_node.node=new node(t);
    new_node.count = 1;
    for(;;){
        auto old_back = back.load(memory_order_acquire);
        claim_node<T>(back,old_back);
        auto ptr=old_back.node;
        auto zero=counted_node{};
        if(ptr->next.compare_exchange_strong(zero,new_node,memory_order_acq_rel,memory_order_acquire)){
            if(back.compare_exchange_strong(old_back,new_node,memory_order_release,memory_order_relaxed)){
                release_structural_claim<T>(old_back,this->deleteFunc);
            }else{
                release_claim<T>(ptr,this->deleteFunc);
            };
            return;
        }else{
            set_tail<T>(back,old_back,zero,this->deleteFunc);
        };
    };
};

template<typename T>
unique_ptr<T> rcq<T>::pop(){

    
    for(;;){
        auto zero=counted_node{};
        auto old_back = back.load(memory_order_acquire);
        claim_node<T>(back,old_back);
        auto old_back_ptr = old_back.node;
        auto next = old_back_ptr->next.load(memory_order_acquire);
        if(next!=zero){
            set_tail<T>(back,old_back,next,this->deleteFunc);
            continue;
        };
        auto old_front = front.load(memory_order_acquire);
        claim_node<T>(front,old_front);
        auto ptr=old_front.node;
        auto old_front_next=ptr->next.load(memory_order_acquire);
        if(old_front_next==zero){
            release_claim<T>(ptr,this->deleteFunc);
            return nullptr;
        };
        T t = old_front_next.node->data;
        if(front.compare_exchange_strong(old_front,old_front_next,memory_order_release,memory_order_relaxed)){
            release_structural_claim<T>(old_front,this->deleteFunc);
            return make_unique<T>(t);
        };
        release_claim<T>(ptr,this->deleteFunc);
    };


    return nullptr;
};

template<typename T>
using counted_node= rcq<T>::counted_node;
template<typename T>
using node=rcq<T>::node;
template<typename T>
using deleter=function<void(node<T>*)>;
template<typename T>
using node_counter = rcq<T>::counter;

template<typename T>
void claim_node(atomic<counted_node<T>>& shared_counted_node,counted_node<T>& old_counted_node){
    counted_node<T> new_counted_node;
    do{
        new_counted_node=old_counted_node;
        ++new_counted_node.count;
    }while (!shared_counted_node.compare_exchange_strong(old_counted_node,new_counted_node,memory_order_acquire,memory_order_relaxed));
    old_counted_node.count=new_counted_node.count;
};

template<typename T>
void release_structural_claim(counted_node<T>& countedNode,deleter<T> deleter_func){
       auto ptr = countedNode.node;
       auto counter = ptr->counter.load(memory_order_relaxed);
       int increase = countedNode.count-2;
       node_counter<T> new_node_counter;
       do{
        new_node_counter=counter;
        --new_node_counter.external;
        new_node_counter.internal+=increase;
       }while (!ptr->counter.compare_exchange_strong(counter,new_node_counter,memory_order_acq_rel,memory_order_relaxed));
       
       if(!new_node_counter.external && !new_node_counter.internal){
            deleter_func(ptr);
       };
};

template<typename T>
void release_claim(node<T>* ptr, deleter<T> deleter_func){
    auto counter = ptr->counter.load();
    node_counter<T> new_node_counter;
    do{
        new_node_counter=counter;
        --new_node_counter.internal;
    }while (!ptr->counter.compare_exchange_strong(counter,new_node_counter,memory_order_acq_rel,memory_order_relaxed));
    if(!new_node_counter.external && !new_node_counter.internal){
            deleter_func(ptr);
    };
};

template<typename T>
void set_tail(atomic<counted_node<T>>& shared_counted_node,counted_node<T>& old_counted_node,counted_node<T>& new_counted_node,deleter<T> deleter_func){
    auto ptr=old_counted_node.node;
     while(!shared_counted_node.compare_exchange_strong(old_counted_node,new_counted_node,memory_order_release,memory_order_relaxed) && ptr==old_counted_node.node);
    
    if(ptr==old_counted_node.node){
        release_structural_claim<T>(old_counted_node,deleter_func);
    }else{
        release_claim<T>(ptr,deleter_func);
    };
};