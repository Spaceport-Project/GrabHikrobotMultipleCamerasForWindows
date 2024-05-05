#include "concurrentqueue.h"
#include <iostream>
moodycamel::ConcurrentQueue<int> q;
const int ProducerCount = 8;
const int ConsumerCount = 8;
std::thread producers[ProducerCount];
std::thread consumers[ConsumerCount];
std::atomic<int> doneProducers{0};
std::atomic<int> doneConsumers{0};
int main() {
    int produce = 0;
  


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
    return 0;
}