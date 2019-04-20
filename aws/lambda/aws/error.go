package aws

import (
    "log"
    "os"
)

var ErrorLogger = log.New(os.Stderr, "ERROR ", log.Llongfile)
