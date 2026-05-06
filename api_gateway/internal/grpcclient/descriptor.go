package grpcclient

import (
	"fmt"
	"sync"

	"google.golang.org/protobuf/reflect/protodesc"
	"google.golang.org/protobuf/reflect/protoreflect"
	"google.golang.org/protobuf/reflect/protoregistry"
	"google.golang.org/protobuf/types/descriptorpb"
)

var (
	queryFileOnce sync.Once                   // 确保只构建一次文件描述符
	queryFileDesc protoreflect.FileDescriptor // 构建后的文件描述符
	queryFileErr  error                       // 构建过程中可能发生的错误
)

// queryAPIFileDescriptor 构建并返回查询API的文件描述符，使用sync.Once确保只构建一次。
func queryAPIFileDescriptor() (protoreflect.FileDescriptor, error) {
	queryFileOnce.Do(func() {
		// 构建查询API的文件描述符，并将其注册到全局文件注册表中，以便后续查询使用。
		queryFileDesc, queryFileErr = protodesc.NewFile(buildQueryAPIFile(), protoregistry.GlobalFiles)
	})
	return queryFileDesc, queryFileErr
}

// messageDescriptor 根据消息名称查询并返回对应的消息描述符，如果未找到则返回错误。
func messageDescriptor(name protoreflect.Name) (protoreflect.MessageDescriptor, error) {
	// 查询全局文件注册表中是否已经存在该消息的描述符，如果存在则直接返回，不存在则构建查询API的文件描述符并从中查找。
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

// buildQueryAPIFile 构建查询API的文件描述符，定义了消息、枚举和服务的结构。
func buildQueryAPIFile() *descriptorpb.FileDescriptorProto {
	return &descriptorpb.FileDescriptorProto{
		Name:       stringPtr("query_api.proto"),
		Package:    stringPtr("monitor.proto"),
		Syntax:     stringPtr("proto3"),
		Dependency: []string{"google/protobuf/timestamp.proto"},
		Options: &descriptorpb.FileOptions{
			GoPackage: stringPtr("monitor-system/api_gateway/internal/pb/queryapi;queryapi"),
		},
		EnumType: []*descriptorpb.EnumDescriptorProto{
			enum("SortOrder", "DESC", "ASC"),
			enum("ServerStatus", "ONLINE", "OFFLINE"),
		},
		MessageType: []*descriptorpb.DescriptorProto{
			message("TimeRange",
				msgField("start_time", 1, ".google.protobuf.Timestamp"),
				msgField("end_time", 2, ".google.protobuf.Timestamp"),
			),
			message("Pagination",
				int32Field("page", 1),
				int32Field("page_size", 2),
			),
			message("QueryTrendRequest",
				stringField("server_name", 1),
				msgField("time_range", 2, ".monitor.proto.TimeRange"),
				int32Field("interval_seconds", 3),
			),
			message("QueryAnomalyRequest",
				stringField("server_name", 1),
				msgField("time_range", 2, ".monitor.proto.TimeRange"),
				floatField("cpu_threshold", 3),
				floatField("mem_threshold", 4),
				floatField("disk_threshold", 5),
				floatField("change_rate_threshold", 6),
				msgField("pagination", 7, ".monitor.proto.Pagination"),
			),
			message("QueryLatestScoreRequest"),
			message("PerformanceRecord",
				stringField("server_name", 1),
				msgField("timestamp", 2, ".google.protobuf.Timestamp"),
				floatField("cpu_percent", 10),
				floatField("usr_percent", 11),
				floatField("system_percent", 12),
				floatField("nice_percent", 13),
				floatField("idle_percent", 14),
				floatField("io_wait_percent", 15),
				floatField("irq_percent", 16),
				floatField("soft_irq_percent", 17),
				floatField("load_avg_1", 20),
				floatField("load_avg_3", 21),
				floatField("load_avg_15", 22),
				floatField("mem_used_percent", 30),
				floatField("mem_total", 31),
				floatField("mem_free", 32),
				floatField("mem_avail", 33),
				floatField("disk_util_percent", 40),
				floatField("send_rate", 50),
				floatField("rcv_rate", 51),
				floatField("score", 60),
				floatField("cpu_percent_rate", 70),
				floatField("usr_percent_rate", 71),
				floatField("system_percent_rate", 72),
				floatField("io_wait_percent_rate", 73),
				floatField("load_avg_1_rate", 74),
				floatField("load_avg_3_rate", 75),
				floatField("load_avg_15_rate", 76),
				floatField("mem_used_percent_rate", 77),
				floatField("disk_util_percent_rate", 78),
				floatField("send_rate_rate", 79),
				floatField("rcv_rate_rate", 80),
			),
			message("AnomalyRecord",
				stringField("server_name", 1),
				msgField("timestamp", 2, ".google.protobuf.Timestamp"),
				stringField("anomaly_type", 3),
				stringField("severity", 4),
				floatField("value", 5),
				floatField("threshold", 6),
				stringField("metric_name", 7),
			),
			message("ServerScoreSummary",
				stringField("server_name", 1),
				floatField("score", 2),
				msgField("last_update", 3, ".google.protobuf.Timestamp"),
				enumField("status", 4, ".monitor.proto.ServerStatus"),
				floatField("cpu_percent", 10),
				floatField("mem_used_percent", 11),
				floatField("disk_util_percent", 12),
				floatField("load_avg_1", 13),
			),
			message("ClusterStats",
				int32Field("total_servers", 1),
				int32Field("online_servers", 2),
				int32Field("offline_servers", 3),
				floatField("avg_score", 4),
				floatField("max_score", 5),
				floatField("min_score", 6),
				stringField("best_server", 7),
				stringField("worst_server", 8),
			),
			message("QueryTrendResponse",
				repeatedMsgField("records", 1, ".monitor.proto.PerformanceRecord"),
				int32Field("interval_seconds", 2),
			),
			message("QueryAnomalyResponse",
				repeatedMsgField("anomalies", 1, ".monitor.proto.AnomalyRecord"),
				int32Field("total_count", 2),
				int32Field("page", 3),
				int32Field("page_size", 4),
			),
			message("QueryLatestScoreResponse",
				repeatedMsgField("servers", 1, ".monitor.proto.ServerScoreSummary"),
				msgField("cluster_stats", 2, ".monitor.proto.ClusterStats"),
			),
		},
		Service: []*descriptorpb.ServiceDescriptorProto{
			{
				Name: stringPtr("QueryService"),
				Method: []*descriptorpb.MethodDescriptorProto{
					method("QueryTrend", ".monitor.proto.QueryTrendRequest", ".monitor.proto.QueryTrendResponse"),
					method("QueryAnomaly", ".monitor.proto.QueryAnomalyRequest", ".monitor.proto.QueryAnomalyResponse"),
					method("QueryLatestScore", ".monitor.proto.QueryLatestScoreRequest", ".monitor.proto.QueryLatestScoreResponse"),
				},
			},
		},
	}
}

// 以下是一些辅助函数，用于简化枚举、消息和字段的创建。
func enum(name string, values ...string) *descriptorpb.EnumDescriptorProto {
	enumValues := make([]*descriptorpb.EnumValueDescriptorProto, 0, len(values))
	for i, value := range values {
		enumValues = append(enumValues, &descriptorpb.EnumValueDescriptorProto{
			Name:   stringPtr(value),
			Number: int32Ptr(int32(i)),
		})
	}
	return &descriptorpb.EnumDescriptorProto{Name: stringPtr(name), Value: enumValues}
}

func message(name string, fields ...*descriptorpb.FieldDescriptorProto) *descriptorpb.DescriptorProto {
	return &descriptorpb.DescriptorProto{Name: stringPtr(name), Field: fields}
}

func method(name, input, output string) *descriptorpb.MethodDescriptorProto {
	return &descriptorpb.MethodDescriptorProto{
		Name:       stringPtr(name),
		InputType:  stringPtr(input),
		OutputType: stringPtr(output),
	}
}

func stringField(name string, number int32) *descriptorpb.FieldDescriptorProto {
	return field(name, number, descriptorpb.FieldDescriptorProto_TYPE_STRING, "")
}

func int32Field(name string, number int32) *descriptorpb.FieldDescriptorProto {
	return field(name, number, descriptorpb.FieldDescriptorProto_TYPE_INT32, "")
}

func floatField(name string, number int32) *descriptorpb.FieldDescriptorProto {
	return field(name, number, descriptorpb.FieldDescriptorProto_TYPE_FLOAT, "")
}

func enumField(name string, number int32, typeName string) *descriptorpb.FieldDescriptorProto {
	return field(name, number, descriptorpb.FieldDescriptorProto_TYPE_ENUM, typeName)
}

func msgField(name string, number int32, typeName string) *descriptorpb.FieldDescriptorProto {
	return field(name, number, descriptorpb.FieldDescriptorProto_TYPE_MESSAGE, typeName)
}

func repeatedMsgField(name string, number int32, typeName string) *descriptorpb.FieldDescriptorProto {
	f := field(name, number, descriptorpb.FieldDescriptorProto_TYPE_MESSAGE, typeName)
	f.Label = descriptorpb.FieldDescriptorProto_LABEL_REPEATED.Enum()
	return f
}

func field(name string, number int32, fieldType descriptorpb.FieldDescriptorProto_Type, typeName string) *descriptorpb.FieldDescriptorProto {
	f := &descriptorpb.FieldDescriptorProto{
		Name:   stringPtr(name),
		Number: int32Ptr(number),
		Label:  descriptorpb.FieldDescriptorProto_LABEL_OPTIONAL.Enum(),
		Type:   fieldType.Enum(),
	}
	if typeName != "" {
		f.TypeName = stringPtr(typeName)
	}
	return f
}

func stringPtr(value string) *string {
	return &value
}

func int32Ptr(value int32) *int32 {
	return &value
}
