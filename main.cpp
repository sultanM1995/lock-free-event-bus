#include <iostream>

#include "includes/event_bus.hpp"
#include "includes/publisher.hpp"
#include "includes/subscriber.hpp"

int main() {
    EventBus bus;
    Subscriber subscriber1(bus);

    subscriber1.subscribe("foo");
    subscriber1.subscribe("bar");

    Publisher pub1(bus, "foo");
    Publisher pub2(bus, "bar");

    std::thread t1(&Publisher::run, &pub1, 10, 100);
    std::thread t2(&Publisher::run, &pub2, 10, 100);


    for (int i = 0; i < 20; ++i) {
        for (const auto& topic : {"foo", "bar"}) {
            auto eventOpt = subscriber1.poll(topic);
            if (eventOpt.has_value()) {
                std::cout << "[Received from " << topic << "] "
                          << eventOpt->payload << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    t1.join();
    t2.join();

    return 0;
}
