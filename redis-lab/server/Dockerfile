FROM golang:1.23-alpine AS builder

WORKDIR /app
COPY go.mod go.sum ./
RUN go mod download
COPY . .
RUN CGO_ENABLED=1 go build -o /redis-lab-server .

FROM alpine:latest
RUN apk --no-cache add ca-certificates
COPY --from=builder /redis-lab-server /redis-lab-server
EXPOSE 8080
CMD ["/redis-lab-server"]
