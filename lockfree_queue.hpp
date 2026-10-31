#pragma once
#include <gtest/gtest_prod.h>
#include <memory>
#include <atomic>
#include <iostream>
#include <thread>
using namespace std;

template<typename T>
class lock_free_queue{
    friend class lock_free_bench;
    struct node{
        T data;
        atomic<node*> next;
        node(T t):data(t),next(nullptr){};
        node():data(T{}),next(nullptr){};
    };
    private:
        atomic<node*> head;
        atomic<node*> tail;
        FRIEND_TEST(LOCK_FREE,CONSTRUCTOR);
        FRIEND_TEST(LOCK_FREE,PUSH_EMPTY_LIST);
        FRIEND_TEST(LOCK_FREE,PUSH_FROM_CONCURRENT_THREADS);
        FRIEND_TEST(LOCK_FREE,POP_HELPS_TAIL_WHEN_TAIL_LAGS);
    public:
        lock_free_queue(){
            node* new_node=new node();
            head=new_node;
            tail.store(new_node);
        };

        void push(T t){
            node* new_node=new node(t);
            for(;;){
                node * old_tail = tail.load(memory_order_acquire);
                node* should_be_null=nullptr;
                if(old_tail->next.compare_exchange_strong(should_be_null,new_node,memory_order_release,memory_order_acquire)){
                    tail.compare_exchange_weak(old_tail,new_node,memory_order_release,memory_order_relaxed);
                    break;
                };
                if(should_be_null!=nullptr){
                    tail.compare_exchange_strong(old_tail,should_be_null,memory_order_release,memory_order_relaxed);
                };
            };
        };

        unique_ptr<node> pop(){
            if(!head.load(memory_order_acquire)->next.load(memory_order_acquire)) return nullptr;
            for(;;){
                auto old_tail = tail.load(memory_order_acquire);
                auto old_tail_next=old_tail->next.load(memory_order_acquire);
                if (old_tail_next){
                    if(tail.compare_exchange_strong(old_tail,old_tail_next,memory_order_release,memory_order_relaxed)) continue;
                };
                
                auto old_head=head.load(memory_order_acquire);
                auto candidate = old_head->next.load(memory_order_acquire);
                if(!candidate) return nullptr;
                if(head.compare_exchange_weak(old_head,candidate,memory_order_acq_rel,memory_order_relaxed)){
                    return make_unique<node>(candidate->data);
                };
            };
            return nullptr;
        };

        
};


class lock_free_bench{
    public:
        template<typename T>
        static void push(lock_free_queue<T>* lq,int per_thread ,int thread_number){
            vector<thread> threads;

            for(int i=0;i<thread_number;i++){
                threads.emplace_back(
                    [&](){for(int j=0;j<per_thread;j++){lq->push(j);};});
            };

            for(auto& t : threads){
                t.join();
            };
        };
};