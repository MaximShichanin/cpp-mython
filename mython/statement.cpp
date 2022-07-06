#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace

VariableValue::VariableValue(const std::string& var_name) 
    : head_(var_name)
{
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids) 
    : head_(dotted_ids.front())
    , body_(dotted_ids.size() > 2u ? std::vector<std::string>{++dotted_ids.begin(), --dotted_ids.end()} : std::vector<std::string>{})
    , tail_(dotted_ids.size() > 1u ? dotted_ids.back() : ""s)
{
}

void PrintObject(const ObjectHolder& obj_h, Context& context) {
    if(obj_h) {
        auto obj_ptr = obj_h.Get();
        obj_ptr->Print(std::cerr, context);
    }
}

ObjectHolder VariableValue::Execute(Closure& closure, [[maybe_unused]] Context& context) {
    auto head_obj_holder_iter = closure.find(head_);
    if(head_obj_holder_iter == closure.end()) {
        throw std::runtime_error("there is no object: "s.append(head_));
    }
    if(tail_.empty()) {
        return head_obj_holder_iter->second;
    }
    auto head_obj_holder = head_obj_holder_iter->second;
    auto obj_ptr = head_obj_holder.TryAs<runtime::ClassInstance>();
    if(!obj_ptr) {
        throw std::runtime_error("object is not a ClassInstance"s);
    }
    for(const auto& next_subfield : body_) {
        auto& fields = obj_ptr->Fields();
        auto iter = fields.find(next_subfield);
        if(iter == fields.end()) {
            throw std::runtime_error("there is no field: "s.append(next_subfield));
        }
        if(!(obj_ptr = iter->second.TryAs<runtime::ClassInstance>())) {
            throw std::runtime_error("object is not a ClassInstance"s);
        }
    }
    auto result_iter = obj_ptr->Fields().find(tail_);
    if(result_iter == obj_ptr->Fields().end()) {
        throw std::runtime_error("there is no field: "s.append(tail_));
    }
    return result_iter->second;
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv) 
    : name_(std::move(var))
    , data_ptr_(std::move(rv)) 
{
}

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    return closure[name_] = data_ptr_->Execute(closure, context);
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                 std::unique_ptr<Statement> rv) 
    : object_(std::move(object))
    , field_name_(std::move(field_name))
    , data_ptr_(std::move(rv))
{
}
// Присваивает полю object.field_name значение выражения rv
ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    auto obj_ptr = object_.Execute(closure, context).TryAs<runtime::ClassInstance>();
    if(!obj_ptr) {
        throw std::runtime_error("object is not ClassInstance"s);
    }
    auto& fields = obj_ptr->Fields();
    auto obj_h = data_ptr_->Execute(closure, context);
    fields[field_name_] = obj_h;
    return fields[field_name_];
}

NewInstance::NewInstance(const runtime::Class& class_) 
    : instance_holder_(runtime::ObjectHolder::Own(std::forward<runtime::ClassInstance>(runtime::ClassInstance{class_})))
{
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
    : instance_holder_(runtime::ObjectHolder::Own(std::forward<runtime::ClassInstance>(runtime::ClassInstance{class_})))
    , args_(std::move(args))
{
}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    size_t argc = args_.size();
    auto& class_instance_ = *instance_holder_.TryAs<runtime::ClassInstance>();
    if(class_instance_.HasMethod("__init__"s, argc)) {
        std::vector<runtime::ObjectHolder> argv;
        for(auto& next_arg : args_) {
            argv.emplace_back(next_arg->Execute(closure, context));
        }
        class_instance_.Call("__init__"s, argv, context);
    }
    return instance_holder_;
}

