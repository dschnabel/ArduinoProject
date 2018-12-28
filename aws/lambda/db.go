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

type DbItem struct {
        Packet   int    `json:"packet"`
        Payload  string `json:"payload"`
}

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

func dbAddOrUpdateNotification(client string, payload string) {
    input := &dynamodb.UpdateItemInput{
        TableName: aws.String("Webcam"),
        ExpressionAttributeValues: map[string]*dynamodb.AttributeValue{":p": {S: aws.String(payload)}},
        Key: map[string]*dynamodb.AttributeValue{"id": {N: aws.String(client)}},
        ReturnValues: aws.String("ALL_NEW"),
        UpdateExpression: aws.String("SET payload = :p"),
    }
    
    _, err := db.UpdateItem(input)
    if err != nil {
        errorLogger.Println(err.Error())
    }
}

func dbGetNotification(client string) string {
    result, err := db.GetItem(&dynamodb.GetItemInput{
        TableName: aws.String("Webcam"),
        Key: map[string]*dynamodb.AttributeValue{"id": {N: aws.String(client)}},
        ProjectionExpression: aws.String("payload"),
    })
    
    if err != nil {
        errorLogger.Println(err.Error())
        return ""
    }
    
    dbItem := new(DbItem)
    err = dynamodbattribute.UnmarshalMap(result.Item, dbItem)
    if err != nil {
        errorLogger.Println(err.Error())
        return ""
    }
    
    return dbItem.Payload
}

func dbDelNotification(client string) {
    input := &dynamodb.DeleteItemInput{
        TableName: aws.String("Webcam"),
        Key: map[string]*dynamodb.AttributeValue{"id": {N: aws.String(client)}},
    }

    _, err := db.DeleteItem(input)
    if err != nil {
        errorLogger.Println(err.Error())
        return
    }
}