package main

import (
    "encoding/json"
    "log"
    "net/http"
    "os"

    "github.com/aws/aws-lambda-go/events"
    "github.com/aws/aws-lambda-go/lambda"
)

var errorLogger = log.New(os.Stderr, "ERROR ", log.Llongfile)

type iotMessage struct {
    Client    int    `json:"c"`
    MessageNr int    `json:"m"`
    PacketNr  int    `json:"p"`
    Type      int    `json:"t"`
    Payload   string `json:"d"`
}

func router(req events.APIGatewayProxyRequest) (events.APIGatewayProxyResponse, error) {
    switch req.HTTPMethod {
    case "POST":
        return create(req)
    default:
        return clientError(http.StatusMethodNotAllowed, nil)
    }
}

func create(req events.APIGatewayProxyRequest) (events.APIGatewayProxyResponse, error) {
    if req.Headers["Content-Type"] != "application/json" && req.Headers["content-type"] != "application/json" {
        return clientError(http.StatusNotAcceptable, nil)
    }

    m := &iotMessage{PacketNr:0, Payload:"-"}
    err := json.Unmarshal([]byte(req.Body), m)
    if err != nil {
        return clientError(http.StatusUnprocessableEntity, err)
    }

    err = putItem(m)
    if err != nil {
        return serverError(err)
    }

    return events.APIGatewayProxyResponse{
        StatusCode: 201,
    }, nil
}

func serverError(err error) (events.APIGatewayProxyResponse, error) {
    errorLogger.Println(err.Error())

    return events.APIGatewayProxyResponse{
        StatusCode: http.StatusInternalServerError,
        Body:       http.StatusText(http.StatusInternalServerError),
    }, nil
}

func clientError(status int, err error) (events.APIGatewayProxyResponse, error) {
    errorLogger.Println(err.Error())
    
    return events.APIGatewayProxyResponse{
        StatusCode: status,
        Body:       http.StatusText(status),
    }, nil
}

func main() {
    lambda.Start(router)
}
