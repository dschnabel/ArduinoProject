package common

import (
    "encoding/base64"
    "encoding/gob"
    "bytes"
    "time"
    "sort"
    
    "../s3"
)

func GetConfiguration(client string) *s3.Configuration {
    var config s3.Configuration
    
    configEnc := s3.DbGetConfig(client)
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
    
    if deleteOldTimestamps(&config) || deleteDuplicateTimestamps(&config) {
        UpdateConfiguration(client, &config)
    }
    
    sort.Ints(config.SnapshotTimestamps)
    return &config
}

func UpdateConfiguration(client string, config *s3.Configuration) {
    var buffer bytes.Buffer
    enc := gob.NewEncoder(&buffer)
    err := enc.Encode(config)
    if err != nil {
        s3.ErrorLogger.Println(err.Error())
        return
    }
    
    encoded := base64.StdEncoding.EncodeToString([]byte(buffer.String()))
    s3.DbAddOrUpdateConfig(client, encoded)
}

func deleteOldTimestamps(config *s3.Configuration) bool {
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

func deleteDuplicateTimestamps(config *s3.Configuration) bool {
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
