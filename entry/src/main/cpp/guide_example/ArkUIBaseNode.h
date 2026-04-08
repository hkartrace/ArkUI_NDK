// ArkUIBaseNode.h
// Provide a base class for component tree operations.

#ifndef MYAPPLICATION_ARKUIBASENODE_H
#define MYAPPLICATION_ARKUIBASENODE_H

#include <arkui/native_type.h>
#include <list>
#include <memory>

#include "NativeModule.h"

namespace NativeModule {

class ArkUIBaseNode {
public:
    explicit ArkUIBaseNode(ArkUI_NodeHandle handle)
        : handle_(handle), nativeModule_(NativeModuleInstance::GetInstance()->GetNativeNodeAPI()) {}

    virtual ~ArkUIBaseNode() {
        // Encapsulate the destructor to implement child node removal functionality.
        if (!children_.empty()) {
            for (const auto& child : children_) {
                nativeModule_->removeChild(handle_, child->GetHandle());
            }
            children_.clear();
        }
        // Encapsulate the destructor to uniformly recover node resources.
        nativeModule_->disposeNode(handle_);
    }

    void AddChild(const std::shared_ptr<ArkUIBaseNode> &child) {
        children_.emplace_back(child);
        OnAddChild(child);
    }

    void RemoveChild(const std::shared_ptr<ArkUIBaseNode> &child) {
        children_.remove(child);
        OnRemoveChild(child);
    }

    void InsertChild(const std::shared_ptr<ArkUIBaseNode> &child, int32_t index) {
        if (index >= children_.size()) {
            AddChild(child);
        } else {
            auto iter = children_.begin();
            std::advance(iter, index);
            children_.insert(iter, child);
            OnInsertChild(child, index);
        }
    }

    ArkUI_NodeHandle GetHandle() const { return handle_; }

protected:
    // Override the following functions in subclasses that act as parent containers to implement component mounting and unmounting.
    virtual void OnAddChild(const std::shared_ptr<ArkUIBaseNode> &child) {}
    virtual void OnRemoveChild(const std::shared_ptr<ArkUIBaseNode> &child) {}
    virtual void OnInsertChild(const std::shared_ptr<ArkUIBaseNode> &child, int32_t index) {}

    ArkUI_NodeHandle handle_;
    ArkUI_NativeNodeAPI_1 *nativeModule_ = nullptr;

private:
    std::list<std::shared_ptr<ArkUIBaseNode>> children_;
};
} // namespace NativeModule

#endif // MYAPPLICATION_ARKUIBASENODE_H