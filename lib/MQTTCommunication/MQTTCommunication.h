/**
 * @file MQTTCommunication.h
 * @brief Declaration of MQTTCommunication: MQTT communication between microcontroller and server.
 */

#ifndef MQTTCOMMUNICATION_H
#define MQTTCOMMUNICATION_H
#include "../CommunicationInterface/CommunicationInterface.h"

// TODO: maybe instead of implementing this just pass MQTT library client less coding
class MQTTCommunication : public CommunicationInterface
{
public:
    MQTTCommunication();
    void publish(char* topic, byte* payload) override;
    void subscribe(char* topic) override;
};
#endif //MQTTCOMMUNICATION_H
