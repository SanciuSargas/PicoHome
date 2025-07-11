/**
 * @file CommunicationInterface.h
 * @brief Declaration of CommunicationInterface: API for communication between microcontroller and server.
 */

#ifndef COMMUNICATIONINTERFACE_H
#define COMMUNICATIONINTERFACE_H
#include <Arduino.h>

class CommunicationInterface
{
    public:
        CommunicationInterface();
        virtual void publish(char* topic, byte* payload) = 0;
        virtual void subscribe(char* topic) = 0;
        virtual ~CommunicationInterface() = default;
};

#endif //COMMUNICATIONINTERFACE_H
