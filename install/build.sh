#!/bin/sh
GOOS=windows GOARCH=amd64 go build -ldflags="-s -w"
