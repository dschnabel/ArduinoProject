package main

import (
    "github.com/aws/aws-sdk-go/aws"
    "github.com/aws/aws-sdk-go/aws/session"
    "github.com/aws/aws-sdk-go/service/iotdataplane"
)

var iot = iotdataplane.New(session.New(), &aws.Config{
    Region:                 aws.String("us-west-2"),
    Endpoint:               aws.String(IotEndpoint),
})

func iotPushUpdate(topic string, payload []byte) bool {
    qos := int64(0)
    input := &iotdataplane.PublishInput{
        Payload: payload,
        Qos: &qos,
        Topic: &topic,
    }
    
    _, err := iot.Publish(input)
    if err != nil {
        errorLogger.Println(err.Error())
        return false
    }
    
    return true
}
