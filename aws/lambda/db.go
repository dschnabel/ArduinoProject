package main

import (
    "strconv"
    "time"
    
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
            "category": {
                N: aws.String(ioTEvent.Category),
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
    
    tries := 0
    var payload map[int]string
    for {
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
        
        payload = make(map[int]string)
        for _, element := range result.Responses["Webcam"] {
            dbItem := new(DbItem)
            err = dynamodbattribute.UnmarshalMap(element, dbItem)
            if err != nil {
                errorLogger.Println(err.Error())
                return nil
            }
            payload[dbItem.Packet] = dbItem.Payload
        }
        
        if (len(payload) == packets || tries >= 10) {
            break
        }
        
        tries++
        errorLogger.Println("Waiting for packet(s) to arrive!")
        time.Sleep(500 * time.Millisecond)
    }
    if (tries >= 10) {
        errorLogger.Println("One or more packets did not arrive!")
        return nil
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

func dbGetNotification(client string) (int, string) {
    ////////////////// get item //////////////////////////////////////
    getInput := &dynamodb.BatchGetItemInput{
        RequestItems: map[string]*dynamodb.KeysAndAttributes{
            "Webcam": {
                Keys: []map[string]*dynamodb.AttributeValue{{"id": {N: aws.String(client)}}},
                ProjectionExpression: aws.String("category, payload"),
            },
        },
    }
    
    result, err := db.BatchGetItem(getInput)
    if err != nil {
        errorLogger.Println(err.Error())
        return 0, ""
    }
    if (len(result.Responses["Webcam"]) != 1) {
        return 0, ""
    }
    
    dbItem := new(DbItem)
    err = dynamodbattribute.UnmarshalMap(result.Responses["Webcam"][0], dbItem)
    if err != nil {
        errorLogger.Println(err.Error())
        return 0, ""
    }
    /////////////////////////////////////////////////////////////////////
    
    ////////////////// delete item //////////////////////////////////////
    deleteInput := &dynamodb.BatchWriteItemInput{
        RequestItems: map[string][]*dynamodb.WriteRequest{
            "Webcam": []*dynamodb.WriteRequest{
                &dynamodb.WriteRequest{
                    DeleteRequest: &dynamodb.DeleteRequest{Key: map[string]*dynamodb.AttributeValue{"id": {N: aws.String(client)}}},
                },
            },
        },
    }
    
    _, err = db.BatchWriteItem(deleteInput)
        if err != nil {
        errorLogger.Println(err.Error())
        return 0, ""
    }
    /////////////////////////////////////////////////////////////////////
    
    return dbItem.Category, dbItem.Payload
}
