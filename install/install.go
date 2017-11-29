package main

import (
	"archive/tar"
	"bytes"
	"errors"
	"io"
	"io/ioutil"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"xi2.org/x/xz"
)

func downloadTXZ(url string) error {
	log.Printf("downloading %v", url)
	resp, err := http.Get(url)
	if err != nil {
		log.Printf("http download failed %v", err)
		return err
	}

	defer resp.Body.Close()

	if resp.StatusCode != 200 {
		log.Printf("http download returned error %v", resp.Status)
		return errors.New("http error")
	}

	xzr, err := xz.NewReader(resp.Body, 0)
	if err != nil {
		log.Printf("invalid xz file %v", err)
		return err
	}

	tr := tar.NewReader(xzr)
	for {
		hdr, err := tr.Next()
		if err == io.EOF {
			break
		} else if err != nil {
			log.Printf("tar error %v", err)
			return err
		}

		switch hdr.Typeflag {
		case tar.TypeReg:
			log.Printf("unpacking %v", hdr.Name)

			dir := filepath.Dir(hdr.Name)
			os.MkdirAll(dir, os.ModePerm)

			f, err := os.Create(hdr.Name)
			if err != nil {
				log.Printf("failed to open file %v for write %v", hdr.Name, err)
				return err
			}

			_, err = io.Copy(f, tr)
			if err != nil {
				log.Printf("tar read failed %v", err)
				f.Close()
				return err
			}
			f.Close()
		case tar.TypeSymlink:
			log.Printf("unpacking %v", hdr.Name)

			dir := filepath.Dir(hdr.Name)
			os.MkdirAll(dir, os.ModePerm)

			log.Printf("creating symlink from %v to %v", hdr.Linkname, hdr.Name)
			err = os.Symlink(hdr.Linkname, hdr.Name)
			if err != nil {
				log.Printf("symlink create failed %v", err)
				return err
			}
		}
	}

	return nil
}

func checkfile(name string, contents string) bool {
	data, err := ioutil.ReadFile(name)
	if err != nil {
		return false
	}
	return contents == string(bytes.TrimSpace(data))
}

func main() {
	log.SetFlags(log.Lmicroseconds | log.Ltime)
	log.Printf("installing toolchain")

	exe, err := os.Executable()
	if err != nil {
		log.Fatalf("failed to get exe filename %v", err)
	}
	dir := filepath.Dir(exe)

	log.Printf("installing to %v", exe, dir)
	os.Chdir(dir)

	log.Printf("checking host toolchain")
	if !checkfile("host/build-date.txt", hostTimeStamp) {
		log.Printf("removing old host directory")
		os.RemoveAll("host")

		if downloadTXZ(hostURL) != nil {
			os.RemoveAll("host")
			os.Exit(2)
		}
	}

	log.Printf("checking lib/arm")
	if !checkfile("lib/arm/build-date.txt", "2017-09-15T18:29:10+00:00") {
		log.Printf("removing old lib/arm directory")
		os.RemoveAll("lib/arm")

		if downloadTXZ("https://storage.googleapis.com/ctct-clang-toolchain/libarm-2017-09-15-1.txz") != nil {
			os.RemoveAll("lib/arm")
			os.Exit(2)
		}
	}
}
