package codec

import (
	"fmt"

	"google.golang.org/protobuf/proto"
)

func init() {
	Register(&ProtoCodec{})
}

// ProtoCodec 使用 google.golang.org/protobuf 实现的 Codec
// 只接受 proto.Message 类型的参数
type ProtoCodec struct{}

func (c *ProtoCodec) Name() string { return "proto" }

func (c *ProtoCodec) Marshal(v any) ([]byte, error) {
	msg, ok := v.(proto.Message)
	if !ok {
		return nil, fmt.Errorf("codec/proto: value does not implement proto.Message: %T", v)
	}
	return proto.Marshal(msg)
}

func (c *ProtoCodec) Unmarshal(data []byte, v any) error {
	msg, ok := v.(proto.Message)
	if !ok {
		return fmt.Errorf("codec/proto: value does not implement proto.Message: %T", v)
	}
	return proto.Unmarshal(data, msg)
}
