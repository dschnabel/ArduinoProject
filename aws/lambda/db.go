package main

import (
    "strconv"
    
    "github.com/aws/aws-sdk-go/aws"
    "github.com/aws/aws-sdk-go/aws/session"
    "github.com/aws/aws-sdk-go/service/dynamodb"
    "github.com/aws/aws-sdk-go/service/dynamodb/dynamodbattribute"
)

var db = dynamodb.New(session.New(), aws.NewConfig().WithRegion("us-west-2"))

func dbPutPacket(ioTEvent *IoTEvent) {
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
    if err != nil {
        errorLogger.Println(err.Error())
    }
}

func dbGetMessage(ioTEvent *IoTEvent) (map[int]string) {
    packets, err := strconv.Atoi(ioTEvent.Packet)
    if err != nil {
        errorLogger.Println(err.Error())
        return nil
    }
    
    /////////////////////////////////////// get packets //////////////////////////
    getAttr := []map[string]*dynamodb.AttributeValue{}
    for i := 0; i < packets; i++ {
        getAttr = append(getAttr, map[string]*dynamodb.AttributeValue{"id": {N: aws.String("1" + ioTEvent.Client + ioTEvent.Message + strconv.Itoa(i))}})
    }
    
    getInput := &dynamodb.BatchGetItemInput{
        RequestItems: map[string]*dynamodb.KeysAndAttributes{
            "Webcam": {
                Keys: getAttr,
                ProjectionExpression: aws.String("packet, payload"),
            },
        },
    }
    
    result, err := db.BatchGetItem(getInput)
    if err != nil {
        errorLogger.Println(err.Error())
        return nil
    }
    
    payload := make(map[int]string)
    for _, element := range result.Responses["Webcam"] {
        dbItem := new(DbItem)
        err = dynamodbattribute.UnmarshalMap(element, dbItem)
        if err != nil {
            errorLogger.Println(err.Error())
            return nil
        }
        payload[dbItem.Packet] = dbItem.Payload
    }
    /////////////////////////////////////////////////////////////////////////////
    
    //////////////////////////////////// delete packets //////////////////////////
    delAttr :=[]*dynamodb.WriteRequest{}
    for i := 0; i < packets; i++ {
        delAttr = append(delAttr, &dynamodb.WriteRequest{DeleteRequest: 
            &dynamodb.DeleteRequest{Key: map[string]*dynamodb.AttributeValue{"id": {N: aws.String("1" + ioTEvent.Client + ioTEvent.Message + strconv.Itoa(i))}}}});
    }
    
    deleteInput := &dynamodb.BatchWriteItemInput{
        RequestItems: map[string][]*dynamodb.WriteRequest{
            "Webcam": delAttr,
        },
    }
    
    _, err = db.BatchWriteItem(deleteInput)
        if err != nil {
        errorLogger.Println(err.Error())
        return nil
    }
    /////////////////////////////////////////////////////////////////////////////
    
    return payload
}
