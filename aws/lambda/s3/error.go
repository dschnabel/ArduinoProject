package s3

import (
    "log"
    "os"
)

var ErrorLogger = log.New(os.Stderr, "ERROR ", log.Llongfile)
