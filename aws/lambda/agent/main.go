package main

import (
    "context"
    "time"
    "bytes"
    "strings"
    "strconv"
    "encoding/base64"
    "encoding/binary"
    "math"
    "image"
    "bufio"
    "image/jpeg"

    "github.com/aws/aws-lambda-go/lambda"
    "github.com/disintegration/imaging"
    "../s3"
    "../common"
)

const MSG_TYPE_CLIENT_SUBSCRIBED = "0"
const MSG_TYPE_PHOTO_DATA = "1"
const MSG_TYPE_PHOTO_DONE = "2"
const MSG_TYPE_NEW_CONFIG = "3"
const MSG_TYPE_VOLTAGE = "4"

func main() {
    lambda.Start(router)
}

func router(ctx context.Context, event s3.IoTEvent) {
    if event.Category == MSG_TYPE_CLIENT_SUBSCRIBED {
        clientSubscribed(event.Payload)
    } else if event.Category == MSG_TYPE_PHOTO_DATA {
        s3.DbPutPhotoData(&event)
    } else if event.Category == MSG_TYPE_PHOTO_DONE {
        processPhotoData(&event)
    } else if event.Category == MSG_TYPE_NEW_CONFIG {
        processNewConfiguration(&event)
    } else if event.Category == MSG_TYPE_VOLTAGE {
        addVoltage(&event)
    } else {
        s3.ErrorLogger.Println("Unknown category: " + event.Category)
    }
}

func clientSubscribed(payload string) {
    direction := strings.Split(payload, "/")[0]
    client := strings.Split(payload, "/")[1]
    
    _, err := strconv.Atoi(client)
    if direction != "c" || err != nil {
        return
    }
    
     config := common.GetConfiguration(client, true)
    
    if len(config.SnapshotTimestamps) == 0 {
        s3.DbDelConfig(client)
        s3.IotPushUpdate("c/" + client, []byte{0})
        return
    }
    
    var buffer bytes.Buffer
    // write next timestamp only and push to client
    binary.Write(&buffer, binary.LittleEndian, int32(config.SnapshotTimestamps[0]))
    
    s3.IotPushUpdate("c/" + client, buffer.Bytes())
}

func processPhotoData(event *s3.IoTEvent) {
    data := assemblePhotoData(event)
    if data != nil && len(data) > 0 {
        filename := event.Client + "/"
        
        decoded, err := base64.StdEncoding.DecodeString(event.Payload)
        if err != nil {
            s3.ErrorLogger.Println("Timestamp decode error: " + err.Error())
            filename += time.Now().Format("2006-01-02T15:04:05") + "GMT-0700_err.jpg"
        } else {        
            var unixTime uint32
            buf := bytes.NewBuffer(decoded)
            binary.Read(buf, binary.LittleEndian, &unixTime)
            filename += time.Unix(int64(unixTime), 0).Format("2006-01-02T15:04:05") + "GMT-0700.jpg"
        }
        
        s3.S3SaveFile(filename, data)
        
        // create thumbnail
        srcImg, _, err := image.Decode(bytes.NewReader(data))
        if err != nil {
            s3.ErrorLogger.Println("Image decode error: " + err.Error())
        }
        dstImg := imaging.Resize(srcImg, 45, 0, imaging.Lanczos)
        
        var b bytes.Buffer
        writer := bufio.NewWriter(&b)
        jpeg.Encode(writer, dstImg, &jpeg.Options{jpeg.DefaultQuality})
        writer.Flush()
        
        s3.S3SaveFile("thumbnails/" + filename, []byte(b.String()))
    }
}

func assemblePhotoData(event *s3.IoTEvent) ([]byte) {
    payload := s3.DbGetPhotoData(event)
    if payload == nil {
        return nil
    }
    
    var data []byte
    for i := 0; i < len(payload); i++ {
        decoded, err := base64.StdEncoding.DecodeString(payload[i])
        if err != nil {
            s3.ErrorLogger.Println("Data decode error: " + err.Error())
            break
        }
        data = append(data, decoded...)
    }
    
    return []byte(data)
}

func processNewConfiguration(event *s3.IoTEvent) {
    decoded, err := base64.StdEncoding.DecodeString(event.Payload)
    if err != nil {
        s3.ErrorLogger.Println(err.Error())
        return
    }
    
    config := common.GetConfiguration(event.Client, false)
    
    timestamps := strings.Split(string(decoded), ",")
    for _, timestamp := range timestamps {
        ts, err := strconv.Atoi(timestamp)
        if err != nil {
            s3.ErrorLogger.Println(err.Error())
        } else {
            if ts > 0 {
                // append new timestamp
                config.SnapshotTimestamps = append(config.SnapshotTimestamps, ts)
            } else {
                // find and delete timestamp
                ts *= -1
                for i, deleteTs := range config.SnapshotTimestamps {
                    if ts == deleteTs {
                        config.SnapshotTimestamps = append(config.SnapshotTimestamps[:i], config.SnapshotTimestamps[i+1:]...)
                        break
                    }
                }
            }
        }
    }
    
    if len(config.SnapshotTimestamps) == 0 {
        s3.DbDelConfig(event.Client)
    } else {
        common.UpdateConfiguration(event.Client, config)
    }
}

func addVoltage(event *s3.IoTEvent) {
    decoded, err := base64.StdEncoding.DecodeString(event.Payload)
    if err != nil {
        s3.ErrorLogger.Println(err.Error())
        return
    }
    
    bits := binary.LittleEndian.Uint32(decoded)
    voltage := math.Float32frombits(bits)
    
    s3.DbPutVoltage(event.Client, voltage)
}
