#!/bin/sh
go build -ldflags="-s -w" && mv generate-vcxproj.exe ..