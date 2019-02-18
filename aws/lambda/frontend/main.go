package main

import (
    "encoding/base64"
    "encoding/gob"
    "encoding/json"
    "bytes"
    "net/http"

    "github.com/aws/aws-lambda-go/events"
    "github.com/aws/aws-lambda-go/lambda"
    "../s3"
)

func main() {
    lambda.Start(router)
}

func router(req events.APIGatewayProxyRequest) (events.APIGatewayProxyResponse, error) {
    switch req.HTTPMethod {
    case "GET":
        return show(req)
    //case "POST":
        //return create(req)
    default:
        return clientError(http.StatusMethodNotAllowed)
    }
}

func serverError(err error) (events.APIGatewayProxyResponse, error) {
    s3.ErrorLogger.Println(err.Error())

    return events.APIGatewayProxyResponse{
        StatusCode: http.StatusInternalServerError,
        Body:       http.StatusText(http.StatusInternalServerError),
    }, nil
}

func clientError(status int) (events.APIGatewayProxyResponse, error) {
    return events.APIGatewayProxyResponse{
        StatusCode: status,
        Body:       http.StatusText(status),
    }, nil
}

func show(req events.APIGatewayProxyRequest) (events.APIGatewayProxyResponse, error) {
    var config s3.Configuration
    
    configEnc := s3.DbGetConfig("0")
    if (configEnc != "") {
        decoded, err := base64.StdEncoding.DecodeString(configEnc)
        if err != nil {
            s3.ErrorLogger.Println(err.Error())
        }
        
        buffer := bytes.NewBuffer(decoded)
        dec := gob.NewDecoder(buffer)
        
        err = dec.Decode(&config)
        if err != nil {
            s3.ErrorLogger.Println(err.Error())
        }
    }
    
    type timelineItem struct {
        Start int           `json:"start"`
        Title string        `json:"title"`
        ClassName string    `json:"className"`
    }
    var timeline []timelineItem

    for _, ts := range config.SnapshotTimestamps {
        timeline = append(timeline, timelineItem{Start: ts * 1000})
    }

    json, _ := json.Marshal(timeline)
    return events.APIGatewayProxyResponse{
        Headers:    map[string]string{"Access-Control-Allow-Origin": "*"},
        StatusCode: http.StatusOK,
        Body:       string(json),
    }, nil
}
