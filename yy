class MessageHandler {
private:
    struct BufferPair {
        std::vector<Data> currentBuffer;
        std::vector<Data> processBuffer;
        std::mutex mutex;
    };
    
    std::unordered_map<int, std::unique_ptr<BufferPair>> typeBuffers_;

public:
    void onMessage(const Message& msg) {
        auto& bufferPair = typeBuffers_[msg.type];
        if (!bufferPair) {
            bufferPair = std::make_unique<BufferPair>();
        }
        
        std::lock_guard<std::mutex> lock(bufferPair->mutex);
        bufferPair->currentBuffer.push_back(msg.data);
    }

    std::vector<Data> getData(int type) {
        auto it = typeBuffers_.find(type);
        if (it == typeBuffers_.end()) {
            return {};
        }
        
        auto& bufferPair = it->second;
        std::lock_guard<std::mutex> lock(bufferPair->mutex);
        
        // 交换缓冲区，保证不会丢失数据
        bufferPair->processBuffer.clear();
        bufferPair->currentBuffer.swap(bufferPair->processBuffer);
        
        return bufferPair->processBuffer;
    }
};
