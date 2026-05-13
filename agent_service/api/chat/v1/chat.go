package v1

type ChatReq struct {
	Id       string `json:"Id" form:"Id"`
	Question string `json:"Question" form:"Question"`
}

type ChatRes struct {
	Answer string `json:"answer"`
}

type ChatStreamReq struct {
	Id       string `json:"Id" form:"Id"`
	Question string `json:"Question" form:"Question"`
}

type ChatStreamRes struct {
}

type FileUploadReq struct {
}

type FileUploadRes struct {
	FileName string `json:"fileName"`
	FilePath string `json:"filePath"`
	FileSize int64  `json:"fileSize"`
}

type AIOpsReq struct {
}

type AIOpsRes struct {
	Result string   `json:"result"`
	Detail []string `json:"detail"`
}
