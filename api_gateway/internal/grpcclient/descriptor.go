package grpcclient

import (
	"fmt"

	"google.golang.org/protobuf/reflect/protoreflect"

	"monitor-system/api_gateway/internal/pb/queryapi"
)

// queryAPIFileDescriptor 返回生成的查询 API 文件描述符。
func queryAPIFileDescriptor() (protoreflect.FileDescriptor, error) {
	return queryapi.File_query_api_proto, nil
}

// messageDescriptor 根据消息名称查询并返回对应的消息描述符，如果未找到则返回错误。
func messageDescriptor(name protoreflect.Name) (protoreflect.MessageDescriptor, error) {
	file, err := queryAPIFileDescriptor()
	if err != nil {
		return nil, err
	}
	desc := file.Messages().ByName(name)
	if desc == nil {
		return nil, fmt.Errorf("query api message descriptor not found: %s", name)
	}
	return desc, nil
}