Print::Print(unique_ptr<Statement> argument) 
{
    data_.emplace_back(std::move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args)
    : data_(std::move(args))
{
}

unique_ptr<Print> Print::Variable(const std::string& name) {
    return std::make_unique<Print>(std::make_unique<VariableValue>(name));
}

// Во время выполнения команды print вывод должен осуществляться в поток, возвращаемый из
// context.GetOutputStream()
ObjectHolder Print::Execute(Closure& closure, Context& context) {
    for(size_t i = 0; i < data_.size(); ++i) {
        auto obj_holder = data_[i]->Execute(closure, context);
        if(obj_holder) {
            obj_holder->Print(context.GetOutputStream(), context);
        }
        else {
            context.GetOutputStream() << "None"s;
        }
        if(i + 1u < data_.size()) {
            context.GetOutputStream() << ' ';
        }
    }
    context.GetOutputStream() << '\n';
    return {};
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                       std::vector<std::unique_ptr<Statement>> args) 
    : object_(std::move(object))
    , method_(std::move(method))
    , argv_(std::move(args))
{
}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
    auto obj_ptr = object_->Execute(closure, context).TryAs<runtime::ClassInstance>();
    if(!obj_ptr) {
        throw std::runtime_error("object is not ClassInstance"s);
    }
    if(!obj_ptr->HasMethod(method_, argv_.size())) {
        throw std::runtime_error("object has no method: "s.append(method_));
    }
    std::vector<ObjectHolder> transformed_argv{};
    std::transform(argv_.begin(), argv_.end(), std::back_inserter(transformed_argv),
        [&closure, &context](const auto& x) {return x->Execute(closure, context);});
    return obj_ptr->Call(method_, transformed_argv, context);
}

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    std::ostringstream oss;
    auto obj_holder = arg_->Execute(closure, context);
    runtime::Object* obj_ptr;
    if(!obj_holder || !(obj_ptr = obj_holder.Get())) {
        oss << "None"s;
    }
    else {
        obj_ptr->Print(oss, context);
    }
    std::string str = oss.str();
    return runtime::ObjectHolder::Own(runtime::String{str});
}

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    auto lhs_obj_holder = lhs_->Execute(closure, context);
    auto rhs_obj_holder = rhs_->Execute(closure, context);
    //try numbers
    {
        auto lhs_num_ptr = lhs_obj_holder.TryAs<runtime::Number>();
        auto rhs_num_ptr = rhs_obj_holder.TryAs<runtime::Number>();
        if(lhs_num_ptr && rhs_num_ptr) {
            return runtime::ObjectHolder::Own(runtime::Number{lhs_num_ptr->GetValue() + rhs_num_ptr->GetValue()});
        }
    }
    //try strings
    {
        auto lhs_str_ptr = lhs_obj_holder.TryAs<runtime::String>();
        auto rhs_str_ptr = rhs_obj_holder.TryAs<runtime::String>();
        if(lhs_str_ptr && rhs_str_ptr) {
            return runtime::ObjectHolder::Own(runtime::String{lhs_str_ptr->GetValue() + rhs_str_ptr->GetValue()});
        }
    }
    //try ClassInstance
    {
        auto lhs_ci_ptr = lhs_obj_holder.TryAs<runtime::ClassInstance>();
        if(lhs_ci_ptr && lhs_ci_ptr->HasMethod("__add__"s, 1u)) {
            return lhs_ci_ptr->Call("__add__"s, {rhs_obj_holder}, context);
        }
    }
    throw std::runtime_error("unable to add"s);
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    auto lhs_obj_holder = lhs_->Execute(closure, context);
    auto rhs_obj_holder = rhs_->Execute(closure, context);
    //try numbers
    {
        auto lhs_num_ptr = lhs_obj_holder.TryAs<runtime::Number>();
        auto rhs_num_ptr = rhs_obj_holder.TryAs<runtime::Number>();
        if(lhs_num_ptr && rhs_num_ptr) {
            return runtime::ObjectHolder::Own(runtime::Number{lhs_num_ptr->GetValue() - rhs_num_ptr->GetValue()});
        }
    }
    throw std::runtime_error("unable to sub"s);
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    auto lhs_obj_holder = lhs_->Execute(closure, context);
    auto rhs_obj_holder = rhs_->Execute(closure, context);
    //try numbers
    {
        auto lhs_num_ptr = lhs_obj_holder.TryAs<runtime::Number>();
        auto rhs_num_ptr = rhs_obj_holder.TryAs<runtime::Number>();
        if(lhs_num_ptr && rhs_num_ptr) {
            return runtime::ObjectHolder::Own(runtime::Number{lhs_num_ptr->GetValue() * rhs_num_ptr->GetValue()});
        }
    }
    throw std::runtime_error("unable to mult"s);
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    auto lhs_obj_holder = lhs_->Execute(closure, context);
    auto rhs_obj_holder = rhs_->Execute(closure, context);
    //try numbers
    {
        auto lhs_num_ptr = lhs_obj_holder.TryAs<runtime::Number>();
        auto rhs_num_ptr = rhs_obj_holder.TryAs<runtime::Number>();
        if(lhs_num_ptr && rhs_num_ptr && rhs_num_ptr->GetValue()) {
            return runtime::ObjectHolder::Own(runtime::Number{lhs_num_ptr->GetValue() / rhs_num_ptr->GetValue()});
        }
    }
    throw std::runtime_error("unable to div"s);
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    const auto True = runtime::ObjectHolder::Own(runtime::Bool{true});
    const auto False = runtime::ObjectHolder::Own(runtime::Bool{false});
    
    auto lhs_obj_holder = lhs_->Execute(closure, context);
    if(!runtime::IsTrue(lhs_obj_holder)) {
        auto rhs_obj_holder = rhs_->Execute(closure, context);
        return runtime::IsTrue(rhs_obj_holder) ? True : False;
    }
    return True;
}

