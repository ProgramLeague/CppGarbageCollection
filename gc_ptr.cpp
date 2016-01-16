#include "gc_ptr.h"
#include <assert.h>
#include <algorithm>
#include <set>
#include <vector>
#include <mutex>
#include <atomic>

using namespace std;

namespace cpp_gc
{
	//////////////////////////////////////////////////////////////////
	// enable_gc
	//////////////////////////////////////////////////////////////////

	void enable_gc::set_record(gc_record _record)
	{
		record = _record;
	}

	enable_gc::enable_gc()
	{
	}

	enable_gc::~enable_gc()
	{
	}

	//////////////////////////////////////////////////////////////////
	// helper functions
	//////////////////////////////////////////////////////////////////

	struct gc_handle //大概是接受管理的对象
	{
		static const int				counter_range = (int32_t)0x80000000;

		int								counter = 0;
		gc_record						record;
        multiset<gc_handle*>			references; //看样子应该是记录可到达对象，不过不清楚是什么时候在维护，请聪明的你告诉我
		multiset<void**>				handle_references;
		bool							mark = false;
	};

	struct gc_handle_dummy
	{
		int								counter = 0;
		gc_record						record;
	};

    struct gc_handle_comparer //几个意思？维护计数？请聪明的你告诉我
	{
		bool operator ()(gc_handle* a, gc_handle* b)
		{
			if (a->counter == gc_handle::counter_range)
			{
				return a->record.start < b->record.start;
			}
			if (b->counter == gc_handle::counter_range)
			{
				return (intptr_t)(a->record.start) + a->record.length <= (intptr_t)b->record.start;
			}
			else
			{
				return a->record.start < b->record.start;
			}
		}
	};

    typedef set<gc_handle*, gc_handle_comparer>		gc_handle_container; //关联的是……
	mutex								gc_lock;
    gc_handle_container*				gc_handles = nullptr; //marking的来源在这里被定义
	size_t								gc_step_size = 0;
	size_t								gc_max_size = 0;
	size_t								gc_last_current_size = 0;
	size_t								gc_current_size = 0;

	gc_handle* gc_find_unsafe(void* handle)
	{
		gc_handle_dummy dummy;
		dummy.record.start = handle;
		gc_handle* input = reinterpret_cast<gc_handle*>(&dummy);
		auto it = gc_handles->find(input);
		return it == gc_handles->end() ? nullptr : *it;
	}

	gc_handle* gc_find_parent_unsafe(void** handle_reference)
	{
		gc_handle_dummy dummy;
		dummy.counter = gc_handle::counter_range;
		dummy.record.start = (void*)handle_reference;
		gc_handle* input = reinterpret_cast<gc_handle*>(&dummy);
		auto it = gc_handles->find(input);
		return it == gc_handles->end() ? nullptr : *it;
	}

    void gc_ref_connect_unsafe(void** handle_reference, void* handle, bool alloc) //维护计数的地方粗线了！
	{
        //这里应该好好研究一下……不过时间不够了，留给聪明的你吧
		gc_handle* parent = nullptr;
		if (alloc)
		{
			if (parent = gc_find_parent_unsafe(handle_reference))
			{
                parent->handle_references.insert(handle_reference); //我用三秒钟的时间扫了一眼是通过references维护计数的（废话
			}
		}
		if (auto target = gc_find_unsafe(handle))
		{
			if (parent || (parent = gc_find_parent_unsafe(handle_reference)))
			{
				parent->references.insert(target);
				if (alloc)
				{
					parent->handle_references.insert(handle_reference);
				}
			}
			else
			{
				target->counter++;
                //夹带私货的揣测一下，这个记数应该是确认一个对象【必然活着】用的，marking从这里开始然后去标记所有活着的对象
			}
		}
	}

    void gc_ref_disconnect_unsafe(void** handle_reference, void* handle, bool dealloc) //楼上反过来
	{
		gc_handle* parent = nullptr;
		if (dealloc)
		{
			if (parent = gc_find_parent_unsafe(handle_reference))
			{
				parent->handle_references.insert(handle_reference);
			}
		}
		if (auto target = gc_find_unsafe(handle))
		{
			if (parent || (parent = gc_find_parent_unsafe(handle_reference)))
			{
				if (dealloc)
				{
					parent->handle_references.erase(handle_reference);
				}
				parent->references.erase(target);
			}
			else
			{
				target->counter--;
			}
		}
	}

	void gc_destroy_disconnect_unsafe(gc_handle* handle)
	{
		for (auto handle_reference : handle->handle_references)
		{
			auto x = reinterpret_cast<gc_ptr<enable_gc>*>(handle_reference);
			*handle_reference = nullptr;
		}
	}

