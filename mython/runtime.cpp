#include "runtime.h"

#include <cassert>
#include <iostream>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {
    if(!object || object.TryAs<Class>() || object.TryAs<ClassInstance>()) {
        return false;
    }
    else if(auto num_ptr = object.TryAs<Number>()) {
        return num_ptr->GetValue() == 0 ? false : true;
    }
    else if(auto str_ptr = object.TryAs<String>()) {
        return str_ptr->GetValue().size() == 0 ? false : true;
    }
    else if(auto bool_ptr = object.TryAs<Bool>()) {
        return bool_ptr->GetValue();
    }
    else {
        return false;
    }
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if(this->HasMethod("__str__"s, 0)) {
        auto obj_holder = Call("__str__"s, {}, context);
        obj_holder.Get()->Print(os, context);
    }
    else {
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    auto current_method = class_ptr_->GetMethod(method);
    return current_method && current_method->formal_params.size() == argument_count;
}

Closure& ClassInstance::Fields() {
    return fields_;
}

const Closure& ClassInstance::Fields() const {
    return fields_;
}

ClassInstance::ClassInstance(const Class& cls) 
    : class_ptr_(&cls)
{
}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {
    if(HasMethod(method, actual_args.size())) {
        auto& current_method = *class_ptr_->GetMethod(method);
        
        Closure arguments{};
        for(size_t i = 0; i < current_method.formal_params.size(); ++i) {
            arguments[current_method.formal_params[i]] = actual_args[i];
        }
        fields_["self"s] = ObjectHolder::Share(*this);
        arguments["self"s] = fields_.at("self"s);
        return current_method.body->Execute(arguments, context);       
    }
    throw std::runtime_error("unable to call "s.append(method));
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
    : name_(name)
    , methods_(std::move(methods))
    , parent_(parent)
{
}

const Method* Class::GetMethod(const std::string& name) const {
    for(const auto& method : methods_) {
        if(method.name == name) {
            return &method;
        }
    }
    return parent_ ? parent_->GetMethod(name) : nullptr;
}

const std::string& Class::GetName() const {
    return name_;
}

void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
    os << "Class "s << GetName();
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if(!lhs && !rhs) {
        return true;
    }
    {
        auto lhs_ptr = lhs.TryAs<String>();
        auto rhs_ptr = rhs.TryAs<String>();
        if(lhs_ptr && rhs_ptr) {
            return lhs_ptr->GetValue() == rhs_ptr->GetValue();
        }
    }
    {
        auto lhs_ptr = lhs.TryAs<Number>();
        auto rhs_ptr = rhs.TryAs<Number>();
        if(lhs_ptr && rhs_ptr) {
            return lhs_ptr->GetValue() == rhs_ptr->GetValue();
        }
    }
    {
        auto lhs_ptr = lhs.TryAs<Bool>();
        auto rhs_ptr = rhs.TryAs<Bool>();
        if(lhs_ptr && rhs_ptr) {
            return lhs_ptr->GetValue() == rhs_ptr->GetValue();
        }
    }
    {
        auto lhs_ptr = lhs.TryAs<ClassInstance>();
        auto rhs_ptr = rhs.TryAs<ClassInstance>();
        if(lhs_ptr && rhs_ptr && lhs_ptr->HasMethod("__eq__"s, 1)) {
            auto obj_ptr = lhs_ptr->Call("__eq__"s, {rhs}, context).TryAs<Bool>();
            return obj_ptr->GetValue();
        }
    }
    throw std::runtime_error("uncompatible types"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    {
        auto lhs_ptr = lhs.TryAs<String>();
        auto rhs_ptr = rhs.TryAs<String>();
        if(lhs_ptr && rhs_ptr) {
            return lhs_ptr->GetValue() < rhs_ptr->GetValue();
        }
    }
    {
        auto lhs_ptr = lhs.TryAs<Number>();
        auto rhs_ptr = rhs.TryAs<Number>();
        if(lhs_ptr && rhs_ptr) {
            return lhs_ptr->GetValue() < rhs_ptr->GetValue();
        }
    }
    {
        auto lhs_ptr = lhs.TryAs<Bool>();
        auto rhs_ptr = rhs.TryAs<Bool>();
        if(lhs_ptr && rhs_ptr) {
            return lhs_ptr->GetValue() < rhs_ptr->GetValue();
        }
    }
    {
        auto lhs_ptr = lhs.TryAs<ClassInstance>();
        auto rhs_ptr = rhs.TryAs<ClassInstance>();
        if(lhs_ptr && rhs_ptr && lhs_ptr->HasMethod("__lt__"s, 1)) {
            auto obj_ptr = lhs_ptr->Call("__lt__"s, {rhs}, context).TryAs<Bool>();
            return obj_ptr->GetValue();
        }
    }
    throw std::runtime_error("uncompatible types"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    if(!lhs && !rhs) {
        throw std::runtime_error("uncompatible types"s);
    }
    return NotEqual(lhs, rhs, context) && !Less(lhs, rhs, context);
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

}  // namespace runtime
