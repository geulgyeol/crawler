// Cross-platform compatibility definitions
#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#ifdef _WIN32
    // Windows-specific includes and definitions
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    
    // Console functions are available on Windows
    inline void SetupConsoleEncoding() {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }
#else
    // Unix/Linux compatibility definitions
    #define CP_UTF8 65001  // UTF-8 code page identifier
    
    // No-op functions for non-Windows platforms
    inline void SetupConsoleEncoding() {
        // On Unix/Linux, UTF-8 is typically the default
        // No action needed
    }
    
    inline void SetConsoleOutputCP(int) { }
    inline void SetConsoleCP(int) { }
#endif

#endif // PLATFORM_COMPAT_H
