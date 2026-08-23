// #include <iostream>
// #include <typeinfo>
// #include <cassert>
// #include <unistd.h>
// #include <any>

// class Any{
//     private:
//         class holder {
//             public:
//                 virtual ~holder() {}
//                 virtual const std::type_info& type() = 0;
//                 virtual holder *clone() = 0;
//         };
//         template<class T>
//         class placeholder: public holder {
//             public:
//                 placeholder(const T &val): _val(val) {}
//                 // 获取子类对象保存的数据类型
//                 virtual const std::type_info& type() { return typeid(T); }
//                 // 针对当前的对象自身，克隆出一个新的子类对象
//                 virtual holder *clone() { return new placeholder(_val); }
//             public:
//                 T _val;
//         };
//         holder *_content;
//     public:
//         Any():_content(NULL) {}
//         template<class T>
//         Any(const T &val):_content(new placeholder<T>(val)) {}
//         Any(const Any &other):_content(other._content ? other._content->clone() : NULL) {}
//         ~Any() { delete _content; }

//         Any &swap(Any &other) {
//             std::swap(_content, other._content);
//             return *this;
//         }

//         // 返回子类对象保存的数据的指针
//         template<class T>
//         T *get() {
//             //想要获取的数据类型，必须和保存的数据类型一致
//             assert(typeid(T) == _content->type());
//             return &((placeholder<T>*)_content)->_val;
//         }
//         //赋值运算符的重载函数
//         template<class T>
//         Any& operator=(const T &val) {
//             //为val构造一个临时的通用容器，然后与当前容器自身进行指针交换，临时对象释放的时候，原先保存的数据也就被释放
//             Any(val).swap(*this);
//             return *this;
//         }
//         Any& operator=(const Any &other) {
//             Any(other).swap(*this);
//             return *this;
//         }
// };

// class Test{
//     public:
//         Test() {std::cout << "构造" << std::endl;}
//         Test(const Test &t) {std::cout << "拷贝" << std::endl;}
//         ~Test() {std::cout << "析构" << std::endl;}
// };

#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <memory>
#include <unistd.h>

using TaskFunc = std::function<void()>;
using ReleaseFunc = std::function<void()>;
class TimerTask{
    private:
        uint64_t _id;       // 定时器任务对象ID
        uint32_t _timeout;  //定时任务的超时时间
        bool _canceled;     // false-表示没有被取消， true-表示被取消
        TaskFunc _task_cb;  //定时器对象要执行的定时任务
        ReleaseFunc _release; //用于删除TimerWheel中保存的定时器对象信息
    public:
        TimerTask(uint64_t id, uint32_t delay, const TaskFunc &cb): 
            _id(id), _timeout(delay), _task_cb(cb), _canceled(false) {}
        ~TimerTask() { 
            if (_canceled == false) _task_cb(); 
            _release(); 
        }
        void Cancel() { _canceled = true; }
        void SetRelease(const ReleaseFunc &cb) { _release = cb; }
        uint32_t DelayTime() { return _timeout; }
};

class TimerWheel {
    private:
        using WeakTask = std::weak_ptr<TimerTask>;
        using PtrTask = std::shared_ptr<TimerTask>;
        int _tick;      //当前的秒针，走到哪里释放哪里，释放哪里，就相当于执行哪里的任务
        int _capacity;  //表盘最大数量---其实就是最大延迟时间
        std::vector<std::vector<PtrTask>> _wheel;
        std::unordered_map<uint64_t, WeakTask> _timers;
    private:
        void RemoveTimer(uint64_t id) {
            auto it = _timers.find(id);
            if (it != _timers.end()) {
                _timers.erase(it);
            }
        }
    public:
        TimerWheel():_capacity(60), _tick(0), _wheel(_capacity) {}
        void TimerAdd(uint64_t id, uint32_t delay, const TaskFunc &cb) {
            PtrTask pt(new TimerTask(id, delay, cb));
            pt->SetRelease(std::bind(&TimerWheel::RemoveTimer, this, id));
            int pos = (_tick + delay) % _capacity;
            _wheel[pos].push_back(pt);
            _timers[id] = WeakTask(pt);
        }
        //刷新/延迟定时任务
        void TimerRefresh(uint64_t id) {
            //通过保存的定时器对象的weak_ptr构造一个shared_ptr出来，添加到轮子中
            auto it = _timers.find(id);
            if (it == _timers.end()) {
                return;//没找着定时任务，没法刷新，没法延迟
            }
            PtrTask pt = it->second.lock();//lock获取weak_ptr管理的对象对应的shared_ptr
            int delay = pt->DelayTime();
            int pos = (_tick + delay) % _capacity;
            _wheel[pos].push_back(pt);
        }
        void TimerCancel(uint64_t id) {
            auto it = _timers.find(id);
            if (it == _timers.end()) {
                return;//没找着定时任务，没法刷新，没法延迟
            }
            PtrTask pt = it->second.lock();
            if (pt) pt->Cancel();
        }
        //这个函数应该每秒钟被执行一次，相当于秒针向后走了一步
        void RunTimerTask() {
            _tick = (_tick + 1) % _capacity;
            _wheel[_tick].clear();//清空指定位置的数组，就会把数组中保存的所有管理定时器对象的shared_ptr释放掉
        }
};

class Test {
    public:
        Test() {std::cout << "构造" << std::endl;}
        ~Test() {std::cout << "析构" << std::endl;}
};

void DelTest(Test *t) {
    delete t;
}

int main()
{
    TimerWheel tw;

    Test *t = new Test();

    tw.TimerAdd(888, 5, std::bind(DelTest, t));

    for(int i = 0; i < 5; i++) {
        sleep(1);
        tw.TimerRefresh(888);//刷新定时任务
        tw.RunTimerTask();//向后移动秒针
        std::cout << "刷新了一下定时任务，重新需要5s中后才会销毁\n";
    }
    tw.TimerCancel(888);
    while(1) {
        sleep(1);
        std::cout << "-------------------\n";
        tw.RunTimerTask();//向后移动秒针
    }
    return 0;
}

