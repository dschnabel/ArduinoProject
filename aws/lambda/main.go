package main

import (
    "context"
    "log"
    "os"
    "time"
    "bytes"
    "strings"
    "strconv"
    "encoding/base64"
    "encoding/binary"

    "github.com/aws/aws-lambda-go/lambda"
)

var errorLogger = log.New(os.Stderr, "ERROR ", log.Llongfile)

type IoTEvent struct {
        Client   string `json:"client"`
        Message  string `json:"message"`
        Category string `json:"category"`
        Packet   string `json:"packet"`
        Payload  string `json:"payload"`
}

type DbItem struct {
        Category int    `json:"category"`    
        Packet   int    `json:"packet"`
        Payload  string `json:"payload"`
}

func main() {
    lambda.Start(router)
}

func router(ctx context.Context, event IoTEvent) {
    if event.Category == "0" {
        clientSubscribed(event.Payload)
    } else if event.Category == "1" {
        dbPutPacket(&event)
    } else if event.Category == "2" {
        processPhotoData(&event)
    } else {
        errorLogger.Println("Unknown category: " + event.Category)
    }
}

func clientSubscribed(payload string) {
    client := strings.Split(payload, "/")[1]
    category, notification := dbGetNotification(client)
    
    decoded, err := base64.StdEncoding.DecodeString(notification)
    if err != nil {
        errorLogger.Println("Notification decode error: " + err.Error())
    } else {
        iotPushUpdate("c/" + client + "/" + strconv.Itoa(category), decoded)
    }
}

func processPhotoData(event *IoTEvent) {
    data := assemblePhotoData(event)
    if data != nil && len(data) > 0 {
        filename := event.Client + "/"
        
        decoded, err := base64.StdEncoding.DecodeString(event.Payload)
        if err != nil {
            errorLogger.Println("Timestamp decode error: " + err.Error())
            filename += time.Now().Format("2006-01-02-15.04.05") + "_err.jpg"
        } else {        
            var unixTime uint32
            buf := bytes.NewBuffer(decoded)
            binary.Read(buf, binary.LittleEndian, &unixTime)
            filename += time.Unix(int64(unixTime), 0).Format("2006-01-02-15.04.05") + ".jpg"
        }
        
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
