CppGarbageCollection
===============
garbage collection library for C++
```c++
#include "gc_ptr.h"

using namespace vczh;

class Node : ENABLE_GC
{
public:
    gc_ptr<Node> next;
};

int main()
{
    gc_start(0x00100000, 0x00500000);
    {
        auto x = make_gc<Node>();
        auto y = make_gc<Node>();
        x->next = y;
        y->next = x;
        // static_gc_cast and dynamic_gc_cast is waiting for you who like doing pointer conversion
    }
    gc_force_collect(); // will search and delete x and y here
    gc_stop(); // will call gc_force_collect()
}
```

在生产环境中使用需要添加的
--------------
* O1的标记整理
* 分代
* 内存池
* 为了管理大量对象的更好数据结构
