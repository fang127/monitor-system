.PHONY: run test fmt proto proto-tools

PROTO_DIR ?= ../proto
PROTO_OUT ?= ./internal/pb/queryapi
BIN_DIR ?= $(CURDIR)/.bin
PROTOC_GEN_GO ?= $(BIN_DIR)/protoc-gen-go
PROTOC_GEN_GO_GRPC ?= $(BIN_DIR)/protoc-gen-go-grpc
PROTOC_GEN_GO_VERSION ?= google.golang.org/protobuf/cmd/protoc-gen-go@v1.34.1
PROTOC_GEN_GO_GRPC_VERSION ?= google.golang.org/grpc/cmd/protoc-gen-go-grpc@v1.4.0

run:
	go run ./cmd/server

test:
	go test ./...

fmt:
	gofmt -w ./cmd ./internal

proto-tools: $(PROTOC_GEN_GO) $(PROTOC_GEN_GO_GRPC)

$(PROTOC_GEN_GO):
	mkdir -p $(BIN_DIR)
	GOBIN=$(BIN_DIR) go install $(PROTOC_GEN_GO_VERSION)

$(PROTOC_GEN_GO_GRPC):
	mkdir -p $(BIN_DIR)
	GOBIN=$(BIN_DIR) go install $(PROTOC_GEN_GO_GRPC_VERSION)

proto: proto-tools
	mkdir -p $(PROTO_OUT)
	PATH="$(BIN_DIR):$$PATH" protoc -I $(PROTO_DIR) --go_out=$(PROTO_OUT) --go_opt=paths=source_relative --go-grpc_out=$(PROTO_OUT) --go-grpc_opt=paths=source_relative query_api.proto
