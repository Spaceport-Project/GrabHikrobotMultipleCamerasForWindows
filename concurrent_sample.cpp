#include "concurrentqueue.h"
#include "blockingconcurrentqueue.h"

#include <iostream>
moodycamel::ConcurrentQueue<int> q;
const int ProducerCount = 8;
const int ConsumerCount = 8;
std::thread producers[ProducerCount];
std::thread consumers[ConsumerCount];
std::atomic<int> doneProducers{0};
std::atomic<int> doneConsumers{0};
moodycamel::BlockingConcurrentQueue<int> blq;
template <typename T>
struct atomwrapper
{
  std::atomic<T> _a;

  atomwrapper()
    :_a()
  {}

  atomwrapper(const std::atomic<T> &a)
    :_a(a.load())
  {}

  atomwrapper(const atomwrapper &other)
    :_a(other._a.load())
  {}

  atomwrapper &operator=(const atomwrapper &other)
  {
    _a.store(other._a.load());
  }
};

int main() {
    int produce = 0;
  
    std::vector<atomwrapper<int>> myVector;

    // Create a new atomic<int> element and assign a value to it
    std::atomic<int> myAtomic(42);
    int a = myAtomic;

    // Push the atomic<int> element into the vector
    myVector.emplace_back(myAtomic);
    // blq.try_dequeue_bulk

    for (int i = 0; i != ConsumerCount; ++i) {
        consumers[i] = std::thread([&]() {
            int item;
            bool itemsLeft;
            bool b;
            do {
                // It's important to fence (if the producers have finished) *before* dequeueing
                int a = doneProducers.load(std::memory_order_acquire);
               
                itemsLeft = a != ProducerCount;
                printf("a:%d, itemleft: %d \n", a, itemsLeft);
                while (q.try_dequeue(item)) {
                    std::cout<<"item:"<<item<<std::endl;
                    itemsLeft = true;
                }
                b = doneConsumers.fetch_add(1, std::memory_order_acq_rel) + 1 == ConsumerCount;
                std::cout<<"b:"<<b<<std::endl;
            } while (itemsLeft || b );
            // The condition above is a bit tricky, but it's necessary to ensure that the
            // last consumer sees the memory effects of all the other consumers before it
            // calls try_dequeue for the last time
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    for (int i = 0; i != ProducerCount; ++i) {
    
        std::cout<<"produce:"<<produce<<std::endl;
        producers[i] = std::thread([produce]() {
                q.enqueue(produce);
                
            doneProducers.fetch_add(1, std::memory_order_release);
        });
        produce++;
    }

    


    for (int i = 0; i != ProducerCount; ++i) {
        producers[i].join();
    }

    for (int i = 0; i != ConsumerCount; ++i) {
        consumers[i].join();
    }


    moodycamel::ConcurrentQueue<int> q;

    moodycamel::ProducerToken ptok(q);
    q.enqueue(ptok, 17);

    moodycamel::ConsumerToken ctok(q);
    int item;
    q.try_dequeue(ctok, item);
    assert(item == 17);
    return 0;
}