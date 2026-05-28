package dto

// 该文件定义了Chat相关的请求和响应结构体

// ChatReq 定义了聊天请求的结构体，包含用户ID和问题内容
type ChatReq struct {
	Id       string `json:"Id" form:"Id"`
	Question string `json:"Question" form:"Question"`
}

// ChatRes 定义了聊天响应的结构体，包含回答内容
type ChatRes struct {
	Answer string `json:"answer"`
}

// ChatStreamReq 定义了聊天流式请求的结构体，包含用户ID和问题内容
type ChatStreamReq struct {
	Id       string `json:"Id" form:"Id"`
	Question string `json:"Question" form:"Question"`
}

// ChatStreamRes 定义了聊天流式响应的结构体，目前为空，可以根据需要添加字段
type ChatStreamRes struct {
}

// FileUploadReq 定义了文件上传请求的结构体，目前为空，可以根据需要添加字段
type FileUploadReq struct {
}

// FileUploadRes 定义了文件上传响应的结构体，包含文件名、文件路径和文件大小
type FileUploadRes struct {
	FileName string `json:"fileName"`
	FilePath string `json:"filePath"`
	FileSize int64  `json:"fileSize"`
}

// AIOpsReq 定义了AIOps请求的结构体，目前为空，可以根据需要添加字段
type AIOpsReq struct {
}

// AIOpsRes 定义了AIOps响应的结构体，包含结果字符串和详细信息列表
type AIOpsRes struct {
	Result string   `json:"result"`
	Detail []string `json:"detail"`
}
