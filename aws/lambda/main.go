package main

import (
    "context"
    "log"
    "os"

    "github.com/aws/aws-lambda-go/lambda"
)

var errorLogger = log.New(os.Stderr, "ERROR ", log.Llongfile)

type IoTEvent struct {
        Client   string `json:"client"`
        Message  string `json:"message"`
        Type     string `json:"type"`
        Packet   string `json:"packet"`
        Payload  string `json:"payload"`
}

type DbItem struct {
        Packet  int    `json:"packet"`
        Payload string `json:"payload"`
}

func main() {
    lambda.Start(router)
}

func router(ctx context.Context, event IoTEvent) {
    if event.Type == "1" {
        dbPutPacket(&event)
    } else if event.Type == "2" {
        processPhotoData(&event)
    } else {
        errorLogger.Println("Unknown type: " + event.Type)
    }
}

func processPhotoData(event *IoTEvent) {
    data := assemblePhotoData(event)
    if data != nil {
        s3SaveFile("abc.txt", data)
    }
}

func assemblePhotoData(event *IoTEvent) ([]byte) {
    payload := dbGetMessage(event)
    if payload == nil {
        return nil
    }
    
    var data string
    for i := 0; i < len(payload); i++ {
        data += payload[i] + "\n===\n"
    }
    
    return []byte(data)
}