ObjectHolder And::Execute(Closure& closure, Context& context) {
    const auto True = runtime::ObjectHolder::Own(runtime::Bool{true});
    const auto False = runtime::ObjectHolder::Own(runtime::Bool{false});
    
    auto lhs_obj_holder = lhs_->Execute(closure, context);
    if(runtime::IsTrue(lhs_obj_holder)) {
        auto rhs_obj_holder = rhs_->Execute(closure, context);
        return runtime::IsTrue(rhs_obj_holder) ? True : False;
    }
    return False;
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    const auto True = runtime::ObjectHolder::Own(runtime::Bool{true});
    const auto False = runtime::ObjectHolder::Own(runtime::Bool{false});
    
    return runtime::IsTrue(arg_->Execute(closure, context)) ? False : True;
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for(auto& next_op : argv_) {
        next_op->Execute(closure, context);
    }
    return runtime::ObjectHolder::None();
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body) 
    : arg_(std::move(body))
{
}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
    try {
        return arg_->Execute(closure, context);
    }
    catch(ObjectHolder& e) {
        return e;
    }
    return runtime::ObjectHolder::None();
}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    auto result = statement_->Execute(closure, context);
    if(result) {
        throw result;
    }
    else {
        return runtime::ObjectHolder::None();
    }
}

ClassDefinition::ClassDefinition(ObjectHolder cls) 
    : class_(cls)
{
}

ObjectHolder ClassDefinition::Execute(Closure& closure, [[maybe_unused]] Context& context) {
    return closure[class_.TryAs<runtime::Class>()->GetName()] = class_;
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
               std::unique_ptr<Statement> else_body) 
    : condition_(std::move(condition))
    , if_body_(std::move(if_body))
    , else_body_(std::move(else_body))
{
}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    if(runtime::IsTrue(condition_->Execute(closure, context))) {
        return if_body_->Execute(closure, context);
    }
    else if(else_body_) {
        return else_body_->Execute(closure, context);
    }
    return {};
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs))
    , cmp_(cmp)
{
}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    const auto True = runtime::ObjectHolder::Own(runtime::Bool{true});
    const auto False = runtime::ObjectHolder::Own(runtime::Bool{false});
    
    return cmp_(lhs_->Execute(closure, context), rhs_->Execute(closure, context), context) ? True : False;
}

}  // namespace ast
