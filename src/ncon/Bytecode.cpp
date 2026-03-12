#include "neuronc/ncon/Bytecode.h"
#include "neuronc/fusion/FusionBuiltins.h"

#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace neuron::ncon {

namespace {

std::string makeConstantKey(const nir::Value *value) {
  std::ostringstream out;
  switch (value->getValueKind()) {
  case nir::ValueKind::ConstantInt:
    out << "i:" << static_cast<const nir::ConstantInt *>(value)->getValue();
    break;
  case nir::ValueKind::ConstantFloat:
    out << "f:" << static_cast<const nir::ConstantFloat *>(value)->getValue();
    break;
  case nir::ValueKind::ConstantString:
    out << "s:" << static_cast<const nir::ConstantString *>(value)->getValue();
    break;
  default:
    out << "u:" << value->getName();
    break;
  }
  return out.str();
}

std::string makeTypeKey(const NTypePtr &type) {
  if (!type) {
    return "<null>";
  }
  return type->toString();
}

class Lowerer {
public:
  Lowerer(const nir::Module &module, Program *program, std::string *error,
          const LowerToProgramOptions &options)
      : m_module(module), m_program(program), m_error(error), m_options(options) {}

  bool lower() {
    if (m_program == nullptr) {
      fail("internal error: null ncon bytecode output");
      return false;
    }

    m_program->moduleName = m_module.getName();

    for (const auto &cls : m_module.getClasses()) {
      (void)registerType(NType::makeClass(cls->getName()));
    }

    for (const auto &global : m_module.getGlobals()) {
      const uint32_t globalId = static_cast<uint32_t>(m_program->globals.size());
      m_globalIds[global.get()] = globalId;
      GlobalRecord record;
      record.nameStringId = internString(global->getName());
      record.typeId = registerType(global->getType());
      if (global->getInitializer() != nullptr) {
        if (!isSupportedConstant(global->getInitializer())) {
          fail("ncon build only supports constant global initializers");
          return false;
        }
        record.initializerConstantId = registerConstant(global->getInitializer());
      }
      m_program->globals.push_back(record);
    }

    for (const auto &fn : m_module.getFunctions()) {
      if (fn->isExtern()) {
        fail("ncon build does not support external native dependencies; use neuron compile for native/FFI builds");
        return false;
      }
      const uint32_t functionId =
          static_cast<uint32_t>(m_program->functions.size());
      m_functionIds[fn.get()] = functionId;
      m_functionNameIds[fn->getName()] = functionId;

      FunctionRecord record;
      record.nameStringId = internString(fn->getName());
      record.returnTypeId = registerType(fn->getReturnType());
      record.argCount = static_cast<uint32_t>(fn->getArguments().size());
      for (const auto &arg : fn->getArguments()) {
        record.argTypeIds.push_back(registerType(arg->getType()));
      }
      m_program->functions.push_back(record);
    }

    for (const auto &fn : m_module.getFunctions()) {
      if (!lowerFunction(*fn)) {
        return false;
      }
    }

    auto it = m_functionNameIds.find("Init");
    if (it == m_functionNameIds.end()) {
      fail("ncon build requires an Init entrypoint");
      return false;
    }
    m_program->entryFunctionId = it->second;
    return true;
  }

private:
  struct FunctionContext {
    std::unordered_map<const nir::Value *, uint32_t> slots;
    std::unordered_map<const nir::Block *, uint32_t> blocks;
    uint32_t nextSlot = 0;
  };

