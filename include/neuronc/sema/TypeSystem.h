#pragma once
#include "neuronc/lexer/Token.h"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>

namespace neuron {

// ══════════════════════════════════════════════════════
// Type system for Neuron
// ══════════════════════════════════════════════════════

enum class TypeKind {
    Void,
    Int,
    Float,
    Double,
    Bool,
    String,
    Char,
    Nullable,

    // Compound types
    Array,      // Array<T>
    Tensor,     // Tensor<T>
    Pointer,    // address of T
    Class,      // user-defined class
    Descriptor, // compile-time descriptor layout type (e.g. Material<Shader>)
    Enum,       // enum type
    Method,     // function/method type
    Dictionary, // Dictionary<K, V>
    Generic,    // unresolved generic <T>
    Module,     // module reference
    Dynamic,    // runtime dynamic type

    // Special
    Unknown,    // not yet resolved
    Error,      // type error occurred
    Auto,       // inferred type
};

// Forward declaration
struct NType;
using NTypePtr = std::shared_ptr<NType>;

// ── Type representation ──
struct NType {
    TypeKind kind;
    std::string name;                       // "int", "float", "Vector2", "Array", etc.
    std::vector<NTypePtr> genericArgs;      // Array<float> → genericArgs = [float]
    NTypePtr pointeeType;                   // for Pointer: the pointed-to type
    NTypePtr returnType;                    // for Method: return type
    std::vector<NTypePtr> paramTypes;       // for Method: parameter types
    std::string className;                  // for Class: the class name

    NType(TypeKind k, std::string n = "")
        : kind(k), name(std::move(n)) {}

    bool isNumeric() const {
        return kind == TypeKind::Int || kind == TypeKind::Float || kind == TypeKind::Double;
    }

    bool isVoid() const { return kind == TypeKind::Void; }
    bool isError() const { return kind == TypeKind::Error; }
    bool isUnknown() const { return kind == TypeKind::Unknown; }
    bool isBool() const { return kind == TypeKind::Bool; }
    bool isAuto() const { return kind == TypeKind::Auto; }
    bool isString() const { return kind == TypeKind::String; }
    bool isDynamic() const { return kind == TypeKind::Dynamic; }
    bool isNullable() const { return kind == TypeKind::Nullable; }
    bool isDescriptor() const { return kind == TypeKind::Descriptor; }

    NTypePtr nullableBase() const {
        if (kind != TypeKind::Nullable || genericArgs.empty()) {
            return nullptr;
        }
        return genericArgs.front();
    }

    std::string toString() const;

    // Equality check
    bool equals(const NType& other) const;

    // ── Factory methods ──
    static NTypePtr makeVoid()   { return std::make_shared<NType>(TypeKind::Void,   "void"); }
    static NTypePtr makeInt()    { return std::make_shared<NType>(TypeKind::Int,    "int"); }
    static NTypePtr makeFloat()  { return std::make_shared<NType>(TypeKind::Float,  "float"); }
    static NTypePtr makeDouble() { return std::make_shared<NType>(TypeKind::Double, "double"); }
    static NTypePtr makeBool()   { return std::make_shared<NType>(TypeKind::Bool,   "bool"); }
    static NTypePtr makeString() { return std::make_shared<NType>(TypeKind::String, "string"); }
    static NTypePtr makeDynamic(){ return std::make_shared<NType>(TypeKind::Dynamic, "dynamic"); }
    static NTypePtr makeUnknown(){ return std::make_shared<NType>(TypeKind::Unknown,"<unknown>"); }
    static NTypePtr makeError()  { return std::make_shared<NType>(TypeKind::Error,  "<error>"); }
    static NTypePtr makeAuto()   { return std::make_shared<NType>(TypeKind::Auto,   "auto"); }
    static NTypePtr makeNullable(NTypePtr baseType) {
        auto t = std::make_shared<NType>(TypeKind::Nullable, "maybe");
        t->genericArgs.push_back(std::move(baseType));
        return t;
    }

    static NTypePtr makePointer(NTypePtr pointee) {
        auto t = std::make_shared<NType>(TypeKind::Pointer, "ptr");
        t->pointeeType = std::move(pointee);
        return t;
    }

