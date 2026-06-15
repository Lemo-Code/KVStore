package codec

import "encoding/json"

func init() {
	// 注册 JSON 编解码器到全局注册表
	Register(&JsonCodec{})
}

// JsonCodec 使用标准库 encoding/json 实现的 Codec
type JsonCodec struct{}

func (c *JsonCodec) Name() string { return "json" }

func (c *JsonCodec) Marshal(v any) ([]byte, error) {
	return json.Marshal(v)
}

func (c *JsonCodec) Unmarshal(data []byte, v any) error {
	return json.Unmarshal(data, v)
}

// registry 全局编解码器注册表
var registry = make(map[string]Codec)

// Register 注册编解码器
// 通常在 init() 中调用
func Register(c Codec) {
	registry[c.Name()] = c
}

// Get 按名称获取编解码器
// 找不到返回 nil
func Get(name string) Codec {
	return registry[name]
}

// DefaultCodec 默认编解码器名称
const DefaultCodec = "json"
