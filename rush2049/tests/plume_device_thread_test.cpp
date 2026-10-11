// plume's device thread layer (runtime patch plume-0002, plume_device_thread.cpp), run by CI against a mock backend
// that logs every call: every call reaches the backend on the device thread, in the order it was made; command lists
// recorded on several threads at once replay their commands with the backend's objects and with copies of what their
// arguments pointed at; push constants of size 0 take the size of their range; a swap chain's present() copies the
// texture drawn into the backend's swap chain texture; map() of a buffer whose data isn't there yet waits for it on
// the calling side (plume-0003); and every backend object is destroyed on the device thread.

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <future>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "plume_device_thread.h"

using namespace plume;

namespace {

int failures = 0;

void check(bool ok, const std::string &what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) {
        failures++;
    }
}

// A device thread for native code: one thread running queued work in order.
struct QueueThread : DeviceThread {
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<std::function<void()>> queue;
    bool stopping = false;
    std::thread thread;

    QueueThread() {
        thread = std::thread([this]() {
            std::unique_lock<std::mutex> lock(mutex);
            while (true) {
                condition.wait(lock, [&]() { return stopping || !queue.empty(); });
                if (queue.empty()) {
                    return;
                }

                std::function<void()> work = std::move(queue.front());
                queue.pop_front();
                lock.unlock();
                work();
                lock.lock();
            }
        });
    }

    ~QueueThread() override {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopping = true;
        }

        condition.notify_all();
        thread.join();
    }

    void runAsync(std::function<void()> &&work) override {
        {
            std::lock_guard<std::mutex> lock(mutex);
            queue.emplace_back(std::move(work));
        }

        condition.notify_all();
    }

    void runSync(const std::function<void()> &work) override {
        std::promise<void> done;
        runAsync([&]() { work(); done.set_value(); });
        done.get_future().wait();
    }
};

// The mock backend. Every call checks it is on the device thread and is logged there, so the log needs no lock.
std::thread::id deviceThreadId;
std::atomic<int> offThreadCalls{ 0 };
std::vector<std::string> events;
int liveObjects = 0;

void call(const std::string &event) {
    if (std::this_thread::get_id() != deviceThreadId) {
        offThreadCalls++;
        printf("     off the device thread: %s\n", event.c_str());
        return;
    }

    events.emplace_back(event);
}

struct Named {
    std::string name;
    Named(const std::string &name) : name(name) { liveObjects++; }
    virtual ~Named() { call("delete " + name); liveObjects--; }
};

int counter = 0;
std::string nextName(const char *kind) { return std::string(kind) + std::to_string(counter++); }

// How many times the next buffer created answers isMapReady() with false.
int nextBufferNotReady = 0;

template<typename T>
std::string nameOfT(const T *object) {
    return (object != nullptr) ? dynamic_cast<const Named *>(object)->name : "null";
}

struct MockBuffer : RenderBuffer, Named {
    std::vector<uint8_t> memory = std::vector<uint8_t>(256, 0);
    int notReady = nextBufferNotReady;
    MockBuffer() : Named(nextName("buffer")) { }
    bool isMapReady() override { call("isMapReady " + name); return (notReady == 0) || (notReady-- == 0); }
    void *map(uint32_t, const RenderRange *) override { call("map " + name); return memory.data(); }
    void unmap(uint32_t, const RenderRange *range) override { call("unmap " + name + " " + std::to_string(range ? range->end : 0) + " " + std::to_string(memory[0])); }
    std::unique_ptr<RenderBufferFormattedView> createBufferFormattedView(RenderFormat) override { call("createBufferFormattedView"); return nullptr; }
    void setName(const std::string &newName) override { call("setName " + name + " " + newName); name = newName; }
    uint64_t getDeviceAddress() const override { call("getDeviceAddress"); return 0; }
};

struct MockTexture : RenderTexture, Named {
    MockTexture(const std::string &name) : Named(name) { }
    std::unique_ptr<RenderTextureView> createTextureView(const RenderTextureViewDesc &) const override { call("createTextureView"); return nullptr; }
    void setName(const std::string &newName) override { call("setName " + name + " " + newName); name = newName; }
};

struct MockShader : RenderShader, Named {
    MockShader() : Named(nextName("shader")) { }
    void setName(const std::string &) override { }
};

struct MockSampler : RenderSampler, Named {
    MockSampler() : Named(nextName("sampler")) { }
};

struct MockPipeline : RenderPipeline, Named {
    MockPipeline(const std::string &name) : Named(name) { }
    void setName(const std::string &) override { }
    RenderPipelineProgram getProgram(const std::string &) const override { return {}; }
};