    static NTypePtr makeArray(NTypePtr elementType) {
        auto t = std::make_shared<NType>(TypeKind::Array, "Array");
        t->genericArgs.push_back(std::move(elementType));
        return t;
    }

    static NTypePtr makeTensor(NTypePtr elementType) {
        auto t = std::make_shared<NType>(TypeKind::Tensor, "Tensor");
        t->genericArgs.push_back(std::move(elementType));
        return t;
    }

    static NTypePtr makeClass(const std::string& className) {
        auto t = std::make_shared<NType>(TypeKind::Class, className);
        t->className = className;
        return t;
    }

    static NTypePtr makeDescriptor(const std::string& descriptorName) {
        auto t = std::make_shared<NType>(TypeKind::Descriptor, descriptorName);
        t->className = descriptorName;
        return t;
    }

    static NTypePtr makeEnum(const std::string& enumName) {
        auto t = std::make_shared<NType>(TypeKind::Enum, enumName);
        t->className = enumName;
        return t;
    }

    static NTypePtr makeDictionary(NTypePtr keyType, NTypePtr valueType) {
        auto t = std::make_shared<NType>(TypeKind::Dictionary, "Dictionary");
        t->genericArgs.push_back(std::move(keyType));
        t->genericArgs.push_back(std::move(valueType));
        return t;
    }

    static NTypePtr makeMethod(NTypePtr returnType, std::vector<NTypePtr> paramTypes) {
        auto t = std::make_shared<NType>(TypeKind::Method, "method");
        t->returnType = std::move(returnType);
        t->paramTypes = std::move(paramTypes);
        return t;
    }

    static NTypePtr makeGeneric(const std::string& name) {
        return std::make_shared<NType>(TypeKind::Generic, name);
    }

    static NTypePtr makeModule(const std::string& name) {
        return std::make_shared<NType>(TypeKind::Module, name);
    }
};

// ══════════════════════════════════════════════════════
// Symbol table
// ══════════════════════════════════════════════════════

enum class SymbolKind {
    Variable,
    Method,
    Class,
    Enum,
    Shader,
    Descriptor,
    Module,
    Parameter,
    Field,
    Constructor,
    GenericParameter,
};

struct SymbolLocation {
    SourceLocation location;
    int length = 1;
};

struct Symbol {
    std::string name;
    SymbolKind kind;
    NTypePtr type;
    std::string signatureKey;
    bool isPublic   = false;
    bool isConst    = false;
    bool isMutable  = true;
    bool isMoved    = false;
    std::optional<SymbolLocation> definition;
    std::vector<SymbolLocation> references;

    Symbol(std::string n, SymbolKind k, NTypePtr t)
        : name(std::move(n)), kind(k), type(std::move(t)) {}
};

// ── Scope — hierarchical symbol table ──
class Scope {
public:
    explicit Scope(std::shared_ptr<Scope> parent = nullptr, const std::string& name = "")
        : m_parent(std::move(parent)), m_name(name) {}

    /// Define a new symbol in this scope. Returns false if already defined.
    bool define(const std::string& name, Symbol symbol);

    /// Look up a symbol in this scope and all parent scopes.
    Symbol* lookup(const std::string& name);
    const Symbol* lookup(const std::string& name) const;

    /// Look up only in the current scope (no parent search).
    Symbol* lookupLocal(const std::string& name);
    const Symbol* lookupLocal(const std::string& name) const;

    /// Get the parent scope.
    std::shared_ptr<Scope> parent() const { return m_parent; }

    /// Get scope name (for debugging).
    const std::string& name() const { return m_name; }

    /// Get all symbols in this scope.
    const std::unordered_map<std::string, Symbol>& symbols() const { return m_symbols; }

private:
    std::shared_ptr<Scope> m_parent;
    std::string m_name;
    std::unordered_map<std::string, Symbol> m_symbols;
};

// ══════════════════════════════════════════════════════
// Class metadata (for class type resolution)
// ══════════════════════════════════════════════════════

struct ClassInfo {
    std::string name;
    NTypePtr type;
    std::vector<std::string> baseClasses;
    std::shared_ptr<Scope> scope;  // class-level scope (fields + methods)
    bool isPublic = false;
};

} // namespace neuron
