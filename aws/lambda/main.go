package main

import (
    "context"
    "log"
    "os"
    "time"
    "bytes"
    "strings"
    "strconv"
    "sort"
    "encoding/base64"
    "encoding/binary"
    "encoding/gob"
    "math"

    "github.com/aws/aws-lambda-go/lambda"
)

var errorLogger = log.New(os.Stderr, "ERROR ", log.Llongfile)

const MSG_TYPE_CLIENT_SUBSCRIBED = "0"
const MSG_TYPE_PHOTO_DATA = "1"
const MSG_TYPE_PHOTO_DONE = "2"
const MSG_TYPE_NEW_CONFIG = "3"
const MSG_TYPE_VOLTAGE = "4"

type IoTEvent struct {
    Client   string `json:"client"`
    Message  string `json:"message"`
    Category string `json:"category"`
    Packet   string `json:"packet"`
    Payload  string `json:"payload"`
}

type Configuration struct {
    SnapshotTimestamps  []int
}

func main() {
    lambda.Start(router)
}

func router(ctx context.Context, event IoTEvent) {
    if event.Category == MSG_TYPE_CLIENT_SUBSCRIBED {
        clientSubscribed(event.Payload)
    } else if event.Category == MSG_TYPE_PHOTO_DATA {
        dbPutPhotoData(&event)
    } else if event.Category == MSG_TYPE_PHOTO_DONE {
        processPhotoData(&event)
    } else if event.Category == MSG_TYPE_NEW_CONFIG {
        processNewConfiguration(&event)
    } else if event.Category == MSG_TYPE_VOLTAGE {
        addVoltage(&event)
    } else {
        errorLogger.Println("Unknown category: " + event.Category)
    }
}

func clientSubscribed(payload string) {
    direction := strings.Split(payload, "/")[0]
    client := strings.Split(payload, "/")[1]
    
    _, err := strconv.Atoi(client)
    if direction != "c" || err != nil {
        return
    }
    
    config := getConfiguration(client)
    
    if len(config.SnapshotTimestamps) == 0 {
        dbDelConfig(client)
        iotPushUpdate("c/" + client, []byte{0})
        return
    }
    
    var buffer bytes.Buffer
    // write next timestamp only and push to client
    binary.Write(&buffer, binary.LittleEndian, int32(config.SnapshotTimestamps[0]))
    
    iotPushUpdate("c/" + client, buffer.Bytes())
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
    payload := dbGetPhotoData(event)
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

func processNewConfiguration(event *IoTEvent) {
    decoded, err := base64.StdEncoding.DecodeString(event.Payload)
    if err != nil {
        errorLogger.Println(err.Error())
        return
    }
    
    config := getConfiguration(event.Client)
    
    timestamps := strings.Split(string(decoded), ",")
    for _, timestamp := range timestamps {
        ts, err := strconv.Atoi(timestamp)
        if err != nil {
            errorLogger.Println(err.Error())
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
        dbDelConfig(event.Client)
    } else {
        updateConfiguration(event.Client, config)
    }
}

func getConfiguration(client string) *Configuration {
    var config Configuration
    
    notification := dbGetConfig(client)
    if (notification != "") {
        decoded, err := base64.StdEncoding.DecodeString(notification)
        if err != nil {
            errorLogger.Println(err.Error())
        }
        
        buffer := bytes.NewBuffer(decoded)
        dec := gob.NewDecoder(buffer)
        
        err = dec.Decode(&config)
        if err != nil {
            errorLogger.Println(err.Error())
        }
    }
    
    if deleteOldTimestamps(&config) || deleteDuplicateTimestamps(&config) {
        updateConfiguration(client, &config)
    }
    
    sort.Ints(config.SnapshotTimestamps)
    return &config
}

func updateConfiguration(client string, config *Configuration) {
    var buffer bytes.Buffer
    enc := gob.NewEncoder(&buffer)
    err := enc.Encode(config)
    if err != nil {
        errorLogger.Println(err.Error())
        return
    }
    
    encoded := base64.StdEncoding.EncodeToString([]byte(buffer.String()))
    dbAddOrUpdateConfig(client, encoded)
}

func deleteOldTimestamps(config *Configuration) bool {
    updated := false
    now := time.Now().Unix()
    ts := config.SnapshotTimestamps
    
    i := 0
    for _, x := range ts {
        if x > int(now) {
            ts[i] = x
            i++
        } else {
            updated = true
        }
    }
    
    if updated {
        config.SnapshotTimestamps = ts[:i]
    }
    
    return updated
}

func deleteDuplicateTimestamps(config *Configuration) bool {
    updated := false
    m := make(map[int]bool)
    
    for _, x := range config.SnapshotTimestamps {
        if _, ok := m[x]; ok {
            updated = true
        } else {
            m[x] = true
        }
    }
    
    var ts []int
    for x, _ := range m {
        ts = append(ts, x)
    }
    
    config.SnapshotTimestamps = ts
    
    return updated
}

func addVoltage(event *IoTEvent) {
    decoded, err := base64.StdEncoding.DecodeString(event.Payload)
    if err != nil {
        errorLogger.Println(err.Error())
        return
    }
    
    bits := binary.LittleEndian.Uint32(decoded)
    voltage := math.Float32frombits(bits)
    
    dbPutVoltage(event.Client, voltage)
}