struct MockPipelineLayout : RenderPipelineLayout, Named {
    MockPipelineLayout(const std::string &name) : Named(name) { }
};

struct MockFence : RenderCommandFence, Named {
    MockFence() : Named(nextName("fence")) { }
};

struct MockSemaphore : RenderCommandSemaphore, Named {
    MockSemaphore() : Named(nextName("semaphore")) { }
};

struct MockDescriptorSet : RenderDescriptorSet, Named {
    MockDescriptorSet(const std::string &name) : Named(name) { }
    void setBuffer(uint32_t index, const RenderBuffer *buffer, uint64_t, const RenderBufferStructuredView *view, const RenderBufferFormattedView *) override {
        call("setBuffer " + name + " " + std::to_string(index) + " " + nameOfT(buffer) + " stride " + std::to_string(view ? view->structureByteStride : 0));
    }
    void setTexture(uint32_t index, const RenderTexture *texture, RenderTextureLayout, const RenderTextureView *) override { call("setTexture " + name + " " + std::to_string(index) + " " + nameOfT(texture)); }
    void setSampler(uint32_t index, const RenderSampler *sampler) override { call("setSampler " + name + " " + std::to_string(index) + " " + nameOfT(sampler)); }
    void setAccelerationStructure(uint32_t, const RenderAccelerationStructure *) override { }
};

struct MockFramebuffer : RenderFramebuffer, Named {
    MockFramebuffer(const std::string &name) : Named(name) { }
    uint32_t getWidth() const override { call("framebuffer getWidth"); return 64; }
    uint32_t getHeight() const override { call("framebuffer getHeight"); return 32; }
};

