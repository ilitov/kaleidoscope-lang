#ifndef _UTILS_HPP_
#define _UTILS_HPP_

class ExprAST;
class PrototypeAST;

namespace utils {
inline std::unique_ptr<ExprAST> logError(const char *str) {
    std::cout << "Error: " << str << '\n';
    return nullptr;
}

inline std::unique_ptr<PrototypeAST> logErrorProto(const char *str) {
    logError(str);
    return nullptr;
}
}  // namespace utils

#endif  // !_UTILS_HPP_
