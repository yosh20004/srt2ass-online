## server和后端逻辑是如何协同工作的 ##
- server不变，但实现一个src/http_handler
- server call http_handler去进行处理工作
- http_handler call 具体逻辑部分代码进行工作
- 最终实现server与后端的**解耦**