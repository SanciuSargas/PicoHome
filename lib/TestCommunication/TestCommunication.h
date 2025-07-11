/**
 * @file TestCommunication.h
 * @brief Declaration of TestCommunication: Test communication between microcontroller and server.
 */

#ifndef TESTCOMMUNICATION_H
#define TESTCOMMUNICATION_H
#include "../CommunicationInterface/CommunicationInterface.h"

class TestCommunication : CommunicationInterface
{
    public:
        void publish(char* topic, byte* payload) override;
        void subscribe(char* topic) override;
};
#endif //TESTCOMMUNICATION_H
