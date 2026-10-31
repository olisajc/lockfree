#include <gtest/gtest.h>
#include "lockfree_queue.hpp"
#include <vector>
#include <thread>
#include <set>
#include <mutex>
#include <atomic>
using namespace std;
// g++ test_lockfree_queue.cpp -I/opt/homebrew/include -L/opt/homebrew/lib -lgtest -lgtest_main -pthread -o test -std=c++20

class LOCK_FREE : public ::testing::Test {
    protected:
        lock_free_queue<int>* q = new lock_free_queue<int>();
};


TEST_F(LOCK_FREE,CONSTRUCTOR){
    EXPECT_NE(q->head.load(),nullptr);
    EXPECT_NE(q->tail.load(),nullptr);
    EXPECT_EQ(q->head.load(),q->tail.load());
}

TEST_F(LOCK_FREE,PUSH_EMPTY_LIST){
    int value=1000;
    q->push(value);
    EXPECT_NE(q->head.load()->data,value);
    lock_free_queue<int>::node* tail =q->tail.load();
    EXPECT_EQ(tail->data,value);
    int second_value=1001;
    q->push(second_value);
    EXPECT_EQ(q->tail.load()->data,second_value);
};

TEST_F(LOCK_FREE,PUSH_FROM_CONCURRENT_THREADS){
    vector<thread> v;
    set<int> s;
    int stop=10;
    for(int i=1;i<stop;i++){
        v.emplace_back([i](lock_free_queue<int>* q){
            q->push(i);
        },q);
        s.insert(i);
    };

    for(auto &t:v){
        t.join();
    };

    lock_free_queue<int>::node* head = q->head.load();
    lock_free_queue<int>::node* temp = head->next.load();
    set<int> s2;
    int count=0;
    while(temp){
        s2.insert(temp->data);
        lock_free_queue<int>::node* nxt=temp->next.load();
        temp=nxt;
        count++;
    };
    EXPECT_EQ(count,stop-1);
    EXPECT_EQ(s,s2);
};

TEST_F(LOCK_FREE,POP_EMPTY_LIST){
    auto result=q->pop();
    ASSERT_EQ(result,nullptr);
};


TEST_F(LOCK_FREE,POP_TEST_ORDER){
    int stop=100;
    for(int i=1;i<=stop;i++){
        q->push(i);
    };
    for(int i=1;i<=stop;i++){
        auto result=q->pop();
        ASSERT_NE(result,nullptr);
        ASSERT_EQ(result->data,i);
    };
};


TEST_F(LOCK_FREE,POP_TEST_MULTITHREADED){
    int stop = 1000;
    int thread_count = 8;
    vector<thread> v;
    vector<int> popped;
    mutex popped_mutex;
    set<int> expected;
    atomic<bool> start{false};

    for(int i=1;i<=stop;i++){
        q->push(i);
        expected.insert(i);
    };

    for(int i=0;i<thread_count;i++){
        v.emplace_back([&](){
            while(!start.load()){
            };

            for(;;){
                auto result=q->pop();
                if(!result){
                    break;
                };

                lock_guard<mutex> lock(popped_mutex);
                popped.push_back(result->data);
            };
        });
    };

    start.store(true);

    for(auto &t:v){
        t.join();
    };

    set<int> actual(popped.begin(),popped.end());
    EXPECT_EQ(popped.size(),stop);
    EXPECT_EQ(actual.size(),stop);
    EXPECT_EQ(actual,expected);
};

TEST_F(LOCK_FREE,POP_HELPS_TAIL_WHEN_TAIL_LAGS){
    q->push(42);

    auto dummy = q->head.load();
    auto first = dummy->next.load();
    ASSERT_NE(first,nullptr);
    ASSERT_EQ(q->tail.load(),first);

    q->tail.store(dummy);
    ASSERT_EQ(q->head.load(),q->tail.load());
    ASSERT_NE(q->head.load()->next.load(),nullptr);

    auto result = q->pop();

    ASSERT_NE(result,nullptr);
    EXPECT_EQ(result->data,42);
    EXPECT_EQ(q->head.load(),q->tail.load());
};

TEST_F(LOCK_FREE,PUSH_AND_POP_AT_SAME_TIME){
    int producer_count = 4;
    int consumer_count = 4;
    int values_per_producer = 250;
    int total = producer_count * values_per_producer;

    vector<thread> producers;
    vector<thread> consumers;
    vector<int> popped;
    mutex popped_mutex;
    set<int> expected;
    atomic<bool> start{false};
    atomic<int> producers_left{producer_count};

    for(int i=1;i<=total;i++){
        expected.insert(i);
    };

    for(int p=0;p<producer_count;p++){
        producers.emplace_back([&,p](){
            while(!start.load()){
                this_thread::yield();
            };

            int base = p * values_per_producer;
            for(int i=1;i<=values_per_producer;i++){
                q->push(base+i);
            };

            producers_left.fetch_sub(1);
        });
    };

    for(int c=0;c<consumer_count;c++){
        consumers.emplace_back([&](){
            while(!start.load()){
                this_thread::yield();
            };

            int empty_after_done = 0;
            while(empty_after_done < 1000){
                auto result=q->pop();
                if(result){
                    lock_guard<mutex> lock(popped_mutex);
                    popped.push_back(result->data);
                    empty_after_done = 0;
                }else if(producers_left.load()==0){
                    empty_after_done++;
                    this_thread::yield();
                }else{
                    this_thread::yield();
                };
            };
        });
    };

    start.store(true);

    for(auto &t:producers){
        t.join();
    };

    for(auto &t:consumers){
        t.join();
    };

    set<int> actual(popped.begin(),popped.end());
    EXPECT_EQ(popped.size(),static_cast<size_t>(total));
    EXPECT_EQ(actual.size(),static_cast<size_t>(total));
    EXPECT_EQ(actual,expected);
};
