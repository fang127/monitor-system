package chat_pipeline

import (
	"context"

	"github.com/cloudwego/eino/compose"
	"github.com/cloudwego/eino/schema"
)

// 定义 ChatAgent 的 Eino Graph

// BuildChatAgent 构建一个聊天 Agent，包含以下步骤：
// 1. 将用户输入转换为 RAG（Retrieval-Augmented Generation）所需的格式。
// 2. 使用 Milvus Retriever 从向量数据库中检索相关文档。
// 3. 将用户输入和检索到的文档一起传递给 ChatTemplate，生成一个适合 ReAct Agent 的提示（prompt）。
// 4. 将生成的提示传递给 ReAct Agent，得到最终的回复。
func BuildChatAgent(ctx context.Context) (r compose.Runnable[*UserMessage, *schema.Message], err error) {
	const (
		InputToRag      = "InputToRag"
		ChatTemplate    = "ChatTemplate"
		ReactAgent      = "ReactAgent"
		MilvusRetriever = "MilvusRetriever"
		InputToChat     = "InputToChat"
	)
	// 1. 创建一个新的 Eino Graph，输入类型为 *UserMessage，输出类型为 *schema.Message
	g := compose.NewGraph[*UserMessage, *schema.Message]()
	// 2. 添加节点，消息转换结点，转换类型：*UserMessage -> string（RAG 需要的输入格式）
	_ = g.AddLambdaNode(InputToRag, compose.InvokableLambdaWithOption(newInputToRagLambda), compose.WithNodeName("UserMessageToRag"))
	// 3. 添加 ChatTemplate 节点，输入类型为 string（用户输入）和 []Document（检索到的文档），输出类型为 string（生成的提示）
	chatTemplateKeyOfChatTemplate, err := newChatTemplate(ctx)
	if err != nil {
		return nil, err
	}
	_ = g.AddChatTemplateNode(ChatTemplate, chatTemplateKeyOfChatTemplate)
	// 4. 添加 ReAct Agent 节点，输入类型为 string（生成的提示），输出类型为 *schema.Message（最终回复）
	reactAgentKeyOfLambda, err := newReactAgentLambda(ctx)
	if err != nil {
		return nil, err
	}
	_ = g.AddLambdaNode(ReactAgent, reactAgentKeyOfLambda, compose.WithNodeName("ReActAgent"))
	// 5. 添加 Milvus Retriever 节点，输入类型为 string（RAG 需要的输入格式），输出类型为 []Document（检索到的文档）
	milvusRetrieverKeyOfRetriever, err := newRetriever(ctx)
	if err != nil {
		return nil, err
	}
	_ = g.AddRetrieverNode(MilvusRetriever, milvusRetrieverKeyOfRetriever, compose.WithOutputKey("documents"))
	// 6. 添加 InputToChat 节点，输入类型为 *UserMessage，输出类型为 map[string]any（用户输入的文本内容）
	_ = g.AddLambdaNode(InputToChat, compose.InvokableLambdaWithOption(newInputToChatLambda), compose.WithNodeName("UserMessageToChat"))

	// 7. 定义节点之间的连接关系，确保数据流正确传递
	_ = g.AddEdge(compose.START, InputToRag)
	_ = g.AddEdge(compose.START, InputToChat)
	_ = g.AddEdge(ReactAgent, compose.END)
	_ = g.AddEdge(InputToRag, MilvusRetriever)
	_ = g.AddEdge(MilvusRetriever, ChatTemplate)
	_ = g.AddEdge(InputToChat, ChatTemplate)
	_ = g.AddEdge(ChatTemplate, ReactAgent)

	// 8. 编译图，生成可运行的实例
	r, err = g.Compile(ctx, compose.WithGraphName("ChatAgent"), compose.WithNodeTriggerMode(compose.AllPredecessor))
	if err != nil {
		return nil, err
	}
	return r, err
}
