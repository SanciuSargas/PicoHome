/**
 * @file Subscriber.h
 * @brief Declaration of Subscriber: Observer design pattern, interface for subscribers (actuators).
 */

#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

class Subscriber
{
    public:
        Subscriber() = default;
        /**
         * @brief Function for Publisher (sensors) to call once an action happens.
         */
        virtual void callback() = 0;
        virtual ~Subscriber() = default;
};
#endif //SUBSCRIBER_H
