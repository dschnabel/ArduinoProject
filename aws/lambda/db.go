package main

import (
    "strconv"
    
    "github.com/aws/aws-sdk-go/aws"
    "github.com/aws/aws-sdk-go/aws/session"
    "github.com/aws/aws-sdk-go/service/dynamodb"
    "github.com/aws/aws-sdk-go/service/dynamodb/dynamodbattribute"
)

var db = dynamodb.New(session.New(), aws.NewConfig().WithRegion("us-west-2"))

func dbPutPacket(ioTEvent *IoTEvent) error {
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
            "type": {
                N: aws.String(ioTEvent.Type),
            },
            "packet": {
                N: aws.String(ioTEvent.Packet),
            },
            "payload": {
                S: aws.String(ioTEvent.Payload),
            },
        },
    }

    _, err := db.PutItem(input)
    return err
}

func dbGetMessage(ioTEvent *IoTEvent) (map[int]string, error) {
    packets, err := strconv.Atoi(ioTEvent.Packet)
    if err != nil {
        return nil, err
    }
    
    attr := []map[string]*dynamodb.AttributeValue{}
    for i := 0; i < packets; i++ {
        attr = append(attr, map[string]*dynamodb.AttributeValue{"id": {N: aws.String("1" + ioTEvent.Client + ioTEvent.Message + strconv.Itoa(i))}})
    }
    
    input := &dynamodb.BatchGetItemInput{
        RequestItems: map[string]*dynamodb.KeysAndAttributes{
            "Webcam": {
                Keys: attr,
                ProjectionExpression: aws.String("packet, payload"),
            },
        },
    }
    
    result, err := db.BatchGetItem(input)
    if err != nil {
        return nil, err
    }
    
    payload := make(map[int]string)
    for _, element := range result.Responses["Webcam"] {
        dbItem := new(DbItem)
        err = dynamodbattribute.UnmarshalMap(element, dbItem)
        if err != nil {
            return nil, err
        }
        payload[dbItem.Packet] = dbItem.Payload
    }
    
    return payload, nil
}
