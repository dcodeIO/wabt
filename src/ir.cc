/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ir.h"

#include <cassert>
#include <cstddef>

namespace wabt {

bool FuncSignature::operator==(const FuncSignature& rhs) const {
  return param_types == rhs.param_types && result_types == rhs.result_types;
}

const Export* Module::GetExport(const string_view& name) const {
  Index index = export_bindings.FindIndex(name);
  if (index >= exports.size())
    return nullptr;
  return exports[index];
}

Index Module::GetFuncIndex(const Var& var) const {
  return func_bindings.FindIndex(var);
}

Index Module::GetGlobalIndex(const Var& var) const {
  return global_bindings.FindIndex(var);
}

Index Module::GetTableIndex(const Var& var) const {
  return table_bindings.FindIndex(var);
}

Index Module::GetMemoryIndex(const Var& var) const {
  return memory_bindings.FindIndex(var);
}

Index Module::GetFuncTypeIndex(const Var& var) const {
  return func_type_bindings.FindIndex(var);
}

Index Module::GetExceptIndex(const Var& var) const {
  return except_bindings.FindIndex(var);
}

Index Func::GetLocalIndex(const Var& var) const {
  if (var.is_index())
    return var.index();

  Index result = param_bindings.FindIndex(var.name());
  if (result != kInvalidIndex)
    return result;

  result = local_bindings.FindIndex(var.name());
  if (result == kInvalidIndex)
    return result;

  // The locals start after all the params.
  return decl.GetNumParams() + result;
}

const Func* Module::GetFunc(const Var& var) const {
  return const_cast<Module*>(this)->GetFunc(var);
}

Func* Module::GetFunc(const Var& var) {
  Index index = func_bindings.FindIndex(var);
  if (index >= funcs.size())
    return nullptr;
  return funcs[index];
}

const Global* Module::GetGlobal(const Var& var) const {
  return const_cast<Module*>(this)->GetGlobal(var);
}

Global* Module::GetGlobal(const Var& var) {
  Index index = global_bindings.FindIndex(var);
  if (index >= globals.size())
    return nullptr;
  return globals[index];
}

Table* Module::GetTable(const Var& var) {
  Index index = table_bindings.FindIndex(var);
  if (index >= tables.size())
    return nullptr;
  return tables[index];
}

Memory* Module::GetMemory(const Var& var) {
  Index index = memory_bindings.FindIndex(var);
  if (index >= memories.size())
    return nullptr;
  return memories[index];
}

const FuncType* Module::GetFuncType(const Var& var) const {
  return const_cast<Module*>(this)->GetFuncType(var);
}

FuncType* Module::GetFuncType(const Var& var) {
  Index index = func_type_bindings.FindIndex(var);
  if (index >= func_types.size())
    return nullptr;
  return func_types[index];
}


Index Module::GetFuncTypeIndex(const FuncSignature& sig) const {
  for (size_t i = 0; i < func_types.size(); ++i)
    if (func_types[i]->sig == sig)
      return i;
  return kInvalidIndex;
}

Index Module::GetFuncTypeIndex(const FuncDeclaration& decl) const {
  if (decl.has_func_type) {
    return GetFuncTypeIndex(decl.type_var);
  } else {
    return GetFuncTypeIndex(decl.sig);
  }
}

const Module* Script::GetFirstModule() const {
  return const_cast<Script*>(this)->GetFirstModule();
}

Module* Script::GetFirstModule() {
  for (const std::unique_ptr<Command>& command : commands) {
    if (command->type == CommandType::Module)
      return command->module;
  }
  return nullptr;
}

const Module* Script::GetModule(const Var& var) const {
  Index index = module_bindings.FindIndex(var);
  if (index >= commands.size())
    return nullptr;
  const Command& command = *commands[index].get();
  assert(command.type == CommandType::Module);
  return command.module;
}

void MakeTypeBindingReverseMapping(
    const TypeVector& types,
    const BindingHash& bindings,
    std::vector<std::string>* out_reverse_mapping) {
  out_reverse_mapping->clear();
  out_reverse_mapping->resize(types.size());
  for (const auto& pair : bindings) {
    assert(static_cast<size_t>(pair.second.index) <
           out_reverse_mapping->size());
    (*out_reverse_mapping)[pair.second.index] = pair.first;
  }
}

ModuleField* Module::AppendField() {
  ModuleField* result = new ModuleField();
  if (!first_field)
    first_field = result;
  else if (last_field)
    last_field->next = result;
  last_field = result;
  return result;
}

FuncType* Module::AppendImplicitFuncType(const Location& loc,
                                         const FuncSignature& sig) {
  ModuleField* field = AppendField();
  field->loc = loc;
  field->type = ModuleFieldType::FuncType;
  field->func_type = new FuncType();
  field->func_type->sig = sig;

  func_types.push_back(field->func_type);
  return field->func_type;
}

void DestroyExprList(Expr* first) {
  Expr* expr = first;
  while (expr) {
    Expr* next = expr->next;
    delete expr;
    expr = next;
  }
}

Var::Var(Index index) : type_(VarType::Index), index_(index) {
  WABT_ZERO_MEMORY(loc_);
}

Var::Var(const string_view& name) : type_(VarType::Name), name_(name) {
  WABT_ZERO_MEMORY(loc_);
}

Var::Var(Index index, const Location& loc) : Var(index) {
  loc_ = loc;
}

Var::Var(const string_view& name, const Location& loc) : Var(name) {
  loc_ = loc;
}

Var::Var(Var&& rhs) {
  loc_ = rhs.loc_;
  type_ = rhs.type_;
  if (rhs.is_index()) {
    index_ = rhs.index_;
  } else {
    new (&name_) std::string(std::move(rhs.name_));
  }
}

Var::Var(const Var& rhs) {
  loc_ = rhs.loc_;
  type_ = rhs.type_;
  if (rhs.type_ == VarType::Index) {
    index_ = rhs.index_;
  } else {
    new (&name_) std::string(rhs.name_);
  }
}

Var& Var::operator=(Var&& rhs) {
  Destroy();
  loc_ = rhs.loc_;
  type_ = rhs.type_;
  if (rhs.is_index()) {
    index_ = rhs.index_;
  } else {
    new (&name_) std::string(std::move(rhs.name_));
  }
  return *this;
}

Var& Var::operator=(const Var& rhs) {
  Destroy();
  loc_ = rhs.loc_;
  type_ = rhs.type_;
  if (rhs.is_index()) {
    index_ = rhs.index_;
  } else {
    new (&name_) std::string(rhs.name_);
  }
  return *this;
}

Var::~Var() {
  Destroy();
}

void Var::Destroy() {
  typedef std::string string;
  if (is_name())
    name_.~string();
}

void Var::SetLoc(const Location& loc) {
  loc_ = loc;
}

void Var::SetIndex(Index index) {
  Destroy();
  index_ = index_;
  type_ = VarType::Index;
}

void Var::SetName(const string_view& name) {
  Destroy();
  new (&name_) std::string(name.to_string());
  type_ = VarType::Name;
}

Const::Const(I32, uint32_t value) : type(Type::I32), u32(value) {
  WABT_ZERO_MEMORY(loc);
}

Const::Const(I64, uint64_t value) : type(Type::I64), u64(value) {
  WABT_ZERO_MEMORY(loc);
}

Const::Const(F32, uint32_t value) : type(Type::F32), f32_bits(value) {
  WABT_ZERO_MEMORY(loc);
}

Const::Const(F64, uint64_t value) : type(Type::F64), f64_bits(value) {
  WABT_ZERO_MEMORY(loc);
}

Const::Const(I32 tag, uint32_t value, const Location& loc_)
    : Const(tag, value) {
  loc = loc_;
}

Const::Const(I64 tag, uint64_t value, const Location& loc_)
    : Const(tag, value) {
  loc = loc_;
}

Const::Const(F32 tag, uint32_t value, const Location& loc_)
    : Const(tag, value) {
  loc = loc_;
}

Const::Const(F64 tag, uint64_t value, const Location& loc_)
    : Const(tag, value) {
  loc = loc_;
}

Block::Block() : first(nullptr) {}

Block::Block(Expr* first) : first(first) {}

Block::~Block() {
  DestroyExprList(first);
}

Catch::Catch(Expr* first) : first(first) {
  WABT_ZERO_MEMORY(loc);
}

Catch::Catch(Var var, Expr* first) : var(var), first(first) {
  WABT_ZERO_MEMORY(loc);
}

Catch::~Catch() {
  delete first;
}

void destroy_catches(CatchVector* catches) {
  for (Catch* catch_ : *catches)
    delete catch_;
  delete catches;
}


Expr::Expr() : type(ExprType::Binary), next(nullptr) {
  WABT_ZERO_MEMORY(loc);
  binary.opcode = Opcode::Nop;
}

Expr::Expr(ExprType type) : type(type), next(nullptr) {
  WABT_ZERO_MEMORY(loc);
}

Expr::~Expr() {
  switch (type) {
    case ExprType::Block:
      delete block;
      break;
    case ExprType::Br:
      br.var.~Var();
      break;
    case ExprType::BrIf:
      br_if.var.~Var();
      break;
    case ExprType::BrTable:
      delete br_table.targets;
      br_table.default_target.~Var();
      break;
    case ExprType::Call:
      call.var.~Var();
      break;
    case ExprType::CallIndirect:
      call_indirect.var.~Var();
      break;
    case ExprType::GetGlobal:
      get_global.var.~Var();
      break;
    case ExprType::GetLocal:
      get_local.var.~Var();
      break;
    case ExprType::If:
      delete if_.true_;
      DestroyExprList(if_.false_);
      break;
    case ExprType::Loop:
      delete loop;
      break;
    case ExprType::Rethrow:
      rethrow_.var.~Var();
      break;
    case ExprType::SetGlobal:
      set_global.var.~Var();
      break;
    case ExprType::SetLocal:
      set_local.var.~Var();
      break;
    case ExprType::TeeLocal:
      tee_local.var.~Var();
      break;
    case ExprType::Throw:
      throw_.var.~Var();
      break;
    case ExprType::TryBlock:
      delete try_block.block;
      destroy_catches(try_block.catches);
      break;
    case ExprType::Binary:
    case ExprType::Compare:
    case ExprType::Const:
    case ExprType::Convert:
    case ExprType::Drop:
    case ExprType::CurrentMemory:
    case ExprType::GrowMemory:
    case ExprType::Load:
    case ExprType::Nop:
    case ExprType::Return:
    case ExprType::Select:
    case ExprType::Store:
    case ExprType::Unary:
    case ExprType::Unreachable:
      break;
  }
}

// static
Expr* Expr::CreateBinary(Opcode opcode) {
  Expr* expr = new Expr(ExprType::Binary);
  expr->binary.opcode = opcode;
  return expr;
}

// static
Expr* Expr::CreateBlock(Block* block) {
  Expr* expr = new Expr(ExprType::Block);
  expr->block = block;
  return expr;
}

// static
Expr* Expr::CreateBr(Var var) {
  Expr* expr = new Expr(ExprType::Br);
  expr->br.var = var;
  return expr;
}

// static
Expr* Expr::CreateBrIf(Var var) {
  Expr* expr = new Expr(ExprType::BrIf);
  expr->br_if.var = var;
  return expr;
}

// static
Expr* Expr::CreateBrTable(VarVector* targets, Var default_target) {
  Expr* expr = new Expr(ExprType::BrTable);
  expr->br_table.targets = targets;
  expr->br_table.default_target = default_target;
  return expr;
}

// static
Expr* Expr::CreateCall(Var var) {
  Expr* expr = new Expr(ExprType::Call);
  expr->call.var = var;
  return expr;
}

// static
Expr* Expr::CreateCallIndirect(Var var) {
  Expr* expr = new Expr(ExprType::CallIndirect);
  expr->call_indirect.var = var;
  return expr;
}

// static
Expr* Expr::CreateCompare(Opcode opcode) {
  Expr* expr = new Expr(ExprType::Compare);
  expr->compare.opcode = opcode;
  return expr;
}

// static
Expr* Expr::CreateConst(const Const& const_) {
  Expr* expr = new Expr(ExprType::Const);
  expr->const_ = const_;
  return expr;
}

// static
Expr* Expr::CreateConvert(Opcode opcode) {
  Expr* expr = new Expr(ExprType::Convert);
  expr->convert.opcode = opcode;
  return expr;
}

// static
Expr* Expr::CreateCurrentMemory() {
  return new Expr(ExprType::CurrentMemory);
}

// static
Expr* Expr::CreateDrop() {
  return new Expr(ExprType::Drop);
}

// static
Expr* Expr::CreateGetGlobal(Var var) {
  Expr* expr = new Expr(ExprType::GetGlobal);
  expr->get_global.var = var;
  return expr;
}

// static
Expr* Expr::CreateGetLocal(Var var) {
  Expr* expr = new Expr(ExprType::GetLocal);
  expr->get_local.var = var;
  return expr;
}

// static
Expr* Expr::CreateGrowMemory() {
  return new Expr(ExprType::GrowMemory);
}

// static
Expr* Expr::CreateIf(Block* true_, Expr* false_) {
  Expr* expr = new Expr(ExprType::If);
  expr->if_.true_ = true_;
  expr->if_.false_ = false_;
  return expr;
}

// static
Expr* Expr::CreateLoad(Opcode opcode, Address align, uint32_t offset) {
  Expr* expr = new Expr(ExprType::Load);
  expr->load.opcode = opcode;
  expr->load.align = align;
  expr->load.offset = offset;
  return expr;
}

// static
Expr* Expr::CreateLoop(Block* block) {
  Expr* expr = new Expr(ExprType::Loop);
  expr->loop = block;
  return expr;
}

// static
Expr* Expr::CreateNop() {
  return new Expr(ExprType::Nop);
}

// static
Expr* Expr::CreateRethrow(Var var) {
  Expr* expr = new Expr(ExprType::Rethrow);
  expr->rethrow_.var = var;
  return expr;
}

// static
Expr* Expr::CreateReturn() {
  return new Expr(ExprType::Return);
}

// static
Expr* Expr::CreateSelect() {
  return new Expr(ExprType::Select);
}

// static
Expr* Expr::CreateSetGlobal(Var var) {
  Expr* expr = new Expr(ExprType::SetGlobal);
  expr->set_global.var = var;
  return expr;
}

// static
Expr* Expr::CreateSetLocal(Var var) {
  Expr* expr = new Expr(ExprType::SetLocal);
  expr->set_local.var = var;
  return expr;
}

// static
Expr* Expr::CreateStore(Opcode opcode, Address align, uint32_t offset) {
  Expr* expr = new Expr(ExprType::Store);
  expr->store.opcode = opcode;
  expr->store.align = align;
  expr->store.offset = offset;
  return expr;
}

// static
Expr* Expr::CreateTeeLocal(Var var) {
  Expr* expr = new Expr(ExprType::TeeLocal);
  expr->tee_local.var = var;
  return expr;
}

// static
Expr* Expr::CreateThrow(Var var) {
  Expr* expr = new Expr(ExprType::Throw);
  expr->throw_.var = var;
  return expr;
}

// static
Expr* Expr::CreateTry() {
  Expr* expr = new Expr(ExprType::TryBlock);
  expr->try_block.block = nullptr;
  expr->try_block.catches = new CatchVector();
  return expr;
}

// static
Expr* Expr::CreateUnary(Opcode opcode) {
  Expr* expr = new Expr(ExprType::Unary);
  expr->unary.opcode = opcode;
  return expr;
}

// static
Expr* Expr::CreateUnreachable() {
  return new Expr(ExprType::Unreachable);
}

FuncDeclaration::FuncDeclaration()
    : has_func_type(false), type_var(kInvalidIndex) {}

FuncDeclaration::~FuncDeclaration() {}

Func::Func() : first_expr(nullptr) {}

Func::~Func() {
  DestroyExprList(first_expr);
}

Global::Global() : type(Type::Void), mutable_(false), init_expr(nullptr) {}

Global::~Global() {
  DestroyExprList(init_expr);
}

Table::Table() {
  WABT_ZERO_MEMORY(elem_limits);
}

ElemSegment::ElemSegment() : table_var(kInvalidIndex), offset(nullptr) {}

ElemSegment::~ElemSegment() {
  DestroyExprList(offset);
}

DataSegment::DataSegment() : offset(nullptr), data(nullptr), size(0) {}

DataSegment::~DataSegment() {
  DestroyExprList(offset);
  delete[] data;
}

Memory::Memory() {
  WABT_ZERO_MEMORY(page_limits);
}

Import::Import() : kind(ExternalKind::Func), func(nullptr) {}

Import::~Import() {
  switch (kind) {
    case ExternalKind::Func:
      delete func;
      break;
    case ExternalKind::Table:
      delete table;
      break;
    case ExternalKind::Memory:
      delete memory;
      break;
    case ExternalKind::Global:
      delete global;
      break;
    case ExternalKind::Except:
      delete except;
      break;
  }
}

ModuleField::ModuleField() : ModuleField(ModuleFieldType::Start) {}

ModuleField::ModuleField(ModuleFieldType type)
    : type(type), next(nullptr), start(kInvalidIndex) {
  WABT_ZERO_MEMORY(loc);
}

ModuleField::~ModuleField() {
  switch (type) {
    case ModuleFieldType::Except:
      delete except;
      break;
    case ModuleFieldType::Func:
      delete func;
      break;
    case ModuleFieldType::Global:
      delete global;
      break;
    case ModuleFieldType::Import:
      delete import;
      break;
    case ModuleFieldType::Export:
      delete export_;
      break;
    case ModuleFieldType::FuncType:
      delete func_type;
      break;
    case ModuleFieldType::Table:
      delete table;
      break;
    case ModuleFieldType::ElemSegment:
      delete elem_segment;
      break;
    case ModuleFieldType::Memory:
      delete memory;
      break;
    case ModuleFieldType::DataSegment:
      delete data_segment;
      break;
    case ModuleFieldType::Start:
      start.~Var();
      break;
  }
}

Module::Module()
    : first_field(nullptr),
      last_field(nullptr),
      num_except_imports(0),
      num_func_imports(0),
      num_table_imports(0),
      num_memory_imports(0),
      num_global_imports(0),
      start(0) {
  WABT_ZERO_MEMORY(loc);
}

Module::~Module() {
  ModuleField* field = first_field;
  while (field) {
    ModuleField* next_field = field->next;
    delete field;
    field = next_field;
  }
}

ScriptModule::ScriptModule() : type(ScriptModule::Type::Text), text(nullptr) {}

ScriptModule::~ScriptModule() {
  typedef std::string string;
  switch (type) {
    case ScriptModule::Type::Text:
      delete text;
      break;
    case ScriptModule::Type::Binary:
      binary.name.~string();
      delete [] binary.data;
      break;
    case ScriptModule::Type::Quoted:
      quoted.name.~string();
      delete [] binary.data;
      break;
  }
}

Action::Action() : type(ActionType::Get), module_var(kInvalidIndex) {
  WABT_ZERO_MEMORY(loc);
}

Action::~Action() {
  switch (type) {
    case ActionType::Invoke:
      delete invoke;
      break;
    case ActionType::Get:
      break;
  }
}

Command::Command() : type(CommandType::Module), module(nullptr) {}

Command::~Command() {
  typedef std::string string;
  switch (type) {
    case CommandType::Module:
      delete module;
      break;
    case CommandType::Action:
      delete action;
      break;
    case CommandType::Register:
      register_.module_name.~string();
      register_.var.~Var();
      break;
    case CommandType::AssertMalformed:
      delete assert_malformed.module;
      assert_malformed.text.~string();
      break;
    case CommandType::AssertInvalid:
    case CommandType::AssertInvalidNonBinary:
      delete assert_invalid.module;
      assert_invalid.text.~string();
      break;
    case CommandType::AssertUnlinkable:
      delete assert_unlinkable.module;
      assert_unlinkable.text.~string();
      break;
    case CommandType::AssertUninstantiable:
      delete assert_uninstantiable.module;
      assert_uninstantiable.text.~string();
      break;
    case CommandType::AssertReturn:
      delete assert_return.action;
      delete assert_return.expected;
      break;
    case CommandType::AssertReturnCanonicalNan:
      delete assert_return_arithmetic_nan.action;
      break;
    case CommandType::AssertReturnArithmeticNan:
      delete assert_return_canonical_nan.action;
      break;
    case CommandType::AssertTrap:
    case CommandType::AssertExhaustion:
      delete assert_trap.action;
      assert_trap.text.~string();
      break;
  }
}

Script::Script() {}

}  // namespace wabt
