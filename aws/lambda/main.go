package main

import (
    "context"
    "log"
    "os"
    "time"
    "bytes"

    "github.com/aws/aws-lambda-go/lambda"
    "github.com/aws/aws-sdk-go/aws/session"
    "github.com/aws/aws-sdk-go/service/s3"
    "github.com/aws/aws-sdk-go/aws"
    "github.com/aws/aws-sdk-go/aws/awserr"
    "github.com/aws/aws-sdk-go/aws/request"
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

func router(ctx context.Context, event IoTEvent) {
    if event.Type == "2" {
        data, err := assemblePhotoData(&event)
        if err != nil {
            errorLogger.Println("Problem with fetching data")
            errorLogger.Println(err)
        } else {
            saveToS3("abc.txt", data)
        }
    } else {
        err := dbPutPacket(&event)
        if err != nil {
            errorLogger.Println(err.Error())
        }
    }
}

func assemblePhotoData(event *IoTEvent) ([]byte, error) {
    payload, err := dbGetMessage(event)
    if err != nil {
        return nil, err
    }
    var data string
    for i := 0; i < len(payload); i++ {
        data += payload[i] + "\n===\n"
    }
    
    return []byte(data), nil
}

func saveToS3(filename string, data []byte) error {
    sess := session.Must(session.NewSession())
    svc := s3.New(sess)
    
    ctx := context.Background()
    var cancelFn func()
    
    timeout, _ := time.ParseDuration("1m")
    if timeout > 0 {
        ctx, cancelFn = context.WithTimeout(ctx, timeout)
    }
    defer cancelFn()
    
    _, err := svc.PutObjectWithContext(ctx, &s3.PutObjectInput{
        Bucket: aws.String("remote-eye"),
        Key:    aws.String(filename),
        Body:   bytes.NewReader(data),
    })
    
    if err != nil {
        if aerr, ok := err.(awserr.Error); ok && aerr.Code() == request.CanceledErrorCode {
            errorLogger.Println("upload canceled due to timeout")
            errorLogger.Println(err)
        } else {
            errorLogger.Println("failed to upload object")
            errorLogger.Println(err)
        }
        
        return err
    }
    
    return nil
}

func main() {
    lambda.Start(router)
}
