package main

import (
    "strconv"
    "github.com/aws/aws-sdk-go/aws"
    "github.com/aws/aws-sdk-go/aws/session"
    "github.com/aws/aws-sdk-go/service/dynamodb"
)

var db = dynamodb.New(session.New(), aws.NewConfig().WithRegion("us-west-2"))

func putItem(iotMessage *iotMessage) error {
    client := strconv.Itoa(iotMessage.Client)
    messageNr := strconv.Itoa(iotMessage.MessageNr)
    packetNr := strconv.Itoa(iotMessage.PacketNr)
    
    input := &dynamodb.PutItemInput{
        TableName: aws.String("Webcam"),
        Item: map[string]*dynamodb.AttributeValue{
            "id": {
                N: aws.String("1" + client + messageNr + packetNr),
            },
            "client": {
                N: aws.String(client),
            },
            "message": {
                N: aws.String(messageNr),
            },
            "packet": {
                N: aws.String(packetNr),
            },
            "type": {
                N: aws.String(strconv.Itoa(iotMessage.Type)),
            },
            "payload": {
                S: aws.String(iotMessage.Payload),
            },
        },
    }

    _, err := db.PutItem(input)
    return err
}
