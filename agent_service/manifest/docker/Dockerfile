FROM golang:1.24-alpine AS builder

WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN go build -o /out/agent_service ./main.go

FROM alpine:3.20

WORKDIR /app
COPY --from=builder /out/agent_service /app/agent_service
COPY manifest/config/config.yaml /app/manifest/config/config.yaml
COPY docs /app/docs

ENV AGENT_DOCS_DIR=/app/docs
EXPOSE 6872

CMD ["/app/agent_service"]
