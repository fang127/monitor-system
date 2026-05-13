.DEFAULT_GOAL := build
GO ?= go
IMAGE_TAG ?= $(shell git rev-parse --short HEAD)$(if $(shell git status --porcelain 2>/dev/null),.dirty,)

.PHONY: build
build:
	@$(GO) build -o bin/agent_service ./main.go

.PHONY: test
test:
	@$(GO) test ./...

.PHONY: image
image:
	@docker build -f manifest/docker/Dockerfile -t $(DOCKER_NAME):$(if $(TAG),$(TAG),$(IMAGE_TAG)) .

.PHONY: image.push
image.push: image
	@docker push $(DOCKER_NAME):$(if $(TAG),$(TAG),$(IMAGE_TAG))
