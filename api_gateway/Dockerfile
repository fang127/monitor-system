FROM golang:1.22-alpine AS builder

WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN go build -o /out/api_gateway ./cmd/server

FROM alpine:3.20

WORKDIR /app
COPY --from=builder /out/api_gateway /app/api_gateway

EXPOSE 8080
CMD ["/app/api_gateway"]