  bool lowerFunction(const nir::Function &function) {
    FunctionContext ctx;
    const uint32_t functionId = m_functionIds.at(&function);
    FunctionRecord &functionRecord = m_program->functions[functionId];
    functionRecord.blockBegin = static_cast<uint32_t>(m_program->blocks.size());

    for (const auto &arg : function.getArguments()) {
      ctx.slots[arg.get()] = ctx.nextSlot++;
    }

    for (const auto &block : function.getBlocks()) {
      const uint32_t blockId = static_cast<uint32_t>(m_program->blocks.size());
      ctx.blocks[block.get()] = blockId;
      BlockRecord blockRecord;
      blockRecord.nameStringId = internString(block->getName());
      blockRecord.instructionBegin =
          static_cast<uint32_t>(m_program->instructions.size());
      m_program->blocks.push_back(blockRecord);

      for (const auto &inst : block->getInstructions()) {
        if (inst->getType() != nullptr &&
            inst->getType()->kind != TypeKind::Void) {
          ctx.slots[inst.get()] = ctx.nextSlot++;
        }
      }
    }

    for (const auto &block : function.getBlocks()) {
      BlockRecord &blockRecord = m_program->blocks[ctx.blocks.at(block.get())];
      blockRecord.instructionBegin =
          static_cast<uint32_t>(m_program->instructions.size());
      for (const auto &inst : block->getInstructions()) {
        InstructionRecord record;
        Opcode opcode;
        if (!opcodeFromInstruction(inst->getKind(), &opcode)) {
          fail("unsupported opcode in ncon lowerer");
          return false;
        }
        record.opcode = static_cast<uint16_t>(opcode);
        record.typeId = registerType(inst->getType());
        auto slotIt = ctx.slots.find(inst.get());
        if (slotIt != ctx.slots.end()) {
          record.dstSlot = slotIt->second;
        }
        record.operandBegin = static_cast<uint32_t>(m_program->operands.size());
        if (!lowerInstructionOperands(function, ctx, *inst, &record)) {
          return false;
        }
        m_program->instructions.push_back(record);
      }
      blockRecord.instructionCount = static_cast<uint32_t>(
          m_program->instructions.size() - blockRecord.instructionBegin);
    }

    functionRecord.blockCount =
        static_cast<uint32_t>(function.getBlocks().size());
    functionRecord.slotCount = ctx.nextSlot;
    return true;
  }

  bool lowerInstructionOperands(const nir::Function &function,
                                const FunctionContext &ctx,
                                const nir::Instruction &inst,
                                InstructionRecord *outRecord) {
    if (outRecord == nullptr) {
      fail("internal error: null instruction output");
      return false;
    }

    if (inst.getKind() == nir::InstKind::Call && !validateCall(inst)) {
      return false;
    }

    for (nir::Value *operand : inst.getOperands()) {
      OperandRecord record;
      switch (operand->getValueKind()) {
      case nir::ValueKind::Argument:
      case nir::ValueKind::Instruction: {
        auto slotIt = ctx.slots.find(operand);
        if (slotIt == ctx.slots.end()) {
          fail("ncon lowerer lost slot mapping for value");
          return false;
        }
        record.kind = OperandKind::Slot;
        record.value = slotIt->second;
        break;
      }
      case nir::ValueKind::ConstantInt:
      case nir::ValueKind::ConstantFloat:
      case nir::ValueKind::ConstantString:
        record.kind = OperandKind::Constant;
        record.value = registerConstant(operand);
        break;
      case nir::ValueKind::Block: {
        const auto *blockRef = static_cast<const nir::BlockRef *>(operand);
        auto blockIt = ctx.blocks.find(blockRef->getBlock());
        if (blockIt == ctx.blocks.end()) {
          fail("ncon lowerer lost block mapping");
          return false;
        }
        record.kind = OperandKind::Block;
        record.value = blockIt->second;
        break;
      }
      case nir::ValueKind::GlobalVariable: {
        auto globalIt = m_globalIds.find(static_cast<const nir::GlobalVariable *>(operand));
        if (globalIt == m_globalIds.end()) {
          fail("ncon lowerer lost global mapping");
          return false;
        }
        record.kind = OperandKind::Global;
        record.value = globalIt->second;
        break;
      }
      default:
        fail("unsupported operand kind in ncon lowerer");
        return false;
      }
      m_program->operands.push_back(record);
      outRecord->operandCount++;
    }
    (void)function;
    return true;
  }

  bool validateCall(const nir::Instruction &inst) {
    if (inst.getOperands().empty() ||
        inst.getOperand(0)->getValueKind() != nir::ValueKind::ConstantString) {
      fail("ncon call lowering requires constant-string callee names");
      return false;
    }

    const std::string callee =
        static_cast<const nir::ConstantString *>(inst.getOperand(0))->getValue();
    if (callee.empty() || callee == "__unknown__") {
      fail("ncon build encountered an unresolved call target");
      return false;
    }
    if (isBuiltin(callee)) {
      return true;
    }
    if (m_functionNameIds.find(callee) != m_functionNameIds.end()) {
      return true;
    }
    if (m_options.nativeCallTargets.find(callee) !=
        m_options.nativeCallTargets.end()) {
      return true;
    }
    fail("ncon build encountered unsupported call target: " + callee);
    return false;
  }

