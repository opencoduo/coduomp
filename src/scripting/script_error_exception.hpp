#ifndef CODUOMP_SCRIPT_ERROR_EXCEPTION_HPP
#define CODUOMP_SCRIPT_ERROR_EXCEPTION_HPP

class ScriptErrorClass {
public:
#if defined(WINDOWS_BEHAVIOR)
    ScriptErrorClass() = default;
#else
    ScriptErrorClass();
#endif
};

#endif
