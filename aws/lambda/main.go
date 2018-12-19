package main

import (
    "context"
    "log"
    "os"
    "time"
    "encoding/base64"

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
    if data != nil && len(data) > 0 {
        decoded, err := base64.StdEncoding.DecodeString(event.Payload)
        var filename string
        if err != nil {
            errorLogger.Println("Filename decode error: " + err.Error())
            filename = time.Now().Format("2006-01-02-15.04.05") + "_err.jpg"
        }
        filename = string(decoded) + ".jpg"
        
        s3SaveFile(filename, data)
    }
}

func assemblePhotoData(event *IoTEvent) ([]byte) {
    payload := dbGetMessage(event)
    if payload == nil {
        return nil
    }
    
    var data []byte
    for i := 0; i < len(payload); i++ {
        decoded, err := base64.StdEncoding.DecodeString(payload[i])
        if err != nil {
            errorLogger.Println("Data decode error: " + err.Error())
            break
        }
        data = append(data, decoded...)
    }
    
    return []byte(data)
}
