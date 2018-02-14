#!/bin/sh
GOOS=windows GOARCH=386 go build -ldflags="-s -w"
