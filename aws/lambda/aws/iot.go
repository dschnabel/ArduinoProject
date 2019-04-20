package aws

import (
    "github.com/aws/aws-sdk-go/aws"
    "github.com/aws/aws-sdk-go/aws/session"
    "github.com/aws/aws-sdk-go/service/iotdataplane"
)

type IoTEvent struct {
    Client   string `json:"client"`
    Message  string `json:"message"`
    Category string `json:"category"`
    Packet   string `json:"packet"`
    Payload  string `json:"payload"`
}

var iot = iotdataplane.New(session.New(), &aws.Config{
    Region:                 aws.String("us-west-2"),
    Endpoint:               aws.String(IotEndpoint),
})

func IotPushUpdate(topic string, payload []byte) bool {
    qos := int64(0)
    input := &iotdataplane.PublishInput{
        Payload: payload,
        Qos: &qos,
        Topic: &topic,
    }
    
    _, err := iot.Publish(input)
    if err != nil {
        ErrorLogger.Println(err.Error())
        return false
    }
    
    return true
}
