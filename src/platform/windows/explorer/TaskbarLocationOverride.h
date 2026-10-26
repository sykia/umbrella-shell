#pragma once
#include <Windows.h>
#include <array>
#include <atomic>
#include <algorithm>

namespace umbrella::explorer::location_override {
// ITaskbarSettings on the RSDS-pinned build has 41 ABI methods; Location is 10.
// The caller retains the COM object and restricts installation/removal to its UI thread.
class Session final {
    inline static SRWLOCK lock_=SRWLOCK_INIT;
    inline static std::array<Session*,16> active_{};
    std::array<void*,41> table_{};
    void* object_{};
    void* original_{};
    unsigned edge_=3;
    static HRESULT WINAPI location(void* object,unsigned* result) noexcept {
        if(!result)return E_POINTER;
        AcquireSRWLockShared(&lock_);
        HRESULT status=E_UNEXPECTED;
        for(const auto session:active_)if(session && session->object_==object) {
            *result=session->edge_;status=S_OK;break;
        }
        ReleaseSRWLockShared(&lock_);
        return status;
    }
public:
    Session()=default;
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
    ~Session(){reset();}
    bool bind(void* object,void* expected,unsigned edge) noexcept {
        if(object_ || !object || !expected || edge>3)return false;
        MEMORY_BASIC_INFORMATION memory{};
        if(!VirtualQuery(object,&memory,sizeof(memory)) || memory.State!=MEM_COMMIT ||
            (memory.Protect!=PAGE_READWRITE && memory.Protect!=PAGE_EXECUTE_READWRITE))return false;
        void* current{};SIZE_T bytes{};
        if(!ReadProcessMemory(GetCurrentProcess(),object,&current,sizeof(current),&bytes) || bytes!=sizeof(current) || current!=expected)return false;
        if(!ReadProcessMemory(GetCurrentProcess(),expected,table_.data(),sizeof(table_),&bytes) || bytes!=sizeof(table_))return false;
        table_[10]=reinterpret_cast<void*>(&location);
        AcquireSRWLockExclusive(&lock_);
        auto slot=std::find(active_.begin(),active_.end(),nullptr);
        if(slot==active_.end()) {ReleaseSRWLockExclusive(&lock_);return false;}
        object_=object;original_=expected;edge_=edge;*slot=this;
        if(InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(object_),table_.data(),expected)!=expected) {
            *slot=nullptr;object_=nullptr;original_=nullptr;ReleaseSRWLockExclusive(&lock_);return false;
        }
        ReleaseSRWLockExclusive(&lock_);return true;
    }
    bool set(unsigned edge) noexcept {
        if(edge>3)return false;
        AcquireSRWLockExclusive(&lock_);
        const bool valid=object_ && *reinterpret_cast<void**>(object_)==table_.data();
        if(valid)edge_=edge;
        ReleaseSRWLockExclusive(&lock_);return valid;
    }
    bool reset() noexcept {
        AcquireSRWLockExclusive(&lock_);
        if(!object_) {ReleaseSRWLockExclusive(&lock_);return true;}
        const bool restored=InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(object_),original_,table_.data())==table_.data();
        // If Explorer replaced the table independently, do not overwrite that
        // replacement.  The callback must nevertheless forget this Session:
        // retaining a dangling Session in active_ would make a later getter
        // call into released module memory during teardown.
        for(auto& slot:active_)if(slot==this)slot=nullptr;
        object_=nullptr;original_=nullptr;
        ReleaseSRWLockExclusive(&lock_);return restored;
    }
    // Explorer is already destroying the object.  It is no longer safe to
    // inspect or exchange its vtable, but the callback registry still needs
    // to stop referring to this module-local Session.
    void abandon() noexcept {
        AcquireSRWLockExclusive(&lock_);
        for(auto& slot:active_)if(slot==this)slot=nullptr;
        object_=nullptr;original_=nullptr;
        ReleaseSRWLockExclusive(&lock_);
    }
    bool bound() const noexcept {return object_!=nullptr;}
};
}
