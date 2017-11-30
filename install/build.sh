#!/bin/sh
go build -ldflags="-s -w" && mv install.exe ..
