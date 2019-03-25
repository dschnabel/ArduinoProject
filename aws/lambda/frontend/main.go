package main

import (
    "encoding/json"
    "net/http"
    "strings"
    "time"
    "strconv"

    "github.com/aws/aws-lambda-go/events"
    "github.com/aws/aws-lambda-go/lambda"
    "../s3"
    "../common"
)

func main() {
    lambda.Start(router)
}

func router(req events.APIGatewayProxyRequest) (events.APIGatewayProxyResponse, error) {
    switch req.HTTPMethod {
    case "GET":
        return show(req)
    case "POST":
        return create(req)
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
    type timelineItem struct {
        Start int           `json:"start"`
        Link  string        `json:"link"`
    }
    var timeline []timelineItem

    config := common.GetConfiguration("0")

    for _, ts := range config.SnapshotTimestamps {
        timeline = append(timeline, timelineItem{Start: ts * 1000})
    }

    // thumbnails
    s3Objects := s3.S3GetThumbnails("0")
    for _, obj := range s3Objects {
        if !strings.HasSuffix(*obj.Key, ".jpg") {
            continue
        }

        date := strings.Split(*obj.Key, "/")[2]
        date = date[:strings.LastIndex(date, ".")]
        t, err := time.Parse("2006-01-02T15:04:05MST-0700", date)

        if err != nil {
            s3.ErrorLogger.Println(err.Error())
            continue
        }
        
        timeline = append(timeline, timelineItem{Start: int(t.Unix() * 1000), Link: *obj.Key})
    }

    json, _ := json.Marshal(timeline)
    
    return events.APIGatewayProxyResponse{
        Headers:    map[string]string{"Access-Control-Allow-Origin": "*"},
        StatusCode: http.StatusOK,
        Body:       string(json),
    }, nil
}

func create(req events.APIGatewayProxyRequest) (events.APIGatewayProxyResponse, error) {   
    var config s3.Configuration
    
    timestamps := strings.Split(string(req.Body), ",")
    for _, timestamp := range timestamps {
        ts, err := strconv.Atoi(timestamp)
        if err != nil {
            s3.ErrorLogger.Println(err.Error())
        } else {
            config.SnapshotTimestamps = append(config.SnapshotTimestamps, ts)
        }
    }
    
    if len(config.SnapshotTimestamps) == 0 {
        s3.DbDelConfig("0")
    } else {
        common.UpdateConfiguration("0", &config)
    }
    
    return events.APIGatewayProxyResponse{
        Headers:    map[string]string{"Access-Control-Allow-Origin": "*"},
        StatusCode: 201,
        Body:       "{\"status\": \"OK\"}",
    }, nil
}