struct MockCommandList : RenderCommandList, Named {
    MockCommandList() : Named(nextName("list")) { }
    void log(const std::string &command) { call(name + ": " + command); }
    void begin() override { log("begin"); }
    void end() override { log("end"); }
    void barriers(RenderBarrierStages, const RenderBufferBarrier *b, uint32_t bc, const RenderTextureBarrier *t, uint32_t tc) override {
        std::string s = "barriers";
        for (uint32_t i = 0; i < bc; i++) s += " " + nameOfT(b[i].buffer);
        for (uint32_t i = 0; i < tc; i++) s += " " + nameOfT(t[i].texture);
        log(s);
    }
    void dispatch(uint32_t x, uint32_t, uint32_t) override { log("dispatch " + std::to_string(x)); }
    void traceRays(uint32_t, uint32_t, uint32_t, RenderBufferReference, const RenderShaderBindingGroupsInfo &) override { }
    void drawInstanced(uint32_t v, uint32_t, uint32_t, uint32_t) override { log("draw " + std::to_string(v)); }
    void drawIndexedInstanced(uint32_t i, uint32_t, uint32_t, int32_t, uint32_t) override { log("drawIndexed " + std::to_string(i)); }
    void setPipeline(const RenderPipeline *p) override { log("setPipeline " + nameOfT(p)); }
    void setComputePipelineLayout(const RenderPipelineLayout *p) override { log("setComputePipelineLayout " + nameOfT(p)); }
    void setComputePushConstants(uint32_t, const void *, uint32_t, uint32_t) override { }
    void setComputeDescriptorSet(RenderDescriptorSet *, uint32_t) override { }
    void setGraphicsPipelineLayout(const RenderPipelineLayout *p) override { log("setGraphicsPipelineLayout " + nameOfT(p)); }
    void setGraphicsPushConstants(uint32_t range, const void *data, uint32_t offset, uint32_t size) override {
        std::string s = "push " + std::to_string(range) + " " + std::to_string(offset) + " " + std::to_string(size) + ":";
        for (uint32_t i = 0; i < size; i++) s += " " + std::to_string(reinterpret_cast<const uint8_t *>(data)[i]);
        log(s);
    }
    void setGraphicsDescriptorSet(RenderDescriptorSet *set, uint32_t index) override { log("setGraphicsDescriptorSet " + nameOfT(set) + " " + std::to_string(index)); }
    void setGraphicsRootDescriptor(RenderBufferReference, uint32_t) override { }
    void setRaytracingPipelineLayout(const RenderPipelineLayout *) override { }
    void setRaytracingPushConstants(uint32_t, const void *, uint32_t, uint32_t) override { }
    void setRaytracingDescriptorSet(RenderDescriptorSet *, uint32_t) override { }
    void setIndexBuffer(const RenderIndexBufferView *view) override { log("setIndexBuffer " + (view ? nameOfT(view->buffer.ref) + "+" + std::to_string(view->buffer.offset) : std::string("null"))); }
    void setVertexBuffers(uint32_t, const RenderVertexBufferView *views, uint32_t count, const RenderInputSlot *slots) override {
        std::string s = "setVertexBuffers";
        for (uint32_t i = 0; i < count; i++) s += " " + nameOfT(views[i].buffer.ref) + "/" + std::to_string(slots ? slots[i].stride : 0);
        log(s);
    }
    void setViewports(const RenderViewport *v, uint32_t count) override { log("setViewports " + std::to_string(count) + " " + std::to_string(int(v[count - 1].width))); }
    void setScissors(const RenderRect *r, uint32_t count) override { log("setScissors " + std::to_string(count) + " " + std::to_string(r[0].right)); }
    void setFramebuffer(const RenderFramebuffer *f) override { log("setFramebuffer " + nameOfT(f)); }
    void setDepthBias(float, float, float) override { }
    void clearColor(uint32_t, RenderColor, const RenderRect *, uint32_t count) override { log("clearColor " + std::to_string(count)); }
    void clearDepthStencil(bool, bool, float, uint32_t, const RenderRect *, uint32_t) override { }
    void copyBufferRegion(RenderBufferReference dst, RenderBufferReference src, uint64_t size) override {
        log("copyBufferRegion " + nameOfT(dst.ref) + "+" + std::to_string(dst.offset) + " " + nameOfT(src.ref) + "+" + std::to_string(src.offset) + " " + std::to_string(size));
    }
    void copyTextureRegion(const RenderTextureCopyLocation &dst, const RenderTextureCopyLocation &src, uint32_t, uint32_t, uint32_t, const RenderBox *box) override {
        log("copyTextureRegion " + nameOfT(dst.texture) + " " + nameOfT(src.buffer) + " " + std::to_string(src.placedFootprint.width) + " box " + std::to_string(box ? box->right : -1));
    }
    void copyBuffer(const RenderBuffer *, const RenderBuffer *) override { }
    void copyTexture(const RenderTexture *dst, const RenderTexture *src) override { log("copyTexture " + nameOfT(dst) + " " + nameOfT(src)); }
    void resolveTexture(const RenderTexture *, const RenderTexture *) override { }
    void resolveTextureRegion(const RenderTexture *, uint32_t, uint32_t, const RenderTexture *, const RenderRect *, RenderResolveMode) override { }
    void buildBottomLevelAS(const RenderAccelerationStructure *, RenderBufferReference, const RenderBottomLevelASBuildInfo &) override { }
    void buildTopLevelAS(const RenderAccelerationStructure *, RenderBufferReference, RenderBufferReference, const RenderTopLevelASBuildInfo &) override { }
    void discardTexture(const RenderTexture *) override { }
    void resetQueryPool(const RenderQueryPool *, uint32_t, uint32_t) override { }
    void writeTimestamp(const RenderQueryPool *, uint32_t) override { }
};

struct MockSwapChain : RenderSwapChain, Named {
    std::vector<std::unique_ptr<MockTexture>> textures;
    uint32_t width = 0, height = 0, next = 0;
    MockSwapChain() : Named("swapChain") {
        textures.emplace_back(std::make_unique<MockTexture>("canvas0"));
        textures.emplace_back(std::make_unique<MockTexture>("canvas1"));
    }
    bool present(uint32_t index, RenderCommandSemaphore **waits, uint32_t count) override { call("present " + std::to_string(index) + " waits " + std::to_string(count)); return true; }
    void wait() override { }
    bool resize() override { call("resize"); width = 640; height = 480; return true; }
    bool needsResize() const override { call("needsResize"); return width == 0; }
    void setVsyncEnabled(bool) override { }
    bool isVsyncEnabled() const override { return true; }
    uint32_t getWidth() const override { return width; }
    uint32_t getHeight() const override { return height; }
    RenderTexture *getTexture(uint32_t index) override { return textures[index].get(); }
    uint32_t getTextureCount() const override { return 2; }
    bool acquireTexture(RenderCommandSemaphore *, uint32_t *index) override { call("acquire"); *index = next; next = (next + 1) % 2; return true; }
    RenderWindow getWindow() const override { return {}; }
    bool isEmpty() const override { return width == 0; }
    uint32_t getRefreshRate() const override { return 60; }
};

