
#include <gtest/gtest.h>
#include "not_lockfree_queue.hpp"
#include <vector>
using namespace std;

// g++ test.cpp -lgtest -lgtest_main -pthread -o test
// ./test
// g++ test_not_lockfree_queue.cpp -I/opt/homebrew/include -L/opt/homebrew/lib -lgtest -lgtest_main -pthread -o test -std=c++20

class not_lock_free_test : public ::testing::Test {
    protected:
        not_lock_free<int> q{};
};

TEST_F(not_lock_free_test,head_tail_equal_on_construction){
    ASSERT_EQ(q.head,q.tail);
}

TEST_F(not_lock_free_test,push_into_empty_list){
    int v=1;
    not_lock_free<int> q;
    q.push(v);
    ASSERT_EQ(q.tail->data,v);
    ASSERT_EQ(q.tail->next,nullptr);
    ASSERT_EQ(q.head->next,q.tail);
}

TEST_F(not_lock_free_test,push){
    int stop=10;
    for(int i=1;i<=stop;i++){
        q.push(i);
    };
    not_lock_free<int>::node * head=q.head->next; 
    for(int i=1;i<=stop;i++){
        EXPECT_NE(head,nullptr);
        EXPECT_EQ(i,head->data);
        head=head->next;
    };
}


TEST_F(not_lock_free_test,pop_empty_queue){
    ASSERT_THROW(q.pop(),not_lock_free<int>::empty_exception);
}

TEST_F(not_lock_free_test,pop){
   q.push(1);
   EXPECT_EQ(1,q.pop());
   ASSERT_EQ(q.head->next,nullptr);
   ASSERT_EQ(q.head,q.tail);
}

TEST_F(not_lock_free_test,pop_removes_all){
    int stop=10;
    for(int i=1;i<=stop;i++){
        q.push(i);
    };

    for(int i=1;i<=stop;i++){
        ASSERT_EQ(i,q.pop());
    };
}