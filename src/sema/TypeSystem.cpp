#include "neuronc/sema/TypeSystem.h"
#include <sstream>

namespace neuron {

bool NType::equals(const NType& other) const {
    if (kind != other.kind) return false;

    if (kind == TypeKind::Descriptor) {
        return className == other.className;
    }

    if (kind == TypeKind::Class || kind == TypeKind::Enum ||
        kind == TypeKind::Generic || kind == TypeKind::Module) {
        return name == other.name;
    }

    if (kind == TypeKind::Array || kind == TypeKind::Tensor ||
        kind == TypeKind::Nullable ||
        kind == TypeKind::Dictionary) {
        if (genericArgs.size() != other.genericArgs.size()) return false;
        for (size_t i = 0; i < genericArgs.size(); i++) {
            if (!genericArgs[i]->equals(*other.genericArgs[i])) return false;
        }
        return true;
    }

    if (kind == TypeKind::Pointer) {
        if (!pointeeType || !other.pointeeType) return false;
        return pointeeType->equals(*other.pointeeType);
    }

    if (kind == TypeKind::Method) {
        if (!returnType->equals(*other.returnType)) return false;
        if (paramTypes.size() != other.paramTypes.size()) return false;
        for (size_t i = 0; i < paramTypes.size(); i++) {
            if (!paramTypes[i]->equals(*other.paramTypes[i])) return false;
        }
        return true;
    }

    // Primitive types match if their kinds match
    return true;
}

std::string NType::toString() const {
    switch (kind) {
        case TypeKind::Void: return "void";
        case TypeKind::Int: return "int";
        case TypeKind::Float: return "float";
        case TypeKind::Double: return "double";
        case TypeKind::Bool: return "bool";
        case TypeKind::String: return "string";
        case TypeKind::Char: return "char";
        case TypeKind::Nullable:
            return "maybe " + (genericArgs.empty() ? std::string("unknown")
                                                   : genericArgs.front()->toString());
        case TypeKind::Dynamic: return "dynamic";

        case TypeKind::Array:
        case TypeKind::Tensor: {
            std::ostringstream oss;
            oss << name << "<";
            for (size_t i = 0; i < genericArgs.size(); ++i) {
                oss << genericArgs[i]->toString();
                if (i < genericArgs.size() - 1) oss << ", ";
            }
            oss << ">";
            return oss.str();
        }

        case TypeKind::Pointer:
            return "address of " + (pointeeType ? pointeeType->toString() : "unknown");

        case TypeKind::Class:
        case TypeKind::Enum:
        case TypeKind::Generic:
        case TypeKind::Module:
            return name;

        case TypeKind::Descriptor:
            return "Material<" + className + ">";

        case TypeKind::Dictionary: {
            std::ostringstream oss;
            oss << "Dictionary<";
            for (size_t i = 0; i < genericArgs.size(); ++i) {
                oss << genericArgs[i]->toString();
                if (i < genericArgs.size() - 1) oss << ", ";
            }
            oss << ">";
            return oss.str();
        }

        case TypeKind::Method: {
            std::ostringstream oss;
            oss << "method(";
            for (size_t i = 0; i < paramTypes.size(); ++i) {
                oss << paramTypes[i]->toString();
                if (i < paramTypes.size() - 1) oss << ", ";
            }
            oss << ") as " << (returnType ? returnType->toString() : "void");
            return oss.str();
        }

        case TypeKind::Unknown: return "<unknown>";
        case TypeKind::Error: return "<error>";
        case TypeKind::Auto: return "auto";
        default: return "<unimplemented_type>";
    }
}

// ── Scope implementation ──

bool Scope::define(const std::string& symName, Symbol symbol) {
    if (m_symbols.find(symName) != m_symbols.end()) {
        return false; // already defined in this local scope
    }
    m_symbols.insert({symName, std::move(symbol)});
    return true;
}

Symbol* Scope::lookupLocal(const std::string& symName) {
    auto it = m_symbols.find(symName);
    if (it != m_symbols.end()) {
        return &it->second;
    }
    return nullptr;
}

const Symbol* Scope::lookupLocal(const std::string& symName) const {
    auto it = m_symbols.find(symName);
    if (it != m_symbols.end()) {
        return &it->second;
    }
    return nullptr;
}

Symbol* Scope::lookup(const std::string& symName) {
    // Check local scope
    if (auto* sym = lookupLocal(symName)) {
        return sym;
    }
    // Traverse parent scopes
    if (m_parent) {
        return m_parent->lookup(symName);
    }
    return nullptr;
}

const Symbol* Scope::lookup(const std::string& symName) const {
    if (const auto* sym = lookupLocal(symName)) {
        return sym;
    }
    if (m_parent) {
        return m_parent->lookup(symName);
    }
    return nullptr;
}

} // namespace neuron
