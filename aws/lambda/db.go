package main

import (
    "github.com/aws/aws-sdk-go/aws"
    "github.com/aws/aws-sdk-go/aws/session"
    "github.com/aws/aws-sdk-go/service/dynamodb"
)

var db = dynamodb.New(session.New(), aws.NewConfig().WithRegion("us-west-2"))

func putItem(ioTEvent *IoTEvent) error {
    input := &dynamodb.PutItemInput{
        TableName: aws.String("Webcam"),
        Item: map[string]*dynamodb.AttributeValue{
            "id": {
                N: aws.String("1" + ioTEvent.Client + ioTEvent.Message + ioTEvent.Packet),
            },
            "client": {
                N: aws.String(ioTEvent.Client),
            },
            "message": {
                N: aws.String(ioTEvent.Message),
            },
            "packet": {
                N: aws.String(ioTEvent.Packet),
            },
            "type": {
                N: aws.String(ioTEvent.Type),
            },
            "payload": {
                S: aws.String(ioTEvent.Payload),
            },
        },
    }

    _, err := db.PutItem(input)
    return err
}