    void gc_destroy_unsafe(gc_handle* handle)
	{
		handle->record.handle->~enable_gc();
		gc_current_size -= handle->record.length;
        free(handle->record.start); //这个貌似是真正在回收对象
		delete handle;
	}

    void gc_destroy_unsafe(vector<gc_handle*>& garbages) //这个貌似是把确认无用的gc_handle集送到楼上统一free
	{
		for (auto handle : garbages)
		{
			gc_destroy_disconnect_unsafe(handle);
		}
		for (auto handle : garbages)
		{
			gc_destroy_unsafe(handle);
		}
		gc_last_current_size = gc_current_size;
	}

    void gc_force_collect_unsafe(vector<gc_handle*>& garbages)
	{
        vector<gc_handle*> markings; //无无的flag

        for (auto handle : *gc_handles)
		{
            if (handle->mark = handle->counter > 0) //通过有计数标记必然活着的对象
			{
				markings.push_back(handle);
			}
		}

        for (int i = 0; i < (int)markings.size(); i++)
		{
			auto ref = markings[i];
            for (auto child : ref->references) //查看所有已标记的references成员，并且全部mark（这俩意思可是不一样的啊！！）
			{
				if (!child->mark)
				{
					child->mark = true;
                    markings.push_back(child); //将之前marking的references成员全部标记（呐，我猜你知道我想说啥）
				}
			}
		}

        for (auto it = gc_handles->begin(); it != gc_handles->end();) //再撸一次，从头到尾
		{
            if (!(*it)->mark) //呐，没有任何一个对象指向你，你被无情的世界抛弃了
			{
                auto it2 = it++; //挪一下
                garbages.push_back(*it2); //确认这是个垃圾对象，它即将逝世（看起来是标准的标记-整理算法）
				gc_handles->erase(it2);
			}
			else
			{
				it++;
			}
		}
	}

	namespace unsafe_functions
	{
        void gc_alloc(gc_record record) //alloc时候GC的操作
		{
			assert(gc_handles);
            auto handle = new gc_handle;
			handle->record = record;
			handle->counter = 1;

			vector<gc_handle*> garbages;
			{
                lock_guard<mutex> guard(gc_lock);//上GC锁
                gc_handles->insert(handle); //让传进来的record接受管理
				gc_current_size += handle->record.length;

                //下面触发收集
                if (gc_current_size > gc_max_size) //用的多了就该收摊了
				{
                    gc_force_collect_unsafe(garbages); //这货告诉garbages什么才是garbages
				}
				else if (gc_current_size - gc_last_current_size > gc_step_size)
				{
					gc_force_collect_unsafe(garbages);
				}
			}
			gc_destroy_unsafe(garbages);
		}

		void gc_register(void* reference, enable_gc* handle)
		{
			assert(gc_handles);

			lock_guard<mutex> guard(gc_lock);
			gc_find_unsafe(reference)->record.handle = handle;
		}

		void gc_ref_alloc(void** handle_reference, void* handle)
		{
			assert(gc_handles);

			lock_guard<mutex> guard(gc_lock);
			gc_ref_connect_unsafe(handle_reference, handle, true);
		}

		void gc_ref_dealloc(void** handle_reference, void* handle)
		{
			assert(gc_handles);

			lock_guard<mutex> guard(gc_lock);
			gc_ref_disconnect_unsafe(handle_reference, handle, true);
		}

		void gc_ref(void** handle_reference, void* old_handle, void* new_handle)
		{
			assert(gc_handles);

			lock_guard<mutex> guard(gc_lock);
			gc_ref_disconnect_unsafe(handle_reference, old_handle, false);
			gc_ref_connect_unsafe(handle_reference, new_handle, false);
		}
	}

	void gc_start(size_t step_size, size_t max_size)
	{
		assert(!gc_handles);

		lock_guard<mutex> guard(gc_lock);
		gc_handles = new gc_handle_container;
		gc_step_size = step_size;
		gc_max_size = max_size;
		gc_last_current_size = 0;
		gc_current_size = 0;
	}

	void gc_stop()
	{
		assert(gc_handles);
		gc_force_collect();

		lock_guard<mutex> guard(gc_lock);
		auto garbages = gc_handles;
		gc_handles = nullptr;
		gc_step_size = 0;
		gc_max_size = 0;
		gc_last_current_size = 0;
		gc_current_size = 0;

		for (auto handle : *garbages)
		{
			gc_destroy_disconnect_unsafe(handle);
		}
		for (auto handle : *garbages)
		{
			gc_destroy_unsafe(handle);
		}
		delete garbages;
	}

	void gc_force_collect()
	{
		assert(gc_handles);
		
		vector<gc_handle*> garbages;
		{
			lock_guard<mutex> guard(gc_lock);
			gc_force_collect_unsafe(garbages);
		}
		gc_destroy_unsafe(garbages);
	}
}
