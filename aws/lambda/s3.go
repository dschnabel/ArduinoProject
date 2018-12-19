package main

import (
    "context"
    "time"
    "bytes"
    
    "github.com/aws/aws-sdk-go/aws/session"
    "github.com/aws/aws-sdk-go/service/s3"
    "github.com/aws/aws-sdk-go/aws"
    "github.com/aws/aws-sdk-go/aws/awserr"
    "github.com/aws/aws-sdk-go/aws/request"
)

func s3SaveFile(filename string, data []byte) {
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
            errorLogger.Println("upload canceled due to timeout: " + err.Error())
        } else {
            errorLogger.Println("failed to upload object: " + err.Error())
        }
    }
}
