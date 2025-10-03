#include "compiler/typeck/type_checker.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace qpc::detail {

Binding* TypeChecker::lookup(const std::string& name) {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            if (auto found = it->find(name); found != it->end()) {
                return &found->second;
            }
        }
        return nullptr;
    }

bool TypeChecker::ends_with_return(const HirBlock& body) {
        return !body.stmts.empty() && std::holds_alternative<HirReturn>(body.stmts.back()->kind);
    }

Binding* TypeChecker::lookup_static(const std::string& name) {
        if (!current_prefix_.empty() && !name_has_path(name)) {
            if (auto it = statics_.find(qualify(current_prefix_, name)); it != statics_.end()) {
                return &it->second;
            }
        }
        if (auto it = statics_.find(name); it != statics_.end()) {
            return &it->second;
        }
        if (auto alias = static_aliases_.find(name); alias != static_aliases_.end()) {
            if (auto it = statics_.find(alias->second); it != statics_.end()) {
                return &it->second;
            }
        }
        return nullptr;
    }

Binding* TypeChecker::lookup_binding(const std::string& name) {
        if (auto* b = lookup(name)) {
            return b;
        }
        return lookup_static(name);
    }

bool TypeChecker::is_generic_param(const std::vector<HirTypeParam>& tps, const std::string& name) {
        for (const auto& tp : tps) {
            if (tp.name == name) {
                return true;
            }
        }
        return false;
    }

Type TypeChecker::as_nullable(Type t) {
        if (t.kind == TypeKind::Nullable || t.kind == TypeKind::Error) {
            return t;
        }
        return Type::nullable(std::move(t));
    }

}  // namespace qpc::detail