  bool isBuiltin(const std::string &callee) const {
    static const std::unordered_set<std::string> builtins = {
        "Print",          "System.Print",      "IO.WriteLine",
        "IO.ReadInt",     "Math.Sqrt",         "Math.Abs",
        "Math.Pow",       "Time.Now",          "Random.Int",
        "Random.Float",   "Logger.Info",       "Logger.Warning",
        "Logger.Error",   "__neuron_throw",    "__neuron_last_exception",
        "__neuron_clear_exception", "__neuron_has_exception",
        "Tensor.Random",  "Tensor.Zeros",      "Tensor.Ones",
        "Tensor.Identity","create_tensor",     "NN.SelfTest",
        fusionBuiltinName(FusionBuiltinKind::Conv2DBatchNormRelu),
        "Resource.Exists","Resource.ReadText", "Resource.ReadBytes",
        "thread",
    };
    return builtins.find(callee) != builtins.end();
  }

  bool isSupportedConstant(const nir::Value *value) const {
    return value->getValueKind() == nir::ValueKind::ConstantInt ||
           value->getValueKind() == nir::ValueKind::ConstantFloat ||
           value->getValueKind() == nir::ValueKind::ConstantString;
  }

  uint32_t internString(const std::string &text) {
    auto it = m_stringIds.find(text);
    if (it != m_stringIds.end()) {
      return it->second;
    }
    const uint32_t id = static_cast<uint32_t>(m_program->strings.size());
    m_stringIds[text] = id;
    m_program->strings.push_back(text);
    return id;
  }

  uint32_t registerType(const NTypePtr &type) {
    const std::string key = makeTypeKey(type);
    auto it = m_typeIds.find(key);
    if (it != m_typeIds.end()) {
      return it->second;
    }

    const uint32_t id = static_cast<uint32_t>(m_program->types.size());
    m_typeIds[key] = id;
    TypeRecord record;
    if (type != nullptr) {
      record.kind = type->kind;
      record.nameStringId = internString(type->name);
      if (type->pointeeType != nullptr) {
        record.pointeeTypeId = registerType(type->pointeeType);
      }
      if (type->returnType != nullptr) {
        record.returnTypeId = registerType(type->returnType);
      }
      for (const auto &generic : type->genericArgs) {
        record.genericTypeIds.push_back(registerType(generic));
      }
      for (const auto &param : type->paramTypes) {
        record.paramTypeIds.push_back(registerType(param));
      }
      if (type->kind == TypeKind::Class) {
        for (const auto &cls : m_module.getClasses()) {
          if (cls->getName() != type->className) {
            continue;
          }
          for (const auto &field : cls->getFields()) {
            record.fieldNameStringIds.push_back(internString(field.name));
            record.fieldTypeIds.push_back(registerType(field.type));
          }
          break;
        }
      }
    } else {
      record.kind = TypeKind::Unknown;
      record.nameStringId = internString("<null>");
    }
    m_program->types.push_back(record);
    return id;
  }

  uint32_t registerConstant(const nir::Value *value) {
    const std::string key = makeConstantKey(value);
    auto it = m_constantIds.find(key);
    if (it != m_constantIds.end()) {
      return it->second;
    }

    ConstantRecord record;
    switch (value->getValueKind()) {
    case nir::ValueKind::ConstantInt:
      record.kind = ConstantKind::Int;
      record.typeId = registerType(NType::makeInt());
      record.intValue = static_cast<const nir::ConstantInt *>(value)->getValue();
      break;
    case nir::ValueKind::ConstantFloat:
      record.kind = ConstantKind::Float;
      record.typeId = registerType(NType::makeFloat());
      record.floatValue =
          static_cast<const nir::ConstantFloat *>(value)->getValue();
      break;
    case nir::ValueKind::ConstantString:
      record.kind = ConstantKind::String;
      record.typeId = registerType(NType::makeString());
      record.stringId = internString(
          static_cast<const nir::ConstantString *>(value)->getValue());
      break;
    default:
      record.kind = ConstantKind::String;
      record.typeId = registerType(NType::makeString());
      record.stringId = internString("<unsupported>");
      break;
    }

    const uint32_t id = static_cast<uint32_t>(m_program->constants.size());
    m_constantIds[key] = id;
    m_program->constants.push_back(record);
    return id;
  }

