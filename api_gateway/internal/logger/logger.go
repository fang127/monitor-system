package logger

import (
	"log"
	"os"
)

func Init(mode string) {
	log.SetOutput(os.Stdout)
	// 包含日期、时间、微秒和文件名
	log.SetFlags(log.LstdFlags | log.Lmicroseconds | log.Lshortfile)
	log.Printf("logger initialized, mode=%s", mode)
}
