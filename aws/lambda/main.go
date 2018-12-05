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
        Packet   string `json:"packet"`
        Type     string `json:"type"`
        Payload  string `json:"payload"`
}

func router(ctx context.Context, event IoTEvent) {
    err := putItem(&event)
    if err != nil {
        errorLogger.Println(err.Error())
    }
}

func main() {
    lambda.Start(router)
}