  void fail(const std::string &message) {
    if (m_error != nullptr) {
      *m_error = message;
    }
  }

  const nir::Module &m_module;
  Program *m_program = nullptr;
  std::string *m_error = nullptr;
  const LowerToProgramOptions &m_options;
  std::unordered_map<std::string, uint32_t> m_stringIds;
  std::unordered_map<std::string, uint32_t> m_typeIds;
  std::unordered_map<std::string, uint32_t> m_constantIds;
  std::unordered_map<const nir::Function *, uint32_t> m_functionIds;
  std::unordered_map<std::string, uint32_t> m_functionNameIds;
  std::unordered_map<const nir::GlobalVariable *, uint32_t> m_globalIds;
};

} // namespace

bool lowerToProgram(const nir::Module &module, Program *outProgram,
                    std::string *outError,
                    const LowerToProgramOptions &options) {
  Lowerer lowerer(module, outProgram, outError, options);
  return lowerer.lower();
}

bool opcodeFromInstruction(nir::InstKind kind, Opcode *outOpcode) {
  if (outOpcode == nullptr) {
    return false;
  }
  switch (kind) {
  case nir::InstKind::Add:
    *outOpcode = Opcode::Add;
    return true;
  case nir::InstKind::Sub:
    *outOpcode = Opcode::Sub;
    return true;
  case nir::InstKind::Mul:
    *outOpcode = Opcode::Mul;
    return true;
  case nir::InstKind::Div:
    *outOpcode = Opcode::Div;
    return true;
  case nir::InstKind::FAdd:
    *outOpcode = Opcode::FAdd;
    return true;
  case nir::InstKind::FSub:
    *outOpcode = Opcode::FSub;
    return true;
  case nir::InstKind::FMul:
    *outOpcode = Opcode::FMul;
    return true;
  case nir::InstKind::FDiv:
    *outOpcode = Opcode::FDiv;
    return true;
  case nir::InstKind::Eq:
    *outOpcode = Opcode::Eq;
    return true;
  case nir::InstKind::Neq:
    *outOpcode = Opcode::Neq;
    return true;
  case nir::InstKind::Lt:
    *outOpcode = Opcode::Lt;
    return true;
  case nir::InstKind::Lte:
    *outOpcode = Opcode::Lte;
    return true;
  case nir::InstKind::Gt:
    *outOpcode = Opcode::Gt;
    return true;
  case nir::InstKind::Gte:
    *outOpcode = Opcode::Gte;
    return true;
  case nir::InstKind::Alloca:
    *outOpcode = Opcode::Alloca;
    return true;
  case nir::InstKind::Load:
    *outOpcode = Opcode::Load;
    return true;
  case nir::InstKind::Store:
    *outOpcode = Opcode::Store;
    return true;
  case nir::InstKind::Cast:
    *outOpcode = Opcode::Cast;
    return true;
  case nir::InstKind::Call:
    *outOpcode = Opcode::Call;
    return true;
  case nir::InstKind::Br:
    *outOpcode = Opcode::Br;
    return true;
  case nir::InstKind::CondBr:
    *outOpcode = Opcode::CondBr;
    return true;
  case nir::InstKind::Ret:
    *outOpcode = Opcode::Ret;
    return true;
  case nir::InstKind::TensorAdd:
    *outOpcode = Opcode::TensorAdd;
    return true;
  case nir::InstKind::TensorSub:
    *outOpcode = Opcode::TensorSub;
    return true;
  case nir::InstKind::TensorMul:
    *outOpcode = Opcode::TensorMul;
    return true;
  case nir::InstKind::TensorDiv:
    *outOpcode = Opcode::TensorDiv;
    return true;
  case nir::InstKind::TensorMatMul:
    *outOpcode = Opcode::TensorMatMul;
    return true;
  case nir::InstKind::TensorMatMulAdd:
    *outOpcode = Opcode::TensorMatMulAdd;
    return true;
  case nir::InstKind::TensorLinearFused:
    *outOpcode = Opcode::TensorLinearFused;
    return true;
  case nir::InstKind::TensorSlice:
    *outOpcode = Opcode::TensorSlice;
    return true;
  case nir::InstKind::TensorFMA:
    *outOpcode = Opcode::TensorFMA;
    return true;
  case nir::InstKind::FieldAccess:
    *outOpcode = Opcode::FieldAccess;
    return true;
  }
  return false;
}

} // namespace neuron::ncon
