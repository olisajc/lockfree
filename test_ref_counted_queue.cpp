#include <gtest/gtest.h>
#include "ref_counted_queue.hpp"
#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <thread>
#include <shared_mutex>
#include <vector>
//g++ test_ref_counted_queue.cpp -I/opt/homebrew/include -L/opt/homebrew/lib -lgtest -lgtest_main -pthread -o test -std=c++20
class TESTRCQ: public ::testing::Test{
    protected:
        rcq<int>* q = new rcq<int>{};

};
void push(rcq<int>* q,int count);
set<int> pop(rcq<int>* q,int count);
TEST_F(TESTRCQ,FIRST_PUSH){
    q->push(100);
    auto front=q->front.load();
    auto ptr = front.node;
    ASSERT_EQ(ptr->next.load().node->data,100);
    ASSERT_EQ(q->back.load().node->data,100);
};

TEST_F(TESTRCQ,PUSH){
    q->push(100);
    q->push(200);
    auto back=q->back.load();
    auto ptr = back.node;
    // ASSERT_EQ(ptr->next.load().node->data,200);
    ASSERT_EQ(q->back.load().node->data,200);
};

TEST_F(TESTRCQ,CONCURRENT_PUSH){
  
    atomic<int> called;
    auto deleterFunc = [&](rcq<int>::node* n){
        called.fetch_add(1);
    };
    q->set_delete_func(deleterFunc);
    set<int> s;
    push(q,200);

    auto front=q->front.load();
    auto ptr=front.node;

    while(ptr->next.load().node){
        ptr=ptr->next.load().node;
        s.insert(ptr->data);
    }
    ASSERT_EQ(called.load(),0);
    ASSERT_EQ(s.size(),200);
};


TEST_F(TESTRCQ,POP_EMPTY){
    auto res=q->pop();
    ASSERT_EQ(res,nullptr);
};


TEST_F(TESTRCQ,POP_FIRST){
    q->push(100);
    ASSERT_EQ(*q->pop(),100);
};


TEST_F(TESTRCQ,POP_CONCURRENT){
    atomic<int> called{0};
    q->set_delete_func([&](rcq<int>::node* n){
        called.fetch_add(1);
        delete n;
    });
    push(q,1000);
    auto s = pop(q,1000);
    ASSERT_EQ(s.size(),1000);
    ASSERT_EQ(called.load(),1000);
};

TEST_F(TESTRCQ,PUSH_AND_POP_CONCURRENT){
    q->set_delete_func([&](rcq<int>::node* n){
        delete n;
    });

    const int producer_count = 4;
    const int values_per_producer = 250;
    const int consumer_count = 8;
    const int total = producer_count * values_per_producer;

    atomic<int> producers_left{producer_count};
    mutex popped_mutex;
    set<int> popped;
    vector<thread> producers;
    vector<thread> consumers;

    for(int p=0;p<producer_count;p++){
        producers.emplace_back([&,p]{
            int base = p * values_per_producer;
            for(int i=1;i<=values_per_producer;i++){
                if(i % 2 == 0){
                    this_thread::sleep_for(chrono::microseconds(100));
                };
                q->push(base + i);
            };
            producers_left.fetch_sub(1);
        });
    };

    for(int c=0;c<consumer_count;c++){
        consumers.emplace_back([&]{
            int empty_after_done = 0;
            for(;;){
                auto res = q->pop();
                if(res){
                    {
                        lock_guard<mutex> lock(popped_mutex);
                        popped.insert(*res);
                    };
                    empty_after_done = 0;
                }else if(producers_left.load() == 0){
                    if(++empty_after_done > 10000){
                        break;
                    };
                    this_thread::yield();
                }else{
                    this_thread::yield();
                };
            };
        });
    };

    for(auto& t : producers) t.join();
    for(auto& t : consumers) t.join();

    for(auto value : popped){
        ASSERT_GE(value,1);
        ASSERT_LE(value,total);
    };
    ASSERT_EQ(popped.size(),static_cast<size_t>(total));
    for(int i=1;i<=total;i++){
        ASSERT_TRUE(popped.contains(i)) << "missing " << i;
    };
};

void push(rcq<int>* q,int count){
    vector<thread> v;
    for(int i=1;i<=count;i++){
        v.emplace_back(
            [](rcq<int>* q,int i){
                if (i % 2 == 0) {
                    this_thread::sleep_for(chrono::milliseconds(1));
                };
                q->push(i);
            },q,i
        );
    };
    for(auto& t : v) t.join();
};

set<int> pop(rcq<int>* q,int count){
    shared_mutex m;
    set<int> s;
    vector<thread> v;
    for(int i=1;i<=count;i++){
        v.emplace_back(
            [&](rcq<int>* q,int i){
                if (i % 2 == 0) {
                    this_thread::sleep_for(chrono::milliseconds(1));
                };
                auto res=q->pop();
                EXPECT_NE(res,nullptr);
                unique_lock<shared_mutex> lock(m);
            
                s.insert(*res);
            },q,i
        );
    };
    for(auto& t : v) t.join();
    return s;
};