struct MockQueue : RenderCommandQueue, Named {
    MockQueue() : Named(nextName("queue")) { }
    std::unique_ptr<RenderCommandList> createCommandList() override { call("createCommandList"); return std::make_unique<MockCommandList>(); }
    std::unique_ptr<RenderSwapChain> createSwapChain(const RenderSwapChainDesc &) override { call("createSwapChain"); return std::make_unique<MockSwapChain>(); }
    void executeCommandLists(const RenderCommandList **lists, uint32_t count, RenderCommandSemaphore **, uint32_t waitCount, RenderCommandSemaphore **signals, uint32_t signalCount, RenderCommandFence *fence) override {
        std::string s = "execute";
        for (uint32_t i = 0; i < count; i++) s += " " + nameOfT(lists[i]);
        for (uint32_t i = 0; i < signalCount; i++) s += " signal " + nameOfT(signals[i]);
        call(s + " waits " + std::to_string(waitCount) + " fence " + nameOfT(fence));
    }
    void waitForCommandFence(RenderCommandFence *fence) override { call("waitForCommandFence " + nameOfT(fence)); }
};

struct MockDevice : RenderDevice, Named {
    RenderDeviceCapabilities capabilities;
    RenderDeviceDescription description;
    MockDevice() : Named("device") { description.name = "Mock"; capabilities.maxTextureSize = 1234; }
    std::unique_ptr<RenderDescriptorSet> createDescriptorSet(const RenderDescriptorSetDesc &desc) override {
        std::string s = "createDescriptorSet";
        for (uint32_t i = 0; i < desc.descriptorRangesCount; i++) {
            const RenderDescriptorRange &range = desc.descriptorRanges[i];
            for (uint32_t j = 0; (range.immutableSampler != nullptr) && (j < range.count); j++) s += " " + nameOfT(range.immutableSampler[j]);
        }
        call(s);
        return std::make_unique<MockDescriptorSet>(nextName("set"));
    }
    std::unique_ptr<RenderShader> createShader(const void *, uint64_t, const char *, RenderShaderFormat) override { call("createShader"); return std::make_unique<MockShader>(); }
    std::unique_ptr<RenderSampler> createSampler(const RenderSamplerDesc &) override { call("createSampler"); return std::make_unique<MockSampler>(); }
    std::unique_ptr<RenderPipeline> createComputePipeline(const RenderComputePipelineDesc &) override { return nullptr; }
    std::unique_ptr<RenderPipeline> createGraphicsPipeline(const RenderGraphicsPipelineDesc &desc) override {
        call("createGraphicsPipeline " + nameOfT(desc.pipelineLayout) + " " + nameOfT(desc.vertexShader) + " " + nameOfT(desc.pixelShader));
        return std::make_unique<MockPipeline>(nextName("pipeline"));
    }
    std::unique_ptr<RenderPipeline> createRaytracingPipeline(const RenderRaytracingPipelineDesc &, const RenderPipeline *) override { return nullptr; }
    std::unique_ptr<RenderCommandQueue> createCommandQueue(RenderCommandListType) override { call("createCommandQueue"); return std::make_unique<MockQueue>(); }
    std::unique_ptr<RenderBuffer> createBuffer(const RenderBufferDesc &) override { call("createBuffer"); return std::make_unique<MockBuffer>(); }
    std::unique_ptr<RenderTexture> createTexture(const RenderTextureDesc &desc) override {
        call("createTexture " + std::to_string(desc.width) + "x" + std::to_string(desc.height));
        return std::make_unique<MockTexture>(nextName("texture"));
    }
    std::unique_ptr<RenderAccelerationStructure> createAccelerationStructure(const RenderAccelerationStructureDesc &) override { return nullptr; }
    std::unique_ptr<RenderPool> createPool(const RenderPoolDesc &) override { return nullptr; }
    std::unique_ptr<RenderPipelineLayout> createPipelineLayout(const RenderPipelineLayoutDesc &desc) override {
        std::string s = "createPipelineLayout";
        for (uint32_t i = 0; i < desc.descriptorSetDescsCount; i++) {
            const RenderDescriptorSetDesc &set = desc.descriptorSetDescs[i];
            for (uint32_t j = 0; j < set.descriptorRangesCount; j++) {
                const RenderDescriptorRange &range = set.descriptorRanges[j];
                for (uint32_t k = 0; (range.immutableSampler != nullptr) && (k < range.count); k++) s += " " + nameOfT(range.immutableSampler[k]);
            }
        }
        call(s);
        return std::make_unique<MockPipelineLayout>(nextName("layout"));
    }
    std::unique_ptr<RenderCommandFence> createCommandFence() override { call("createCommandFence"); return std::make_unique<MockFence>(); }
    std::unique_ptr<RenderCommandSemaphore> createCommandSemaphore() override { call("createCommandSemaphore"); return std::make_unique<MockSemaphore>(); }
    std::unique_ptr<RenderFramebuffer> createFramebuffer(const RenderFramebufferDesc &desc) override {
        call("createFramebuffer " + nameOfT(desc.colorAttachments[0]));
        return std::make_unique<MockFramebuffer>(nextName("framebuffer"));
    }
    std::unique_ptr<RenderQueryPool> createQueryPool(uint32_t) override { return nullptr; }
    void setBottomLevelASBuildInfo(RenderBottomLevelASBuildInfo &, const RenderBottomLevelASMesh *, uint32_t, bool, bool) override { }
    void setTopLevelASBuildInfo(RenderTopLevelASBuildInfo &, const RenderTopLevelASInstance *, uint32_t, bool, bool) override { }
    void setShaderBindingTableInfo(RenderShaderBindingTableInfo &, const RenderShaderBindingGroups &, const RenderPipeline *, RenderDescriptorSet **, uint32_t) override { }
    const RenderDeviceCapabilities &getCapabilities() const override { call("getCapabilities"); return capabilities; }
    const RenderDeviceDescription &getDescription() const override { call("getDescription"); return description; }
    RenderSampleCounts getSampleCountsSupported(RenderFormat) const override { call("getSampleCountsSupported"); return RenderSampleCount::COUNT_1; }
    bool beginCapture() override { return false; }
    bool endCapture() override { return false; }
};

