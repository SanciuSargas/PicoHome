/**
 * @file Publisher.h
 * @brief Declaration of Publisher: Observer design pattern, notify actuators on sensor change.
 */

#ifndef PUBLISHER_H
#define PUBLISHER_H
#include "../Subscriber/Subscriber.h"

class Publisher
{
    public:
        Publisher();
        virtual void subscribe(Subscriber* subscriber) = 0;
        virtual void unsubscribe(Subscriber* subscriber) = 0;
        virtual void notifySubscribers() = 0;
        virtual ~Publisher() = default;
    private:
        Subscriber* subscribers[];
};

#endif //PUBLISHER_H
