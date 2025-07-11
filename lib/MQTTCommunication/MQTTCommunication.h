/**
 * @file MQTTCommunication.h
 * @brief Declaration of MQTTCommunication: MQTT communication between microcontroller and server.
 */

#ifndef MQTTCOMMUNICATION_H
#define MQTTCOMMUNICATION_H
#include "../CommunicationInterface/CommunicationInterface.h"

class MQTTCommunication : CommunicationInterface
{
    public:
        void publish(char* topic, byte* payload) override;
        void subscribe(char* topic) override;
};
#endif //MQTTCOMMUNICATION_H