struct MockInterface : RenderInterface, Named {
    std::vector<std::string> deviceNames = { "Mock" };
    RenderInterfaceCapabilities capabilities;
    MockInterface() : Named("interface") { capabilities.shaderFormat = RenderShaderFormat::SPIRV; }
    std::unique_ptr<RenderDevice> createDevice(const std::string &) override { call("createDevice"); return std::make_unique<MockDevice>(); }
    const std::vector<std::string> &getDeviceNames() const override { call("getDeviceNames"); return deviceNames; }
    const RenderInterfaceCapabilities &getCapabilities() const override { call("getCapabilities"); return capabilities; }
};

// The events whose text starts with prefix, in order.
std::vector<std::string> eventsStartingWith(const std::string &prefix) {
    std::vector<std::string> found;
    for (const std::string &event : events) {
        if (event.compare(0, prefix.size(), prefix) == 0) {
            found.emplace_back(event);
        }
    }

    return found;
}

bool contains(const std::string &event) {
    for (const std::string &e : events) {
        if (e == event) {
            return true;
        }
    }

    return false;
}

void printEvents(const std::vector<std::string> &list) {
    for (const std::string &event : list) {
        printf("       %s\n", event.c_str());
    }
}

} // namespace

int main() {
    auto deviceThread = std::make_shared<QueueThread>();
    deviceThread->runSync([]() { deviceThreadId = std::this_thread::get_id(); });

    std::unique_ptr<RenderInterface> renderInterface = CreateDeviceThreadInterface(deviceThread, []() { return std::make_unique<MockInterface>(); });
    check(renderInterface->getCapabilities().shaderFormat == RenderShaderFormat::SPIRV, "the interface's capabilities are the backend's");
    std::unique_ptr<RenderDevice> device = renderInterface->createDevice();
    check(device->getDescription().name == "Mock" && device->getCapabilities().maxTextureSize == 1234, "the device's description and capabilities are the backend's");
    std::unique_ptr<RenderCommandQueue> queue = device->createCommandQueue(RenderCommandListType::DIRECT);

    // Immutable samplers and shaders reach the backend as its own objects.
    std::unique_ptr<RenderSampler> sampler = device->createSampler(RenderSamplerDesc());
    const RenderSampler *immutableSamplers[] = { sampler.get(), sampler.get() };
    RenderDescriptorRange ranges[] = { RenderDescriptorRange(RenderDescriptorRangeType::SAMPLER, 0, 2, immutableSamplers), RenderDescriptorRange(RenderDescriptorRangeType::TEXTURE, 2, 1) };
    RenderDescriptorSetDesc setDesc(ranges, 2);
    std::unique_ptr<RenderDescriptorSet> set = device->createDescriptorSet(setDesc);
    RenderPushConstantRange pushRanges[] = { RenderPushConstantRange(0, 0, 0, 8, RenderShaderStageFlag::VERTEX), RenderPushConstantRange(1, 0, 8, 4, RenderShaderStageFlag::PIXEL) };
    std::unique_ptr<RenderPipelineLayout> layout = device->createPipelineLayout(RenderPipelineLayoutDesc(pushRanges, 2, &setDesc, 1));
    std::unique_ptr<RenderShader> vertexShader = device->createShader(nullptr, 0, "VSMain", RenderShaderFormat::SPIRV);
    std::unique_ptr<RenderShader> pixelShader = device->createShader(nullptr, 0, "PSMain", RenderShaderFormat::SPIRV);
    RenderGraphicsPipelineDesc pipelineDesc;
    pipelineDesc.pipelineLayout = layout.get();
    pipelineDesc.vertexShader = vertexShader.get();
    pipelineDesc.pixelShader = pixelShader.get();
    std::unique_ptr<RenderPipeline> pipeline = device->createGraphicsPipeline(pipelineDesc);
    deviceThread->runSync([]() {});
    check(contains("createDescriptorSet sampler1 sampler1"), "a descriptor set's immutable samplers are the backend's");
    check(contains("createPipelineLayout sampler1 sampler1"), "a pipeline layout's immutable samplers are the backend's");
    check(contains("createGraphicsPipeline layout3 shader4 shader5"), "a pipeline's layout and shaders are the backend's");

    // Threads record at once, each its own list, buffer and texture; a descriptor write, an upload through map() and
    // unmap(), and every argument that points at something, overwritten once recorded.
    const int threadCount = 6;
    std::vector<std::thread> threads;
    std::unique_ptr<RenderFramebuffer> framebuffer;
    std::unique_ptr<RenderTexture> target = device->createTexture(RenderTextureDesc::ColorTarget(64, 32, RenderFormat::R8G8B8A8_UNORM));
    target->setName("target");
    const RenderTexture *targetTextures[] = { target.get() };
    framebuffer = device->createFramebuffer(RenderFramebufferDesc(targetTextures, 1));
    check((framebuffer->getWidth() == 64) && (framebuffer->getHeight() == 32), "a framebuffer's size is the backend's");

    for (int t = 0; t < threadCount; t++) {
        threads.emplace_back([&, t]() {
            const std::string id = std::to_string(t);
            std::unique_ptr<RenderBuffer> buffer = device->createBuffer(RenderBufferDesc::UploadBuffer(256));
            buffer->setName("upload" + id);
            std::unique_ptr<RenderTexture> texture = device->createTexture(RenderTextureDesc::Texture2D(16, 16, 1, RenderFormat::R8G8B8A8_UNORM));
            texture->setName("tex" + id);
            std::unique_ptr<RenderCommandList> list = queue->createCommandList();
            std::unique_ptr<RenderCommandFence> fence = device->createCommandFence();
            for (int frame = 0; frame < 20; frame++) {
                uint8_t *memory = reinterpret_cast<uint8_t *>(buffer->map());
                memory[0] = uint8_t(t * 20 + frame);
                RenderRange written(0, 16);
                buffer->unmap(0, &written);
                written.end = 99;

                list->begin();
                list->setGraphicsPipelineLayout(layout.get());
                uint8_t push[8] = { uint8_t(t), uint8_t(frame), 2, 3, 4, 5, 6, 7 };
                list->setGraphicsPushConstants(0, push);
                list->setGraphicsPushConstants(1, push + 4, 0, 2);
                memset(push, 0xEE, sizeof(push));
                list->setPipeline(pipeline.get());
                list->setGraphicsDescriptorSet(set.get(), 0);
                RenderTextureBarrier barrier(texture.get(), RenderTextureLayout::COPY_DEST);
                list->barriers(RenderBarrierStage::COPY, &barrier, 1);
                barrier.texture = nullptr;
                RenderBox box(0, 0, 16, 16);
                list->copyTextureRegion(RenderTextureCopyLocation::Subresource(texture.get()), RenderTextureCopyLocation::PlacedFootprint(buffer.get(), RenderFormat::R8G8B8A8_UNORM, 16, 16, 1, 16), 0, 0, 0, &box);
                box.right = 0;
                list->copyBufferRegion(buffer->at(64), buffer->at(t), 8);
                RenderViewport viewports[2] = { RenderViewport(0, 0, 10, 10), RenderViewport(0, 0, float(32 + t), 10) };
                list->setViewports(viewports, 2);
                viewports[1].width = 0;
                RenderRect scissor(0, 0, 50 + t, 10);
                list->setScissors(scissor);
                scissor.right = 0;
                RenderVertexBufferView vertexView(buffer.get(), 256);
                RenderInputSlot slot(0, 12);
                list->setVertexBuffers(0, &vertexView, 1, &slot);
                vertexView.buffer.ref = nullptr;
                slot.stride = 0;
                RenderIndexBufferView indexView(buffer->at(128), 64, RenderFormat::R16_UINT);
                list->setIndexBuffer(&indexView);
                indexView.buffer.ref = nullptr;
                list->setFramebuffer(framebuffer.get());
                list->drawIndexedInstanced(3, 1, 0, 0, 0);
                list->end();
                queue->executeCommandLists(list.get(), fence.get());

                // Record the next frame before waiting for this one: the recording executed must be this frame's.
                if ((frame % 2) == 0) {
                    list->begin();
                    list->drawInstanced(1000 + frame, 1, 0, 0);
                    list->end();
                    queue->executeCommandLists(list.get(), fence.get());
                }

                queue->waitForCommandFence(fence.get());
                set->setTexture(1 + t, texture.get(), RenderTextureLayout::SHADER_READ);
            }
        });
    }

    for (std::thread &thread : threads) {
        thread.join();
    }

    // Each thread's list replayed what that thread recorded, in its order, with copies of its arguments.
    for (int t = 0; t < threadCount; t++) {
        const std::string id = std::to_string(t);
        std::vector<std::string> unmaps = eventsStartingWith("unmap upload" + id + " ");
        bool unmapsOk = (unmaps.size() == 20);
        for (size_t frame = 0; unmapsOk && (frame < unmaps.size()); frame++) {
            unmapsOk = (unmaps[frame] == "unmap upload" + id + " 16 " + std::to_string(t * 20 + frame));
        }

        check(unmapsOk, "thread " + id + ": unmap() keeps its range, after the caller's writes, in order");

        // Find this thread's command list by its first push constants.
        std::string listName;
        for (const std::string &event : events) {
            const std::string push = ": push 0 0 8: " + id + " 0 2 3 4 5 6 7";
            if ((event.size() > push.size()) && (event.compare(event.size() - push.size(), push.size(), push) == 0)) {
                listName = event.substr(0, event.size() - push.size());
            }
        }

        std::vector<std::string> commands = eventsStartingWith(listName + ": ");
        std::vector<std::string> expected;
        for (int frame = 0; frame < 20; frame++) {
            const std::string f = std::to_string(frame);
            const std::vector<std::string> frameCommands = {
                "begin",
                "setGraphicsPipelineLayout layout3",
                "push 0 0 8: " + id + " " + f + " 2 3 4 5 6 7",
                "push 1 0 2: 4 5",
                "setPipeline pipeline6",
                "setGraphicsDescriptorSet set2 0",
                "barriers tex" + id,
                "copyTextureRegion tex" + id + " upload" + id + " 16 box 16",
                "copyBufferRegion upload" + id + "+64 upload" + id + "+" + id + " 8",
                "setViewports 2 " + std::to_string(32 + t),
                "setScissors 1 " + std::to_string(50 + t),
                "setVertexBuffers upload" + id + "/12",
                "setIndexBuffer upload" + id + "+128",
                "setFramebuffer framebuffer8",
                "drawIndexed 3",
                "end"
            };

            for (const std::string &command : frameCommands) {
                expected.emplace_back(listName + ": " + command);
            }

            if ((frame % 2) == 0) {
                expected.emplace_back(listName + ": begin");
                expected.emplace_back(listName + ": draw " + std::to_string(1000 + frame));
                expected.emplace_back(listName + ": end");
            }
        }

        const bool listOk = !listName.empty() && (commands == expected);
        check(listOk, "thread " + id + ": its command list replays what it recorded, with the backend's objects and copied arguments");
        if (!listOk) {
            printEvents(commands);
        }

        check(contains("setTexture set2 " + std::to_string(1 + t) + " tex" + id), "thread " + id + ": a descriptor write reaches the backend with its texture");
    }

    // A swap chain hands out its own textures and present() copies the one drawn into the backend's.
    std::unique_ptr<RenderSwapChain> swapChain = queue->createSwapChain(RenderSwapChainDesc({}, RenderFormat::B8G8R8A8_UNORM, 2));
    check(swapChain->isEmpty() && swapChain->needsResize(), "a new swap chain is empty and needs a resize");
    check(swapChain->resize() && (swapChain->getWidth() == 640) && (swapChain->getHeight() == 480), "resize() takes the backend's size");
    check(swapChain->getTextureCount() == 2, "the swap chain has the backend's texture count");
    std::unique_ptr<RenderCommandList> presentList = queue->createCommandList();
    std::unique_ptr<RenderCommandSemaphore> acquired = device->createCommandSemaphore();
    std::unique_ptr<RenderCommandSemaphore> drawn = device->createCommandSemaphore();
    std::unique_ptr<RenderCommandFence> presentFence = device->createCommandFence();
    std::vector<std::string> drawnInto;
    for (int frame = 0; frame < 3; frame++) {
        uint32_t index = 0;
        check(swapChain->acquireTexture(acquired.get(), &index) && (index == uint32_t(frame % 2)), "acquireTexture() hands out the swap chain's textures in turn");
        presentList->begin();
        presentList->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(swapChain->getTexture(index), RenderTextureLayout::COLOR_WRITE));
        presentList->clearColor(0, RenderColor(), nullptr, 0);
        presentList->end();
        RenderCommandSemaphore *waitSemaphore = acquired.get();
        RenderCommandSemaphore *signalSemaphore = drawn.get();
        const RenderCommandList *lists[] = { presentList.get() };
        queue->executeCommandLists(lists, 1, &waitSemaphore, 1, &signalSemaphore, 1, presentFence.get());
        swapChain->present(index, &signalSemaphore, 1);
    }

    queue->waitForCommandFence(presentFence.get());
    std::vector<std::string> presentEvents;
    for (const std::string &event : events) {
        if ((event.find("copyTexture ") != std::string::npos) || (event.compare(0, 7, "present") == 0) || (event == "acquire") || (event.compare(0, 14, "createTexture ") == 0 && event.find("640x480") != std::string::npos)) {
            presentEvents.emplace_back(event);
        }
    }

    // The two textures of the swap chain are made at resize(), then each present acquires, copies and presents.
    const bool presentOk = (presentEvents.size() == 2 + 3 * 3) && (presentEvents[0] == "createTexture 640x480") && (presentEvents[1] == "createTexture 640x480") &&
        (presentEvents[2] == "acquire") && (presentEvents[3].find("copyTexture canvas0 ") != std::string::npos) && (presentEvents[4] == "present 0 waits 1") &&
        (presentEvents[5] == "acquire") && (presentEvents[6].find("copyTexture canvas1 ") != std::string::npos) && (presentEvents[7] == "present 1 waits 1") &&
        (presentEvents[8] == "acquire") && (presentEvents[9].find("copyTexture canvas0 ") != std::string::npos);
    check(presentOk, "present() acquires the backend's texture, copies the one drawn into it and presents it, in one go");
    if (!presentOk) {
        printEvents(presentEvents);
    }

    // The texture drawn and the texture copied are the same backend texture.
    std::vector<std::string> barriers;
    for (const std::string &event : events) {
        if ((event.find(": barriers texture") != std::string::npos) && (event.find("canvas") == std::string::npos)) {
            barriers.emplace_back(event.substr(event.find(": barriers ") + 11));
        }
    }

    bool sameTexture = (barriers.size() >= 3) && (presentEvents.size() == 11);
    for (int frame = 0; sameTexture && (frame < 3); frame++) {
        const std::string &copy = presentEvents[3 + 3 * frame];
        sameTexture = (copy.substr(copy.rfind(' ') + 1) == barriers[frame]);
    }

    check(sameTexture, "the texture a frame is drawn into is the one present() copies");
    check(eventsStartingWith("execute").size() >= 3 && contains("execute signal semaphore" + std::to_string(counter - 3) + " waits 0 fence null"), "acquireTexture() signals the caller's semaphore");

    // A buffer whose data arrives late: map() asks isMapReady() from the calling side until it's true, then maps.
    nextBufferNotReady = 3;
    std::unique_ptr<RenderBuffer> readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(256));
    nextBufferNotReady = 0;
    readback->setName("readback");
    const void *mapped = readback->map();
    deviceThread->runSync([]() {});
    std::vector<std::string> readbackEvents = eventsStartingWith("isMapReady readback");
    check((mapped != nullptr) && (readbackEvents.size() == 4) && (eventsStartingWith("map readback").size() == 1), "map() waits on the calling side until isMapReady(), then maps once");
    readback.reset();

    // Everything is destroyed on the device thread, the interface last.
    presentFence.reset(); drawn.reset(); acquired.reset(); presentList.reset(); swapChain.reset();
    framebuffer.reset(); target.reset(); pipeline.reset(); pixelShader.reset(); vertexShader.reset();
    layout.reset(); set.reset(); sampler.reset(); queue.reset(); device.reset(); renderInterface.reset();
    deviceThread->runSync([]() {});
    check(liveObjects == 0, "every backend object was destroyed (" + std::to_string(liveObjects) + " left)");
    check(offThreadCalls == 0, "no call reached the backend off the device thread (" + std::to_string(offThreadCalls.load()) + ")");

    printf("%s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
