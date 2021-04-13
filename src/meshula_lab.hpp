#pragma once

#ifdef __cplusplus
# define EXTERNC extern "C"
#else
# define EXTERNC
#endif

#if defined(__CYGWIN__)
# define ML_CYGWIN
#endif
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
# define ML_WINDOWS
# ifdef _WIN64
#  define ML_WINDOWS_64
# else
#  define ML_WINDOWS_32
# endif
#elif __APPLE__
# define ML_APPLE
# include <TargetConditionals.h>
# if TARGET_IPHONE_SIMULATOR
#  define ML_IOS_SIMULATOR
#  define ML_IOS
# elif TARGET_OS_IPHONE
#  define ML_IOS
# elif TARGET_OS_MAC
#  define ML_MACOS
#endif
#elif __linux__
# define ML_NIX
# define ML_LINUX
#elif __unix__
# define ML_NIX
# define ML_UNIX
#elif defined(_POSIX_VERSION)
# define ML_NIX
# define ML_POSIX
#else
# error "Unknown platform"
#endif

#ifdef ML_WINDOWS
# define VC_EXTRALEAN
# include <Windows.h>
# undef VC_EXTRALEAN
#elif defined(ML_APPLE)
# include <mach-o/dyld.h>
#elif defined(ML_NIX)
# include <uinstd.h>
#endif

struct ml_String {};
EXTERNC void ml_String_release(ml_String**);
EXTERNC const char* ml_String_cstr(ml_String*);
EXTERNC int ml_String_length(ml_String*);
EXTERNC ml_String* ml_String_trim_file_separator(ml_String*);
EXTERNC ml_String* ml_String_trim_filename(ml_String*);

EXTERNC ml_String* ml_application_directory_path();
EXTERNC ml_String* ml_application_executable_path();

#ifdef __cplusplus

namespace ml
{
    

}

#endif


#ifdef MESHULA_LAB_DRY

EXTERNC
struct ml_String_
{
    char* str;
    int len;
};

EXTERNC
void ml_String_release(ml_String** ps)
{
    if (!ps || !*ps)
        return;

    ml_String* s = *ps;
    ml_String_* str = (ml_String_*) s;
    if (str->str)
        free(str->str);
    free(str);
    *ps = nullptr;
}

EXTERNC
int ml_String_length(ml_String* s)
{
    if (!s)
        return 0;

    ml_String_* str = (ml_String_*) s;
    return str->len;
}

EXTERNC
const char* ml_String_cstr(ml_String* ps)
{
    if (!ps)
        return nullptr;

    ml_String_* str = (ml_String_*) ps;
    return str->str;
}


#ifdef ML_WINDOWS

EXTERNC
ml_String* ml_application_executable_path()
{
    ml_String_ * s = (ml_String_*) malloc(sizeof(ml_String_));
    s->str = nullptr;
    s->len = 0;
    char buf[1024] = { 0 };
    DWORD ret = GetModuleFileNameA(NULL, buf, sizeof(buf));
    if (!ret || ret == sizeof(buf))
        return (ml_String*) s; // return an empty string, as either the function failed, or path is unusually long

    s->len = (int) strlen(buf);
    s->str = (char*) malloc(s->len + 1);
    if (!s->str)
    {
        s->len = 0;
        return (ml_String*) s; // return empty if malloc failed
    }
    strncpy(s->str, buf, s->len);
    s->str[s->len] = '\0';
    return (ml_String*) s;
}

#elif defined(ML_APPLE)

EXTERNC
ml_String* ml_application_executable_path()
{
    ml_String_ * s = (ml_String_*) malloc(sizeof(ml_String_));
    s->str = nullptr;
    s->len = 0;
    char buf[1024] = { 0 };
    uint32_t buf_size = sizeof(buf);
    int ret = _NSGetExecutablePath(buf, &buf_size);
    if (ret)
        return (ml_String*) s; // return an empty string on failure

    s->len = strlen(buf);
    s->str = (char*) malloc(s->len + 1);
    if (!s->str)
    {
        s->len = 0;
        return (ml_String*) s; // return empty if malloc failed
    }
    strncpy(s->str, buf, s->len);
    s->str[s->len] = '\0';
    return (ml_String*) s;
}

#elif defined(ML_NIX)

EXTERNC
ml_String* ml_application_executable_path()
{
    ml_String_ * s = (ml_String_*) malloc(sizeof(ml_String_));
    s->str = nullptr;
    s->len = 0;
    char buf[1024] = { 0 };
    ssize_t buf_size = readlink("/proc/self/exe", buff, sizeof(buf));
    if (!buf_size || buf_size == sizeof(buf))
        return (ml_String*) s; // return an empty string on failure, or unreasonably long path

    s->len = strlen(buf);
    s->str = (char*) malloc(s->len + 1);
    if (!s->str)
    {
        s->len = 0;
        return (ml_String*) s; // return empty if malloc failed
    }
    strncpy(s->str, buf, s->len);
    s->str[s->len] = '\0';
    return (ml_String*) s;
}

#endif // platform implementations

EXTERNC
ml_String* ml_String_trim_file_separator(ml_String* s0)
{
    if (!s0)
        return nullptr;
    
    ml_String_ *src = (ml_String_*) s0;
    if (!src->str || !src->len)
        return nullptr;
    
    ml_String_ * s = (ml_String_*) malloc(sizeof(ml_String_));
    s->str = (char*) malloc(src->len + 1);
    if (!s->str)
    {
        free(s);
        return nullptr;
    }
    s->len = src->len;
    memcpy(s->str, src->str, src->len+1);
    const char c = s->str[s->len - 1];
    if (c == '/' || c == '\\')
    {
        s->len -= 1;
        s->str[s->len - 1] = '\0';
    }
    return (ml_String*) s;
}

EXTERNC
ml_String* ml_String_trim_filename(ml_String* s0)
{
    if (!s0)
        return nullptr;
    
    ml_String* trimmed = ml_String_trim_file_separator(s0);
    ml_String_* src = (ml_String_*) trimmed;

    const char* sep = strrchr(src->str, '/');
    if (!sep)
        sep = strrchr(src->str, '\\');
    
    if (!sep)
    {
        free(src->str);
        src->str = (char*) malloc(1);
        src->str[0] = '\0';
        src->len = 0;
        return (ml_String*) src;
    }

    src->len = (int)(sep - src->str);
    src->str[src->len] = '\0';
    
    return (ml_String*) trimmed;
}

EXTERNC
ml_String* ml_application_directory_path()
{
    ml_String* app_path = ml_application_executable_path();
    ml_String* trimmed_app_path = ml_String_trim_file_separator(app_path);
    ml_String* dir_path = ml_String_trim_filename(trimmed_app_path);
    ml_String_release(&app_path);
    ml_String_release(&trimmed_app_path);
    return dir_path;
}

#endif // MESHULA_LAB_DRY

