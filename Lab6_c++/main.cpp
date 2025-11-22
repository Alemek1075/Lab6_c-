#include <coroutine>
#include <iostream>
#include <random>
#include <memory>

struct Task;

struct TransferControl {
    std::coroutine_handle<> target;

    bool await_ready() const noexcept { return false; }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> current) const noexcept {
        return target;
    }

    void await_resume() const noexcept {}
};

struct Task {
    struct promise_type {
        int value = 0;

        Task get_return_object();
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    using Handle = std::coroutine_handle<promise_type>;
    Handle handle;

    Task(Handle h) : handle(h) {}
    ~Task() { if (handle) handle.destroy(); }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle) handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    void setValue(int v) {
        handle.promise().value = v;
    }
};

Task Task::promise_type::get_return_object() {
    return Task{ Handle::from_promise(*this) };
}

struct GetValue {
    int value;

    bool await_ready() const noexcept { return false; }

    bool await_suspend(std::coroutine_handle<Task::promise_type> h) noexcept {
        value = h.promise().value;
        return false;
    }

    int await_resume() const noexcept {
        return value;
    }
};

struct Context {
    std::coroutine_handle<> generator;
};

Task coroutineA(Context* ctx) {
    while (true) {
        int val = co_await GetValue{};
        std::cout << "[Coroutine A] Received even number: " << val << std::endl;
        if (ctx->generator)
            co_await TransferControl{ ctx->generator };
    }
}

Task coroutineB(Context* ctx) {
    while (true) {
        int val = co_await GetValue{};
        std::cout << "[Coroutine B] Received odd number: " << val
            << ". Square: " << (val * val) << std::endl;
        if (ctx->generator)
            co_await TransferControl{ ctx->generator };
    }
}

Task generator(Task* tA, Task* tB) {
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(1, 256);

    std::cout << "Generator started." << std::endl;

    for (int i = 0; i < 10; ++i) {
        int num = dist(rng);
        std::cout << "Generator: generated " << num << std::endl;

        if (num % 2 == 0) {
            tA->setValue(num);
            co_await TransferControl{ tA->handle };
        }
        else {
            tB->setValue(num);
            co_await TransferControl{ tB->handle };
        }
    }

    std::cout << "Generator finished." << std::endl;
    exit(0);
}

int main() {
    Context ctx;

    Task tA = coroutineA(&ctx);
    Task tB = coroutineB(&ctx);

    Task gen = generator(&tA, &tB);

    ctx.generator = gen.handle;

    gen.handle.resume();

    return 0;
}